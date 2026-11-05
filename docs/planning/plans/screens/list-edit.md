# List Edit — review és TODO

Állapot: terv/review. Alap: **statikus kódolvasás** (build, futtatás, hardver nélkül) a `kick-plans` worktree-ben (2026-11-04). A „KÉSZ” azt jelenti: van bekötött handler az engine-ig, nem azt, hogy hardveren igazolt. Ahol a kód és a régi dokumentumok ellentmondanak, a kód az igazság. „NEM ELLENŐRIZHETŐ KÓDBÓL” = futtatás/hardver kell hozzá.

## Állapot (R-8, 2026-11-04 — a S-C és az R-8 kör után; ez a szakasz a mérvadó, az alábbi táblák a review idejét mutatják)

| Tétel | Állapot |
|---|---|
| LE-01..06, 08..12, 20, 27, 31, 33, 34, 40, 41 | KÉSZ az S-C körben (`8a5a18d6`, `da20b90c`, `39846b60`) |
| **LE-07** sor-virtualizálás | KÉSZ: a `KickTableView` opt-in virtuális sorait a List Edit táblára kötöttük (`enableVirtualRows`); csak a látható ablak (± 6 sor) kap komponenst. Mac smoke: 3057 eseménnyel `rows=3057 built=13` (`KICK_SMOKE_SEQUENCERS_BIG=1`, `KICK_SMOKE_LISTEDIT_ROWS`). A Pad cella már S-C-ben könnyű. MPC-s megnyitási idő: ember kell (HW lista R8-4) |
| **LE-21** nem-jegyzet események | RÉSZBEN, azzal ami a modellben van: a sáv **Level és Pan automáció-sávjának töréspontjai** (`AUTOLANE`, eddig csak felvétel/lejátszás) eseményként a listában: nézet-szűrő (View: All / Notes / Track Automation), Time (érintés = BBT ablak), paraméter, érték (Level 0..127, Pan -100..100), Delete, Insert (TYPE = Track Automation, PARAM, VALUE), Q-Link 3 = érték, csoportos Vel +/-, Nudge, undo. Motor: `MTAudioInterface::getAutomationEvents / setAutomationEvent / deleteAutomationEvent` (`MTTrack::setLanePoint` stb., a ValueTree + UndoManager útján, a lejátszás a meglévő figyelőn át követi). Smoke: insert / move / delete mind egy undo-lépés. **Nincs modell** (ezért nincs UI): aftertouch, pitch bend, CC, program change, channel pressure, mute-események, MPCe, tempó-események (Tempo fül), mixer / pad-mixer / insert-effekt automáció, jegyzet-modifier (Mod Type / Value). Ezek a motor hosszabb munkája, nem a List Edit-é |
| **LE-22** többes kijelölés | KÉSZ: Select kapcsoló (érintés) vagy a fizikai Shift tartása sorérintéskor; a kijelölés a motor kijelölt jegyzetei (Timing Correct „Selected”, Grid); csoportos Delete, Vel -/+ (±5, jegyzet: velocity, töréspont: érték), Nudge < / > (egy TC osztás, a szekvencián belül marad); egy undo-lépés műveletenként; a kijelölés a sorok újrarendezése után is megmarad (azonosító szerint). Host-teszt: `ListEditLogicTests` (toggleRow, nudgeStart, selectionAfterGroupDelete) |
| **LE-32** Go to playhead | KÉSZ: Playhead gomb (a lejátszófej sorára görget, az auto-scroll szünet megszűnik); a lejátszófej sorát piros nyíl jelzi a sor bal szélén (a kézikönyv piros nyila); azonos időpontú sorok ideje szürke (kézikönyv) |
| **LE-11** maradék | KÉSZ kód-oldalon: a beágyazott popup (TYPE / EVENTS / időosztás) nyitva a hardveres ENTER / STOP / Shift+STOP / MAIN **a popupot zárja** (a TC ablak marad nyitva, a következő nyomás zárja azt), `ListEditTimingCorrect::closeFromHardware`. A „Range” opció S-C-ben megszűnt. A Cancel és a popup-zárás mérése: ember kell (HW lista R8-5) |
| Step record (kézikönyv: REC + pad) | KÉSZ: a sorozat REC-je élesítve, megállított transzporttal a pad egy jegyzetet szúr be a lejátszófej pozíciójára, egy TC osztás hosszan, a pad velocity-jével; az Auto Advance (Settings, újra látható) a pozíciót a jegyzet hosszával előre viszi (`seekToTick`). Host-teszt: `stepRecord*` |
| LE-26 Akai-hiánylista | lásd alább („Akai-összevetés, WP-E List Edit”) |
| LE-23 FILTER / Snap | a FILTER most a kézikönyv View menüje (3 nézet, a gomb felirata VIEW); a Snap gomb továbbra is rejtett (nincs funkció) |
| LE-24 / LE-25 / LE-42 / LE-43 | LE-24: híd (R-3); LE-25: owner-döntés; LE-42 / 43: ember kell (HW lista), LE-43 smoke: `KICK_SMOKE_SEQUENCERS_BIG=1 KICK_SMOKE_SCREENS=MenuListEdit` |

