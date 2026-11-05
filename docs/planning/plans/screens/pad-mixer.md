# Pad Mixer — review és TODO

Állapot: **statikus kódolvasás** (build/futtatás/hardver nélkül), 2026-11-04, ág `claude/kick-plans`. A „KÉSZ” itt azt jelenti: van bekötött handler az engine-ig, nem azt, hogy hardveren igazolt. Ahol a régi docs és a kód ellentmond, a **kód az igazság**. Fájlhivatkozások a repó gyökeréhez képest; `M/` = `Kick/modules/`, `A/` = `Kick/apps/kick/`. Alap: `research/screen-inventory.md` §4.14, `research/known-issues-and-roadmap.md`, `docs/hardware/key61/walkthrough/wp-d.md` (D-045..D-068), `doc-vs-kick.md`, `touch-strip-modes.md`. A két Q-Link-hiba (QL-01, QL-02) közös gyökere a `knob-edit-qlinks.md`-ben van részletezve.

## 1. Áttekintés

**Mit csinál.** Egy Drum/Clip program mintával rendelkező padjaihoz soronként Level, Pan, Solo, Mute (bal oldali sorlista, `ROW_HEIGHT` 107, 590 px széles: `M/kick_ui/modes/mixers/kick_PadMixer.cpp:461`, `kick_PadMixer.h:25`), jobbra 4x4 pad-rács mini mérőkkel (`:296-406`). Fejléc: Program- és Track-választó, „pad rács be/ki” gomb (`padEnabler` `:172`), „Sample Assign” gomb (`:176-179`, bezár, majd megnyitja a `MenuAssignSample`-t). Nem Drum/Clip programnál tájékoztató felirat („Pad Mixer Is Not Applicable For … Program”, `:422-428, 448-449`).

**Elérés.**
- Menü → „Padmixer” (`M/kick_ui/dialogs/kick_AppMenuOptions.cpp:33,91`).
- MPC Key 61: **Shift+MIX** (`HW_BTN_TRACK_MIX` + Shift → `MenuPadMixer`, `A/kick_hardware_bridge.h:1243-1251`); a `HW_BTN_PAD_MIX` ág is ide vezet (`:1252-1256`); a DCL-kód `EN_PAD_MIX` → `kick_AppHome.cpp:503`. Mód-LED: MIX gomb, „B” szín (piros) (`bridge:274`). Hardveren igazolt: képernyőnyitás és LED (RPT L93-94, L112: „MIX / +Shift: fehér / piros OK”).

**Hardver-kötések a képernyőn.**
| Vezérlő | Mit tesz itt | Kód | Hardveres állapot |
|---|---|---|---|
| Pad (16) | élő MIDI a **kiválasztott trackre** (`noteOn`/`noteOff`, `ae.getBankOffset()+pad`) + a pad kijelölése a rácson/sorlistán | `kick_PadMixer.cpp:229-255, 680-686` | pad-ütés igazolt (RPT L119); a Pad Mixeren belüli kijelölés nem mért |
| Bank A–D, Shift: E–H | `ae.setBank` (KickHome), a `bankChanged` jel frissíti a rácsot | `kick_AppHome.cpp:523-530`, `kick_PadMixer.cpp:396-405` | A–D LED igazolt; E–H és a görgetés nem mért |
| Q-Link 1–3 | 1 = Level, 2 = Pan, 3 = Mute/Solo (az **első látható sorra**); 4 = nincs | `:117-128, 628-678` | **nem működik helyesen** (3. pont) |
| Data Dial, −/+, ENTER | az utoljára megérintett vezérlőt állítja (pl. A01 Level) | `M/kick_ui_controls/source/kick_ControlBase.h:126, 205-209` | **igazolt** (0a42bfdb: A01 Level 1,00 → 0,95 öt kattanásra) |
| Touch Strip „Pad Level” mód | az utoljára ütött pad gain-je (0..127 → `midiToGain`); a képernyő 25 Hz-en követi (`syncLevels`) | `bridge:1611-1623`, `kick_PadMixer.cpp:541-568` | injektált értékkel igazolt (A02 0,70 → 0,87, TO L15); fizikai ujj nem |
| PLAY / PLAY START / STOP | `setSequencePlay` / `setSequencePause` | `kick_PadMixer.cpp:256-267` | nem mért ezen a képernyőn |
| Full / Half Level | a pad-sebesség 127/64 (a híd fixálja) — a Pad Mixer nem tud róla | `A/hardware/pad_level.h` | igazolt (0947f1f5) |
| Main Volume | a master busz (Channel Mixer master csík), **nem** a Pad Mixer | lásd `channel-mixer.md` | igazolt (898a122a) |
Érintés: koppintás kijelöl (piros keret), csúszka ujjal húzható (0a42bfdb), dupla koppintás a padon → Sample Assign lista (`:370-384`, igazolt: 42cfdc3c).

