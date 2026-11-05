# MIDI Control — review és TODO

Alap: statikus kódolvasás, 2026-11-04, `claude/kick-plans`. Hardveren nem mértem. Rövidítések: `T` = `Kick/modules/kick_ui/modes/midicontrol/kick_MidiControlTabs.cpp`, `M` = `.../kick_MidiControl.cpp`, `H` = `.../kick_MidiControlTabs.h`, `B` = `Kick/apps/kick/kick_hardware_bridge.h`.

## 1. Áttekintés

- **Elérés:** Key 61-en Shift+KEYBOARD CONTROL (`B:1284-1291`), a LED ilyenkor halvány (`B:282`). A DCL `EN_MIDI_CONTROL` ugyanide visz (`kick_AppHome.cpp:517`).
- **Felépítés:** három fül (Hardware, XY, MIDI Learn) és a MIDI Monitor ablak (`M:6-86`). A MIDI Output Port lista csak a Hardware fülön látszik (`M:28-41`).
- **Hardware fül:** a fizikai vezérlők (18 gomb, 4 Q-Link, 16 pad, 2 XY-tengely) kimenő MIDI-üzenetre kötése (CC, Note, Aftertouch). A bal oldali lista a vezérlő, a jobb oldali panel a beállításai. Fizikai gombnyomásra a lista a megnyomott elemre ugrik (`M:139-176`).
- **XY fül:** `XYFXGrid` (ugyanaz a rács, mint az XY FX-ben), húzásra két CC (`T:1649-1693`).
- **MIDI Learn fül:** a bejövő MIDI-t Kick-paraméterre (track, program, return, submix, main) képezi le. A tábla a projekt `MIDILEARN` ValueTree-ében él (`T:3029-3032`).
- **Hardver-kötés a Key 61-en:**
  - A híd a legtöbb gombot közvetlenül kezeli, DCL-gombüzenet nélkül. Ezek elhagyják a képernyőt: STEP SEQ, TRACK MIX, PAD MIX, TRACK MUTE, PAD MUTE, PROG EDIT, SAMPLE EDIT, SAMPLER, SOUNDS (`B:1171`, `B:1220-1300`), a TOUCH FX (`B:1111`) és a NEXT SEQ (`B:1366`).
  - Csak ezek jutnak el a képernyőre mint DCL-gomb: ERASE, COPY, TAP, ENTER, +, −, TC, a bank gombok, a PLAY/STOP és a Q-Link gomb.
  - A Hardware fül listájának 18 gombja közül ezért legfeljebb 7 mapelhető a Key 61 előlapjáról.
  - A Q-Link elfordulás csak `knobControl.getMode()==1` esetén jut a képernyőre (`kick_AppHome.cpp:561-569`, alapérték 1 a `kick_settings.h:217`). Egyedi Q-Link módban (0) nem.
  - A 61 billentyű, a pitch bend, a mod wheel, a sustain, a strip és a Data Dial nem mapelhető kontroller.
  - A pad-aftertouch nem érkezik meg (`B:1775-1780`, `kSendPadAftertouchPackets=false`).

## 2. Funkciók és állapotuk

