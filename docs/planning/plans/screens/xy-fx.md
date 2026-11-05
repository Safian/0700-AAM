# XY FX / Touch FX — review és TODO

Alap: statikus kódolvasás, 2026-11-04, `claude/kick-plans`. Hardveren nem mértem. Rövidítések: `X` = `Kick/modules/kick_ui/audio_components/xyfx/kick_XyFxView.cpp`, `G` = `Kick/modules/kick_ui_components/source/kick_XYFXGrid.cpp`, `B` = `Kick/apps/kick/kick_hardware_bridge.h`.

## 1. Áttekintés

- **Mit csinál ma:** 8x8-as érintőrács (`G:4-5`), amely koordinátát számol és két címkét frissít. Jobbra nyitható Setup-panel: preset-lista (11 db: Beat Repeat LPF/BPF/HPF, LPF/BPF/HPF Manual, LPF/BPF/HPF, Phaser Manual, Phaser — `kick_common_defs.h:40`), Attack, Release, Wet/Dry csúszka, X és Y tengelycímke és -érték. Fent Output lista (Output 1, Output 2, Output 1+2), On/Off, Latch, fejléc Automation-ikonnal.
- **Hang:** semmi. A teljes XY modul egyetlen motor-hívása a Play, PlayStart és Stop (`X:195-197`). A motorban (`Kick/modules/engine`, `AudIO/AudIO`) nincs XY/Touch FX/Beat Repeat kód (grep: 0 találat).
- **Elérés:** TOUCH FX gomb (`B:1111-1114`), Shift+PROG EDIT, vagyis az "EDIT/XYFX" pár (`B:1220-1224`; hardveren igazolt: `known-issues-and-roadmap.md` 1. szakasz), DCL `EN_FX` (`kick_AppHome.cpp:508`), felső sáv (`kick_AppTopbar.cpp:98`). LED: a TOUCH FX LED teljes fényű (`B:278`, `kick_leds.cpp:90`, `:196`).
- **Hardver-kötés:**
  - Q-Link 1..4 → Preset, Attack, Release, Wet/Dry (`X:80-97`).
  - Play, PlayStart, Stop (`X:188-199`).
  - Nincs Data Dial, ± vagy Enter-kötés.
  - A Touch Strip alapmódja (Kick slider) a fókuszált vezérlőt viszi (`kick_ControlBase.h:85`); az XY képernyőn nincs fókuszált vezérlő.
  - Nincs Touch FX strip-mód (`strip_modes.h:7-9`).
  - Egyedi Q-Link módban (0) a Q-Link nem jut a képernyőre (`kick_AppHome.cpp:561-569`).
- **Érintés:** egyujjas (a "manual" út egyetlen forrás, `04-screen-audit.md` §4); az XY-hez ez elég, a többujjas használatot nem támogatja.

## 2. Funkciók és állapotuk

| Funkció | Állapot | Bizonyíték |
|---|---|---|
| XY rács érintés → X, Y érték | KÉSZ (UI) | `G:91-107`, `G:133-160`, `G:183-202` |
| X, Y érték → hang/MIDI | STUB | `X:129-131` csak a címkéket frissíti; nincs motorhívás |
| Preset lista (11) → tengelycímkék | KÉSZ (UI) / STUB (hatás) | `X:32-37`, `kick_common_defs.h:40-52` |
| Attack, Release, Wet/Dry | STUB | `X:25-27`; nincs fogyasztó, nem mentődik |
| Output lista | STUB | `X:111`; nincs fogyasztó; a nevek eltérnek a busz-nevektől ("Out-1,2") |
| On/Off | RÉSZLEGES | `X:119-122` csak a rács engedélyezését váltja |
| Latch | STUB | `X:173-176` → `G:34-40`: csak a cella vizuális kiemelése |
| Setup panel (kapcsoló) | KÉSZ | `X:158-172` |
| Q-Link 1–4 | BUGOS | `X:185-187`, `X:80-97`; zárt panelnél a láthatatlan elemeket állítja (`X:151`) |
| Play/PlayStart/Stop | KÉSZ | `X:188-199` |
| Fejléc Automation | STUB | `X:107` + `kick_modeHeader.cpp:54-55` üres lambda |
| Program-választó a fejlécben (`hdrComp`) | NEM ELLENŐRIZHETŐ KÓDBÓL | `X:74` a hatása nem követhető a modulban |
| Beállítások mentése | HIÁNYZIK | minden érték tagváltozó, nincs ValueTree |
| Touch Strip → XY | HIÁNYZIK | `strip_modes.h:7-9` |
| Data Dial, ± → preset | HIÁNYZIK | `X:179-199` |

