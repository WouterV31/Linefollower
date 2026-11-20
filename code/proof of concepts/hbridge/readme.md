# H-Bridge proof of concept

minimale hard- & software + stappenplan dat aantoont dat 2 motoren onafhankelijk van elkaar kunnen draaien, en (traploos) regelbaar zijn in snelheid en draairichting.

### hardware
* ESP32-S3 SuperMini
* DRV8833 h-brug
* 2x N20 DC-motor

### schema
[schema H-brug.pdf](schema%20H-brug.pdf) · origineel EasyEDA-bestand: [schema H-brug.epro2](schema%20H-brug.epro2)

### aansluitingen
| DRV8833 | ESP32-S3 |
|---------|----------|
| IN1 | GPIO13 |
| IN2 | GPIO12 |
| IN3 | GPIO4 |
| IN4 | GPIO3 |
| VCC | batterij + (na schakelaar) |
| GND | GND |

Motor A (M1) hangt aan OUT1/OUT2 (gestuurd door IN1/IN2), motor B (M2) aan OUT3/OUT4 (gestuurd door IN3/IN4).  
Deze pinnen zijn dezelfde als in de finale linefollower, zodat de code zonder aanpassingen op de robot draait.

### code
[Hbrug_POC.ino](Hbrug_POC/Hbrug_POC.ino)

De functie `stelMotorSnelheidIn(pin1, pin2, snelheid)` stuurt één motor aan met een waarde tussen -255 en 255:
* positief: PWM op pin1, pin2 laag → vooruit
* negatief: PWM op pin2, pin1 laag → achteruit
* 0: beide pinnen laag → stop

### stappenplan
De code doorloopt in een lus volgende testen, telkens 4 seconden. Elke test wordt ook naar de seriële monitor (115200 baud) gestuurd.

1. motor A 25% vooruit, motor B 100% vooruit
2. motor A 50% achteruit, motor B 100% vooruit
3. motor A stop, motor B 75% vooruit
4. beide motoren 100% achteruit
5. beide motoren stop

### aandachtspunten
* De DRV8833 wordt rechtstreeks gevoed door de 2 Li-ion cellen in serie (tot 8,4 V volgeladen). De N20-motoren zijn 6 V-types: bij 100% PWM krijgen ze dus meer dan hun nominale spanning. In deze POC wordt de PWM niet begrensd.

* Arduino IDE: board "ESP32S3 Dev Module", "USB CDC On Boot: Enabled" (seriële monitor via USB).

### resultaat
De POC werkt: getest op de hardware op 20/11/2026.

| test | verwacht | waargenomen |
|---|---|---|
| 1 | A traag vooruit, B volle snelheid vooruit | |
| 2 | A halve snelheid achteruit, B volle snelheid vooruit | |
| 3 | A stil, B 75% vooruit | |
| 4 | A en B volle snelheid achteruit | |
| 5 | A en B stil | |

*(foto / video van de werkende opstelling nog toe te voegen)*
