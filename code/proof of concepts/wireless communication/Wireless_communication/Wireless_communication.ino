// Proof of concept: non-blocking draadloze communicatie (wifi + webinterface)
// ESP32-S3 SuperMini als access point, webserver op poort 80.
//  - laptop/smartphone -> robot : nieuwe waarde voor Kp via /save?kp=...
//  - robot -> laptop/smartphone : webpagina met Kp, uptime en aantal loops per seconde
// Non-blocking: loop() wacht nergens op. server.handleClient() kijkt enkel of er
// een verzoek is en keert meteen terug. Het aantal loops per seconde blijft dus
// hoog, ook terwijl er met de webinterface gewerkt wordt.

#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "LineFollower";
const char* password = "GiliamWouter";

WebServer server(80);

float kp = 0.30;

unsigned long loopTeller = 0;      // loops sinds de laatste meting
unsigned long loopsPerSeconde = 0; // laatste gemeten waarde
unsigned long vorigeMeting = 0;    // tijdstip laatste meting (ms)

void sendPage()
{
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head>
  <body>

  <h1>Wireless Communication PoC</h1>

  <p>Huidige Kp: %KP%</p>
  <p>Uptime: %UPTIME% s</p>
  <p>Loops per seconde: %LPS%</p>

  <form action="/save">
  Kp
  <input type="number" step="0.01" name="kp" value="%KP%">
  <input type="submit" value="Opslaan">
  </form>

  <p><a href="/">Vernieuwen</a></p>

  </body>
  </html>
  )rawliteral";

  html.replace("%KP%", String(kp, 2));
  html.replace("%UPTIME%", String(millis() / 1000));
  html.replace("%LPS%", String(loopsPerSeconde));

  server.send(200, "text/html", html);
}

void handleRoot()
{
  sendPage();
}

void handleSave()
{
  if (server.hasArg("kp"))
  {
    kp = server.arg("kp").toFloat();

    Serial.print("Ontvangen Kp = ");
    Serial.println(kp);
  }

  sendPage();
}

void setup()
{
  Serial.begin(115200);   // geen while(!Serial): moet ook zonder USB werken

  WiFi.softAP(ssid, password);

  Serial.print("Access point gestart, IP-adres: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/save", handleSave);

  server.begin();
}

void loop()
{
  server.handleClient();   // non-blocking: keert meteen terug als er geen verzoek is

  // bewijs dat loop() blijft draaien: loops per seconde tellen
  loopTeller++;
  unsigned long nu = millis();
  if (nu - vorigeMeting >= 1000)
  {
    loopsPerSeconde = loopTeller;
    loopTeller = 0;
    vorigeMeting = nu;

    Serial.print("Loops per seconde: ");
    Serial.println(loopsPerSeconde);
  }
}