## 3. Mi nem működik / hibák

- **XY-B1 (P0): a képernyő hatástalan.** Lásd a 2. szakasz. Hatás: a TOUCH FX gomb, a LED és a képernyő együtt azt sugallja, hogy van effekt. Repro: nyomd a TOUCH FX-et, On, húzz a rácson lejátszás közben. Nem változik a hang.
- **XY-B2 (P1): rejtett Setup-panel állítása.** Zárt panelnél a `setupScreen` mérete 0 (`X:151`), a Q-Link mégis módosítja (`X:185-187`). Nincs visszajelzés a változásról.
- **XY-B3 (P1): nincs "érintés vége" esemény.**
  - A `mouseUp` csak a `currentIndex`-et nullázza (`G:97-101`). Egy effekt-gate-hez (ujj fent/lent) nincs visszahívás, és nincs tartós "érintés" állapot.
  - A latch kiemelés a `paint`-ben a `currentIndex`-et (−1 elengedés után) használja: `-1/8` és `-1%8` → oszlop −1 (`G:40-45`). Valójában a `timerCallback` állítja be a `prevSel` cellát (`G:123-127`).
  - A lecsengés 30 ms-os időzítőn fut, hosszabb ideig tartó érintés alatt is.
- **XY-B4 (P2): induláskori NaN.** `XyFxView` konstruktorában az `xyFx.setXYCoordinates(...)` hívás (`X:117`) még 0 méretű komponensen számol (`G:183-198` nullával osztás). A `yCoordinate` NaN lehet az első érintésig (a `jlimit` NaN-t tovább ad).
- **XY-B5 (P2): trükkös logika.** A tengelyek logikája string-összehasonlításra épül ("Beat Division", `X:72`, `G:166`, `G:186`). A cella-találat (`G:138-142`, nincs `spacing`) és a rajz (`G:80-82`, `spacing=2`) kissé eltér (kb. 2 px a jobb szélen). A Preset 3 és 9 tengelypárja azonos (Cutoff/Resonance, `kick_common_defs.h:42-54`), ami a "Manual" változatok és a "Phaser Manual" közti eltérést csak a nevekben hordozza.
- **XY-B6 (P1): fejléc Automation-ikon halott.** Látszik (`X:107`), semmit nem csinál.
- **XY-B7 (P2): állapot a képernyő elhagyásakor.** `visibilityChanged` üres (`X:154-156`); On és Latch nem áll vissza. Hatástalan állapotban nem látszik, de az effekt bekötésekor probléma (gate nyitva maradhat).

## 4. Tervben / hiányzik (Akai MPC Standalone-hoz képest)

