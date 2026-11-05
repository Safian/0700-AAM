# Grid képernyő és zongora/billentyűzet nézet: felmérés (2026-11-04)

Csak olvasás. Forrás: kód (ág `claude/kick-plans`, worktree `Kick/`), `docs/`, és az MPC Standalone OS User Guide v3.10 szövege (a PDF a `.worktrees/kick-mpc-test/docs/hardware/key61/manuals/` alatt van; a kick-plans worktree `manuals/` mappája üres, csak `.gitignore`). Jelölés: KÉSZ / RÉSZLEGES / STUB / BUGOS GYANÚ / NEM ELLENŐRIZHETŐ KÓDBÓL. Útmutató-oldal = nyomtatott oldalszám.
Fájlok rövidítve: `G/` = `Kick/modules/kick_ui/audio_components/gridview/`.

## 0. Terminológiai korrekció (fontos)
- A Kick (és az MPC Standalone) **"Grid View" = eseményszerkesztő / piano roll** (jegyzetek, velocity), NEM clip-indító rács. Útmutató p.165: "lets you view and edit the note events or audio regions of each track". A kérdésben felsorolt clip/scene-indítás, rács-cella indítás, színes clip-ek, rács-szintű mute/solo/hangerő **nincs** sem a Kick Gridjében, sem az MPC Grid View-jában.
- A legközelebbi "indító" funkciók: `Kick/modules/kick_ui/modes/kick_nextSequence.cpp` (Next Sequence: pad = szekvencia, Next Bar / Sudden / Hold / Clear gombok, :128-197), `kick_song.cpp`, `kick_padMute.cpp`, `kick_trackMute.cpp`. `MTProgramType::Clip` létezik, de csak mint program-típus (nincs clip-launcher UI; pl. `kick_AppAssignSample.cpp:365` üres ág). Az MPC "Clip Launching Behavior" (Pro Pack, p.179) nincs.
- A "mainmode/" mappa (`dialogs/mainmode/kick_AppHumanize.*`) csak a Humanize dialógus; a Main Mode (`dialogs/kick_AppMainMode.*`) Sequence / Track / Program fülek listanézete (add/del/undo/redo, program-típus választó), nem Grid.

## A) GRID