## 2. Funkciók és állapotuk

| Funkció | Állapot | Bizonyíték |
|---|---|---|
| Level (pad gain) csúszka, érintés / Data Dial | KÉSZ (MPC-n mérve: Data Dial) | `kick_PadMixer.cpp:65-72`; 0a42bfdb |
| Pan csúszka, érintés / Data Dial | KÉSZ (kód) | `:74-81` |
| Mute | KÉSZ (kód); a solo-nak engedő logika (`mutedDueToNoteSolo`) UI-szintű | `:48-63, 588-617` |
| Solo (pad) | RÉSZLEGES: a solo-állapot `MTIProgram::soloingNotes` bitkészlet, **nem perzisztens, nem visszavonható** | `:38-46`; `M/engine/audio_program/source/mt_audio_iprogram.h:227-229` |
| Pad-rács (név, pan, level, mute/solo szín) | KÉSZ, de a szöveg nyers `double` + „ db” | `:506-516` |
| Pad-mérők | BUGOS GYANÚ: minden padon a **program** kimeneti mérője, jobb csúcs = bal csúcs | `:527-539` (`:532` TODO, `:536`) |
| Program/Track választó | KÉSZ; a program-váltás globálisan átállítja a kiválasztott programot | `:181-196` |
| Q-Link 1–2 (Level, Pan) | BUGOS: egy kattanás = teljes skála; érintés után egyáltalán nem érkezik | 3. pont: PM-H1, PM-H3 |
| Q-Link 3 (Mute/Solo) | BUGOS: csak a gomb képe vált, az engine nem | `:123-126` (PM-H2) |
| Q-Link 4 | STUB (nincs eset) | `:117-128` |
| Valós idejű követés (Strip Pad Level) | KÉSZ (kód + injektált mérés) | `:541-568`; 88e6670c |
| Sample Assign (dupla koppintás / gomb) | KÉSZ (igazolt dupla koppintás) | `:370-394, 176-179` |
| Sor kijelölése → pad-bank váltás | BUGOS GYANÚ: csak a kijelzett bank vált, az engine-bank nem (PM-H5) | `:352-358` |
| „Pad rács be/ki” | KÉSZ | `:172-174, 456-472` |
| Hardveres pad → kijelölés a rácson | KÉSZ (kód); igazolatlan (D-051) | `:680-686` |
| Send / Insert / MIDI Out / Audio Out per pad | HIÁNYZIK (D-063..D-068) | lásd 4. pont |
| Keygroup / Plugin program támogatás | HIÁNYZIK (a képernyő „nem alkalmazható”-t ír) | `:422` |
| Halott kód: `MixerTabView`, `PadMixersDrumProgramHandler`, `PadMixerKeygroupProgramHandler` | STUB / nem használt (csak include) | `M/kick_ui/kick_ui.h:120-121`, `modes/mixers/kick_PadMixerDrumProgram.h`, `kick_PadMixerKeygroupProgram.h`, `kick_MixerTabView.*` (külső hivatkozás nincs) |

## 3. Mi nem működik / hibák

