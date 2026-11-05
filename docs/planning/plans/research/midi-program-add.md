# MIDI program / MIDI track hozzaadasa a Program tabon - kodfelmeres (csak olvasas, 2026-11-04)

Jeloles: NEM ELLENORIZHETO KODBOL = futtatas/hardver kellene hozza. Utvonalak a repo gyokerehez (Kick/...) viszonyitva.

## 1. Hozzaadas a UI-bol
- Gomb: a fo mod toolbar `tBar.add` (Kick/modules/kick_ui/dialogs/kick_AppMainMode.cpp:169). A viselkedes a fulre fugg:
  - Program ful -> "CREATE PROGRAM" dialogus (`AppSelectProgramMode`, :26), `ae.createProgram(selectedProgram)` (:187).
  - Track ful -> `ae.createTrack(seq)` (:175), program nelkul.
- Tipusvalaszto: 7 ikon: Drum, Keygroup, Clip, Audio, Midi ("Cord"), CV, Plugin (:32-38). MIDI tipusra megy (`MTProgramType::Midi=2`, Kick/modules/engine/interface/source/mt_audio_interface_types.h:97).
- Nev: ures nev -> `MTIProgram` ctor "Program NN" (NN = process-szintu globalis szamlalo, mt_audio_iprogram.cpp:38-42). Nincs tipus-specifikus nev ("MIDI 1"). Nevegyediseget az ures-nevu ag nem ellenorzi (mt_audio_manager_program.cpp:123-135 csak nem ures nevre).
- Hova kerul: `programs` vektor vegere + `vTree.appendChild(VT, undoManager)` (mt_audio_manager_program.cpp:111-117). Program-szamkorlat nincs a kodban. Trackre: MAX_TRACKS=128 (mt_audio_interface_types.h:56; mt_audio_sequence.cpp:256), a track neve "Track N" egy statikus, soha vissza nem allitott szamlalobol (mt_audio_sequence.cpp:267,275).
- Program es track kapcsolata: a program hozzaadasa NEM hoz letre trackot. A track `programId`-t (IDs::program) tarol; hozzarendeles a Track tab valasztojaval (kick_AppTrackComponent.cpp:60-62 -> `setProgramToTrack`) vagy a Program tab Options "Duplicate Program To Track" gombjaval (kick_AppProgramOptions.cpp:80-88). Az uj track programja ures (program==nullptr), `midiIndexes` ilyenkor "MIDI trackkent" szamit (mt_audio_sequence.cpp:497-504); Kick "MIDI track" = minden nem-Audio programu track.
- Frissites: `updateProgramList()` -> `progListChanged` -> listenerek (AppProgram, ProgramMixer, KickLEDs, SendsMixer). AppProgram csak lathatoan frissul (`populate` :52, listener csak lathatosag alatt, kick_AppProgram.cpp). Az uj program nincs kijelolve, kiveve ha az elso (mt_audio_manager_program.cpp:136-137). Pad-szin/Grid: AppGrid `setViewType` Midi -> MidiView (kick_AppGrid.cpp:479); LED: Midi a Keygroup agon (kick_leds.cpp:403).
- Hibak: (a) `selectedProgram` kezdeti erteke Unknown; Apply valasztas nelkul csendben nem csinal semmit (createProgram Unknown -> nullptr, mt_audio_manager_program.cpp:98-120), a dialogus bezarul. (b) `deleteProgram`: `setSelectedProgram(--selectedProgram)` csak ha a torolt a kijelolt; kisebb indexu program torlesekor a kijeloles csendben a kovetkezo programra csuszik (:141-160).

## 2. MIDI program beallitasok (MTIMidiProgramData, mt_audio_interface_types.cpp:441-452)
| Beallitas | UI | Engine | Megjegyzes |
|---|---|---|---|
| Port (MIDI OUT) | AppProgramComponent.cpp:80, lista :232 | `sendMidiMessage(midiPort, ...)` mt_audio_midi_program.cpp:126,146 -> MTMidiDeviceManager::sendToOutput(nev) midi_device_manager.cpp:339 | nev alapu, alapertek "" -> a lista elso eleme "None" (nincs kimenet) |
| Csatorna | `midiCh` lista 1..16, indexbol `Value+1` (:107-111), visszatoltes `-1` (:~155) | `it.setChannel(midiCh)` (mt_audio_midi_program.cpp:125,145); tarolt tartomany 1..16 (Constants.h:36) | +1/-1 konzisztens; a track `midiOutChannel` felul van irva |
| Program Change | `progCH` lista "Off",1..127 (kick_AppProgramComponent.h:57), ertek=index | `ProgCH` property valtozasra PC kuldes (mt_audio_midi_program.cpp:73-80) | "Off"(0) is PC 0-t kuld; PC 128 nem erheto el |
| Bank MSB/LSB | ugyanilyen listak | property valtozasra CC0+CC32+PC (:51-71) | ugyanaz az Off/127 limit |
| Note-tartomany, transzpozicio | NINCS program szinten | track szinten van transzpozicio (`getEffectiveNoteNo`, mt_audio_track.cpp ~815) | Note/Velocity Range, MIDI filter: HIANYZIK (WP-D D-165,D-176,D-178) |
| Velocity | nincs UI | program `level` skalazza a velocity-t (:114,134-136) | |
| Mod wheel | nincs program-UI | CC1 elo uton atmegy, csatorna felulirva (kick_hardware_bridge.h:1572; noteProcess) | a "WHEEL/VELOCITY/GATE/NOTE" mezok CSAK a CV programe |
| CC0-127 | MidiProgramEdit 128 slider (program_edit_ui/.../kick_MidiProgramEdit.cpp) es KnobEdit | `CCn` property valtozas -> CC kuldes (mt_audio_midi_program.cpp:39-52) | a CC csatorna a KIJELOLT track `midiOutChannel`-je (:46), nem a program `midiCh`-ja |
| Pan -> CC10 | mixer PAN | `IDs::pan` ag, `panToCC10` (:89-103) | csak property-valtozaskor; betoltes/port-valtas utan nem kuldi ujra (TODO.md:237 is jelzi) |
| Mute/solo | | muted: csak release-tipusu uzenetek (:114-127) | |
| CV/Gate | AppProgramComponent CV ag | MTCVProgram (mt_audio_cv_program.cpp) -> I2C | lasd 5.f; Key61-en a CV hw-iras szandekosan ki (HW_TEST_REPORT.md:260) |

