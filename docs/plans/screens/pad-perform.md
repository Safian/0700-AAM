# Pad Perform / Notes / Keyboard Control — review és TODO

Alap: **statikus kódolvasás** (build, futtatás, hardver nélkül), ág `claude/kick-plans` HEAD `40fb2db6`, 2026-11-04. „KÉSZ” = van bekötött handler a hangig/LED-ig, nem hardveres igazolás. A képernyő fejlécében **„Pad Notes”** áll (`PP.h:153`); a doksikban Pad Perform / Notes / Pad Notes néven szerepel — ugyanaz a képernyő (`MenuPadPerform`). A **Keyboard Control menü a Kickben nem létezik**; a KEYBOARD CONTROL gomb ezt a képernyőt nyitja (lásd 1. és 4.).
Útvonal-rövidítések: `PP` = `Kick/modules/kick_ui/modes/padperform/kick_PadPerform` (`.cpp`/`.h`), `M/` = `Kick/modules/`, `A/` = `Kick/apps/`, `EN/` = `Kick/modules/engine/`.

## 1. Áttekintés (mit csinál, hogyan lehet elérni, hardver-gombok kötése)

**Mit csinál.** A 16 hardver-padot (és a képernyős pad-rácsot) hangmagasság-térképpé alakítja, és a padütést a **kijelölt trackre** élő MIDI-note-ként küldi (`ae.addLiveMidiMsg(ae.getSelected(), …)`, `PP.cpp:39-64`). Módok (négy gomb, `PP.cpp:143-178`; a modell hatot ismer: Off, Chromatic, Notes, Chords, Progression, Chromatic Chords — `M/kick_ui_common/source/kick_common_defs.h:81-83`):
- **Chromatic** — a pad-sor félhangonként halad; **Notes** — csak a kiválasztott skála hangjai (Root Note, Scale, Start-on octave); **Chords** — a pad hangjára a lista szerinti akkord épül (`chord` lista: `1-3-5`, `1-4-5`, `1-2-5`, `1-3-5-7`, `1-3-5-7b`, `M/kick_ui_common/source/kick_scales.h:121-123`); **Progressions** — a gomb létezik, de nincs külön viselkedése (lásd B2).
- Beállítások: Root Note (12), Scale (43 skálanév, `kick_scales.h:196-244`), Start on Octave (−2..8), Banks (Continuous / Start on Root), Chords; a beállítások a projektben tárolódnak (`performanceState`: `EN/audio_engine/engine/mt_audio_interface_types.cpp:384-389`, alapértékek: Chromatic, Major, C, −2, „1-3-5”, Continuous).
- Pad-feliratok (hangnév), keret-szín (gyökér/aktív/inaktív) a képernyőn, és pad-szín a hardveren (`pushToUI`/`pushToHW`, `PP.cpp:789-821`; piros = gyökér, sötétsárga = aktív).

**Hogyan érhető el.**
- **KEYBOARD CONTROL gomb** → `sigScreenClose(MenuPadPerform)` (`A/kick/kick_hardware_bridge.h:1284-1291`); Shift+KEYBOARD CONTROL → MIDI Control. MPC-n fizikailag igazolt, hogy a gomb a „Pad Notes”-t nyitja és ez **eltér** a kézikönyvtől (Keyboard Control menü): `docs/AKAI_MPC_KEY61_HW_TEST_REPORT.md:97,115`; a mód-LED követi (`bridge:281`).
- **Shift + 16 LEVEL** → ugyanez a képernyő (`bridge:1137-1142`, DCL `EN_NOTES` → `kick_AppHome.cpp:517`; döntés: marad, `docs/AKAI_MPC_KEY61_TODO.md:13,55`).
- Menü „Padnotes” csempe (`M/kick_ui/dialogs/kick_AppMenuOptions.cpp:42,100`); Program fül `edit` gombja Keygroup / Midi / Cv programnál (`kick_AppProgramComponent.cpp:65-67`); asztali billentyű `]` (`M/contracts/utils/keyboard/keyboard_detector.cpp:158`).
- A sima **16 LEVEL** gomb: DCL `EN_16LEVELS` megy ki (`bridge:1143-1144`), **fogyasztója nincs** (lásd B6).