**PM-H1 — Q-Link 1/2 lépésköze hibás (P0).** `PadMixerDisplay::linkRotated` a csúszkák `knobRotated(value)`-ját hívja a **nyers** számlálóval (`kick_PadMixer.cpp:121-122`), a `KickSlider::knobRotated` pedig ezt egyszerűen hozzáadja az értékhez (`M/kick_ui_controls/source/kick_slider.cpp:128-134`). A Level tartománya 0..1 (lépés 0,01) és a Pan −1..1 (`:33-34`, `AudIO/AudIO/Includes/Constants.h:38-39`), a Q-Link delta ±1 (relatív CC, `A/kick_hardware_bridge.h:1530-1542`). Egy kattanás tehát a teljes skálát bejárja (0 ↔ 1, −1 ↔ 1). A Data Dial ugyanezt a hibát az 0a42bfdb-ben `* getIncrement()`-tel kapta javítva (`kick_ControlBase.h:126`); a Q-Link-ág kimaradt. Hatás: MPC-n a Pad Mixer Q-Link 1–2 használhatatlan. Reprodukció: `echo "encoder 0 1" > /data/kick_cmd` a Pad Mixeren (a híd `on_encoder_turn` útja azonos a fizikaival). Megoldás: QL-02.

**PM-H2 — Q-Link 3 (Mute/Solo) nem hat az engine-re (P0).** `mute.setToggleState(Value > 0, dontSendNotification); solo.setToggleState(Value <= 0, …)` (`:123-126`): a `dontSendNotification` miatt a `onClick` (`:38-63`) nem fut le, tehát a `setNoteMute`/`setNoteSolo` nem hívódik; jobbra tekerés „mute” képet, balra „solo” képet kapcsol. A T11 megjegyzés (`docs/hardware/key61/todo.md` T11) pont ezt a képi billenést írja le „káros nulla-deltaként”, de nem vette észre, hogy az engine soha nem értesül. Ugyanez a minta még 7 helyen (Channel Mixer: `channel-mixer.md` CM-H3). Megoldás: QL-03.

**PM-H3 — Q-Link érintés után halott (P0, magas valószínűség).** `KickHome::handleCommand`: ha van „kijelölt vezérlő” (`ui::getComponentInFocus()`), **minden** navigációs üzenet (a `isKnobMessage` is, tehát a Q-Link is; csak a pad-üzenet kivétel) ahhoz megy, és `else if`-ágban a képernyő `handleCommand`-ja nem kap semmit (`M/kick_ui/dialogs/kick_AppHome.cpp:573-580`). A vezérlő `handleCommand`-ja a `EN_KNOB_1..16` ágat szándékosan eldobja („Nothing to be done for link knobs…”, `kick_ControlBase.h:98-121`). Az MPC hídja minden érintésnél beállítja a kijelölést (`bridge:561` `selectControlFromTouch`), a képernyőváltás törli (`:789`, `kick_AppHome.cpp:346-355`). Eredmény: a Pad Mixer megnyitása után a Q-Link csak addig működne, amíg nem érintettél meg egy csúszkát/gombot. Következtetés a 0a42bfdb commit-üzenetéből: a hiba e commit óta áll fenn (előtte a JUCE-fókusz hiánya miatt `getComponentInFocus()` az X nélküli MPC-n null volt, a Q-Link tehát a képernyőig jutott). Hardveren nem mértem. Reprodukció: `touch <x> <y>` egy Level csúszkára, majd `encoder 1 1`. Megoldás: QL-01.

**PM-H4 — A Q-Link az „első látható sorra” hat, és a `knobCommand` védtelen (P0).** `PadMixerContent::knobCommand` (`:628-678`): `rowComp[0]` ellenőrzés nélkül (`:632`; 0 minta, vagy nem alkalmazható program esetén üres lista → UB/összeomlás); `rowIndex = rowList[i] - 1` alsó határ nélkül (`:640`; `getRowNumbers` -1-et is adhat); a `getElementBounds(state)` hozzárendelése eltolt (1 → pan, 2 → mute, 3/4 → level, `:130-139`), így a Shift-os oszlopkiemelés rossz helyre kerül; a 4. Q-Link-nek nincs hatása. MPC-n az 1–4 gomb a **négy Q-Link-sor első sorára** korlátozódik (a Behringer 16 gombja 4 sort fedett), a kijelölt padra/sorra nem. Megoldás: PM-03, QL-07.