### A1. Funkció-állapot (Kick)
| Funkció | Állapot | Bizonyíték |
|---|---|---|
| Képernyő: fejléc, időléptékes sáv, billentyű/pad-oszlop (Drum: pad-nevek, Keys: zongora-sáv), jegyzet-rács, playhead, velocity/paraméter-sáv | KÉSZ | `G/kick_AppGrid.cpp:134-430`, `kick_AppGrid.h` (NotesView, ScaleComponent, GridPlayhead) |
| Nézet-típus Drum / Keys programtípus szerint (Drum, Clip -> pad-lista; Keygroup, Plugin, Midi, Cv -> zongora) | KÉSZ | `kick_AppGrid.cpp:467-486` |
| Audio-track nézet (hullámforma, régió-eszközök, Warp, Flatten) | HIÁNYZIK: Audio programnál a Grid kilép a Program Editre | `kick_AppGrid.cpp:156`; `setViewType` switch nem kezeli az `Audio` esetet |
| Eszközök: Pencil, Eraser, Select, Magnify | KÉSZ (Magnify lásd lent) | `kick_AppGridHeader.cpp:45-48`, `G/kick_Grid*Tool.cpp` |
| Pencil: koppintás = jegyzet, koppintás jegyzetre = kijelölés, húzás = mozgatás, dupla koppintás = törlés, Shift+koppintás = törlés, téglalap-kijelölés | KÉSZ | `kick_GridPencilTool.cpp:123-185` |
| Eraser: koppintás / húzás törli | KÉSZ | `kick_GridEraseTool.cpp:21-33` |
| Select: doboz-kijelölés, mozgatás | KÉSZ | `kick_GridSelectTool.cpp:43-145` |
| Copy-húzás eszköz (`GridCopyTool`) | STUB/ELÉRHETETLEN: létrehozva (`kick_GridComponent.cpp:57`), de nincs fejléc-gomb, amely `GridToolType::GridCopyTool`-t választana | `kick_AppGridHeader.cpp:45-48` (csak 4 tool jelez) |
| Magnify (húzás-görgetés, kétujjas csípés-zoom) | BUGOS GYANÚ (MPC-n halott): minden ág `if(event.source.isTouch())`; az MPC "manual" érintés-útja a főegér-forrást adja (nem touch) | `kick_GridMagnifyTool.cpp:14,32,45`; `apps/kick/kick_hardware_bridge.h:542,572,620` (`getMainMouseSource()`), `docs/hardware/key61/hw-test-report.md` 13. kör (`delivery=manual` 184, `peer` 0) |
| Cut / Copy / Paste gombok (kijelölés, beillesztés a playheadnél) | KÉSZ | `kick_AppGrid.cpp:230-246`, `kick_AppGridHeader.cpp:50-87` |
| Select All, Duplicate, Delete gomb, Solo, Clear Auto (útmutató p.168-171) | HIÁNYZIK (Delete = Cut; Select All/Duplicate nincs) | nincs találat a `G/` alatt |
| Edit Start / Edit End / Transpose / Nudge / Don't Snap (a Data Dial vagy +/- állítja) | KÉSZ | `kick_ParameterView.cpp:11-60`, `kick_AppGrid.cpp:564-590` (dial-ág), `:618-780` (helperek) |
| Quantize / időosztás (TIME DIV lista) | KÉSZ (a sequence TempoMgr quantizációját állítja) | `kick_AppGrid.cpp:265-271` |
| Timing Correct ablak a Gridből (óra-ikon, Shift+TC) | HIÁNYZIK: a TC ablak csak a List Editben van (`modes/kick_ListEdit.cpp:156`) | grep `TimingCorrect` a `G/` alatt: nincs |
| Zoom: +/- gombok, alapzoom-gomb, Q-Link 3/4 | RÉSZLEGES; Q-Link zoom BUGOS GYANÚ: lépés `value/100` (Key 61-en delta = ±1 -> 0,01 egység) | `kick_AppGrid.cpp:605-609` |
| Görgetés: nézet-szinkron viewportok, Q-Link 1/2 | KÉSZ | `kick_AppGrid.cpp:402-428`, `:599-603` |
| Playhead + Auto-Scroll (Off/Follow/Page), seek a léptékre koppintással | KÉSZ | `kick_AppGrid.cpp:287-298`, `:877-925` |
| Track-váltó (fejléc lista), track hossz (bars) | KÉSZ | `kick_AppGrid.cpp:152-179`, `kick_AppGridHeader.cpp` |
| Grid Settings: Snap Mode (Absolute/Relative), Hitting Pad Selects All, Auto-Scroll | KÉSZ | `kick_GridData.cpp:7-90` |
| Q-Link Playhead Increment, MIDI Rec Erase (útmutató p.166) | HIÁNYZIK | nincs a `GridSettings`-ben |
| Velocity / Probability / Ratchet sáv (rajzolás, kijelölt jegyzetekre) | KÉSZ | `kick_ParameterView.cpp:100-220` |
| Automation sáv: "Add New" paraméter, rajzolás, törlés (p.176) | HIÁNYZIK (csak a 3 fix paraméter) | `kick_ParameterView.h:38` |
| Grid Edit menü (12 elem): Clear, Double Length, Half/Double Speed, Pitch Quantize, Humanize, Random Events, Split Events | KÉSZ, kivéve lent | `kick_AppGrid.cpp:355-390` |
| ... Trim To Length (4), Convert To Progression (9), Bounce To Sample (10), Bounce To Audio Track (11) | STUB (üres `case`) | `kick_AppGrid.cpp:368-370, 383-385` |
| ... Humanize ablak: nincs Velocity-humanize (az motor tudja: `isVelocityEnabled`), a "Strength" csúszka a hosszra megy | RÉSZLEGES | `kick_GridEditOptions.h:46-50`; `engine/.../mt_audio_note.cpp:1660`; a teljes változat `dialogs/mainmode/kick_AppHumanize.cpp:294` |
| Split ablak megszakítás: a popup szivárog (delete csak APPLY ágon) | BUGOS GYANÚ (kicsi) | `kick_AppGrid.cpp:948-975` |
| Time-signature beszúrás UI (`TimeSigSetting`) | STUB/árva: az osztály sehol nincs bekötve | grep `TimeSigSetting`: csak saját fájljai |
| Ütemváltás-API (`insertTimeSig`/`deleteTimeSig`) | KÉSZ az API, UI nincs | `kick_GridComponent.cpp:185-192` |
| Jegyzet némítás (Shift+Ctrl+End/Home) | RÉSZLEGES: csak asztali billentyű | `kick_GridPencilTool.cpp:225-238` |
| Hardveres pad: hang + a sor kijelölése | BUGOS GYANÚ: a bankeltolás kétszer adódik (`padNo += bankOffset` :542, majd `padNo + ae.getBankOffset()` :558), A banknál (offset 0) nem látszik; B-D bankon rossz sor | `kick_AppGrid.cpp:542,558`; egyezik a postaládás megfigyeléssel ("Grid csak az A bankot mutatja", `hw-mailbox/TO_CLOUD.md:32`) |
| Hardveres COPY / ERASE | BUGOS GYANÚ: nincs gomb-állapot ellenőrzés; a híd press ÉS release csomagot küld (`kick_hardware_bridge.h:1159-1170`, `:1486-1490`) -> COPY kétszer szúr be (`pasteSelectedNotesDefault`), ERASE kétszer töröl | `kick_AppGrid.cpp:503-513` |
| Hardveres EN_KNOB_CW / CCW (jegyzet-szerkesztés) | STUB (`///TBD`) | `kick_AppGrid.cpp:516,521` |
| Mute / Solo, hangerő, program-fejléc, szín, loop-hossz, tempó a Gridben | NINCS (a Gridnek nem feladata); track-hossz van; tempó/loop a státuszsor / Locate | lásd 0. pont |

