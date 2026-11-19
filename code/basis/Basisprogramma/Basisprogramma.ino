// =====================================================================
// Basisprogramma linefollower (ESP32-S3 SuperMini)
//
// Vertrekt van Web_Interface_met_EEPROM_met_melding_opslaan (3/11/2026)
// en het basisprogramma uit het leerpad (cyclus.ino):
//   - draadloze communicatie: wifi access point + webinterface
//   - seriele commando's (SerialCommand)
//   - parameters in externe I2C-EEPROM (24C256)
//   - start/stop met drukknop via externe interrupt
//   - vaste cyclus (zoals een plc), met meting van de rekentijd
//
// PID en motoraansturing komen in de finale code.
// =====================================================================

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include "SerialCommand.h"

// ============================
// Pinnen (VOORLOPIG: definitieve pintoewijzing volgt, zie open pinconflicten)
// ============================

const int sensor[8] =
{
  8,   // D1
  9,   // D2
  10,  // D3
  11,  // D4
  12,  // D5
  13,  // D6
  1,   // D7
  2    // D8
};

const int PIN_KNOP = 7;      // drukknop start/stop KEY1 (naar GND, interne pull-up), zoals schema + interrupt-POC
const int PIN_SDA  = 6;      // I2C EEPROM
const int PIN_SCL  = 5;

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
#define PARAM_MAGIC       0x4C460001UL   // "LF" + versie 1

// ============================
// Instellingen (worden in EEPROM opgeslagen)
// ============================

struct param_t
{
  uint32_t magic;          // herkent een geldige inhoud in de EEPROM

  float kp;
  float ki;
  float kd;

  int maxSpeed;

  bool debug;

  unsigned long cycleTime; // [us]

  int black[8];
  int white[8];
};

param_t params;

// ============================
// Status
// ============================

volatile bool runRobot = false;          // gedeeld met de interrupt
volatile unsigned long laatsteDruk = 0;  // debounce
const unsigned long DEBOUNCE_MS = 200;

unsigned long previous = 0;              // start vorige cyclus [us]
unsigned long calculationTime = 0;       // grootste rekentijd [us]
unsigned long vorigeDebug = 0;           // debug-uitvoer [ms]

int  sensorValue[8];                     // genormaliseerd 0 (wit) .. 1000 (zwart)
long lijnPositie = 0;                    // -3500 .. 3500

bool showSaveMessage = false;
String statusMessage = "";

// ============================
// Interrupt start/stop
// ============================

