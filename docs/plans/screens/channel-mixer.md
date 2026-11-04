# Channel (Track) Mixer — review és TODO

Állapot: **statikus kódolvasás** (build/futtatás/hardver nélkül), 2026-11-04, ág `claude/kick-plans`. „KÉSZ” = van bekötött handler az engine-ig, nem hardveres igazolás. Ahol a régi docs és a kód ellentmond, a **kód az igazság**. `M/` = `Kick/modules/`, `A/` = `Kick/apps/kick/`, `CM/` = `Kick/modules/kick_ui_channel_mixer/`. Alap: `research/screen-inventory.md` §4.15, `research/known-issues-and-roadmap.md`, `walkthrough/WP-D.md` (D-001..D-044), `KEY61_HW_BEHAVIOUR.md`, `KEY61_TOUCH_STRIP_MODES.md`. A Q-Link közös hibái (routing, lépésköz, mute/solo): `knob-edit-qlinks.md` QL-01..QL-03. A **Main Volume**, a **Track Level strip** és a **master-busz útvonal** ennek a képernyőnek a részei (1. és 3. pont).

## 1. Áttekintés

**Mit csinál.** `ChannelMixer` (`CM/kick_ui_channel_mixer.cpp:56-100`): fent egy nézetválasztó (Programs, Sends, Returns, Submixes, Output, Midi, Audio — `:50`), középen az adott nézet sorlistája (`IRowView`, soronként szint/pan/mute/solo/mérő), alul a **master csík** (`MasterRouteDisplay`, 122 px magas, `:120`, `CM/source/kick_master_route_mix.h`). 25 Hz-es időzítő frissíti a mérőket (`:133`). **Nem 8 oszlopos csíkos Akai Track Mixer**: programonkénti (nem sávonkénti) sorlista (D-004/D-005).

| Nézet | Sorok | Vezérlők (kód) |
|---|---|---|
| Programs | programonként | kimenet (`output`), Inserts gomb, level (0..127 → `pProg->level`), pan (−1..1), mute, solo, in/out mérő (`CM/source/kick_program_mix.cpp:188-204`) |
| Sends | programonként | SEND 01–04 (0..127 → `midiToGain`), mérő (`kick_sends_mix.cpp:159-162, 229`) |
| Returns | return-bus | kimenet, level, pan, mute, solo (STUB), Inserts, mérők (`kick_returns_mix.cpp:168-185`) |
| Submixes | submix-bus | kimenet, level, pan, mute, Inserts (`kick_submixes_mix.cpp:167-172`) |
| Output | kimeneti bus | level, pan, mute, solo (STUB), Inserts (`kick_output_track_mix.cpp:121-137`) |
| Midi | minden **nem-Audio programú track** | MIDI input, csatorna, Send To, level (= **Track Level**), mute, solo, arm/monitor (nincs handler) (`kick_track_midi_mix.cpp:209-252`; `M/engine/audio_engine/engine/mt_audio_sequence.cpp:509-516`) |
| Audio | **Audio programok** (nem trackek) | kimenet, input (üres handler), level, pan, mute, solo, Inserts, arm/monitor (nincs handler) (`kick_audio_track_mix.cpp:186-218`) |

**Elérés.** Menü → „Track Mixer” (`M/kick_ui/dialogs/kick_AppMenuOptions.cpp:28,86`). MPC Key 61: **MIX** gomb (Shift nélkül) → `MenuChannelMixer` (`A/kick_hardware_bridge.h:1243-1251`; DCL-kód `kick_AppHome.cpp:504`); mód-LED: MIX, „A” szín (fehér) (`bridge:273`). Hardveren igazolt: képernyőnyitás és LED (RPT L93, L112).