| Funkció | Állapot | Bizonyíték |
|---|---|---|
| MIDI Output Port választó | BUGOS GYANÚ | `M:115` a `getMidiDeviceNames(true)` a BEMENETI listát adja (`midi_device_manager.cpp:94-96`); a kimenet névegyezéssel működik (`:339-345`); nem perzisztens (`H:outputPort{"None"}`) |
| Gomb → CC (Momentary) | RÉSZLEGES | `T:1376-1393`; csak a Key 61-en elérhető gombokra |
| Gomb → CC (Toggle) | STUB | `T:1389-1393` azonos a Momentary ággal |
| Gomb "Light Led" (Never/Midi Input/Local) | STUB | `ledUpdate` csak Learn alatt hívódik (`T:2513`), nincs OFF; a híd eldobja a UI LED-csomagokat (`B:137-160`) |
| Q-Link → CC | RÉSZLEGES | `T:1544-1565` `updateValue`: az érték ±1-gyel halmozódik `LowRange..HighRange` között; a Mode (Abs/Rel) nem számít |
| Q-Link Touch Sense / Note | STUB | `T:1411-1436` csak a ritkán futó ágban; `on_encoder_touch` szándékosan üres (`B:1513-1518`) |
| Pad → Note On/Off, bankonként | RÉSZLEGES | `T:1440-1510`; bank-lista, Set All; a Velocity "Off" = fix teljes sebesség |
| Pad Aftertouch Channel | BUGOS | `T:1465`, `T:1503` `channelPressureChange(.., randomValue)`: véletlen szám |
| Pad Aftertouch Poly | STUB | az aftertouch-csomag nem érkezik (`B:1775-1780`) |
| Pad LED (Midi Input / Local) | STUB | lásd Light Led |
| XYFX tengely-beállítások (Hardware fül) | RÉSZLEGES | `T:674-686`; az "XYFXY-Axis" elem hatástalan (lásd 3. MC-B8) |
| XY fül: két CC húzásra | RÉSZLEGES | `T:1649-1693`; mute, tartomány, csatorna működik; mellékhatásokkal (MC-B9) |
| Beállítások mentése | BUGOS | `T:268` törlés indításkor |
| MIDI Learn: sor hozzáadás/törlés/clear | BUGOS GYANÚ | `T:1867-1926`; `numRows` és `midiRightDataStruct` elcsúszik a fától |
| Learn: CC felvétel | RÉSZLEGES | `T:2490-2514` ment CH, CC, Data |
| Learn: Note felvétel | STUB | `T:2470-2488` csak a velocityt menti, a hangszámot nem; sosem képeződik le |
| Learn: leképezés futás közben | BUGOS | `T:2519-2772`; nem CC-azonosító szerinti |
| Flip (invertálás) | STUB | csak tulajdonság és kijelzés (`T:1885`, `T:2377-2383`); `parameterMapping` nem használja |
| Cél-lista: MIDI Track, Audio Track, Program, Return, Submix, Main | RÉSZLEGES | `T:1975-2260`; lásd MC-B13 |
| Program→Drum/Clip/Keygroup/MIDI/CV paraméterek | RÉSZLEGES | `T:2589-2711`; Audio és Plugin típusra üres lista |
| MIDI Monitor, OUT nézet | RÉSZLEGES | `M:279-305` |
| MIDI Monitor, IN nézet | STUB | `midiInData` sosem íródik (`M:302` csak `midiOutData`) |
| Monitor Clear | BUGOS | `M:266-267` az IN-nél `midiOutData`-t töröl |
| MIDI Learn bemeneti port választás | STUB | `T:2423` mindig az első eszköz |

## 3. Mi nem működik / hibák

Súlyosság szerint rendezve.

- **MC-B1 (P0): a Hardware fül mentett kiosztása elvész.**
  - `T:264-268`: a `vTree = mt::getSettings().midiControl.vTree` közös fa, a `removeAllChildren(nullptr)` törli a gyerekeit, utána csak az alapértékek épülnek újra (`T:269-690`).
  - A `mt::getSettings()` fa mentődik (`kick_settings.cpp:44`), de a konstruktor a következő indításkor minden mentett CONTROL/BANK csomópontot eldob.
  - Hatás: nincs tartós MIDI-kiosztás.
  - Repro: állítsd át a Step Seq CC-jét, indítsd újra az appot, nyisd meg a képernyőt. Az érték 2-re áll vissza.

- **MC-B2 (P0): "fantom" MIDI-üzenetek.**
  - `M:137-177` csak akkor választ új elemet a listában, ha a gomb vagy pad ismert (`index != -1`).
  - A `hardWareTab.handleCommand` (`T:1336-1343`) viszont mindig meghívja a `midimessages`-t, mielőtt eldöntené, milyen üzenet jött.
  - A `midimessages` (`T:1357-1393`) az éppen KIJELÖLT vezérlőt küldi ki a pillanatnyi `buttonState`-tel.
  - Következmény: minden olyan hardver-üzenet, amely nem ismert elem, a legutóbb kijelölt vezérlő üzenetét sugározza. Ilyen a Shift, a REC, az OVERDUB, a bank gombok (a bank ág is továbbmegy), a Q-Link gomb, a Data Dial és a PLAY/STOP/PLAYSTART elengedése.
  - Repro: jelöld ki a "Pad 3"-at, nyisd a MIDI Monitort OUT nézetben, nyomd meg a Shiftet. Várt: nincs üzenet. Kódból várható: Note On a pad 3 hangjával.