### Akai-összevetés, WP-E List Edit (MPC Standalone OS User Guide v3.10, 182–187. o.) — LE-26
| Kézikönyv | Kick |
|---|---|
| Time Counter | a státuszsáv BBT-je |
| TC ikon + Shift+TC = globális TC | KÉSZ (híd: Shift+TC) |
| View menü (All / Notes / Aftertouch / MIDI Automation / Track Automation / Pitch Bend / Control Change / Program Change / Ch Pressure / Mute / MPCe) | All / Notes / Track Automation KÉSZ; a többi eseménytípusnak nincs modellje |
| Tempo fül (tempóesemények) | NINCS (a tempó-sávnak nincs eseménymodellje) |
| Modifier ikon (Note Modifier Settings: Absolute / Relative) | NINCS (a jegyzetnek nincs modifier-modellje) — NEM-KICK, később |
| Gear: Hitting Pad Selects All Events / Auto-Advance on Step Record / Mod Type Selection / Auto-Scroll (Follow / Page / Off) | Hitting pad, Auto Advance (most van fogyasztója), Auto-Scroll KÉSZ; Mod Type Selection NINCS (nincs Mod Type) |
| Oszlopok: #, Time (azonos idő szürke), típus-ikon, Pad/Note, Length, CH, Velocity, Mod Type, Value, Prob, Rat | KÉSZ, kivéve: CH (a jegyzetnek nincs csatornája, a sávé van), Mod Type / Value (nincs modifier), típus-ikon (a Pad/Param oszlop szövege jelzi a típust) |
| Piros nyíl = lejátszófej | KÉSZ |
| Kijelölés: érintés, Shift = több | KÉSZ (érintés + Select kapcsoló + Shift) |
| Beszúrás: REC + pad; Insert ablak (BBT, Type: Note / Pad Automation / Mixer Automation / MIDI Automation, Pad / Parameter) | REC + pad KÉSZ; Insert: Notes és Track Automation (Level / Pan) KÉSZ; a többi típus: nincs modell |
| Szerkesztés: érték érintés + Data Dial / +/-; dupla érintés = numerikus billentyűzet; Shift+dial = TC szerinti hossz; Shift+dial a Pad/Note-on = pad-bank; Nudge gomb; Play (jegyzet lejátszása); Delete | Érték + Data Dial (a megérintett cellára a vezérlő sajátja; sorléptetés a dial alapja), Delete, Nudge (csoportos), **Play gomb** (a kijelölt jegyzet egyszer megszólal), **Shift+dial = a kijelölt események TC-osztásnyi eltolása**, **Shift+Q-Link 1 = pad-bank (±16), Shift+Q-Link 2 = hossz TC-osztásban** KÉSZ. **Nincs**: Mute/Solo On/Off kapcsoló (nincs Mute-esemény); a dupla érintés numerikus billentyűzete a KickNumber saját viselkedése (eszközön ellenőrizendő, HW lista R8-4). A hardveres kiosztás (Shift+Q-Link) a mi választásunk: a kézikönyv a cellát érintés után, Shift+dial-lal szerkeszti, a Kick dialja a sorokat lépteti |


Rövidítések (a repó gyökeréhez képest): `LE/` = `Kick/modules/kick_ui/modes/` (`kick_ListEdit.cpp/.h`); `B:` = `Kick/apps/kick/kick_hardware_bridge.h`; `Home:` = `Kick/modules/kick_ui/dialogs/kick_AppHome.cpp`; `Eng/` = `Kick/modules/engine/audio_engine/engine/`; `Ctl/` = `Kick/modules/kick_ui_controls/source/`; `RPT` = `docs/hardware/key61/hw-test-report.md`; `DVK` = `docs/hardware/key61/doc-vs-kick.md`; `KIR` = `docs/planning/plans/research/known-issues-and-roadmap.md`; `CHK` = `docs/hardware/key61/hw-test-checklist.md`.

## 1. Áttekintés
**Mit csinál.** Az aktuális sáv **jegyzet-eseményeinek táblázata** (`ListEditMode`, `LE/kick_ListEdit.h:58-103`): sor, idő (BAR:BEAT:TICK), pad/note, hossz, velocity, Mod Type, Value, Prob, Rat (`headerTitles`, `.h:92`). Fejléc: Back, sáv-választó, FILTER, TC gomb, Settings (Hitting pads / Auto Advance / Auto Scroll, `Kick/modules/kick_ui/audio_components/gridview/kick_GridData.cpp:68-84`); alul Snap, Delete, Insert. A **Timing Correct ablak** (`ListEditTimingCorrect`, `.h:26-55`) is itt él: Global TC Off/On, időosztás + triplet, TYPE (Start/End/Length/Legato), EVENTS (All/Selected/Range), SWING, SHIFT TIMING, WINDOW, STRENGTH.

**Hogyan érhető el.**
- Menü csempe #5 (`loadScreen` 4-es index, `Kick/modules/kick_ui/dialogs/kick_AppMenuOptions.cpp:90`).
- **Hardver:** nincs saját gombja. A `TC` gomb a List Edit-re visz (`Home:518-519`, `B:1307-1319`); az injektált `btn TC` önmagában csak a képernyőt váltja, az ablakot a TC gomb érintése nyitja (RPT L100, L257 „Kiegészítés”). A híd a List Edit-hez **nem rendel mód-LED-et** (`modeLedForScreen`, `B:253-273`: alapág).
- A képernyő indításkor jön létre (`Home:302`).