**Hardver-kötések a képernyőn.**
| Vezérlő | Mit tesz itt | Kód | Hardveres állapot |
|---|---|---|---|
| Q-Link 1–4 | az **első látható sor** 1–4. paramétere (Programs: kimenet, level, pan, mute/solo) | `CM/kick_ui_channel_mixer.cpp:154-182`, `kick_program_mix.cpp:79-122, 318-330` | **hibás**: CM-H1..H4 |
| Q-Link 13–16 | a master csík (kimenet, level, pan, mute) — **az MPC-n nincs ilyen gomb** | `kick_master_route_mix.h:84-100` | elérhetetlen |
| Data Dial / −/+ / ENTER | a legutóbb megérintett vezérlő | `kick_ControlBase.h:126, 205-209` | igazolt általánosan (0a42bfdb) |
| Pad (16) | élő MIDI a kiválasztott trackre | `kick_ui_channel_mixer.cpp:187-210` | nem mért itt |
| PLAY / PLAY START / STOP | transzport | `:217-230` | nem mért itt |
| **Main Volume** gomb | SysEx → a **master (Out 1) busz gain-je** (a Master csík csúszkája követi) | `A/hardware/main_volume.h`, `bridge:1707-1722` | **igazolt** (898a122a, HW_BEHAVIOUR L127) |
| Touch Strip „Track Level” | a kijelölt track `level` (0..127), a „Midi” nézetben látszik | `bridge:1625-1635, 1663-1664` | injektált értékkel igazolt a Track képernyőn (HW_BEHAVIOUR L129); a Mixerben **nem nézték** (H6) |
| Touch Strip egyéb módok | Mod Wheel/Expression/Sustain/Pitch Bend/Pad Level — nincs hatás a Mixerre | `bridge:1655-1676` | — |

**Main Volume útvonal.** Port 1 SysEx `F0 47 7F 4B 5B 00 01 <v> F7`, `v` 0x00..0x75 (mért) → `decode` (`main_volume.h:42-55`) → `toMidi127` → `on_main_volume` (`A/hardware/adapters/akai_mpc_key61_adapter.h:413`) → `callAsync` → `handleMainVolume`: `getBusManager().getOutput(0)->gain = midiToGain(0..127)` (`bridge:1707-1722`). A `gain` a projekt ValueTree-jében él `UndoManager`-rel (`M/engine/audio_engine/engine/mt_audio_bus_manager.cpp:77-83`), tehát **projektállapot**. A 898a122a javította, hogy a busz engine-oldali bypass-a miatt a gain/mute/pan nem hatott.

**Track Level útvonal.** `setTrackLevel(seq, track, 0..127)` → `MTTrack::setLevel` (`mt_audio_track.cpp:92`); a motor a `level` tulajdonságból `levelScaler`-t számol (`:164-166`), amely az **új note-ok sebességét** skálázza (`getEffectiveNoteVelocity`, `:855-858`) — nem utófader hangerő, a már szóló hangokra nincs hatása. Write automation módban a mozgás rögzül (`:314-330`).

## 2. Funkciók és állapotuk

| Funkció | Állapot | Bizonyíték |
|---|---|---|
| Program level/pan/mute/solo (érintés, Data Dial) | KÉSZ (kód) | `kick_program_mix.cpp:188-199` |
| Program kimenet-választó (output bus / submix) | KÉSZ | `:165-187` |
| Program Inserts → Inserts képernyő | KÉSZ (kód) | `:201-204` |
| Sends 1–4 | KÉSZ | `kick_sends_mix.cpp:159-162` |
| Returns / Submixes: level, pan, mute, kimenet, Inserts | KÉSZ (kód) | `kick_returns_mix.cpp:168-178`, `kick_submixes_mix.cpp:167-172` |
| Returns / Output **solo** | STUB (az engine-hívás kikommentezve, csak a kép vált) | `kick_returns_mix.cpp:180-185`, `kick_output_track_mix.cpp:132-137` |
| Output bus level/pan/mute | KÉSZ (kód) | `kick_output_track_mix.cpp:121-123` |
| Midi nézet: level (Track Level), port, csatorna, Send To | KÉSZ (kód) | `kick_track_midi_mix.cpp:209-252` |
| Midi nézet: mute | RÉSZLEGES: közvetlen `pTrack->mute =` megkerüli a mute-csoportot (CM-H11) | `:224-236` vs `mt_audio_track.cpp:100-113` |
| Midi nézet: solo | KÉSZ (`ae.setTrackSolo`, egyszeres solo-lista) | `:238-242` |
| Audio nézet: output, level, pan, mute, solo | KÉSZ (kód) | `kick_audio_track_mix.cpp:194-218` |
| Audio nézet: **input**, **arm**, **monitor** (minden nézetben) | STUB (nincs handler) | `:191-192, 144, 221`; D-023/D-037..D-040 |
| Master csík: level, pan, mute, kimenet-választó, Inserts | KÉSZ (kód); Main Volume igazolt | `kick_master_route_mix.h:56-65` |
| Master **Limiter** gomb | STUB (nincs `onClick`) | `kick_master_route_mix.h:111,129` |
| Master RMS / PEAK feliratok | STUB (sosem frissülnek) | `kick_master_route_mix.h:105-106` (nincs `setText`) |
| Mérők (25 Hz) | RÉSZLEGES/BUGOS GYANÚ (CM-H8, CM-H9) | `kick_ui_channel_mixer.cpp:147-153` |
| Q-Link a sorokon | BUGOS (CM-H1..H6) | lásd 3. pont |
| Main Volume gomb → master gain | KÉSZ, de projektállapotot ír (CM-H12) | `bridge:1707-1722` |
| Strip Track Level → Midi nézet | KÉSZ (kód, Track képernyőn igazolt); Mixerben igazolatlan | `kick_track_midi_mix.cpp:293-314` |
| Automation / Solo-Cue mód / crossfader / mérési módok / Track Settings | HIÁNYZIK (D-009..D-015, D-021, D-022) | WP-D |

