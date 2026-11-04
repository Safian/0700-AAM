# Knob Edit / Q-Links — review és TODO

Állapot: **statikus kódolvasás** (build/futtatás/hardver nélkül), 2026-11-04, ág `claude/kick-plans`. „KÉSZ” = van bekötött handler az engine-ig, nem hardveres igazolás. Ahol a régi docs és a kód ellentmond, a **kód az igazság**. `M/` = `Kick/modules/`, `A/` = `Kick/apps/kick/`, `KE/` = `Kick/modules/kick_ui/modes/knobedit/`. Alap: `research/screen-inventory.md` §4.16, `research/known-issues-and-roadmap.md` §3.2/§3.4, `KEY61_DOC_VS_KICK.md` (Q-Link: L12 „nem vizsgált”), `KEY61_TOUCH_STRIP_MODES.md` §3 (Q-Link strip-mód), `WP-D.md`, `WP-G.md` (G-118, G-134, G-135, G-140, G-168, G-169, G-220..G-222). **A Q-Link-ágat érintő három közös hiba (QL-01 routing, QL-02 lépésköz, QL-03 mute/solo) itt van részletesen; a `pad-mixer.md`, `channel-mixer.md` és `pad-mute-track-mute.md` ide hivatkozik.** A WP-H (Q-Link Edit az Akai útmutatóban) a repóban nincs elkészítve.

## 1. Áttekintés

Két réteg van, ezeket külön kell nézni:

**(A) A Q-Link hardver és a csomag útja a képernyőkig (globális).**
- Hardver: Q-Link 1–4 = CC 16–19 relatív (`0x01` = +1, `0x7F` = −1), ch1, Port 1; érintés = Note 84–87 be/ki (mért, `KEY61_HW_BEHAVIOUR.md` L16). A Q-Link LED-sornak nincs szoftveres funkciója (az útmutató „Light LED”, `AKAI_MPC_KEY61_HW_TEST_REPORT.md` L191).
- Adapter: `A/hardware/adapters/akai_mpc_key61_adapter.h:472-475` → `on_encoder_turn(idx 0..3, delta)`; a 0 lépés nem megy tovább. Híd: `A/kick_hardware_bridge.h:1530-1542` → DCL `EN_PKT_TYPE_KNOBS / EN_KNOB_QLINK`, `EN_KNOB_1 + idx`, irány, `abs(delta)`. Érintés: `on_encoder_touch` **szándékosan üres** (`:1544-1549`, T11).
- Elosztás: `DclSignals::dispatchHardwareInput` (modális „owner”-ek előbb) → `hardwareInputTriggered` → `KickHome::handleCommand` (`M/kick_ui/dialogs/kick_AppHome.cpp:561-580`):
  1. **Custom mód** (`knobControl.getMode() == 0`): minden Q-Link a `MenuKnobEdit` `knobCommand`-jára megy (`:561-570`).
  2. **Screen mód** (alapérték 1, `M/engine/project/kick_settings.h:213-219`; nem perzisztens): ha van „kijelölt vezérlő”, a Q-Link **ahhoz** megy, ami eldobja; különben az aktuális képernyő `handleCommand` → `knobCommand` (`:573-580`).
- Q-Link **gomb**: Shift nélkül `EN_LINK` nyomás → `KnobEdit::knobLongPress()` (`kick_AppHome.cpp:541-542`); Shift+Q-LINK → `MenuKnobEdit` megnyitása (`bridge:1195-1202`); a `EN_LINK_EDIT` DCL-kód is ide vezet (`kick_AppHome.cpp:514`). Mód-LED: QLINK világít a Knob Edit képernyőn (`bridge:286`).
- Menü: „Qlinks” csempe → `MenuKnobEdit` (`kick_AppMenuOptions.cpp:44,102`).

**(B) A „Q-Link Edit” képernyő** (`KE/kick_KnobEdit.cpp`): fent track-lista + program-típus címke, balra a 4x4 `KnobGrid` (16 forgatógomb, `KE/kick_KnobGrid.cpp`), jobbra a tartalom; alul „Custom” / „Screen” kapcsoló. *Screen* módban a jobb oldal üres (`Content` csak háttér, `kick_KnobTabs.cpp:4-17`), a felirat: „Q-Link controls are fixed to the mode you are in”. *Custom* módban `CustomTab`: PROGRAM/PAD váltó + bank + pad (Drum/Clip), Mix / Program / Ins1-4 / MIDI CC gombok, alul a paraméterlista; a kijelölt gombhoz paramétert rendel (`kick_KnobTabs.cpp:16-30`).