**Hardver-kötés a képernyőn** (`LE/kick_ListEdit.cpp:246-335`):
| Vezérlő | Hatás | Hivatkozás |
|---|---|---|
| Pad ütés | „Hitting pads” módban kijelöli az adott pad (+ engine bank) összes sorát; ütésre és felengedésre is lefut | `:248-263` |
| Q-Link 1-4 | a **legfelső látható sor** 4 oszlopa: Pad, Hossz, Velocity, Prob (5-16: nincs a Key 61-en) | `:264-270,286-335` |
| Data Dial, +/- | **nincs kezelve** (a `EN_KNOB_CENTER` ki van hagyva, a gombok közül csak PLAY/PLAY START/STOP/TC) | `:268,271-281` |
| PLAY / PLAY START / STOP | engine hívás (a híd is hívja, idempotens: `Eng/mt_audio_manager_transport.cpp:206-210`) | `:277-279` |
| TC | megnyitja a TC ablakot | `:280` |
| TC ablak nyitva: ENTER / STOP = Apply, Shift+STOP / MAIN = Cancel | a híd zárja az ablakot | `B:889-905`; `Kick/apps/kick/hardware/modal_dialog_keys.h`; ENTER=Apply mért (RPT L252) |
| UNDO / Shift+UNDO | közvetlenül a UndoManager; a lista nem frissül (3.5) | `B:1322-1329` |
Az engedélyezett, de a híd által nem vizsgált út: a Data Dial a **kiválasztott vezérlőhöz** megy (`Kick/modules/kick_ui_controls/source/kick_ControlBase.h:163-215`, `Home:561-579`). Az érintés kiválasztja a legközelebbi Kick-vezérlőt (cella: `KickNumber`/`KickListBox`), a képernyőváltás törli a kiválasztást (`B:789`, `Home:347`). Ezért a **H4** („Data Dial a List Editen nem látszik hatni”, `KIR`) kódból magyarázható: kiválasztás nélkül a dial a képernyőig jut, ott pedig eldobódik (`:268`); a `KickTableView::knobRotated` csak akkor lépteti a sorokat, ha maga a tábla a kiválasztott vezérlő (`Ctl/kick_table.cpp:300-306`). Melyik érintés választja a táblát vagy a cellát, NEM ELLENŐRIZHETŐ KÓDBÓL.

**Érintés (manual, egyujjas).** Sorra koppintás = kijelölés és az engine-jegyzet kijelölése (`:83-91`, a Grid kijelölésével közös); a cellák érintésre állíthatók (lista-felugró, számlépték). Görgetés húzással; lejátszás közben a Follow/Page auto-scroll ráfut a kézi görgetésre (3.8). Érintés-mérés a List Edit-re nincs (a TC-ablak gombjai érintéssel igazoltak, RPT L257).

## 2. Funkciók és állapotuk
| Funkció | Állapot | Bizonyíték |
|---|---|---|
| Jegyzetlista a kiválasztott sávra (sor, idő BBT, pad, hossz, velocity, prob, ratchet) | KÉSZ (csak jegyzetek) | `LE/kick_ListEdit.cpp:79-104,483-507` |
| Cella-szerkesztés (Pad, Length, Velocity, Prob, Rat) | RÉSZLEGES: közvetlen `ValueTree` írás, undo-határ nélkül; aszinkron visszaírás | `:453-467` |
| Idő (Time) szerkesztése | HIÁNYZIK (csak címke) | `:499`; `.h:105-123` |
| Mod Type / Value oszlop | STUB: minden sor „Note”, Value fix 26 | `:428,505`; `.h:121` |
| Sáv-választó | KÉSZ | `:146-151` |
| FILTER lista | STUB: üres lista, nincs kezelő | `.h:86`; `:64-68,205` |
| Snap gomb | STUB: nincs kezelő | `.h:87`; `:70-71,207` |
| Insert (Notes) | BUGOS GYANÚ | `:106-131`; `.h:127-215` |
| Insert típus (Program Automation, Program Mixer Automation, Track Automation) | STUB: a választás figyelmen kívül marad | `.h:163,195` (`typeList.onSelectionChanged = {}`) |
| Delete (kijelölt sor) | BUGOS GYANÚ | `:133-144` |
| Timing Correct ablak: időosztás + triplet, TYPE, SWING, SHIFT, WINDOW, STRENGTH, Global On/Off, Apply | KÉSZ (kód); ENTER=Apply mért; hatás hardveren nem mért | `:153-169,561-684`; `Eng/mt_audio_note.cpp:1667-1720`; RPT L252 |
| TC „EVENTS: Range” | STUB: az engine −1-et ad, értesítés: „range not supported” | `Eng/mt_audio_note.cpp:1671`; `:163-165` |
| TC ablak hardveres zárása (Apply/Cancel) | RÉSZLEGES: Cancel nincs mérve; beágyazott popup (TYPE/EVENTS) nyitva nem zárható | `B:889-905`; `:586-598`; KIR H5 |
| Settings: Hitting pads | KÉSZ | `:248` |
| Settings: Auto Scroll (Off/Follow/Page) | KÉSZ | `:361-398` |
| Settings: Auto Advance On Step Record | STUB: nincs fogyasztója | `kick_GridData.cpp:31-33,40-43`; `rg getAutoAdvance` = nincs hívás |
| Kijelölés-szinkron a Griddel | KÉSZ | `:83-91,337-359` |
| Pad ütés → sorok kijelölése | KÉSZ (görgetés a találatra NEM ELLENŐRIZHETŐ) | `:248-263` |
| Q-Link 1-4 szerkesztés | BUGOS GYANÚ (nullptr; nem a kijelölt sort szerkeszti) | `:286-335` |
| Data Dial / +/- | STUB (a képernyő nem kezeli) | `:268,271-281`; KIR H4 |
| Élő frissítés (felvétel, undo, Grid szerkesztés) | STUB | `:146-151` (egyetlen frissítési pont); nincs listener |
| Skálázás sok eseményre | BUGOS GYANÚ (teljesítmény, nem mért) | `Ctl/kick_table.cpp:340-390`; `Ctl/kick_listbox.cpp:61-77`; `.h:120` |
| PLAY/STOP/TC hardver gombok | KÉSZ | `:277-280` |
| TC LED / Shift+TC globális TC | KÉSZ (kód), hardveren nem mért | `B:328,1307-1319` |

