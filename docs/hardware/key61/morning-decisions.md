# Reggeli átnézés: éjszakai döntések és dummy elemek

A felhasználó utasítása: ne álljunk meg apró döntés miatt; ha UI-elem kell, **dummyt** teszünk be (működő hely, látható jelölés, minimális viselkedés), reggel véglegesítjük. Minden worker és az orchestrator ide jegyzi (sorokat hozzáfűzve):

| Dátum | Terület | Döntés / dummy | Indok | Hol (fájl) | Reggeli teendő |
|---|---|---|---|---|---|
| r5 J1 | Mentes / pan (G10) | FELULIRVA: a mentes az elo fat nem modositja; `KickXML::patchSnapshot` a szerializalt masolatba irja a `panBases` base pant (`MTAudioManager`); read-only helyen Autosave/Close a `prepareSave` elott kilep | Az elo fa visszairasa hallhato pan-ugrast okozott Read modban autosave-nel | `kick_xml_manager.h/.cpp`, `mt_audio_manager.cpp` | Jovahagyas; ismert: `relativizeSamplePaths` meg az elo fan fut (kulon vizsgalat) |
| r2 A | Nemitott track (G5) | Mute mellett note-off, pitch wheel, CC64, CC123 atmegy (`mt_mute_filter.h`) | Ne ragadjon hang/pedal mute kozben | `mt_audio_track.cpp`, `mt_mute_filter.h` | Jovahagyas |

## Már meghozott döntések (round 1)
- Sustain és pitch bend csak az élő bemenetre hat (Behringer-elv): megvalósítva (`noteProcess(msgs, live)`).
- RECALL: tick vagy wall-clock eseményenként; megállítva rögzített hosszú felvétel a track hosszára hajtogatva.
- Song módban a seek tiltott.
- Globális TC kapcsoló csak az Apply-ra érvényesül.
- `KICK_TOUCH_SINGLE_TAP_LISTBOX` CMake opció (alap: ki); az MPC-s build ezt BE kell kapcsolja.

| 2026-11-01 | Akai LED protokoll (G4) | DUMMY: futásidejű kapcsoló `KICK_AKAI_LED_MODE=note\|cc\|noteoff0` (alap: note = mai viselkedés); az első snd_rawmidi_write hiba egyszer logolva | Gombok LED-je bekapcsolva marad, bár `0x90 <note> 0x00` megy ki; a valós protokollt csak az MPC-n lehet eldönteni | `Kick/apps/kick/hardware/led_map.h`, `adapters/akai_mpc_key61_adapter.h` (`start()`) | MPC-n: `KICK_AKAI_LED_MODE=cc ./Kick`, majd `noteoff0`; melyiknél alszik el a STOP/PLAY LED indításkor; a nyertes legyen az alap, a kapcsoló törölhető |
| 2026-11-01 | Pan CC10 mentés közben (R2 follow-up 1) | ELAVULT (J1): a `isSavingProject()` jelző és a CC10-tiltás törölve, mert a mentés már nem ír az élő fába | nincs visszaírás, ezért nincs mit elnyomni | - | - |
| 2026-11-01 | `isEffectivelyMute` figyelmeztetés (R2 follow-up 2) | A változó törölve a `MTTrack::onTick` régiójában | Nem használta semmi; a némítás-szűrés (`mutefilter::passesWhileMuted`) élő bemenetnél (mt_audio_track.cpp ~783) és az arp-nál már `isTrackEffectivelyMuted()`-ot hív | `Kick/modules/engine/audio_engine/engine/mt_audio_track.cpp` | L |
| 2026-11-01 | Bridge lebegő `this` a callAsync-ben (R2 follow-up 3) | `std::shared_ptr<std::atomic<bool>> m_alive` (stop() hamisra állítja), a lambdák másolatban fogják; az érintés-lambda is ellenőriz | A `m_stopping` a megsemmisülő objektumban él, a késői lambda felszabadított memóriát olvasna; a megosztott flag túléli a hidat | `Kick/apps/kick/kick_hardware_bridge.h` | L |

