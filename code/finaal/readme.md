# finale code

[LineFollower_finaal.ino](LineFollower_finaal/LineFollower_finaal.ino)

Uitbreiding van het basisprogramma (webinterface + EEPROM + start/stop) met sensorverwerking, PID-regelaar en motoraansturing.

### opbouw van één cyclus
De cyclus wordt om de `cycleTime` µs uitgevoerd. Webinterface en seriële monitor worden buiten de cyclus afgehandeld.

1. **sensoren uitlezen en normaliseren**: per sensor `map(ruw, wit, zwart, 0, 1000)` → wit = 0, zwart = 1000
2. **lijn kwijt?** ligt de zwartste sensor onder `LINE_LOST_LEVEL` (500), dan stopt de robot
3. **interpoleren** naar een positie in mm (0 = midden, sensorafstand QTR-8A = 9,525 mm), instelbaar via de webinterface:
   * gewogen gemiddelde: Σ(waarde · positie) / Σ waarde
   * kwadratisch: top van de parabool door de zwartste sensor en zijn 2 buren
4. **PID**: `error = -positie`
   * P: `Kp · error`
   * I: `iTerm += Ki* · error`, begrensd op ±510 (anti-windup), op 0 gezet bij elke start
   * D: `Kd* · (error - vorige error)`
   * output begrensd op ±510
5. **motoren**: verdeling van de output over links en rechts met `power` en `diff`, teken = draairichting, absolute waarde = PWM

Ki en Kd worden ingegeven in 1/s en s. Intern wordt `Ki* = Ki · cyclustijd` en `Kd* = Kd / cyclustijd` opgeslagen, zodat het gedrag ongeveer gelijk blijft als de cyclustijd verandert.

### instellingen (opgeslagen in de 24C256 EEPROM)

| parameter | bereik | betekenis |
|---|---|---|
| power | 0 - 255 | basisvermogen van de motoren |
| diff | 0 - 1 | verdeling van de bijsturing: 0 = enkel het binnenwiel trager, 1 = enkel het buitenwiel sneller |
| Kp | ≥ 0 | proportionele versterking |
| Ki | ≥ 0 [1/s] | integrerende versterking |
| Kd | ≥ 0 [s] | differentiërende versterking |
| cycle time | 100 - 100000 µs | cyclustijd |
| interpolatie | gewogen gemiddelde / kwadratisch | methode voor de positie |
| wit / zwart | ADC 0 - 4095 | kalibratiewaarden per sensor |

### bediening
* wifi `LineFollower` → http://192.168.4.1: start, stop, debug, kalibratie wit/zwart, instellingen
* drukknop: start/stop (interrupt, debounce 200 ms)
* seriële monitor (115200 baud): `s` start/stop, `w` wit, `b` zwart, `p` instellingen, `n` genormaliseerde waarden, `l` positie, `d` rekentijd

Met debug aan toont de webpagina live: positie, output, motorvermogens, grootste rekentijd en per sensor ruw / genormaliseerd / wit / zwart.

De robot start niet zolang niet gekalibreerd is (zwart moet hoger liggen dan wit voor elke sensor). Instellingen opslaan en kalibreren kan enkel als de robot stilstaat.

### eerste test (volgens leerpad)
1. kalibreer wit en zwart, controleer met debug dat D1 de meest linkse sensor is
2. controleer dat de rekentijd maar een fractie van de cyclustijd is, zet de cyclustijd op ongeveer 2× de rekentijd
3. power = 100, diff = 0,5, Kp = 0 → start: beide wielen moeten vooruit draaien. Zo niet: pinnen van die motor omwisselen
4. Kp = 1 → robot op het parcours. Rijdt hij van de lijn weg in plaats van ernaartoe: `powerLeft` en `powerRight` omwisselen

### aandachtspunten
* de pintoewijzing is nog niet definitief: GPIO1 en GPIO2 worden zowel door de sensor (D7/D8) als door de H-brug gebruikt. Zolang dat zo is, meldt het programma dit op de seriële monitor en blijven de motoren geblokkeerd
* sensoren D4 - D6 zitten op ADC2-pinnen (GPIO11 - 13), die kunnen met wifi aan foute waarden geven
* `LINE_LOST_LEVEL` en de cyclustijd moeten nog afgesteld worden tijdens het testen