## 3. Mi nem működik / hibák
### 3.1 Q-Link: nullptr-hivatkozás és rossz célsor (LE-01, LE-02)
- `knobCommand` a `tableView->getRowComponents()` **0. elemét** `dynamic_cast`-olja, majd azonnal `comp1->getElementBounds(…)`-et hív (`:288-290`). A `rowComponents` csak a `refresh()`-ben nő (`Ctl/kick_table.cpp:362-366`), és `OwnedArray::operator[]` üres tömbnél `nullptr`-t ad → üres sávon (jegyzet nélkül, a lista soha nem épült) a Q-Link forgatás **null-dereferencia**. Reprodukció-ötlet: új projekt, üres sáv, List Edit, `rawmidi B0 10 01` (Q-Link 1). Ha a tömb korábbi sávból még tartalmaz elemeket, nem omlik össze, de elavult sort szerkeszthet.
- A cél-sor nem a kijelölt: `getRowNumbers( 4, …, 128 )` a **128 képernyő-pixelnél** lévő sort veszi, a `canvas.getScreenPosition()`-hez mérve (`:292`; `Ctl/kick_table.cpp:254-283`). MPC-n (az ablak a (0,0)-nál) a legfelső látható sor-környék; asztali ablakban eltolódhat (NEM ELLENŐRIZHETŐ KÓDBÓL). A 16 Q-Linkes (4x4) Behringer-kiosztásból a Key 61-en csak az 1. sor marad (`B:1530-1541`: `encoder_idx < 4`).
- A `KickListBox::knobRotated` az aszinkron `setSelected`-re épít (`Ctl/kick_listbox.cpp:559-566,577-583`): két gyors forgatás ugyanabból az indexből indul, a második felülírja az elsőt (léptékvesztés).

### 3.2 Data Dial / +/- eldobása (LE-03, P1) — KIR H4
- `ListEditMode::handleCommand` a `knob != EN_KNOB_CENTER` feltétellel a dialt eldobja (`:268`), a gombágon csak PLAY/PLAY START/STOP/TC van (`:275-281`). Az MPC-n ez az elsődleges navigáció-eszköz; a `Track` és más képernyők kezelik (`docs/hardware/key61/morning-decisions.md` AD1b, (a) pont).

### 3.3 Delete: rossz jegyzet törlődhet, kijelölés nélkül is töröl (LE-04, P1)
- `ae.eraseNote( number, start, -1 )` a „-1 hossz” ágon **a pad első, a kezdőpontot lefedő** jegyzetét törli (`Eng/mt_audio_note.cpp:449-457`, lista kezdőpont szerint rendezett), ami egy korábban kezdődő, átnyúló jegyzet is lehet, nem a kijelölt sor (`:133-137`). Megoldás: törlés id/ValueTree alapján (`eraseNote( juce::ValueTree )` létezik, `:633-646`).
- Kijelölés nélkül a `getSelectedRow()` a `canvas.selectedRow.getLast()` értéke (`Ctl/kick_table.h:153-156`), ami üres `juce::Array`-nél **0** (nem -1): a Delete ilyenkor **csendben a 0. sort törli** (`:136`). Ugyanígy a `KickTableView::knobRotated` kijelölés nélkül a 0. sorról indul (`Ctl/kick_table.cpp:300-306`: az első dial-lépés az 1. sorra ugrik, a 0. sort kihagyja).
- A Delete gomb `toggleState`-je csak az `if`-en belül áll vissza (`:134-143`): jegyzet nélküli sávon a gomb bekapcsolva marad.
- Az utolsó sor törlése után `selectRow( 0 )` csak ha `getNumRows()==1` (`:139-140`): a kijelölés a törlés után nem követi a listát.