**Screen módban ténylegesen Q-Link-et fogyasztó képernyők** (grep a `knobCommand` / `knobRotated` hívásokra; nem követtem végig mindet): Grid (Q-Link 1–4: görgetés/zoom, `kick_AppGrid.cpp:594-607`), Step Sequencer (`kick_stepSequencer.cpp:970`), Track-lista sor (input midi, length, level, transpose: `AppTrackComponent.cpp:254-257`), Sequence-lista sor (bpm, bars, start, end: `AppSequenceComponent.cpp:131-134`), XY FX (preset, attack, release, wet/dry: `kick_XyFxView.cpp:85-94`), automation-popup a fejlécben (`kick_modeHeader.cpp:385-387`), Sample Editor opciók, Sampler/Looper felvevő, File Browser, Pad Mixer, Channel Mixer, Pad Mute/Track Mute (csoport-hozzárendelés).

**Hardver-kötések összefoglalva**
| Vezérlő | Mit tesz | Hardveres állapot |
|---|---|---|
| Q-Link 1–4 forgatás | lásd fent (Custom / Screen) | CC16–19 dekódolás igazolt (RelativeCcTests, mért nyers MIDI); a képernyőkre jutás **nem igazolt** |
| Q-Link érintés (Note 84–87) | nincs (szándékosan) | mért a bemenet; nincs fogyasztó |
| Q-LINK gomb (Shift nélkül) | `EN_LINK` → `knobLongPress` (5 s-es modál-záró időzítő) | nem igazolt, hibás (QL-H6) |
| Shift+Q-LINK | Knob Edit megnyitása | képernyőnyitás nem külön igazolt (checklist L31) |
| Q-Link oszlopváltás, Shift+Q-Link beállítóablak | nincs | „nem vizsgált” (DVK L12) |
| Touch Strip „Q-Link” mód | nincs (a 12 módból hiányzó 5 egyike) | STRIP §3 |

## 2. Funkciók és állapotuk

| Funkció | Állapot | Bizonyíték |
|---|---|---|
| Q-Link 1–4 hardveres dekódolás és kiküldés | KÉSZ (host-teszt + mért) | `adapter:472-475`, `bridge:1530-1542`, `RelativeCcTests.cpp` |
| Q-Link → képernyő (Screen mód) | BUGOS GYANÚ (érintés után halott) | `kick_AppHome.cpp:573-580` (QL-H1) |
| Q-Link → képernyő (Custom mód) | KÉSZ az útvonal (a fókusz-ág előtt tér el) | `:561-570` |
| Lépésköz a számvezérlőkön | BUGOS (nyers delta) | `M/kick_ui_controls/source/kick_slider.cpp:128-134` (QL-H2) |
| Mute/solo Q-Link a mixer-sorokon | BUGOS (csak vizuális) | lásd `channel-mixer.md` CM-H3 |
| Custom: program Level (0..127), Fine, Semi, Poly | KÉSZ (kód) | `KE/kick_KnobGrid.cpp:227-256` |
| Custom: program Pan, pad Level, pad Pan | BUGOS: teljes skála egy kattanásra | `:187, 193, 223` (QL-H2) |
| Custom: pad Pan/Level/SemiTune/FineTune/PadSolo/PadMute | BUGOS: bank nélküli írás (mindig A bank) | `:185-217` (QL-H3) |
| Custom: Mute / Solo / PadMute / PadSolo | BUGOS: a rotáció iránya figyelmen kívül marad, mindig bekapcsol | `:210-217, 257-264` (QL-H4) |
| Custom: Insert-paraméterek (Ins 1–4) | RÉSZLEGES: 0,1/kattanás, cím szerinti egyeztetés (ütközhet a beépített nevekkel) | `:149-176` |
| Custom: MIDI program CC lista | KÉSZ (`CC<n>` tulajdonság → CC kimenet), de a *kiválasztott* programot írja | `KE/kick_KnobTabs.cpp:32-43`; `M/engine/audio_program/source/mt_audio_midi_program.cpp:33-52` |
| Custom: `sends01..04`, Keygroup program-lista, Clip Reverse/Wrap/Fade/Launch Quantize, Drum-PAD „Osc/LFO” lista | STUB (a lista nevei nem egyeznek egyetlen kezelővel sem) | `kick_KnobTabs.cpp:82-170` vs `kick_KnobGrid.cpp:139-266` (QL-H5) |
| Hozzárendelések mentése | HIÁNYZIK: csak a gomb címe, nem perzisztens | `kick_KnobGrid.cpp:105-109` (QL-H8) |
| Screen mód: tartalom (térkép) | STUB (üres panel) | `kick_KnobTabs.cpp:4-17` |
| Q-Link gomb: oszlop/sor váltás | HIÁNYZIK | `bridge:1195-1202` |
| „LINK hosszú nyomás” | BUGOS / STUB (5 s-es modál-záró) | `kick_KnobEdit.cpp:121-139`, `kick_KnobEdit.h` `timeinms = 5000` (QL-H6) |
| Q-Link érintés visszajelzés (Show Q-Link Status) | HIÁNYZIK (G-168) | `bridge:1544-1549` |
| Touch Strip Q-Link mód | HIÁNYZIK | STRIP §3 |
| Automation Lane Follows Q-Links / Default Q-Link Mode / Playhead Increment / Swing Control | HIÁNYZIK (G-169, G-140, G-134, G-135) | WP-G |

