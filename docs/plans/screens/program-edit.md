# Program Edit — review és TODO

Alap: **statikus kódolvasás** (build, futtatás, hardver nélkül), ág `claude/kick-plans` HEAD `40fb2db6`, 2026-11-04. A „KÉSZ” azt jelenti: van bekötött handler a motorig, **nem** azt, hogy hallgatással vagy MPC-n igazolt. Ahol a kód és a régi doksik (`research/screen-inventory.md` §4.9) ellentmondanak, a kód az igazság; az eltérést jelzem. Ami futtatás nélkül nem dönthető el: „NEM ELLENŐRIZHETŐ KÓDBÓL”.
Útvonal-rövidítések: `M/` = `Kick/modules/`, `A/` = `Kick/apps/`, `PE/` = `Kick/modules/program_edit_ui/sources/`, `EN/` = `Kick/modules/engine/`.

## 1. Áttekintés (mit csinál, hogyan lehet elérni, hardver-gombok kötése)

**Mit csinál.** Program-típusonként más szerkesztő. A `ProgramEdit` képernyő (`PE/kick_ProgramEdit.cpp:81-147`) hat belső szerkesztőt tart (`:86-91`: CV, MIDI, Clip, Drum, Keygroup, Audio), a fejléci program-választóval (`:110-117`) váltja őket (`loadProgram`, `:149-226`). **Plugin** típusra nincs szerkesztő a switchben (`:165-173`), azt a CLAP-modál (`M/kick_ui/program_view/source/kick_ClapPanel.h`) kezeli, külön úton (lásd lent).

| Típus | Lapok / tartalom | Fájl |
|---|---|---|
| Drum | Main, Samples, Layer Mix, Envelopes, LFO (5 tab) | `PE/drum/kick_DrumProgramEdit.cpp:28-32` |
| Keygroup | Main, Samples 1, Samples 2, Layer Mix, Envelopes, LFO (a „Samples” gomb a két mintalapot váltogatja, `PE/keygroup/kick_KeyGroupProgramEdit.cpp:75-84`) | `…KeyGroupProgramEdit.cpp:8-13` |
| Clip | Program fül (semi/fine/launch quantize + pad-rács), Pad fül (hullámforma, start/end, semi/fine, mute group, warp/reverse/fade) | `PE/clip/*` |
| Audio | Grid-szerű eszköztár (Select/Pencil/Erase/Cut/Magnify/…), hullámforma-sávok, automation-sáv, minta-paraméterek | `PE/audio/*` |
| MIDI | 8 × 16 CC-csúszka (CC0..127) | `PE/midi/kick_MidiProgramEdit.cpp` |
| CV | 8 db csak olvasható feszültség-csúszka | `PE/cvgate/kick_CvGateProgramEdit.cpp` |
| Plugin (CLAP) | modális panel: Scan, Add folder, Load, Remove, Panic, hibák, paraméterlista + keresés, 4 makró a Q-Linkeken | `M/kick_ui/program_view/source/kick_ClapPanel.h`, `Kick/clap_host/juce/PluginPanel.cpp` |

**Hogyan érhető el.**
- **HW EDIT gomb** → `sigScreenClose(MenuProgramEdit)` típustól függetlenül (`A/kick/kick_hardware_bridge.h:1220-1227`); Shift+EDIT → XY FX. MPC-n fizikailag igazolt: „EDIT … fizikai nyomásra: Program Edit … OK” (`docs/AKAI_MPC_KEY61_HW_TEST_REPORT.md:90`). A mód-LED követi (`bridge:277`).
- **Program fül `edit` gomb** (`M/kick_ui/program_view/source/kick_AppProgramComponent.cpp:60-71`): Clip/Audio → Program Edit; **Drum → Sample Edit** (`MenuSampleEdit`); **Keygroup/Midi/Cv → Pad Perform**; Plugin → CLAP-modál. Következmény: a Drum és Keygroup *Program Edit* lapjai csak a HW EDIT gombbal (vagy a Menü „Program edit” csempéjével) érhetők el, a Program fül más képernyőre visz — a felhasználónak két párhuzamos Drum-szerkesztője van (Sample Edit és Program Edit).
- A Plugin-szerkesztő **csak** a Program fül `edit` gombján át nyílik (`openClapInstrument`, `kick_AppProgramComponent.cpp:62-64`; más hívó nincs). A HW EDIT gomb Plugin programnál a Program Edit „No Programs Available” üzenetét hozza (`kick_ProgramEdit.cpp:216-224`).

**Hardver-gombok kötése a képernyőn** (`ProgramEdit::handleCommand`, `:228-273`; a KickHome csak akkor adja át, ha nincs fókuszált vezérlő, `M/kick_ui/dialogs/kick_AppHome.cpp:573-585`):
- **16 pad**: az aktív szerkesztő a pad-ot kiválasztja (Drum/Keygroup: közös `pads[0]` / `keys[0]` vektorelem = `padNum + bankOffset`, pl. `PE/drum/kick_MasterDrumProgramEdit.cpp:186-194`), **és** a képernyő a kijelölt trackre élő note-ot küld csatorna 1-en (`:246-256`). A „kiválasztott pad” felirat (`selectedPadLabel`, `:241`) csak a HW-úton frissül.
- **Bank A–H**: a KickHome állítja (`ae.setBank`, `kick_AppHome.cpp:523-530`); a szerkesztők `ae.getBankOffset()`-et olvasnak. Nincs képernyős bank-választó.
- **PLAY / PLAY START / STOP** (`:259-270`); Audio típusnál a **Q-Link/Data Dial** a kijelölt minta nudge/edit-start/-end lépése (`PE/audio/kick_AudioProgramEditGrid.cpp:1038-1071`).
- **Data Dial / +/- / dial-nyomás**: a legutóbb érintett vezérlőt állítja (`M/kick_ui_controls/source/kick_ControlBase.h:163-221`, igazolt: `known-issues-and-roadmap.md` §1, 0a42bfdb); fókuszált vezérlő nélkül a képernyő kapja — lásd a 3. fejezet B3 hibáját.
- **Q-Link 1–4**: Custom módban (`knobControl` mód 0) mindig a KnobEdit-hez mennek (`kick_AppHome.cpp:561-571`), **Screen módban — ez az alapértelmezés** (`knobMode{ 1 }`, `M/engine/project/kick_settings.h:218`) — a fókuszált vezérlőhöz vagy a `ProgramEdit::handleCommand`-hoz; **a Program Edit-nek nincs `knobCommand`-ja**, így nincs Q-Link→paraméter hozzárendelés (a CLAP-modál kivétel, ott a 4 makró).
- **Érintés**: minden vezérlő JUCE-komponens (KickRotary/KickNumber/lista); dupla koppintás (450 ms/24 px) a hídban igazolt (42cfdc3c). Többujjas érintés nincs (egy egér-forrás).

