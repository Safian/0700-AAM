# MPC Key 61 – touch strip módok az útmutatóból

Forrás: *MPC Standalone OS User Guide v3.10* (a PDF-oldal = a nyomtatott oldalszám), fejezet: **Touch Strip, 101–103. oldal**; kiegészítésként a *Preferences > Hardware* rész (100. oldal), a *MIDI Control Mode* (Touch Sense, Note, Low/High Range, 334. oldal) és a *Key 61 Quickstart v1.2* (33–35. elem). Csak az útmutató szövege szerepel itt; amit az útmutató nem mond ki, az **„nincs a doksiban”** jelzést kapott. A hardveren mért adatok külön, a végén vannak.

## 1. Általános működés (101. o.)
- A **Touch Strip** gomb körbeléptet a módokon; a gombot nyomva tartva a módot a touchscreenről lehet kiválasztani. A **Touch FX** gomb azonnal a Touch FX módot választja. (Quickstart 34–35.: ugyanez; Shift + gomb = a konfigurációs ablak.)
- A módok sorrendje a listában: **Q-Link, Pitch Bend, Pad Level, Mod Wheel, Track Level, Sustain, Touch FX, Expression, Notes, Crossfader, Note Strum, Off.** (12 mód.)
- Az alapértelmezett módot a **Preferences > Hardware** menüben lehet beállítani.
- Használat: az ujjat a sávra téve fel-le mozgatva vezérli a kiválasztott paramétert.
- **Shift + Touch Strip** vagy **Shift + Touch FX** nyitja a mód beállítását, „ha van” ilyen (módonként lent). A beállítóablakok a **szoftverben** (az MPC képernyőn) nyílnak.
- Nincs a doksiban: a módok közötti léptetés irányának és a Touch Strip LED-ének állapota (a MIDI Control Mode fejezet szerint a strip „Light LED” beállításának nincs funkciója a hardveren).

## 2. A Kickben létező módok

| Mód | Mit vezérel (doksi) | Beállítás (Shift + gomb) | Oldal |
|---|---|---|---|
| **Pad Level** | az aktuális **Pad Level**-t | nincs a doksiban | 101 |
| **Mod Wheel** | a **MIDI CC1** modulációt, mint egy mod wheel | nincs beállítás | 103 |
| **Track Level** | az aktuális **Track Level**-t | nincs a doksiban | 101 |
| **Sustain** | mint egy sustain pedál: bekapcsolva a hangok addig szólnak, amíg ki nem kapcsolják | nincs beállítás | 103 |
| **Expression** | a **MIDI CC11** expressziót, mint egy külső expression pedál | nincs beállítás | 103 |
| **Pitch Bend** (a Kick is implementálja) | mint egy pitch wheel | **Pitch Bend Settings** ablak: **Relative Mode** (bekapcsolva a középpont ott van, ahová az ujjat először teszed; kikapcsolva a sáv fix, a közepe a változatlan hang), **Bend Up Range** / **Bend Down Range** (félhang), **Global Pitch Bend** (felülírja a sávok egyéni beállítását). A tartományok csak MPC-hangszerekre érvényesek, harmadik féltől származó pluginokra nem. | 103 |

Nincs a doksiban, módonként (a Kick fejlesztéséhez ezekre nincs útmutató-fedezet):
- **Pad Level / Track Level:** a tartomány, a skála (0–127 vagy dB), és hogy elengedéskor marad-e az érték.
- **Mod Wheel (CC1) / Expression (CC11):** a visszaugrás elengedéskor, és hogy a CC a MIDI portokra is kimegy-e (a szöveg csak a CC számot nevezi meg).
- **Sustain:** a be/ki küszöb a sáv mentén, és hogy a sáv érintésre kapcsol vagy tart.
- **Pitch Bend:** a visszaugrás elengedéskor (a szöveg a „fix közép” és a „relatív közép” viselkedést írja le, a visszaugrást nem).

## 3. A Kickben nem létező dologra vonatkozó módok
- **Q-Link** (101. o.): a strip egy Q-Linket vezérel. Beállítás: **Touch Strip Q-Link** ablak, **Q-Link** mező (1–16); az ablak nyitva létekor a 16 pad egyikének megnyomása is kiválasztja a Q-Linket.
- **Crossfader** (103. o.): virtuális crossfader; a sáv közepe a crossfader közepe, lefelé az A oldal, felfelé a B oldal. A paramétereket a **Q-Link Edit > Crossfader** menüben lehet hozzárendelni.
- **Note Strum** (103. o.): a lenyomott hangok „gitárszerű” pengetése. Beállítások: **Notes Sound When Strip Not Pressed**, **Aftertouch to Velocity**. A beállítóablak megnyitásáról a doksi nem ír.

## 4. A módok, amelyekről a Kick viszonyát ebben a fájlban nem vizsgáltam
- **Touch FX** (101–102. o.): a strip a **Touch FX plugin** paraméterét vezérli (insert effekt a kijelölt kimeneten). A Touch FX gomb nyomva tartva a presetlistát nyitja; a **Shift + Touch FX** a **Touch FX Setup** ablakot (Destination: Track / Return / Submix / Output, **Insert Touch FX**), majd a **Shift + Touch Strip** ugyanazt újra. Beállítások: **Touch FX Preset** (a Data Dial vagy a –/+ is léptet), **Latch** (a pozíció az elengedés után is marad), **Touch > Wet/Dry**, **Wet/Dry**, **Attack**, **Release**, **Touch FX Setup** (Assigned Parameter). A plugin ablak bal oldala: Touch Strip, **Touch Enable**, Assigned Parameter, Touch Value, Wet Amount, Touch Value / Wet/Dry Lock, Touch Attack, Touch Release.
- **Notes** (103. o.): a strip hangokat játszik a kijelölt sávban. Beállítás: **Touch Strip Notes Config** ablak: **Root Note**, **Scale**, **Octave**, **Velocity**, **Number of Notes** (hány hangot fed le a sáv hossza).
- **Off:** a doksi külön leírást nem ad.

## 5. Touch Sense (MIDI Control Mode, 334. o.)
A strip (és a Q-Link gombok) MIDI-beállításában:
- **Touch Sense On:** az érintés **Note On** üzenetet küld a szoftvernek (a doksi szerint így működik a hardver alapból); **Off:** csak CC-t küld, Note On-t nem.
- **Note:** az érintéskor küldött MIDI jegyzet száma (0–127 vagy C-2…G8); csak Touch Sense On mellett érvényes.
- **Low Range / High Range:** a strip legalacsonyabb / legmagasabb értéke (0–127).
- A **Mode (Absolute/Relative)** a strip esetén **nem elérhető**.
- **CC Number** és **MIDI Channel:** beállítható; az alapértelmezett érték nincs a doksiban.

## 6. Mért adatok (hardver, nem a doksiból)
A Key 61-en nyers MIDI-naplóból (Port 1, 2026-11-02): a strip pozíciója **CC 33 (MSB) + CC 34 (LSB)**, ch1, 14 bites; az érintés/elengedés **Note 78** be/ki (ch1). Ez mért tény, nem dokumentáció; hogy a Touch Sense mely beállításnál történt a mérés, nem ellenőriztem; a módonkénti leképezés (melyik módban mi megy ki) mérve nincs.
