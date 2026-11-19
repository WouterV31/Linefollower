// =====================================================================
// Finale code linefollower (ESP32-S3 SuperMini + QTR-8A + DRV8833 + 24C256)
//
// Vertrekt van het basisprogramma (code\basis\Basisprogramma) en vult de
// cyclus aan volgens het leerpad:
//   6. code - sensor   : kalibratie, normalisatie, interpolatie
//                        (gewogen gemiddelde of kwadratisch), lijn kwijt,
//                        kruispunten -> rondetijd
//   7. code - regelaar : P, I (Ki* = Ki * cyclustijd, anti-windup, reset
//                        bij start), D (Kd* = Kd / cyclustijd),
//                        verdeling over de motoren met power en diff
//   8. testen          : automatisch stoppen + reden, alle waarden in debug
//
// Bediening: wifi "LineFollower" -> http://192.168.4.1, seriele commando's
// (115200 baud) of de drukknop.
// =====================================================================

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "SerialCommand.h"

// ============================
// Pinnen (VOORLOPIG: definitieve pintoewijzing volgt, zie open pinconflicten)
// GPIO1/2 zitten zowel bij de sensor (D7/D8) als bij de H-brug (IN4/IN3,
// zoals in de H-brug-POC). Zolang dat zo is, blokkeert checkPins() de motoren.
// In het schema hangen IN3/IN4 aan RX (GPIO44) / TX (GPIO43).
// ============================

const int sensor[8] =
{
  8,   // D1 (meest links)
  9,   // D2
  10,  // D3
  11,  // D4
  12,  // D5
  13,  // D6
  1,   // D7
  2    // D8 (meest rechts)
};

const int PIN_MOTOR_L_A = 4;   // DRV8833 IN1
const int PIN_MOTOR_L_B = 3;   // DRV8833 IN2
const int PIN_MOTOR_R_A = 2;   // DRV8833 IN3
const int PIN_MOTOR_R_B = 1;   // DRV8833 IN4

const int PIN_KNOP = 7;        // drukknop start/stop KEY1 (naar GND, interne pull-up), zoals schema + interrupt-POC
const int PIN_SDA  = 6;        // I2C EEPROM
const int PIN_SCL  = 5;

// ============================
// Sensor: vaste waarden (af te stellen tijdens het testen)
// ============================

const float SENSOR_AFSTAND_MM = 9.525;   // afstand tussen 2 sensoren op de QTR-8A (0,375 inch, datasheet)

// genormaliseerd: wit = 0, zwart = 1000
const int LIJN_DREMPEL = 500;            // zwartste sensor lager -> geen lijn meer (leerpad 6, extra)
const int KRUISPUNT_SENSOREN = 6;        // zoveel sensoren of meer boven de drempel = kruispunt
const unsigned long KRUISPUNT_MIN_MS = 500;   // 2 kruispunten tellen pas na deze tijd
const int KRUISPUNTEN_PER_RONDE = 2;     // afhankelijk van het parcours

// ============================
// Communicatie
// ============================

#define SerialPort Serial
#define Baudrate 115200

const char* ssid = "LineFollower";
const char* password = "GiliamWouter";

WebServer server(80);
SerialCommand sCmd(SerialPort);

// ============================
// EEPROM
// ============================

#define EEPROM_ADDR       0x50
#define EEPROM_PAGE       64      // 24C256: pagina van 64 bytes
#define EEPROM_CHUNK      16      // bytes per I2C-transactie
#define PARAM_MAGIC       0x4C460002UL   // "LF" + versie 2 (finale code: andere struct dan basis)

// ============================
// Instellingen (worden in EEPROM opgeslagen)
// ============================

struct param_t
{
  uint32_t magic;          // herkent een geldige inhoud in de EEPROM

  unsigned long cycleTime; // [us]

  int   power;             // 0..255  basisvermogen
  float diff;              // 0..1    verdeling bijsturing over de wielen

  float kp;
  float ki;                // intern: Ki * cyclustijd[s]  (gebruiker: Ki in 1/s)
  float kd;                // intern: Kd / cyclustijd[s]  (gebruiker: Kd in s)

  int   methode;           // 0 = gewogen gemiddelde, 1 = kwadratische interpolatie

  bool  debug;

  int black[8];
  int white[8];
};

param_t params;

// ============================
// Status
// ============================