- **MC-B3 (P0): a fizikai gombok rossz listaelemet választanak.**
  - `M` `controlEnums` sorrend (`kick_MidiControl.h:100-106`): … BROWSE, PLUS, TAP_TEMPO, KNOB_ENTER, MINUS, ERASE …
  - A lista (`T:215-218`): … Browser, Tap, Enter, "-", "+ ", Erase …
  - A fizikai + a "Tap"-ot, a TAP az "Enter"-t, az ENTER a "-"-t, a − a "+ "-t választja ki és küldi.
  - A "−/+" alapérték neve `"-/+"` (`T:342`), a lista elemei `"-"` és `"+ "` (szóközzel). A `saveData` és a `refresh` a `PadParameters` szerint egyezik, ezért ez a két elem nem szerkeszthető.
  - A defaultban szereplő "Undo" (`T:315-320`) nincs a listában.

- **MC-B4 (P1): Q-Link elfogadott események elnyelése.**
  - `T:815-829` `knobCommand` beállítja a `knobRotated` jelzőt, `T:1359-1363` a következő `midimessages` hívást egyszerűen eldobja.
  - Hatás: a Q-Link forgatás utáni első gomb vagy pad esemény (például a Note On) elvész.
  - A Mode (Absolute/Relative) soha nem érvényesül, a Relative mód sem küld relatív kódolást (`T:1544-1565`).
  - A Q-Link 1 lépés/érzékelő-delta ±1 (`relative_cc.h`): 127 detent a teljes tartományhoz, nincs gyorsítás.

- **MC-B5 (P1): rossz alapértékek.**
  - TC és PAD Mute CC-je is 14 (`T:369`, `T:377`), és a 15 kimarad.
  - A 7., 8. és 11. pad alapérték-blokkjában `pad1Control`-ra íródik a PadNo, a NoteNumber és a NoteValue (`T:547-549`, `T:569-571`, `T:603-606`). A Pad 11-nek így nincs NoteNumber/NoteValue értéke.
  - A Pad 12 hangja 11, nem 10.
  - A `loadDefaultSettings` a `Velocity` stringet (`"Off"`/`"On"`) közvetlenül bool-lá alakítja (`T:1330`), ami mindig false.

- **MC-B6 (P1): "XYFXY-Axis" és a jobb panel.**
  - A lista 40 elemű (0..39), a pad-tartomány 22..37, az XY ág `index == 37 || index == 38` (`T:713`). A 37 a pad 16-é (előbb elkapja a `T:700`), a 38 az X-tengely, a 39 sosem kezelt.
  - Hatás: az "XYFXY-Axis" kijelölve a jobb panel nem vált, és a `saveData` nem talál egyező CONTROL-t.

- **MC-B7 (P1): XY fül mellékhatások.**
  - `T:1651-1690`: ha az X vagy Y tengely némítva van, a másik ág hiányzó oldalán egy alapértelmezett konstruált `juce::MidiMessage` (üres SysEx `F0 F7`) marad, és az `onGridCoordinateChanged` mindkettőt elküldi (`M:65-68`).
  - A névadás fordított: az `isMute1Active` a "nincs némítva" állapotot jelenti (`T:1658`).
  - Nincs csökkentés vagy deduplikáció: minden `mouseDrag` két CC-t és két monitor-bejegyzést okoz (kb. 1000 üzenet/s elérhető; a DIN-sávszélesség ~333 CC/s).
  - A `mouseUp` nem küld semmit.

- **MC-B8 (P1): MIDI Output lista.**
  - `M:115`: bemeneti eszközök. A csak-kimenetű eszközök (DIN OUT, szintimodul) nem jelennek meg.
  - A lista minden megjelenéskor `setListOptions(.., selIndex)` hívást kap, ami aszinkron `onSelectionChanged`-et (`kick_listbox.cpp:513-516`) és így `setOutput`-ot vált ki.
  - A port neve nem tárolódik (`outputPort{"None"}`), ezért újraindítás után nincs kimenet, amíg a képernyő meg nem jelenik.

