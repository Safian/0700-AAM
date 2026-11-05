# Terv: képernyő-elhalványítás tétlenség után (érintésre ébred)

Állapot: TERV, nincs megvalósítva. A Kickben ma csak fix fényerő van (Settings > Hardware csúszka, `setBrightness`, indításkor érvényesül); tétlenségi időzítő, halványítás és ébresztés nincs (`git grep` megerősítette).

## Cél
Ha X ideig nincs bemenet, a háttérvilágítás lehalványul egy alacsony szintre; bármilyen érintés vagy hardver-bemenet visszaállítja. Az első érintés, ami ébreszt, csak ébreszt, nem nyom gombot.

## Kapcsolódási pontok (a kódban)
- Háttérvilágítás-írás: `PlatformServices::setBrightness(percent)` (`kick_PlatformServices.cpp`). Az MPC változat a `/sys/class/backlight/mipi-backlight/brightness` fájlba ír, 20..255 skálán (`formatBrightnessPercentToValue`). A régi Behringer-hardveren `runBoardCommand`.
- Beállítás: `KickHardware::brightness` (`kick_settings.cpp`, `IDs::screenBrightness`), UI: `kick_ui_settings_hardware.cpp`.
- Bemenet: egy helyen fut át minden érintés és hardver-esemény a hídban: `KickHardwareBridge::on_touch_down/move/up`, `dispatchTouch`, gombok, pad, jog, Q-Link, billentyűzet-MIDI (`kick_hardware_bridge.h`). Desktopon JUCE-egér/billentyű.

## Tervezett működés
1. **Beállítások (új, Preferences > Hardware):**
   - `Dim after`: Off / 1 / 5 / 10 / 30 perc (alap: Off, hogy a mostani viselkedés ne változzon).
   - `Dim level`: a normál fényerő százaléka (alap 20 %), a hardver minimuma (20/255) alá nem megy.
   - Mentés: `IDs::screenDimMinutes`, `IDs::screenDimLevel` a `KickHardware` fában, a `timeZoneUserSet` mintájára.
2. **Idle-felügyelet (egy komponens, `ScreenDimmer`):**
   - Egy `juce::Timer` (1 s) a message threaden; `lastInputMs` időbélyeg; `noteActivity()` hívja a híd minden bemeneti eseménye.
   - Állapotgép: `Awake` → (idő letelt) → `Dimmed` → (bemenet) → `Awake`.
   - Letelt idő: `setBrightness(dimLevel)` fokozatosan (3-5 lépés, ~1 s, ne hirtelen). Ébresztéskor azonnal az eredeti szintre (nem a csúszka felülírásával: a felhasználó beállított értéke a `KickHardware`-ben marad, a dim csak átmeneti sysfs-érték).
   - Nem halványít: ha hang szól/transzport fut, felvétel megy, modális ablak (Power, megerősítés) nyitva, vagy Sampler/Looper felvétel aktív. Opció: ezek külön kapcsolhatók. Alap: játszás közben ne halványuljon.
3. **Ébresztő érintés elnyelése:**
   - A hídban, ha az állapot `Dimmed`, a `touch_down` ébreszt és a hozzá tartozó `touch_move/up` sorozatot (slot szerint) el kell nyelni, amíg az ujj fel nem emelkedik. Így nem történik véletlen gombnyomás.
   - Hardver-gombok, padek, jog: ébresztenek, de azok működnek is (a hangszerjáték nem késhet). Pad-ütés: ne nyelje el soha.
4. **Hibakezelés:** ha a sysfs-fájl nem írható (lásd `[PREFS] MPC backlight sysfs not found`), a funkció kikapcsolja magát és naplóz egyet; nincs ismételt hiba.
5. **Más platformok:** desktop/iOS: no-op (a rendszer kezeli); a beállítás elrejtve vagy szürke.

## Naplózás és teszt
- `kick_log("prefs", ...)` a dim/ébresztés eseményekről.
- Host-teszt (`Kick/tests/platform`, ASAN/UBSAN): az állapotgép idő-injektálással (hamis óra): letelés → Dimmed, bemenet → Awake, ébresztő érintés elnyelése slot szerint, aktív felvétel/modális ablak tiltja a halványítást, Off nem halványít.
- Hardveres próba (ember kell): 1 perces beállítással tényleg lehalványul, az első érintés csak ébreszt, pad-ütés azonnal szól halvány állapotban is, újraindítás után a beállítás megmarad, a fényerő-csúszka továbbra is működik.

## Kockázatok
- A híd a közös, érzékeny fájl (a régi MPC-s érintés-hibák itt vannak); a módosítás minimális legyen: egyetlen `noteActivity()` hívás és az elnyelő szűrő a `dispatchTouch` elején.
- Az érintés-elnyelés hibája "nem tappolható képernyő" tüneteit adná: a hiba-esetben (pl. állapot beragad `Dimmed`-ben) időkorlátos visszaesés kell: ha a Dimmed állapot után N ms-ig érkezik bemenet, de nem ébredt fel, kényszerített `Awake`.
- Alapértelmezés Off, így a funkció nem érinti a mostani viselkedést.

## Nyitott kérdések (owner)
1. Alap `Dim after`: Off (javasolt) vagy 10 perc?
2. Játszás/felvétel közben halványuljon-e (javasolt: ne)?
3. Kell-e teljes kikapcsolás (háttérvilágítás 0) is, vagy csak halványítás?
4. Ébresztés pad-ütésre is (javasolt: igen, az ütést nem nyeli el).

## Becslés
Kis-közepes munka: ~150-250 sor új kód (`ScreenDimmer`, beállítások, híd-hívások), plusz teszt. Egy worker, egy ARM-build, egy MPC-s próba.