### 3.4 Insert: egy-tick eltolás, inicializálatlan érték, néma típusok (LE-05, P1)
- A dialógus TICK mezője 1-től indul (`.h:157`), az érték **nyersen hozzáadódik** az idő-számításhoz (`:123`: `… + startTickValue`), a lista viszont 0-tól jelzi a tick-et (`:498`: `bnb.tick`, bar/beat +1). Tehát „1:1:1” beszúrás a 1. tick-re kerül, az ütés-rácsra (tick 0) nem lehet beszúrni. Reprodukció: Insert a bar 1, beat 1, tick 1-re → a lista „01:01:01” (a rács „01:01:00”).
- `ListInsertOptions info` tagjai inicializálatlanok (`.h:130-137,187`); a `ticksPerQuantization( data.bar )` hívás (`:124`) az `info.bar`-t használja, amit csak a `bar.onValueChanged` állít. Ha a felhasználó nem módosítja a BAR mezőt, olvasatlan érték megy az engine-be (egy tempo-beállításnál ártalmatlan, nem definiált viselkedés).
- A `typeList` 4 elemű (Notes / Program Automation / Program Mixer Automation / Track Automation), de a kezelő üres (`.h:163`), az Apply mindig jegyzetet szúr be.
- A frissítés a `visibilityChanged()` közvetlen meghívása (`:125`), nem tiszta frissítő függvény; a `numNotes` a sávválasztó aszinkron értesítésén át frissül (`:146-151`), a `tableView->refresh()` közvetlenül utána még a régi számot láthatja.

### 3.5 Nincs élő frissítés (LE-06, P1)
- A `noteList` ValueTree-re nincs listener; `numNotes` csak sávváltáskor, Insert/Delete után frissül (`:146-151,138`). Hardveres UNDO/REDO (`B:1322-1329`), élő felvétel (mért: a pad-felvétel a List Editben látszik, de újranyitás/sávváltás után, `KIR` 1. tábla), és a Grid/Step Seq szerkesztése nem látszik, amíg a képernyő nyitva van. Reprodukció: List Edit → hardveres REC+PLAY pad-ütések → a lista nem nő.

### 3.6 Skálázás (LE-07, P1; NEM MÉRT)
- A `KickTableView::refresh` **minden sorhoz** létrehoz egy `ListEntry`-t, és mind a canvas gyereke (`Ctl/kick_table.cpp:340-390`; nincs virtualizálás). Egy `ListEntry` 9 vezérlő, köztük a Pad `KickListBox` mind a **128 pad-névvel** (`.h:120`, `kick_common_defs.h:102-110`), és a `KickList` a 128 opciónak `KickToggleButton`-t hoz létre a konstruktorban (`Ctl/kick_listbox.cpp:61-77,102-116`). N esemény → ~N x 128 gomb + N x 9 vezérlő, és a `refreshData` soronként aszinkron `setSelected(… sendNotification)`-t sorba állít (`:500`). Hosszú, felvett sávnál (több száz jegyzet) az ARMv7-en lassú megnyitás/memóriahiány valószínű; mérni kell.

### 3.7 Undo-határok és a „TC = egy undo-lépés” állítás (LE-08, P1; közös SS-10)
- A cellák közvetlen `vt.setProperty( …, &um )`-et hívnak (`:453-467`), tranzakció-határ nélkül → az előző nyitott tranzakcióhoz csatlakoznak (a `beginNewTransaction` csak néhány helyen van: `Eng/mt_audio_manager.cpp:466,647`, `mt_audio_interface.cpp:1803,1815,1885`).
- A TC Apply `beginNewTransaction("Timing Correct")`-et hív (`mt_audio_interface.cpp:1803`), de a belső `MTNote` írások (`updateNote`: `Note.length = …; Note.start = …`, `mt_audio_note.cpp:1329-1340,1708-1712`) `CachedValue`-n át mennek, ami **minden változásnál új tranzakciót kezd** (`Kick/modules/common/source/kick_CachedValue.h:128-137`). Így a TODO T30 / CHK sora szerinti „egy undo-lépés” (`docs/hardware/key61/todo.md:68`, `CHK:33`) a kód alapján **nem teljesül**: N módosított jegyzet → N tranzakció. Hardveres UNDO-val kell ellenőrizni (a host-teszt csak a tiszta logikát fedi, `Kick/tests/platform/TimingCorrectTests.cpp`).