## 3. Mi nem működik / hibák

**QL-H1 — Q-Link halott a képernyőkön egy érintés után (P0, magas valószínűség).**
- `KickHome::handleCommand`: `if( isNavigationMessage(msg) && ui::getComponentInFocus() && !isPadButtonMessage(msg) ) getComponentInFocus()->handleCommand(msg); else if( screen->requiresHandleCommand() ) screen->handleCommand(msg);` (`kick_AppHome.cpp:573-580`). `isNavigationMessage` tartalmazza az `isKnobMessage`-t (`M/contracts/dcl/dcl_contracts_helper.cpp:90`), tehát a Q-Link-csomagot is.
- `getComponentInFocus()` JUCE-fókuszt, vagy az `selectedControlSlot()`-ot adja (`M/kick_ui_controls/source/kick_ControlBase.h:205-209`). Az MPC hídja minden **valódi érintésnél** beírja a megérintett vezérlőt (`bridge:561` `ui::selectControlFromTouch`), törli képernyőváltáskor (`:789`; `kick_AppHome.cpp:348` `clearFocusComponent`) és ha az érintés nem vezérlőre esett (`kick_ControlBase.h:194`).
- A vezérlő `handleCommand`-ja az `EN_KNOB_1..16` ágat dobja (`kick_ControlBase.h:98-121`, „Nothing to be done for link knobs”).
- Következmény: Screen módban egy csúszka/gomb megérintése után a Q-Link 1–4 a képernyőre **nem jut el** (Pad Mixer, Channel Mixer, Pad/Track Mute, Grid-görgetés, Step Seq …), amíg újra nem töltöd a képernyőt vagy üres felületet nem érintesz. A hibát az 0a42bfdb vezette be („the data dial / +/- act on the last touched control on every screen”); előtte az X nélküli MPC-n a fókusz null volt. Ez a Data Dial *szándékos* működése (kell a fókuszolt vezérlőhöz), a Q-Link-nek viszont a képernyőhöz kell mennie.
- Reprodukció: bármely Q-Link-fogyasztó képernyőn `touch <x> <y>` egy csúszkára, majd `echo "encoder 0 1" > /data/kick_cmd` (a híd injektált útja azonos a fizikaival, `bridge:1963`). **Hardveren nem mértem.**
- Megoldás: QL-01.

**QL-H2 — Q-Link lépésköz: a nyers számláló ≠ a vezérlő lépése (P0).** `KickSlider::knobRotated(delta)` a delta-t közvetlenül hozzáadja az értékhez (`kick_slider.cpp:128-134`); a Data Dial ezzel szemben `getKnobValue * getIncrement()`-et hív (`kick_ControlBase.h:126`, 0a42bfdb javította). A Q-Link-ág (`linkRotated` → `slider.knobRotated(value)`) kimaradt, ezért: gain 0..1 / pan −1..1 / `KickPanRotary` egy kattanásra a teljes skálán fut végig; a 0..127-es `KickLevelSlider` ±1-et lép (rendben). A Knob Edit Custom-ágában a program Pan (`KE/kick_KnobGrid.cpp:223`) és a pad Level/Pan (`:187, 193`) ugyanígy `get() + value`. A Key 61 relatív CC-je kattanásonként ±1, gyors tekerésnél N (`abs(delta)`, `bridge:1530-1542`) — gyorsítás/dinamika nincs.