## Felhasználói döntések (2026-11-01, ébren)
| Téma | Döntés | Teendő |
|---|---|---|
| Tartósan hiányzó minta | Egyszeri, nem blokkoló figyelmeztetés a UI-ban (nem csak napló, nem blokkoló hiba). A napló marad. | DUMMY UI-elem + callback a KickXML-ben (mintánként egyszer); reggeli finomítás |
| Shift+`<` / Shift+`>` | Ütem-ugrás (Akai-konvenció); a sima `<`/`>` marad a rácsos léptetés | bridge: `seekToBar(±1)` Shift mellett |
| Review-fix ciklus | Amíg a kör pontosan „Nincs megerősített hiba”-t mond | folyamatban (mentési sample-ág: K3…Q2) |
| Gomb-LED | A felhasználó javítása: csak STOP világított, a többi nem (az előző „mind világít” megfigyelés téves volt). A kamerás ellenőrzést a felhasználó végzi. | LEZÁRVA: `cc` (`0xB0 <note> <érték>`) az alap, a `KICK_AKAI_LED_MODE` kapcsoló és a `note`/`noteoff0` ág törölve; mérés: `docs/hardware/key61/led-colours.md` |

## Egyéb döntések a mentési sample-ágon (autonóm)
- Hiányzó forrás + hiányzó másolat: a mentés nem blokkol (lecsatolt meghajtó ne akadályozza az autosave-et/bezárást), a mentett út megmarad; naplósor mentésenként (szerkesztés után, 1,5 s debounce).
- PLAY/STOP az üzenetszálon marad (eszközön mérendő); a STOP alatti átmeneti Paused állapot elfogadott.
- Pitch wheel némított MIDI programon minden értéket átenged; némított sample programnál nincs release-átengedés; az autosave gyakorisága Read módban változatlan.

## Review-fix ciklus összegzés (3–16. kör, `195ffd30..62226301`)
- A 16. opus kör pontosan „Nincs megerősített hiba”-t mondott a `195ffd30..62226301` tartományra (vége a ciklusnak; az U1 érintés-javítás után egy új, rövid kör jön az új kódra).
- Mentési sample-ág (K3…S1), végső elv: **a mentés az élő fát nem módosítja**; a samples/ alá másolás és a relatív utak csak a mentési snapshotban (`KickXML::patchSnapshot` a pan-nél, `relativizeSamplePaths` a snapshoton). Ismert korlát: ha egy külső forrás tartalma két mentés között változik, a már rögzített másolat nem frissül.
- Tartósan hiányzó minta: a mentés nem blokkol, a mentett út megmarad, naplósor; felhasználói jelzés (egyszeri, nem blokkoló) még DUMMY/teendő.
- Hardveres visszajelzés: tartalék Desktop-display (SOUNDS összeomlás) javítva és eszközön igazolt; LED `cc` alap; érintés vizsgálat folyamatban (U1).

## Hardveres session 6–7. kör (autonóm döntések)
| Téma | Döntés | Teendő |
|---|---|---|
| `kick_cmd touch X Y` | framebuffer-koordináta (1280x800, a snapshot ppm tere); nyers érintés: `rawToUi` | a régi UI-koordinátás próbák átszámítandók |
| TC modális ablak | hardveres zárás: STOP/ENTER = Apply, SHIFT+STOP/Back = Cancel (V1 worker); érintés külön (U1) | hardveres próba |
| Két Shift LED | note 49 és 50 együtt (V1) | hardveres próba |
| KEYBOARD CONTROL → Pad Perform | marad (nincs Keyboard Control menü a Kickben); eltérés dokumentált | felhasználói döntés ha kell |
| Main Volume gomb | DUMMY: nincs kodek-mixer (`amixer`) vezérlés, amíg a nyers MIDI-mérés nem mutatja, hogy a gomb küld-e eseményt; a gyári MCU/kodek útja `acvs` nélkül hatástalan | felhasználói döntés: kell-e kodek-mixer vezérlés |