- **MC-B9 (P0): MIDI Learn szálkezelés és élettartam.**
  - `T:2448-2458`: `handleIncomingMidiMessage` a MIDI-bemeneti szálon fut, és közvetlenül `processBufferedMessages` → `saveData` → tábla-frissítés, `ledUpdate`, `parameterMapping` (ValueTree-írás, `vt.setProperty` a `T:2640-2650`-ben, komponens-hívások) fut. A `messageBuffer` és az `H:lock` védelem nélkül (a `lock` sosem használt).
  - A `callAsync([&]...)` lambdák (`T:2555`, `T:2567`, `T:2715`, `T:2729`, `T:2758`) a lokális `ccValue`, `value`, `seqNum`, `trackNum`, `selectedRow`, `program` változókat referenciával fogják. A hívás a függvény visszatérése után fut, tehát dangling referencia: UB, összeomlás.
  - Mapping módban a `messageBuffer` soha nem ürül (`T:2450`, csak a Learn ág törli): minden bejövő MIDI-üzenet (MIDI clock, active sensing is) örökre felgyűlik.
  - `H:174-175`: az `isLearning` és `isMappingEnabled` nem inicializált.

- **MC-B10 (P0): a leképezés nem azonosító szerinti.**
  - A `parameterMapping` (`T:2519-2772`) nem hasonlítja össze a bejövő CC-számot vagy csatornát a tárolt "CC"/"CH" értékkel. A `hasProperty("CC")` mindig igaz, mert az Add `"CC":"None"`-t ír (`T:1885-1893`).
  - A kijelölt tábla-sor célját vezérli minden bejövő CC, bármelyik CC-számmal. Egyszerre egy leképezés működik.
  - Note üzenetre is lefut: a `getControllerValue()` a velocityt adja.

- **MC-B11 (P0): mentett sorok nem látszanak, az Add törli őket.**
  - `T:3029-3032`: a `visibilityChanged` újra lekéri a projekt `MIDILEARN` fáját (undoval, minden megjelenítéskor és elrejtéskor), de a `numRows` és a `midiRightDataStruct` csak a `saveData`-ban frissül (`T:2391-2411`). A fa tartalma ezért nem jelenik meg.
  - A `add.onClick` (`T:1869-1871`) `numRows == 0` esetén `midilearn.removeAllChildren()`-t hív. Betöltés után az első Add törli az összes mentett leképezést.
  - Projektváltás után az előző projekt sorai maradnak a táblában.

- **MC-B12 (P1): törlés és undo desync.**
  - A `del.onClick` (`T:1920-1926`) a fát módosítja, de nem a `midiRightDataStruct`-ot. A tábla rossz sorokat mutat, a `numRows` negatívba mehet.
  - A Clear All és a Delete a globális UndoManagerrel megy (`T:1898-1900`), az Undo visszahozza a sorokat, de a `numRows` nem követi.

- **MC-B13 (P1): célok és értékkezelés.**
  - A sorindex a szűrt listából jön, nem a valódi track-indexből (`T:2528`): az Audio trackek kimaradnak a "MIDI Track" listából, ezért az index elcsúszik.
  - A leképezés csak akkor fut, ha az app aktuális trackje egyezik a kiválasztottal (`T:2551-2553`).
  - Track Pan és Send ajánlott, de nincs kezelve. Submix Volume nincs. Submix "Solo" vs "solo" kisbetűs név (`T:2737`).
  - Main Mute: az `isBusSolo` eredménye nem használt (`T:2762-2768`).
  - Drum Filter/Amp Sustain a `getEnvelopeSustain` getter-t hívja (`T:2610`, `T:2618`).
  - A Drum célokban szóközös nevek (`" Lfo To Filter"`, `" Lfo To Amp "`).
  - A pad-bank figyelmen kívül marad (`selBank` a "Bank" stringből int, `T:2549`).
  - Mute és Solo MINDEN üzenetre vált (nem él, `T:2556-2561`): egy tekerés tucatszor kapcsol.
  - `("Audio Track") || ("Submix")` mindig igaz (`T:2195`).

- **MC-B14 (P1): MIDI Monitor.**
  - Az `in.triggerClick()` (`M:276`) IN nézetet jelöl ki, az viszont mindig üres.
  - A Clear IN nézetben az OUT-ot törli (`M:266-267`).
  - A velocity százalék egész-osztással (`M:292`): 127-nél 100, egyébként 0.
  - A nem-note üzeneteket "Control Change"-ként címkézi (`M:297-299`).
  - A `midiOutData` korlátlanul nő (`M:302`), a statikus `counter` közös.
  - A Walkthrough WP-G G-023 "OK" ítélete ezzel cáfolt.

- **MC-B15 (P2): teljesítmény.** Minden pad-ütés `option1List.setSelected` → `onSelectionChanged` → teljes `refresh()` (~40 `getChild` hívás sorra, ~45 gyerekre) még a MIDI-küldés előtt (`M:176`, `T:979-1123`). Késleltetés nem mért.