**QL-H3 — Pad-paraméterek bank nélkül (P0).** A pad-ág olvasása bank-eltolással, írása nélküle: `getNotePan(padNum + bankNum*16)` / `setNotePan(padNum, …)` (`kick_KnobGrid.cpp:187-189`); Level `getNoteGain(padNum)` / `setNoteGain(padNum, …)` (`:193-195`); SemiTune/FineTune/PadSolo/PadMute szintén `padNum` (`:197-217`). A `padNum` a Pad listából 0..15 (`kick_KnobTabs.cpp:49-51, 209-217`), így B–H bankban a **A bank** megfelelő padját módosítja. A getterek `getOrCreate`-et hívnak (üres padra üres minta jön létre a projektben, `mt_audio_iprogram.cpp:118-122, 546-586`).

**QL-H4 — Mute/Solo Q-Link nem kapcsol ki (P0).** `( value ) ? setMute( true ) : setMute( false )` (`kick_KnobGrid.cpp:210-217, 257-264`): a `value` előjeles darabszám (`getKnobValue`: `CLOCKWISE ? n : -n`, `dcl_contracts_helper.cpp:115`), tehát mindkét irányban `true`; a 0 érték sosem érkezik. A kapcsoló egyszer bekapcsol, Q-Link-ről soha nem kapcsol vissza. Mixer-oldalon a megfelelő hiba a `dontSendNotification` (CM-H3, PM-H2). Közös megoldás: QL-03.

**QL-H5 — Paraméterlista ↔ kezelő eltérés (P1).** A lista címei a `KnobGrid::setValues` `title ==` ágaival egyeznek vagy nem: a *nem egyezők* hatástalanok: `sends01..04` (Drum/Clip/Keygroup „Mix”, `kick_KnobTabs.cpp:89, 120, 134, 151`), Drum-program „Polyphony” (a kezelő `"Poly"`-t vár, `:105` vs `kick_KnobGrid.cpp:245`), a Drum-PAD + Program lista Keygroup-szintézis paraméterei („Osc 1 Octave”, „LPF Cutoff”, …, `:105`; Drum programnak nincs ilyen), Keygroup „Program Polyphony” / „Global SemiTune” / „Global FineTune” / „Portamento Time” / „PortLegato” / „PortQuantize” (szóközös nevek, `:153`; a kezelő `GlobalSemiTune`-t vár), Clip „Launch Quantize” (`:122`) és PAD-„Reverse” / „Wrap” / „Fade” (`:136`), a Keygroup PAD-ágra egyáltalán nincs lista (a régi tartalom marad). A Drum „Velocity” cím a program *Level*-jét írja (`kick_KnobGrid.cpp:233-238`). A MIDI CC lista gépelési hibái: `"CC21"` kétszer (CC22 helyett) és `"Send4"` kétszer (Send5 helyett) a MIDI-listában (`kick_KnobTabs.cpp:166`), `"Filter Release"` kétszer a Drum-PAD „Osc” listában (`:105`) — a lista elejének számolása szerint az index a CC-számmal egyezik (CC0..CC23 ellenőrizve), a címke hamis; a teljes lista 128 elemét nem számoltam végig.

**QL-H6 — „LINK hosszú nyomás” = 5 s-es modál-záró (P1).** A Q-LINK gomb Shift nélkül `EN_LINK`-et küld, ami minden nyomásra `knobLongPress()`-t hív (`kick_AppHome.cpp:541-542`; nincs nyomvatartás-mérés). Ez 5 s-es időzítőt indít (`kick_KnobEdit.cpp:121-123`), a lejártakor `getCurrentlyModalComponent()->exitModalState(0)` (`:133-139`) — **bármely** megnyitott modális ablakot kilépési kóddal 0 (mégse) zár be; a `comp` és a `tempBounds` használaton kívüli (`:125-131`). Ráadásul a `screens_[MenuKnobEdit]` lusta létrehozás miatt nullptr lehet az induláskor (`dynamic_cast<KnobEdit*>(…)->` ellenőrzés nélkül; `getScreen(MenuKnobEdit)->knobCommand` is, `:569`). Hatás: a Q-LINK gomb megnyomása után 5 másodperccel egy nyitott TC-ablak / mentés-párbeszéd magától bezárul.