Ures callback/STUB: a MIDI-programra nincs ures callback; a MidiProgramEdit `refresh()/handleCommand()/timerCallback()` ures (kick_MidiProgramEdit.cpp:~108-117), a selector `themeChanged` hibas (`id` sosem no, :91 -> minden ikon "0.svg").

## 3. Perzisztencia
- Egesz fa XML-be (kick_xml_manager.cpp:197 `ValueTree::fromXml`). PROGRAM csomopont property-k: type, name, id, MIDIPort, ProgCH, BankMSB, BankLSB, midiCh, CCn, gain, pan, mute, solo, level, OUT + SEND/CLIPS gyerek.
- Betoltes: `createProgram(vt,false)` (mt_audio_manager_program.cpp:55-62). Hianyzo `type` -> 0 (Drum); ismeretlen tipus -> `prog==nullptr`: a csomopont a fan marad, de futasidoben nem letezik (nincs naplo).
- Track -> program: id szerint, nev-fallback (mt_audio_manager_program.cpp getProgramById) -> regi nev-alapu projektek mukodnek.
- Undo/redo: property-szerkesztes tranzakcionkent undo-zhato (kick_CachedValue.h:131-136). Undo VISSZAJATSSZA a MIDI kuldest is (minden property-valtozas CC/PC-t kuld). STRUKTURA: a `MTAudioProgramManager` es `MTSequence` nem figyeli a ValueTree gyerek-hozzaadast/torlest (nincs valueTreeChildAdded/Removed a motorban, csak mt_audio_note.h-ban) -> a program/track hozzaadas/torles undo-ja a futasi `programs`/`tracks` vektorral szinkronon kivul kerul (a fa visszaall, az objektumok nem). Hozzaadas elott nincs `beginNewTransaction`, igy az elozo szerkesztessel egy tranzakcioba kerulhet. NEM ELLENORIZHETO KODBOL: a pontos felhasznaloi hatas (futtatas kell).

## 4. Hardver (MPC Key 61)
- Az adapter ALSA rawmidi portokat nyit kizarolagosan: Port 0 = keybed/Public (hw:0,0,0), Port 1 = Private vezerlo (hw:0,0,1), Port 2 = DIN (hw:0,0,2) (akai_mpc_key61_adapter.h:77,628-645; KEY61_HW_BEHAVIOUR.md:10). `m_out2` (DIN OUT) sehol nincs irva (csak nyitas/zaras).
- A MIDI program kimenet a JUCE MidiOutput (ALSA seq) listat hasznalja: `MTMidiDeviceManager` 1 Hz-en pollozza `MidiOutput::getAvailableDevices()` (midi_device_manager.cpp:~57,118-182), "None" + eszkoznevek; plusz 8 virtualis kimenet "Kick:Out:1..8" (:~44-55, Linux/Mac). USB hot-plug: a lista valtozasra ujraepul, megnyitja az uj eszkozt; a program a nevet tarolja, ujracsatlakozaskor ujra talal.
- NEM ELLENORIZHETO KODBOL: latja-e a JUCE a Key61 DIN/USB kimeneteit, ha az adapter a rawmidi alportokat kizarolagosan nyitva tartja (KEY61_HW_BEHAVIOUR.md:10: `amidi` "Device or resource busy"); a tenyleges port-nevlista a hardveren; a "Private/Public" portok megjelennek-e a listaban. TODO.md:131: "DIN MIDI ... JUCE viszi?" nyitott.
- Lejatszas: MTTrack::onTick -> `pProgram->noteProcess(midiMsgs)` (mt_audio_track.cpp:758), elo bemenet `noteProcess(list,true)` (:821). Felvetel: raw note a `recCap` pufferbe (mt_audio_track.cpp:~780), program-fuggetlen. Hardveres bizonyitek MIDI programra: nincs (HW_TEST_REPORT.md:22 "nem tesztelt"; csak a PAN->CC10 es a pedal/strip utak).