## 4. Tervben / hiányzik (Akai MPC Standalone-hoz képest)

- **Doksi-források:** `touch-strip-modes.md` §5, `doc-vs-kick.md` 19. sor, WP-G G-022, G-095, G-210, `screen-inventory.md` 4.19 ("nem olvasva mélyen").
- **Strip a MIDI Control módban:** CC-szám, csatorna, Low/High Range, Touch Sense, Note (útmutató 334. o.); a Mode itt nem elérhető. A Kick listában nincs strip-elem.
- **MIDI Control Mode Output** a Preferences-ben (G-095): nincs. A Kick egyetlen portot használ a képernyőn.
- **MIDI Monitor** az I/O ikonokról és a fejlécből (G-022, G-210): csak ezen a képernyőn érhető el.
- **Q-Link Touch Sense:** a híd szándékosan nem továbbítja az érintést (`B:1513-1518`).
- **Kiegészítő kontrollerek:** Data Dial, Keyboard (61 billentyű), mod wheel, pedál a MIDI Control listájában nincs.
- **NEM ELLENŐRIZHETŐ KÓDBÓL / docs-ból:** az Akai MIDI Control Mode teljes oldal-készlete (nincs manuál a repóban), a pad-nyomás, a bank-eltolás pontos Akai-viselkedése, a Program Change/Bank Select küldés.
- **Doksi-ellentmondás:** WP-G G-023 ("OK") a kód alapján nem igaz (lásd MC-B14).

## 5. Javaslataim

1. **Új modell a Hardware fülhöz.** Jelenleg a vezérlő azonosítóját a lista kijelölése hordozza, és minden esemény ezen megy át. Ehelyett legyen egy tiszta `ControllerMap` adatszerkezet: egy tömb, amelyben minden elem `{ControlId enum, üzenetleírás, mód}`. A DCL-üzenet közvetlenül `ControlId`-ra képződik, a kijelölés csak a szerkesztő-felület állapota. Ez szünteti meg az MC-B2, MC-B3, MC-B4 hibákat, és olcsó, determinisztikus. Header-only, JUCE-mentes logika (mint `relative_cc.h`), így host-tesztelhető.
2. **Perzisztens séma verzióval.** A beállítások kerüljenek a settings-fába típusos tulajdonságokkal, ne string-összehasonlítással. Betöltéskor ne töröljön semmit, csak hiányzó elemeket töltsön alapértékkel.
3. **MIDI Learn motor az engine-ben.** Egy `MidiLearnEngine` (nem UI-komponens):
   - kötés = `(port, csatorna, típus, szám)` → `(célazonosító, skálázás, flip, mód)`;
   - a MIDI-szál egy lock-free FIFO-ba tesz, az alkalmazás az üzenetszálon;
   - soft-takeover az abszolút CC-hez, relatív kódoló támogatás (két-komplemens, mint a Data Dial);
   - Mute és Solo élérzékelés (≥64) toggle helyett;
   - több bemeneti port, port-kiválasztás, ütközés-figyelmeztetés;
   - a projektbetöltés újraépíti a kötéseket, a UI csak a táblát mutatja;
   - Note, Pitch Bend, Program Change tanítása.
4. **Kimenet.** Port-lista a valódi kimeneti eszközökből (és megjegyezve), per-vezérlő port opció, üzenet-deduplikáció és sebesség-korlát az XY fülön (például csak értékváltozáskor, ~100–200 Hz).
5. **Érintés/UX.** A tábla sorai nagyobb találati felülettel; "Learn" a sorban egy gombbal, időtúllépéssel; a legutóbb kapott üzenet megjelenítése a státuszsorban; törlés megerősítéssel vagy undo-toasttal.
6. **MPC-hardver.** A Key 61 előlap-gombjait, amelyek navigálnak, vagy kivenni a listából (a használhatatlan elemeket elrejteni), vagy a híd módosításával Shift+gomb kombinációkat mapelhetővé tenni MIDI Control módban. A strip, a Data Dial és a 61 billentyű felvétele a listába (ezek a Key 61 valódi értéke).
7. **Tesztelhetőség.** A leképezés-logika és a skálázás tiszta headerben; host-teszt az adat-ágakra (lásd 6. pont).

## 6. Hardveres és unit-tesztek

