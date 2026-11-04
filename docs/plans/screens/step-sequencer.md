# Step Sequencer — review és TODO

Állapot: terv/review. Alap: **statikus kódolvasás** (build, futtatás, hardver nélkül) a `kick-plans` worktree-ben (2026-11-04). A „KÉSZ” itt azt jelenti: van bekötött handler az engine-ig, **nem** azt, hogy hardveren igazolt. Ahol a kód és a régi dokumentumok ellentmondanak, a kód az igazság. „NEM ELLENŐRIZHETŐ KÓDBÓL” = futtatás/hardver kell hozzá.

## Állapot (R-8, 2026-11-04 — a S-C és az R-8 kör után; ez a szakasz a mérvadó, az alábbi táblák a review idejét mutatják)

S-C (`d7076177`, `39846b60`): SS-01..06, 08..16, 18 (xN jelzés, pontos törlés), 21, 23, 33, 40, 43; a GRID SELECT kapcsoló (SS-31 egy része). R-8 (ez a kör):

| Tétel | Állapot |
|---|---|
| **SS-17** nem 4/4 | Host-teszt KÉSZ: `StepModelTests` 8 ütemmutató (2/4, 3/4, 5/4, 6/8, 7/8, 12/8, 7/4, 9/8) x 5 lépésméret (1/4..1/64), vegyes ütemhosszak, triplettek: lépésszám = ütemhossz / lépés (sosem az enum értéke), oldalszám, utolsó lépés, lejátszási pozíció. Motor-smoke: a nem 4/4 ütemmutatók elfogadva és megmaradnak. **Lelet:** ha a lépés nem osztja az ütemet (7/8 negyedekkel: 3 lépés = 288 / 336 tick), az ütem vége (48 tick) lépésként nem érhető el, ott a finomabb osztás kell (`uncoveredTicks`); a lejátszófej ott nem mutat lépést. Ember kell: egy valódi 3/4 és 7/8 projekt a készüléken (HW lista R8-1) |
| **SS-18** maradék | A beállítások ablak (a „Settings” gomb az alsó sávban: a fejléc jobb végét a TOTAL BARS / BPM / nézetváltó eltakarja) **Velocity** mezője: 0 = Auto (CLASSIC 127, GRID 100, mint eddig), 1..127 = minden bekapcsolt lépés alap-velocity-je mindkét nézetben (kézikönyv: „default step velocity”). **Owner-döntés:** az egységes alapérték (127 vagy 100 vagy más) — egy konstans: `step_model::kClassicDefaultVelocity` / `kGridDefaultVelocity`. Sormagasság: ember kell (lásd SS-31) |
| **SS-20** Akai-összevetés | alább (kézikönyv 177–181. o.) |
| **SS-24** Akai-paritás | A kézikönyv Step Sequencer fejezetében **nincs** lépéshossz / gate, másolás-beillesztés, véletlen-generálás: nincs mit megvalósítani. A fejezet tételeiből megvalósítva: Shift+pad = pad kijelölése, Q-Link 0-ra = lépés törlése, alap-Velocity, UNDO a státuszsávban. **Nincs megvalósítva:** Step Automation (a Level / Pan sáv lépésenkénti szerkesztése a lépés-nézetben: a StepComponent / StepPresets jegyzet-alapú és paraméter-enumos, a sáv-alapú lépés-adat külön L méretű egység; a sáv töréspontjai a List Edit-ben már szerkeszthetők), Track Length / SEQ mező (a sáv hossza a Main > Track fülön állítható), Seq/Gbl BPM-kapcsoló (nincs globális-tempó motor-oldal a lépés-nézethez), Clear Page (a Clear Bar az egész ütemre hat; 16T / 32T-nél a lap és az ütem eltér) |
| **SS-30** Q-Link kiosztás | JAVASLAT (UX-döntés, owner): a kézikönyv Screen módja szerint **a Q-Link gomb a lépésnek felel meg** (egy gomb = egy lépés). A Key 61-en 4 gomb van, ezért a **Q-Link gomb oszlopváltása** (SS-22, a híd, R-3) 4 gombos csoportokon át éri el a 16 lépést: a Step Sequencer már oldal-relatívan kezeli a `EN_KNOB_1..16`-ot, tehát ha a híd 16 gombot küld, nincs további UI-munka. Amíg ez nincs: Q-Link 1-4 = az oldal 1-4. lépése. Alternatíva (ha az oszlopváltás nem kell): Q-Link 1 = a kurzor-lépés velocity-je, 2 = probability, 3 = ratchet, 4 = eltolás, a kurzort a Data Dial lépteti — egy lépés mind a három értéke egyszerre elérhető, de eltér a kézikönyvtől. Ajánlás: a kézikönyv szerinti oszlopváltás (hidas munka), mert a Pad / Q-Link megfelelés ugyanaz, mint az MPC-n |
| **SS-31** maradék | A „Select” eszköz Shift nélkül KÉSZ (S-C). Ember kell: az ujj-pontosság mérése a 19 px sormagasságon; ha kevés, 8 sor / oldal (bank-fele váltó) és ≥ 36 px sor (HW lista R8-2). A mérésig a sormagasság nem változik |
| **SS-32** Undo a státuszsávban | KÉSZ: Clear Pad, Clear Bar, a TOTAL BARS rövidítése után a státuszsáv üzenete mellett UNDO gomb (8 mp, vagy a következő üzenetig); a Menü / Fejléc / Projects nincs érintve (`AppStatusbar::offerUndo`) |
| **SS-34** pad-sebesség | KÉSZ: a beállítások ablak „Pad velocity” kapcsolója (a pad saját velocity-je a lépésé); a felhasználói szint (Half Level) ezt eddig is tette |
| SS-07, SS-22 | a híd (R-3) |