## 5. Konkret hibak (kodbol)
1. Szalbiztonsag: `midiOutputs`/`midiOutPorts` statikus kontenerek; az uzenet-szal 1 Hz-es Timer cserel (`midiDevices = newDeviceList`, `midiDeviceNames = newPortNames`, midi_device_manager.cpp:179,181), mikozben a tick-szal `sendToOutput(nev)` iteralja (:339-345). Nincs zar -> adatverseny, hot-plug alatt osszeomlas lehetseges.
2. Hianyzo/eltunt port a program UI-n: `updateMidiPorts` (kick_AppProgramComponent.cpp:232-246) csak ket esetben frissiti a listat (a mentett port benne van, vagy ures); ha a mentett port eltunt, a lista NEM frissul (regi opciok, uj eszkoz nem latszik), es nincs jelzes. A `setListOptions` szinkron meghivja az `onSelectionChanged`-et (kick_listbox.cpp:~156), es a 2.5 s-es timer minden korben ujraepiti a listat (`removeAllChildren`).
3. CC csatorna-eltérés: CC/knob a kijelolt track `midiOutChannel`-jen megy (mt_audio_midi_program.cpp:46), a note a program `midiCh`-jan -> eltero csatorna, ha a kijelolt track mas programot hasznal. KnobTabs a `getSelectedProgram()` faját irja (kick_KnobTabs.cpp:38-40), a tipus-valaszto viszont a track programjat (:~337).
4. `isNoteEffectivelyMuted(it.getNoteNumber())` minden uzenettipusra (mt_audio_midi_program.cpp:135): CC/aftertouch eseten a "note number" a 2. adatbajt (CC szam) -> CC64 stb. elnyelodhet, ha az az indexu pad nemitott.
5. KnobTabs CC-lista (kick_KnobTabs.cpp:~164): 128 elem, de a 22. index "CC21" (duplikalt cimke) es a 94-95. "Send4" ketszer; cimke-alapu `getIndex` miatt a CC95 elerhetetlen/felrevezeto.
6. `MTTrack::setMidiPort(const String& MidiPort)` onmagat irja at (`setValue(midiPort,...)`, mt_audio_track.cpp:116) - a parameter sosem hasznalodik (a UI kozvetlenul a property-t allitja, igy lappang).
7. MidiControl "output" lista a BEMENETI nevekbol (`getMidiDeviceNames(true)`, kick_MidiControl.cpp:115), a kuldes viszont kimenetre megy (kick_MidiControlTabs.cpp:1518).
8. `duplicateProgram` ("_copy", mt_audio_manager_program.cpp:173) nem egyedi: ketszeri duplikalasnal ket azonos nev; a `duplicateProgramToTrack` nev szerint rendel (`setProgramToTrack(String)`, mt_audio_interface.cpp:661) -> az elso masolatot kapja.
9. Programnev-utkozes: a szamlalo folyamat-szintu, betoltott "Program 05" utan az uj program ugyanezt a nevet kaphatja; nev-alapu keresesek (getProgramIndexByName) ketertelmuek.
10. CV program: a getterek alapertelmezese eltér a CachedValue-tol (CVPort 1 vs 0, CVVelocity 100 vs 0; mt_audio_cv_program.h:39-42 vs mt_audio_interface_types.cpp:432-437); az UI index 0 = "NA" de az engine csatornakent 0-t is hajt. Uj CV programnal a megjelenitett es a hasznalt port elterhet (hardver nelkul nem igazolhato).
- Tesztek: Kick/tests/platform/MidiPanTests.cpp (csak a tiszta `panToCC10` lekepezes), AppSmoke.h/py (createProgram/duplicate/mentes csak CLAP programra). MIDI program hozzaadasra, portlistara, csatornara, PC/bank-ra, hot-plugra, undo-ra NINCS teszt.

## 6. Osszehasonlitas az Akai MPC Standalone-nal
A kezikonyv NINCS a repoban (docs/akai_mpc_hardware/manuals ures); az osszevetes a walkthrough lapokra (WP-D, WP-G) tamaszkodik. "Track Type" kifejezes egyik dokumentumban sincs.
- Hianyzik a Kick-bol (walkthrough): track Output mezo (D-164), Key Range + Learn (D-165,166,176,177), Velocity Range (D-178), MIDI Filter/CC Filter (D-175,179), Monitor Off/In/Auto/Merge (D-039,169,170), MIDI Out Enabled/Note/Channel padonkent (D-065..067), Track Duplicate (D-159), Track Settings ablak (D-162), MIDI program mentese (G-184), Track as MIDI export (G-188), MIDI port atnevezes/reset/auto-enable (G-092..094), MIDI Clock/MTC/MMC fogadas-kuldes (G-096..100), Event/Audio mute megkulonboztetes (D-077).
- Megvan: track bemeneti port+csatorna (D-041,D-163), Send To routing (D-042), arm (D-167).