bool runRobot = false;
volatile bool knopGedrukt = false;       // gezet door de interrupt, verwerkt in loop()
volatile unsigned long laatsteDruk = 0;  // debounce
const unsigned long DEBOUNCE_MS = 200;

bool pinFout = false;
String stopReden = "";

unsigned long previous = 0;              // start vorige cyclus [us]
unsigned long calculationTime = 0;       // grootste rekentijd [us]
unsigned long vorigeDebug = 0;           // debug-uitvoer [ms]

int   ruw[8];
int   sensorValue[8];                    // genormaliseerd 0 (wit) .. 1000 (zwart)
float positie = 0;                       // [mm], 0 = midden, negatief = links
bool  lijnGevonden = false;

float error = 0;
float iTerm = 0;
float lastErr = 0;
float output = 0;
int   powerLeft = 0;
int   powerRight = 0;

// kruispunten / rondetijd
bool  opKruispunt = false;
int   aantalKruispunten = 0;
unsigned long laatsteKruispunt = 0;      // [ms]
unsigned long rondeStart = 0;            // [ms]
unsigned long laatsteRonde = 0;          // [ms], 0 = nog geen ronde

bool showSaveMessage = false;
String statusMessage = "";

// Ki en Kd in gebruikerseenheden (leerpad 7: onDebug)
float kiGebruiker() { return params.ki / (params.cycleTime / 1000000.0); }
float kdGebruiker() { return params.kd * (params.cycleTime / 1000000.0); }

// ============================
// Interrupt start/stop
// ============================

void IRAM_ATTR knopISR()
{
  unsigned long nu = millis();

  if (nu - laatsteDruk > DEBOUNCE_MS)
  {
    knopGedrukt = true;
    laatsteDruk = nu;
  }
}

// ============================
// Webpagina
// ============================