- **Docs-ból ismert tervek:** `04-screen-audit.md` §6 (nem hat hangra/MIDI-re); `screen-inventory.md` 4.6 (STUB); `known-issues-and-roadmap.md` §3.2 (Touch FX strip-mód, Shift+gomb beállítóablakok hiányoznak); `KEY61_DOC_VS_KICK.md` 33–35. sor (Touch FX gomb → XY képernyő). A `05-fx.md` nem említi.
- **Akai Touch FX (`KEY61_TOUCH_STRIP_MODES.md` §4):**
  - A strip a Touch FX plugin paraméterét vezérli; a plugin egy insert effekt a kijelölt kimeneten.
  - TOUCH FX gomb nyomva tartva a preset-listát nyitja; a Shift+TOUCH FX a Touch FX Setup ablakot (Destination: Track / Return / Submix / Output, "Insert Touch FX"); majd Shift+Touch Strip ugyanazt.
  - Beállítások: Touch FX Preset (a Data Dial vagy a −/+ léptet), Latch (a pozíció elengedés után is marad), Touch > Wet/Dry, Wet/Dry, Attack, Release, Assigned Parameter.
  - Plugin-ablak: Touch Strip, Touch Enable, Assigned Parameter, Touch Value, Wet Amount, Touch Value / Wet/Dry Lock, Touch Attack, Touch Release.
  - A TOUCH FX gomb azonnal Touch FX strip-módot választ (Quickstart 34–35.).
- **Ami az Akai-hoz képest teljesen hiányzik a Kickből:** a destination (Track/Return/Submix/Output), a strip-mód, a gomb-gesztusok, az insert-alapú feldolgozás.
- **NEM ELLENŐRIZHETŐ KÓDBÓL / docs-ból:** az Akai XY pad és a strip pontos kapcsolata (a docs csak a strip oldalát írja le); a Touch FX preset-lista pontos tartalma az Akai-n; hogy a Touch FX állapot projekt- vagy globális.

## 5. Javaslataim — teljes megvalósítás

**Elv:** az XY FX egy valódi, destinációnként beszúrt Touch FX effekt (az Akai modell szerint), nem a 4 felhasználói insert egyike. Az X/Y gesztus két paramétert, az érintés pedig a wet-szintet nyitja, Attack és Release simítással.

1. **DSP (új, natív):** `TouchFx` AudIO `Object` (platformfüggetlen C++17, nincs alloc/zár az audio-szálon, NEON-barát, a `RampedParameter` simítással). Módok, a jelenlegi presetek szerint:
   - **Manual:** állapotváltós szűrő (LPF/BPF/HPF), X = Cutoff, Y = Resonance; Phaser Manual: X = Cutoff, Y = Resonance.
   - **Beat Division × Depth** (LPF/BPF/HPF): a vágófrekvenciát tempó-szinkron LFO modulálja, X = ritmus-osztás (2 Bar … 1/64, 8 oszlop), Y = mélység.
   - **Beat Repeat** (LPF/BPF/HPF): érintéskor egy körpuffer rögzít, az X az ismétlés hossza (beat division), Y a szűrő; az ismétlés a transport-tempóhoz kötött.
   - **Phaser** (Beat Division × Resonance): allpass-lánc, tempó-szinkron sebesség.
   - Wet/Dry és Attack/Release: az érintés gate-je a wetet `attack` ms alatt nyitja, `release` ms alatt zárja; a Latch tartja.
2. **Befűzés:** minden cél-buszhoz egy állandó, alapból bypass-olt (wet = 0) `TouchFx` példány, indításkor létrehozva. Így futásidőben nincs szerkezeti módosítás, csak atomi paraméter-írás. Út: a Core `Chain` / `Design::createInsert(chainOrBusId, std::unique_ptr<Object>)` (`Design.cpp:338-343`, `Chain.cpp:36-48`), vagy külön "Touch FX" slot a `DawInserts`-ban a 4 felhasználói slot MELLETT (`DawInserts.h:15`), feldolgozva az utolsó insert után. Program, Looper és Return célhoz ugyanaz a mixin használható; Output/Submix/Return célhoz előbb az INS-B1 hibát kell javítani (`inserts.md`).
3. **Engine API:** `MTAudioInterface::setTouchFx(dest, preset, x, y, touching, attack, release, wetDry, latch)`; az érték átmegy lock-free atomokba; a tempót az engine adja. Destination: Track (a kijelölt track programja), Return, Submix, Output (az `Output 1/2/1+2` helyett a valódi busz-nevek, `busMgr.getOutputBusNames()`).
4. **UI-bekötés:**
   - `XYFXGrid` két új visszahívást kap (`onTouchDown`, `onTouchUp`) a gate-hez.
   - `onValueChanged` → engine.
   - On/Off, Latch, Attack, Release, Wet/Dry és Output mind az engine-be.
   - Az állapot a projekt fájában (`TOUCHFX` csomópont: preset, destination, attack, release, wet, latch; undo-val).
   - Q-Link: a Setup-panel akkor is mutassa az értéket (státuszsor-toast), ha zárt.
   - Data Dial és ± léptet a preset-listán; Enter vált a Latch-en.