**Ami fedi most:** semmi nincs erre a képernyőre. Közvetett: `RelativeCcTests.cpp` (Q-Link dekódolás), `MidiParserTests.cpp` (híd MIDI-parsere), `StripModesTests.cpp`. Az `AppSmoke` csak az indítást és a képernyőképet fedi. A Walkthrough WP-G G-023 "OK" nem teszt.

**Hiányzik:**
- host-teszt a `ControllerMap` leképezésre (gomb/pad/Q-Link → üzenet, minden Momentary és Toggle eset, Q-Link Abs/Rel);
- host-teszt a lista-sorrend és a `controlEnums` egyezésére (statikus összehasonlítás);
- host-teszt a MIDI Learn kötésre (azonosító-egyezés, flip, mute-él, skála, soft-takeover);
- perzisztencia round-trip (mentés, újraindítás, betöltés: semmi nem vész el);
- stressz: nagy mennyiségű bejövő MIDI és szálbiztonság (TSAN/ASAN host buildben).

**Az MPC-n ellenőrzendők:**
1. Shift+KEYBOARD CONTROL megnyitja a képernyőt, a LED halvány.
2. Output port: a Key 61 DIN OUT / USB portja szerepel-e a listában (nyitott: `hw-behaviour.md:10`).
3. Minden pad ütésre a megfelelő Note On/Off a monitoron és egy külső MIDI-vevőn; bankváltás utáni hangok.
4. Fantom üzenetek: Shift, REC, OVERDUB, bank gomb, Data Dial, PLAY elengedés mérése a MIDI Monitoron.
5. Q-Link lépésszám és sebesség, első ütés a forgatás után, Custom Q-Link mód hatása.
6. XY fül terhelés és CC-sebesség.
7. MIDI Learn: külső kontroller (USB), CC-azonosító szerinti leképezés, hosszú futás, projektbetöltés utáni sorok.
8. Újraindítás után a Hardware fül kiosztása megmarad.

## 7. TODO

Hibák:
- [x] **MC-01** (P0, S) A Hardware fül konstruktorában ne törölj semmit; mentett fa betöltése, hiányzó elemek alapértékkel — `T:264-268`.
- [x] **MC-02** (P0, M) Eseményút szétválasztása: a DCL-üzenet közvetlenül `ControlId`-ra képződik, a fantom-küldés (MC-B2) megszüntetése — `M:137-177`, `T:1336-1393`. Függ: MC-03.
- [x] **MC-03** (P0, S) `controlEnums` és lista egységesítése (egyetlen táblázatból generálva), "−/+" szétválasztása, "Undo" felvétele, XYFXY-Axis javítása — `kick_MidiControl.h:100-106`, `T:215-218`, `T:342`, `T:713`.
- [x] **MC-04** (P0, M) MIDI Learn szálbiztonság és élettartam: FIFO a MIDI-szálról, üzenetszálon feldolgozás, értékkel fogott lambdák, `messageBuffer` ürítése/zárása, inicializált jelzők — `T:2448-2517`, `T:2555-2770`, `H:174-175`.
- [x] **MC-05** (P0, M) Leképezés azonosító szerint (port/csatorna/típus/szám → sor), nem a kijelölt sor szerint — `T:2519-2772`. Függ: MC-04.
- [x] **MC-06** (P0, S) A `numRows` a fából számolódjon, a tábla betöltéskor feltöltődjön, az Add ne töröljön; Delete és Clear egységes — `T:1869-1926`, `T:3029-3032`.
- [x] **MC-07** (P1, S) Toggle mód valódi megvalósítása (belső állapot, nyomásra vált) — `T:1389-1393`.
- [x] **MC-08** (P1, M) Q-Link: Absolute/Relative kódolás, lépésnagyság/gyorsítás, a `knobRotated` elnyelés megszüntetése, Touch Sense eltávolítása vagy bekötése — `T:815-829`, `T:1359-1363`, `T:1544-1565`.
- [x] **MC-09** (P1, S) Aftertouch: a véletlen érték eltávolítása; Poly/Channel csak valódi csomaggal (függ a hídtól, `B:1775-1780`) — `T:1465`, `T:1503`.
- [x] **MC-10** (P1, S) XY fül: nincs üres SysEx, mute-logika átnevezése/egyszerűsítése, csökkentés és deduplikáció, `mouseUp`-nál utolsó érték — `T:1649-1693`, `M:65-68`.
- [x] **MC-11** (P1, S) Output lista a valódi kimeneti eszközökből, a választott port mentése, nincs automatikus első eszköz — `M:111-120`.
- [x] **MC-12** (P1, M) Alapértékek javítása (CC 14/15, pad 7/8/11, Velocity-bool) — `T:369`, `T:377`, `T:547-606`, `T:1330`.
- [ ] **MC-13** (P1, M) Célok javítása: valódi track-index, Pan/Send, Submix Volume/Solo, Main Mute, Drum Sustain setter, bank-figyelembevétel, Mute/Solo élérzékelés — `T:2528-2770`, `T:2195`.
- [x] **MC-14** (P1, S) Monitor: IN lista feltöltése a Learn-bemenetről, Clear javítás, % számítás, méretkorlát, helyes típuscímkék — `M:209-305`.