void IRAM_ATTR knopISR()
{
  unsigned long nu = millis();

  if (nu - laatsteDruk > DEBOUNCE_MS)
  {
    runRobot = !runRobot;
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

input{
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

.save{
background:#2196F3;
color:white;
}

.start{
background:#28a745;
color:white;
}

.stop{
background:#dc3545;
color:white;
}

.debug{
background:#ff9800;
color:white;
}

h1{
text-align:center;
}

.statusText{
font-size:18px;
}

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

<h2>PID Instellingen</h2>

<form action="/save">

Kp [-]

<input type="number" step="0.01" min="0" max="50" name="kp" value="%KP%">

Ki [-]

<input type="number" step="0.01" min="0" max="50" name="ki" value="%KI%">

Kd [-]

<input type="number" step="0.01" min="0" max="100" name="kd" value="%KD%">

Max Speed [0 - 255]

<input type="number" min="0" max="255" name="speed" value="%SPEED%">

Cycle Time [&micro;s]

<input type="number" min="100" max="100000" name="cycle" value="%CYCLE%">

<button class="save" type="submit" %SAVESTATE%>
OPSLAAN
</button>

</form>

</div>

<div class="card">

<h2>Robot Bediening</h2>

<button class="start" onclick="send('/start')">START</button>

<button class="stop" onclick="send('/stop')">STOP</button>

<button class="debug" onclick="send('/debug')">DEBUG ON / OFF</button>

<button class="save" onclick="send('/calibrateWhite')" %SAVESTATE%>CALIBRATE WHITE</button>

<button class="save" onclick="send('/calibrateBlack')" %SAVESTATE%>CALIBRATE BLACK</button>

<button class="save" onclick="send('/normaliseTest')">NORMALISE TEST</button>

<button class="save" onclick="send('/linePosition')">LINE POSITION</button>

</div>

<div class="card">

<h2>Status</h2>

<div class="statusText">

<p>
<b>Status:</b>
<span class="%STATUSCLASS%">
%STATUS%
</span>
</p>

<p><b>Debug:</b> %DEBUG%</p>

<p><b>Kp:</b> %KP%</p>

<p><b>Ki:</b> %KI%</p>

<p><b>Kd:</b> %KD%</p>

<p><b>Max Speed:</b> %SPEED%</p>

<p><b>Cycle Time:</b> %CYCLE% &micro;s</p>

<p><b>Max. rekentijd:</b> %CALC% &micro;s</p>

<p><b>Lijnpositie:</b> %POS%</p>

<p><b>Sensoren (0 wit - 1000 zwart):</b> %SENS%</p>

</div>

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

</script>

</body>

</html>

)rawliteral";

  html.replace("%KP%", String(params.kp,2));
  html.replace("%KI%", String(params.ki,2));
  html.replace("%KD%", String(params.kd,2));
  html.replace("%SPEED%", String(params.maxSpeed));
  html.replace("%CYCLE%", String(params.cycleTime));

  html.replace("%DEBUG%", params.debug ? "AAN" : "UIT");

  html.replace("%STATUS%", runRobot ? "ROBOT ACTIEF" : "ROBOT GESTOPT");
  html.replace("%STATUSCLASS%", runRobot ? "running" : "stopped");
  html.replace("%SAVESTATE%", runRobot ? "disabled" : "");

  // grootste rekentijd sinds de vorige keer tonen, daarna resetten (zoals onDebug)
  html.replace("%CALC%", String(calculationTime));
  calculationTime = 0;

  html.replace("%POS%", String(lijnPositie));

  String s = "";
  for (int i = 0; i < 8; i++)
  {
    s += String(sensorValue[i]);
    if (i < 7) s += " | ";
  }
  html.replace("%SENS%", s);

  html.replace("%MESSAGE%", showSaveMessage ? statusMessage : "");

  server.send(200, "text/html", html);
}

void zetMelding(const String& tekst)
{
  statusMessage = tekst;
  showSaveMessage = true;
}

// ============================
// Sensoren
// ============================

int normaliseer(int i, int ruw)
{
  int bereik = params.black[i] - params.white[i];

  if (bereik == 0) return 0;   // niet gekalibreerd: deling door nul vermijden

  long waarde = (long)(ruw - params.white[i]) * 1000 / bereik;

  return constrain(waarde, 0, 1000);
}

void readSensors()
{
  for (int i = 0; i < 8; i++)
  {
    sensorValue[i] = normaliseer(i, analogRead(sensor[i]));
  }
}

long calculatePosition()
{
  readSensors();

  long weightedSum = 0;
  long total = 0;

  for (int i = 0; i < 8; i++)
  {
    weightedSum += (long)sensorValue[i] * (i * 1000);
    total += sensorValue[i];
  }

  if (total == 0)
  {
    return 3500;
  }

  return (weightedSum / total) - 3500;
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
  params.kp = 0.30;
  params.ki = 0.00;
  params.kd = 2.00;

  params.maxSpeed = 180;

  params.debug = false;

  params.cycleTime = 1000;

  for (int i = 0; i < 8; i++)
  {
    params.white[i] = 0;
    params.black[i] = 4095;
  }

  saveSettings();
}

// ============================
// Parameters instellen (gedeeld door webinterface en seriele commando's)
// ============================

bool zetParameter(const char* naam, const char* waarde)
{
  if (naam == NULL || waarde == NULL) return false;

  if (strcmp(naam, "cycle") == 0)
  {
    params.cycleTime = constrain(atol(waarde), 100L, 100000L);
  }
  else if (strcmp(naam, "kp") == 0)
  {
    params.kp = constrain((float)atof(waarde), 0.0f, 50.0f);
  }
  else if (strcmp(naam, "ki") == 0)
  {
    params.ki = constrain((float)atof(waarde), 0.0f, 50.0f);
  }
  else if (strcmp(naam, "kd") == 0)
  {
    params.kd = constrain((float)atof(waarde), 0.0f, 100.0f);
  }
  else if (strcmp(naam, "speed") == 0)
  {
    params.maxSpeed = constrain(atoi(waarde), 0, 255);
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

  SerialPort.print("Kp = ");
  SerialPort.println(params.kp);

  SerialPort.print("Ki = ");
  SerialPort.println(params.ki);

  SerialPort.print("Kd = ");
  SerialPort.println(params.kd);

  SerialPort.print("MaxSpeed = ");
  SerialPort.println(params.maxSpeed);

  SerialPort.print("CycleTime = ");
  SerialPort.print(params.cycleTime);
  SerialPort.println(" us");

  SerialPort.print("Debug = ");
  SerialPort.println(params.debug ? "on" : "off");

  SerialPort.print("Status = ");
  SerialPort.println(runRobot ? "RUN" : "STOP");
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
    sendPage();
    return;
  }

  const char* namen[] = { "cycle", "kp", "ki", "kd", "speed" };

  for (int i = 0; i < 5; i++)
  {
    if (server.hasArg(namen[i]))
    {
      zetParameter(namen[i], server.arg(namen[i]).c_str());
    }
  }

  saveSettings();
  printInstellingen();

  zetMelding("Instellingen opgeslagen.");
  sendPage();
}

void handleStart()
{
  runRobot = true;
  showSaveMessage = false;

  SerialPort.println("ROBOT START (web)");
  server.send(200, "text/plain", "OK");
}

void handleStop()
{
  runRobot = false;
  showSaveMessage = false;

  SerialPort.println("ROBOT STOP (web)");
  server.send(200, "text/plain", "OK");
}

void handleDebug()
{
  params.debug = !params.debug;
  saveSettings();

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

void handleNormaliseTest()
{
  readSensors();
  printRij("NORMALISED:", sensorValue);

  zetMelding("Normalisatie uitgevoerd");
  server.send(200, "text/plain", "OK");
}

void handleLinePosition()
{
  lijnPositie = calculatePosition();

  SerialPort.print("Position = ");
  SerialPort.println(lijnPositie);

  zetMelding("Line position berekend: " + String(lijnPositie));
  server.send(200, "text/plain", "OK");
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

// set <cycle|kp|ki|kd|speed|debug> <waarde>
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
    SerialPort.println("gebruik: set <cycle|kp|ki|kd|speed|debug> <waarde>");
  }
}

// debug: alle parameters + grootste rekentijd sinds vorige oproep
void onDebug()
{
  printInstellingen();

  printRij("WHITE:", params.white);
  printRij("BLACK:", params.black);

  SerialPort.print("calculation time: ");
  SerialPort.print(calculationTime);
  SerialPort.println(" us");
  calculationTime = 0;
}

void onStart()
{
  runRobot = true;
  SerialPort.println("ROBOT START");
}

void onStop()
{
  runRobot = false;
  SerialPort.println("ROBOT STOP");
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

void onNormalise()
{
  readSensors();
  printRij("NORMALISED:", sensorValue);
}

void onPosition()
{
  SerialPort.print("Position = ");
  SerialPort.println(calculatePosition());
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

  SerialPort.println("ESP START");

  /// EEPROM ///
  Wire.begin(PIN_SDA, PIN_SCL);

  loadSettings();

  if (params.magic != PARAM_MAGIC ||
      params.maxSpeed < 0 || params.maxSpeed > 255 ||
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

  /// Start/stop-knop ///
  pinMode(PIN_KNOP, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_KNOP), knopISR, FALLING);

  /// Seriele commando's ///
  sCmd.addCommand("set", onSet);
  sCmd.addCommand("debug", onDebug);
  sCmd.addCommand("start", onStart);
  sCmd.addCommand("stop", onStop);
  sCmd.addCommand("calibrate", onCalibrate);
  sCmd.addCommand("normalise", onNormalise);
  sCmd.addCommand("position", onPosition);
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
  server.on("/normaliseTest", handleNormaliseTest);
  server.on("/linePosition", handleLinePosition);

  server.begin();

  SerialPort.println("Webserver gestart");
  SerialPort.println("Commando's: set <param> <waarde>, debug, start, stop,");
  SerialPort.println("            calibrate <white|black>, normalise, position");
  SerialPort.println("ready");

  previous = micros();
}

// ============================
// Loop
// ============================

void loop()
{
  // communicatie (niet blokkerend)
  server.handleClient();
  sCmd.readSerial();

  // vaste cyclus
  unsigned long current = micros();

  if (current - previous >= params.cycleTime)
  {
    previous = current;

    // sensoren uitlezen + normaliseren + positie
    lijnPositie = calculatePosition();

    if (runRobot)
    {
      /* pid regeling */
      /* aansturen motoren */
    }
    else
    {
      /* motoren stoppen */
    }

    // geen seriele uitvoer in het cyclische deel
  }

  unsigned long difference = micros() - current;
  if (difference > calculationTime) calculationTime = difference;

  // debug: 1x per seconde status, buiten het cyclische deel
  if (params.debug && millis() - vorigeDebug >= 1000)
  {
    vorigeDebug = millis();

    SerialPort.print(runRobot ? "RUN" : "STOP");
    SerialPort.print("  pos = ");
    SerialPort.print(lijnPositie);
    SerialPort.print("  max. rekentijd = ");
    SerialPort.print(calculationTime);
    SerialPort.println(" us");
  }
}
