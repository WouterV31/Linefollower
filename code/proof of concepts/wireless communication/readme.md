# draadloze communicatie proof of concept
minimale hard- en software waarmee aangetoond wordt dat duplex kan gecommuniceerd worden tussen de microcontroller en een smartphone of laptop, gebruik makend van een webbrowser (webinterface via wifi)
<br />

### hardware
* ESP32-S3 SuperMini (ingebouwde wifi, geen extra module nodig)
* voeding: 18650-batterij → schakelaar → DC-DC buck naar 3,3 V (of via USB)

### schema
[schema communicatie.pdf](schema%20communicatie.pdf)

Op het schema staat ook de I2C-EEPROM (SDA = GPIO6, SCL = GPIO5). Die wordt in deze proof of concept niet gebruikt, pas in het basisprogramma.

### code
[Wireless_communication.ino](Wireless_communication/Wireless_communication.ino)

### configuratie
* de ESP32 start een eigen wifi-netwerk (access point), er is dus geen router nodig
  * SSID: `LineFollower`
  * wachtwoord: zie `password` in de code
  * IP-adres: `192.168.4.1` (standaard voor een ESP32-access point, wordt ook naar de seriële monitor gestuurd)
* de ESP32 draait een webserver op poort 80
  * `/` toont de webpagina met de huidige waarde van Kp, de uptime en het aantal loops per seconde
  * `/save?kp=...` stuurt een nieuwe waarde voor Kp naar de robot
* seriële monitor: 115200 baud

### opmerkingen
* **duplex**: de smartphone/laptop stuurt een waarde naar de robot (Kp), de robot stuurt een webpagina terug met de ontvangen waarde en met gegevens van de robot zelf (uptime, loops per seconde)
* **non-blocking**: `loop()` wacht nergens op. `server.handleClient()` kijkt enkel of er een verzoek binnengekomen is en keert meteen terug. Om dat aan te tonen telt de code hoeveel keer `loop()` per seconde doorlopen wordt. Dat getal blijft hoog, ook terwijl er waarden verstuurd worden. In het finale programma draait de regelcyclus (sensoren + PID + motoren) op die manier gewoon verder tijdens het instellen van parameters.
* er staat bewust geen `while(!Serial)` in `setup()`: dan zou de robot zonder USB-kabel blijven wachten
* ontvangen waarden worden ook naar de seriële monitor gestuurd ter controle
* de waarde van Kp gaat verloren bij een herstart. Bewaren in EEPROM gebeurt in het basisprogramma.

### gebruiksaanwijzing
1. upload de code naar de ESP32-S3
2. verbind de smartphone of laptop met het wifi-netwerk `LineFollower`
3. surf naar `192.168.4.1`
4. vul een waarde in voor Kp en klik op *Opslaan*
5. de pagina toont de nieuwe waarde, en de seriële monitor toont `Ontvangen Kp = ...`
6. kijk in de seriële monitor naar `Loops per seconde: ...`: die waarde zakt niet in terwijl je de pagina gebruikt

### resultaat
*(nog in te vullen: screenshot van de webpagina op de smartphone/laptop + screenshot van de seriële monitor met `Ontvangen Kp` en `Loops per seconde`, eventueel een korte video)*

| test | resultaat |
|---|---|
| verbinden met `LineFollower` | |
| pagina `192.168.4.1` laadt | |
| Kp versturen → zichtbaar in seriële monitor | |
| Kp versturen → nieuwe waarde op de pagina | |
| loops per seconde zonder webverkeer | |
| loops per seconde tijdens versturen | |
| werkt op batterij (zonder USB) | |