**PM-H5 — A bank-állapot kettéhasad (P1, GYANÚ).** Sor-kijelölésnél `pads->setBank(bankIndex)` (`:352-358`) csak a komponens bankját állítja és DCL-csomagot küld (`M/kick_ui_components/source/kick_padButtonGroup.cpp:82-114`), amit a híd eldob (csak az aftertouch-engedélyt és a pad-színt dolgozza fel: `bridge:137-160`). `ae.setBank` nem hívódik. A fizikai pad-ütés (`ae.getBankOffset()+pad`, `kick_PadMixer.cpp:235-241, 684`) és a Strip Pad Level (`bridge:1622`, `padNote(ae.getBankOffset(), pad)`) ezért a **régi** bankra megy, míg a kijelzett bank a másik. Csak érintéssel való bankváltásnál jelentkezik; a hardveres bankgombnál nem (az `ae.setBank` + `bankChanged` jel megy). Hardveres ellenőrzés kell.

**PM-H6 — Pad-mérők hamisak (P1).** Minden pad a program kimeneti mérőjét mutatja (`:532` „TODO - Need to call different API for Pad”), és a jobb csúcsnak a bal csúcsot adja (`:536`: `outMeter[0].peak, outMeter[0].peak`, a jobb `[1]` kellene). A Channel Mixerben a csúcs-átadás ugyanígy (CM-H9). Mellékhatás: a mérőt nem kapcsolja be a képernyő (`enableOutputMetering` nincs hívva); csak azért kap adatot, mert `iSynthObject::enableOutputMetering(bool)` az argumentumát figyelmen kívül hagyja és mindig `true`-t ír (`M/engine/audio_program/synth/iSynthObject.h:45`, `audioPreview.h:52`) — a mérés tehát soha nem kapcsol ki (CPU).

**PM-H7 — Hamis dB-felirat, nyers számok (P1).** A Level csúszka lineáris gain 0..1, de „ db” utótagot kap (`:35`), a rács sora `String(double)` + „ db” (`:509-510`; nyers lebegőpontos kiírás, pl. `0.300000011920929`). A Channel Mixer 0..127-es skálát (`KickLevelSlider`), a Strip 0..127 → `midiToGain`-t használ (`M/common/source/kick_common_utils.cpp:48-56`), tehát három egymással nem egyező skála van; az eredeti Kick skálája nem ismert (a `Constants.h` „API ranges required by the released UI” rekonstrukció).

**PM-H8 — Pad-üzenet kezelés (P2).** `padState` se PRESSED, se RELEASED esetén üres `juce::MidiMessage` kerül az `addLiveMidiMsg`-be (`:244-252`; ugyanez a Channel Mixerben, `kick_ui_channel_mixer.cpp:227-232`). A pad `OnButtonClicked` a sigslot-útról (`kick_padButtonGroup.cpp:684-731`) és a `PadMixerContent::handleCommand`-ból (`:684-686`, press **és** release) is lefut: kétszeri kijelölés, a sorlista ugrálhat.

**PM-H9 — Program ≠ kiválasztott track (P2, NEM ELLENŐRIZHETŐ KÓDBÓL).** A Pad Mixer program-választója `ae.setSelectedProgram(Sel)`-et hív (`:181-186`), a pad-ütés viszont `ae.getSelected()` trackre megy (`:252`). Hogy a `setSelectedProgram` állítja-e a kiválasztott trackot, a motorban nem követtem; ha nem, a pad más hangot szólaltat meg, mint amit a képernyő mutat.

**PM-H10 — Mintát hozó getterek (P2).** `MTIProgram::getNoteGain/getNotePan/isNoteMute/getNoteMuteGroup` `getOrCreate`-et hívnak (`mt_audio_iprogram.cpp:118-122, 546-586`), tehát üres padra hívva üres `MTISample` jön létre a projektben. A Pad Mixer maga csak mintás padokra hív, de a Q-Link „Custom” út (QL-04) igen.

**PM-H11 — Teljesítmény (P2, NEM MÉRT).** Minden mintás pad külön, teljes sor-komponens (2 csúszka, 2 gomb, 2 címke), akár 128 sor; a 25 Hz-es időzítő (`:288`) mintánként `syncLevels` + mérő-frissítést futtat. ARMv7-en mérni kell (CPU, bejáráskor késés).