## Felhasználói döntések, 2026-11-02 (ébren)
| Téma | Döntés |
|---|---|
| TC ablak hardveres zárása (STOP/ENTER = Apply, Shift+STOP/MAIN = Cancel) | MARAD, mint Kick-specifikus, kijelző nélküli használathoz szükséges kiegészítés (az Akai útmutató csak érintéses zárást ír). Nem Akai-funkcióként dokumentálandó. A hardveres zárás csak akkor hat, ha a TC ablaka a legfelső modális. |
| Jobb Shift LED tükrözése (note 49 is) | MARAD, a hardveres teszt dönt; a helyi agent mérje meg a firmware viselkedését (bal LED mindig világít, jobb csak Shift-nyomásra). |
| Review-ciklus kereten belül | Az új funkciókra (Y2/Z1/Y4) a ciklus tovább megy; új funkciót a listán kívül nem veszek fel, csak hibajavítást. |

## R25 / M1: upstream PR-agak beolvasztasa a safian/main-be
Minden beolvasztott ag tartalma mar (cherry-pick / ujraimplementalas formajaban) a safian/main-ben volt, ezert a merge-ek a fa tartalmat nem valtoztattak; a commit-ancestry rogzitve van. Konfliktusnal a safian/main allapota nyert.
| Ag | Hash | Mit hozott | Konfliktus-feloldas |
|---|---|---|---|
| fix/cv-output-inversion | cf430255 | CV velocity/mod-wheel kimenet fordított skálázás javítása | `kick_I2c.cpp` konfliktus: a safian/main `stepToDac` (kick_cv_scale.h, hostteszttel) megoldása megtartva; az ág inline `step*4095/127` + `setVoltageOff` változata elvetve |
| fix/drumvoice-dsp-sustain-pitch | 1bb75ab5 | drum_voice DSP sustain/pitch javítás | konfliktus nélkül; nettó diff nulla (már benne volt) |
| fix/program-resolution-by-name | 4821a3a8 | program feloldás név alapján (`mt_audio_manager_program.cpp`) | konfliktus nélkül; nettó diff nulla |
| fix/retain-virtual-midi-ports | 4e517853 | virtuális MIDI kimenetek megtartása (`virtualOutputs`) | `midi_device_manager.cpp/.h`: safian/main megtartva (hiányzó backend esetén `break`, `&& !JUCE_IOS` védelem; az ág `JUCE_IOS`-t is engedett volna, ez ellentmond a platformszabálynak) |
| feat/keyboard-qwertz-layout | b9c87f4a | QWERTZ 4x4 pad rács, `KICK_KEYBOARD_LAYOUT`, mac layout-detektálás | `keyboard_detector.cpp`, `tests/platform/CMakeLists.txt`: safian/main megtartva (azonos funkció, Linux-barát tesztcél `if(APPLE)` Carbon-nal; az ág feltétel nélkül `-framework Carbon`-t linkelt volna, ami Linuxon törik) |
| fix/project-relative-samples | cf4647c8 | mentéskor sample-útvonal relativizálás + import relatív útvonal | KIHAGYVA (nem olvasztva be). A mentési rész (`kick_xml_manager.cpp` `relativizeSamplePaths`) az ÉLŐ fát írja át, ez ellentmond a safian/main snapshot-alapú elvének (`relativizeSamplePaths`/`chooseDest`/`collectLiveRefs`/`sessionCopies`, az élő fa mentéskor érintetlen) – ott másképp megoldott. Az import rész (`mt_audio_asset_manager.cpp`) relatív útvonalat ír az élő asset-fába és második `sliceMap`-kulcsot (`addToAssetMgr(pathForTree)`) vesz fel, így az élő útvonal nem marad az asset kulcsával egyező abszolút; ez a safian/main elvével nem összeegyeztethető (kettős kulcs, slice-ok szétcsúszása), a feloldás (`getChildFile`) a safian/main-ben már megvan. Upstream PR ezen részét a safian/main másképp oldja meg. |