## 3. Mi nem működik / hibák

**CM-H1 — Összeomlás Q-Link forgatásra az Output és a Midi nézeten (P0).** A nézetek sorrendje Output (4), Midi (5) (`kick_ui_channel_mixer.cpp:71-72`), a `knobCommand` viszont a 4. indexhez `TrackMidiMixer*`-t, az 5.-hez `OutputTrackMixer*`-t kasztol (`:173-177`). A `dynamic_cast` `nullptr`-t ad, amire `->knobCommand` hívódik → null-dereferencia. Az alapértelmezett Q-Link-mód a „Screen” (`kick_settings.h:218`), tehát az út éles. Reprodukció: Output vagy Midi nézet, `encoder 0 1`. Megoldás: CM-01.

**CM-H2 — Q-Link érintés után halott (P0, magas valószínűség).** Közös gyök: `KickHome::handleCommand` a Q-Link-csomagot a kijelölt vezérlőnek adja (`kick_AppHome.cpp:573-580`), amely eldobja (`kick_ControlBase.h:120`); lásd `pad-mixer.md` PM-H3 és QL-01.

**CM-H3 — Mute/solo Q-Link csak képet vált, 7 helyen (P0).** `setToggleState(value > 0, dontSendNotification)` + `solo.setToggleState(value <= 0, …)` az engine nélkül: Programs `kick_program_mix.cpp:326`, Returns `kick_returns_mix.cpp:300`, Submixes `kick_submixes_mix.cpp:292`, Output `kick_output_track_mix.cpp:235`, Midi `kick_track_midi_mix.cpp:452`, Audio `kick_audio_track_mix.cpp:323`, **Master** `kick_master_route_mix.h:97` (a master mute Q-Link nem némít). Megoldás: QL-03.

**CM-H4 — Pan és (egyes) level Q-Link lépésköz (P0).** A pan −1..1 (`KickPanRotary`, `M/kick_ui_components/source/kick_common_controls.h:74-84`), a Q-Link delta ±1 nyersen adódik hozzá (`kick_slider.cpp:128-134`): egy kattanás teljes kitérés. A 0..127-es `KickLevelSlider` (level, send) ±1-gyel lépked, ott rendben van. Érinti: Programs, Returns, Submixes, Output, Audio és a master pan. Megoldás: QL-02.

**CM-H5 — Master csík Q-Link 13–16 az MPC-n elérhetetlen (P1).** A master csík a 13–16. gombra van kötve (`kick_master_route_mix.h:84-100`), az MPC-n csak 1–4 létezik; a Q-Link nem hat a masterre. A master mute-gomb ráadásul csak vizuális (CM-H3).

**CM-H6 — Üres lista = összeomlás (P0).** Mind a hat nézet `knobCommand`-ja `rowComp[0]`-t ellenőrzés nélkül kasztolja és dereferálja (`kick_program_mix.cpp:82`, `kick_sends_mix.cpp:86`, `kick_returns_mix.cpp:66`, `kick_submixes_mix.cpp:66`, `kick_output_track_mix.cpp:56`, `kick_track_midi_mix.cpp:101`, `kick_audio_track_mix.cpp:99`). Nulla program / return / audio program esetén, vagy sorok nélküli nézeten Q-Link-re UB. Az `rows[i]` indexelés alsó határa csak az Audio nézetben van (`kick_audio_track_mix.cpp:108`), máshol `rowIndex < numRows` van csak.

