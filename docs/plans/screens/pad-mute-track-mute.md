# Pad Mute / Track Mute — review és TODO

Állapot: **statikus kódolvasás** (build/futtatás/hardver nélkül), 2026-11-04, ág `claude/kick-plans`. „KÉSZ” = van bekötött handler az engine-ig, nem hardveres igazolás. Ahol a régi docs és a kód ellentmond, a **kód az igazság**. `M/` = `Kick/modules/`, `A/` = `Kick/apps/kick/`, `MD/` = `Kick/modules/kick_ui/modes/`. Alap: `research/screen-inventory.md` §4.12/4.13 (ott „nem követtem az engine-ig”, itt követtem), `research/known-issues-and-roadmap.md`, `walkthrough/WP-D.md` (D-071..D-143), `KEY61_DOC_VS_KICK.md` (#17), `KEY61_LED_COLOURS.md`. A Q-Link közös hibái: `knob-edit-qlinks.md` (QL-01..QL-03). Két képernyő, közös szerkezet; a jelölés `[Pad]` / `[Track]` / `[Mind]`.

## 1. Áttekintés

**Mit csinálnak.**
- **Track Mute** (`MD/kick_trackMute.{h,cpp}`): az aktuális szekvencia trackjeit némítja/szólózza; 128 pad-helyes „mini-map” + pad-rács (a track-index = pad-index az aktuális bankban), alul fülek: *Track Mute* / *Track Group* (mute-csoport 1..16 vagy None), *Solo*, *Clear Solos*, *Instant / Time Division* + időosztás-lista. Fent szekvencia-választó (`HeaderBarSequenceSelector`).
- **Pad Mute** (`MD/kick_padMute.{h,cpp}`): az aktuális **Drum** track padjait (mintái) némítja/szólózza; ugyanaz az elrendezés, fent Track-választó. Nem Drum trackre: „PAD MUTE IS NOT AVAILABLE FOR SELECTED TRACK” (`kick_padMute.cpp:46, 144-153`).
- Mindkettő: fogaskerék → „… AUTOMATION SETTINGS” ablak (`kick_padMute.cpp:73-76`, `kick_trackMute.cpp:65-68`).

**Elérés.** Menü → „Track Mutes” / „Padmutes” (`M/kick_ui/dialogs/kick_AppMenuOptions.cpp:29,34,87,92`). MPC Key 61: **MUTE** → Track Mute, **Shift+MUTE** → Pad Mute (`A/kick_hardware_bridge.h:1257-1270`; DCL-kódok `kick_AppHome.cpp:505-506`). Mód-LED: MUTE, Track Mute „A” szín (fehér), Pad Mute „B” szín (piros) (`bridge:275-276`). Hardveren igazolt: képernyőnyitás és LED (RPT L95-96, L113). A képernyők saját pad-LED-eket küldenek, a `KickLEDs` ilyenkor nem ír rájuk (`M/kick_leds/kick_leds.h:73-79` `invalidScreens`).

**Hardver-kötések a képernyőkön.**
| Vezérlő | Mit tesz | Kód | Hardveres állapot |
|---|---|---|---|
| Pad (16) | mute-váltás / solo-váltás (Solo kapcsolóval) / csoport-hozzárendelés (Group fül): a pad-csoport komponens a sigslot-úton `triggerClick()`-et hív | `M/kick_ui_components/source/kick_padButtonGroup.cpp:684-731`, `kick_trackMute.cpp:176-216`, `kick_padMute.cpp:203-234` | **nem mért** (a gombnyitás igen) |
| Bank A–D, Shift: E–H | `tab->changeBank` + `ae.setBank` (KickHome) | `kick_padMute.cpp:108-117`, `kick_trackMute.cpp:113-122` | A–D LED igazolt; E–H nem mért |
| Q-Link 1–4 | csoportszám léptetése a pad/track 1–4-hez (`assignKnobToPad`) | `kick_padMute.cpp:449-457`, `kick_trackMute.cpp:401-411` | **hibás** (MU-H2) és érintés után halott (QL-H1) |
| PLAY / PLAY START / STOP | transzport (a híd is végrehajtja → kétszer fut, MU-H8) | `kick_padMute.cpp:119-121`, `kick_trackMute.cpp:124-126`, `bridge:946-960` | — |
| Data Dial | nincs képernyő-szintű kezelés (csak a megérintett vezérlőre hat) | — | — |
| Pad-LED-ek | `EN_PAD_SET_COLOUR_STATE` csomag: némított = piros, nem némított = világoszöld, solo = kék (track), mini-map színek | `kick_trackMute.cpp:330-364`, `kick_padMute.cpp:331-358`, `bridge:137-160`, `A/hardware/pad_colour_map.h` | pad-színek általában igazoltak (RPT L240); a mute-színek nem külön |
Érintés: pad-rács koppintás, mini-map bankváltás, fülek — a gombok igazoltan érkeznek (RPT 8. kör után `delivery=manual`).

**Mute az engine-ben.** A track `mute` és `muteGroup` a projekt ValueTree-jében él `UndoManager`-rel (`M/engine/audio_engine/engine/mt_audio_interface_types.cpp:500-505`): mentődik és visszavonható, **szekvenciánként** külön (a track a szekvenciához tartozik). Hatás: `isTrackEffectivelyMuted() = (!solo && mute) || soloMuted` (`mt_audio_track.h:165`); a némított track új note-ot nem indít, de a note-off / pitch wheel / CC64 / CC123 átmegy (`mt_mute_filter.h`, `mt_audio_track.cpp:797-800`, host-teszt: `MuteFilterTests.cpp`). A pad `mute` a minta (`MTISample`) tulajdonsága (szintén ValueTree+undo, `mt_audio_interface_types.cpp:130-137`); a pad-solo viszont a program memóriabeli `soloingNotes` bitkészlete (`mt_audio_iprogram.h:227-229`), a lejátszás ezt olvassa (`synth/DrumProgram.cpp:80`).

## 2. Funkciók és állapotuk

| Funkció | Állapot | Bizonyíték |
|---|---|---|
| [Track] pad/érintés → track mute váltás | KÉSZ (kód); mute-csoportot is propagál | `kick_trackMute.cpp:203` → `mt_audio_interface.cpp:1227-1232` → `mt_audio_track.cpp:100-113` |
| [Track] Solo (egyszeres: új solo kioltja a többit) | KÉSZ (kód); csak Single viselkedés (D-091) | `kick_trackMute.cpp:181-200` |
| [Track] Solo kapcsoló ki → minden solo törlése | KÉSZ | `:227-238` |
| [Track] **Clear Solos** | BUGOS: a motor nem nullázza (MU-H3) | `:244-251` → `mt_audio_sequence.cpp:427-434` |
| [Track] Track Group fül: csoport kijelölése, pad-ütés hozzárendel | KÉSZ (kód); a sorrend fordított az Akai-hoz képest (D-108/D-110) | `:206-213`, `mt_audio_interface.cpp:1035-1039` |
| [Track] Q-Link csoport-hozzárendelés | BUGOS (bank nélkül, nincs LED/mini-map frissítés, nincs None) | `:401-411` (MU-H2) |
| [Track] Sequence-választó | KÉSZ; **globálisan** átállítja a kiválasztott szekvenciát | `:41-49` |
| [Pad] pad/érintés → pad mute váltás | KÉSZ (kód) | `kick_padMute.cpp:219-222` → `mt_audio_iprogram.cpp:464-487` |
| [Pad] Solo / Clear Solos | RÉSZLEGES: működik, de nem perzisztens / nem undo-zható; Clear: `dynamic_cast` null-ellenőrzés nélkül | `:212-218, 253-262` (MU-H4, MU-H6) |
| [Pad] Pad Group fül | KÉSZ (kód); a hozzárendelés a **sorozat-indexet** adja program-indexként (MU-H2) | `:224-230, 455` |
| [Pad] Pad-LED frissítés nyomásra | BUGOS: az egyedi pad-csomagot a híd eldobja (MU-H1) | `:231-232, 360-408`, `bridge:137-160` |
| [Pad] drum-only korlát, infó-felirat | KÉSZ; Clip/Drum következetlenség a LED-nél (MU-H5) | `:144-153, 160-163` |
| [Mind] Pad-LED-ek bank-/képernyőváltáskor | KÉSZ (`EN_PAD_SET_COLOUR_STATE`) | `kick_trackMute.cpp:330-364`, `kick_padMute.cpp:331-358` |
| [Mind] Instant / Time Division + időosztás-lista | STUB: csak a lista engedélyezése, nincs fogyasztó, nincs késleltetett némítás | `kick_padMute.cpp:249-251`, `kick_trackMute.cpp:240-242`; a `timeDivList` máshol nem szerepel (grep) |
| [Mind] Settings ablak (Instant / Play / Record Mute and Solo Events) | STUB: három kapcsoló, soha nem olvassa semmi, az Apply `delete p` | `kick_padMute.cpp:73-76, 474-495`, `kick_trackMute.cpp:65-68, 428-445` |
| [Mind] Mini-map, bank-jelzés, csoportszám a padon | KÉSZ (kód) | `kick_padMute.cpp:277-305`, `kick_trackMute.cpp:276-304` |
| [Mind] Mute/solo események rögzítése automationbe, undo-lista | HIÁNYZIK (D-088/D-089/D-126/D-127) | — |
| [Mind] Event/Audio mute, ikonok | HIÁNYZIK (D-076/D-077/D-094/D-098) | — |

## 3. Mi nem működik / hibák

**MU-H1 — [Pad] A pad-LED nem frissül a nyomásra az MPC-n (P1).** A Pad Mute nyomásra `refreshData(); setPadLED(Id);`-et hív (`kick_padMute.cpp:231-232`); a `setPadLED` egyedi pad-csomagot (`setPadDataMessage` = `EN_PAD_SET_SINGLE_PAD`, `:360-408`) küld. Az MPC-híd DCL-oldalon csak az `EN_PAD_SET_AFTER_TOUCH`-ot és az `EN_PAD_SET_COLOUR_STATE`-et (32 érték: 16 szín + 16 állapot) dolgozza fel, minden mást eldob (`A/kick_hardware_bridge.h:137-160`). Ezért a pad színe (zöld ↔ piros) csak bank-/képernyőváltáskor vagy Clear Solos után frissül (ott `refreshAllLEDs`, `:331-358`), nyomásra nem; a Track Mute helyesen `refreshAllLEDs()`-et hív (`kick_trackMute.cpp:214-215`). A `clearAllPadsMessage` (`EN_PAD_SET_ALL_SINGLE_COLOUR`) és a `setPadColorModeMessage` (`EN_PAD_COLOR_MODE`) is eldobódik (`kick_padMute.cpp:171-172, 178`, `kick_trackMute.cpp:147, 153, 221`). Reprodukció: Shift+MUTE, pad-ütés, LED nézése; `echo "pad 0 100" > /data/kick_cmd`.

**MU-H2 — Q-Link csoport-hozzárendelés több helyen hibás (P0).** (a) [Pad] `ae.padAddToGroup( ae.getSelectedSequence(), padId, grp )` (`kick_padMute.cpp:455`): az első paraméter **program-index** (`mt_audio_interface.cpp:704-711`), a hívás a szekvencia-indexet adja át. Csak akkor jó, ha a szekvencia-index véletlenül egyenlő a program-indexszel (0/0). Másik program csoportja módosul. (b) [Mind] `padId = knobId - 1` / `trackId = knobId - 1`, **bank nélkül** (`kick_padMute.cpp:451`, `kick_trackMute.cpp:405`), és az MPC-n csak az 1–4 gomb létezik → mindig az A bank első négy padja/trackje. (c) A szám `jlimit(1,16)` (`kick_padMute.cpp:454`, `kick_trackMute.cpp:408`): a „None”-ra nem lehet visszalépni; a motor `MUTEGROUP` tartománya −1..32 (`M/common/source/kick_Constants.h:81-87`), a UI 1..16. (d) Nincs `refreshData()` / LED-frissítés / mini-map frissítés a hozzárendelés után, csak a pad-szám. (e) Érintés után a Q-Link a képernyőig sem jut (QL-H1). A WP-D `OK` besorolásai (D-101, D-105, D-112, D-135, D-137, D-143) ezért cáfolhatók.

**MU-H3 — [Track] „Clear Solos” nem szünteti meg a szólót a motorban (P0).** `MTSequence::resetTrackSolos()` csak a `soloingTracks` bitkészletet törli (`mt_audio_sequence.cpp:427-434`); a track-ek `solo` flagjét nem, és az `updateTrackSoloList()`-et (ami a track-ek `soloMuted` atomját frissíti, `:403-416`) nem hívja. A `MTAudioInterface::resetTrackSolos()` (`mt_audio_interface.cpp:1108-1113`) ennyit tesz. Eredmény: a UI (`getTrackSolo` → `soloingTracks`) „nincs solo”-t mutat, de a nem-szólózott trackek `soloMuted` állapota marad (`isTrackEffectivelyMuted`, `mt_audio_track.h:165, 193`) → **tovább némák**; a szólózott track `solo` flagje is marad, és a következő `setTrackSolo` visszaállítja a szólót. A „Solo” kapcsoló kikapcsolása viszont helyesen minden trackre `setTrackSolo(false)`-t hív (`kick_trackMute.cpp:227-238`). Reprodukció: két trackkel Solo → Clear Solos → lejátszás. A pad-oldali megfelelő (`resetNotesSolo`) rendben van, mert a lejátszás közvetlenül a bitkészletet olvassa.

**MU-H4 — [Pad] Összeomlás-gyanú Clear Solos-ra nem Drum programnál (P1).** `dynamic_cast<mt::MTDrumProgram*>( pProg )->resetNotesSolo()` null-ellenőrzés nélkül (`kick_padMute.cpp:256`), miközben `visibilityChanged` a Clip-et is engedi (`:160`), és a `resetNotesSolo` a közös `MTIProgram`-on van (`mt_audio_iprogram.h:230`), tehát a kasztolás fölösleges.

**MU-H5 — [Pad] Forrás-inkonzisztencia (P2).** Az adatot `ae.getProgramForTrack(sel.seq, sel.track)` adja (`kick_padMute.cpp:204, 254, 282, 320`), a LED-ek és a láthatóság viszont `ae.getSelectedProgram()`-ot használnak (`:159, 333, 362`); a „drum-only” ellenőrzés a track programját nézi (`:137-144`). Ha a kiválasztott program nem a kiválasztott track programja, az LED és a rács külön programot mutat. Clip programnál a LED-kód fut (`setScreenState(true)`, `:160-163`), a képernyő viszont „nem elérhető”-t ír (`:144-153`).

**MU-H6 — [Pad] Solo/csoport szemantika (P2).** A pad-solo nem perzisztens, nem visszavonható (bitkészlet), a mute igen (ValueTree+undo), tehát az Undo-lista egyik toggle-t látja, a másikat nem. `MTIProgram::setNoteMute` mute-csoportban az egész csoportot állítja (`mt_audio_iprogram.cpp:464-480`); `toggleNoteMute` a **program** `mute` értékét adja vissza, nem a pad-ét (`:482-487`, hatástalan, mert a hívó nem használja). A getterek `getOrCreate`-tel üres mintát hozhatnak létre (`:118-122, 546-586`).

**MU-H7 — [Track] Mute a Mixerből és a Track Mute-ból különbözően (P1).** Lásd `channel-mixer.md` CM-H11: a Midi nézet közvetlenül írja a `mute` tulajdonságot, így a mute-csoport nem érvényesül.

**MU-H8 — [Mind] Dupla transzport-parancs (P2, GYANÚ).** A híd a PLAY/STOP gombra maga hívja az engine-t (`bridge:921-960`), **és** DCL-csomagot küld (`sendButtonMsg(EN_PLAY…)`), amit a Pad Mute, Track Mute, Pad Mixer, Channel Mixer, Knob Edit `handleCommand`-ja még egyszer végrehajt (`kick_padMute.cpp:119-121`, `kick_trackMute.cpp:124-126`, `kick_PadMixer.cpp:263-265`, `kick_ui_channel_mixer.cpp:225-228`, `kick_KnobEdit.cpp` `handleCommand`). A STOP-párt a motor már védi (`mt_audio_manager_transport.cpp:169-173`: a második `pause()` no-op), a PLAY-párnál (`setSequencePlay` kétszer, `seqRecBeforePlay` kétszer) hatása nem vizsgált. **NEM ELLENŐRIZHETŐ KÓDBÓL**, hogy a második `play()` ugrást/újraarmolást okoz-e.

**MU-H9 — [Mind] Hamis kezelőszervek (P1).** A Settings ablak három kapcsolója és az Instant / Time Division kapcsoló működő-látszó, de hatástalan (2. pont). A beállítások nem íródnak sehová, a Time Division lista nincs bekötve. A Preferences-auditban megfogalmazott elv itt is érvényes: vagy bekötni, vagy eltüntetni.

**MU-H10 — [Mind] Pad-ütés útja törékeny (P2, NEM ELLENŐRIZHETŐ).** A fizikai pad-ütés a pad-csoport komponens `visibilityChanged`-jében csatlakoztatott sigslot-lambdán át ér el az `OnButtonClicked`-hez (`kick_padButtonGroup.cpp:684-742`); a képernyő `handleCommand`-ja pad-üzenetet nem kezel. A `disconnect(handle_Bank)` a sigslot-ban a lambda *típusa* szerint egyez (`M/3rdParty/signal/signal.hpp:296-380, 985-993, 1631-1636`), tehát egy bármely pad-csoport elrejtése az **összes** pad-csoport hardveres lambdáját leköti; képernyőváltás sorrendje (régi el, új be: `kick_AppHome.cpp:359-380`) mellett ez működik, de dupla megjelenéskor (két `visibilityChanged(true)` egymás után) kétszeres csatlakozás → egy ütés két váltás (= nincs változás). Hardveres teszt: egy ütés = egy váltás, több képernyőváltás után.

**MU-H11 — [Track] Kisebb pontok (P2).** `OnButtonClicked` a mute-váltáshoz a **UI** állapotát használja (`!pads->getMute(Id)`, `kick_trackMute.cpp:203`) az engine helyett; a `KickTrackMuteTab::visibilityChanged` nem frissít (`:366-386`), az első feltöltés a szekvencia-választó aszinkron visszahívására épül (`kick_headerBarComponents.cpp`, `setListOptions` → `sendNotificationAsync`); `tracksMax` 0-ról indul, a kattintás-szűrő (`Id >= tracksMax`) addig mindent eldob. A némított pad/track színe piros, a nem némítotté világoszöld, a szólóé kék; a híd az `StateOn` állapotot 30%-os fényerőre tompítja (`pad_colour_map.h:29-49`), a villogó állapotokat nem animálja (`:40`), az Akai viszont narancs/sárga (D-096/D-097) és sárga villogó csoport-jelzést (D-109/D-141) használ.

**MU-H12 — [Track] Szekvencia-választó mellékhatás (P2).** A fejléc választója `ae.setSelectedSequence(Sel)`-et hív (`kick_trackMute.cpp:41-49`): a mute-szerkesztés közben a *globális* kiválasztott szekvencia is vált (Next Sequence nélkül); lejátszás közben a kijelzett és a játszott szekvencia eltérhet (NEM ELLENŐRIZHETŐ, hogy a lejátszás a kiválasztott szekvenciát követi-e).

## 4. Tervben / hiányzik (Akai MPC Standalone-hoz képest)

Forrás: WP-D D-071..D-143, `KEY61_DOC_VS_KICK.md`.
- **Event Mute vs Audio Mute** (kapcsoló, ikonok; MIDI/CV track mindig Event, audio track mindig Audio mute): hiányzik (D-076, D-077, D-094, D-098, D-099). A motorban a némítás mindig „új note nem indul, a lecsengés marad”.
- **Timing Correct / Time Division kvantálás**: a mute az Akai-n a következő időosztásnál lép életbe (1/16 … 2 Bar, triola); a Kick kapcsolója/listája (1/16 … 1) hatástalan, nincs 2 Bar és triola (D-080..D-083, D-120..D-122).
- **Settings**: Track Mute State Per Sequence (a Kickben alapból per-szekvencia), Mute Events Write Automation, Mute and Solo Events are Undoable, Use Track/Pad Colors, **Solo Behavior Single/Multi** (D-087..D-091, D-126..D-128). A Kick rögzített Single, a mute undo-zható (ValueTree), a solo nem.
- **Time Counter**, **Automation gomb** a képernyőn (D-078, D-079, D-118, D-119).
- **Csoport-hozzárendelés gesztus**: az Akai-n előbb a pad, aztán a csoportszám (D-108, D-110, D-140); a Kickben előbb a csoport, aztán a pad (közvetlen hozzárendelés). Zöld/sárga villogó pad-jelzés (D-109, D-141).
- **Pad Mute**: csak Drum track (D-117) — megegyezik. Pad mute csoport = „mute together”, a Kick ugyanezt teszi (`setNoteMute` csoportban).
- **LED**: némított = narancs, nem némított = track-/pad-szín vagy sárga (D-096/D-097/D-132/D-133) — a Kick piros/világoszöld; a Key 61 pad-RGB-je a bridge palettájából jön (`pad_colour_map.h`), a pontos eltérés hardveren nem mért.
- **Track Mute párja a Next Sequence-ben** (queue mute): a Next Sequence képernyő külön audit tárgya.

## 5. Javaslataim

1. **LED-frissítés egy helyen**: a Pad Mute is `refreshAllLEDs()`-et hívjon (a Track Mute mintája), vagy a híd kapja meg az egyedi pad-csomagot (`EN_PAD_SET_SINGLE_PAD`) — az utóbbi minden képernyőt javít, ami `setPadDataMessage`-et használ (pl. `kick_leds.cpp:36-39`).
2. **Mute-szemantika a motorban egy API-n**: `MTAudioInterface::setPadGroup(program, pad, group)` és `setTrackGroup(seq, track, group)` bank-független, tartomány-ellenőrzött; a UI csak ezt hívja; a hívó-oldali indexelés (program vs szekvencia) típusosan szétválasztva (`ProgramIndex` / `SequenceIndex`), hogy MU-H2(a) ne fordulhasson elő.
3. **Clear Solos a motorban**: `resetTrackSolos()` hívja az `updateTrackSoloList()`-et és nullázza a `solo` flageket; a pad-oldal is egységesen (perzisztens solo, vagy dokumentált átmeneti).
4. **Q-Link ↔ Mute**: Q-Link 1–4 a *kijelölt pad/track* csoportszámát állítsa, a Q-Link gomb váltson sort/bankot (QL-07); a None elérhető legyen (0 = None). A Data Dial a kijelölt pad csoportját léptesse (ha nincs fókuszolt vezérlő).
5. **Egyedi, hardver-barát gesztusok**: pad nyomva tartva + Q-Link = csoport; hosszú nyomás = solo (mint az Akai-ban „Mute/Solo” váltó), így nem kell a Solo kapcsoló a kijelzőn.
6. **Hamis kezelőszervek**: az Instant / Time Division bekötése (a Timing Correct tiszta logikája, `mt_timing_correct.h`, újrahasználható: a mute a következő rács-pontnál lép életbe), a Settings ablak valós beállításokkal vagy eltüntetve.
7. **Visszajelzés**: a Track Mute / Pad Mute állapot a Track-lista és a Grid sorfejlécén is jelenjen meg (némított track halvány); a pad-LED fényerő MPC-n dim 30% → a mute-állapotokat nagyobb fényerővel (külön „strong” állapot).
8. **Teljesítmény**: `refreshData()` + `refreshAllLEDs()` minden pad-nyomásra a teljes 16 padot újraszámolja (`getProgramSamples` másolat); az MPC-n elég a változott pad.
9. **Tesztelhetőség**: a mute/solo logikát (egyszeres solo, csoport-propagáció, Clear Solos) a motor-oldalon tiszta, JUCE-független függvényekre bontani, hogy `Kick/tests/platform`-ban futtatható legyen (a `mt_mute_filter.h` mintája).

## 6. Hardveres és unit-tesztek

**Ami ma fedi:** `MuteFilterTests.cpp` (mit enged át a némított track — csak a szűrő), `TimingCorrectTests.cpp` (a kvantálás tiszta logikája, de a mute nem használja), `LedMapTests.cpp`/`PadColourMapTests.cpp` (LED-/szín-térkép). A mute/solo/csoport UI-t és az engine API-kat (`setTrackMute`, `setTrackSolo`, `resetTrackSolos`, `padAddToGroup`, `seqAddTrackToGroup`) **semmilyen teszt nem fedi**. Hardveren igazolt: MUTE/Shift+MUTE képernyőnyitás és LED (RPT L95-96, L113), pad-színek bankonként (RPT L240). Képernyőkép-smoke: `KICK_SMOKE_SCREENS="MenuTrackMute,MenuPadMute"` (vizuális).

**Hiányzik:** engine-szintű teszt a Clear Solos-ra (MU-H3), a csoport-propagációra, a `padAddToGroup` indexelésre; hídszintű teszt, hogy a mute-képernyők által küldött DCL-csomagok mindegyike feldolgozásra kerül (MU-H1); routing-teszt (QL-17).

**MPC-n ellenőrzendők** (`kick_cmd`: `btn MUTE`/`btn TRACK_MUTE`/`btn PAD_MUTE`, `pad <n> <vel>`, `encoder <idx> <delta>`, `snapshot`):
- [ ] Pad-ütés a Track Mute és a Pad Mute képernyőn: **egy** ütés = **egy** váltás; több képernyőváltás után is (MU-H10).
- [ ] Pad-LED színe nyomásra vált-e (Pad Mute: nem fog — MU-H1); Track Mute igen. Fényerő, narancs/piros/zöld szemmel, a `KEY61_LED_COLOURS.md`-hez rögzítve.
- [ ] **Clear Solos** két szólózott trackkel, majd lejátszás: hallható-e a némítás (MU-H3).
- [ ] Mute-csoport: két track azonos csoportban — az egyik némítása a másikat is némítja-e (képernyőről és a Mixerből, MU-H7).
- [ ] Q-Link 1–4: friss képernyőn és érintés után; B bankban melyik pad kap csoportot; a program-index (MU-H2); None-ra visszalépés.
- [ ] Bank E–H (Shift+bank) a mute-képernyőkön; a mini-map kijelzés.
- [ ] Pad Mute nem Drum trackre (Keygroup, Midi) és Clip programra: felirat, LED (MU-H5).
- [ ] Solo pad (Pad Mute) → Pad Mixer: ugyanaz a solo látszik-e; projekt mentés/betöltés után a pad-solo elvész, a mute marad (MU-H6).
- [ ] PLAY/STOP a mute-képernyőkön: a transzport egyszer indul-e/áll-e meg, REC-armolt trackkel is (MU-H8).
- [ ] Mute-állapot és undo: mute-váltás után Undo visszaállítja-e (a szekvenciánkénti állapot).

## 7. TODO

Függőségek: MU-04 a QL-01/QL-07-re, MU-12 a Timing Correct logikára épül.

- [ ] **MU-01** (P0, S) [Track] `MTSequence::resetTrackSolos`: a track `solo` flagek nullázása + `updateTrackSoloList()` — `mt_audio_sequence.cpp:427-434`; host-/engine-teszt (MU-14).
- [ ] **MU-02** (P0, S) [Pad] `padAddToGroup`: a program-index helyes átadása (a track programja / `getSelectedProgram()`), nem a szekvencia-index — `kick_padMute.cpp:455`; típusosítás (lásd 5/2).
- [ ] **MU-03** (P0, S) [Mind] Q-Link csoport: bank-eltolás (`padId`/`trackId`), None (0) elérhető, `refreshData()`+LED-frissítés a hozzárendelés után — `kick_padMute.cpp:449-457`, `kick_trackMute.cpp:401-411`. Függ: QL-02, QL-07.
- [ ] **MU-04** (P0, S) [Mind] Q-Link lépésköz/irány közös réteggel (csoportszám ±1 n-szeresen, jlimit) — QL-02 szerint. Függ: QL-01, QL-02.
- [ ] **MU-05** (P1, S) [Pad] Pad-LED nyomásra: `setPadLED` helyett `refreshAllLEDs()` (vagy a híd kezelje az `EN_PAD_SET_SINGLE_PAD`-et) — `kick_padMute.cpp:231-232`, `bridge:137-160`.
- [ ] **MU-06** (P1, S) [Pad] Clear Solos: null-védelem, felesleges `MTDrumProgram` kasztolás eltávolítása — `kick_padMute.cpp:256`.
- [ ] **MU-07** (P2, S) [Pad] Forrás-egységesítés: minden hely `getProgramForTrack(sel)`-t használjon, a Clip kezelése döntés szerint (Drum+Clip vagy csak Drum) — `kick_padMute.cpp:144-163, 204, 254, 282, 320, 333, 362`.
- [ ] **MU-08** (P1, S) [Track] Mute a Mixerből a `ae.setTrackMute`-on át (mute-csoport) — `channel-mixer.md` CM-10 (itt hivatkozva).
- [ ] **MU-09** (P2, S) [Mind] Dupla transzport-parancs vizsgálata és megszüntetése: a screen-szintű PLAY/STOP/PLAY START kezelők eltávolítása (a híd már végrehajtja), vagy a híd ne küldjön DCL-csomagot — `kick_padMute.cpp:119-121`, `kick_trackMute.cpp:124-126`, `kick_PadMixer.cpp:263-265`, `kick_ui_channel_mixer.cpp:225-228`, `bridge:946-960`; előbb hardveres/injektált ellenőrzés (MU-H8).
- [ ] **MU-10** (P2, S) [Mind] Pad-sigslot robusztussá tétele: a csatlakozás kapcsolata tárolva (`sigslot::connection`), `disconnect` a saját kapcsolaton, duplázás ellen — `kick_padButtonGroup.cpp:684-742` (MU-H10).
- [ ] **MU-11** (P1, M) [Mind] A hatástalan Settings ablak és az Instant/Time Division kapcsoló: bekötés (MU-12) vagy eltávolítás.
- [ ] **MU-12** (P2, L) [Mind] Time Division mute: a váltás a következő rácspontnál lép életbe (1/16 … 2 Bar, triola), a Timing Correct tiszta logikájával; Instant = azonnal — D-080..D-083, D-120..D-122. Függ: `mt_timing_correct.h`.
- [ ] **MU-13** (P2, M) [Mind] Mute/Solo események rögzítése automationbe és az Undo-lista (D-088/D-089/D-126/D-127); pad-solo perzisztencia/undo (PM-16). Függ: automation-terv (T35).
- [ ] **MU-14** (P1, M) Tesztek: engine-szintű tesztek (Clear Solos, csoport-propagáció, `padAddToGroup`/`seqAddTrackToGroup` indexelés) tiszta fejlécekre bontva, `Kick/tests/platform/MuteSoloTests.cpp` + CMake; híd-teszt a DCL-csomag-feldolgozásra (MU-05).
- [ ] **MU-15** (P2, M) Event Mute vs Audio Mute kapcsoló és ikonok (D-076, D-077, D-094, D-098); Solo Behavior Single/Multi (D-091).
- [ ] **MU-16** (P2, S) LED: nagyobb fényerő a mute-állapotoknak, narancs/sárga paletta a hardveres összehasonlítás után (D-096/D-097, MU-H11); villogó állapotok animálása a hídban.
- [ ] **MU-17** (P2, S) Time Counter és Automation gomb a képernyőn (D-078, D-079, D-118, D-119); a szekvencia-választó mellékhatásának tisztázása (MU-H12).
- [ ] **MU-18** (P1, S) MPC-s checklist (6. pont) lefuttatása, rögzítés a `HW_TEST_REPORT`-ban.
