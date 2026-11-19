# H-Bridge proof of concept

minimale hard- & software + stappenplan dat aantoont dat 2 motoren onafhankelijk van elkaar kunnen draaien, en (traploos) regelbaar zijn in snelheid en draairichting.

### hardware
* ESP32-S3 SuperMini
* DRV8833 h-brug
* 2x N20 DC-motor

### schema
[schema H-brug.pdf](schema%20H-brug.pdf)

### aansluitingen
| DRV8833 | ESP32-S3 |
|---------|----------|
| IN1 | GPIO4 |
| IN2 | GPIO3 |
| IN3 | GPIO2 |
| IN4 | GPIO1 |

Motor A hangt aan IN1/IN2, motor B aan IN3/IN4.

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

### resultaat
*(nog in te vullen: beschrijving / foto / video van de werkende opstelling)*