**CM-H7 — Midi nézet: sor ≠ track-index (P1).** A sorok a nem-Audio programú trackek (`midiTracks[Row]`), de `onRowSelected(Row)` → `ae.setSelectedTrack(Row)` (`kick_track_midi_mix.cpp:35-38`) és `selectRow(ae.getSelectedTrack())` (`:172`) a **sor**-számot használja track-indexként. Ha Audio programú track is van a szekvenciában, a koppintás rossz tracket jelöl ki, és a Strip Track Level is rossz tracket állítja.

**CM-H8 — Mérők be/ki kapcsolása hibás (P1).** (a) A Submixes `visibilityChanged` a **return** busz-neveken kapcsolja a mérést (`kick_submixes_mix.cpp:114` `getReturnBusNames()`, másolási hiba; a `getSubmixBusNames()` kellene) → a submix mérők sosem kapnak adatot a nézetből. (b) A Programs nézet csak a kimeneti mérést kapcsolja (`kick_program_mix.cpp:315`), de a bemeneti mérőt (`readInMeter`, `:276`) soha. (c) A master és az Output busz mérőjét nem kapcsolja be a kód (`enable*Metering` hívás nincs), a mérő csak az alapállapottól függ: `AudIO/AudIO/Core/Chain.h:58-59` `std::atomic<bool> inMeterEnabled, outMeterEnabled;` **inicializálatlan** C++17-ben (a `Chain.cpp` konstruktora sem állítja), tehát a kezdeti állapot nem determinisztikus. (d) A szintézis-oldali `enableOutputMetering` argumentumát figyelmen kívül hagyja (`iSynthObject.h:45`, `audioPreview.h:52`), így a program-mérés soha nem kapcsol ki.

**CM-H9 — Csúcsjelzés: azonos csatorna kétszer (P2, GYANÚ).** A mérők `setSignalStrength(inL.rms, inR.rms, in[0].peak, in[0].peak)` és `(outL.rms, outR.rms, out[1].peak, out[1].peak)` alakúak: Programs `kick_program_mix.cpp:278-279`, Returns `:269-270`, Submixes `:262-263`, Output `:209-210`, Audio `:291-292`, Midi `:394`, master `kick_master_route_mix.h:80`; egyedül a Sends helyes (`out[0].peak, out[1].peak`, `kick_sends_mix.cpp:229`). Lehet eredeti tervezés (egy csúcs-jelzés), de a Sends-eltérés miatt valószínűbb másolási hiba.

**CM-H10 — Működő-látszó, hatástalan kezelőszervek (P1).** Master Limiter gomb és RMS/PEAK feliratok (lásd 2. pont), Returns/Output solo (engine-hívás kikommentezve), minden Arm/Monitor, az Audio Input lista. Az Audio nézet 25 Hz-en újraépíti a programlistát (`updateProgramData()` minden `meterUpdate()`-ben; ha nincs Audio program, `refresh()` is, `kick_audio_track_mix.cpp:62-78`).

**CM-H11 — Mute-csoport a Mixerből nem érvényesül (P1).** A Midi nézet `pTrack->mute = …` közvetlenül írja az adatot (`kick_track_midi_mix.cpp:233-234`), a Track Mute képernyő `ae.setTrackMute` → `MTTrack::setMute` a csoporttagokat is némítja (`mt_audio_track.cpp:100-113`). Ugyanaz a track a két képernyőn különbözően viselkedik.

**CM-H12 — Main Volume a projekt master-gain-jét írja (P1, tervezési).** (1) A fizikai gomb abszolút pozíciót küld, a projektben tárolt gain viszont betöltéskor felülírja az értéket — a gomb állása és a tényleges hangerő eltér, amíg a gombot nem mozdítod. (2) A gain mentődik a projekttel és (UndoManager-rel hivatkozott) undo-lépést is okozhat — hogy a gombmozgások egyesülnek-e, nem vizsgáltam. (3) Az SysEx `0x5B` parancsbájt jelentése ismeretlen, `0x76..0x7F` sosem látott (H12 a known-issues-ban). (4) 2026-11-04-én a lehúzott gomb miatt „nem szólt a zongora” (`KEY61_HW_BEHAVIOUR.md` L127): a UI nem jelezte a hangerőt a Mixeren kívül, nincs OSD. (5) `gainToMidi` csonkít (`(int)(Gain*127)`, `kick_common_utils.cpp:53-56`): float32-szimulációval a 9, 13, 18, 26, 36, 52, 72, 104 értékek 1-gyel kisebbet adnak vissza (a master/Output/Sends csúszka kijelzése ennyivel eltér; ARM-on ellenőrizendő).