### Akai-összevetés, WP-E Step Sequencer (MPC Standalone OS User Guide v3.10, 177–181. o.) — SS-20
| Kézikönyv | Kick |
|---|---|
| Csak MIDI sávokon (audióra nem) | KÉSZ (S-C SS-23: audió / clip sávon letiltva + üzenet) |
| Belépés: Menu, Step Seq gomb (Key 61: Shift+Grid) | KÉSZ (híd) |
| Track mező, Track Length mező + SEQ kapcsoló (független sávhossz) | Track mező KÉSZ; a Track Length / SEQ **nincs** a lépés-nézetben (a Main > Track fülön van) |
| BPM mező; Seq/Gbl kapcsoló (Shift+Tap) | BPM mező KÉSZ; Seq/Gbl **nincs** |
| Időszámláló | a státuszsáv BBT-je |
| Gear: Velocity (alap-velocity) | KÉSZ (R-8) |
| Gear: Step Size (a TC értékét is állítja) | a TIME DIV választó (a kvantálást állítja) KÉSZ |
| Gear: Auto-Scroll Off / Page | a lap-követés a CLASSIC nézetben KÉSZ (S-C), kapcsoló nincs |
| Gear: Automation Unlocked / Locked to Step Length | NINCS (Step Automation nincs) |
| TC ikon (Timing Correct ablak) | a TC gomb a List Edit-en át (KIR) |
| Bar -/+, Pad -/+ gombok, a szám a bal felső sarokban | KÉSZ |
| Shift + Track -/+ = sáv választása | a TRACK lista |
| Shift + Clear = a szekvencia összes lépése; Shift + Clear Page = az oldal lépései | Clear Pad (a pad összes lépése) és Clear Bar (az ütemé) KÉSZ UNDO-val; Clear Page **nincs** (eltérő részletesség) |
| Lépés be / ki: Pad -/+ vagy Shift+pad vagy Pad Select (nyomva tartva / rögzítve) | KÉSZ (R-8: Shift+pad is) |
| Bar -/+ | KÉSZ |
| Lépések: a pad a lépés, a pad a velocity színével világít | a pad-LED-ek a MPC-n nem világítanak (híd, SS-07 / R-3) |
| Nagyobb osztásnál több pad-bank: a < > nyilak a lépésszámok mellett vagy a bank gombok | bank gombok KÉSZ (S-C: felfelé kerekített oldalszám); a < > nyilak: PAGE választó |
| Velocity: érintés a lépés-oszlopon, csúszka (összes lépés), Q-Link a lépéshez; **0 = törli a lépést** | érintés, csúszka, Q-Link KÉSZ; a 0-ra állítás törlése KÉSZ (R-8) |
| Nudge < > (a minta eltolása egy lépéssel, körbefordul; független sávhossznál a saját sávon belül) | KÉSZ (körbefordulás a szekvencián; a sáv saját hosszán belüli nem vizsgált) |
| Flip Steps (az üres lépés 127-tel be, a hangos ki) | KÉSZ (S-C) |
| Presets (6 gomb: rámpa, 1/0 minta, háromszög, tükör, invert) | KÉSZ (S-C) |
| Step Automation (Parameter mező: Add New; rajzolás, Q-Link, Shift+Clear, nudge, csúszka, presets; sötét sáv = az utolsó aktív lépés értéke) | NINCS |


Rövidítések a hivatkozásokban (mind a repó gyökeréhez képest): `SSq/` = `Kick/modules/kick_ui/audio_components/step_sequencer/`; `B:` = `Kick/apps/kick/kick_hardware_bridge.h`; `Home:` = `Kick/modules/kick_ui/dialogs/kick_AppHome.cpp`; `Eng/` = `Kick/modules/engine/audio_engine/engine/`; `Ctl/` = `Kick/modules/kick_ui_controls/source/`; `RPT` = `docs/AKAI_MPC_KEY61_HW_TEST_REPORT.md`; `DVK` = `docs/akai_mpc_hardware/KEY61_DOC_VS_KICK.md`; `KIR` = `docs/plans/research/known-issues-and-roadmap.md`.