**Hardver-kötés a képernyőn** (`PadPerform::handleCommand`, `PP.cpp:39-83`):
- **16 pad**: nyomás/felengedés → transzponált note-on/off (`:41-64`); a velocity a híd Full/Half Level logikáján át érkezik (`A/kick/hardware/pad_level.h`, `bridge:1747-1749`; MPC-n igazolt: `known-issues-and-roadmap.md` §1). **Aftertouch nincs** (`kSendPadAftertouchPackets = false`, `bridge:1777`).
- **Bank A–H**: a KickHome állítja (`ae.setBank`, `kick_AppHome.cpp:523-530`), a képernyő a `bankChanged` jelre követi (`PP.cpp:114-120`). A `PadPerformContent::handleCommand` bank-ága (`:351-372`) **halott**, mert a `PadPerform::handleCommand` nem hívja.
- **Q-Link 1–4** (Screen módban): `content.knobCommand` (`:65-71,373-402`) — hibás leképezés, B3. Custom módban a KnobEdit viszi (`kick_AppHome.cpp:561-571`).
- **PLAY / PLAY START / STOP** (`:72-82`). A Data Dial itt nem csinál semmit (nincs ág), csak ha fókuszált lista-vezérlő van (általános ág).
- **Érintés**: a lista-dobozok (Scale/Root/Octave/Chords/Banks) érinthetők, a mód-gombok és a ± bank-gombok működnek; a **képernyős pad-ek érintése nem szólaltat meg hangot** (nincs `OnButtonClicked`/`mouseDown` bekötés a `PadPerformContent` konstruktorában, `PP.cpp:86-327`; ugyanezt rögzíti `04-screen-audit.md` §4).
- **Billentyűzet (61 billentyű)**: *nem* megy át a Pad Perform leképezésen — a híd közvetlenül a kijelölt trackre küldi (oktáv/félhang-transzponálással), `bridge:1817-1843`.

## 2. Funkciók és állapotuk