## 4. Tervben / hiányzik (Akai MPC Standalone-hoz képest)

Forrás: WP-D (D-045..D-068) és `doc-vs-kick.md`. A Pad Mixer soronként nem 8 csík, hanem sorlista (D-049, döntés).
- **Program-típusok**: az Akai drum- és Keygroup-sávra is ad Pad Mixert (D-046); a Kick Keygroupra „nem alkalmazható”. A zongora/Keygroup irány (Terv 02) miatt ez fontos hiány.
- **Per-pad Sends fül** (D-063) és **Effects/Inserts** (D-064): nincs vezérlő; az engine per-pad send/insert támogatása **nem vizsgált**.
- **MIDI Out** (Enabled/Note/Channel, D-065..D-067) és **Audio Out** (Track vagy közvetlen kimenet, D-068): nincs.
- **Mixer Settings ikon**: Filter by events / Filter by samples / Write automation when recording (D-054..D-057); a Kick csak a mintás padokat mutatja, kapcsoló nélkül (D-056).
- **Nagy csúszka dupla érintésre** (D-060), automation gomb (D-022 analóg), pad kijelölése hardveres pad megérintésével (D-051, nem mért).
- **Q-Link**: az Akai Q-Link-gombja oszlopot (bankot) vált (`doc-vs-kick.md` L12 — oszlopváltás „nem vizsgált”); a Pad Mixer Q-Link-térképe az Akai-n nem ismert a repóban (WP-H nincs elkészítve).
- **Pad színek / LED**: az Akai-ban a pad-LED a mixer-állapotot követi; a Pad Mixer jelenleg nem küld mute-szín csomagot (a híd saját idle-színét használja).

## 5. Javaslataim

1. **Q-Link „kijelölt sor” modell** (UX, MPC): a 4 Q-Link a **kijelölt pad** Level, Pan, Mute, Solo vezérlője legyen (nem az első látható soré), a Q-Link gomb (később) a paramétercsoportot váltsa. Egyszerűbb, mint a 16 gombos rács emulálása négy gombra, és nem kell `getRowNumbers`-trükk (PM-03, QL-07).
2. **Közös Q-Link lépésköz-réteg** (QL-02): egyetlen hely számolja át a számlálót a vezérlő skálájára (gain/pan 0,01 × n, CC 1 × n, kapcsoló iránya), így a Pad Mixer, Channel Mixer, Knob Edit és Mute képernyők nem másolják a hibát.
3. **Egységes dB-megjelenítés** (PM-H7): gain → dB (`20·log10`), „−∞” 0-nál, egy tizedes; döntés kell, hogy a mixerek 0..127 (MIDI) vagy dB skálát mutatnak.
4. **Mérők**: a per-pad mérő vagy tényleges (engine-API a padhoz), vagy ne jelenjen meg; a csúcs-átadás javítása; a mérő be/ki kapcsolása a láthatósággal (ARM CPU).
5. **Bank-szinkron**: a sor-kijelölés hívja `ae.setBank`-ot (vagy a rács kövesse az engine-bankot), hogy a fizikai pad és a kijelzés mindig ugyanarra a bankra mutasson.
6. **Data Dial a sorlistán**: ha nincs kijelölt vezérlő, a tárcsa léptesse a pad-sorokat, ENTER nyissa a Sample Assign-t (jelenleg a `PadMixerContent::handleCommand` a középső gombot eldobja: `:691`).
7. **Érintésbarát méretek**: a 107 px-es sorok és a 70 px-es csúszkák ujjal használhatók, de a Sample Assign dupla koppintása ujjal nehéz (450 ms / 24 px ablak, 42cfdc3c) — külön „Assign” gomb a sorban.
8. **Halott kód törlése** (`MixerTabView`, két `*ProgramHandler`): fordítási idő és zavar.
9. **Q-Link visszajelzés** a képernyőn: az éppen hatott paraméter kiemelése (az Akai „Show Q-Link Status When Touched” analógja, G-168).

## 6. Hardveres és unit-tesztek