**CM-H13 — Track Level hatása a Mixerben igazolatlan (P1).** A Strip Track Level mód a Track képernyőn igazolt (HW_BEHAVIOUR L129), a Mixer „Midi” nézete az `onValueChange` bekötés miatt követi (`kick_track_midi_mix.cpp:313-314`), de ezt nem nézték hardveren (H6). A Track Level velocity-skálázó (lásd 1. pont), nem hangerő: hallható hatása csak új note-oknál van, Audio programú trackre nincs.

**CM-H14 — Apróságok (P2).** `DBG("ChannelMixer VISIBLE")` zaj a kódban (`:134,140`). Az `AudioTrackMixer::updateProgramData` az `ae.getProgram(idx)->getType()`-ot null-ellenőrzés nélkül hívja (`kick_audio_track_mix.cpp:87`).

## 4. Tervben / hiányzik (Akai MPC Standalone-hoz képest)

Forrás: WP-D D-001..D-044, `KEY61_DOC_VS_KICK.md`.
- **Layout**: 8 csík egy képernyőn, Volume / Pan&Volume / Sends / Effects / I-O fülek, balra söprésre Submix/Return/Output (D-004, D-008, D-016, D-032, D-036) — a Kick sorlistás, legördülős (döntés).
- **Mixer Settings** ikon: Solo Tracks / Cue Tracks mód, crossfader profil (Linear/Exp/Log), MIDI/Audio Input Metering módok (D-009..D-014, D-021).
- **Track Settings** (sávfejléc dupla érintés), nagy csúszka/mérő dupla érintésre, pan nagy csúszka (D-015, D-019, D-025).
- **Automation gomb** soronként (D-022, D-028).
- **Arm / Monitor** (Off/In/Auto/Merge) (D-023, D-037..D-039), Audio In mező (D-040), **MIDI Out** port/csatorna (D-044), **Key Range / Learn / MIDI Filter** (D-165, D-166, D-175..D-179), Track -/+ gombok, Duplicate Track (D-155, D-159).
- **Sends fülön** mute/solo/arm (D-027, D-029).
- **Master**: az Akai Main Volume a kimenetek és a fejhallgató hangereje, **nem** projektállapot (a Key 61 gombja SysEx-et küld, a firmware kezeli; HW_BEHAVIOUR L20). Main és Phones azonos jelet kap, Outputs 3/4 nincs (TODO §2.5).
- **Q-Link**: az Akai Q-Link-gombja oszlopot vált; a Mixer Q-Link-térképe nincs dokumentálva a repóban (WP-H nincs kész).
- **Touch Strip**: Q-Link és Crossfader mód nincs; a Track Level hatása a doksiban nincs leírva (STRIP §2): tartomány, skála, elengedés — jelenleg 1:1 0..127, értéke marad (DUMMY, MD L126).

## 5. Javaslataim

