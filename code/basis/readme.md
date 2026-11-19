# basis programma

volgende zaken werken samen in dit template project  

* instelbare parameters mbv. SerialCommands 
  * cyclus tijd
  * debug on/off
  * start/stop
* parameters worden opgeslagen in eeprom geheugen
* start/stop kan ook geactiveerd worden mbv. een externe interrupt
* draadloze communicatie

Met behulp van de cyclus kan je bvb. een knipperende led programmeren.

## Code

Sketch: [Basisprogramma/Basisprogramma.ino](Basisprogramma/Basisprogramma.ino), met de bibliotheek SerialCommand (uit het leerpad) in dezelfde map.
Vertrekt van de webinterface-code met I2C-EEPROM en van het basisprogramma `cyclus.ino` uit het leerpad.

| onderdeel | uitwerking |
|---|---|
| draadloze communicatie | wifi access point `LineFollower`, webinterface op `192.168.4.1` |
| seriële commando's | SerialCommand, 115200 baud, afsluiten met CR (+LF) |
| EEPROM | externe 24C256 via I2C (SDA = GPIO6, SCL = GPIO5); alle parameters in één struct op adres 0, geschreven per pagina met ACK polling; een magic-waarde herkent een lege/ongeldige EEPROM → standaardwaarden |
| interrupt | drukknop op GPIO7 naar GND (interne pull-up), `FALLING`, debounce 200 ms in de ISR; wisselt start/stop |
| cyclus | vaste cyclustijd `cycleTime` [µs] met `micros()`; de grootste rekentijd wordt bijgehouden en getoond via `debug` of de webpagina. In de cyclus: sensoren uitlezen, normaliseren (wit 0 – zwart 1000) en lijnpositie berekenen. PID en motoren volgen in de finale code |

### Instelbare parameters

| parameter | bereik | standaard |
|---|---|---|
| `cycle` (cyclustijd, µs) | 100 – 100000 | 1000 |
| `kp` | 0 – 50 | 0,30 |
| `ki` | 0 – 50 | 0,00 |
| `kd` | 0 – 100 | 2,00 |
| `speed` (max. snelheid) | 0 – 255 | 180 |
| `debug` | on / off | off |

Parameters wijzigen en kalibreren kan alleen als de robot stilstaat (een EEPROM-schrijfactie blokkeert enkele ms).

### Seriële commando's

| commando | functie |
|---|---|
| `set <cycle\|kp\|ki\|kd\|speed\|debug> <waarde>` | parameter instellen en opslaan in EEPROM |
| `debug` | alle parameters, kalibratie en grootste rekentijd sinds de vorige `debug` |
| `start` / `stop` | robot starten / stoppen |
| `calibrate <white\|black>` | sensoren kalibreren (gemiddelde van 100 metingen) en opslaan |
| `normalise` | genormaliseerde sensorwaarden tonen |
| `position` | lijnpositie tonen (−3500 … 3500) |

Met `debug on` stuurt de robot elke seconde status, positie en rekentijd naar de seriële monitor (buiten het cyclische deel).

### Webinterface

Dezelfde functies als de seriële commando's: PID-parameters, max. snelheid en cyclustijd opslaan, start/stop, debug aan/uit, kalibratie wit/zwart, normalisatie- en positietest. De statuskaart toont ook de grootste rekentijd en de genormaliseerde sensorwaarden.

### Resultaat

*(nog te testen op de robot: cyclustijd vs. rekentijd, start/stop met knop en webinterface, parameters na spanningsloos maken)*