### 3.8 Egyéb (P2)
- **Aszinkron visszaírás (LE-09):** a `refreshData` `note.setSelected( n, sendNotification )`-t hív (`:500`), ami `callAsync`-kel fut (`Ctl/kick_listbox.cpp:559-566`); az `onSelectionChanged` a **pillanatnyi** `vt`-re ír (`:453-455`). Újrahasznosított sornál (`setValues` másik jegyzetre) az előző sor értéke átmenetileg a másik jegyzetre kerülhet, majd a következő callAsync visszaírja; közben undo-bejegyzés és motor-frissítés keletkezhet. A `prob`/`rat` `setValue( …, sendNotification )` (`:503-504`) tartományon kívüli tárolt értéknél (pl. ratchet 0) adat-visszaírást vált ki a megnyitás során.
- **Auto Scroll vs kézi görgetés (LE-10):** a Follow/Page mód 25 Hz-en állítja a nézetet (`:170-172,361-398`), lejátszás közben a sor nem szerkeszthető/görgethető kézzel.
- **Hosszértékek:** a `length` mező 1..9999 (`.h:121`); hosszabb jegyzet megjelenítése csonkul (NEM ELLENŐRIZHETŐ, mekkora a PPQN-hossz, de a 4/4 ütem >3840 tick lehet).
- **TC ablak (LE-11):** a beágyazott TYPE/EVENTS lista-popup nyitva a hardveres zárás hatástalan (`:586-598`, KIR H5); Cancel (MAIN / Shift+STOP) nem mért (RPT L252); a „Range” opció mindig hibát ad. A hardveren a „Global TC” kapcsoló csak Apply-ra érvényesül (`:162`), a Shift+TC közvetlenül kapcsol (`B:1307-1319`).
- **Nem működő vezérlők (LE-12):** FILTER, Snap, Auto Advance, Mod Type/Value, Insert típusok (2. tábla) látszanak, de nem hatnak.
- **Pad-nevek:** a Pad oszlop mindig a `padNames` (A1…H16) címkét használja (`.h:120`), program-típustól függetlenül; Keygroup/MIDI sávon a note-név (pl. C3) hiánya NEM ELLENŐRIZHETŐ kódból.

## 4. Tervben / hiányzik (Akai MPC Standalone-hoz képest, a docs-ból)
- **Az Akai List Edit szakasza nincs átvéve:** a `walkthrough-plan.md` E csomagja (Grid, Step Sequencer, **List Edit**, Track Edit; 165–212, 252–255 o.) **nem készült el**; csak WP-D és WP-G van (`KIR` 3.1). A List Edit funkciólistája (események típusai, szerkeszthető mezők, törlés/beszúrás/szűrő/kijelölés) a dokumentáció szerint NEM ELLENŐRIZHETŐ; ez a review Akai-paritást nem állít. → LE-26.
- **TC elhelyezése:** az Akai felső sávjában TC ikon van, hosszú nyomásra ablak (WP-G G-213), a Track listán Shift+TC (WP-D D-160); a Kickben a TC ablak a List Edit-ben van, nincs TC ikon a felső sávon (`Home:519`) — mindkét sor „ELTÉR / döntés”. Grid-ből sem érhető el (`docs/planning/plans/research/grid-and-piano-view.md:29`).
- **Timing Correct:** a Range nem támogatott, a beállítások nem perzisztensek, rövid track Start-mód csavar, TC LED (`KIR` H26; a `updateTimingCorrectLed` hívás a hídban már megvan, `B:328` — a H8/H26 „nincs TC LED” állítás elavult lehet, hardveren nem mért).
- **Nem-jegyzet események és automáció-sávok:** nincs lane-szerkesztő UI, track PAN nem automatizálható, a Write nem undo-zható (`KIR` H25). A List Edit „Mod Type/Value” oszlopai erre készültek, de üresek.
- **Q-Link oszlopváltás, Shift+Q-Link ablak:** nem vizsgált (DVK 3., 5. sor).
- **Locate ablak** (a `<` `>` lépés mértékét adná, DVK 6., 7. sor), **Erase / Copy / Delete** gombok szerepe a lista-szerkesztésben (DVK 31–32. sor „nem vizsgált”).
- **Step record + Auto Advance:** a Settings-ben megvan a kapcsoló, step-record mód nincs.
- Nincs külön terv a `docs/planning/plans/` alatt a List Edit-re.

## 5. Javaslataim
1. **Dial-first használat (UX + MPC).** A Key 61 fő navigációja a Data Dial és a +/-: a List Edit-ben a dial léptesse a **kijelölt sort**, ENTER/dial-nyomás válassza a szerkesztendő oszlopot, majd a dial állítsa az értéket; a Q-Link 1-4 a kijelölt sor Pad/Hossz/Velocity/Prob oszlopa legyen (nem a képernyő legfelső sora). Indok: ma a hardvernek ebben a képernyőben nincs használható navigációja (3.1, 3.2).
2. **Sor-virtualizálás.** Csak a látható ablak (kb. 10–12 sor) létezik komponensként; a Pad cella ne 128 gombos lista legyen, hanem érték-léptető + felugró választó. Indok: 3.6; az ARMv7 memóriája és a 25 Hz-es auto-scroll.
3. **Közös engine-jelzés a jegyzetlista változásáról** (`ValueTree::Listener` vagy generációszámláló), amire a List Edit, a Step Seq és a Grid feliratkozik. Indok: 3.5; megszünteti a 5 Hz-es lekérdezést is a Step Seq GRID-nézetében (SS-08).
4. **Undo-hatókör segéd** (`ScopedUndoTransaction`), a TC Apply, Insert, Delete, cella-szerkesztés egy-egy lépés. Indok: 3.7; hardveren a UNDO a fő hibajavító gomb.
5. **Hiányzó vezérlők kezelése:** a nem működő FILTER / Snap / Auto Advance / Mod Type / Insert-típusok **elrejtése vagy letiltott megjelenítése**, amíg nincs mögöttük funkció (ugyanez az elv, mint a `04-screen-audit.md` 2.3: ne maradjon félrevezető kezelőszerv).
6. **Pad-ütés: görgetés a találatra és a pad nevének kiírása**; „Go to playhead” gomb és az aktuális sor kiemelése lejátszáskor (az auto-scroll helyett érintés-tiltással).
7. **Multi-select / csoportos művelet** (Delete, Velocity ±, Nudge) a már létező `select mode`-ra építve (`Ctl/kick_table.cpp:563-566`, `:337-359`); a TC „Selected” opciója ma ezt a kijelölést használja, tehát hasznos is lenne.
8. **Idő-szerkesztés BBT mezőkkel**, a tick 0-alapú egységesítésével a lista és az Insert között.
9. **Az Insert/Delete után „Undo” felirat a státuszsávban** (a modális megerősítés hardveren nehézkes, KIR H5).
10. **Tesztelhetőség:** a lista-modellt (sor ↔ jegyzet, rendezés, idő-formázás, beszúrás-idő számítása) JUCE-mentes függvényekbe (mint `mt_timing_correct.h`), hoston tesztelve.