### A2. Key 61 hardver a Gridben (a híd -> DCL -> `AppGrid::handleCommand`)
| Gomb | Hatás Gridben | Forrás |
|---|---|---|
| GRID / Shift+GRID | Grid View / Step Sequencer megnyitása (mért: OK) | `kick_hardware_bridge.h:1240-1246`; `doc-vs-kick.md` 15. sor |
| 16 pad | note on/off ch1 a kijelölt trackre (`addLiveMidiMsg`) + sorkijelölés (HittingPad) | `kick_AppGrid.cpp:535-560`; pad-szín: `hardware/pad_colour_map.h` |
| Data Dial | az aktív mód (Edit Start/End, Transpose, Nudge) a kijelölt jegyzetekre | `kick_AppGrid.cpp:564-590` |
| +/- | `EN_PLUS/MINUS` (a Grid külön nem kezeli) | `kick_hardware_bridge.h:1220-1230` |
| Q-Link 1-4 | csak `EN_KNOB_1..4` jut el (`on_encoder_turn` `encoder_idx<4`): 1/2 görgetés, 3/4 zoom; az 5-10 (BBT, metronóm) a Key 61-en elérhetetlen | `kick_hardware_bridge.h:1539-1550`, `kick_AppGrid.cpp:594-617` |
| PLAY / PLAY START / STOP | transzport | `kick_AppGrid.cpp:524-531` |
| COPY / Shift+COPY (=ERASE) / ERASE | lásd BUGOS GYANÚ fent | `kick_hardware_bridge.h:1159-1170` |
| UNDO / Shift+UNDO | `performUndo/Redo` (mért) | `:1333` |
| `<`/`>`/`<<`/`>>`, LOCATE | playhead-léptetés / seek (megállítva is) | `:1342-1380` |
| TC | TC ablak (List Edit-féle), nem Grid-specifikus | `:1318` |
Nincs Key 61-gomb a Grid-eszközök (Pencil/Eraser/Select) váltására, a zoomra, a Select All-ra; csak érintés. A pad-aftertouch ki van kapcsolva (`kSendPadAftertouchPackets=false`, `:1783`).

### A3. Ami az Akai MPC Grid View-jában van, itt hiányzik (útmutató p.165-176)
- Audio-track Grid (p.167-169): Marquee/Arrow/Pencil/Eraser/Scissors/Mute eszközök, Flatten (Elastique), Warp (BPM/Semi/Fine/Algorithm), Fade In/Out, Level, Reverse, Split, Duplicate, Bounce.
- Select All / Duplicate / Delete / Solo / Clear Auto (Shift+gomb a képernyő alján), "Automation" szerkesztés a dial-lal, Velocity-gomb a dial-lal.
- Grid-and-magnifier "aktív terület" (max. 3 pad-bank, 32 ütem); Kick: csak az alapzoom-gomb.
- TC ablak a Gridből; Q-Link Playhead Increment; MIDI Rec Erase.
- Automation sáv "Add New" paraméterrel; Probability/Ratchet megvan.
- Grid Edit: Trim To Length, Convert to Progression, Bounce (üres), Pitch Quantize (megvan), Humanize velocity.
- Pinch-zoom érintésen (kód megvan, de MPC-n nem jut el: 0. lent).
- A walkthrough WP-E (Grid, Step Seq, List Edit, útmutató p.165-212) NINCS elkészítve; csak WP-D és WP-G van (`docs/hardware/key61/walkthrough/`). A fenti összevetés ezen felmérés saját munkája, nem claims-register.