**QL-H7 — Csak a Q-Link 1–4 létezik; a 16-os rács 3/4-e elérhetetlen (P1).** Az MPC-n az 1–4 gomb a rács *első sorát* éri el; a Behringer 4x4-es rácsa (16 gomb, soronkénti kiemeléssel `KnobGrid::state`, `kick_KnobGrid.cpp:132-137`) a többi sort nem. A Q-Link gomb oszlopot váltana az Akai-n; itt semmit sem tesz. Ez a Custom módot MPC-n gyakorlatilag 4 hozzárendelésre korlátozza.

**QL-H8 — Hozzárendelés nem perzisztens (P1).** A Custom paraméter-választás csak a gomb *címét* állítja (`KnobGrid::labelInfo`, `kick_KnobGrid.cpp:105-109`); nincs mentés se a projektbe, se a beállításokba. A mód (`knobMode`) sima `int`, újraindításkor Screen (`kick_settings.h:218`). A hozzárendelés a *kijelzett* track/pad-hez nem kötött: a `KnobGrid` egyetlen 16 elemű állapot.

**QL-H9 — Program-választás inkonzisztens (P1).** A KnobEdit track-választója `indexNum`-ot állít (`kick_KnobEdit.cpp:15-24`; `KnobGrid::setValues` ebből veszi a programot, `kick_KnobGrid.cpp:142-146`), de a MIDI CC ág a *kiválasztott programot* írja (`ae.getProgramVT(ae.getSelectedProgram())`, `kick_KnobTabs.cpp:37`), és a CC kimenet a *kiválasztott track* kimeneti csatornáját használja (`mt_audio_midi_program.cpp:41-46`); a property-szűrő `containsIgnoreCase("CC")` bármely „cc”-t tartalmazó tulajdonságra CC-t küldhet (`:39`). Ha a választott track nem a kiválasztott, rossz programra/csatornára megy.

**QL-H10 — Védtelen pointerek (P1).** `KnobGrid::setValues`: `auto program = ae.getProgram(pProg->name.get()); auto* inserts = program->getInserts();` null-ellenőrzés nélkül (`kick_KnobGrid.cpp:145-149`); az Insert-paraméter és a beépített név ugyanarra a címre ütközhet (a ciklus a beépített ág *előtt* fut, mindkettő hat, `:151-176`); `CustomTab::setInsertsList` `program->getInsertsInfo` ellenőrzés nélkül (`kick_KnobTabs.cpp:67-71`).

**QL-H11 — Screen mód üres (P2).** A felhasználó nem látja, mit vezérel a Q-Link 1–4 az éppen megnyitott képernyőn; az Akai megmutatja (és Show Q-Link Status When Touched, G-168). A képernyő csak annyit ír: „Q-Link controls are fixed to the mode you are in” (`kick_KnobEdit.cpp:29`).

## 4. Tervben / hiányzik (Akai MPC Standalone-hoz képest)

Forrás: `KEY61_DOC_VS_KICK.md` (L12), `known-issues-and-roadmap.md` §3.2/§3.4, WP-G, `KEY61_TOUCH_STRIP_MODES.md`.
- **Q-Link Edit ablak** (bármely paraméter bármely Q-Link-hez, banki oszlopok): jelenleg Custom rács 16 elemmel; az Akai-ban 4 oszlop × 4 gomb (a Q-Link gomb váltja az oszlopot).
- **Shift+Q-Link**: Q-Link Configuration ablak, második nyomás: vissza a főoldalra (G-118). A híd Shift+QLINK-re a Knob Edit képernyőt nyitja (közelítés).
- **Touch Strip Q-Link mód** (strip → Q-Link 1–16, „Touch Strip Q-Link” ablak, pad kiválasztja a Q-Link-et), **Crossfader** (Q-Link Edit > Crossfader) — hiányzik (STRIP §3).
- **Preferences**: Default Q-Link Mode (G-140), Q-Link Playhead Increment (G-134), Swing Control Applies TC on Release (G-135), Show Q-Link Status When Touched (G-168), Automation Lane Follows Q-Links (G-169).
- **Makró-vezérlők, Learn** (G-220..G-222, WP-H terület).
- **Q-Link érintés**: visszajelzés (a hozzárendelt paraméter és érték felugró buboréka) — a bemenet mért, a Kicknek nincs knob-touch fogalma (T11), a megoldás új UI-esemény, nem nulla-delta csomag.
- **Q-Link LED**: az első oszlop-LED mindig piros (DVK L12); a „Light LED” beállításnak nincs funkciója a hardveren (L191) — nem hiba.
- **MIDI Control mód**: a Q-Link/strip MIDI-beállítása (Touch Sense, Note, Low/High Range, CC szám/csatorna) nincs (STRIP §5).