## 2. Funkciók és állapotuk

| Funkció | Állapot | Bizonyíték (fájl:sor) |
|---|---|---|
| Program-választó fejléc, típus szerinti szerkesztő-váltás | KÉSZ | `kick_ProgramEdit.cpp:110-117,149-226`; két párhuzamos forrás a megjelenítéskor (track programja vs kijelölt program): `:46-47` és `:296-304` → BUGOS GYANÚ (B16) |
| Plugin típus a Program Edit-ben | STUB | nincs ág: `:165-173`; üzenet `:216-224` |
| Pad-kiválasztás HW pad-del + élő hang | RÉSZLEGES / BUGOS | `:232-258` (a Data Dial/Q-Link is ide fut, B3) |
| **Drum / Main**: master level/pan/semi/fine/poly, pad level/pan/semi/fine, mute group, Layer Play, Sample Play | KÉSZ (UI→modell) | `PE/drum/kick_MasterDrumProgramEdit.cpp:79-137`; a Layer Play motorja hibás (B6) |
| **Drum / Samples**: hullámforma, réteg-választó, Sample/Loop Start–End, semi, fine, loop, reverse | RÉSZLEGES | `PE/drum/kick_DrumProgramSampleTab.cpp:55-125`; „Sample Start/End” és „Loop Start/End” **ugyanazt a két tulajdonságot** írja (`:60,73,86,99`), nincs minta-hozzárendelés |
| **Drum / Layer Mix** (réteganként semi/fine/level/pan) | KÉSZ (UI) | `PE/drum/kick_DrumProgramLayerMix.cpp:61-79`; nincs velocity-tartomány szerkesztés (B14) |
| **Drum / Envelopes** (Amp/Filter/Pitch ADSR) | **STUB a hangra** (UI→modell megy, a hang nem olvassa) | B2: `EN/audio_program/synth/drum_voice.h:37`, `drum_voice.cpp:28-32` (nincs hívó); `DrumProgram.cpp:36-39` |
| **Drum / LFO** | **STUB a hangra** + nincs enable | B2; `drum_voice.cpp:47`; `mt_audio_iprogram_atomics.h:86` |
| Szűrő típus / cutoff / rezonancia (Drum, Keygroup) | HIÁNYZIK (modell-API és UI sincs) | `rg Cutoff|Resonance` a modell-API-ban nulla; régi UI-osztály csak halott kódban (`PE/common/kick_FilterEnvEdit.cpp:5-42`, nem fordul) |
| Drum fejléc: Flatten Pads / Edit Zone / Convert to KG / pad-lista | STUB (halott) | `DrumHeader` (`PE/drum/kick_DrumProgramEdit.cpp:127-154`) sehol nincs hozzáadva; `FlattenPads`, `DrumPadEdit`, `PanVelocityEdit` .cpp-k nincsenek beépítve (`program_edit_ui_drum.cpp:21-38`) |
| Drum insert-szerkesztés | HIÁNYZIK a Program Edit-ből | `DrumGroupInsert` osztályok nincsenek példányosítva (`PE/drum/kick_DrumInserts.h:4,59,74`) |
| **Keygroup / Main**: level/pan/semi/fine, OUT bus, INSERTS gomb, mute group, poly, Layer Play, Sample Play | RÉSZLEGES / BUGOS | `PE/keygroup/kick_KeyGroupMaster.cpp:47-90`; B7 |
| Keygroup KEY TRACK | STUB | `:16` hozzáadva, nincs handler (`kick_KeyGroupMaster.h:20`) |
| **Keygroup / Samples 1** (start/end, semi, fine, réteg) | KÉSZ (UI) | `PE/keygroup/kick_KeyGroupProgramEdit.cpp:117-237` |
| **Keygroup / Samples 2** (loop, offset, tail, reverse, level, layer pan, slice) | RÉSZLEGES / BUGOS | `:256-388`; B8 |
| **Keygroup / Layer Mix** | KÉSZ (UI) | `PE/keygroup/kick_PanVelocityKG.cpp:59-152` |
| **Keygroup / Envelopes, LFO** | **STUB** (a motor figyelmen kívül hagyja) | `EN/audio_program/synth/KeygroupProgram.cpp`, `audio_voice.h`, `iSynthVoice.h`: adsr/filter/lfo/envelope grep üres; `research/codebase-survey.md` §1 |
| Keygroup zóna (min/max/root) szerkesztés | HIÁNYZIK | `EditZones` nem fordul (`program_edit_ui.cpp:17,37` kommentben); alapérték 0..127 (`EN/audio_engine/engine/mt_audio_interface_types.cpp:133-134`) |
| Réteg 2–4 minta-hozzárendelés, velocity-tartomány | HIÁNYZIK (UI-ból nem elérhető) | minden hívó `Layer=0` (`kick_AppAssignSample.cpp:110`, `kick_ClipProgramEdit_programTab.cpp:70`, `kick_PadMixer.cpp:379`…); `setLayerVelocityStart/End` hívó nincs a fordított UI-ban (csak `EN/…/mt_audio_iprogram.cpp:766-767`) |
| **Clip / Program fül** (semi, fine, launch quantize, pad-rács, dupla koppintás = minta hozzárendelés) | BUGOS (P0) | `PE/clip/kick_ClipProgramEdit_programTab.cpp:25-76`; B1 |
| **Clip / Pad fül** | BUGOS | `PE/clip/kick_ClipProgramEdit_padTab.cpp:25-159`; B10 |
| **Audio**: eszköztár, rács, pencil/erase/cut/select/drag/magnify, automation-sáv (Volume/PAN/Mute/Send1-4), mute/reverse/warp/level/bpm, duplicate, minta hozzáadása pozícióra, nudge/edit start/end Q-Linkkel | RÉSZLEGES | `PE/audio/kick_AudioProgramEditGrid.cpp:778-1071` |
| Audio: Split, ruler-seek, Fade In/Out, Semi, Fine, TC, Don't Snap, Copy/Paste gomb | STUB | `:904-915` (üres ciklus), `:830-835` (csak kiszámol), nincs `onValueChanged`/`onClick` a `fadeIn/fadeOut/semi/fine/tc/dontSnap/copy/paste`-ra (`rg` az egész fájlban) |
| **MIDI**: CC0..127 csúszkák | **BUGOS GYANÚ (magas)** — a csúszkák nem kötődnek a programhoz | `PE/midi/kick_MidiProgramEdit.cpp:76-85`: `state` sosem kap értéket (`PE/kick_ProgramEditInterface.h:54`, `rg 'state *='` nulla); B4 |
| **CV**: kimeneti feszültség-monitor | KÉSZ (csak olvasható) | `PE/cvgate/kick_CvGateProgramEdit.cpp:26-62`; hardveren nem tesztelt (`known-issues-and-roadmap.md` H18) |
| **CLAP-panel**: Scan (külön folyamat), Add folder, Load, Remove, Panic, hibalista, paraméter-lista + keresés, 4 makró a Q-Linkeken, státusz-sor (latency/peak/missed/dropped) | KÉSZ (kód), MPC-n nem igazolt | `Kick/clap_host/juce/PluginPanel.cpp:44-196`, `kick_ClapPanel.h:8-44`; hardver-bemenet-teszt: `Kick/clap_host/tests/HardwareInputTests.cpp` |
| CLAP natív plugin-GUI | HIÁNYZIK (tudatos korlát) | `Kick/clap_host/README.md` „Deliberate limits”; `research/codebase-survey.md` §2 |
| Q-Link → paraméter-hozzárendelés a Drum/Keygroup/Clip lapokon | HIÁNYZIK | nincs `knobCommand` felülírás (`kick_ProgramEdit.h`), `IScreen::knobCommand` üres (`M/kick_ui_common/source/kick_IScreen.h:106`) |