### A4. Ismert hibák a dokumentumokból (kód-igazsággal egyeztetve)
- H30: Grid csak A bank sorait mutatja / hang "nincs bent" (`known-issues-and-roadmap.md`); a kód-ok valószínűleg a dupla bankeltolás + a nézet-pozíció (`kick_AppGrid.cpp:186-190` bankChanged görgetés).
- H4: List Edit dial nem látszik hatni (nem Grid).
- `docs/hardware/key61/*.md` Gridet érintő sorai: TODO L67 (GRID gomb OK), L101 (pad dupla trigger: a Grid maga indít hangot, ez a `handleCommand` pad-ág).
- Az érintés a GridComponentre működik (HW report 13. kör célpontok között).

## B) ZONGORA / BILLENTYŰZET NÉZET ÉRINTŐ FELÜLETEN

### B1. Mi van
1. **`ui::KeyboardComponent`** (`Kick/modules/kick_ui_components/source/kick_KeyboardComponent.{h,cpp}`, 690 sor): JUCE MidiKeyboardComponent-átirat. Fordul (`kick_ui_components.cpp:35`), de **sehol nincs példányosítva** (grep: csak a saját fájljai, `apps/midi/*` demó, és a `clap_host/tools/Player.cpp` a JUCE-eredetit használja).
   - Tud: fix 61+ billentyű-rács, `firstKey=48`, `keyWidth=16`, fel/le oktáv-gombok (`UpDownButton`), fehér/fekete billentyű hit-test, **velocity az érintés magasságából** (`useMousePositionForVelocity=true`, privát flag; `remappedXYToNote` :639-681), per-ujj követés `e.source.getIndex()` alapján (`mouseOverNotes/mouseDownNotes` 32 elem, :204-257), MIDI csatorna 1, qwerty-leképezés regisztrálva (:55-58).
   - NEM tud / hibás: nincs `mouseDrag` -> nincs glissando; nincs `keyPressed` (a qwerty-tábla halott); a `state.addListener(this)` ki van kommentezve (:61) -> külső note-on nem rajzolódik; nincs pitch bend / vibrato / sustain / aftertouch; nincs skála-/akkord-jelölés; a velocity a billentyű tetején 0 felé megy (`jmax(0, y/hossz)`): 0 velocity = note-off kockázat; paint-ben `getHeight() - 108` varázsszám.