1. **Q-Link a kijelölt sorra, négy fix szerepkörrel** (UX, MPC): Q-Link 1–4 = a kijelölt program/track/bus kimenet (vagy send), level, pan, mute/solo; a Q-Link gomb váltson paramétercsoportot (QL-07). A 16 gombos rácsot emuláló `getRowNumbers`/`getElementBounds` logika így elhagyható (egyszerűbb, kevesebb hibaforrás).
2. **Egységes `QLinkTarget` absztrakció**: minden vezérlő deklarálja a Q-Link-lépését (tartomány/lépés/kapcsoló), a képernyők csak a célt adják át — egy hely a CM-H1/H3/H4/H6 és a PM-H1/H2 javítására.
3. **Main Volume mint hardver-hangerő** (CM-H12): a gomb ne a projekt master-gain-jét írja, hanem egy nem perzisztens „output trim”-et (vagy legalább ne jelölje módosítottnak a projektet), felugró OSD-vel (érték %), és a fizikai állás vs. projekt-gain eltérését jelezze („pickup”). Owner-döntés, mert a mai viselkedés (a Mixer master csúszkája = a gomb) szándékos volt (MD AD1a).
4. **Mérők egy helyen kapcsolva**: `ChannelMixer::visibilityChanged` kapcsolja be/ki az összes látható nézet és a master mérését (a mai szétszórt, részben hibás kód helyett); a `Chain` atomjainak inicializálása; a csúcs-átadás javítása.
5. **Valódi bus-solo** vagy a gomb eltüntetése (Returns/Output); Limiter és RMS/PEAK megvalósítása vagy eltávolítása — működő-látszó kezelőszerv ne maradjon (ahogy a Preferences-auditnál).
6. **Midi nézet átnevezése/bővítése**: „Tracks” legyen (Akai terminológia), a sor a track-index szerint, Audio programú trackekkel együtt, arm/monitor/Key Range bekötésével (D-155..D-170).
7. **Teljesítmény**: nézetenként csak a látható sorok mérői; az Audio nézet ne építsen újra 25 Hz-en; a 128 pad / sok program nézeteknél sor-virtualizálás.
8. **dB-megjelenítés**: egységes skála a Pad Mixerrel (PM-05).
9. **Hangerő-OSD** a Main Volume és a Strip Track/Pad Level módokhoz bármely képernyőn (a „nem szól” incidens megelőzésére).

## 6. Hardveres és unit-tesztek

**Ami ma fedi:** a Mixerre host-teszt **nincs**. Közvetve: `MainVolumeTests.cpp` (SysEx-dekódolás: `decode`, `toMidi127`, kerekítés/határ; nem a bus-írás), `StripModesTests.cpp` (pozíció → érték), `AutomationTests.cpp` (track level lane), `MuteFilterTests.cpp` (némított track mit enged át), `RelativeCcTests.cpp` (Q-Link dekódolás). Hardveren igazolt: képernyőnyitás+LED (RPT L93, L112), Main Volume → hallható hangerő (898a122a, mérve), Data Dial érték-állítás (0a42bfdb), Strip Track Level a Track képernyőn (HW_BEHAVIOUR L129, injektált). Képernyőkép-smoke: `KICK_SMOKE_SCREENS="MenuChannelMixer"`.

**Hiányzik:** Q-Link routing/lépésköz/kapcsoló, a nézet-sorrend ↔ kasztolás (CM-H1), üres nézetek, mérő-bekapcsolás, a bus-gain írás (Main Volume ↔ projekt), `gainToMidi` körút-teszt (0..127), Midi-nézet sor↔track leképezés.

**MPC-n ellenőrzendők** (`kick_cmd`: `encoder <idx> <delta>`, `touch`, `btn`, `pad`, `snapshot`, `strip`, `rawmidi`):
- [ ] Q-Link forgatás minden hét nézeten (különösen Output és Midi: összeomlás-e) — CM-H1.
- [ ] Q-Link érintés előtt/után (CM-H2); Q-Link 3 mute: hallható-e a némítás (CM-H3); master mute Q-Link (nincs 13–16 → n/a).
- [ ] Pan Q-Link lépésköze (CM-H4).
- [ ] Main Volume: gomb végigtekerése 0..max, a Master csúszka követi-e; projekt betöltése után mekkora a master gain a gomb állásához képest; mentés/betöltés megőrzi-e; Undo-lépés keletkezik-e.
- [ ] Strip Track Level a Mixer Midi nézetén: ujjal húzva a csúszka mozog-e, új note-ok hangereje változik-e, Audio programú trackkel a sor-index (CM-H7).
- [ ] Mérők: minden nézeten ütésre mozognak-e; submix mérő (CM-H8); master mérő első indításkor.
- [ ] Returns/Output „Solo” gomb: hat-e (CM-H10); Midi nézet mute-csoporttal (CM-H11).
- [ ] CPU/UI-késés 7 nézeten 25 Hz-es frissítéssel (ARMv7).

## 7. TODO

Függőségek: CM-02..CM-04 a QL-01/QL-02/QL-03-ra épülnek.