**Ami ma fedi:** semmilyen host-teszt nem hivatkozik a Pad Mixerre (`Kick/tests/platform/*` átnézve). Közvetett: `PadLevelTests.cpp` (Full/Half Level), `StripModesTests.cpp` (strip→érték), `RelativeCcTests.cpp` (Q-Link/Data Dial dekódolás), `AkaiMapTests.cpp` (gomb-térkép). Képernyőkép-smoke: `KICK_SMOKE_SCREENS="MenuPadMixer"` + `KICK_SMOKE_SHOTS_DIR` (TODO.md L78-82), csak vizuális. Hardveren igazolt: képernyőnyitás+LED (RPT L93-94), Data Dial → A01 Level (0a42bfdb), injektált Strip Pad Level (TO L15).

**Ami hiányzik:** a Q-Link-út (routing + lépésköz + kapcsoló), a `knobCommand` határesetei (üres lista), a bank-szinkron, a mérők.

**MPC-n ellenőrzendők** (a `kick_cmd` FIFO-val injektálva is: `encoder <idx> <delta>`, `touch <x> <y>`, `pad`, `btn <NÉV>`, `snapshot`; bridge `:1940-2040`):
- [ ] Shift+MIX → Pad Mixer, LED piros; MIX (Shift nélkül) → Channel Mixer.
- [ ] Q-Link 1/2/3 forgatás **érintés nélkül** és **érintés után**: Level/Pan lépése, Mute az engine-ben (hangban hallható-e) (PM-H1..H3).
- [ ] Üres programnál / Keygroupnál Q-Link forgatás: nincs összeomlás (PM-H4).
- [ ] Bankváltás érintéssel (sor kijelölése B+ padon), majd fizikai pad: melyik bank szól (PM-H5).
- [ ] Strip Pad Level fizikai ujjal: a csúszka és a rács követi, a pad hangereje változik.
- [ ] Pad-mérők: egy pad ütésére csak az a pad mutasson (PM-H6) — jelenleg mind mutat.
- [ ] Sok mintás program (≥ 64 pad) megnyitási ideje és a 25 Hz-es frissítés CPU-terhelése (PM-H11).
- [ ] Solo/mute: Pad Mixer solo → Pad Mute képernyőn ugyanaz látszik; projekt mentés/betöltés után a solo elvész (2. pont, Solo sor: nem perzisztens bitkészlet).

## 7. TODO

Függőségek: PM-01..PM-03 a `knob-edit-qlinks.md` QL-01/QL-02/QL-03 javítására épülnek.

