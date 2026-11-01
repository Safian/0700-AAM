# Review 2. kör (opus), `4605b75e..195ffd30`

Csoportok (fájlok szerint, párhuzamosan javíthatók): A = engine track/note/manager (`mt_audio_track.*`, `mt_audio_note.*`, `mt_audio_manager.*`), B = program/transport (`mt_audio_iprogram.cpp`, `mt_audio_midi_program.cpp`, `mt_audio_manager_transport.cpp`), C = bridge (`kick_hardware_bridge.h`, `hw_targets.h`), D = Akai adapter/LED (`akai_mpc_key61_adapter.h`, `led_map.h`), E = AudIO (`AudIO/AudIO/Daw/DawInserts.*`, `AudIO/src/juce_adapter.cpp`).

| # | Súly | Csop. | Hely | Hiba és javítás |
|---|---|---|---|---|
| G1 | high | C | bridge `on_button_press/_release/on_jog_dial` (~640, 1121, 1227); az adapter az ALSA olvasó-szálról hívja | F3 hiányos: UNDO/REDO, ~25 `sigScreenClose`, BPM, bank, arp még a MIDI-szálon fut. **Javítás:** a három törzs átnevezése `handleButtonPress/handleButtonRelease/handleJogDial`; az override-ok csak `juce::MessageManager::callAsync([this,..]{ handle..(..); })` (FIFO, sorrend marad), `std::atomic<bool> m_stopping` a `stop()` elején, a lambdák elején `if (m_stopping) return;`. |
| G2 | high | A | `mt_audio_track.cpp:42` | A note-map listener `readerLock` nélkül nem zárja ki a tick-szálat (use-after-free undo közben). **Javítás:** az `MTTrack` konstruktorában `vTree.addListener(this);` után `MTNoteList::setReaderLock( &lock );`. |
| G3 | high | C | bridge ~330-331, `Main.cpp:247-250` | Kijelző nélkül láthatatlan ablak: `getComponentAt` nullptr, az érintés halott (a hardveres riport 2. hibája). **Javítás:** a találatkeresés a content komponensről induljon (`ResizableWindow::getContentComponent()`, `hitRoot->getComponentAt(hitRoot->getLocalPoint(nullptr, p))`). |
| G4 | med | D | `akai_mpc_key61_adapter.h:645`, `led_map.h` | A LED-ek nem kapcsolnak ki; valószínű ok: a firmware a LED-et nem `0x90`, hanem CC-vel (`0xB0 <note> <érték>`) vezérli. **Dummy-megoldás:** futásidejű kapcsoló `KICK_AKAI_LED_MODE=note|cc` (alap: `note`), `kAkaiLedStatus` konstans a `led_map.h`-ban, az `snd_rawmidi_write` hibájának egyszeri naplózása; a hardveres próba dönti el. |
| G5 | med | B+A | `mt_audio_iprogram.cpp:305,320`, `mt_audio_track.cpp:760` | A pedál a szekvenszer hangjait is tartja; track-mute elnyeli a felengedést. **Javítás:** (B) `if( !live \|\| sustain.onNoteOff(n) ) noteOff(n);`, `onNoteOn` csak `live`-ra; (A) mute mellett is átengedni a release-típusú üzeneteket (note-off, pitch wheel, CC64, CC123). |
| G6 | med | B | `mt_audio_midi_program.cpp:112-113` | Mute-olt MIDI programnál a bend-centre/CC64-felengedés/note-off nem jut a külső portra. **Javítás:** a korai `return` helyett ciklus, amely mute-nál csak a release-típusú üzeneteket küldi, méter nélkül. |
| G7 | med | C | bridge `:1069-1082` | `<`/`>` EN_MINUS/EN_PLUS-t küld; az Akai-konvenció a lejátszási pozíció léptetése a rács szerint. **Javítás:** `stepPlayhead(±1)` (callAsync, `seekToTick(from ± step)`, `step = gridTicks>0 ? gridTicks : 24`), a CURSOR_LEFT/RIGHT marad EN_MINUS/PLUS. |
| G8 | med | A+B | `mt_audio_track.cpp:782`, `mt_audio_iprogram.cpp:291,368`, `mt_audio_midi_program.cpp:119-121` | A méter minden üzenetre (bend, aftertouch, CC) felugrik. **Javítás:** `reloadMidiMeter` csak `isNoteOn()` esetén. |
| G9 | med | E | `AudIO/AudIO/Daw/DawInserts.cpp:20-30,55-66` | Az insertek `prepareToPlay` nélkül futnak (`blockSize()==1`): ez a `sampleFrames=0` lenyomata az `Aura::processReplacing`-ben. **Javítás:** `DawInserts` `_rate/_block` tagok, az `addEffect` a `create` után hív `prepareToPlay(_rate,_block)`-et. |
| G10 | med | A | `mt_audio_manager.cpp:277-280` | Read módban mentett projektbe az automatizált pan kerül. **Javítás:** a mentés idejére a `panBases` értékeit visszaírni, mentés után az aktuálisat visszaállítani. |
| G11 | low | A | `mt_audio_track.cpp:878-889` | TC/RECALL a teljes szerkesztés alatt tartja az `MTTrack::lock`-ot: tick-csúszás. **Döntés:** marad, dokumentálva a header kommentjében. |
| G12 | low | A | `mt_audio_track.cpp:537-544` | A `stop()` minden programra pitch-bend-centre-t küld. **Javítás:** `std::atomic<bool> liveBendActive`, a `stop()` csak akkor küld centre-t, ha a bend el volt mozdítva. |
| G13 | low | B | `mt_audio_manager_transport.cpp:274-281` | Song módon kívül minden ticken `onSongStopped`+`resetSong` (régi hiba). **Javítás:** `if( songActive.load() && lastSeqTicks <= playTime )`. |
| G14 | low | A | `mt_audio_track.cpp:433-434, 1311-1334` | `commitPanLane` fölösleges reloadot és `panRec` resetet okoz. **Javítás:** `inPanCommit` RAII jelző, a `reloadLaneFromState` elején kilép. |
| G15 | low | C | bridge `:349-376` | Peeres buildben a főablak-érintés duplán dispatchel. **Javítás:** peer esetén csak `peer->handleMouseEvent`, a kézi út csak peer nélkül. |
| G16 | low | C | bridge `:1272-1284, 1341-1355` | A bend-célpont csak pontosan 8192-nél zár; strip és kerék közös célpont; CC1/CC11 a pillanatnyi trackre megy. **Javítás:** `abs(val-8192)<=2`, külön `HeldTarget` a stripnek, CC1/CC11 `HeldTarget` (begin: val>0, end: val==0). |
| G17 | low | E | `AudIO/src/juce_adapter.cpp:65-68` | `printf`+`fflush` az audio callbackben. **Javítás:** csúcsérték atomic, kiírás a message thread timerjében, vagy `#if KICK_AUDIO_DEBUG`. |

