# LineFollower

Autonome linefollower robot, gebouwd in het kader van het syntheseproject (HOGENT).

![foto linefollower (nog toe te voegen)](images/empty.png)

## specifications

microcontroller: ESP32-S3 SuperMini

motors: 2x N20 6V, ±500 RPM, metal gear

h-bridge: DRV8833

sensors: QTR-8A (8 analoge IR-reflectiesensoren)

batteries: 2x 18650 Li-ion in serie (2S, 8,4V) + DC-DC step-down buck converter naar 3,3V

wireless communication: wifi (ESP32 als access point) + webinterface in de browser

distance sensor - motors: *(nog in te vullen)*

weight: *(nog in te vullen)*

speed: *(nog in te vullen)*

kostprijs: €50,85 (zie [bill of materials](bill%20of%20materials/))

## status

| onderdeel | status |
|-----------|--------|
| bill of materials | klaar |
| elektronisch schema (plan B) | klaar, definitieve pintoewijzing (PDF + EasyEDA-bronbestand) |
| proof of concepts | code en uitleg klaar, testen op hardware lopen nog |
| basisprogramma | compileert, nog te testen op de robot |
| finale code | compileert, nog te testen en te tunen op de robot |
| mechanisch, instructable, gebruiksaanwijzing | nog te doen |

## overzicht repository

| map | inhoud |
|-----|--------|
| [bill of materials](bill%20of%20materials/) | lijst van alle onderdelen met prijs |
| [code/proof of concepts](code/proof%20of%20concepts/) | H-brug, sensoren, start/stop interrupt, draadloze communicatie |
| [code/basis](code/basis/) | basisprogramma |
| [code/finaal](code/finaal/) | finale code |
| [datasheets](datasheets/) | datasheets van de gebruikte componenten |
| [technische tekeningen](technische%20tekeningen/) | elektronisch schema en mechanische tekeningen |
| [instructable](instructable/) | stappenplan om de robot na te bouwen |
| [gebruiksaanwijzing](gebruiksaanwijzing/) | bediening, commando's, kalibratie en instellingen |
| [parcours](parcours/) | testparcours om af te drukken |
