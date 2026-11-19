# Sensoren proof of concept

minimale hard- en software die aantoont dat minimaal 6 sensoren onafhankelijk van elkaar kunnen uitgelezen worden (geen calibratie, normalisatie of interpolatie). Hierbij moet een zo groot mogelijk bereik van de AD converter benut worden (indien van toepassing)

### hardware
* ESP32-S3 SuperMini
* QTR-8A (8 analoge IR-reflectiesensoren)

### schema
[schema sensor.pdf](schema%20sensor.pdf)

### aansluitingen
| QTR-8A | ESP32-S3 | ADC |
|--------|----------|-----|
| D1 | GPIO8 | ADC1 |
| D2 | GPIO9 | ADC1 |
| D3 | GPIO10 | ADC1 |
| D4 | GPIO11 | ADC2 |
| D5 | GPIO12 | ADC2 |
| D6 | GPIO13 | ADC2 |
| D7 | GPIO1 | ADC1 |
| D8 | GPIO2 | ADC1 |

### code
[Sensoren_Uitlezen_POC.ino](Sensoren_Uitlezen_POC/Sensoren_Uitlezen_POC.ino)

De 8 sensoren worden om de 200 ms uitgelezen met `analogRead` (12-bit ADC van de ESP32-S3, bereik 0 - 4095, demping 11 dB = grootste meetbereik). De ruwe waarde per sensor gaat naar de seriële monitor (115200 baud). Per sensor wordt ook de minimum- en maximumwaarde bijgehouden.

Commando's via de seriële monitor:

| commando | functie |
|---|---|
| `w` | wifi access point aan/uit (om te testen of de metingen met wifi aan blijven kloppen) |
| `r` | min/max van alle sensoren wissen |
| `s` | tabel met min/max per sensor tonen (markeert waarden 0 of 4095) |

### werking QTR-8A
Elke sensor is een spanningsdeler van een fototransistor en een vaste weerstand van 47 kΩ naar Vcc. Wit reflecteert veel IR-licht → uitgang naar 0 V (lage waarde). Zwart reflecteert weinig → uitgang naar Vcc (hoge waarde). Optimale afstand tot de ondergrond is 3 mm, maximaal 6 mm. Bij Vcc = 3,3 V moeten de pinnen *3.3V BYPASS* doorverbonden worden, anders branden de IR-leds te zwak (bron: QTR-8x datasheet).

Een gemeten waarde van 0 of 4095 is geen bruikbare informatie: de spanning ligt dan op of buiten de grens van het meetbereik.

### resultaat

#### meting 1 (probleem)
![resultaat seriële monitor](resultaat%20seriele%20monitor.png)

* wit: 2841 - 3451 → gebruikt maar een klein deel van het bereik
* zwart: 4095 → ADC verzadigd, geen bruikbare waarde

#### meting 2 (na aanpassing)
Aanpassing: *(in te vullen: hoogte sensor, 3.3V BYPASS, ...)*

Hoogte sensor boven de ondergrond: … mm

Werkwijze: sensor over wit schuiven → `r` → enkele seconden alle sensoren over wit → `s`. Daarna idem over zwart. Dan `w` (wifi aan) en alles herhalen.

| sensor | wit, wifi uit | zwart, wifi uit | wit, wifi aan | zwart, wifi aan |
|---|---|---|---|---|
| D1 | | | | |
| D2 | | | | |
| D3 | | | | |
| D4 | | | | |
| D5 | | | | |
| D6 | | | | |
| D7 | | | | |
| D8 | | | | |

### besluit
*(in te vullen na meting 2)*