- [ ] **CM-01** (P0, S) `ChannelMixer::knobCommand` kasztolás-csere javítása: az indexek legyenek az `pViews` sorrendjéből (virtuális `knobCommand` az `IRowView`-n, kasztolás nélkül) — `kick_ui_channel_mixer.cpp:71-72, 154-182`.
- [ ] **CM-02** (P0, S) Mute/solo Q-Link valódi engine-hívással, 7 helyen (Programs, Returns, Submixes, Output, Midi, Audio, master) — lásd CM-H3. Függ: QL-03.
- [ ] **CM-03** (P0, S) Pan és bus-level Q-Link lépésköz a közös rétegen át — CM-H4. Függ: QL-02.
- [ ] **CM-04** (P0, S) `rowComp[0]` / index-határ védelem mind a hét nézetben (üres lista, −1, ≥ méret) — CM-H6.
- [ ] **CM-05** (P1, M) Q-Link a kijelölt sorra (nem az első láthatóra); a master csík 1–4-es kiosztása vagy külön „Master” mód — CM-H5. Függ: QL-01, QL-07.
- [ ] **CM-06** (P1, S) Midi nézet: sor→track leképezés (`midiTracks[Row]`) a kijelölésnél és a `selectRow`-nál — `kick_track_midi_mix.cpp:35-38, 172`.
- [ ] **CM-07** (P1, S) Mérők: Submix `getSubmixBusNames()` (`kick_submixes_mix.cpp:114`), Programs bemeneti mérő be/ki, master+Output be/ki a láthatóság szerint, `Chain.h:58-59` atomok `{false}` inicializálása, `iSynthObject`/`audioPreview` `enableOutputMetering` argumentuma.
- [ ] **CM-08** (P2, S) Csúcsjelzés: `out[0].peak, out[1].peak` (hat helyen) — owner-döntés, ha a Sends eltérése szándékos volt.
- [ ] **CM-09** (P1, M) Hatástalan kezelőszervek: bus-solo az engine-ben (`setBusSolo`) vagy a gombok eltávolítása; Limiter; RMS/PEAK feliratok; Arm/Monitor/Audio Input bekötése (D-023, D-037..D-040).
- [ ] **CM-10** (P1, S) Midi nézet mute a `ae.setTrackMute`-on át (mute-csoport) — `kick_track_midi_mix.cpp:224-236`.
- [ ] **CM-19** (P2, S) Dupla transzport-parancs: a `ChannelMixer::handleCommand` PLAY/PLAY START/STOP ága (`kick_ui_channel_mixer.cpp:225-228`) megismétli a hídét (`bridge:921-960`) — vizsgálat és eltávolítás a `MU-09` szerint.
- [ ] **CM-11** (P1, M) Main Volume átgondolása: nem perzisztens trim + OSD + pickup/eltérés-jelzés (owner-döntés kell) — `bridge:1707-1722`.
- [ ] **CM-12** (P2, S) `gainToMidi` kerekítés (`std::lround`) a csonkítás helyett — `kick_common_utils.cpp:53-56`; körút-teszt 0..127.
- [ ] **CM-13** (P1, S) Strip Track Level hatásának hardveres igazolása a Mixer Midi nézetén, dokumentálása; a „velocity-skálázó, nem hangerő” viselkedés feliratozása a UI-n („Track Velocity Scale”) vagy valódi track-hangerő.
- [ ] **CM-14** (P2, M) Teljesítmény: az Audio nézet ne építsen újra 25 Hz-en (`kick_audio_track_mix.cpp:62-78`), nézetenként csak látható sorok mérői; mérés ARMv7-en.
- [ ] **CM-15** (P2, L) Akai-paritás: Mixer Settings (Solo/Cue, crossfader, mérési módok), Track Settings ablak, Key Range/Learn/MIDI Filter, automation gomb, Track -/+, Duplicate Track (D-009..D-015, D-155..D-179) — a mixer-elrendezés (8 csík) owner-döntése után.
- [ ] **CM-16** (P2, S) `DBG` zaj eltávolítása, Midi→„Tracks” átnevezés (lásd 5. pont).
- [ ] **CM-17** (P1, M) Host-tesztek: Q-Link lépés/irány (QL-17), `gainToMidi` körút, Midi sor↔track leképezés (kiemelt tiszta függvény), Main Volume bus-írás (tiszta „apply” függvény) — `Kick/tests/platform`.
- [ ] **CM-18** (P1, S) MPC-s checklist (6. pont) lefuttatása és rögzítése a `HW_TEST_REPORT`-ban.
