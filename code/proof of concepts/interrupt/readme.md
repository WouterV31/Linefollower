# start/stop interrupt proof of concept
minimale hard- en software die de correcte werking van een start/stop drukknop aantoont, gebruik makend van een hardware interrupt

### hardware
* ESP32-S3 SuperMini
* monostabiele drukknop (KEY1)
* voeding: 18650-batterijen, schakelaar, DC-DC step-down naar 3,3V, power-led met 220 Ω (zoals in het volledige schema)

### schema
[schema interrupt.pdf](schema%20interrupt.pdf) · origineel EasyEDA-bestand: [schema interrupt.epro2](schema%20interrupt.epro2)

* de drukknop hangt tussen **GPIO11** en **GND**
* er is geen externe weerstand: de interne pull-up van de ESP32-S3 houdt de ingang hoog zolang de knop niet ingedrukt is

### code
[StartStop_Interrupt_POC.ino](StartStop_Interrupt_POC/StartStop_Interrupt_POC.ino)

* GPIO11 staat als `INPUT_PULLUP`: niet ingedrukt = hoog, ingedrukt = laag
* een hardware interrupt op de dalende flank (`FALLING`) roept de ISR `knopISR` op, ongeacht wat de rest van de code op dat moment doet. De ISR doet bewust zo weinig mogelijk: ze zet enkel een vlag (`knopFlag`) en onthoudt het tijdstip van de flank. Variabelen die in de ISR en de loop gebruikt worden zijn `volatile`
* **debounce**: een mechanische drukknop "dendert", waardoor één druk meerdere flanken kan geven. De loop verwerkt de vlag pas na `debounceTijd` (30 ms) en controleert dan of de knop nog echt laag is. Zo telt een korte storing niet als druk
* **vergrendeling**: na een geldige druk wisselt `run` (STOP ↔ RUN) en wordt de knop vergrendeld tot hij minstens `losTijd` (50 ms) stabiel losgelaten is. Zo geeft één druk precies één wissel, ook als de knop lang ingedrukt blijft of dendert bij het loslaten
* bij elke wissel verschijnt `RUN` of `STOP` in de seriële monitor (115200 baud); daarnaast toont de loop elke seconde de status (`status: RUN` / `status: STOP`)

### resultaat
Getest op 20/11/2026 (Arduino IDE 2.3.6, board "ESP32S3 Dev Module").

![seriële monitor](seriele%20monitor.png)

In de seriële monitor is de knop 8 keer ingedrukt (11:22:56 → 11:23:14). Elke druk gaf precies één wissel (RUN, STOP, RUN, …), zonder dubbele wissels door dender. Tussen de drukken blijft de status elke seconde correct gemeld.

*(foto / video van de opstelling nog toe te voegen)*