## r28 AC1: Szekvenciafelvétel (REC + PLAY) – kutatás, megvalósítás, dummyk
**Kutatás (Behringer-módszer az első).** Az eredeti Kick-kódban (`git log -S setRecordState` és `-S RecordState`: egyetlen commit, az "Initial public release"; az `origin/main` azonos) NINCS élő felvétel-útvonal: a `MTISequenceData::rState` / `MTSequence::setRecordState` / `isArmed` / `isRecording` (+ `MTAudioTransportManager::isArmed/isRecording`) csak deklarált, hívó nincs; a `MTITrackData::record` (sima bool) olvasatlan; a `KickRecordButton` (sáv "Arm" gomb) csak képi állapotváltó (3 kép, nem kötött a motorhoz); az `EN_RECORD`/`EN_OVERDUB` egyetlen fogyasztója a Looper (`kick_LoopRecorder.cpp`). A hardveres Kick felvétele (Behringer firmware) nincs a nyílt kódban, ezért a legkisebb saját utat építettük, a meglévő RECALL-lal azonos elven: élő note-on/off (`addLiveMidiMsg` -> `MTTrack::processIncomingMidiMessage`) -> az élesített sáv note-listája (`addOrModifyNote`), a lejátszási tick szerint. Második forrás az Akai (MPC v3.10) leírás: REC élesít, PLAY/PLAY START indít, a sima REC felvétel törli az élesített sáv eseményeit, az OVERDUB megtartja.

**Megvalósítás.** `mt_seq_record.h` (JUCE-mentes állapotgép + felvétel-puffer + párosítás, hosztteszt `SeqRecordTests`), `MTTrack` (rec arm/capture/flush), `MTAudioManager` (take-kezelés, 25 Hz poll), `MTAudioInterface` (API), `KickRecordButton` -> sáv Rec Arm, bridge REC/OVERDUB + LED. Szálmodell: a bemeneti szál csak a sáv saját, előre foglalt `Capture` pufferébe ír (spin flag, O(1), nincs allokáció, nincs `MTTrack::lock`); a párosítás/commit az üzenetszálon fut, `MTTrack::lock`-ot tartva (a RECALL/TC-vel azonos zárrend: MTTrack::lock -> MTNoteList::lock); a megosztott pufferben a párosítás ugyanazon spin flag alatt fut, hogy ne legyen "kivett és visszatett" ablak (SustainState-tanulság). A hosztteszt TSAN alatt is fut.