5. **Strip és gombok:**
   - Új `kTouchFx` mód a `strip_modes.h`-ban, az Akai sorrendben (a 12-es körben a Sustain után). A TOUCH FX gomb a képernyőt nyitja ÉS a strip-módot Touch FX-re állítja (`B:1111`). A strip pozíciója → Y (vagy az Assigned Parameter), az érintés → gate. A mód-léptetés és a LED-szint a `modeLedLevel`-ben.
   - Gomb-gesztusok: hosszú TOUCH FX = preset-lista; Shift+TOUCH FX és Shift+TOUCH STRIP = Setup. Ehhez hosszú-nyomás felismerés kell a hídban.
6. **Automation:** a gesztus rögzítése lane-ként (`mt_automation.h` új `Param` azonosítók: `TouchX`, `TouchY`; a számok perzisztensek, soha nem szabad átrendezni), és a fejléc Automation-ikon bekötése vagy eltávolítása. Ez külön döntés, nem része az alap megvalósításnak.
7. **MIDI-mód:** a `MidiControlXY` fül már küld két CC-t. Egy közös XY-motor ("Destination: Internal FX / MIDI CC") elkerüli a kétszeres megvalósítást.
8. **Teljesítmény (MPC, ARMv7):** egy példány/cél, bypass-ban nulla feldolgozás, wet=0 esetén kihagyható. A CPU-mérés kötelező a Beat Repeat és a Phaser módokra.
9. **Tesztelhetőség:** tiszta fejléc `xy_fx_map.h` (cella → paraméter, beat-osztás tábla, preset-tábla, Y-skála), host-teszttel; DSP-tesztek impulzus- és szinusz-válasszal.

## 6. Hardveres és unit-tesztek

**Ami fedi most:** semmi nincs az XY modulra. Közvetett: `StripModesTests.cpp` (a strip-módok matematikája, Touch FX nélkül), `RelativeCcTests.cpp` (Q-Link). Az `AppSmoke` nem nyitja meg az XY képernyőt. Hardveren igazolt: a gombpár az XY képernyőt nyitja (`known-issues-and-roadmap.md` 1. szakasz, RPT L105–L120).

**Hiányzik:**
- `xy_fx_map` host-teszt (cellák, beat-osztás, skálák);
- DSP host-tesztek: szűrő frekvencia-válasz, a beat-repeat puffer hossza a BPM szerint, a ramp idők (Attack/Release), wet=0 esetén bit-pontos átengedés;
- gate-állapotgép teszt (touch down/up, Latch);
- strip `kTouchFx` mód a `StripModesTests`-ben;
- perzisztencia round-trip.

**Az MPC-n ellenőrzendők:**
1. TOUCH FX és Shift+EDIT megnyitja a képernyőt; a strip-mód átvált.
2. Hallható hatás mind a 11 presetnél, mind az 4 destinationnél (Output, Return, Submix, Track).
3. Érintés → hang késleltetése (a híd 30 Hz-es festéséhez képest nincs adat, `04-screen-audit.md` §4/9).
4. CPU/xrun mérés a Beat Repeat és a Phaser módnál ARMv7-n.
5. Latch, Attack, Release, Wet/Dry Q-Linkkel és Data Dial-lal.
6. Újraindítás és projekt-betöltés utáni állapot.
7. Strip: pozíció → Y, elengedéskor a gate zárása Latch nélkül.