- [x] **PM-01** (P0, S) Q-Link 1–2 lépésköz: a Pad Mixer csúszkái a közös Q-Link-rétegen (QL-02) át kapjanak lépést (`knobRotated(n * getIncrement())`) — `kick_PadMixer.cpp:117-128`. Függ: QL-02. — KÉSZ (S-A, kód + host-teszt/Mac-smoke; hardveren nem igazolt)
- [x] **PM-02** (P0, S) Q-Link 3: valódi `setNoteMute` / `setNoteSolo` hívás (irány: jobbra be, balra ki; Shift+tekerés = solo) a `dontSendNotification` helyett — `:123-126`. Függ: QL-03. — KÉSZ (S-A, kód + host-teszt/Mac-smoke; hardveren nem igazolt)
- [x] **PM-03** (P0, M) `knobCommand` újraírása: a kijelölt pad sorára hat (nem az első láthatóra), üres lista / −1 index védelem, 4. Q-Link kiosztása (pl. Send vagy Solo), `getElementBounds` helyes leképezés — `:628-678, 130-139`. Függ: QL-01, QL-07. — KÉSZ (S-A, kód + host-teszt/Mac-smoke; hardveren nem igazolt)
- [x] **PM-04** (P1, S) Mérők: jobb csúcs `[1]` (`:536`), per-pad mérő megvalósítása vagy elrejtése, `enableOutputMetering` kezelése a láthatóság szerint; `iSynthObject::enableOutputMetering` argumentumának tiszteletben tartása (`iSynthObject.h:45`, `audioPreview.h:52`). — S-A: jobb csúcs [1], argumentum tiszteletben tartva, per-pad mérő elrejtve (nincs engine API)
- [x] **PM-05** (P1, S) Egység és formátum: dB-átszámítás és egy tizedes a Level feliratra (`:35, 509-510`); owner-döntés: MIDI vs dB skála egységesen a mixerekben. — S-A: dB + egy tizedes kész; a mixerek közös MIDI/dB skálája owner-döntés marad
- [x] **PM-06** (P1, M) Bank-szinkron: `provider.onRowSelected` hívjon `ae.setBank`-ot (vagy a rács kövesse az engine-bankot) — `:352-358`; hardveres ellenőrzés után. — KÉSZ (S-A, kód + host-teszt/Mac-smoke; hardveren nem igazolt)
- [x] **PM-07** (P2, S) Üres `MidiMessage` védelem a pad-üzenetnél (`:244-252`) és a dupla kijelölés megszüntetése (`:684-686`, csak PRESSED-nél). — KÉSZ (S-A, kód + host-teszt/Mac-smoke; hardveren nem igazolt)
- [x] **PM-08** (P2, S) Getterek mellékhatása: üres padra ne jöjjön létre minta (`getOrCreate` csak íráskor) — `mt_audio_iprogram.cpp:118-122, 546-586`. — KÉSZ (S-A, kód + host-teszt/Mac-smoke; hardveren nem igazolt)
- [ ] **PM-20** (P2, S) Dupla transzport-parancs: a Pad Mixer PLAY/PLAY START/STOP kezelője (`kick_PadMixer.cpp:256-267`) megismétli, amit a híd már végrehajtott (`bridge:921-960`) — vizsgálat és eltávolítás a `MU-09` szerint.
- [ ] **PM-09** (P1, M) Keygroup programok támogatása a Pad Mixerben (a `:422` korlát feloldása, billentyű-tartomány szerinti sorok). Függ: Terv 02 döntés.
- [ ] **PM-10** (P2, L) Per-pad Sends / Inserts / MIDI Out / Audio Out (D-063..D-068); előbb az engine támogatását kell felmérni.
- [ ] **PM-11** (P2, M) Mixer Settings: Filter by events/samples, Write automation (D-054..D-057); függ: Terv 04 automation (T35).
- [x] **PM-12** (P2, S) Nagy csúszka dupla érintésre (D-060), külön „Assign” gomb a sorban. — S-A: külön Assign gomb a sorban kész; a nagy csúszka dupla érintésre (D-060) nincs
- [x] **PM-13** (P2, S) Data Dial/ENTER a sorlistán, ha nincs kijelölt vezérlő (`:691`). — KÉSZ (S-A, kód + host-teszt/Mac-smoke; hardveren nem igazolt)
- [ ] **PM-14** (P2, M) Teljesítmény: sorok virtualizálása / csak a látható bank mérői, mérés ARMv7-en (PM-H11).
- [x] **PM-15** (P2, S) Halott kód törlése: `kick_MixerTabView.*`, `kick_MixerHandlerInterface.h`, `kick_PadMixerDrumProgram.h`, `kick_PadMixerKeygroupProgram.h`, `kick_ui.h:120-121`. — KÉSZ (S-A, kód + host-teszt/Mac-smoke; hardveren nem igazolt)
- [ ] **PM-16** (P2, M) Solo-állapot perzisztencia és visszavonhatóság (nem a `soloingNotes` bitkészletben) — owner-döntés (Akai: „Mute and Solo Events are Undoable”, D-127).
- [x] **PM-17** (P1, M) Host-teszt a közös Q-Link-lépésközre és mute/solo-irányra (`Kick/tests/platform`, lásd QL-17); a Pad Mixer-specifikus rész: `padLevel` skála-átváltás (gain ↔ MIDI ↔ dB). — S-A: tiszta logika host-tesztje kész (MixerLogicTests); a gain/MIDI/dB körút benne
- [x] **PM-18** (P1, S) Képernyőkép-smoke bővítés: `MenuPadMixer` injektált `touch`+`encoder` sorozattal (QL-01 regresszió). — KÉSZ (S-A, kód + host-teszt/Mac-smoke; hardveren nem igazolt)
- [ ] **PM-19** (P1, S) MPC-s checklist (6. pont) lefuttatása és rögzítése a `HW_TEST_REPORT`-ban.