## 3. Mi nem működik / hibák

Súly: **P0** = összeomlás/adatkárosodás, **P1** = funkció hibás/félrevezető, **P2** = kisebb.

**B1 (P0) Clip: az engine tulajdonában lévő programra `shared_ptr` veszi át a tulajdonjogot.** `PE/clip/kick_ClipProgramEdit.cpp:90-91`: `pClip.reset( dynamic_cast<mt::MTClipProgram*>( pIProg ) )`, ahol `pClip` `std::shared_ptr` (`kick_ClipProgramEdit.h:63`), `pIProg` pedig az engine `std::vector<std::unique_ptr<MTIProgram>>` elemére mutat (`EN/audio_engine/engine/mt_audio_manager_program.h:82`). Hatás: két tulajdonos → másik Clip program kiválasztásakor az **előző program törlődik az engine alól** (használat-után-felszabadítás a hangszálban), kilépéskor dupla törlés. Reprodukció: két Clip programmal Program Edit → váltás az egyikről a másikra (kijelölés a fejléc-választóban) → összeomlás/hang-hiba; vagy projekt-zárás/kilépés ASAN-nal.

**B2 (P0/P1) Drum Envelopes + LFO a hangra nem hat — a `DrumVoice` atomic-jai sosem kapcsolódnak a modellhez.** `DrumVoice::attachToAndReset` (`EN/audio_program/synth/drum_voice.h:37`, `drum_voice.cpp:28-32`) az egyetlen hely, ami a `pLfo`/`pEnv` atomokat a clip-fához köti; **`rg attachToAndReset` az egész repóban csak a deklarációt és a definíciót adja, hívó nincs**. A hangok `DrumProgram.cpp:36-39`-ben `DrumVoice(master)`-ként jönnek létre, további csatolás nélkül, így a voice az alapértékeket használja (amp ADSR 0/0/0/0/127, szűrő cutoff 20000, LFO és pitch-env **kikapcsolva**). Ráadásul: (a) a modell az env-típust `IDs::TYPE`-ra írja (`mt_audio_interface_types.cpp:219,234,249`), az atomok `IDs::type`-ot olvasnak (`mt_audio_iprogram_atomics.h:114`) — a két azonosító különbözik (`mt_audio_ids.h:54,77`, `DECLARE_ID` kis/nagybetű-érzékeny); (b) a szűrő-env `type` és az ADSR `adsrType` ugyanarra a tulajdonságra csatol (`atomics.h:114` és `:159`); (c) az LFO/pitch-env/filter-env `isEnabled` alapértéke hamis (`:86,139,158,186`), és a UI-ban nincs engedélyező kapcsoló (`rg isEnabled` a `program_edit_ui`-ban nulla); (d) az LFO kikapcsolva `1.0`-t ad (`AudIO/AudIO/Dsp/lfo.cpp:163-164`), így a Pan/Pitch/Filter „LFO” csúszkák statikus eltolásként viselkednének (`drum_voice.cpp:182,189,190`). Hatás: a felhasználó látszólag működő Envelopes/LFO lapot kap, amelyik a project-fájlba ment, de nem szól bele a hangba. Ellenőrzés (hallgatással): A01 pad Amp Decay/Attack állítás → változik-e a hang. Ez a `04-screen-audit.md` §2.3 „Keygroup szűrő/burkoló/LFO stub” megállapítás **Drum-ra is érvényes**, a `screen-inventory.md` §4.9 „Drum program editors KESZ” sora ezért túlzó.