| Terület | Döntés / DUMMY | Indok | Hol | Reggeli teendő |
|---|---|---|---|---|
| REC fogyasztó | A Looper képernyőn (`Screen::MenuLooper`) vagy ha a looper épp felvesz: a Looper kapja az EN_RECORD/EN_OVERDUB-ot (változatlan); minden más képernyőn a SZEKVENCIA (motor-hívás, nincs DCL csomag) | Az eredeti Kick képernyőnként irányítja a gombokat; a looper az egyetlen eredeti fogyasztó | `kick_hardware_bridge.h` `recOverdubGoesToLooper` | Jóváhagyás; a hardver-fiók / F9 / F10 / szimulátor továbbra is csak a looper |
| SEQ REC ARM | DUMMY: nincs külön "SEQ REC ARM" kapcsoló; a hardveres REC élesíti a kijelölt szekvenciát | Nincs Kick-megfelelő | `MTAudioManager::seqRecPressed` | Kell-e UI kapcsoló |
| Rec Arm Behavior (Single/Multi) | DUMMY: Multi (több sáv is élesíthető) | Preferences nincs a Kickben | `MTTrack::setRecArmed` | Preferences opció kell-e |
| Sáv Rec Arm gomb | A `KickRecordButton` 2 állapotú lett (nem élesített / élesített = piros pont); a harmadik "stop" kép állapota elhagyva | Az eredeti 3 állapotnak nem volt jelentése | `kick_buttons.cpp` | Jóváhagyás |
| Élesített sáv nélkül | A take elindul, de nem rögzít semmit (nincs implicit "kijelölt sáv") | Az útmutató a sáv Rec Arm-ot követeli | `seqRecBeginTake` | Kell-e implicit kijelölt sáv |
| REC törlés | A sima REC take a lejátszás indításakor (PLAY) törli az ÉLESÍTETT sávok eseményeit (nem a teljes szekvenciáét); OVERDUB megtartja | "REC törli" + a Rec Arm az élesített sávra vonatkozik | `seqRecBeginTake` | Jóváhagyás |
| "Egy kör után Overdub" | A törlés csak a take elején történik, utána minden kör hozzáad (gyakorlatilag overdub); a felvett hangok ~40 ms-en belül bekerülnek a sávba, így a következő körben hallhatók | Az útmutató mondatának felhasználói hatása | `pollSequenceRecording` | - |
| REC gomb felvétel közben | DUMMY: kilépteti a felvételt (punch out), a lejátszás megy tovább; REC lejátszás közben csak élesít (a PLAY indít, nincs azonnali punch-in) | Az útmutató nem írja | `seqrec::onRecPressed` | Akai-e a punch-in |
| OVERDUB gomb | DUMMY: kapcsolja az overdub módot; ha nincs élesítés, el is élesít; a take végén (STOP) az overdub mód is kikapcsol (egyszeri) | A LED így mindig igazat mutat; az útmutató nem írja | `seqrec::onOverdubPressed` | Marad-e a mód STOP után |
| STOP / pause | A take véget ér (STOP, pause, nem-loopolt szekvencia vége); az élesítés megszűnik (REC újra kell). Pause-ból folytatva nem vesz fel újra | Egyszerű, kiszámítható | `pollSequenceRecording` (2 poll, ~80 ms késleltetés) | - |
| Count-in | A metronóm count-in beállításnak csak "Off" eleme van (`kick_modeHeader.cpp`), ezért nincs count-in; ha lesz, a take indításának (`seqRecBeforePlay`) a helye | Nincs motor-count-in | `seqRecBeforePlay` | Count-in megépítése kell-e |
| Nem azonnali PLAY | A "next bar" sorba állított indítás (`Now=false`) nem rögzít | Nincs bar-szinkron take-indító | `MTAudioInterface::setSequencePlay` | - |
| Quantize / TC | Ha a globális Timing Correct be van kapcsolva, a take által hozzáadott jegyzetekre az elmentett TC paraméterek érvényesek (mint RECALL-nál); egyébként nyers (tick-pontos) | A meglévő TC logika újrahasznosítva; nincs külön felvétel-quantize beállítás | `MTAudioInterface` konstruktor, `recordedNotesHook` | - |
| Hosszan tartott hang | Egy körnél tovább tartott hang egy körre vágva kerül be | Nincs wall-clock a pufferben | `seqrec::spanTicks` | - |
| Undo | A take elején `beginNewTransaction("Record")`; egy undo lépés, amíg közben más nem nyit tranzakciót | Meglévő undo mechanizmus | `seqRecBeginTake` | - |
| LED (felhasználói döntés) | REC élesítve = piros folyamatos, felvétel közben villog (piros); OVERDUB aktív = zöld; a looper felvétele elsőbbséget élvez. A piros=B, zöld=A színkódot az AUTOMATION LED bizonyítja, REC/OVERDUB-ra nem ellenőrzött | Felhasználói döntés 2026-11-02 | `led_map.h`, `updateRecOverdubLeds` | Kamerás ellenőrzés |

## Felhasználói döntés, 2026-11-02 (scope: Kick-funkciók előbb, Akai-specifikus a végére)
- A Kick funkciókat fixáljuk ki ELŐSZÖR. Az Akai-specifikus részek (Locate ablak, Sounds Mode, Keyboard Control menü stb.) a végére maradnak, az Akai ágon/rétegben, end-to-end megírva; hogy pontosan hogyan, még nincs kitalálva.
- A régi Proper-skin dolgok NEM kerülnek be ebbe az ágba; az Akai-dolgok igen, de az Akai-rétegbe.
- A helyi agent jegyzete (NEM-KICK scope "elfogadva") a felhasználó szavai szerint pontatlan volt; ez a bejegyzés az érvényes.
- Következmény: Locate ablak / Sounds Mode / Keyboard Control menü most NEM épül; Y4 (strip módok) a Kickben létező módokra a leírás megérkezése után.