| Funkció | Állapot | Bizonyíték (fájl:sor) |
|---|---|---|
| Chromatic leképezés (pad → note, bank-eltolással) | KÉSZ | `PP.cpp:722-781` (`updateNoteData`), `:447-477` |
| Notes (skála-szűrt) leképezés, Root/Scale/Octave | KÉSZ (részben: Start-on-Root heurisztika) | `:722-781`; Start on Root: `:743-751` (`noteIndex −= 4` minden 16. padnál), Akai-egyezés NEM IGAZOLT |
| Chords (egy pad → akkord) | BUGOS | `:461-474`; képletek `kick_scales.h:132-138` (B1) |
| Progressions | STUB | gomb `:171-178`; nincs külön ág: `transpose` csak `EN_TYPE_CHORDS`-ra képez akkordot (`:462`), `updateNoteData` a PROGRESSION-t krómatikusnak kezeli (`:740`) |
| Chromatic Chords, Off mód | STUB (modell-érték van, gomb/viselkedés nincs) | `kick_common_defs.h:81-83`, `kick_scales.h:125-127` (`ChromaticChord` lista nincs bekötve, `chordFormulaMap` nem tartalmazza) |
| Root / Scale / Octave / Banks / Chords listák | KÉSZ | `PP.cpp:273-319`; Octave kapu: `Sel <= 10 && bankIndex < 7` (`:286`) → H bankon az Octave nem alkalmazódik (B5) |
| Képernyős bank −/+ | BUGOS | `:127-141` csak a UI-csoportot állítja, `ae.setBank`-ot nem (B4) |
| Pad-feliratok, keretek, HW pad-színek | KÉSZ (két párhuzamos megvalósítás fut) | új: `:789-821`; régi: `:503-616,667-711` (`st_NoteData`, `ae.setKeyGroupPadColour`) — B8 |
| HW pad → élő note (kijelölt track, csatorna 1) | KÉSZ / RÉSZLEGES | `:41-64`; csatorna fix 1 (`:57,59`); felengedés a *pillanatnyi* leképezésből számol (B7) |
| Q-Link 1–4 → lista-léptetés | BUGOS | `:373-402` (B3) |
| 16 LEVEL (utolsó pad 16 változata) | HIÁNYZIK | `EN_16LEVELS` fogyasztó nélkül (`rg EN_16LEVELS` a `modules/`-ban: csak a definíció, a billentyűzet- és chassis-térkép); Shift+16 LEVEL a Notes képernyő |
| Keyboard Control menü (Transpose, MIDI Channel, Aftertouch, Note On Velocity, Scale Filter, Internal/External routing, Velocity Response/Curve/Gain, Black/White Key Sensitivity, Pedal Settings) | HIÁNYZIK | `KEY61_DOC_VS_KICK.md` #19 vezérlő-sor; a híd nem ismeri: `rg 'scale.?filter|velocity.?curve|key.?sensitiv'` a `apps/kick`-ben nulla |
| Oktáv −/+ és félhang (billentyű-transzponálás) | KÉSZ (host-teszt) | `A/kick/hardware/key_transpose.h`, `Kick/tests/platform/KeyTransposeTests.cpp`; a beállítás-ablak (mindkettő + billentyű) nem vizsgált (`KEY61_DOC_VS_KICK.md` #41 vezérlő-sor) |
| Billentyű-aftertouch | RÉSZLEGES | `bridge:1694-1698` (`channelPressureChange(1, val)`, csatorna fix 1, beállítás nincs) |
| Pedál / touch strip Sustain | KÉSZ (host-teszt, részben MPC) | `bridge:1596-1609,1691`; `Kick/tests/platform/SustainTests.cpp`; beállítás-képernyő nincs |
| Képernyős pad-érintés hang | STUB | lásd 1.; `04-screen-audit.md` §4 |
| Perzisztencia (mód/skála/root/oktáv/akkord/bank) | KÉSZ | `mt_audio_interface_types.cpp:384-389`, `PP.cpp:425-445` |
| Mód-LED (KEYBOARD CONTROL világít) | KÉSZ (igazolt) | `bridge:281`; checklist-sor `HW_TEST_CHECKLIST.md:50` |

## 3. Mi nem működik / hibák

**B1 (P1) Az akkord-képletek harmóniailag hibásak.** `kick_scales.h:132-138`: `"1-3-5-7"` = `{5, 7, 11}` és `"1-3-5-7b"` = `{5, 7, 10}` — a „3” helyén 5 félhang (tiszta kvart), nem 4/3 (terc). A hang tehát sus4-maj7 / sus4-7, nem tercépítésű négyeshangzat. Ezen túl **minden** akkord fix félhang-távolságú a pad hangjától (`PP.cpp:469-472`: `idx + i`), tehát a „1-3-5” mindig dúr hármas (`{4, 7}`), akkor is, ha a kiválasztott skálában az adott fokon moll lenne — Chords módban a skála nem hat az akkordminőségre. Az akkord-hangok `idx + i` értéke 127 fölé mehet, és a `juce::MidiMessage::noteOn(1, it, …)` számára érvénytelen (`PP.cpp:57,59`; debug-assert, release-ben maszkolódik → rossz hang). Reprodukció: Chords, `1-3-5-7`, C-dúr, A01 → hangok C–F–G–B (a vártnál E helyett F).

**B2 (P1) A Progression gomb és a Chromatic Chords/Off mód nem létezik viselkedésként.** `PP.cpp:171-178` a PROGRESSION módot állítja, de a `transpose` csak CHORDS-ra bővít akkorddal (`:462`), az `updateNoteData` a PROGRESSION-t krómatikus leképezésnek veszi (`:740`), a `chord` lista viszont Progression alatt is látszik (`:494-495`: csak a nem CHORDS/PROGRESSION típusnál rejti). Az `OFF` (0) és `CHROMATIC_CHORDS` (5) mód nem választható (nincs gomb), de egy projektben tárolva lehet (`refresh()` ekkor egy gombot sem kapcsol → `refreshData()` nem fut, `:439-443`); ilyenkor a `noteData` alapállapotú marad és **minden pad a 0. note-ot (C-2) szólaltatja meg** (`transposeNote` → `activeDisplayIndex = 0`, `:447-451`, `kick_PadPerform.h:48-64` `noteData_t`/`clear()`).

**B3 (P1) Q-Link leképezés hibás.** `PP.cpp:373-402`: 1. Q-Link → `type` lista, de a `type` komponens **nincs elhelyezve** (`resized`, `:329-349`, nem állít neki határt) és nincs `onSelectionChanged`-je → semmit sem csinál; 2. és 4. Q-Link **mindkettő** a `scale`-t léptet (`:380-388`, másolási hiba); az `octave`, `chord`, `startOn` listák csak az 5.–7. gombbal érhetők, amelyek a Key 61-en nincsenek (4 Q-Link). Hatás: oktáv/akkord/bank-indítás hardverrel nem állítható; a Key 61 Q-Linkjei „holtak”/duplák. Az alapértelmezett Q-Link mód a *Screen* (`knobMode{ 1 }`, `M/engine/project/kick_settings.h:218`), tehát ez az ág az alapállapotban él; Custom módban (mód 0) a KnobEdit viszi el a Q-Linkeket, és a képernyő sosem látja őket (`kick_AppHome.cpp:561-571`).

**B4 (P1) A képernyős bank-váltás és a hardver bank szétcsúszik.** `bankInc/bankDec` (`PP.cpp:127-141`) csak `pads->setBank(bankIndex)`-et hív; az engine bankját (`ae.setBank`) nem — szemben pl. a Pad Color-ral (`M/kick_ui/modes/kick_PadColor.cpp:77`) és az Assign Samplel (`kick_AppAssignSample.cpp:735`). A `PadPerform::handleCommand` a pad-számot `ae.getBankOffset()`-tel egészíti ki (`:43,49`). Hatás: érintéssel C bankra lépve a kijelző és a pad-LED a C bank hangjait mutatja (`pushToHW`, `:810-821`, a híd feldolgozza: `bridge:137-160`), de a fizikai padok az engine (A) bankjának hangjait szólaltatják meg. Az UI→hardver bank-parancs (`setBankMessage`) MPC-n eldobódik (`bridge:137-160`).

**B5 (P2) Octave kapu.** `PP.cpp:282-292`: az új oktáv csak `Sel <= 10 && bankIndex < 7` esetén érvényesül — H bankon (bankIndex 7) a lista elmozdul és a modell is frissül (`:285`), de a leképezés nem (`selectedOctave` marad) → kijelzett és tényleges oktáv eltér.

**B6 (P1) 16 LEVEL nem működik.** A híd kiküldi az `EN_16LEVELS` gombot (`bridge:1143-1144,1475-1476`), de a KickHome kapcsolójában nincs ilyen eset (`kick_AppHome.cpp:507-540`), és más fogyasztó sincs → a gomb nem csinál semmit, a LED-je sincs hajtva (`known-issues-and-roadmap.md` H8).

**B7 (P1, ritka) Beragadó hang: a felengedés a pillanatnyi leképezésből számol.** `PP.cpp:49-62`: a note-off a *felengedéskor* érvényes `bankOffset`/`noteData`/akkord-lista szerint képződik, nem a lenyomáskor küldött hangokéból. Ha a nyomva tartott pad közben bankot váltasz, skálát/root-ot/akkordot állítasz (Q-Link/érintés), a note-off más hangra megy → az eredeti hang szól tovább (Keygroup `NOTE_ON` módban hangos). Az engine `ONE_SHOT` mintáknál ártalmatlan.

**B8 (P2) Két párhuzamos leképezés-implementáció.** Új: `updateNoteData` + `pushToUI` + `pushToHW` (`:722-821`) — ez adja a `transposeNote`-ot. Régi: `pushPadInfoToDisplay` + `displayFromNote` + `st_NoteData[128]` + `ae.setKeyGroupPadColour` (`:503-616,667-711`; a gyökér-felismerés a hangnév első betűjével: `:570`, a kijelölt gyökér „C#” esetén a természetes „C”-ket is gyökérnek veszi) — a `selectionChanged` mindkettőt lefuttatja (`:487-501`), majd a `refreshData` újra az újat (`:479-485`): háromszoros DCL pad-szín forgalom és felesleges 16 egyedi pad-parancs változásonként. Halott: `PadPerformContent::handleCommand` (`:351-372`), `Increment/DecrementBankIndex` (`kick_PadPerform.h:120-132`), a kikommentezett oktáv-gombkód (`PP.cpp:180-271`, ~90 sor).

**B9 (P2) Csatorna fix 1.** A pad-note-ok (`PP.cpp:57,59`) és a billentyű-aftertouch (`bridge:1696`) csatorna 1-et használnak; MIDI programnál a program saját csatornájára íródik át (`midi-program-add.md` §2), külső szinti-tartományokhoz (Keyboard Control „MIDI Channel”) nincs beállítás.

**B10 (P2) Teljesítmény/állapot.** `refreshData` → `selectionChanged` → `pushToUI/HW/Display` mindegyik lista- és mód-váltásnál; a Q-Link léptetés (`KickListBox::knobRotated` → `setSelected` értesítéssel, `M/kick_ui_controls/source/kick_listbox.cpp:577-584`) minden egyes tick-re teljes újraszámolást + DCL-csomagokat indít.

## 4. Tervben / hiányzik (az Akai MPC Standalone-hoz képest, a docs-ból ismert tervek)

- **Keyboard Control menü** — owner-döntés (2026-11-02): a Kick-funkciók után, az Akai-rétegben, end-to-end, még nincs kitalálva a mód (`docs/AKAI_MPC_KEY61_MORNING_DECISIONS.md:47,93-96`; `known-issues-and-roadmap.md` §3.1). Tartalom a v3.10-ből (`KEY61_DOC_VS_KICK.md` #19 vezérlő-sor): *Keyboard Input* (Transpose, MIDI Channel, Aftertouch, Note On Velocity), *Scale Filter*, *Internal/External Keyboard Routing*, *Keyboard Settings* (Velocity Response/Curve/Gain, Black/White Key Sensitivity), *Pedal Settings*. A gomb Shift-változata (MIDI Control) már megvan.
- **16 Level** — „az utoljára ütött pad másolata mind a 16 padon, egy paraméter nő (Velocity, Tune, Filter, Layer, Attack, Decay)” (`KEY61_DOC_VS_KICK.md` #30 vezérlő-sor, „nem vizsgált”); TODO T6: a Shift+16 LEVEL a `MenuPadPerform` marad.
- **Notes mód** (skálák/akkordok) „Pad Perform ablak” — a doksi szerint a Shift+16 LEVEL; működése MPC-n „nem vizsgált” (#30 vezérlő-sor).
- **Aftertouch / pad-nyomás** — `kSendPadAftertouchPackets = false`, „reggeli teendő” (`known-issues-and-roadmap.md` §3.4; R2 L38-L40).
- **Touch strip Notes mód** (és Note Strum, Crossfader, Q-Link, Touch FX) — hiányzik (`known-issues-and-roadmap.md` §3.2; `KEY61_TOUCH_STRIP_MODES.md`).
- **ARP/Latch UI** (mód/ráta/oktáv/gate) — az arpeggiator kész, UI nincs (`known-issues-and-roadmap.md` §3.4); a Notes leképezéssel összekapcsolandó (arp a skála hangjain).
- **Érintő-zongora / képernyős pad-játék** — terv (`04-screen-audit.md` §4: per-ujj touch, `KeyboardComponent`, glissando, velocity-mód, skála-jelölés a Pad Perform közös skála-forrásával).
- **Zongora-motor** (`plans/02-piano-clap.md`): a Pad Notes/Chords skálája a közös forrás lenne a zongora-nézettel.

## 5. Javaslataim (improvementek, indoklással)

1. **Tiszta, tesztelhető leképező-mag.** A mostani logika a UI-komponensbe ágyazott és kétszeres. Egy `PadMapper` (bemenet: mód, root, skála, oktáv, start-on, akkord, bank, pad; kimenet: note-lista) JUCE-függetlenül — egy forrása a pad-hangoknak, a pad-feliratoknak, a LED-színeknek és (később) a billentyű-Scale Filternek és a képernyős zongorának. Indoklás: B1/B2/B7/B8 mind ebből fakad, és a 6. fejezet unit-tesztjei csak így írhatók.
2. **Akkordok a skálából**: fokonként egymásra rakott tercek a kiválasztott skálában (diatonikus hármas-/négyeshangzatok); a fix-intervallumú sus-akkordok külön „Chromatic Chords” (a `ChromaticChord` lista már megvan). Indoklás: Chords módban a skála ma nem hat.
3. **Progression**: a skála fokai (I–VII) mint 7 akkord a padokon, a progression mentése/lejátszása későbbi lépés — a gombot addig rejteni vagy szürkíteni (félrevezető kezelőszerv ne maradjon; `04-screen-audit.md` §2.3 elve).
4. **Held-note tábla pad-enként** (pad → küldött hangok), a felengedés ebből — B7 megszűnik, és panic/STOP is tudja a nyitott hangokat.
5. **Q-Link kiosztás a Key 61-hez**: 4 gomb = Scale, Root, Oktáv, Akkord/Mód (a Start-on a listán marad); a nem látható `type` lista eltávolítása.
6. **16 Level mint pad-üzemmód a Pad Perform keretében**: a kiválasztott pad 16 másolata; a „megnövelt paraméter” Velocity / Tune / Layer azonnal megvalósítható; Filter / Attack / Decay csak ha a Drum-hang tényleg olvassa (lásd `program-edit.md` PE-02/PE-21 — függés!).
7. **Keyboard Control menü a híd mögé**: a billentyű-oldali beállítások (csatorna, velocity-görbe, fix velocity, scale filter, aftertouch) a `on_keybed_note_on` (`bridge:1817-1843`) előtt egy tiszta, tesztelhető „KeybedProcessor”-ban (a `KeyTranspose` mintájára, `hardware/key_transpose.h` + `KeyTransposeTests.cpp`). A beállítások a Preferences > Hardware vagy külön képernyő alatt; a KEYBOARD CONTROL gomb a menüt nyitná, a Pad Notes pedig Shift+16 LEVEL (a doksi szerinti kiosztás) — ez a jelenlegi gomb-kiosztás megváltoztatása, **owner-döntés**.
8. **Vizuális visszajelzés**: a nyomva tartott padok és a megszólaló akkord neve a képernyőn (pl. „Cmaj7”), az aktuális skála fokszámai a pad-feliraton; MPC-n a hardver pad-LED a nyomva tartáskor világítson.
9. **Teljesítmény**: debounce a Q-Link léptetésre (≈ 50 ms), a DCL pad-szín csomag változás esetén egyszer küldve; a régi implementáció törlése (B8).
10. **Érintés**: a képernyős pad-rács játszhatóvá tétele (hely-alapú velocity), a per-ujj touch (`04-screen-audit.md` §4) megvalósulásával; addig a rács legyen jelölve „csak kijelző”.

## 6. Hardveres és unit-tesztek

**Ami most fedi:** a Pad Perform leképezésére **nincs** egységteszt. Kapcsolódó host-tesztek: `KeyTransposeTests.cpp` (oktáv/félhang), `PadLevelTests.cpp` (Full/Half Level), `PadColourMapTests.cpp` (pad-szín), `SustainTests.cpp`/`SustainStressTests.cpp` (pedál), `AkaiMapTests.cpp`/`LedMapTests.cpp` (gomb-/LED-térkép; a 16 LEVELS gomb és LED-note megvan: `LedMapTests.cpp:49`), `KeyboardDetectorTests.cpp`. Hardveres igazolás: a KEYBOARD CONTROL → Pad Notes megnyílik, az LED rózsaszínes-piros (`HW_TEST_REPORT.md:97,115`); a pad-ütés egyszeri triggerrel és a pad-színek bankonként (`known-issues-and-roadmap.md` §1). A checklist 16 pad (Pad Perform) és 16 LEVEL sora (`HW_TEST_CHECKLIST.md:13,28`) nincs kipipálva.

**Javasolt egységtesztek (a `PadMapper` kiemelése után):** (1) Chromatic: A01 = C-2, bank-eltolás 16-os lépésekkel, 127 felett nincs hang; (2) Notes, Major C-ben: 8 egymást követő pad C D E F G A B C; Start on Root; (3) Chords: minden képlet intervallumai (terc = 3/4 félhang), 127 fölötti hang eldobása; (4) felengedés = a lenyomáskor küldött hangok halmaza bank-/skálaváltás után is; (5) tárolt `OFF`/`CHROMATIC_CHORDS` mód nem ad „minden pad = 0. note”-ot; (6) Q-Link leképezés (4 gomb → 4 lista); (7) a bank-váltás (UI és HW) ugyanazt az engine-bankot adja.

**MPC-n ellenőrzendő lista:**
- [ ] KEYBOARD CONTROL és Shift+16 LEVEL megnyitja a Pad Notes-t; LED rendben (igazolt: első).
- [ ] Chromatic / Notes (Major, Minor) / Chords: a 16 pad hangnevei egyeznek a hallott hangokkal; a pad-LED a gyökeret pirosra, az aktívat sárgára színezi.
- [ ] Chords `1-3-5-7`: **terc vagy kvart szól-e** (B1); Progression gomb: mi történik (B2).
- [ ] Q-Link 1–4 mit állít (Screen és Custom módban) (B3).
- [ ] Képernyős ± bank után a fizikai pad-ek melyik bank hangját szólaltatják meg (B4); HW bank-gombok után a kijelző követi.
- [ ] Nyomva tartott pad + bank/skála váltás után felengedés: beragad-e a hang (B7).
- [ ] Octave lista H bankon (B5).
- [ ] 16 LEVEL gomb: nincs hatás (B6) — LED állapot.
- [ ] Keygroup / MIDI / CV programon és Drum programon: a transzponált note-ok hová érkeznek (MIDI monitor / hang).
- [ ] Billentyű (61 billentyű) + Pad Notes egyszerre: a billentyű *nem* követi a skálát (vártan).
- [ ] Érintés: lista-popupok bezárása, ± gombok, mód-gombok; a pad-rács érintése nem szól (vártan).

## 7. TODO

Sorrend: hibák → hiányzó funkciók → javaslatok → tesztek. „Függ” = előfeltétel.

- [x] **PP-01** (P1, S) Akkord-képletek javítása: `1-3-5-7` = `{4,7,11}`, `1-3-5-7b` = `{4,7,10}`; 127 fölötti hangok eldobása a küldés előtt — `kick_scales.h:132-138`, `PP.cpp:461-474,52-62`.
- [x] **PP-02** (P1, S) Bank-szinkron: `bankInc/bankDec` és a lista-alapú bank-választás hívja az `ae.setBank`-ot (egy igazságforrás), a `bankChanged` visszahívás ne duplázzon — `PP.cpp:114-141`, minta: `kick_PadColor.cpp:77`.
- [x] **PP-03** (P1, S) Q-Link kiosztás a 4 gombra (Scale, Root, Oktáv, Akkord/Mód), a láthatatlan `type` lista eltávolítása; Custom/Screen módban a viselkedés dokumentálása — `PP.cpp:373-402,329-349`, `kick_AppHome.cpp:561-571`.
- [x] **PP-04** (P1, S) Held-note tábla: a note-off a lenyomáskor küldött hangokra megy — `PP.cpp:39-64`. Függ: –
- [x] **PP-05** (P1, S) Mód-inicializálás megjelenéskor: `OFF`/`CHROMATIC_CHORDS` tárolt módnál is épüljön `noteData` (vagy a mód normalizálása Chromatic-ra) — `PP.cpp:425-451`.
- [x] **PP-06** (P1, M) Progression: megvalósítás (skála-fokok akkordjai) **vagy** a gomb és a `chord` lista elrejtése Progression alatt — `PP.cpp:171-178,462,494-495,740`. Owner dönt.
- [x] **PP-07** (P2, S) Octave kapu javítása (`bankIndex < 7` feltétel kivétele) — `PP.cpp:282-292`.
- [x] **PP-08** (P2, S) MIDI-csatorna: a kijelölt track/program csatornája a fix 1 helyett (pad-note és aftertouch) — `PP.cpp:57,59`, `A/kick/kick_hardware_bridge.h:1696`.
- [x] **PP-09** (P2, M) A régi leképezés-implementáció és a halott kód törlése (`pushPadInfoToDisplay`, `displayFromNote`, `st_NoteData`, `setKeyGroupPadColour` hívás, `PadPerformContent::handleCommand`, kikommentezett oktáv-kód, `Increment/DecrementBankIndex`) **owner jóváhagyásával** — `PP.cpp:180-271,351-372,503-616,667-711`, `PP.h:97,120-132`. Függ: PP-30 (tesztek előbb).
- [x] **PP-20** (P1, L) 16 LEVEL: pad-üzemmód (utolsó pad 16 másolata, léptetett paraméter: Velocity, Tune, Layer azonnal; Filter/Attack/Decay a hang-motor kötése után); KickHome-ág `EN_16LEVELS`-re, LED, kilépés — `kick_AppHome.cpp:507-540`, `bridge:1143-1144`, `KEY61_DOC_VS_KICK.md` #30 vezérlő-sor. Függ: PP-30; Filter/Attack/Decay → `program-edit.md` PE-02, PE-21.
- [ ] **PP-21** (P1, L) Keyboard Control menü — új képernyő (`Screen::MenuKeyboardControl`) + beállítás-tároló; KEYBOARD CONTROL gomb átkötése, Pad Notes marad Shift+16 LEVEL. **Owner-döntés** szükséges (Akai-réteg, `MORNING_DECISIONS.md:93-96`; a gomb-kiosztás változik). Részei:
  - [ ] **PP-21a** (P1, M) Transpose + MIDI Channel + Aftertouch be/ki — `bridge:1817-1843,1694-1698`, `key_transpose.h`. Függ: PP-21.
  - [ ] **PP-21b** (P1, M) Note On Velocity (fix/változó) + Velocity Response/Curve/Gain + Black/White Key Sensitivity — `KeybedProcessor` a `on_keybed_note_on` előtt. Függ: PP-21, PP-30b.
  - [ ] **PP-21c** (P2, M) Scale Filter (billentyű a skálára kvantálva) — közös skála-forrás a PadMapperrel. Függ: PP-30.
  - [ ] **PP-21d** (P2, M) Internal/External Keyboard Routing (belső track vs külső MIDI-port) — `midi-program-add.md` MIDI-kimenet adatverseny-javításával (04 §5.1). Függ: PP-21.
  - [ ] **PP-21e** (P2, M) Pedal Settings (polaritás, funkció) — `bridge:1596-1609,1691`. Függ: PP-21.
- [ ] **PP-22** (P2, M) Pad-nyomás (aftertouch) használata Notes/Chords módban — `kSendPadAftertouchPackets` megoldása (`bridge:1777`) a screen-oldali „csak nyomás” megkülönböztetővel. Függ: R2 „reggeli teendő”.
- [x] **PP-23** (P2, M) Képernyős pad-játék érintéssel (velocity hely alapján) — `04-screen-audit.md` §4 per-ujj touch előfeltétellel.
- [ ] **PP-24** (P2, M) Touch strip „Notes” mód (a Pad Perform közös skála-forrásával) — `known-issues-and-roadmap.md` §3.2.
- [ ] **PP-25** (P2, M) ARP/Latch UI a Notes leképezéssel (arp a skála hangjain). Függ: arp-UI terv.
- [ ] **PP-40** (P2, M) Vizuális visszajelzés: nyomva tartott padok, akkord-név, skála-fokszámok; HW pad-LED nyomva tartáskor.
- [x] **PP-41** (P2, S) Q-Link léptetés debounce + DCL pad-szín csomag csak változáskor — `PP.cpp:479-501,810-821`.
- [x] **PP-30** (P1, M) `PadMapper` kiemelése (JUCE-/DCL-függetlenül) és egységtesztek a 6. fejezet 1–7. pontjaira — `Kick/tests/platform/` (CMake-bővítés).
- [ ] **PP-30b** (P1, M) `KeybedProcessor` (csatorna, velocity-görbe/gain, scale filter) tiszta egységtesztekkel — minta: `KeyTransposeTests.cpp`. Függ: PP-21b.
- [x] **PP-31** (P1, S) MPC hardveres ellenőrzőlista (6. fejezet) a `HW_TEST_CHECKLIST.md`-be + lefuttatás; különösen B1/B3/B4/B7.

### Állapot (S-F, `sonnet/s-f-padperform`)

Kész és host-teszttel (`Kick/tests/platform/PadMapperTests.cpp`, `kick-pad-mapper`) + Mac smoke (`KICK_SMOKE_PROGRAMEDIT=1`, `MenuPadPerform`):
PP-01 (akkord-képletek: terc, 127 fölötti hang eldobva), PP-02 (a motor bankja az egyetlen igazság), PP-03 (Q-Link 1-4 = Scale, Root, Oktáv, Akkord/Mód; `type` lista törölve), PP-04 (held-note tábla, a note-off a lenyomáskor küldött hangokra megy), PP-05 (tárolt Off / Chromatic Chords mód Chromatic-ként játszik), PP-06 (Progression: diatonikus akkordok a skála fokain), PP-07 (az Octave minden bankon érvényes), PP-08 (a pad-note a track kimeneti csatornájára íródik át az engine-ben; az aftertouch ugyanígy, `mt_audio_track.cpp`), PP-09 (a régi leképezés és a halott kód törölve — a Progression/leképezés átírása miatt egyben), PP-20 (16 LEVEL: Velocity és Tune; a Layer / Filter / Attack / Decay a hangmotor hook-ja nélkül nem valósítható meg), PP-23 (képernyős pad-játék per ujj, velocity a pad-on belüli helyből), PP-30 (PadMapper + tesztek), PP-31 (ellenőrzőlista: `AKAI_MPC_KEY61_HW_TEST_CHECKLIST.md` 20-29; lefuttatása MPC-n: ember kell), PP-41 (Q-Link debounce 40 ms, a DCL pad-szín csomag csak változáskor).
PP-40 részben: akkord-/hangnév a képernyőn kész; a skála-fokszám a pad-feliraton és a hardver pad-LED nyomva tartáskor nincs.
Nyitva (nem kóddal dönthető / más hatókör): PP-21, PP-21a-e, PP-30b (Keyboard Control menü: owner-döntés a gomb-kiosztásról), PP-22 (pad-aftertouch: a híd `kSendPadAftertouchPackets` döntése), PP-24 (touch strip Notes mód: híd), PP-25 (ARP UI: terv).
