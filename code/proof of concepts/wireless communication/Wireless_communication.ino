#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "LineFollower";
const char* password = "GiliamWouter";

WebServer server(80);

float kp = 0.30;

void sendPage()
{
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <body>

  <h1>Wireless Communication PoC</h1>

  <form action="/save">

  Kp

  <input
  type="number"
  step="0.01"
  name="kp"
  value="%KP%">

  <input
  type="submit"
  value="Opslaan">

  </form>

  </body>
  </html>
  )rawliteral";

  html.replace("%KP%", String(kp,2));

  server.send(200,"text/html",html);
}

void handleRoot()
{
  sendPage();
}

void handleSave()
{
  if(server.hasArg("kp"))
  {
    kp = server.arg("kp").toFloat();

    Serial.print("Ontvangen Kp = ");
    Serial.println(kp);
  }

  sendPage();
}

void setup()
{
  Serial.begin(115200);

  WiFi.softAP(ssid,password);

  Serial.println(WiFi.softAPIP());

  server.on("/",handleRoot);
  server.on("/save",handleSave);

  server.begin();
}

void loop()
{
  server.handleClient();
}