## 5. Javaslataim

1. **Q-Link = „kijelölt elem” modell, 4 fix paraméterrel képernyőnként.** Az MPC 4 gombjára a 16-os rács emulációja helyett (`getRowNumbers`, soronkénti kiemelés) a képernyő a *kijelölt* elem négy paraméterét adja (Pad Mixer: Level, Pan, Mute, Solo; Channel Mixer: kimenet, Level, Pan, Mute; Mute: csoport…). A képernyők csak egy `QLinkSlot[4]` listát (név, tartomány, lépés, getter/setter) regisztrálnak.
2. **Q-Link gomb = sor/oszlop (bank) váltás**: a híd `EN_KNOB_1 + idx + 4*bank`-ot küld, így a meglévő 16-gombos kódok (rács, soronkénti kiemelés) MPC-n is elérhetők (QL-07). A bank-állapot LED-visszajelzése: a Q-Link gomb LED-je a Key 61-en egyszínű, ezért képernyős kijelzés kell.
3. **Közös lépésköz-réteg** (QL-02): egyetlen `qlinkStep(control, count)` függvény (gain/pan 0,01·n, int 1·n, kapcsoló iránya, opcionális gyorsítás n>1-re).
4. **A Q-Link-ágat külön kell választani a Data Dialtól** a `KickHome::handleCommand`-ban (QL-01): a fókuszolt vezérlő csak a Data Dial/±/ENTER/csúszka/pad üzeneteket kapja.
5. **Perzisztencia**: a Custom hozzárendelés a ValueTree-be (programonként/projektenként), a mód a beállításokba.
6. **Q-Link-érintés + OSD** (Show Q-Link Status): a Note 84–87 esemény új, nulla-delta nélküli UI-jel; a képernyőn felugró „név: érték”.
7. **A „Q-Link Strip” mód** (STRIP §3) a hiányzó 5 mód közül a legolcsóbb: egy Q-Link `knobCommand`-ot hajt abszolút értékkel — a közös lépésköz-réteg után.
8. **Az „LINK hosszú nyomás”** valódi funkciót kapjon (pl. a mód váltása Screen↔Custom) vagy törlődjön.

## 6. Hardveres és unit-tesztek

**Ami ma fedi:** `RelativeCcTests.cpp` (a CC16–19 / CC100 relatív dekódolás), `AkaiMapTests.cpp` (a Q-Link érintés-note-ok nem gombok: L41-47, a QLINK gomb note 0), `LedMapTests.cpp` (QLINK mód-LED). A képernyőkre jutás, a lépésköz, a Custom-leképezés **nincs tesztelve**. Hardveren: a Q-Link 1 forgatása nyers MIDI-ként mérve (RPT „Main Volume gomb – mérés” szakasz, kontroll: `[AKAI-RAW] port=1 status=B0 d0=16 d1=1`, 108 sor); a checklist „Q-LINK (+Shift KnobEdit); Q-Link 1–4 forgatás” pontja **nincs kipipálva** (`HW_TEST_CHECKLIST.md` L31).