2. **`ui::KickVirtualKeyBoard`** (`kick_ui_controls/source/kick_VirtualKeyboard.*`): **szöveges** (QWERTY/szám/szimbólum) képernyő-billentyűzet, nem zongora. (`kick_mobile_t_layout.h:745` is ezt ismeri fel.)
3. **Pad Perform / Notes / 16 Level** (`modes/padperform/kick_PadPerform.*`, 822 sor): a 16 padra Chromatic / Notes (skála) / Chords / Progression / Chromatic Chords kiosztás (root, scale, chord, oktáv, bank); a képernyő mutatja a kiosztást, de a hangot csak a **hardveres pad-csomag** indítja (`kick_PadPerform.cpp:41-65`), a képernyő-pad érintése nem (nincs `OnButtonClicked` kötés). Key 61: KEYBOARD CONTROL és Shift+16 LEVEL is ide visz (`kick_hardware_bridge.h:1150-1153`). Az MPC "Keyboard Control" menü (Transpose, Channel, Aftertouch, Velocity Response, Scale Filter, routing) hiányzik (`doc-vs-kick.md` 19. sor).
4. **`Kick/apps/mpc_gui/main_gui.cpp`** (1686 sor; NINCS az `apps/CMakeLists.txt`-ben): régi önálló prototípus, amelyen van **61 billentyűs érintő zongora** (36 fehér, `get_keyboard_note_at` :1186-1224), **10 ujjas** slot-követés (`MAX_TOUCH_SLOTS`, :149, :1265-1420), csúszás-váltás (a `on_touch_move` = `on_touch_down`: a hang vált az ujjal), oktáv-sáv -3..+3 (:1045-1069), fix velocity 105 (pad: 115), 21 mintás F9 zongora. Pitch bend / vibrato / sustain érintésre nincs; sustain csak a fizikai pedálról (CC64). Nem a Kick UI-jának része.
5. **`Kick/apps/mpc_piano/main.cpp`** (ALSA additív szintetizátor + MIDI thru): csak fizikai billentyűk (port 16:0 -> 16:2), sustain/pitch bend/aftertouch; nincs érintés (`touch_probe.c` csak az ABS-tartományt írja ki). Nincs a CMake-ben.
6. **`Kick/apps/multitouch/Main.cpp`**: JUCE csípés-zoom demó (`KICK_MULTITOUCH=OFF` alapból, `CMakeLists.txt:37`); a Grid Magnify eszköz mintája.
7. **`docs/planning/plans/02-piano-clap.md` + `assets/piano-ui/`**: a terv a zongora-CLAP (motor, minta, plugin-GUI). A mockup (`main-dark.html`, `compact.html`: 88-billentyűs geometria `build.py`, rövid billentyűzet a kompakt nézetben) a **plugin saját ablakához** készült (borostyán `#FFB608`, cián `#00AAEE`); nem érintő-játék tervrajz. A terv szerint a Kick nem ágyaz be plugin-GUI-t (`clap.gui` sehol; `research/codebase-survey.md` L48): a Kickben a zongora paraméter-sorokon + 4 makrón át érhető el. Az érintő-zongora nézet tehát **Kick-oldali UI**, nem a pluginé.
8. MPC Standalone: az útmutató szerint **nincs** képernyő-zongora (a "virtual keyboard" csak szövegbevitel); az MPC-n a zongora = Key 61 billentyűzet + Pad Perform (p.78-84).

### B2. Érintő-bemenet útvonala MPC-n
- evdev ILI2117 (`/dev/input/event0`, kereséssel), MT slot/tracking-id, **10 slot**: `hardware/adapters/akai_mpc_key61_adapter.h:200-270` -> `on_touch_down/move/up(slot,x,y)` (1280x800 fb-tér, `mapTouch` skálázás; mért max 2048).
- Bridge: `kick_hardware_bridge.h:845-858` -> `dispatchTouch` (`callAsync` az üzenetszálra, :~443-640). Két út: **peer** (`ComponentPeer::handleMouseEvent` per-ujj `ST::touch` index = slot; a JUCE-patch `patches/juce_multitouch_7.0.12.patch` engedi) és **manual** (közvetlen `mouseDown/Drag/Up`).
- **MPC-n a manual út fut** (nincs X kijelző, ablak rejtett): `chooseDeliveryPath` (`hardware/touch_space.h:80`); mért `delivery=manual` 184, `peer` 0 (HW report 13. kör). A manual út MINDEN ujjhoz `getMainMouseSource()`-t ad (egy forrás, `isTouch()==false`, `getIndex()==0`), a húzás csak a 24 px-es slop után továbbítódik, az első ujj célpontja ragad (`m_touch_targets[slot]`). Következmények egy zongora-widgetre: (a) az ujjak azonosíthatatlanok (ua. `fingerNum=0` -> a második ujj `mouseDownNotes[0]`-t felülírja, az első hang note-off-ot kap: akkord nem megy); (b) a mouseDrag csak az első-ujj `downTarget`-nek érkezik; (c) a `touch X Y` kick_cmd csak slot 0 (`:1944-1957`, `:2050`), tehát többujjas bemenet parancsból sem injektálható; (d) multitouch hardveren nincs mérve (CHK §5 "Multitouch (2 ujj)" nyitott; `known-issues` 3.5).
- Nyomás / terület: az ILI2117 adapter nem olvas `ABS_MT_PRESSURE`/`TOUCH_MAJOR`-t (`grep`: nincs). Velocity tehát csak hely-alapú lehet (mint a `KeyboardComponent`-ben).