## 6. Hardveres és unit-tesztek
**Ma ezt fedi.** Automatikus teszt a List Edit képernyőre és a jegyzetlista-API-ra **nincs** (`rg` a `Kick/tests` és `Kick/clap_host/tests` alatt: nincs ListEdit/`MTNoteList`/`addOrModifyNote`/`deleteNotesInRange` találat). Host-tesztek közelről: `TimingCorrectTests` (a tiszta `mt_timing_correct.h` logika: snap, strength, window, swing), `ModalDialogKeysTests` (a TC ablak gomb→művelet táblája), `EventSeekTests`, `SeqRecordTests`. Hardveren igazolt (`KIR` 1. tábla, RPT): a pad-felvétel eredménye a List Edit-ben (6 A1 jegyzet, velocity 32–127), a TC gomb ablaknyitása érintéssel, **ENTER = Apply**. Nem igazolt: a TC tényleges kvantálási hatása a lejátszásban/UNDO-ban, a Cancel, a Data Dial a List Edit-en (H4), a Q-Link.
**Hiányzik / javasolt.**
- Host (ValueTree-vel, mint `clap_host/tests`): `MTNoteList` — `find` hossz-egyezés és „-1 hossz” (lefedő) szemantika, `eraseNote` id-alapon, undo-lépésszám egy TC Apply után — LE-40/LE-41.
- Szkript: `kick_cmd` + `snapshot` md5 (RPT módszere): `btn TC`, `touch` a TC gombon, `btn ENTER`, `rawmidi B0 10 01` (Q-Link), `jog`; majd `snapshot`. A `KICK_SMOKE_SCREENS="MenuListEdit"` képernyőkép-futtatás (`docs/hardware/key61/todo.md:78-82`) nagy eseményszámú projekttel (LE-43).
**MPC-n ellenőrzendő** (a CHK bővítéséhez):
1. TC gomb: List Edit-re vált, a 2. nyomás/érintés nyitja az ablakot; ENTER = Apply, STOP = Apply, Shift+STOP és MAIN = Cancel; beágyazott TYPE/EVENTS popup nyitva ugyanezek.
2. Egy TC Apply után UNDO: hány nyomás kell a teljes visszaálláshoz (3.7).
3. Data Dial a List Edit-en: sor-kiválasztás nélkül, a sor/cella megérintése után, +/- (3.2).
4. Q-Link 1-4 üres és nem üres sávon (3.1); forgatás közben a lista viselkedése.
5. Pad-ütés „Hitting pads” módban: kijelölés, görgetés, felengedés.
6. Élő felvétel közben nyitva a lista (3.5); UNDO közben.
7. Megnyitási idő és memória 100 / 500 / 1000 jegyzettel (3.6).
8. Insert BAR 1 / BEAT 1 / TICK 1: a beszúrt jegyzet időpontja (3.4); Delete átfedő jegyzet esetén (3.3).
9. Auto Scroll Follow/Page lejátszás közben: kézi görgetés/szerkesztés lehetősége (3.8).
10. Egyujjas érintés: sorok görgetése húzással, cellaválasztó popup, Delete/Insert gomb méretek.
11. Hosszú (>9999 tick) jegyzet megjelenítése és szerkesztése.

## 7. TODO
Függőségek: `SS-xx` = `step-sequencer.md`. Méret: S < fél nap, M ~1–2 nap, L > 2 nap.