## 7. TODO

Hibák / alap:
- [x] **XY-01** (P0, L) `TouchFx` natív DSP-objektum (szűrő, tempó-szinkron LFO, beat repeat, phaser, wet/dry ramp) — `AudIO/AudIO` új fájl; függ: XY-06.
- [x] **XY-02** (P0, M) Befűzés: állandó, bypass-olt `TouchFx` példány minden cél-buszon / dedikált Touch FX slot a `DawInserts`-ban — `DawInserts.h:15`, `Design.cpp:338`. Függ: INS-01 (Out/Return/Submix cél), XY-01.
- [x] **XY-03** (P0, M) `MTAudioInterface::setTouchFx` és lock-free atomi paraméterút, tempó-szinkron — függ: XY-01, XY-02.
- [x] **XY-04** (P0, M) `XyFxView` bekötés: rács → engine, touch-down/up gate, On/Off, Latch, Attack/Release/Wet-Dry, Destination — `X:119-131`, `G:91-107`. Függ: XY-03.
- [x] **XY-05** (P1, S) Q-Link: látható érték zárt panelnél, Data Dial és ± preset-léptetés, Enter = Latch — `X:151`, `X:179-199`.
- [x] **XY-06** (P1, S) `xy_fx_map.h` tiszta leképező header + preset/beat-osztás tábla + host-teszt.
- [x] **XY-07** (P1, M) Perzisztencia: `TOUCHFX` csomópont a projekt fájában, undo-val.
- [x] **XY-08** (P1, M) Destination modell (Track/Return/Submix/Output) az `Output 1/2` lista helyett — függ: INS-01.
- [x] **XY-09** (P1, S) Rács-javítások: `mouseUp`/Latch kirajzolás, induláskori NaN, találat és rajz összehangolása, enum a string-összehasonlítás helyett — `G:40-45`, `G:97-101`, `G:183-198`.
- [x] **XY-10** (P2, S) Állapot-visszaállítás képernyő-elhagyáskor (gate zárás, Latch tartása szándék szerint) — `X:154-156`.

Hiányzó funkciók:
- [ ] **XY-11** (P1, M) Strip `kTouchFx` mód, TOUCH FX = azonnali mód, pozíció → Y/Assigned Parameter, érintés → gate — `strip_modes.h`, `B:1111`, `B:1637-1660`. Függ: XY-04.
- [ ] **XY-12** (P1, M) Gomb-gesztusok: hosszú TOUCH FX = preset-lista, Shift+TOUCH FX / Shift+TOUCH STRIP = Setup — `B:1111`. Függ: XY-04.
- [x] **XY-13** (P2, M) Gesztus-automation (új lane-azonosítók) és a fejléc Automation-ikon sorsa — `mt_automation.h`, `kick_modeHeader.cpp:54-55`.
- [x] **XY-14** (P2, M) Közös XY-motor MIDI CC módhoz (`MidiControlXY` összevonás) — függ: MC-10.

Javaslatok:
- [x] **XY-15** (P2, S) Értékek mértékegységgel (Hz, Q, ms), cella-feliratok.
- [x] **XY-16** (P2, S) Custom Q-Link módban jelzés az XY képernyőn — `kick_AppHome.cpp:561-569`.

Tesztek:
- [x] **XY-17** (P1, M) Host-tesztek: `xy_fx_map`, DSP, gate-állapotgép, strip-mód, perzisztencia (6. pont).
- [ ] **XY-18** (P1, S) MPC-ellenőrzőlista (6. pont) a `HW_TEST_CHECKLIST`-be, CPU-mérés ARMv7-n.

## 8. Állapot (S-E kör, 2026-11-04)