**B3 (P1) Data Dial / Q-Link forgatás pad-ütésként értelmeződik.** `PE/kick_ProgramEdit.cpp:232`: `isPadButtonMessage || isKnobMessage` közös ág; a knob-csomag mezői (`data[0]` = gomb-kód, `[1]` = irány, `[2]` = lépés; `M/contracts/dcl/dcl_contracts_helper.cpp:112-115`) pad-szám/állapot/velocity-ként olvasódnak (`:237-239`). Az irány-értékek egybeesnek a gomb-állapotokkal (`EN_KNOB_CLOCKWISE = 1 = BTN_PRESSED`, `ANTICLOCKWISE = 0 = BTN_RELEASED`: `M/contracts/dcl/dcl_contracts.h:138-142,215-217`). Hatás: fókuszált vezérlő nélkül a Data Dial jobbra forgatása `noteOn(1, bankOffset + 0, velocity = lépés)`-t küld a kijelölt trackre, balra `noteOff`-ot, és a „A01” felirat a kijelölt pad helyére íródik. Q-Link-nél (Screen mód, ami az alapértelmezés) ugyanez a gomb-kódra (1..4) — ez tehát az alapállapot, nem kivétel. MIDI programnál valódi MIDI note-on megy ki a portra. Reprodukció: Program Edit megnyitása érintés nélkül → Data Dial forgatás → halk note + felirat-ugrás.

**B4 (P1, magas gyanú) MIDI program CC-csúszkái nincsenek a programhoz kötve.** `onProgramChanged` (`PE/midi/kick_MidiProgramEdit.cpp:76-85`) a `state` ValueTree-ből építi az `attachment`-eket, de `ProgEditInterface::state` (`PE/kick_ProgramEditInterface.h:54`) sosem kap értéket (`setProgram`, `:36-45`, csak `idx`/`pIProg`-ot állít). Érvénytelen fára csatolt `Value` nem ír vissza a program `CCn` tulajdonságaiba (`research/midi-program-add.md` §2). A selector-ikonok témaváltáskor mind „0.svg”-t kapnak (`:87-94`, `id` nem nő). Hatás: a MIDI Program Edit 128 csúszkája dekoratív; a CC-ket csak a Q-Link/KnobEdit állítja (`midi-program-add.md` §2 CC-sor). Ellenőrzés: MIDI programnál csúszka mozgatás → CC jelenik-e meg a kimeneten / a projekt-XML-ben.

**B5 (P1, gyanú) Drum: a „Main” lap a megjelenítéskor mindig látható marad.** `PE/drum/kick_DrumProgramEdit.cpp:124`: `comps[sIdx]->setVisible(visible)`, de `sIdx` soha nem változik (`kick_DrumProgramEdit.h:77`, `{ 0 }`); a Keygroup helyesen `comps[tIndex]`-et használ (`kick_KeyGroupProgramEdit.cpp:114`). Hatás: ha a felhasználó a Samples/Envelopes/… lapról lép ki és vissza (vagy másik programról tér vissza), a Main lap a kiválasztott lap **fölé/alá is** láthatóvá válik. Ellenőrzés: Drum → Envelopes → MAIN gomb (képernyőváltás) → vissza EDIT.

**B6 (P1) A motor Layer Play-logikája hibás (a Program Edit Layer Play kapcsolójának hatása).** `EN/audio_program/synth/DrumProgram.cpp`: CYCLE `jlimit` — nem körbejár, hanem az utolsó rétegen ragad (`:101-107`); CYCLE/RANDOM a `validSourceIndex` *pozícióját* használja réteg-indexként (`:99,107,122`), így üres köztes réteg esetén rossz/üres réteg szólal; VELOCITY a felső határt kizárja (`Velocity < velEnd`, `:113`), tehát 127-es ütés (Full Level!) a 127-ig tartó réteget **nem** találja, és a 0. rétegre esik; a Keygroup ezzel szemben zárt tartományt használ (`research/codebase-survey.md` §1). Plusz: a Keygroup motor csak VELOCITY-t ismer, CYCLE/RANDOM nem megy (`KeygroupProgram.cpp:99`).

**B7 (P1) Keygroup / Main: csonkolt és felcserélt vezérlők.**
- A Pan és Level/Semi/Fine lambdák `juce::int64`-et vesznek, a `KickRotary` `double`-t ad (`M/kick_ui_controls/source/kick_slider.h:33`) → a program-pan **egészre csonkolódik**: csak −1/0/+1 érhető el (`PE/keygroup/kick_KeyGroupMaster.cpp:57-60`).
- `semiSlider.setRange(SEMI::MIN, SEMI::MAX, SEMI::DEFAULT)` — a lépés 0 (az alapérték került a lépés helyére, `:39`).
- **Sample Play címkék felcserélve**: `{ "One Shot", "Note On", "Note Off" }` (`kick_KeyGroupMaster.h:25`), de az enum `ONE_SHOT, NOTE_OFF, NOTE_ON` (`EN/interface/source/mt_audio_interface_types.h:139-144`); a Drum helyesen `{ …, "Note Off", "Note On" }` (`kick_MasterDrumProgramEdit.h:56`). A motorban a NOTE_ON a „tartva szól”, a NOTE_OFF a „felengedéskor szól” (`EN/audio_program/source/mt_audio_iprogram.cpp:305-335`): a Keygroup „Note On” választása valójában **felengedéskor** indít hangot.
- KEY TRACK gomb: nincs handler (`:16`).

**B8 (P1) Keygroup / Samples 2.** `slice` (`kick_KeyGroupProgramEdit.h:102`) nincs bekötve (csak frissül, `:386`); `layerPan` `KickNumberT<int>` (`:112`) `PAN::MIN..MAX` (−1..1) tartománnyal (`:291`) → a réteg-pan egész, lépés 0, a megjelenítés csonkolja a valós értéket (`:381`); a lap „Loop Start/End” és az 1. lap „Start/End” ugyanazt a két tulajdonságot írja (`:155,165` és `:298,308`).

**B9 (P2) Envelope-lapok frissítéskor modellt írnak, és a DEPTH rejtett.** `PE/drum/kick_DrumProgramEnvelopes.cpp:115` és `PE/common/kick_FilterEnvEdit.cpp:194`: `env[i].setSelected(…, juce::sendNotification)` → az `onSelectionChanged` visszaírja a típust a modellbe minden pad-váltásnál/megjelenítésnél (módosított-jelző, undo-tranzakció). A `DEPTH` csúszka csak ADSR/AHDSR típusnál látszik (`:68-71`, Index 1 és 4), pedig a hang a pitch/filter env depth-et típustól függetlenül használná (`drum_voice.cpp:185-190`) → AD típusnál (tipikus 808-kick pitch-esés) a mélység nem állítható.