## Az első kör javításainak állapota
F1, F5, F6, F7, F8, F9, F10, F11, F13, F14, F15, F17 ok (F5, F16 kisebb maradvánnyal). **Hiányos:** F2 (G2), F3 (G1), F4 (G5, G6), F12 (G15). A 2. kör új elemei (song-seek tiltás, RECALL időillesztés, live-only bend) ok; a live-only sustain hiányos (G5).

## Hardveres megfigyelések magyarázata
- **SIGSEGV `Aura::processReplacing(sampleFrames=0)`:** a `sampleFrames=0` a `while(--sampleFrames>=0)` ciklusszámlálója, nem argumentum: `blockSize()==1`-gyel futó insert lenyomata (G9, régi AudIO-hiba). A backtrace megbízhatatlan (eltérő binárisból feloldott LTO-s core); a gyökérok nincs megállapítva: javítani G9-et, a következő crash-t a pontos binárissal és `-funwind-tables`-szel elfogni.
- **LED-ek:** G4. **Pad-ismétlés:** a 4605b75e buildben a mainmode bekapcsolta az aftertouch-ot és a csomagot új note-onnak vették; HEAD-en `kSendPadAftertouchPackets=false`, a funkció most ki van kapcsolva. **`<`/`>`:** G7. **Méter-ugrás:** G8 (a méter, nem a hangerő).

## Tervezési kérdések: döntések (az elv szerint, a felhasználó alszik)
1. A teljes gombkezelés a message threadre megy (G1).
2. `<`/`>` lépése a TC rács (`gridTicks`), 0 esetén 1/16 (24 tick); a Shift+`<`/`>` egyelőre változatlan (reggeli teendő).
3. A méter csak note-onra reagál, külső MIDI CC-re is (hibajavítás).
4. A pad-aftertouch kikapcsolva marad; nyomás-only DCL-üzenet reggeli teendő (dummy: a flag marad).
5. STOP elengedi a pedál alatt tartott élő hangokat (`resetPerformance` marad); a bend-centre csak `liveBendActive` esetén megy (G12).
6. Kijelző nélküli dialógusok/popupok: fizikai próba kell, kódváltozás nincs.
7. Az arp hangjaira a pedál nem hat: az arp tick-kimenete és a flush is `live=false` (G5 része).
8. A TC/RECALL alatti tick-csúszás elfogadott, dokumentált (G11).