**MPC-n ellenőrzendők** (a `kick_cmd` FIFO-val reprodukálhatók: `encoder <0..3> <delta>`, `touch <x> <y>`, `btn QLINK`, `snapshot`):
- [ ] Q-Link 1–4 forgatás **friss képernyőn** és **érintés után** a Grid, Pad Mixer, Channel Mixer, Track/Pad Mute, Track-lista képernyőn (QL-H1).
- [ ] Pan/gain Q-Link lépés: egy kattanás mekkora ugrás (QL-H2).
- [ ] Custom mód: Pad Level/Pan B és C bankban — az A bank pad-je mozdul-e (QL-H3); Mute/Solo vissza lehet-e kapcsolni (QL-H4).
- [ ] MIDI program Custom CC: a kimenő MIDI-t `amidi -d` figyelve; másik track kiválasztva a KnobEditben (QL-H9).
- [ ] Q-LINK gomb: nyitott TC-ablak/Save-dialógus 5 s múlva bezárul-e (QL-H6); Shift+Q-LINK megnyitja-e a Knob Editet; LED.
- [ ] Hosszú tekerésnél a `abs(delta)` kezelése (gyors tekerés = nagy ugrás).
- [ ] Custom hozzárendelés újraindítás után (QL-H8: elvész).

## 7. TODO

Sorrend: hibák → hiányzó funkciók → javaslatok → tesztek. A QL-01..QL-03 a `PM-*`, `CM-*`, `MU-*` tételek előfeltétele.

- [x] **QL-01** (P0, M) Q-Link routing: a `KickHome::handleCommand` a Q-Link (`knob != EN_KNOB_CENTER`) csomagot Screen módban is az aktuális képernyő `handleCommand`/`knobCommand`-jára adja, a fókuszolt vezérlő csak Data Dial/±/ENTER/csúszka/pad üzenetet kapjon — `kick_AppHome.cpp:573-580`. Regresszió-teszt: injektált `touch` + `encoder`.
- [x] **QL-02** (P0, M) Közös Q-Link lépésköz: `knobRotated(n * getIncrement())` a Q-Link-ágban is (minden `linkRotated`), vagy központi `qlinkStep`; Knob Edit `KnobGrid::setValues` Pan/Level ágai skálázva (`kick_KnobGrid.cpp:187, 193, 223`); opcionális gyorsítás. Érinti: PM-01, CM-03, MU-04.
- [x] **QL-03** (P0, S) Mute/Solo Q-Link közös helper: irány-alapú (jobbra be, balra ki), **valódi engine-hívással** — Knob Edit (`kick_KnobGrid.cpp:210-217, 257-264`), Pad Mixer (`kick_PadMixer.cpp:123-126`), Channel Mixer 7 hely (CM-H3). Érinti: PM-02, CM-02.
- [x] **QL-04** (P0, S) `KnobGrid` pad-ágak bank-eltolása olvasásnál és írásnál egyaránt (`padNum + bankNum*16`), `getOrCreate`-et elkerülő olvasás üres padra — `kick_KnobGrid.cpp:185-217`.
- [x] **QL-05** (P1, S) `KnobGrid::setValues` védelem: `program`/`inserts` null, Insert- és beépített cím ütközése (egy paraméter egyszer hat), `getProgramVT` a KnobEdit track-programjára (`kick_KnobTabs.cpp:37`), CC-csatorna a program saját trackjéről, szigorú `^CC\d+$` szűrő (`mt_audio_midi_program.cpp:39`).
- [x] **QL-06** (P1, S) A `knobLongPress` és az `EN_LINK` ág kezelése: valódi hosszú-nyomás-detektálás vagy törlés, a modál-záró időzítő megszüntetése, `screens_[MenuKnobEdit]` null-védelem — `kick_AppHome.cpp:541-542, 569`, `kick_KnobEdit.cpp:121-139`.
- [x] **QL-07** (P1, M) Q-Link gomb = bank/sor váltás: a híd `EN_KNOB_1 + idx + 4*bank`, `KnobGrid::state`/sor-kiemelés követi, képernyős bank-jelzés; Shift+Q-Link marad a Knob Edit/konfig. Függ: QL-01, QL-02.
- [x] **QL-08** (P1, M) Paraméterlista ↔ kezelő egyeztetése: `sends01..04` bekötése, Drum „Polyphony”/`Poly`, Keygroup-nevek (szóköz nélkül), Clip Reverse/Wrap/Fade/Launch Quantize bekötése vagy a lista szűkítése, a Drum-PAD „Osc/LFO” lista törlése, MIDI CC lista elírásai (`CC22`, `Send5`) — `kick_KnobTabs.cpp:82-170`, `kick_KnobGrid.cpp:139-266`. Egy táblázat (cím → getter/setter/tartomány) a két hely helyett.
- [x] **QL-09** (P1, M) Custom hozzárendelések és a mód perzisztenciája (ValueTree / beállítások), program- vagy projekt-szintű hatókör — `kick_KnobGrid.cpp:105-109`, `kick_settings.h:213-219`. Owner-döntés: a hatókör.
- [ ] **QL-10** (P2, M) Screen mód: az aktuális képernyő négy Q-Link-szerepének megjelenítése (név + érték) a Knob Edit jobb oldalán és/vagy felugró OSD-ként (G-168).
- [ ] **QL-11** (P2, M) Q-Link érintés (Note 84–87) → OSD/kiemelés új UI-eseménnyel (nem nulla-delta csomag, T11) — `bridge:1544-1549`, `adapter:439-460`.
- [ ] **QL-12** (P2, L) Touch Strip Q-Link mód (+ „Touch Strip Q-Link” ablak, pad-választás); Crossfader mód (Q-Link Edit > Crossfader) — STRIP §3. Függ: QL-02.
- [ ] **QL-13** (P2, M) Preferences: Default Q-Link Mode, Playhead Increment, Swing Control, Show Q-Link Status, Automation Lane Follows Q-Links (G-134, G-135, G-140, G-168, G-169) — a Preferences-terv 8.3-as pontjával együtt.
- [ ] **QL-14** (P2, L) Q-Link Edit az Akai-paritás felé: 4 oszlopos bank, makró-vezérlők, Learn (G-220..G-222); előbb WP-H végigvétele.
- [x] **QL-15** (P2, S) Gyorsítás: `abs(delta) > 1` kezelése (gyors tekerés ne ugorjon pusztán n-szeresen).
- [x] **QL-16** (P1, M) Host-teszt: új tiszta fejléc (pl. `Kick/apps/kick/hardware/qlink_step.h`): számláló → lépés (gain/pan/int/kapcsoló irány), bank-eltolás; `Kick/tests/platform/QLinkStepTests.cpp` + CMake.
- [x] **QL-17** (P1, M) Routing-teszt: `KickHome::handleCommand` tiszta döntési függvényre bontása (Q-Link → képernyő / Custom → KnobEdit / fókusz → vezérlő), host-teszt; kiegészítve `AppSmoke`-szerű `touch`+`encoder` injektált szkenárióval.
- [ ] **QL-18** (P1, S) MPC-s checklist (6. pont) lefuttatása, a `HW_TEST_CHECKLIST.md` L31 pontjának kipipálása/rögzítése a `HW_TEST_REPORT`-ban.