Kész (host-teszt: `TouchFxTests` ASan alatt, `XyFxMapTests`, `BusInsertsTests` a láncon át; Mac build + smoke): **XY-01** (`AudIO/AudIO/Dsp/TouchFx.h`: kézi szűrő, tempó-szinkron szűrő-söprés, beat repeat, 4 fokozatú phaser, wet/dry és attack/release, bit-pontos átengedés zárt kapunál), **XY-02** (a `DawInserts` Touch FX slotja a négy insert után; minden program, a looper és minden busz megkapja a hosztolt lánccal), **XY-03** (`MTIInsertsInterface::setTouchFx`, lock-free atomok, a tempót az UI adja a szekvenciából), **XY-04** (rács -> motor, touch-down/up kapu, On/Off, Latch, Attack/Release/Wet), **XY-05** (Q-Link zárt panelnél a státuszsorban mutatja az értéket, Data Dial és +/- preset, ENTER = Latch), **XY-06** (`kick_xy_fx_map.h`), **XY-07** (`TOUCHFX` csomópont a projekt fájában, undo nélkül), **XY-08** (cél: Track / Return / Submix / Output buszok valódi nevekkel), **XY-09** (találat és rajz egy geometria, Latch kirajzolás, induláskori NaN, `mouseUp` esemény, enum a string helyett), **XY-10** (képernyő elhagyásakor a kapu zár, a rács sötét).

Részben: **XY-15** (az Attack/Release ms/s, a koordináták Hz/Q/% szöveggel; cella-feliratok nincsenek), **XY-13** (a fejléc Automation-ikon elrejtve; a gesztus-lane NYITOTT), **XY-17** (a gate-állapotgép és a leképezés tesztelt; strip-mód, perzisztencia-körút nem).

Nyitott: **XY-11** (Touch FX strip-mód: `strip_modes.h`, híd), **XY-12** (hosszú nyomás / Shift+TOUCH FX: híd), **XY-14** (közös XY-motor a MIDI CC módhoz), **XY-16** (Custom Q-Link jelzés), **XY-18** (MPC-lista: checklist 40.; CPU-mérés ARMv7-n: ember). Hang: a hatás Mac-hoszt-teszttel igazolt, MPC-n és fülre nem hallgatva.

## 9. Állapot (R-11 befejező kör, 2026-11-04)

Kész (host-teszt: `XyFxMapTests`, `MidiControlLogicTests`, `AutomationTests`, `TouchFxTests` ASan+UBSan; motor-smoke `KICK_SMOKE_FXLANES=1`): **XY-13** (a gesztus a kijelölt track három FX-lane-jére íródik: `TouchX`, `TouchY`, `TouchGate`; az eddigi `setAutomationEvent` hívások csendben elbuktak, mert a track csak Level és Pan lane-t ismert; Read módban az `applyAutomatedFx` játssza vissza, a kapu a `TouchGate` lane-ből jön, kapu-lane nélkül nyitva marad; csak a "Track" célra, busz-célra nincs track-lane; a fejléc Automation-ikon be van kötve), **XY-14** (a "MIDI CC" cél a MIDI Control XY fül motorját használja: `midictl::XyCcEngine`, ugyanaz a csatorna / CC / tartomány / némítás és ugyanaz a kimeneti port, csak változáskor küld), **XY-15** (cella-feliratok a `xy_fx_map` tiszta függvényéből, a cella közepének értékével; a régi, nem a tényleges X/Y skálát követő táblák törölve), **XY-16** (Custom Q-Link jelzés), **XY-17** (cella-címkék, `XyCcEngine`, TOUCHFX csomópont mentés/betöltés a projekttel, a Touch FX nem foglal memóriát az audio-szálon, `setGesture`; a strip `kTouchFx` mód a `StripModesTests`-ben), **XY-18** (a lista a checklistben).

Nyitott (nem R-11 tétel, a híd miatt): **XY-11**, **XY-12**.
Hang: a hatás host-teszttel igazolt, MPC-n és fülre nem hallgatva. Blokkonkénti idő (hoszt, ASan nélkül, 256 frame): 4-16 us/blokk módonként (a 48 kHz-es blokk-költségvetés 5333 us), allokáció az audio-szálon: 0 (számláló a tesztben).