Hiányzó funkciók:
- [x] **MC-15** (P1, M) Flip megvalósítása a leképezésben és tesztben — `T:2377-2383`.
- [x] **MC-16** (P1, M) Note, Pitch Bend, Program Change tanítása és leképezése (a hangszám is tárolva) — `T:2470-2488`.
- [x] **MC-17** (P1, M) Több bemeneti port, port-választó a Learn-hez, az `activeMidiInput` életciklusa — `T:2413-2445`.
- [x] **MC-18** (P1, M) Projektbetöltésre újraépülő kötések a motorban (háttérben működik a képernyő megnyitása nélkül) — függ: MC-05.
- [ ] **MC-19** (P1, M) Strip-elem a Hardware listába (CC, csatorna, Low/High Range, Touch Sense) — függ: híd T12.
- [ ] **MC-20** (P2, M) LED Midi Input/Local megvalósítása a híd LED-táblájával (a UI csomagot a híd ma eldobja) — `T:2813-2845`, `B:137-160`.
- [x] **MC-21** (P2, L) Preferences: MIDI Control Mode Output port (G-095).

Javaslatok:
- [x] **MC-22** (P1, L) `ControllerMap` + `MidiLearnEngine` refaktor és verziózott séma (5. pont) — függ: MC-01..MC-06.
- [x] **MC-23** (P2, M) Front-panel gombok, amelyek navigálnak, rejtve vagy alternatív mapping (Shift-kombináció) a Hardware listában.
- [x] **MC-24** (P2, S) UX: nagyobb sorok, megerősítés/undo-toast, utolsó üzenet a státuszsorban.
- [ ] **MC-25** (P2, S) Pad-ütés késleltetés mérése és a teljes `refresh()` kiváltása diffelt frissítéssel — `M:176`, `T:979-1123`.

Tesztek:
- [ ] **MC-26** (P1, M) Host-tesztek: leképezés, lista-egyezés, Learn-kötés, perzisztencia (6. pont) — `Kick/tests/platform`.
- [x] **MC-27** (P1, S) MPC-s ellenőrzőlista bővítése (6. pont) a `HW_TEST_CHECKLIST`-be.
- [ ] **MC-28** (P2, M) TSAN/ASAN futás a Learn útvonalon külső MIDI-forrással.

## 8. Állapot (S-E kör, 2026-11-04)

Kész (host-teszt `MidiControlLogicTests`, Mac build + smoke): **MC-01** (a mentett kiosztás megmarad; sémaverzió: a régi hibás alapértékű fa nem olvad be), **MC-02/03** (a vezérlő a hardver-üzenetből jön, nem a lista kijelöléséből: nincs fantom-üzenet; egy táblázat a lista, az enum-sorrend és az alapértékek számára; a "-/+" szétvált, XY-sorok elérhetők; az Undo bejegyzés törölve, nincs gombja), **MC-04/05/06** (a Learn a MIDI-szálról a üzenetszálra másol, értékkel fogott lambdák, a kötés típus+csatorna+szám szerint, minden sor a saját céljára; a tábla a projekt fájából épül, az Add nem töröl; undo/törlés a fa figyelőjén át frissít), **MC-07** (Toggle), **MC-08** (Absolute/Relative, gyorsítás, nincs elnyelt esemény; a Touch Sense / Note sor elrejtve, a híd nem adja), **MC-09** (nincs véletlen aftertouch; a sor elrejtve), **MC-10** (tengelyenként, csak változásra, nincs üres SysEx), **MC-11** ("None" az első elem, a port mentve), **MC-12** (CC-k, pad 7/8/11, Velocity), **MC-14** (IN nézet a Learn bemenetéről, Clear, százalék, 200 bejegyzés, helyes típuscímkék), **MC-15** (Flip), **MC-16** (Note, Pitch Bend, Program Change tanítás és leképezés, a hangszám tárolva), **MC-17** (minden bemeneti port nyitva).