**B10 (P1) Clip / Pad fül.** `muteGroup.onSelectionChanged` `Value - 1`-et ír (`:110`, lista: „Off”, 1..32, `kick_ClipProgramEdit_padTab.h`), a frissítés viszont `setSelected(muteGroup)`-ot (`:157`) → eggyel elcsúszott kijelzés; `getClip(selectedPad)->` null-ellenőrzés nélkül (`:110`); a start/end csak a határokat állítja be, az aktuális loopStart/End értéket nem tölti vissza (`:142-145`, a kikommentezett sor `:146`) → újranyitáskor 0..max látszik, a tényleges érték más lehet; a `nameLabel` sosem kap szöveget (`:28-31`).

**B11 (P1) Audio szerkesztő.** Stubok: lásd a 2. fejezet táblája. Hibák: `AudioProgramGrid::handleCommand` a `pAudio`-t null-ellenőrzés előtt dereferálja (`:1045`, az ellenőrzés `:1052`); `bpm` `KickNumberf`, de a lambda `int`-et vesz (`:894`, `.h:323`); a TRACK-lista Audio programnál csak programot vált, `setSelectedTrack`-ot nem (`kick_ProgramEdit.cpp:131-135`), miközben a rács `aeSel.track`-et használ (`AudioGridData::refresh`, `:17-29`) → a rács az előző track hosszát/tartalmát mutathatja (gyanú); `AudioGridData::refresh` `callAsync([&…])`-t használ (`:19`) → a komponens megszűnése után futó lambda (alacsony kockázat).

**B12 (P1) Plugin / CLAP.** HW EDIT nem nyitja a panelt (lásd 1.); `Add folder` natív `FileChooser`-t használ (`PluginPanel.cpp:101-111`) — MPC-n (nincs X) a viselkedés NEM ELLENŐRIZHETŐ KÓDBÓL; a mappa-fájl duplikátumokat gyűjt (`:109`, `appendText` ismétlés-ellenőrzés nélkül); a panel fix 960×540 (`:83`), a modál ablak `launchAsync` (`kick_ClapPanel.h:36-42`), a híd a desktop-komponenseket rárajzolja (`A/kick/kick_hardware_bridge.h:720-730`) — MPC-n a megjelenés nem igazolt (H5: a modális ablakok hardveres kezelése nyitott).

**B13 (P2, gyanú) Témaváltás null-hivatkozás.** `kick_ProgramEdit.cpp:144-146`: `selectedProgram->setupHeader(header)` ellenőrzés nélkül; ha a téma futás közben cserélődik, mielőtt bármelyik programot betöltötték (`M/kick_ui_controls/source/kick_Theme.cpp:185-187`), `nullptr` hívás.

**B14 (P1) Réteg 2–4 és velocity-tartomány UI-ból nem szerkeszthető.** A „Layer Mix”, „Layer Play: Velocity” és a 4-rétegű modell csak betöltött projekttel használható: minden UI-hívó `Layer = 0`-val rendel mintát (lásd 2. fejezet), `setLayerVelocityStart/End`-nek nincs UI-hívója, `setLayerSource` egyetlen hívója halott fájlban van (`PE/drum/kick_PanVelocityEdit.cpp:40`).

**B15 (P2) Halott kód zavaró nyomai.** Nem fordul: `kick_FlattenPads.cpp`, `kick_DrumPadEdit.cpp`, `kick_PanVelocityEdit.cpp`, `kick_FilterEnvEditKG.cpp`, és a kommentelt blokk `program_edit_ui.cpp:15-21,35-41` (HeaderBase, EditZones, SampleEditorPv, SampleOffset, EffectsEdit). Fordul, de nincs példányosítva: `KeyGroupInserts`, `PanVelocityEditKG` (`kick_PanVelocityKG.cpp:3-47,154-270`; a `edit1.onClick` négyszer ugyanarra az elemre van állítva, `:172-175`). A `screen-inventory.md` „kick_SampleOffset.cpp:30-45” hivatkozása ezért halott kódra mutat.

**B16 (P2, gyanú) Megjelenítéskor kettős programbetöltés.** `ProgramEdit::visibilityChanged` (`:296-304`) és `ProgramEditHeaderComponent::visibilityChanged` (`:39-49`) is `sendNotification`-nel állítja a program-választót, különböző forrásból (track programja vs kijelölt program) → minden megnyitáskor 2× `loadProgram`, és ha a kettő eltér, az utolsó nyer.

**Kisebb (P2):** Drum „Layer 0–3” (`kick_DrumProgramSampleTab.cpp:33`) vs „LAYER 1–4” a Layer Mixben (`kick_DrumProgramLayerMix.cpp:28`); Poly dupla-kattintás visszaállítás 8, az alapérték 16 (`kick_MasterDrumProgramEdit.cpp:72`, `AudIO/AudIO/Includes/Constants.h:37`); a Drum/Keygroup Samples lap minden pad-váltásnál a hullámformát az **üzenetszálon** tölti (`getOrLoad`, `kick_DrumProgramSampleTab.cpp:172`) → nagy fájlnál akadás; mute group a Drumban nyers szám (−1..32, `kick_MasterDrumProgramEdit.h`), a Clip pad fülön „Off/1..32”; a `selectedPadLabel` fix pixel-pozícióban (`kick_ProgramEdit.cpp:282`).

## 4. Tervben / hiányzik (az Akai MPC Standalone-hoz képest, a docs-ból ismert tervek)