## 1. Áttekintés
**Mit csinál.** Egy sáv egy ütemének (BAR) egy padjához tartozó lépéseit mutatja és szerkeszti. Két nézet (`viewModeButton`, `SSq/kick_stepSequencer.cpp:173,808-810,1062-1068`):
- **CLASSIC** (`StepComponent`): lépés-gombok alul (be/ki), fölötte lépésenként egy érték-oszlop a kiválasztott paraméterre (Velocity / Probability / Ratchet); jobbra Presets panel (Ramp/Triangle/Pulse x1/x2/x4 és fordított, Flip/Mirror/Inverse, offset-csúszka). Egyszerre 16 lépés látszik, `PAGE` választóval lapozva (`SSq/kick_stepSequencer.cpp:374-389`).
- **GRID** (`DrumGrid`, upstream PR #19b, `docs/UPSTREAM_TRIAGE.md` UPSTREAM-FEAT-01): a kiválasztott bank mind a 16 padja sorként, a lépések oszlopként, 4 réteg-fül (NOTES/VELOCITY/PROBABILITY/RATCHET), festés/húzás, Shift+érintés többkijelölés, a Data Dial a kijelölteket relatívan állítja (`SSq/kick_DrumGrid.h:24-31`).
Közös felső sáv: PAD (A01..H16), PARAMETER, PAGE, BAR, TIME DIV; alsó sáv: Clear Pad, Clear Bar, Pad Select, BAR -/+, PAD -/+, Nudge -/+; fejlécben TRACK, TOTAL BARS, BPM (`SSq/kick_StepSequencerControl.cpp:5-44,50-106`, `SSq/kick_stepSequencer.cpp:823-839`).

**Hogyan érhető el.**
- Hardver: **Shift+GRID** vagy a `STEP_SEQ` gomb (`B:1229-1241`); a GRID LED halványan világít (`B:272`; `docs/akai_mpc_hardware/KEY61_LED_COLOURS.md:43`: a két állapot színe nem különböztethető meg). Hardveren mért: a gombpár a várt képernyőt nyitja (RPT L111, L234).
- Érintés: a felső sáv Step Seq gombja (`Kick/modules/kick_ui/dialogs/kick_AppTopbar.cpp:94`). A **Menü csempék között nincs** (`kick_AppMenuOptions.cpp:84-101`).
- A képernyő az indításkor jön létre (`Home:289`), a lejátszási jelekre ezért már akkor is feliratkozik, amikor nem látszik (lásd 3.6).

**Hardver-kötés a képernyőn** (`SSq/kick_stepSequencer.cpp:841-1006`, a híd oldala `B:`):
| Vezérlő | Hatás | Hivatkozás |
|---|---|---|
| 16 pad | lépés be/ki (Velocity módban az oldal `page*lépés/oldal + pad`-ja; Probability/Ratchet módban **oldal nélkül**, lásd 3.2) | `:930-953` |
| Full Level / Half (user) Level | user-szintnél a pad sebessége a lépés velocity-je, különben fix 127 | `:932-946`; `B:1125-1135` |
| Pad Select (érintő gomb) | a pad = pad-választó; a Bank gomb ilyenkor pad-bankot vált | `:728-753,903-929`, `:856-876` |
| Bank A-D (Shift: E-H) | lap (page) kiválasztása a bank sorszámával; a `Home` közben az engine bankját is váltja | `:848-887`; `Home:523-530`; `B:1070-1100` |
| Data Dial | nudge (egy lépés, wrap); GRID nézetben a kijelölt jegyzetek relatív állítása, ha nincs kijelölés: nudge | `:955-987` |
| +/- és Cursor L/R | ugyanaz, mint a dial; **felengedésre** fut (a nyomás nem kezelt) | `:989-1005`; `B:1209-1223,1377-1382` |
| Q-Link 1-4 | a lépés-érték állítása a 0..3. lépésen (nem lap-relatív) | `:969-970`; alapmód: `Kick/modules/engine/project/kick_settings.h:213-220` |
| PLAY / PLAY START / STOP | a híd is hívja az engine-t; a képernyő is → idempotens (`Eng/mt_audio_manager_transport.cpp:206-210`) | `:892-894` |
| UNDO / Shift+UNDO | közvetlenül a UndoManager-en (a képernyő nem frissül, lásd 3.6) | `B:1322-1329` |
| Pad LED-ek | **az MPC hídja nem dolgozza fel** a Step Seq saját LED-parancsait (lásd 3.5) | `B:138-160` |
A Data Dial / +/- csak akkor éri el a képernyőt, ha **nincs kiválasztott vezérlő**: bármely Kick-vezérlő (lista, gomb) érintése átveszi (`Kick/modules/kick_ui_controls/source/kick_ControlBase.h:163-215`, `Home:561-579`); ezért a DrumGrid minden érintésnél `ui::clearFocusComponent()`-et hív (`SSq/kick_DrumGrid.cpp:363`).

**Érintés (manual, egyujjas).** Lépés-gomb: koppintás vagy húzás-festés; érték-oszlop: koppintás/húzás állítja az értéket; a Presets panel és a GRID fülek koppintásra. Hardveren a Step Seq csúszkájának érintése mért (RPT L202, L233); lépés-be/ki és GRID érintés külön nincs mérve. A GRID többkijelölése **fizikai Shift** gombot kíván (`SSq/kick_DrumGrid.cpp:398`, `contracts::DclContractsHelper::isShiftPressed()`), egyujjas érintéssel és a Shift gomb lenyomásával kivitelezhető, érintés-only alternatíva nincs.

## 2. Funkciók és állapotuk
| Funkció | Állapot | Bizonyíték |
|---|---|---|
| Lépés be/ki érintéssel (tap, húzás-festés) | KÉSZ (apró hiba: SS-13) | `SSq/kick_stepSequencer.cpp:421-458,574-605` |
| Érték-oszlop (Velocity/Prob/Ratchet), üres lépésen létrehozza a jegyzetet | KÉSZ (velocity 0 lehetséges: SS-04) | `:488-533` (üres ág `:496-511`) |
| PARAMETER választó (3 érték) | KÉSZ | `SSq/kick_StepSequencerControl.h:88` |
| TIME DIV (1…64T) | BUGOS GYANÚ: az egész szekvencia kvantálását írja; belépéskor felülírhatja | `SSq/kick_stepSequencer.cpp:774-779,1029-1031`; `Kick/modules/kick_ui/audio_components/gridview/kick_GridData.cpp:254-296` |
| PAGE választó + bank gombos lapozás | RÉSZLEGES (`TODO` a kódban) | `:879-886`, `SSq/kick_StepSequencerControl.cpp:121-128` |
| BAR választó (+/-), TOTAL BARS | KÉSZ (TOTAL BARS közvetlenül `setSequenceLength`; megerősítés nincs, hatás rövidítéskor NEM ELLENŐRIZHETŐ) | `:694-711,755-758` |
| PAD választó (A01..H16), PAD -/+, Pad Select | RÉSZLEGES (MPC-n LED nélkül; a +/- után elavult index, SS-14) | `:660-692,728-753` |
| Nudge (gombok, dial, +/-): wrap, jegyzet-tulajdonságok megőrzése | KÉSZ (undo-lépés határa N/E) | `:535-572` |
| Clear Pad / Clear Bar | KÉSZ (megerősítés nincs; a Clear Bar `getTicksPerBar(idx+1)`-et szoroz, egyenletes ütemhosszt feltételez) | `:1070-1088` |
| Presets: Ramp/Triangle/Pulse (+x2/x4, fordított), Inverse | BUGOS GYANÚ: velocity 0-t ír (SS-04) | `SSq/kick_StepPresets.cpp:249-345,489-496`; `:104-132` |
| Presets: Flip/Mirror | BUGOS: a lépés többi paraméterét alaphelyzetbe állítja (SS-03) | `:134-161`; `Kick/modules/engine/interface/source/mt_audio_interface.h:381`; `Eng/mt_audio_note.cpp:606-631` |
| Offset-csúszka (-64..64) | KÉSZ (velocity 0-ig lemehet) | `SSq/kick_StepPresets.cpp:19-33`; `:65-102` |
| GRID nézet: 16 sor x lépések, 4 réteg, festés/törlés | KÉSZ (érintés-méret kérdéses, SS-31) | `SSq/kick_DrumGrid.cpp:359-474` |
| GRID: Shift többkijelölés, dial/+- relatív szerkesztés | KÉSZ (kód); hardveren nem mért | `:315-340,398-403,428-437` |
| GRID: lejátszási oszlop kiemelés | KÉSZ | `:487-514,637-642` |
| Hardveres pad = lépés kapcsoló | RÉSZLEGES: Prob/Ratchet módban lap nélkül | `SSq/kick_stepSequencer.cpp:930-953` |
| Q-Link 1-4 → lépés-érték | BUGOS (felülírja a többi paramétert; csak a 0..3. lépés) | `:460-486,969-970` |
| Data Dial → nudge / GRID kijelölés | KÉSZ (kód); Data Dial általában mért (KIR 1. tábla), Step Seq-en nem | `:955-987` |
| Pad LED visszajelzés (lépés-állapot a padokon, Pad Select színek) | STUB MPC-n (nem dolgozzák fel) | `:265-269`; `B:138-160`; `Kick/modules/contracts/dcl/dcl_contracts_helper.cpp:290-321` |
| Lejátszási pozíció CLASSIC nézetben | STUB (`notePlaying` kiszámolva, sehol nem használt) | `:39-45` (a képlet `:42`), `rg notePlaying` = 2 találat |
| Élő frissítés külső szerkesztésre (undo, felvétel, List Edit, Grid) | CLASSIC: STUB; GRID: 5 Hz lekérdezés | `SSq/kick_DrumGrid.cpp:31,504-510`; `StepComponent`-nek nincs engine-listenere |
| BPM mező | KÉSZ (egész BPM) | `SSq/kick_stepSequencer.cpp:760-762,1090-1094` |
| TRACK választó, „Create any Track” figyelmeztetés | KÉSZ | `:812-815,1017-1024` |
| TRACK LENGTH lista | STUB: nincs feltöltve, és a képernyőn kívülre kerül | `:156,633-634,827` (`setBoundsRelative( 440, …)`) |
| TRK VOLUME gomb | STUB: létrejön, de nincs elhelyezve | `SSq/kick_StepSequencerControl.cpp:7,30-44` |
| Képernyőgombok PLAY/PLAY START/STOP | KÉSZ | `:892-894` |
| Audio/Clip sáv védelme | HIÁNYZIK (csak a sávszámot nézi) | `:1017` |

## 3. Mi nem működik / hibák
### 3.1 Q-Link forgatás felülírja a lépés másik paramétereit (SS-01, P0)
- Ok: `StepComponent::knobRotated` a `data.indexValues/indexProbValues/indexRatchetValues[Index]` értékekből indul (`SSq/kick_stepSequencer.cpp:471-473`), de a `handleAsyncUpdate` **minden körben nullázza mindhármat, és csak az aktuális paraméter tömbjét tölti** (`:204-207`, feltöltés `:225-252`). A `addOrModifyNote(…, velocity, probability, ratchet)` (`:482`) tehát Velocity módban `probability=0`, `ratchet=0`-t ír; Probability módban `velocity=0`, `ratchet=0`-t; Ratchet módban `velocity=0`, `probability=0`-t.
- Hatás: az engine `rand <= probability` feltétele miatt a 0% valószínűségű jegyzet **soha nem szól** (`Eng/mt_audio_track.cpp:659-663`); a velocity 0 jegyzet néma/MIDI note-off jellegű. A Key 61 Q-Link 1-4 az egyetlen hardveres érték-bevitel ebben a képernyőben.
- Reprodukció-ötlet: Velocity módban egy bekapcsolt lépésen `rawmidi` Q-Link 1 (`B0 10 01`, RPT L143) → lejátszás: a lépés elnémul, a Probability nézet 1%-ot (a min 1 megjelenítés, `:237`) mutat. Mérés: `snapshot` + List Edit Prob oszlop.
- Mellékhatás: ha a jegyzet hossza ≠ lépéshossz, `find(Note,Start,Length)` nem találja meg (`Eng/mt_audio_note.cpp:440-457`), így **duplikált jegyzet** jön létre ugyanazon a kezdőponton.

### 3.2 Q-Link és Prob/Ratchet pad nem lap-relatív (SS-02, P1)
- `knobRotated( knob - 1, value )` (`:970`) és `toggleStep( padNum, … )` Probability/Ratchet módban (`:949-952`) a lap nélküli indexet kapja; Velocity módban a pad-ág helyesen `page * numStepsPerPage + padNum` (`:936,944`). A 2. oldalon (pl. 1/32) a Q-Link és a Prob/Ratchet pad az 1. oldal lépéseit írja.
- Key 61-en csak 4 Q-Link van (a Behringer 16 gombja helyett): a kötés ezért a bar 0..3. lépésére korlátozódik (`B:1530-1541` csak `encoder_idx < 4`). A DVK szerint a Q-Link gomb oszlopot váltana (DVK 3., 5. sor, „nem vizsgált”); a híd ma `EN_LINK`-et küld (`B:1195-1203`).

### 3.3 Mirror / Flip törli a lépések többi paraméterét (SS-03, P1)
- A `addValuestoEngine` (`:134-161`) Velocity módban `addOrModifyNote( n, start, len, Values[i] )`-t hív, a valószínűség/ratchet alapértéke 100 / 1 (`mt_audio_interface.h:381`); a „módosít” ág felülírja a meglévő jegyzet mindkét mezőjét (`Eng/mt_audio_note.cpp:612-619`). Probability módban a velocity lesz 127 és a ratchet 1 (`:150`), Ratchet módban a velocity 127 és a prob 100 (`:152`).
- Hatás: egy Mirror után minden jegyzet elveszti az egyedi Probability/Ratchet értékét. Reprodukció: Prob 50%-ú lépések → Velocity mód → Mirror → Prob nézet: mind 100%.

### 3.4 Belépéskor felülírhatja a szekvencia kvantálását (SS-05, P1)
- `visibilityChanged` a szekvencia kvantálását indexre képezi le `GridData::getTimeBaseIndex` segítségével, és `sendNotification`-nel állítja be (`:1029-1031`) → a `timeDivValueListBox.onSelectionChanged` visszaírja a `setQuantization`-t (`:774-779`). A leképezés nem teljes: `getTimeBase(_32T)` **78**-at ad (`kick_GridData.cpp:254-269`; az enum értéke 48, `AudIO/AudIO/Daw/DawTime.h:39`), a `getTimeBaseIndex` pedig 2-re, 3-ra és 48-ra az alapértelmezett **16**-ot adja (`:275-295`; a Step Seq listájában (`kick_common_defs.h:66-67`) nincs „2”/„2T”).
- Hatás: ha a Gridben 1/2, 1/2T vagy 1/32T a kvantálás, a Step Seq megnyitása észrevétlenül 1/16-ra állítja (ugyanaz a mező a Gridé is). Reprodukció: Grid TIME DIV = 1/32T → Shift+GRID → vissza a Gridre.

### 3.5 MPC-n nincs pad-LED visszajelzés (SS-07, P1)
- A képernyő a lépés-állapotot `EN_PAD_SET_STEP_ALL_PAD` / `EN_PAD_SET_STEP_SINGLE_PAD` / `EN_PAD_COLOR_MODE` / `EN_LED_SET_SINGLE_LED` csomagokkal küldi (`:196-282,665-745`; csomag-építés `dcl_contracts_helper.cpp:234-238,284-321`). Az MPC híd egyetlen `sendCommandToDcl` fogyasztója csak az aftertouch-engedélyt és az `EN_PAD_SET_COLOUR_STATE`-et dolgozza fel (`B:138-160`); a `EN_PAD_SET_STEP_*`-nak az egész repóban nincs fogyasztója (`rg EN_PAD_SET_STEP` csak a szerződésben).
- Hatás: a Key 61 padjai a Step Seq-ben nem mutatják, mely lépések aktívak, és nincs Pad Select kiemelés; a lépés-programozás „vakon” megy. Hardveres mérés nincs (NEM IGAZOLT), a hiány kódból következik.

### 3.6 CLASSIC nézet nem frissül külső változásra, és fut rejtve is (SS-08/SS-09, P1)
- Frissítés: a `handleAsyncUpdate` csak a saját műveletek után, a lejátszás-pozíció jelre és `refresh()`-re fut (`:39-45,163-190`). Hardveres UNDO/REDO a UndoManagerhez nyúl közvetlenül (`B:1322-1329`) → megállított szekvenciánál a lépések elavultak maradnak; ezután a `toggleStep` az elavult `data.selected`-re épít (`:429`). A GRID nézet 5 Hz-cel újraolvas (`kick_DrumGrid.cpp:31,504-510`), a CLASSIC nem.
- Teljesítmény (NEM MÉRT, kódból): a lejátszás-jelre (25 Hz, `Eng/mt_audio_manager.cpp:259,745`) a `handleAsyncUpdate` **akkor is** lefut, ha a képernyő nem látszik: lépésenként `getNotesInRange`, majd DCL-küldés és `repaint()` (`:192-284`). A `drawSteps` minden festésnél lépésenként SVG-t olvas a lemezről (`:364-370`, `Ctl/kick_Theme.cpp:204-205`: `createFromImageFile`, gyorsítótár nélkül) → lejátszás közben ~16 fájlolvasás/festés az ARMv7-en. Mérendő MPC-n.
- Pozíció-jelzés CLASSIC nézetben nincs (`notePlaying` holt kód, `:42`; a képlet `(bar+1)*beat*tick/…` szorzat, nem pozíció).

### 3.7 Undo-határok (SS-10, P1; közös a List Edit LE-08-cal)
- A jegyzet-tulajdonságok `CachedValue`-n át íródnak, és **minden változás új undo-tranzakciót kezd** (`Kick/modules/common/source/kick_CachedValue.h:128-137`). Ugyanakkor a törlés (`removeChild(..., um)`) és az új jegyzet felvétele tranzakciót nem kezd, így a **megelőző művelet tranzakciójához csatlakozik** (nincs `beginNewTransaction` a Step Seq útvonalán: csak `mt_audio_manager.cpp:466,647`, `mt_audio_interface.cpp:1803,1815,1885`). Következmény (JUCE UndoManager szemantika alapján): egy húzás több tucat undo-lépés; Clear Pad / lépés kikapcsolása az előző szerkesztéssel együtt vonódhat vissza. Hardveres UNDO-nál ellenőrizendő.

### 3.8 Lapozás: kerekítés lefelé (SS-06, P1)
- Bank gomb: `maxPage = numSteps / numStepsPerPage` (egész osztás, `:880`), a PAGE lista viszont `ceil` (`SSq/kick_stepSequencer.cpp:384`). 16T (24 lépés/ütem): 2 oldal van, de a B bank gomb nem lép (maxPage = 1). Az 1/4T–1/8T oldalszám 1. A kód `TODO` megjegyzése is jelzi (`:879`).

### 3.9 Velocity 0 (SS-04, P1)
- `MIDI::MIN` = 0 (`AudIO/AudIO/Includes/Constants.h:32`). A Pulse (második fél = 0, `kick_StepPresets.cpp:321`), az Inverse (`:494`), az offset-csúszka (`:82`) és a Q-Link (`:476`) 0 velocity-t írhat; az üres lépés ága viszont 1-nél vág (`:501`) és a megjelenítés is 1-re vág (`:228`). Következmény: kikapcsolt Pulse-lépések „bekapcsolva” maradnak néma jegyzettel.

### 3.10 Egyéb hibák (P2)
- **Átfedés-szemantika két nézet között (SS-11):** a CLASSIC `getNotesInRange` és a lépés-kikapcsolás `deleteNotesInRange` az **átfedő** jegyzeteket is érinti (`Eng/mt_audio_note.cpp:219-260`): egy korábbról átnyúló hosszú jegyzet miatt „be”-nek látszó lépés kikapcsolása azt a korábbi jegyzetet törli (`SSq/kick_stepSequencer.cpp:453-454`); a GRID az ilyeneket kihagyja (`kick_DrumGrid.cpp:117-118`). A GRID törlése `eraseNote(note, start, -1)`: a `-1` hossz „lefedő jegyzetet” keres, ami egy korábbi átfedő jegyzet lehet (`mt_audio_note.cpp:449-457`, `kick_DrumGrid.cpp:247`).
- **Húzás-festés (SS-13):** a `lastIndex` csak nem húzó `mouseUp`-on nullázódik (`:593-599`); egy húzás után a következő, ugyanazon lépésen induló húzás nem kapcsolja át azt a lépést. Induláskor `lastIndex = 0` (`StepSequencer.h:109`): az első húzás a 0. lépésen nem kapcsol.
- **Elavult index (SS-14):** `padMinus/padPlus/barPlus/barMinus` a `knobRotated` után azonnal `getSelectedIndex()`-et olvas, de a `KickListBox::setSelected` aszinkron (`Ctl/kick_listbox.cpp:559-566`) → a pad-kiemelés egy lépéssel lemaradhat (`:660-726`).
- **Bar-szám egységhiba (SS-15):** `gridData.getBarFromX( tick )` pixel-helyett tick-értékkel hívva (`:137,427,466`; `GridData::getBarFromX` pixelt vár, `kick_GridData.cpp:312-316`). Egy tempo-beállításnál ártalmatlan; több tempo/mérték esetén rossz kvantálás-lekérdezés.
- **Dial darabszám (SS-16):** a dial `abs(delta)` darabszámát a nudge nem veszi figyelembe (`:962-987`): gyors forgatás lépést veszít.
- **Nem 4/4 ütem (SS-17, NEM ELLENŐRIZHETŐ KÓDBÓL, ha van ilyen projekt):** `numSteps` = `getQuantizationPerBar` = a kvantálás **enum-értéke** (`Eng/mt_audio_transport.cpp:36,282`), nem a mérték függvénye; 3/4-ben 16 lépés 16 lépés-hosszal túllóg az ütemen. A Sequence „Time Signature” művelete stub (`docs/plans/04-screen-audit.md` 2.1), így ma ritka.
- **GRID (SS-18):** a lépésenkénti **első** jegyzetet mutatja, a többit elrejti (`kick_DrumGrid.cpp:126-127`); alap velocity 100 (`:29`) a CLASSIC 127-ével szemben (`kick_stepSequencer.cpp:584`); a sorok ≈19 UI-pixel magasak (`stepViewport`/`drumGrid` = 0,64 x képernyő, 16 sor, `:834`; a 7"-os kijelzőn kb. 3 mm, számítás, nem mért).
- **Bank-gomb és engine-bank (tervezési kérdés):** a lapozó bank gomb az engine bankját is váltja (`Home:523-530`), így Pad Select-ben a pad-szám + `getBankOffset()` mást jelenthet, mint a látott lap (`:914,927`).

## 4. Tervben / hiányzik (Akai MPC Standalone-hoz képest, a docs-ból)
- **Az Akai Step Sequencer szakasza nincs átvéve**: a `WALKTHROUGH_PLAN.md` E csomagja (Grid, Step Sequencer, List Edit; útmutató 165–212, 252–255 o.) **nem készült el** (csak WP-D, WP-G; `KIR` 3.1 utolsó pont). A Step Seq funkciólistája ezért a dokumentáció szerint NEM ELLENŐRIZHETŐ; a jelen review nem állít Akai-paritást. A DVK csak a gombot említi (15. sor: Grid / Shift: Step Sequencer, „nincs eltérés”). → SS-20.
- **Q-Link oszlopváltás és Shift+Q-Link ablak**: nem vizsgált (DVK 3., 5. sor, `KIR` 3.4). A Step Seq 4 Q-Linkje csak a 0..3. lépést éri el, amíg az oszlopváltás nincs meg. → SS-22.
- **Pad nyomás/aftertouch** ki van kapcsolva (`B:1777`, `kSendPadAftertouchPackets=false`, `KIR` 3.4); a pad-velocity csak user-szinten érvényesül.
- **Full/Half Level „user level”**, **16 Level**, **Notes mód**, **Erase / Copy** a lépés-bevitellel: DVK 29–32. sor „nem vizsgált”.
- **Dupla Bank-nyomás = E–H**: nem vizsgált (DVK 28. sor); a Shift-es E–H a kódban van (`B:1070-1100`).
- **TC ablak, Locate ablak, Sounds/Keyboard Control menü** — a Step Seq-et közvetve érintik (a `<`/`>` lépés a TC rácsát használja, `B:1331-1349`); Locate-ablak hiányzik (`KIR` 3.1).
- **Timing Correct:** a Range nem támogatott, a beállítások nem perzisztensek (`KIR` H26).
- Nincs külön terv a `docs/plans/` alatt a Step Seq-re.

## 5. Javaslataim
1. **Egy forrás a lépés-adatra (UX + helyesség).** A CLASSIC (tömbök) és a GRID (cellák) más-más módon olvas az engine-ből, ez okozza az 3.1, 3.3, 3.10 hibákat. Egy közös, JUCE-mentes `StepModel` (pad, bar, page → tick-tartomány; a lépés jegyzete = a lépésben **kezdődő** jegyzet; paraméter-szintű `set` az `editNote*` hívásokkal, nem `addOrModifyNote` újraírással) kiküszöböli a hibaosztályt és hoston tesztelhető (SS-40). Indok: a mostani kód a lépés állapotát a UI-tömbökben tartja (`:204-252`), és onnan ír vissza.
2. **Közös „jegyzetlista változott” jelzés az engine-ben** (generációs számláló vagy `ValueTree::Listener`), amire a Step Seq, a Grid és a List Edit feliratkozik; kiváltja a 5 Hz-es lekérdezést és az elavult CLASSIC nézetet (SS-08/LE-06). Indok: UNDO/felvétel/más képernyő szerkesztése ma nem látszik.
3. **Undo-hatókör segéd** (`ScopedUndoTransaction`, gesztusonként egy `beginNewTransaction("Step edit")` a `mouseDown`/dial-lépés/Clear/Preset elején). Indok: 3.7; a hardveres UNDO a Key 61 fő hibajavító gombja.
4. **Pad-LED a Key 61-en**: a lépés-állapot leképezése a híd meglévő pad-RGB útjára (`m_pad_colour_id/state`, `kick_hw::padColourToRgb7`, `B:113-180`) vagy új `set_step_leds`; lejátszó-fej „futófény” a padokon. Indok: az MPC-használat fő visszajelzése a pad; nélküle a Step Seq csak a kijelzőn használható.
5. **Q-Link kiosztás újratervezése 4 enkóderre**: kurzor-lépés (a legutóbb érintett/kiválasztott lépés), Q-Link 1 = velocity, 2 = probability, 3 = ratchet, 4 = nudge; a Q-Link gomb oszlopváltása (4x4 → `EN_KNOB_1..16`) kiegészítheti. Indok: a Behringer 16 gombos kiosztása a Key 61-en csak az első 4 lépést adja (3.2).
6. **GRID érintés-ergonómia**: ≥ 36–40 UI-px soronként (8 sor/oldal, bank-fele váltó vagy függőleges görgetés), „Select” eszköz-gomb a fizikai Shift helyett, érintés-találati tűrés. Indok: 3.10; a kijelző 7"-os, a mérés hiányzik, ezért a tényleges ujj-pontosságot hardveren kell mérni.
7. **Teljesítmény**: a `handleAsyncUpdate` ne fusson rejtve (`isShowing`/saját `visible` jelző), a lépés-gomb SVG-k egyszeri betöltése (`ImageCache`/tagváltozó), és csak a változott oszlop festése lejátszás közben. Előbb MPC-n mérni (CPU%, képkocka-idő lejátszás közben).
8. **Biztonsági háló a destruktív műveletekre**: Clear Pad / Clear Bar / TOTAL BARS rövidítés előtt „Undo” felirat a státuszsávban (az UndoManager már megvan), nem modális megerősítés (a modális ablakok hardveren nehézkesek, `KIR` H5).
9. **Az aktuális pad mintaneve a fejlécben** (a `DrumGrid::readPadNames` már kiolvassa, `kick_DrumGrid.cpp:138-161`), és a lejátszási pozíció a CLASSIC nézetben + lap-követés (az Akai-használatban elvárt, a GRID már tudja).
10. **Alap-velocity egységesítése** (CLASSIC 127, GRID 100): owner-döntés; a Full Level 127 / Half Level 64 gombokkal összhangban.

## 6. Hardveres és unit-tesztek
**Ma ezt fedi.** Automatikus teszt a Step Seq-re **nincs** (`rg` a `Kick/tests` és `Kick/clap_host/tests` alatt: nincs találat StepSequencer/DrumGrid/StepPresets/`addOrModifyNote`/`deleteNotesInRange` ellen). Szomszédos host-tesztek: `TimingCorrectTests` (tiszta TC-logika), `EventSeekTests`, `SeqRecordTests`, `LiveCaptureTests`, `ModalDialogKeysTests`, `RelativeCcTests` (a dial/Q-Link darabszám), `PadLevelTests`; a `clap_host/tests/HardwareInputTests.cpp` a DCL-elosztás szerződését nézi, képernyőt nem. Hardveren igazolt (`KIR` 1. tábla, RPT): GRID/STEP SEQ gombpár, a Step Seq csúszkájának érintése, a pad-felvétel (List Edit), a Data Dial a Track képernyőn; a dial **Step Seq-en** és a Q-Link ebben a képernyőben nincs mérve.
**Hiányzik / javasolt.**
- Host: JUCE-mentes `step_model.h` (lap/lépés/pad leképezés, `ceil` oldalszám, pad→lépés, preset-értékek tartománya, nudge wrap) — SS-40.
- Host: a jegyzetlista API (JUCE `ValueTree`-vel, mint a `clap_host` tesztek): `find` hossz-egyezés, átfedés, `probability=0`, undo-lépésszám — SS-41 (közös LE-40).
- Szkript: `kick_cmd` forgatókönyv + `snapshot` md5/PNG (a RPT módszere): `pad N`, `jog`, `rawmidi B0 10 01`, `btn UNDO`; a `KICK_SMOKE_SCREENS="HomeSequencerView"` képernyőkép-futtatás (`docs/AKAI_MPC_KEY61_TODO.md:78-82`) mindkét nézetre — SS-42/43.
**MPC-n ellenőrzendő** (a `docs/AKAI_MPC_KEY61_HW_TEST_CHECKLIST.md` bővítéséhez):
1. Shift+GRID / STEP SEQ → képernyő; pad-bank LED viselkedés.
2. 16 pad = 16 lépés, ütésre azonnali kijelző-frissítés; Full/Half Level hatása a velocity-re.
3. Pad LED-ek (3.5): ég-e bármi a Step Seq-ben (várt: nem).
4. Q-Link 1-4 forgatás Velocity módban → lejátszás (3.1): elnémul-e a lépés; 2. oldalon melyik lépés változik (3.2).
5. Data Dial és +/-: nudge, majd kijelző-érintés után a dial viselkedése (kiválasztott vezérlő).
6. Bank A-D lapozás 16T/32T/64T felbontáson (3.8).
7. Mirror/Flip/Pulse/Inverse hatása Prob/Ratchet értékekre és a velocity 0-ra (3.3, 3.9).
8. UNDO/REDO a Step Seq-en: frissül-e a kijelző, hány lépést vonnak vissza egy húzás/Clear/Nudge után (3.6, 3.7).
9. Lejátszás közben CPU/képkocka-idő a Step Seq-ben és más képernyőn, a Step Seq megnyitása előtt és után (3.6).
10. TIME DIV = 1/32T a Gridben → Step Seq belépés → a Grid kvantálása (3.4).
11. GRID nézet ujj-pontosság (19 px sor), húzás-festés, Shift+érintés többkijelölés fizikai Shifttel.
12. Hosszú jegyzet (felvett, > lépéshossz) átfedése: be/ki kapcsolás CLASSIC vs GRID (3.10).
13. Egyujjas érintés: húzás a lépés-gombokon, a csúszka és a Presets panel.
14. Kiválasztott pad `A01` és `H16` szélső értékek, kiválasztott sáv nélküli állapot (figyelmeztetés-felirat).

## 7. TODO
Függőségek: `LE-xx` = `list-edit.md`. Méret: S < fél nap, M ~1–2 nap, L > 2 nap.

**Hibák**
- [ ] **SS-01** (P0, S) Q-Link `knobRotated`: csak az aktuális paramétert írja (`editNote*`), a jegyzetet az engine-ből olvassa, ne a nullázott UI-tömbökből; duplikátum-ág megszüntetése — `SSq/kick_stepSequencer.cpp:460-486,204-207`
- [ ] **SS-02** (P1, S) Lap-relatív index a Q-Link-hez és a Prob/Ratchet pad-bevitelhez (`page*numStepsPerPage + idx`) — `:949-952,969-970`
- [ ] **SS-03** (P1, M) Flip/Mirror: a lépés többi paraméterét megőrző írás (`editNote*`/meglévő jegyzet másolása) — `:134-161`; `kick_StepPresets.cpp:446-475`
- [ ] **SS-04** (P1, S) Velocity alsó korlát egységesen (min 1), Pulse „ki” = jegyzet törlése vagy velocity 1; Inverse/offset ugyanígy — `kick_StepPresets.cpp:321,494`; `:65-132,476`
- [ ] **SS-05** (P1, S) Kvantálás-felülírás megszüntetése: `getTimeBase(_32T)` 78→48, a `getTimeBaseIndex` teljes (2, 3), belépéskor ne legyen visszaíró értesítés; „2”/„2T” a listába vagy jelzés — `kick_GridData.cpp:254-296`; `:1029-1031`
- [ ] **SS-06** (P1, S) Oldalszám `ceil` a bank gombnál; bank-gomb viselkedés a `maxPage` felett — `:879-886`
- [ ] **SS-07** (P1, M) Pad-LED a Key 61 hídon (lépés-állapot, Pad Select, lejátszó-fej) — `B:138-160`; `:196-282` — függ: bridge-oldali LED-leképezés
- [ ] **SS-08** (P1, M) CLASSIC nézet élő frissítése külső változásra — függ: **LE-06** (közös engine-jelzés)
- [ ] **SS-09** (P1, M) Teljesítmény: rejtett állapotban ne frissítsen, SVG-cache, részleges festés; előbb MPC-s mérés — `:39-45,192-284,364-370`
- [ ] **SS-10** (P1, M) Undo-tranzakciók gesztusonként — függ: **LE-08** (közös segéd) — `kick_CachedValue.h:128-137`
- [ ] **SS-11** (P2, S) „A lépés jegyzete” egységes definíciója (a lépésben kezdődő); `eraseNote` id alapján; átfedő jegyzet kezelése CLASSIC/GRID között — `:453-454`; `kick_DrumGrid.cpp:117,247`
- [ ] **SS-12** (P2, S) Holt UI: TRACK LENGTH lista, TRK VOLUME gomb, `notePlaying` eltávolítása vagy megvalósítása — `:156,827`; `kick_StepSequencerControl.cpp:7`
- [ ] **SS-13** (P2, S) Húzás-festés `lastIndex` nullázása minden `mouseUp`/`mouseDown`-on — `:593-599`
- [ ] **SS-14** (P2, S) Pad/Bar +/- után a frissített indexet használni (callback-ből) — `:660-726` — függ: SS-07
- [ ] **SS-15** (P2, S) `getBarFromX(tick)` helyett tick→bar (`getBarAtTick`) — `:137,427,466`
- [ ] **SS-16** (P2, S) Dial/Q-Link darabszám alkalmazása (N lépés nudge) — `:962-987`
- [ ] **SS-17** (P2, M) Nem 4/4 ütem: lépésszám a mértékből; előbb vizsgálat, van-e ilyen projekt — `Eng/mt_audio_transport.cpp:36,282`
- [ ] **SS-18** (P2, S) GRID: többes jegyzet jelzése, `eraseNote` hossz-`-1` kikerülése, alap-velocity egységesítés (owner-döntés) — `kick_DrumGrid.cpp:29,126,247`

**Hiányzó funkciók**
- [ ] **SS-20** (P1, M) WP-E (Akai útmutató 165–212, 252–255 o.) végigvétele Step Sequencerre: Akai-hiánylista (csak dokumentum) — `docs/akai_mpc_hardware/WALKTHROUGH_PLAN.md:23`
- [ ] **SS-21** (P2, M) Lejátszási pozíció + lap-követés a CLASSIC nézetben — `:39-45`
- [ ] **SS-22** (P2, M) Q-Link oszlopváltás a hídban (4x4 → `EN_KNOB_1..16`), közös a List Edit-tel (**LE-02**) — `B:1195-1203,1530-1541`; DVK 3., 5. sor
- [ ] **SS-23** (P2, S) Audio/Clip sáv: letiltott állapot + jelzés — `:1017`
- [ ] **SS-24** (P2, L) Akai-paritás tételek az SS-20 eredménye alapján (lépéshossz/gate, másolás/beillesztés, véletlen) — függ: SS-20

**Javaslatok**
- [ ] **SS-30** (P1, M) Q-Link kiosztás 4 enkóderre (kurzor-lépés + 4 paraméter) — függ: SS-01, SS-02
- [ ] **SS-31** (P1, M) GRID érintés-ergonómia (sormagasság, 8 sor/oldal, Select eszköz Shift nélkül) — `kick_DrumGrid.cpp:163-171,398`
- [ ] **SS-32** (P2, S) Clear Pad/Bar és TOTAL BARS rövidítés: „Undo” gomb a státuszsávban — `:650-658,755-758`
- [ ] **SS-33** (P2, S) Pad mintaneve a CLASSIC fejlécben — `kick_DrumGrid.cpp:138-161`
- [ ] **SS-34** (P2, S) Pad-sebesség érzékeny lépés-bevitel kapcsoló (Full/Half Level mellett) — `:930-947`

**Tesztek**
- [ ] **SS-40** (P1, M) JUCE-mentes `step_model.h` + host-teszt (lap/lépés/pad leképezés, oldalszám, preset-értékek, nudge wrap) — függ: SS-02, SS-06
- [ ] **SS-41** (P1, M) Jegyzetlista-API teszt ValueTree-vel (find, átfedés, probability 0, undo-lépésszám) — közös **LE-40**
- [ ] **SS-42** (P1, S) MPC-s `kick_cmd`+`snapshot` forgatókönyv és a HW checklist bővítése a 6. pont listájával — `docs/AKAI_MPC_KEY61_HW_TEST_CHECKLIST.md`
- [ ] **SS-43** (P2, S) Smoke-képernyőkép HomeSequencerView CLASSIC és GRID nézetre — `docs/AKAI_MPC_KEY61_TODO.md:78-82`
