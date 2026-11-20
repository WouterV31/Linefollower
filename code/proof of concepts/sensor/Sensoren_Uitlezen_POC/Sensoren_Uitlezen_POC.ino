/*
  Sensor proof of concept
  QTR-8A + ESP32-S3 SuperMini

  Leest de 8 sensoren onafhankelijk uit (ruwe ADC-waarde, 12 bit: 0 - 4095)
  en houdt per sensor de minimum- en maximumwaarde bij.
  Wifi (access point) kan aan/uit gezet worden om te testen of de metingen
  met wifi aan nog kloppen. Alle sensoren zitten op ADC1 (GPIO1..10):
  ADC2 (GPIO11..20) wordt gedeeld met wifi en geeft dan foute waarden.
  Per uitgang een weerstand van 100 kOhm naar GND, zodat zwart onder de
  bovengrens van de ADC blijft (anders 4095).

  Commando's via de seriele monitor (115200 baud):
    w  wifi access point aan/uit
    r  min/max van alle sensoren wissen
    s  tabel met min/max per sensor tonen

  Aansluitingen:
  D1 -> GPIO1
  D2 -> GPIO2
  D3 -> GPIO5
  D4 -> GPIO6
  D5 -> GPIO7
  D6 -> GPIO8
  D7 -> GPIO9
  D8 -> GPIO10
*/

#include <WiFi.h>

const int aantalSensoren = 8;
const int sensor[aantalSensoren] = {1, 2, 5, 6, 7, 8, 9, 10};

int minimum[aantalSensoren];
int maximum[aantalSensoren];

bool wifiAan = false;
unsigned long vorigeMeting = 0;

void resetMinMax()
{
  for (int i = 0; i < aantalSensoren; i++)
  {
    minimum[i] = 4095;
    maximum[i] = 0;
  }
}

void wisselWifi()
{
  wifiAan = !wifiAan;
  if (wifiAan)
  {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("LineFollower");
    Serial.println("Wifi AAN (AP LineFollower)");
  }
  else
  {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("Wifi UIT");
  }
  resetMinMax();
}

void toonMinMax()
{
  Serial.println();
  Serial.print("min/max, wifi ");
  Serial.println(wifiAan ? "AAN" : "UIT");
  for (int i = 0; i < aantalSensoren; i++)
  {
    Serial.print("D");
    Serial.print(i + 1);
    Serial.print("  min ");
    Serial.print(minimum[i]);
    Serial.print("  max ");
    Serial.print(maximum[i]);
    if (minimum[i] == 0 || maximum[i] == 4095)
    {
      Serial.print("  <-- grens ADC bereikt");
    }
    Serial.println();
  }
  Serial.println();
}

void setup()
{
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);  // grootste meetbereik van de ADC
  WiFi.mode(WIFI_OFF);
  resetMinMax();
  Serial.println("Commando's: w = wifi aan/uit, r = reset min/max, s = toon min/max");
}

void loop()
{
  if (Serial.available())
  {
    char c = Serial.read();
    if (c == 'w') wisselWifi();
    if (c == 'r') { resetMinMax(); Serial.println("min/max gewist"); }
    if (c == 's') toonMinMax();
  }

  if (millis() - vorigeMeting >= 200)
  {
    vorigeMeting = millis();

    Serial.print(wifiAan ? "wifi AAN | " : "wifi UIT | ");
    for (int i = 0; i < aantalSensoren; i++)
    {
      int waarde = analogRead(sensor[i]);
      if (waarde < minimum[i]) minimum[i] = waarde;
      if (waarde > maximum[i]) maximum[i] = waarde;

      Serial.print("D");
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(waarde);
      Serial.print("\t");
    }
    Serial.println();
  }
}
