# Grid View – Kézikönyv-ellenőrzés (Akai MPC v3.10, Serum 2, Safian Kick) vs. Kick kódállapot

Dátum: 2026-11-11 (branch: `sync_main_next_tasks`).  
Módszer: pontonkénti összevetés a hivatalos kézikönyvekkel és a Kick forráskódjával.  
Jelölések:
- **OK**: megvalósítva, mobilon (iOS) és Akai MPC-n is használható.
- **JAVÍTVA**: ebben a körben javítva / pótolva.
- **RÉSZLEGES**: működik hardveren vagy desktopon, de mobilon korlátozott vagy hiányos.
- **HIÁNYZIK**: nincs megvalósítva, TODO-ra véve.

Források:
- **Akai MPC Standalone OS – User Guide v3.10** (Grid View fejezet, **165–176. oldal**).
- **Xfer Serum 2 – User Guide** (Clip / Arp Piano Roll & Grid szerkesztés).
- **Safian Kick kézikönyv** (`docs/manual/USER_MANUAL.md`, 2. és 5. fejezet).

---

## 1. Akai MPC Standalone OS v3.10 – Grid View (165–176. oldal)

| # | Forrás (oldal) | Funkció / leírás | Kick kódállapot (`Kick/modules/...`) | Mobilos (touch) állapot | Státusz / teendő |
|---|---|---|---|---|---|
| 1 | v3.10 p.165 | Nézetek track típus szerint: Drum (pad sorok) vs Keygroup/Plugin/MIDI/CV (zongora) vs Audio (hullámforma) | `AppGrid::setViewType` (`kick_AppGrid.cpp`); Audio külön: `AudioProgramEdit` (`kick_AudioProgramEditGrid.cpp`) | Drum + Keys működik; `Unknown` nem kezelt | **OK** |
| 2 | v3.10 p.166 | Grid Settings: `Snap Mode` (`Absolute` / `Relative`) | `GridSettings::snapMode` (`kick_GridSettings.cpp`); `nudge`/`editStart`/`editEnd` figyeli (`AppGrid.cpp`) | `GridPencilTool` rajzoláskor/húzáskor mindig `Absolute` rácsra pattint | **RÉSZLEGES** → TODO: `Relative` snap a `GridPencilTool`-ban |
| 3 | v3.10 p.166 | Grid Settings: `Hitting Pad Selects All Events` | `AppGrid.cpp:585` + `NotesView` kattintás | Hardveren és zongorabillentyűre koppintva működik | **OK** |
| 4 | v3.10 p.166 | Grid Settings: `Auto-Scroll` (`Off` / `Follow` / `Page`) | `AppGrid::updatePlayhead` (`kick_AppGrid.cpp`) | Működik | **OK** |
| 5 | v3.10 p.166 | Grid Settings: `Q-Link Playhead Increment` (`1/16 Note` vs `TC Division`) | Nincs a `GridSettings`-ben | Nincs | **HIÁNYZIK** → TODO |
| 6 | v3.10 p.166 | Grid Settings: `MIDI Rec Erase` (Overwrite vs Overdub) | Nincs a `GridSettings`-ben | Nincs | **HIÁNYZIK** → TODO |
| 7 | v3.10 p.167–168 | **Audio Tracks Grid**: 7 eszköz (Marquee, Arrow, Pencil, Eraser, Scissors, Mute, Magnifying Glass), Fade In/Out, Level, Reverse, Mute, Warp, Detect BPM, Split at playhead, Duplicate | `AudioProgramEdit` (`kick_AudioProgramEditGrid.cpp`), `AudioGridMagnifyTool` (`kick_AudioMagnifyTool.cpp`) | `AudioHeaderTools::magnify` kép nélkül van és `AudioGridMagnifyTool` régi viewport-koordináta hibás | **RÉSZLEGES** → TODO (`AudioGridMagnifyTool` + ikon javítás) |
| 8 | v3.10 p.169 | Audio/MIDI Grid: `TC` óra ikon / `Timing Correct` ablak megnyitása érintőképernyőn (`Shift + TC` / óra ikon) | `AppGrid::openTimingCorrectPopUp()` létezik, de csak hardveres `EN_TC` gomb hívja (`kick_AppGrid.cpp:498`) | Mobilon (hardveres `TC` gomb nélkül) a Gridből közvetlenül nem nyitható a teljes `Timing Correct` (Swing) ablak, csak a Time Div lista (`timeDivListBox`) | **RÉSZLEGES** → TODO (`TC` gomb / gyorselérés a Gridben) |
| 9 | v3.10 p.170 | `Pencil` (Draw Mode): üres helyre koppintva hang beszúrása, húzás, áthelyezés, élhúzás, dupla koppintás = törlés | `GridPencilTool` (`kick_GridPencilTool.cpp`) | `mouseDown` kurzorfrissítés érintésre javítva, élhúzás (`Edit Start`/`Edit End`) 16 px-re szélesítve, kvantálatlan 6 px slop dupla-koppintás törlés | **JAVÍTVA** |
| 10 | v3.10 p.170 | `Eraser` (Erase Mode): egyedi koppintás és gyors áthúzás, kijelölt hang törlése mindet törli | `GridEraseTool`, `MTNoteList::find` (`mt_audio_note.cpp`) | Rendezetlen `ValueTree` korai `break` javítva, `yToNote` 1:1 sorleképezés, 6 px tap-slop, 4 px húzási mintavétel | **JAVÍTVA** |
| 11 | v3.10 p.170 | `Select Box` (Select Mode): dobozos kijelölés, kijelölt hangok mozgatása, eszközváltás után kijelölve marad | `GridSelectTool` (`kick_GridSelectTool.cpp`) | `mouseDown` kurzor + 4 px keskeny-hang találati küszöb javítva | **JAVÍTVA** |
| 12 | v3.10 p.170 | `Magnifying Glass` (Navigation Mode): 1 ujjas pan + 2 ujjas pinch zoom (H / V pitch / mindkettő) | `GridHeader`, `GridComponent`, `GridMagnifyTool`, `grid_logic::computePinchPan` | Nagyító gomb ikon pótolva; **2 ujjas pinch (idő + pitch) és 2 ujjas görgetés már minden eszközben** működik, 1 ujjas pan pedig a `Magnify`-ban | **JAVÍTVA** |
| 13 | v3.10 p.171 | Alsó bal oldali alapzoom (`Z` / Grid+Magnify ikon) | `ParameterViewControl::defaultZoomBtn` (`kick_AppGrid.cpp`) | MIDI/Keygroup nézetben a középső oktávra (`64`) pozícionál (`15` helyett) | **JAVÍTVA** |
| 14 | v3.10 p.171 | **Hosszú nyomás / Note Properties** ablak: `Note/Pitch`, `Start` (BBT/tick), `Length`, `Velocity`, `Probability`, `Ratchet`, `Mute`, `Delete` | `GridComponent` 450 ms timer → `AppGrid::openNotePropertiesPopUp` → `GridNotePropertiesDialog` + `MTAudioInterface::editNoteMute` | Ujjal hosszan nyomva (~450 ms) megnyílik, közvetlen `- / +` érintőgombokkal módosítható billentyűzet nélkül | **JAVÍTVA** |
| 15 | v3.10 p.171 | `Undo` / `Shift + Undo` (Redo) | `MTUndo` + hardver gomb | Mobilon a Grid fejlécben nincs külön érinthető Undo/Redo gomb (csak statusbar Undo felajánlás egyes műveleteknél) | **RÉSZLEGES** → TODO (érinthető Undo/Redo ikon mobilon) |
| 16 | v3.10 p.171 | Alsó sáv: `Edit Start`, `Edit End`, `Transpose`, `Nudge`, `Don't Snap` | `ParameterViewControl` + `AppGrid::applyBottomBarEditStep` | Régen csak hardveres Data Diallal (`EN_KNOB_CENTER`) ment; hozzáadva érinthető **`<` / `>` léptető gombok** az alsó sávba | **JAVÍTVA** |
| 17 | v3.10 p.171 | `Shift + Select All`, `Shift + Duplicate`, `Cut / Copy / Paste` | `GridHeader`: `cut`/`copy`/`paste` létezik (`paste` csak Copy/Cut után látszik). `Select All` és `Duplicate` érintőgomb nincs | `Cut/Copy` egy gombpáron osztozik (`Paste`-ből nincs „Mégse”); `Select All` és `Duplicate` mobilon hiányzik | **RÉSZLEGES** → TODO |
| 18 | v3.10 p.172–175 | **Grid Edit Menu** (`Clear Events`, `Double Length with Events`, `Half Speed`, `Double Speed`, `Pitch Quantize`, `Humanize`, `Random Events`, `Split Events`) | `GridEditOptions` (`kick_AppGrid.cpp:350`) megvolt, de **nem volt hozzá nyitógomb** | Hozzáadva az `editOptions` gomb a `GridHeader` jobb szélére | **JAVÍTVA** |
| 19 | v3.10 p.175 | Grid Edit Menu: `Convert to Progression` | Nincs megvalósítva | Nincs | **HIÁNYZIK** → TODO (`Akai-paritás` / döntés) |
| 20 | v3.10 p.176 | **Velocity / Automation Lane** (`^` nyit/zár, Velocity/Prob/Ratchet csíkok szerkesztése) | `ParameterView` + `ParameterViewControl` | `parameterViewport` drag-görgetése lekapcsolva (`ScrollOnDragMode::never`), így 1 ujjas húzás valóban rajzol | **JAVÍTVA** |
| 21 | v3.10 p.176 | Automation Lane: Track CC / CLAP paraméter-automatizációs görbék (`Add New`, törlő kuka / `Clear Auto`) | Csak `Velocity`, `Probability`, `Ratchet` szerepel `paramList`-ben (`kick_ParameterView.h:38`) | Track/plugin automatizációs görbék még nincsenek bekötve a Grid alsó sávjába | **HIÁNYZIK** → TODO (`Step/Track Automation`) |

