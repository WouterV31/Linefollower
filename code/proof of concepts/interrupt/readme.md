# start/stop interrupt proof of concept
minimale hard- en software die de correcte werking van een start/stop drukknop aantoont, gebruik makend van een hardware interrupt

### hardware
* ESP32-S3 SuperMini
* monostabiele drukknop (KEY1)
* voeding: 18650-batterijen, schakelaar, DC-DC step-down naar 3,3V, power-led (zoals in het volledige schema)

### schema
[schema interrupt.pdf](schema%20interrupt.pdf)

* de drukknop hangt tussen **GPIO7** en **GND**
* er is geen externe weerstand: de interne pull-up van de ESP32-S3 houdt de ingang hoog zolang de knop niet ingedrukt is

### code
[StartStop_Interrupt_POC.ino](StartStop_Interrupt_POC/StartStop_Interrupt_POC.ino)

* GPIO7 wordt ingesteld als `INPUT_PULLUP`: niet ingedrukt = hoog, ingedrukt = laag
* een hardware interrupt op de dalende flank (`FALLING`) roept de ISR `knopISR` op, ongeacht wat de rest van de code op dat moment doet
* de ISR keert de variabele `run` om (start ↔ stop). De variabelen die zowel in de ISR als in de loop gebruikt worden zijn `volatile`
* **debounce**: een mechanische drukknop "dendert" bij het indrukken en loslaten, waardoor één druk meerdere dalende flanken (en dus meerdere interrupts) kan geven. De ISR onthoudt het tijdstip van de laatst aanvaarde druk (`millis()`) en negeert elke nieuwe flank binnen `debounceTijd` (200 ms). Zo geeft één druk precies één wissel tussen RUN en STOP
* de loop stuurt elke seconde de status (RUN of STOP) naar de seriële monitor (115200 baud), samen met het aantal aanvaarde drukken en het totaal aantal interrupts. Het verschil tussen beide toont hoeveel dender er weggefilterd werd
* de `delay(1000)` in de loop staat er bewust: ze stelt code voor die bezig is. Een druk tijdens die delay wordt toch geregistreerd, wat aantoont dat de start/stop non-blocking werkt

### resultaat
*(nog in te vullen na de test)*

Testprocedure:
1. sketch uploaden, seriële monitor openen op 115200 baud → `Status = STOP`
2. knop 1× kort indrukken → status wordt `RUN`, `drukken` stijgt met 1
3. knop opnieuw indrukken → status wordt `STOP`
4. een tiental keer drukken, ook snel na elkaar → elke druk geeft precies één wissel; noteer hoeveel `interrupts` er geteld werden tegenover `drukken`
5. de knop indrukken net nadat een regel verschenen is (dus tijdens de `delay`) → de wissel wordt bij de volgende regel getoond

Meting:

| aantal keer gedrukt | drukken (geteld) | interrupts (geteld) | correct gewisseld? |
|---|---|---|---|
| | | | |

*(screenshot seriële monitor / foto / video van de opstelling toevoegen)*