**Hibák**
- [ ] **LE-01** (P0, S) Q-Link: üres/hiányzó sorkomponens őr (`comp1 == nullptr`), nincs-művelet üres sávon — `LE/kick_ListEdit.cpp:288-290`
- [ ] **LE-02** (P1, M) Q-Link a **kijelölt** sort szerkessze (nem a 128 px-nél lévő sort); az 1-4 = Pad/Hossz/Velocity/Prob — `:286-335`; `Ctl/kick_table.cpp:254-283`
- [ ] **LE-03** (P1, S) Data Dial és +/- kezelése: sorléptetés, ENTER = cella-szerkesztés (KIR H4) — `:268,271-281`; függ: LE-30
- [ ] **LE-04** (P1, S) Delete id/ValueTree alapján; kijelölés nélkül ne töröljön (a `getSelectedRow()` üresen 0-t ad); a gomb toggle visszaállítása mindig — `:133-144`; `Eng/mt_audio_note.cpp:633-646`
- [ ] **LE-05** (P1, S) Insert: tick 0/1-alap egységesítése, `info` inicializálása, a 3 automáció-típus tiltása/megvalósítása, tiszta frissítő függvény a `visibilityChanged()` helyett — `:106-131`; `.h:127-215`
- [ ] **LE-06** (P1, M) Élő frissítés a jegyzetlista változására (felvétel, UNDO, más képernyő) — közös engine-jelzéssel, **SS-08**-cal
- [ ] **LE-07** (P1, M) Sor-virtualizálás és könnyű Pad-cella; előbb MPC-s mérés — `Ctl/kick_table.cpp:340-390`; `.h:120`
- [ ] **LE-08** (P1, M) Undo-tranzakciók: cella-szerkesztés, Insert, Delete, TC Apply egy-egy lépés; a TC „egy undo-lépés” állítás ellenőrzése és javítása — `:453-467`; `mt_audio_interface.cpp:1803`; `kick_CachedValue.h:128-137`; közös **SS-10**
- [ ] **LE-09** (P1, S) `refreshData`: `dontSendNotification`-nel töltsön, a visszaírás csak felhasználói változásra fusson (aszinkron, újrahasznosított sor) — `:483-507`
- [ ] **LE-10** (P2, S) Auto Scroll ne írja felül a kézi görgetést (érintés alatt/után szünet) — `:361-398`
- [ ] **LE-11** (P2, S) TC ablak: „Range” opció letiltása/elrejtése, beágyazott popup hardveres zárása, Cancel mérése — `:163-165,586-598`; `Eng/mt_audio_note.cpp:1671`
- [ ] **LE-12** (P2, S) FILTER, Snap, Auto Advance, Mod Type/Value: elrejtés vagy letiltott állapot, amíg nincs funkció — `.h:86-87`; `kick_GridData.cpp:31-43`; `:428,505`

**Hiányzó funkciók**
- [ ] **LE-20** (P1, M) Idő (Time) szerkesztése BBT mezőkkel — `:499`
- [ ] **LE-21** (P1, L) Nem-jegyzet események (CC, pitch bend, PC, automáció) a Mod Type/Value oszlopokkal — függ: automáció-modell (KIR H25), LE-26
- [ ] **LE-22** (P2, M) Többes kijelölés + csoportos Delete / Velocity / Nudge — `Ctl/kick_table.cpp:563-566`; `:337-359`
- [ ] **LE-23** (P2, M) FILTER (esemény-típus, pad, tartomány) és Snap megvalósítása — függ: LE-12
- [ ] **LE-24** (P2, M) Q-Link oszlopváltás a hídban (4x4 → `EN_KNOB_1..16`), közös **SS-22**
- [ ] **LE-25** (P2, S) Döntés a hardveres belépésről (mód-LED/gomb, TC-gomb szerepe, Menü) — `B:253-273`; `Home:518-519`
- [ ] **LE-26** (P1, M) WP-E (Akai útmutató 165–212, 252–255 o.) végigvétele List Edit-re: Akai-hiánylista (csak dokumentum) — `docs/hardware/key61/walkthrough-plan.md:23`
- [ ] **LE-27** (P2, S) Note-név megjelenítése a program típusa szerint (Keygroup/MIDI) — `.h:120` — előbb vizsgálat

**Javaslatok**
- [ ] **LE-30** (P1, M) Dial-first szerkesztési modell (sor → oszlop → érték) — függ: LE-02, LE-03
- [ ] **LE-31** (P2, S) Pad-ütés: görgetés a találatra + pad neve — `:248-263`
- [ ] **LE-32** (P2, S) „Go to playhead”, aktuális sor kiemelése lejátszáskor — `:361-398`
- [ ] **LE-33** (P2, S) Insert/Delete után „Undo” felirat a státuszsávban — függ: LE-08
- [ ] **LE-34** (P2, S) Idő-formázás és beszúrás-idő számítás JUCE-mentes függvényekbe — `:123,498`

**Tesztek**
- [ ] **LE-40** (P1, M) Jegyzetlista-API teszt ValueTree-vel (find, „-1 hossz”, eraseNote id, undo-lépésszám) — közös **SS-41**
- [ ] **LE-41** (P1, S) TC integrációs teszt: All/Selected, duplikátum-törlés, undo-lépésszám egy Apply után — `Eng/mt_audio_note.cpp:1667-1720`; függ: LE-08
- [ ] **LE-42** (P1, S) MPC-s `kick_cmd`+`snapshot` forgatókönyv és CHK-bővítés a 6. pont listájával — `docs/hardware/key61/hw-test-checklist.md`
- [ ] **LE-43** (P2, S) Smoke-képernyőkép MenuListEdit 1000+ eseménnyel, megnyitási idő mérése — `docs/hardware/key61/todo.md:78-82`; függ: LE-07