---

## 2. Xfer Serum 2 – Clip / Arp Grid & Piano Roll összevetés

| # | Serum 2 funkció | Kick állapot | Mobilos állapot / teendő | Státusz |
|---|---|---|---|---|
| 1 | Hangjegy rajzolás, áthelyezés, szélhúzás, dupla kattintás/koppintás törlés | `GridPencilTool`, `GridEraseTool` | Érintőképernyőre javítva (`findNoteAtPosition`, `hitTestNoteEdge`, `yToNote`) | **JAVÍTVA** |
| 2 | Rácsfelbontások és triolák (`1`, `1/4`, `1/4T`, `1/8`, `1/8T`, `1/16`, `1/16T`, `1/32`, `1/32T`, `1/64`) | `timeDivListBox` + `TimeDivisionStrings` | Működik | **OK** |
| 3 | Egyedi tupletek (pl. 5-ös / 7-es felosztás) | Csak standard bináris és triola osztások | Nincs | **HIÁNYZIK** → TODO |
| 4 | Per-note `Velocity`, `Chance (Probability)` és `Ratchet` | `MTNote` + `ParameterView` + mostantól a **hosszú nyomásos Note Properties** ablak is | Teljesen szerkeszthető érintőképernyőn | **JAVÍTVA** |
| 5 | **Key / Scale Lock & Scale Highlighting** a Piano Roll háttérben | Csak utólagos `Pitch Quantize` van (`GridEditOptions`); élő skála-kiemelés / skálához kötés a Gridben nincs (`Pad Perform` külön képernyőn tudja) | TODO: élő skálakiemelés + Scale Snap opció a Gridben | **HIÁNYZIK** → TODO |
| 6 | Clip bankok / gyors variációváltás, MIDI drag export | Sequence lista (`HomeListView`) kezeli a szekvenciákat | Nincs inline clip bank a Gridben | **NEM-KICK / TODO** |

---

## 3. Safian Kick saját kézikönyv + SynthyBoy (`USER_MANUAL.md`)

| Terület | Tapasztalt hiba | Javítás | Státusz |
|---|---|---|---|
| **SynthyBoy (CLAP Plugin UI)** (`kick_ClapPanel.h`) | `openClapInstrument` külön OS `DialogWindow::launchAsync()` ablakban nyitotta `960x540` méretben a `dawStage` (`1024x600`) skálázáson kívül → iPhone-on (`932x430` pt) kilógott jobbra/lefelé és nem volt teljes képernyős | Átállítva `mt::utils::showDialogOwned(..., ui::layout::screen, false)`-ra és a modal Q-Link ellenőrzés javítva (`isVisible()` + modal szülővizsgálat), így minden más programhoz hasonlóan skálázva, teljes `1024x600` képernyőn nyílik | **JAVÍTVA** |
| **Grid Szekvenszer** (`USER_MANUAL.md` §2) | Valós idejű felvétel, Q-Link szerkesztés, kvantálás | Mobilon a `<`/`>` alsó léptetők, az `editOptions` gomb, a `ParameterView` húzás és a hosszú nyomásos Note Properties teszik teljessé a hardver nélküli szerkesztést | **JAVÍTVA** |