## AD1a – Main Volume gomb (2026-11-02, nyers MIDI mérés alapján)
| Elem | Állapot | Indok | Hol | Reggeli teendő |
|---|---|---|---|---|
| Bejövő SysEx értelmezése | KÉSZ: `MidiParser` összeállítja a teljes F0..F7 üzenetet (darabolt olvasás, 32 bájt korlát, a hosszabb egészében eldobva, F7 nélküli üzenetet státuszbájt/új F0 megszakít); csak Port 1-en értelmezett | Mért: `F0 47 7F 4B 5B 00 01 <v> F7` | `hardware/midi_parser.h`, `hardware/main_volume.h` | - |
| Parancsbájt `0x5B` jelentése | DUMMY: ismeretlen; csak az üzenet felismerésére szolgál. Más Akai SysEx (más parancs/hossz/eszköz/termék-bájt, vagy bármely Akai SysEx Port 0/2-n) NEM dobódik el csendben: az első egyszer naplózva (`[AKAI-SYSEX] ...`), a hex tartalommal | Nincs dokumentáció | `main_volume.h` (`kCommand`), `akai_mpc_key61_adapter.h` (`handle_sysex`) | Ha a parancs jelentése kiderül (pl. a jövőbeli firmware több parancsot küld), bővíteni |
| `<v>` tartomány és skála | `0x00..0x75` mért; `0x76..0x7F` nem látott, `0x75`-re vágva; skála: `v*127/0x75` kerekítve, 0..127 (a Mixer Master csúszka skálája) | A 0..0x75 mért adat; a hardver a 0x75-nél telít | `main_volume.h` (`toMidi127`) | Hardveren ellenőrizni, hogy tényleg 0x75 a felső határ |
| Mire köt | A Kick master szintje: kimeneti busz 0 `gain = CommonUtils::midiToGain(0..127)` (ugyanaz, amit a Mixer "Master" csúszka tesz, `kick_master_route_mix.h`); üzenetszálon `callAsync`+`alive` | A Kickben nincs külön "fő hangerő" API; az eredeti Kick fő hangereje analóg hardveres volt (a kódból nem látszik szoftveres párja) | `kick_hardware_bridge.h` (`on_main_volume`/`handleMainVolume`) | Döntés: (1) mentődjön-e a projekttel (most igen, mint a Mixer csúszka), (2) legyen-e a busz kiválasztható (most mindig az 1. kimeneti busz), (3) a Mixer UI nem kap értesítést, ha a Master képernyő épp nem látható (megnyitáskor frissül) |
| Hardveres hangerő | NEM nyúlunk hozzá: a gomb a hardver firmware-ében a kimeneti/fejhallgató szintet is állíthatja (a felhasználó szerint a Kick alatt nem változott a hangerő); hogy a SysEx mellett a firmware maga is állít-e, nem ismert | Nincs doksi | - | Hardveren: a gomb forgatása alatt hallható-e kettős hatás |