A kézikönyv nincs a repóban (`docs/akai_mpc_hardware/manuals` üres); az Akai-összevetés a `KEY61_DOC_VS_KICK.md`-re, a WP-D/WP-G lapokra és a tervekre támaszkodik. Általános MPC-ismeretből származó állításokat „(ellenőrizendő)” jelöl.
- **EDIT gomb jelentése**: a v3.10 szerint „Track Edit Mode” (a Quickstart v1.2: Program Edit); a Kickben nincs külön Track Edit (`KEY61_DOC_VS_KICK.md` #14 vezérlő-sor). Track-szintű tartomány/szűrő/Monitor/Output/Key Range: WP-D (51 HIÁNYZIK, 34 ELTÉR), `research/midi-program-add.md` §6.
- **Program-szintű note/velocity-tartomány, transzpozíció, MIDI/CC-szűrők** (D-165, D-176, D-178, D-175, D-179): hiányzik (`midi-program-add.md` §2, §6; `04-screen-audit.md` §5).
- **Keygroup-motor** (a Program Edit lapjai ezt vezérelnék): release-ág, burkológörbe, szűrő, LFO, round-robin, voice stealing, loop-crossfade, streaming, max 4 réteg — `plans/02-piano-clap.md`, `research/codebase-survey.md` §1; `04-screen-audit.md` §2.3 javaslata: vagy bekötni, vagy elrejteni a lapokat, amíg nem működnek.
- **Plugin-szerkesztő**: `clap.gui` (natív GUI) tervben (`04-screen-audit.md` §2.3; `plans/02`, `plans/03`); Sounds/Favorites mód hiányzik (WP-G G-285..287).
- **Q-Link oszlopváltás, Shift+Q-Link konfiguráció**: nem vizsgált (`KEY61_DOC_VS_KICK.md` #3,#5 vezérlő-sor).
- **Zóna-szerkesztés (Edit Zones), Flatten Pads, Convert to KG, Drum insert-szerkesztés**: a régi Behringer-UI tervezte (header-ikonok léteznek), a Kickben nem épült meg (B15).
- **Automation „Add New”, Clear Auto, TC ablak az Audio szerkesztőből**: Akai-hiányok (`04-screen-audit.md` §3, Grid-mintára).
- **MPC 16 pad-os bank-nézet (Drum pad-lista a fejlécben)**: halott fejléc-komponens (B15) — (ellenőrizendő) az Akai Program Edit pad-választója.

## 5. Javaslataim (improvementek, indoklással)

1. **Bizonyíték-alapú sorrend: előbb a hang-útvonal, aztán a felület.** B2 a legnagyobb félrevezetés: egy egész lap (Drum Envelopes, LFO, ráadásul a Keygroup is) látszólag működik. A javítás kis kódmennyiség (a clip-fa átadása a voice-nak a `noteOn` előtt, az azonosítók egységesítése, enable-jelzők), de **hallgatásos MPC-teszt** kell hozzá; a döntés, hogy a Keygroup-lapok bekötése a zongora-terv (02) motorjához tartozik-e, az ownerre vár.
2. **Félrevezető kezelőszerv ne maradjon**: amíg a Keygroup Envelopes/LFO/Layer Mix és a Drum Filter hang nélküli, vagy tiltva/szürkítve + „nem aktív” felirat, vagy rejtve (`04` §2.3 javaslata). Indoklás: a project-fájlba mentett, de hangtalan paraméter adat-bizalmi hiba.
3. **Pad-kiválasztás érintéssel is**: a halott `DrumHeader` pad-listája helyett egy 16-pad rács vagy lista (a Clip programTab `KickPadButtonGroupComponent`-jét újrahasznosítva) — MPC-n a pad-ek megvannak, de a desktop/érintős használat és a „mit szerkesztek épp” visszajelzés (selectedPadLabel érintésre is) hiányzik.
4. **Q-Link-leképezés a szerkesztő oldalakon**: 4 Q-Link = az aktív lap első 4 forgatója (a Pad Mixer/List Edit `knobCommand` mintájára), és a stray-note ág (B3) kivétele; MPC-n a Q-Link a legkézenfekvőbb finom-állító.
5. **Teljesítmény**: a `refresh()` minden pad-váltásnál újracsatlakoztatja az ScopeTracker-eket és hullámformát tölt az üzenetszálon (Samples lapok) → gyorsítótár (`getOrLoad` már van) + háttérszálon betöltés, a hullámforma csak látható lapon.
6. **Egységes elnevezés/tartományok**: Layer 1–4, Mute Group „Off/1..32”, Sample Play címkék ugyanabból a közös listából (egy forrás a két szerkesztőnek).
7. **Törlés-biztonság**: B1 javítása után tegyünk ownership-assertet (a szerkesztők csak nyers/gyenge hivatkozást tartsanak programra), valamint program törlésekor/lecserélésekor (`valueTreeChildRemoved` hiány, `04` §5) a szerkesztők érvénytelenítsék a pointereket (`pDrum`, `pKeyGroup`, `pAudio` jelenleg nyers pointer, az Audio-ban `visibilityChanged`-re frissül).
8. **CLAP-panel MPC-hez**: nagyobb (≥ 64 px) érintőcélpontok, Add folder helyett a támogatott mappák felsorolása/egyszerű lista (a natív fájlválasztó MPC-n kérdéses), „Scan” háttérfolyamat állapota a státuszsorban (már van), hibák felugró ablak helyett a lista alján.
9. **Tesztelhetőség**: a lapok `refresh()`-e és a lambda-konverziók (int64/double) tiszta egységteszttel fedhetők, ha a UI-logika mögé tesszük a leképezést (érték → modell) — lásd 6.

## 6. Hardveres és unit-tesztek

**Ami most fedi:** a Program Edit-re **nincs** egység- vagy smoke-teszt. `Kick/tests/platform/AppSmoke.{h,py}` csak CLAP-program létrehozást/duplikálást/mentést fed; `ClapInstrumentProcessorTests.cpp`, `MockClapSynth.cpp` és `Kick/clap_host/tests/*` (Host/Session/Persistence/HardwareInput/Benchmark) a CLAP-hostot. Az `AudioTests.cpp`/`SustainTests.cpp` a motor általános részét. A hardveres jegyzőkönyv: EDIT → Program Edit „OK” (`HW_TEST_REPORT.md:90,110`); a checklist mód-LED sora (`HW_TEST_CHECKLIST.md:50`) nincs kipipálva.

**Hiányzik / javasolt egységtesztek:** (1) `ClipProgramEdit` tulajdonlási teszt (ASAN, két Clip program váltása); (2) `DrumVoice`-kötés teszt: modell-változás → a voice atomok értéke (és renderelt hang eltérése); (3) a `ProgramEdit::handleCommand` knob-csomag nem generál note-ot; (4) Layer Play: CYCLE körbejár, VELOCITY 127-et eltalál (`DrumProgram`/`KeygroupProgram`); (5) Sample Play címke↔enum leképezés; (6) Clip pad mute group oda-vissza; (7) MIDI editor: CC-csúszka → program `CCn`.

**MPC-n ellenőrzendő lista:**
- [ ] EDIT megnyit; típusváltás a fejlécben; mind a 6 típus lapjai megjelennek (1024×600 skálán, nincs levágás).
- [ ] Pad-ütés kiválaszt + szól; **Data Dial forgatás érintés nélkül nem szólaltat meg hangot** (B3).
- [ ] Drum: Amp/Pitch/Filter envelope és LFO **hallható** hatása (B2); Layer Play Cycle/Velocity/Random viselkedés 127-es ütéssel (Full Level) (B6).
- [ ] Keygroup: Sample Play „Note On/Off” valós viselkedése (B7); Layer Pan finom értékei.
- [ ] Clip: két Clip program közti váltás nem omlik össze (B1); mute group kijelzés.
- [ ] MIDI program: CC-csúszka → külső szinti/MIDI-monitor (B4).
- [ ] Audio: nudge/edit start/end a Q-Linkkel és a Data Dial-lal; Split/Copy gombok „nem csinál semmit” a megjelölés szerint.
- [ ] Plugin: Program fül → Edit → panel látszik, Scan lefut, Load, 4 Q-Link makró, Panic; HW EDIT a pluginon (üzenet).
- [ ] Érintés: dupla koppintás (Clip pad-rács minta-hozzárendelés), csúszkák ujjal, lista-popup bezárása hardverrel (H5).
- [ ] Bank A–H váltás közben a kijelölt pad megmarad/változik a vártnak megfelelően.

## 7. TODO

Sorrend: hibák → hiányzó funkciók → javaslatok → tesztek. (P0/P1/P2; méret S/M/L)

- [x] **PE-01** (P0, S) Clip: a `shared_ptr<MTClipProgram>` helyett nem-tulajdonos hivatkozás (nyers pointer vagy `WeakReference`), `ClipProgramTab/PadTab` aláírás igazítása — `PE/clip/kick_ClipProgramEdit.cpp:90-91`, `.h:63`, `EN/audio_engine/engine/mt_audio_manager_program.h:82`.
- [ ] **PE-02** (P0, M) Drum envelope/LFO/szűrő kötése a hanghoz: a clip-fa átadása a `DrumVoice`-nak (`attachToAndReset` meghívása vagy helyette közvetlen csatolás), `IDs::TYPE`/`type` egységesítése, a szűrő-env `type` ütközés feloldása, enable-jelzők alapértéke/UI — `drum_voice.h:37`, `DrumProgram.cpp:36-39`, `mt_audio_interface_types.cpp:219,234,249`, `mt_audio_iprogram_atomics.h:86,114,139,158,159,186`. **MPC hallgatásos igazolás kötelező.**
- [x] **PE-03** (P1, S) A knob-csomag ne fusson a pad-ágon: `isKnobMessage` kivétele a note-küldő ágból (`kick_ProgramEdit.cpp:232`), Q-Link/Data Dial külön kezelés — `dcl_contracts.h:138-142,215-217`.
- [x] **PE-04** (P1, S) MIDI Program Edit: `state` beállítása `setProgram`-ban (`pIProg->getState()`), a selector-ikon index javítása — `PE/kick_ProgramEditInterface.h:36-45,54`, `PE/midi/kick_MidiProgramEdit.cpp:76-94`. Függ: PE-30 (teszt).
- [x] **PE-05** (P1, S) Drum: `comps[sIdx]` → `comps[tIndex]`, a `sIdx` megszüntetése — `PE/drum/kick_DrumProgramEdit.cpp:124`, `.h:77`.
- [x] **PE-06** (P1, M) Motor Layer Play javítása: CYCLE körbejár, RANDOM/CYCLE a valódi réteg-indexre, VELOCITY zárt tartomány (127), Keygroup CYCLE/RANDOM — `EN/audio_program/synth/DrumProgram.cpp:94-122`, `KeygroupProgram.cpp:99`. Függ: –; teszt PE-31.
- [x] **PE-07** (P1, S) Keygroup Main: `double` lambdák (pan), `semi` lépés, Sample Play címkék (közös lista a Drummal), KEY TRACK bekötése vagy elrejtése — `PE/keygroup/kick_KeyGroupMaster.cpp:39,57-60`, `.h:20,25`.
- [x] **PE-08** (P1, S) Keygroup Samples 2: `slice` bekötése, `layerPan` `KickRotary`/`KickNumberf` típusra, Start/End vs Loop Start/End szétválasztása — `kick_KeyGroupProgramEdit.cpp:276-386`, `.h:102,112`.
- [x] **PE-09** (P1, S) Clip pad fül: mute group oda-vissza, null-ellenőrzés, start/end visszatöltése, `nameLabel` szöveg — `PE/clip/kick_ClipProgramEdit_padTab.cpp:110,142-157`.
- [ ] **PE-10** (P1, M) Audio szerkesztő stubok: Split (`:904-915`), ruler-seek (`:830-835`), Fade In/Out, Semi, Fine, TC, Don't Snap, Copy/Paste bekötése **vagy** elrejtése; `pAudio` null-ellenőrzés (`:1045`); TRACK-lista `setSelectedTrack` (`kick_ProgramEdit.cpp:131-135`).
- [x] **PE-11** (P1, S) Plugin típus kezelése a Program Edit-ben: HW EDIT Plugin programnál nyissa a CLAP-panelt (vagy értelmes üzenet) — `kick_ProgramEdit.cpp:165-173,216-224`, `A/kick/kick_hardware_bridge.h:1220-1227`.
- [x] **PE-12** (P2, S) Envelope-lapok frissítése ne írjon a modellbe (`dontSendNotification` + külön láthatóság-frissítés); DEPTH minden típusnál látható a pitch/filter soron — `kick_DrumProgramEnvelopes.cpp:68-71,115`, `kick_FilterEnvEdit.cpp:194`.
- [x] **PE-13** (P2, S) `themeChanged` null-védelem — `kick_ProgramEdit.cpp:144-146`.
- [x] **PE-14** (P2, S) Megjelenítéskor egy forrásból töltsön programot (B16) — `kick_ProgramEdit.cpp:39-49,296-304`.
- [x] **PE-15** (P2, S) Kisebb: Layer 1–4 felirat, Poly reset 16, Mute Group „Off/1..32” egységesen — `kick_DrumProgramSampleTab.cpp:33`, `kick_MasterDrumProgramEdit.cpp:72`.
- [ ] **PE-16** (P2, S) Halott kód takarítása (B15) **az owner jóváhagyásával** — `program_edit_ui.cpp:15-21,35-41`, a nem fordított .cpp-k, `KeyGroupInserts`, `PanVelocityEditKG`.
- [ ] **PE-20** (P1, L) Réteg 2–4 mintakezelés + velocity-tartomány UI (Samples lap réteg-gombjai mintát fogadjanak; start/end velocity) — `PE/drum/kick_DrumProgramSampleTab.cpp:138-163`, `EN/audio_program/source/mt_audio_iprogram.cpp:760-767`. Függ: PE-06; az Assign Sample réteg-választóval (AS-10) együtt tervezendő.
- [ ] **PE-21** (P1, L) Szűrő (típus/cutoff/rezonancia) modell-API + UI Drumra (és Keygroupra a motor kész állapotában). Függ: PE-02.
- [ ] **PE-22** (P1, L) Keygroup Envelopes/LFO/Layer Mix motor-bekötés — a zongora-terv (02) motorjával egyesítve; addig PE-23. Függ: `plans/02-piano-clap.md` döntés.
- [x] **PE-23** (P1, S) Keygroup Envelopes/LFO addig jelezze „nem aktív”-ként (szürke + felirat) vagy rejtse — `04-screen-audit.md` §2.3. (Owner dönt PE-22 vs PE-23.)
- [x] **PE-24** (P1, M) Zóna-szerkesztés (min/max/root) Keygroupra — `mt_audio_interface_types.cpp:133-134`; Drum Flatten/Convert (döntés).
- [x] **PE-25** (P1, M) Q-Link→paraméter leképezés a Program Edit lapjain (`knobCommand`, 4 Q-Link/lap) — `M/kick_ui_common/source/kick_IScreen.h:106`, mintának `PadMixer::knobCommand` (`kick_PadMixer.cpp:628`).
- [x] **PE-26** (P2, M) Pad-kiválasztás érintéssel (pad-rács/lista a fejlécben) + `selectedPadLabel` érintésre is.
- [x] **PE-27** (P2, M) Samples lap hullámforma háttérszálon / cache; `refresh` ScopeTracker újracsatlakozás csökkentése — `kick_DrumProgramSampleTab.cpp:172`.
- [ ] **PE-28** (P2, M) CLAP-panel MPC-s érintő-ergonómia + Add folder alternatíva + dedup (`PluginPanel.cpp:101-111`); a panel MPC-s megjelenésének igazolása (H5).
- [x] **PE-30** (P1, M) Egységteszt: MIDI-editor CC-kötés, Clip pad mute group, knob-csomag ≠ note (`Kick/tests/platform/`).
- [x] **PE-31** (P1, M) Motor-egységteszt: Layer Play (Cycle/Random/Velocity 127), Sample Play enum↔címke, DrumVoice-kötés (modell→atom→renderelt eltérés).
- [x] **PE-32** (P1, M) ASAN-os Clip programváltó smoke (az `AppSmoke.py` bővítése Clip/Drum/Keygroup programokkal és a Program Edit megnyitásával).
- [x] **PE-33** (P1, M) MPC hardveres ellenőrzőlista bővítése a 6. fejezet pontjaival (`docs/AKAI_MPC_KEY61_HW_TEST_CHECKLIST.md`), eredmény-rögzítés a jegyzőkönyvben.

### Állapot (S-F, `sonnet/s-f-padperform`)

Kész (host-teszt: `LayerPlayTests.cpp`, `AudioSplitTests.cpp`; Mac smoke: `KICK_SMOKE_PROGRAMEDIT=1`, ProgramEditSmoke.h):
PE-01 (Clip: nem-tulajdonos pointer, a smoke Clip A/B váltást ellenőriz), PE-03 (a knob-csomag nem pad), PE-04 (MIDI CC-csúszkák a program fájára kötve, smoke), PE-05, PE-06 (Layer Play: Cycle körbejár, valódi réteg-szám, zárt velocity tartomány, Keygroup Cycle/Random), PE-07 (Keygroup Main: double pan/semi/fine, Sample Play címkék közös lista + `static_assert`, KEY TRACK elrejtve), PE-08 (layerPan float, Slice bekötve; a Start/End és Loop Start/End ugyanaz a modell-pár, az engine-nek csak egy van), PE-09, PE-10 (Split megvalósítva; Fade/Semi/Fine/TC/Don't Snap/Copy elrejtve, mert nincs mögöttük modell/motor; `pAudio` null-védelem, TRACK-lista `setSelectedTrack`), PE-11 (HW EDIT Plugin programnál megnyitja a CLAP-panelt), PE-12, PE-13, PE-14, PE-15, PE-20 részben (réteg velocity-tartomány a Layer Mix lapon; minta-hozzárendelés a 2-4. rétegre az Assign Sample réteg-választójával együtt: másik worker fájljai), PE-23 (a Keygroup Envelopes/LFO/Layer Mix rejtve marad, nem kapcsoltam vissza), PE-24 részben (Keygroup zóna min/max; Flatten/Convert döntés), PE-25 (Q-Link 1-4 = az oldal első négy forgatója), PE-26 (PAD lista érintéses kiválasztáshoz), PE-27 (háttérszálas hullámforma-betöltés, a hangcache zárolva), PE-30/31/32/33.
Drum Envelopes + LFO: elrejtve (`kDrumEnginePagesVisible = false`), mert a `DrumVoice` atomjai sosem kapcsolódnak a modellhez (B2).
Nyitva: PE-02 és PE-21 (hang-kötés és szűrő: hallgatásos MPC-teszt kell), PE-16 (halott kód törlése: owner jóváhagyás), PE-22 (Keygroup-motor: owner-döntés), PE-28 (`Kick/clap_host/*`: másik hatókör).