void sendPage()
{
  String html = R"rawliteral(

<!DOCTYPE html>
<html>

<head>

<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">

<style>

body{
font-family:Arial;
max-width:600px;
margin:auto;
padding:20px;
background:#f4f4f4;
}

.card{
background:white;
padding:15px;
border-radius:12px;
margin-bottom:15px;
box-shadow:0 0 8px rgba(0,0,0,0.12);
}

.running{
color:#28a745;
font-weight:bold;
animation:pulseGreen 0.8s infinite;
}

.stopped{
color:#dc3545;
font-weight:bold;
}

@keyframes pulseGreen
{
  0%   { opacity:1; }
  50%  { opacity:0.35; }
  100% { opacity:1; }
}

input, select{
width:100%;
padding:12px;
font-size:18px;
margin-top:5px;
margin-bottom:15px;
box-sizing:border-box;
}

button{
width:100%;
padding:15px;
font-size:18px;
margin-top:8px;
border:none;
border-radius:10px;
font-weight:bold;
cursor:pointer;
}

.save{ background:#2196F3; color:white; }
.start{ background:#28a745; color:white; }
.stop{ background:#dc3545; color:white; }
.debug{ background:#ff9800; color:white; }

h1{ text-align:center; }

.statusText{ font-size:18px; }

pre{ font-size:14px; overflow-x:auto; }

</style>

</head>

<body>

<h1>Line Follower</h1>

<div class="card">
<span style="color:green;font-weight:bold;">
%MESSAGE%
</span>
</div>

<div class="card">

<h2>Robot Bediening</h2>

<button class="start" onclick="send('/start')">START</button>

<button class="stop" onclick="send('/stop')">STOP</button>

<button class="debug" onclick="send('/debug')">DEBUG ON / OFF</button>

<button class="save" onclick="send('/calibrateWhite')" %SAVESTATE%>CALIBRATE WHITE</button>

<button class="save" onclick="send('/calibrateBlack')" %SAVESTATE%>CALIBRATE BLACK</button>

</div>

<div class="card">

<h2>Status</h2>

<div class="statusText">
<p><b>Status:</b> <span class="%STATUSCLASS%">%STATUS%</span> %REASON%</p>
<p><b>Laatste ronde:</b> %LAP%</p>
<p><b>Debug:</b> %DEBUG%</p>
</div>

<pre id="data" style="display:%DEBUGDISPLAY%;"></pre>

</div>

<div class="card">

<h2>Instellingen</h2>

<form action="/save">

Power [0 - 255]
<input type="number" min="0" max="255" name="power" value="%POWER%">

Diff [0 - 1]
<input type="number" step="0.01" min="0" max="1" name="diff" value="%DIFF%">

Kp [-]
<input type="number" step="0.01" min="0" name="kp" value="%KP%">

Ki [1/s]
<input type="number" step="0.01" min="0" name="ki" value="%KI%">

Kd [s]
<input type="number" step="0.0001" min="0" name="kd" value="%KD%">

Cycle Time [&micro;s]
<input type="number" min="100" max="100000" name="cycle" value="%CYCLE%">

Interpolatie
<select name="method">
<option value="0" %M0%>gewogen gemiddelde</option>
<option value="1" %M1%>kwadratisch</option>
</select>

<button class="save" type="submit" %SAVESTATE%>OPSLAAN</button>

</form>

</div>

<script>

function send(url)
{
  fetch(url)
  .then(() =>
  {
    setTimeout(function()
    {
      location.href = '/';
    },200);
  });
}

if (%DEBUGJS%)
{
  setInterval(function()
  {
    fetch('/data')
    .then(r => r.text())
    .then(t => { document.getElementById('data').textContent = t; })
    .catch(() => {});
  }, 500);
}

</script>

</body>

</html>

)rawliteral";

  html.replace("%POWER%", String(params.power));
  html.replace("%DIFF%", String(params.diff, 2));
  html.replace("%KP%", String(params.kp, 2));
  html.replace("%KI%", String(kiGebruiker(), 2));
  html.replace("%KD%", String(kdGebruiker(), 4));
  html.replace("%CYCLE%", String(params.cycleTime));

  html.replace("%M0%", params.methode == 0 ? "selected" : "");
  html.replace("%M1%", params.methode == 1 ? "selected" : "");

  html.replace("%DEBUG%", params.debug ? "AAN" : "UIT");
  html.replace("%DEBUGDISPLAY%", params.debug ? "block" : "none");
  html.replace("%DEBUGJS%", params.debug ? "true" : "false");

  html.replace("%STATUS%", runRobot ? "ROBOT ACTIEF" : "ROBOT GESTOPT");
  html.replace("%STATUSCLASS%", runRobot ? "running" : "stopped");
  html.replace("%REASON%", (!runRobot && stopReden != "") ? "(" + stopReden + ")" : "");
  html.replace("%SAVESTATE%", runRobot ? "disabled" : "");

  html.replace("%LAP%", laatsteRonde > 0 ? String(laatsteRonde / 1000.0, 2) + " s" : "-");

  html.replace("%MESSAGE%", showSaveMessage ? statusMessage : "");

  server.send(200, "text/html", html);
}

void zetMelding(const String& tekst)
{
  statusMessage = tekst;
  showSaveMessage = true;
}

// ============================
// Sensoren: normaliseren + interpoleren
// ============================

int normaliseer(int i, int waarde)
{
  int bereik = params.black[i] - params.white[i];

  if (bereik == 0) return 0;   // niet gekalibreerd: deling door nul vermijden

  long genormaliseerd = (long)(waarde - params.white[i]) * 1000 / bereik;

  return constrain(genormaliseerd, 0, 1000);
}

void readSensors()
{
  for (int i = 0; i < 8; i++)
  {
    ruw[i] = analogRead(sensor[i]);
    sensorValue[i] = normaliseer(i, ruw[i]);
  }
}

// x-positie [mm] van sensor i, midden van de array = 0
float sensorX(float i)
{
  return (i - 3.5) * SENSOR_AFSTAND_MM;
}

// leest de sensoren en berekent positie [mm]; false = geen lijn
bool calculatePosition()
{
  readSensors();

  // zwartste sensor = hoogste waarde (zwart = 1000)
  int index = 0;
  for (int i = 1; i < 8; i++)
    if (sensorValue[i] > sensorValue[index]) index = i;

  if (sensorValue[index] < LIJN_DREMPEL) return false;

  if (params.methode == 1)
  {
    // kwadratische interpolatie (leerpad 6): parabool door de zwartste sensor en zijn 2 buren
    if (index == 0) index = 1;
    else if (index == 7) index = 6;

    long sZero     = sensorValue[index];
    long sMinusOne = sensorValue[index - 1];
    long sPlusOne  = sensorValue[index + 1];

    float b = ((float)(sPlusOne - sMinusOne)) / 2;
    float a = sPlusOne - b - sZero;

    float top = 0;                        // top van de parabool t.o.v. de middelste sensor
    if (a < 0) top = -b / (2 * a);        // a < 0: bergparabool (zwart = maximum)
    top = constrain(top, -1.0f, 1.0f);    // buitenste sensoren: niet verder dan de buren

    positie = sensorX(index + top);
  }
  else
  {
    // gewogen gemiddelde (leerpad 6): som(waarde * positie) / som(waarden)
    float gewogen = 0;
    long totaal = 0;

    for (int i = 0; i < 8; i++)
    {
      gewogen += (float)sensorValue[i] * sensorX(i);
      totaal += sensorValue[i];
    }

    positie = gewogen / totaal;           // totaal > 0: minstens 1 sensor >= LIJN_DREMPEL
  }

  return true;
}

// kruispunt = veel sensoren tegelijk zwart (leerpad 6, extra) -> rondetijd
void controleerKruispunt()
{
  int zwart = 0;
  for (int i = 0; i < 8; i++)
    if (sensorValue[i] >= LIJN_DREMPEL) zwart++;

  bool kruispunt = (zwart >= KRUISPUNT_SENSOREN);
  unsigned long nu = millis();

  if (kruispunt && !opKruispunt && nu - laatsteKruispunt >= KRUISPUNT_MIN_MS)
  {
    laatsteKruispunt = nu;
    aantalKruispunten++;

    if (aantalKruispunten == 1)
    {
      rondeStart = nu;                    // eerste kruispunt: start van de meting
    }
    else if ((aantalKruispunten - 1) % KRUISPUNTEN_PER_RONDE == 0)
    {
      laatsteRonde = nu - rondeStart;
      rondeStart = nu;
    }
  }

  opKruispunt = kruispunt;
}

bool isGekalibreerd()
{
  for (int i = 0; i < 8; i++)
    if (params.black[i] <= params.white[i]) return false;   // zwart moet hoger liggen dan wit
  return true;
}

// gemiddelde van 100 metingen per sensor; enkel gebruiken als de robot stilstaat
void kalibreer(int* doel)
{
  for (int i = 0; i < 8; i++)
  {
    long sum = 0;

    for (int j = 0; j < 100; j++)
    {
      sum += analogRead(sensor[i]);
      delay(2);
    }

    doel[i] = sum / 100;
  }

  saveSettings();
}

void printRij(const char* titel, const int* waarden)
{
  SerialPort.println(titel);

  for (int i = 0; i < 8; i++)
  {
    SerialPort.print("D");
    SerialPort.print(i + 1);
    SerialPort.print(" = ");
    SerialPort.println(waarden[i]);
  }

  SerialPort.println();
}

// ============================
// Motoren (zoals in de H-brug-POC: PWM op de ene ingang, de andere 0)
// ============================

void zetMotor(int pin1, int pin2, int snelheid)
{
  snelheid = constrain(snelheid, -255, 255);

  if (snelheid > 0)
  {
    analogWrite(pin1, snelheid);
    analogWrite(pin2, 0);
  }
  else if (snelheid < 0)
  {
    analogWrite(pin1, 0);
    analogWrite(pin2, -snelheid);
  }
  else
  {
    analogWrite(pin1, 0);
    analogWrite(pin2, 0);
  }
}

void motorenUit()
{
  powerLeft = 0;
  powerRight = 0;

  if (!pinFout)
  {
    zetMotor(PIN_MOTOR_L_A, PIN_MOTOR_L_B, 0);
    zetMotor(PIN_MOTOR_R_A, PIN_MOTOR_R_B, 0);
  }
}

// ============================
// Start / stop
// ============================

// geeft een foutmelding terug, of "" als de robot gestart is
String startRobot(const char* bron)
{
  if (pinFout) return "Pinconflict: motoren geblokkeerd (zie seriele monitor).";
  if (!isGekalibreerd()) return "Eerst wit en zwart kalibreren.";

  iTerm = 0;                           // leerpad 7: iTerm op 0 bij elke start
  lastErr = 0;

  aantalKruispunten = 0;
  opKruispunt = false;
  laatsteRonde = 0;

  stopReden = "";
  runRobot = true;

  SerialPort.print("ROBOT START (");
  SerialPort.print(bron);
  SerialPort.println(")");
  return "";
}

void stopRobot(const String& reden)
{
  runRobot = false;
  stopReden = reden;
  motorenUit();

  SerialPort.print("ROBOT STOP: ");
  SerialPort.println(reden);
}

// ============================
// EEPROM (24C256 via I2C)
// ============================

// wacht tot de EEPROM klaar is met schrijven (ACK polling, max 10 ms)
void eepromWacht()
{
  unsigned long start = millis();

  do
  {
    Wire.beginTransmission(EEPROM_ADDR);
    if (Wire.endTransmission() == 0) return;
  }
  while (millis() - start < 10);
}

void eepromSchrijf(uint16_t adres, const byte* data, unsigned int lengte)
{
  while (lengte > 0)
  {
    // niet over een paginagrens schrijven
    unsigned int ruimte = EEPROM_PAGE - (adres % EEPROM_PAGE);
    unsigned int n = min(lengte, min(ruimte, (unsigned int)EEPROM_CHUNK));

    Wire.beginTransmission(EEPROM_ADDR);
    Wire.write(highByte(adres));
    Wire.write(lowByte(adres));
    Wire.write(data, n);
    Wire.endTransmission();

    eepromWacht();

    adres += n;
    data += n;
    lengte -= n;
  }
}

void eepromLees(uint16_t adres, byte* data, unsigned int lengte)
{
  while (lengte > 0)
  {
    unsigned int n = min(lengte, (unsigned int)EEPROM_CHUNK);

    Wire.beginTransmission(EEPROM_ADDR);
    Wire.write(highByte(adres));
    Wire.write(lowByte(adres));
    Wire.endTransmission();

    Wire.requestFrom((uint8_t)EEPROM_ADDR, (uint8_t)n);

    for (unsigned int i = 0; i < n; i++)
    {
      data[i] = Wire.available() ? Wire.read() : 0;
    }

    adres += n;
    data += n;
    lengte -= n;
  }
}

void saveSettings()
{
  params.magic = PARAM_MAGIC;
  eepromSchrijf(0, (const byte*)&params, sizeof(params));
}

void loadSettings()
{
  eepromLees(0, (byte*)&params, sizeof(params));
}

void standaardInstellingen()
{
  params.cycleTime = 1000;   // nakijken: ongeveer 2x de gemeten rekentijd (leerpad 7)

  params.power = 100;        // leerpad 7: startwaarden
  params.diff = 0.5;
  params.kp = 0;             // leerpad 7: eerst Kp = 0 -> beide wielen moeten vooruit draaien
  params.ki = 0;
  params.kd = 0;

  params.methode = 0;        // gewogen gemiddelde
  params.debug = false;

  for (int i = 0; i < 8; i++)
  {
    params.white[i] = 0;     // niet gekalibreerd -> start wordt geweigerd
    params.black[i] = 0;
  }

  saveSettings();
}

// ============================
// Parameters instellen (gedeeld door webinterface en seriele commando's)
// ============================

bool zetParameter(const char* naam, const char* waarde)
{
  if (naam == NULL || waarde == NULL) return false;

  float cycleTimeInSec = params.cycleTime / 1000000.0;

  if (strcmp(naam, "cycle") == 0)
  {
    // Ki* en Kd* herrekenen naar de nieuwe cyclustijd (leerpad 7)
    unsigned long nieuw = constrain(atol(waarde), 100L, 100000L);
    float ratio = (float)nieuw / (float)params.cycleTime;
    params.ki *= ratio;
    params.kd /= ratio;
    params.cycleTime = nieuw;
  }
  else if (strcmp(naam, "power") == 0)
  {
    params.power = constrain(atoi(waarde), 0, 255);
  }
  else if (strcmp(naam, "diff") == 0)
  {
    params.diff = constrain((float)atof(waarde), 0.0f, 1.0f);
  }
  else if (strcmp(naam, "kp") == 0)
  {
    params.kp = max((float)atof(waarde), 0.0f);
  }
  else if (strcmp(naam, "ki") == 0)
  {
    params.ki = max((float)atof(waarde), 0.0f) * cycleTimeInSec;    // ingave in 1/s
  }
  else if (strcmp(naam, "kd") == 0)
  {
    params.kd = max((float)atof(waarde), 0.0f) / cycleTimeInSec;    // ingave in s
  }
  else if (strcmp(naam, "method") == 0)
  {
    params.methode = constrain(atoi(waarde), 0, 1);
  }
  else if (strcmp(naam, "debug") == 0)
  {
    if (strcmp(waarde, "on") == 0) params.debug = true;
    else if (strcmp(waarde, "off") == 0) params.debug = false;
    else return false;
  }
  else
  {
    return false;
  }

  return true;
}

void printInstellingen()
{
  SerialPort.println("===== INSTELLINGEN =====");

  SerialPort.print("Power = ");     SerialPort.println(params.power);
  SerialPort.print("Diff = ");      SerialPort.println(params.diff);
  SerialPort.print("Kp = ");        SerialPort.println(params.kp);
  SerialPort.print("Ki [1/s] = ");  SerialPort.println(kiGebruiker());
  SerialPort.print("Kd [s] = ");    SerialPort.println(kdGebruiker(), 4);

  SerialPort.print("CycleTime = ");
  SerialPort.print(params.cycleTime);
  SerialPort.println(" us");

  SerialPort.print("Interpolatie = ");
  SerialPort.println(params.methode == 1 ? "kwadratisch" : "gewogen gemiddelde");

  SerialPort.print("Debug = ");
  SerialPort.println(params.debug ? "on" : "off");

  SerialPort.print("Status = ");
  SerialPort.println(runRobot ? "RUN" : "STOP");
}

// alle waarden in tekst (leerpad 8: debug met ALLE waarden)
String debugTekst()
{
  String s = "";

  s += "status:     " + String(runRobot ? "RUN" : "STOP");
  if (!runRobot && stopReden != "") s += " (" + stopReden + ")";
  s += "\n";

  s += "positie:    " + String(positie, 1) + " mm" + (lijnGevonden ? "" : "  (geen lijn)") + "\n";
  s += "error:      " + String(error, 1) + "\n";
  s += "iTerm:      " + String(iTerm, 1) + "\n";
  s += "output:     " + String(output, 1) + "\n";
  s += "motoren:    L " + String(powerLeft) + "  R " + String(powerRight) + "\n";
  s += "power " + String(params.power) + "  diff " + String(params.diff, 2) +
       "  kp " + String(params.kp, 2) + "  ki " + String(kiGebruiker(), 2) +
       "  kd " + String(kdGebruiker(), 4) + "\n";
  s += "cyclus:     " + String(params.cycleTime) + " us, max. rekentijd " + String(calculationTime) + " us\n";
  s += "kruispunten " + String(aantalKruispunten) + ", laatste ronde " +
       (laatsteRonde > 0 ? String(laatsteRonde / 1000.0, 2) + " s" : String("-")) + "\n\n";

  s += "      ruw   norm   wit  zwart\n";
  for (int i = 0; i < 8; i++)
  {
    char lijn[48];
    snprintf(lijn, sizeof(lijn), "D%d  %5d  %5d  %4d  %5d\n",
             i + 1, ruw[i], sensorValue[i], params.white[i], params.black[i]);
    s += lijn;
  }

  calculationTime = 0;     // grootste rekentijd sinds de vorige opvraging (zoals onDebug)

  return s;
}

// ============================
// Webinterface: handlers
// ============================

void handleRoot()
{
  sendPage();
}

void handleSave()
{
  if (runRobot)
  {
    zetMelding("Stop de robot eerst om op te slaan.");
  }
  else
  {
    // cycle eerst, zodat Ki en Kd met de nieuwe cyclustijd omgerekend worden
    const char* namen[] = { "cycle", "power", "diff", "kp", "ki", "kd", "method" };

    for (int i = 0; i < 7; i++)
    {
      if (server.hasArg(namen[i]))
      {
        zetParameter(namen[i], server.arg(namen[i]).c_str());
      }
    }

    saveSettings();
    printInstellingen();

    zetMelding("Instellingen opgeslagen.");
  }

  // terug naar / zodat vernieuwen het formulier niet opnieuw verstuurt
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleStart()
{
  String fout = startRobot("web");

  if (fout == "") showSaveMessage = false;
  else zetMelding(fout);

  server.send(200, "text/plain", fout == "" ? "OK" : fout);
}

void handleStop()
{
  stopRobot("web");
  showSaveMessage = false;

  server.send(200, "text/plain", "OK");
}

void handleDebug()
{
  params.debug = !params.debug;
  saveSettings();
  showSaveMessage = false;

  SerialPort.print("Debug = ");
  SerialPort.println(params.debug ? "on" : "off");

  server.send(200, "text/plain", "OK");
}

void handleCalibrateWhite()
{
  if (runRobot)
  {
    zetMelding("Stop de robot eerst om te kalibreren.");
  }
  else
  {
    kalibreer(params.white);
    printRij("WHITE:", params.white);
    zetMelding("White calibration opgeslagen");
  }

  server.send(200, "text/plain", "OK");
}

void handleCalibrateBlack()
{
  if (runRobot)
  {
    zetMelding("Stop de robot eerst om te kalibreren.");
  }
  else
  {
    kalibreer(params.black);
    printRij("BLACK:", params.black);
    zetMelding("Black calibration opgeslagen");
  }

  server.send(200, "text/plain", "OK");
}

void handleData()
{
  server.send(200, "text/plain", debugTekst());
}

// ============================
// Seriele commando's: handlers
// ============================

void onUnknownCommand(char* command)
{
  SerialPort.print("unknown command: \"");
  SerialPort.print(command);
  SerialPort.println("\"");
}

// set <cycle|power|diff|kp|ki|kd|method|debug> <waarde>
void onSet()
{
  char* naam = sCmd.next();
  char* waarde = sCmd.next();

  if (runRobot)
  {
    SerialPort.println("stop de robot eerst");
    return;
  }

  if (zetParameter(naam, waarde))
  {
    saveSettings();
    printInstellingen();
  }
  else
  {
    SerialPort.println("gebruik: set <cycle|power|diff|kp|ki|kd|method|debug> <waarde>");
  }
}

void onDebug()
{
  SerialPort.print(debugTekst());
}

void onStart()
{
  String fout = startRobot("serieel");
  if (fout != "") SerialPort.println(fout);
}

void onStop()
{
  stopRobot("serieel");
}

// calibrate <white|black>
void onCalibrate()
{
  char* kleur = sCmd.next();

  if (runRobot)
  {
    SerialPort.println("stop de robot eerst");
  }
  else if (kleur != NULL && strcmp(kleur, "white") == 0)
  {
    kalibreer(params.white);
    printRij("WHITE:", params.white);
  }
  else if (kleur != NULL && strcmp(kleur, "black") == 0)
  {
    kalibreer(params.black);
    printRij("BLACK:", params.black);
  }
  else
  {
    SerialPort.println("gebruik: calibrate <white|black>");
  }
}

// ============================
// Controle pinnen
// ============================

bool controleerPinnen()
{
  int pinnen[] = { sensor[0], sensor[1], sensor[2], sensor[3],
                   sensor[4], sensor[5], sensor[6], sensor[7],
                   PIN_MOTOR_L_A, PIN_MOTOR_L_B, PIN_MOTOR_R_A, PIN_MOTOR_R_B,
                   PIN_KNOP, PIN_SDA, PIN_SCL };
  const int n = sizeof(pinnen) / sizeof(pinnen[0]);
  bool ok = true;

  for (int i = 0; i < n; i++)
    for (int j = i + 1; j < n; j++)
      if (pinnen[i] == pinnen[j])
      {
        SerialPort.printf("FOUT: GPIO%d wordt dubbel gebruikt -> motoren geblokkeerd\n", pinnen[i]);
        ok = false;
      }

  for (int i = 0; i < 8; i++)
    if (sensor[i] < 1 || sensor[i] > 10)
      SerialPort.printf("LET OP: sensor D%d op GPIO%d is geen ADC1-pin (GPIO1..10): stoort mogelijk met wifi\n",
                        i + 1, sensor[i]);

  return ok;
}

// ============================
// Setup
// ============================

void setup()
{
  SerialPort.begin(Baudrate);

  // niet eindeloos wachten: op batterij is er geen USB
  unsigned long t = millis();
  while (!SerialPort && millis() - t < 2000)
  {
    delay(10);
  }

  SerialPort.println("ESP START (reset)");
  zetMelding("Robot opgestart (reset).");     // leerpad 8: melding na een reset, ook op de webpagina

  pinFout = !controleerPinnen();

  /// EEPROM ///
  Wire.begin(PIN_SDA, PIN_SCL);

  loadSettings();

  if (params.magic != PARAM_MAGIC ||
      params.power < 0 || params.power > 255 ||
      params.cycleTime < 100 || params.cycleTime > 100000)
  {
    SerialPort.println("EEPROM leeg of ongeldig: standaardinstellingen");
    standaardInstellingen();
  }
  else
  {
    SerialPort.println("EEPROM geladen");
  }

  printInstellingen();
  printRij("WHITE UIT EEPROM:", params.white);
  printRij("BLACK UIT EEPROM:", params.black);

  /// Motoren ///
  if (!pinFout)
  {
    pinMode(PIN_MOTOR_L_A, OUTPUT);
    pinMode(PIN_MOTOR_L_B, OUTPUT);
    pinMode(PIN_MOTOR_R_A, OUTPUT);
    pinMode(PIN_MOTOR_R_B, OUTPUT);
    motorenUit();
  }

  /// Start/stop-knop ///
  pinMode(PIN_KNOP, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_KNOP), knopISR, FALLING);

  /// Seriele commando's ///
  sCmd.addCommand("set", onSet);
  sCmd.addCommand("debug", onDebug);
  sCmd.addCommand("start", onStart);
  sCmd.addCommand("stop", onStop);
  sCmd.addCommand("calibrate", onCalibrate);
  sCmd.setDefaultHandler(onUnknownCommand);

  /// Web interface ///
  WiFi.softAP(ssid, password);

  SerialPort.println();
  SerialPort.print("IP adres: ");
  SerialPort.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.on("/start", handleStart);
  server.on("/stop", handleStop);
  server.on("/debug", handleDebug);
  server.on("/calibrateWhite", handleCalibrateWhite);
  server.on("/calibrateBlack", handleCalibrateBlack);
  server.on("/data", handleData);

  server.begin();

  SerialPort.println("Webserver gestart");
  SerialPort.println("Commando's: set <cycle|power|diff|kp|ki|kd|method|debug> <waarde>,");
  SerialPort.println("            debug, start, stop, calibrate <white|black>");
  SerialPort.println("ready");

  previous = micros();
}

// ============================
// Loop
// ============================

void loop()
{
  // communicatie (niet blokkerend), buiten het cyclische deel
  server.handleClient();
  sCmd.readSerial();

  if (knopGedrukt)
  {
    knopGedrukt = false;

    if (runRobot) stopRobot("drukknop");
    else
    {
      String fout = startRobot("drukknop");
      if (fout != "") SerialPort.println(fout);
    }
  }

  // vaste cyclus
  unsigned long current = micros();

  if (current - previous >= params.cycleTime)
  {
    previous = current;

    // ---------- sensoren: normaliseren + interpoleren ----------
    lijnGevonden = calculatePosition();

    if (runRobot && !lijnGevonden) stopRobot("lijn kwijt");

    if (runRobot)
    {
      controleerKruispunt();

      // ---------- pid regeling (leerpad 7) ----------
      error = -positie;                         // error = setpoint (0) - positie

      output = error * params.kp;               // proportioneel

      iTerm += params.ki * error;               // integrerend
      iTerm = constrain(iTerm, -510, 510);      // reset windup
      output += iTerm;

      output += params.kd * (error - lastErr);  // differentierend
      lastErr = error;

      output = constrain(output, -510, 510);    // fysiek maximum

      // ---------- verdeling over de motoren ----------
      if (output >= 0)
      {
        powerLeft  = constrain(params.power + params.diff * output, -255, 255);
        powerRight = constrain(powerLeft - output, -255, 255);
        powerLeft  = powerRight + output;
      }
      else
      {
        powerRight = constrain(params.power - params.diff * output, -255, 255);
        powerLeft  = constrain(powerRight + output, -255, 255);
        powerRight = powerLeft - output;
      }

      // ---------- aansturen motoren ----------
      zetMotor(PIN_MOTOR_L_A, PIN_MOTOR_L_B, powerLeft);
      zetMotor(PIN_MOTOR_R_A, PIN_MOTOR_R_B, powerRight);
    }

    // geen seriele uitvoer in het cyclische deel

    unsigned long difference = micros() - current;
    if (difference > calculationTime) calculationTime = difference;

    // leerpad 8: automatisch stoppen als de berekening langer duurt dan de cyclus
    if (runRobot && difference > params.cycleTime) stopRobot("rekentijd > cyclustijd");
  }

  // debug: 1x per seconde naar de seriele monitor, buiten het cyclische deel
  if (params.debug && millis() - vorigeDebug >= 1000)
  {
    vorigeDebug = millis();

    SerialPort.print(runRobot ? "RUN" : "STOP");
    SerialPort.print("  pos = ");
    SerialPort.print(positie, 1);
    SerialPort.print(" mm  output = ");
    SerialPort.print(output, 1);
    SerialPort.print("  L ");
    SerialPort.print(powerLeft);
    SerialPort.print("  R ");
    SerialPort.println(powerRight);
  }
}