## AD1b – Data Dial (2026-11-02): a Kick-oldali út végigkövetve, kódhiba nem található
Út: Port 1 `B0 64 01|7F` (ch1) -> `akai_mpc_key61_adapter.h` (`handle_midi_message`, CC100) -> `on_jog_dial(delta)` -> `kick_hardware_bridge.h` `on_jog_dial` (`callAsync` + `m_alive`) -> `handleJogDial` -> `DclSignals::dispatchHardwareInput` (`EN_PKT_TYPE_KNOBS`, `EN_KNOB_ROTARY`, `EN_KNOB_CENTER`, irány, `abs(delta)`) -> `inputOwners` (modális fogyasztók) -> `hardwareInputTriggered` -> a képernyők `handleCommand`-ja.
| Kérdés | Eredmény |
|---|---|
| (a) Fogyasztók | `EN_KNOB_CENTER` forgatást ~25 fájl kezel: Track (`kick_AppTrack.cpp:94-97` -> `IRowView::knobRotated`), Sequence (`kick_AppSequence.cpp:123-125`), Grid (`kick_AppGrid.cpp:569`), Step Sequencer, Song, Next Sequence, Track/Pad Mute, Pad Mixer, Channel Mixer (`kick_ui_channel_mixer.cpp:216`), Program (`kick_AppProgram.cpp:60`), List Edit, File Browser, Sampler, Sample Editor, XY FX, Looper, Recent Project, Assign Sample, Midi Control. A Data Dial az eredeti Kickben a kurzort/listát/értéket vezérli (a fókuszban lévő sor/érték `knobRotated`). A Home képernyő (`kick_AppHome.cpp:515`) a CENTER forgatást nem adja a MenuKnobEdit-nek (csak a nem-CENTER gombokat, ha a `knobControl` mód 0), a többi ágra (fókuszált komponens `handleCommand`, aktuális képernyő `requiresHandleCommand`) esik. Olyan képernyő, amelyik a CENTER-t kellene kezelje és nem kezeli, a kódból nem állapítható meg; UI-t nem találtam ki. |
| (b) Relatív CC | Helyes: `0x01` = +1, `0x7F` = -1 (7 bites kettes komplemens), egy üzenetben több lépés (`abs(delta)`, gyors forgatásnál 0x02, 0x03...) átmegy a darabszámban; az irány a delta előjele. Javítva: `0x00` (nincs lépés) korábban nulla darabszámú forgatásként ment tovább (`delta>0` hamis -> ANTICLOCKWISE, 0 darab), amit a UI-ban pl. a Q-Link oldalak mute/solo kapcsolója félreértelmezne (lásd a híd T11 megjegyzését); mostantól a Data Dial és a Q-Link is eldobja. Függvény: `hardware/relative_cc.h`, host-teszt `RelativeCcTests`. Nincs gyorsítás-leképezés (a Kick nem kér ilyet; a darabszám jön a hardverből). |
| (c) Nyomás | Helyes: Note 111 -> `HW_BTN_ENTER` (`akai_map.h:78`) -> `EN_KNOB_ENTER` gomb press/release (`kick_hardware_bridge.h` ~1152/1373) -> a vezérlők `knobClicked()`-je (`kick_ControlBase.h:63`), File Browser, Recent Project, Assign Sample. Ez ugyanaz az üzenet, mint a Hardware Drawer ENTER-e (`kick_HardwareDrawer.cpp:383`). NEM `EN_KNOB_CENTER` (az forgatás-azonosító). |
| (d) Elnyelő feltétel | A forgatás útján nincs: a híd `handleJogDial`-ja nem ellenőriz képernyőt/modális jelzőt/Shift-et; az `m_modal_consumed` csak a gomb-press/release párosítására van (TC ablak). Egyetlen eltérés: amíg a TAP TEMPO gomb FIZIKAILAG lenyomva (`m_tap_tempo_held`), a tárcsa a BPM-et állítja a UI helyett (szándékos, T20); a release mindig törli. A TC ablak (`ListEditTimingCorrect`) nyitva alatt a forgatás a `dispatchHardwareInput` útján megy (nem a gombfeldolgozón). |
| Következtetés | Az injektált `jog` ugyanazt a `handleJogDial`-t hívja (mért: Track képernyőn a kijelölés lépett). A fizikai ok a Kick-kódból NEM állapítható meg. Nyitott (hardveres mérés kell): fizikai tekerés futó appon a Track képernyőn + snapshot; ha ott sem lép, naplózni kell, hogy a `B0 64 xx` megérkezik-e a `handle_midi_message`-be (jelenleg a Data Dial nem naplóz) és a `callAsync` lefut-e. |