## 8. Állapot (S-E kör, 2026-11-04)

Kész (host-teszt `QLinkLogicTests`, Mac build + smoke): **QL-01** (a `KickHome::handleCommand` döntése `knob_logic::routeHardwareMessage`: Custom -> Knob Edit, adat-tárcsa -> vezérlő, Q-Link -> képernyő érintés után is), **QL-02/03** a Knob Edit rácsán (lépés = a paraméter lépése × tick, kapcsoló iránya; a mixer-képernyők ugyanezt a `mixlogic`-ot kapták a saját workereiktől), **QL-04** (bank-eltolás olvasáskor és íráskor, `knob_logic::padNote`), **QL-05** (null-védelem, az insert-gombok a saját slotjukhoz kötve, a MIDI CC a KnobEdit track programjára, szigorú `CC<n>` szűrő az engine-ben), **QL-06** (az 5 s-os modál-záró megszűnt; a Q-LINK gomb a következő négyes sort választja), **QL-07** (sor-váltás a Q-LINK gombbal, híd-módosítás nélkül), **QL-08** (egy cím-táblázat a listához és a kezelőhöz; a hatástalan címek törölve), **QL-09** (a hozzárendelés a beállítás-fában, globális hatókörrel: az owner dönthet program-szintről), **QL-15** (gyorsítás), **QL-16/17** (tiszta lépés- és útvonal-függvények, host-teszt).

Nyitott: **QL-10** (Screen mód: a négy szerep kijelzése), **QL-11** (Q-Link érintés -> OSD: híd), **QL-12** (Touch Strip Q-Link mód), **QL-13** (Preferences: `kick_ui_settings` másik worker), **QL-14** (4 oszlopos Akai-paritás, WP-H). **QL-18** ember: checklist 41.