### B3. Touch -> hang útvonal és késleltetés
- Kész eszközök: `ae.addLiveMidiNote(sel, note, vel)` / `ae.addLiveMidiMsg(sel, msg)` (`modules/engine/audio_engine/engine/mt_audio_interface.cpp:1867-1878`); ezt használja a fizikai billentyű (`kick_hardware_bridge.h:1823-1840`, `m_key_target` a note-off célhoz, `KeyTranspose` oktáv/félhang), a pedál (`m_sustain`, CC64, `:1694`), pitch bend (`sendPitchBend`, `:1686`), mod wheel/expression (`:1708-1715`), csatorna-aftertouch (`:1703`).
- Egy érintő-zongora ugyanezt hívhatja; az útvonal üzenetszál -> engine live-MIDI sor -> hangblokk.
- Késleltetés: NEM ELLENŐRIZHETŐ KÓDBÓL (nincs mért érték). Tényezők: evdev szál -> `callAsync` -> üzenetszál, amely közben 30 Hz-en az egész 1280x800 UI-t szoftverrel festi a framebufferre (`timerCallback`, `startTimerHz(30)` :814); az audio-puffer mérete az MPC-n nincs a Kick appban kiolvasva. Az érintő-zongora hang-késését hardveren le kell mérni; a fizikai billentyű ALSA-szál -> engine közvetlenebb.
- Pitch bend: motor-oldalon csak keygroup/dob programra, fix +-2 félhang, blokkonként lépcsőzik (H13, TODO T26). Sustain: csak NOTE_ON mintára hat, a generált zongora-projekt ONE_SHOT (`codebase-survey.md` L23).

### B4. Mi hiányzik a teljes értékű érintő zongorához (Key 61 billentyűivel együtt)
1. **Beépített nézet**: Kick-képernyő (új `Screen`), a `ui::KeyboardComponent` bekötésével vagy átírásával; billentyű-szám/tartomány beállítható, oktáv-váltás (a `KeyTranspose` állapot közös a fizikai OCT+/- gombokkal, hogy a két bemenet ne essen szét).
2. **Többujjas polifónia az MPC útvonalon**: a manual út per-ujj forrást kell adjon (valódi `MouseInputSource` touch index vagy a widgetnek közvetlen `on_touch_down(slot)` csatorna); enélkül egy ujj játszik. Valamint hardveres 2+ ujj mérés (CHK §5).
3. **Glissando / csúszás**: `mouseDrag` + `mouseDraggedToKey` implementáció (jelenleg üres), a 24 px slop-latch nem akadály, de ujjanként kell.
4. **Velocity**: választható mód (fix / helyalapú / "Touch Sense"); nyomás-érzékelés nincs a hardveren; a 0 velocity-védelem.
5. **Pitch bend / vibrato érintésre**: x/y elmozdulás -> `pitchWheel` / CC1 (a motor-oldali bend-lépcsőzés H13 és a +-2 félhang határ).
6. **Sustain / hold**: képernyő-gomb -> `m_sustain` (a strip-Sustaint és a pedált már kezeli külön HeldTarget-tel), a ONE_SHOT-probléma miatt a zongora-programnál a hatás is kell.
7. **Kiosztás-jelölés**: skála / akkord / root kiemelés a billentyűkön (a `PadPerformContent` `noteData_t` + `mt::ScaleAndNotes` adatait újra lehet használni); aktív hang kirajzolása külső (fizikai) bemenetre (a kikommentezett `state.addListener` pótlása).
8. **Együttműködés a fizikai billentyűzettel**: közös cél-track (`m_key_target`, `sel`), közös transzpozíció, közös note-off logika (hogy egy track-váltás ne hagyjon lógó hangot), Pad Perform kiosztás (a hardveres Notes/Chords módot a képernyő-zongorán is érvényesíteni).
9. **Késleltetés-mérés** hardveren és a festés 30 Hz-es terhelésének hatása a touch-eseményekre.
10. **Felvétel**: a `addLiveMidiMsg` élő jegyzetet a Rec Arm track felveszi (az MPC-n mért útvonal: pad/billentyű -> sáv), tehát az érintő-hang is bekerülne; az érintő billentyű-hanghoz is kell note-off az engedéskor.

### B5. UI-minta
- `docs/planning/plans/assets/piano-ui/` (88 billentyű geometria `build.py`; `compact.html` 640x400 rövid billentyűzet) a színekhez és a billentyű-arányokhoz használható. Az MPC képernyő 1280x800 fb / 1024x600 UI-fa (`fbToUi`); a mockup 960x600 és 640x400, tehát átskálázandó. Az érintő-zongora méretezése ujjhoz: a `mpc_gui` prototípus 36 fehér billentyű 1216 px-en (~33 px/billentyű, `main_gui.cpp:1049-1053`) szűk; oktáv-váltó + kevesebb látható billentyű kell.