## Y4 – Touch Strip módok (2026-11-02; forrás: `docs/hardware/key61/touch-strip-modes.md`, MPC v3.10 101–103. o.)
Sorrend (TOUCH STRIP gomb): Kick slider (Kick-natív alap, az első) → Pitch Bend → Pad Level → Mod Wheel → Track Level → Sustain → Expression → Off → (újra Kick slider). A doksi sorrendje a Kickben nem létező módok (Q-Link, Touch FX, Notes, Crossfader, Note Strum) kihagyásával; a Touch FX gomb továbbra is a meglévő XY képernyőt nyitja (nem változott).
| Mód | Állapot | „nincs a doksiban” → döntés + indok |
|---|---|---|
| Kick slider | A Kick eredeti csomagja (`EN_SLIDER_TOUCH_INPUT` 0..100), változatlan | Nem MPC-mód; kezdőállapot/alap. A doksi alapértelmezett módot a Preferences > Hardware-ben enged választani: ez nincs megépítve (nincs UI) |
| Pitch Bend | Doksi: mint egy pitch wheel. Változatlan: fix közép (14 bit), elengedéskor visszarugózás középre | Visszarugózás nincs a doksiban (csak a fix/relatív közép) → marad a mostani viselkedés. **Relative Mode, Bend Up/Down Range, Global Pitch Bend (Shift+gomb ablak) NINCS megépítve** (nincs UI a Kickben) |
| Pad Level | Doksi: az aktuális Pad Level-t vezérli. A Kick pad szintje: `setLevelForSelectedProgram(note, 0..127)` (= `setNoteGain(note, midiToGain)`, ugyanaz, mint a Pad Mixer csúszka) | DUMMY: (1) „aktuális pad” = az utoljára ütött pad (`bank*16 + pad`; a Kickben nincs külön kiválasztott-pad állapot a hídban; amíg nem ütöttek padet, a mód nem csinál semmit); (2) tartomány/skála: strip 0..127 → 0..127 (teljes tartomány, 1:1), nincs a doksiban; (3) elengedéskor az érték marad; (4) üzenetszálon, `callAsync`+`m_alive`, egy függő feladat |
| Mod Wheel | Doksi: CC1. Változatlan (`sendCc(1, ..)`) | DUMMY: elengedéskor az érték marad (CC-nél nincs természetes visszaugrás; változatlan a korábbi viselkedéshez képest) |
| Track Level | Doksi: az aktuális Track Level-t vezérli. `setTrackLevel(seq, track, 0..127)` az `getSelected()` sávra (ugyanaz, mint a Track/Song képernyő) | DUMMY: tartomány/skála 0..127 1:1 (nincs a doksiban); elengedéskor marad; üzenetszálon `callAsync`+`m_alive` |
| Sustain | Doksi: mint egy sustain pedál. A fizikai pedállal azonos út (`on_sustain_pedal`, CC64 127/0, a release ugyanarra a sávra megy, mint a press) | DUMMY: be/ki küszöb nincs a doksiban → pozíció ≥ 64 (felső fele) = pedál le, alatta fel; elengedéskor (és módváltáskor) mindig FEL; csak az élváltásnál küld |
| Expression | Doksi: CC11. `sendCc(11, ..)` az aktuális sávra, mint a hardveres expression pedál | DUMMY: elengedéskor az érték marad |
| Off | Doksi: külön leírás nincs. A strip nem küld semmit | – |
| Mód-LED | A TOUCH STRIP LED egyszínű (off/dim/full), a módok nem különböztethetők meg | Egyszerű szabály: Off = LED ki; bármely más aktív mód = világít (Kick slider: dim, a többi: full). A doksi a LED állapotáról nem ír („nincs a doksiban”), a dim állapot hardveren nem ellenőrzött |

Nincs megépítve (scope): Q-Link, Crossfader, Note Strum, Touch FX részletek, Notes; Shift+gomb beállítóablakok; Preferences > Hardware alapértelmezett mód. Szálmodell: a strip a reader szálon jön; a mód `std::atomic`, a sustain/pad-hang állapot atomic, a szint-módok `callAsync`+`m_alive`.

## AE1 – Data Dial diagnosztikai napló (2026-11-02, viselkedés nem változott)
A fizikai Data Dial nem hat, az injektált `jog` igen. Ritkított napló (az első 20 esemény, utána minden 50-edik): adapter `[AKAI-DIAL] cc100 val=.. -> steps=..` (a `B0 64 xx` megérkezése, reader szál), híd `[DIAL] handleJogDial steps=.. screen=..` (a `callAsync` lefutása, `screen` = az utolsó betöltött `kick_ui::Screen` egész értéke, -1 ha még nincs). Hardveres értelmezés: ha az első sor megjelenik és a második nem → `callAsync`/`m_alive` ág; ha egyik sem → a CC100 nem érkezik; ha mindkettő megjelenik de a UI nem lép → UI-oldali fogyasztó (AD1b).