Részben: **MC-13** (a cél név szerint, Pan -1..1, Gain 0..1, Mute/Solo él, Drum Sustain setter, bank; track Pan és Submix Volume nincs motor-hívás: a listából kivéve), **MC-26** (a leképezés, kötés-egyezés, tábla és monitor-logika tesztelt; perzisztencia-körút és szál-stressz nem).

Nyitott: **MC-18** (a kötések a képernyő megnyitása nélkül is működjenek: a bemenetek ma az első "Enable Mapping"-kor nyílnak), **MC-19** (strip-elem: híd T12), **MC-20** (LED Midi Input / Local: a Light Led sor elrejtve, a híd eldobja a UI LED-csomagokat), **MC-21** (Preferences kimeneti port: `kick_ui_settings`), **MC-22** (teljes `ControllerMap`/`MidiLearnEngine` refaktor), **MC-23** (a navigáló gombok elrejtése a listából), **MC-24**, **MC-25**, **MC-27** (checklist 42.), **MC-28** (TSAN: ember / külön futás).

## 9. Állapot (R-11 befejező kör, 2026-11-04)

Kész (host-teszt `MidiControlLogicTests` ASan+UBSan, Mac build, képernyő-smoke): **MC-18** (a tanult sorok induláskor élnek: a képernyő az indításkor épül, a bemeneti portokat és a figyelő időzítőt az üzenetszál nyitja; 3 másodpercenként a port-lista összevetése: később csatlakoztatott eszköz megnyílik, eltűnt bezárul; `diffPorts` tesztelt; a kötések a beállítás-fából jönnek, nem a projektből, ezért projektbetöltéstől függetlenek), **MC-21** (a kimeneti port saját beállítás: `midiControlModeOutputPort`, a régi `HwOutputPort` csak átvétel; a Preferences-felület másik worker), **MC-22** (a tanuló-illesztő valódi motor: `midictl::MidiLearnEngine`, a tábla sorai kötésekké alakulnak, egy üzenet *minden* egyező sort megtalál, soronkénti flip-pel; az agy `ControllerMap` / `MidiLearnEngine` másodpéldánya, amelyet semmi nem használt, törölve: a vezérlő-adatmodell a meglévő, tesztelt `buttons()` / `controlListNames()` táblák), **MC-23** (nincs mit elrejteni: a hardver-gombok csak akkor mennek MIDI-be, amikor a MIDI Control képernyő nyitva van, ott pedig a `KickHome::handleCommand` a navigációt letiltja, `kick_AppHome.cpp:551`; háttérben nincs gomb -> MIDI út; az alternatív Shift-kombinációkhoz a híd kell), **MC-24** (egy sor törlése és a sor-törlés visszavonható, a státuszsor UNDO-gombjával; az utolsó bejövő üzenet a státuszsorban; a sorok 52 px magasak, mint a többi táblában), **MC-25** részben (a lista-frissítés csak a megváltozott sorra fut; a pad-ütés késleltetés mérése: ember), **MC-13** részben marad (a cél a névből oldódik fel; a track Pan és a Submix Volume motor-hívás híján a listából kivéve: ehhez motor-oldali setter kell, nem R-11 fájlkör), **MC-26** részben (a leképezés, az illesztő-motor, a port-diff, az XY-motor tesztelt; a perzisztencia-körút és a szál-stressz nem), **MC-27** (a lista a checklistben).

Nyitott / blokkolt:
- **MC-19** (strip-elem a listában): híd T12.
- **MC-20** (LED Midi Input / Local): a híd a UI LED-csomagokat eldobja (`kick_hardware_bridge.h:137-160`, tilos); a sor rejtve marad. Híd-lépés: a UI-ról jövő LED-csomagot a `set_led` táblára képezni, ha az érintett vezérlő LED-módja `Midi Input`.
- **MC-28** (TSAN a Learn úton): az ASan+UBSan futás a tiszta logikára kész; a *valódi* útvonal (MIDI-szál -> `callAsync` -> üzenetszál) külső forrást kér: ember (lásd a checklistet).
