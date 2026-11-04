# Assign Sample — review és TODO

Alap: **statikus kódolvasás** (build, futtatás, hardver nélkül), ág `claude/kick-plans` HEAD `40fb2db6`, 2026-11-04. „KÉSZ” = van bekötött handler a motorig, nem hardveres/hallgatásos igazolás. Ahol a régi doksik (`research/screen-inventory.md` §4.24: „KESZ”, WP-G G-272..G-284) és a kód eltér, a kód az igazság. Ami futtatás nélkül nem dönthető el: „NEM ELLENŐRIZHETŐ KÓDBÓL”.
Útvonal-rövidítések: `M/` = `Kick/modules/`, `A/` = `Kick/apps/`, `AS` = `Kick/modules/kick_ui/dialogs/kick_AppAssignSample` (`.cpp`/`.h`), `EN/` = `Kick/modules/engine/`.

## 1. Áttekintés (mit csinál, hogyan lehet elérni, hardver-gombok kötése)

**Mit csinál.** A kijelölt program (Drum, Clip, Keygroup) padjaihoz a **projekt minta-poolját** (`getAssetNames(Samples)`, `AS.cpp:149`) rendeli: bal oldalon lista („Clear Assignment” az első elem, `AS.cpp:35,150`), jobb oldalon a 16 pados rács (`KickPadButtonGroupComponent`, bank A–H), a fejlécben bank −/+ és a File Browser-ugró gomb (`AS.cpp:710-761`). Kiválasztott listaelem = előhallgatás (`:65-73`, `:288-293`); a hozzárendelés a kijelölt pad(ok)ra történik (`assignSampleIntoPad`, `:90-127`). Egyetlen réteg (0.) kap mintát (`:110`).

**Hogyan érhető el** (nincs külön hardver-gomb):
- Program fül `assign` gomb → `ae.setSelectedProgram(progNumber)` + `sigScreenClose(MenuAssignSample)` (`M/kick_ui/program_view/source/kick_AppProgramComponent.cpp:55-58`); a gomb Drum, Keygroup, **Audio** és Clip típusnál látszik (`:259-287`).
- Pad Mixer „sample assign” gomb (`M/kick_ui/modes/mixers/kick_PadMixer.cpp:176-179`); File Browser alsó „Sample Assign” gomb (`M/kick_ui/modes/filebrowser/kick_fileBrowser.cpp:1758-1771`).
- Vissza: `header.onBackButtonPressed = closeScreen()` (`AS.cpp:718`); a fejléc File Browser-gombja `sigScreenOpen(MenuFileBrowser)` (`:748-750`).

**Hardver-kötés a képernyőn** (`AppAssignSample::handleCommand` → `AssignSampleContent::handleCommand`, `AS.cpp:295-372`; a KickHome csak fókuszált vezérlő nélkül adja át, `M/kick_ui/dialogs/kick_AppHome.cpp:573-585`):
- **16 pad** — ütés: a program hangja szól (`prog->noteOn(padNo, velocity)`, `:349-368`); egyidejűleg a `KickPadButtonGroupComponent` a hardver-pad-et a képernyős pad kattintására fordítja (`M/kick_ui_components/source/kick_padButtonGroup.cpp:684-746`) → kijelölés/kiemelés (`AS.cpp:43-46`).
- **Shift + pad** → hozzárendelés (`onShiftPadPressed`, `:55-58`; a Shift-állapot szinkronban a híddal: `M/contracts/dcl/dcl_signals.cpp:51`).
- **Data Dial forgatás** → lista léptetés + előhallgatás (`:340-347`); **Data Dial nyomás (EN_KNOB_ENTER)** → hozzárendelés a kiemelt padra (`:324-328`); `onKnobClicked` ugyanez (`:60-63`).
- **ERASE** → a kiemelt pad mintájának törlése (`:303-322`).
- **Bank A–H gombok** → a KickHome `ae.setBank`-ja, a `bankChanged` jel frissíti a képernyőt (`:758-760,802-813`).
- **PLAY / PLAY START / STOP** (`:329-338`).
- **Érintés**: egy koppintás a padra = kiemelés (`:43-46`); **dupla koppintás = hozzárendelés** (`:48-53`) — MPC-n igazolt: „Taps within 450 ms / 24 px count as clicks … (verified with Sample Assign pad assignment on the MPC)” (commit `42cfdc3c`; `known-issues-and-roadmap.md` §1). Húzás-és-ejtés nincs (WP-G G-278).

## 2. Funkciók és állapotuk

| Funkció | Állapot | Bizonyíték (fájl:sor) |
|---|---|---|
| Belépés a Program fülről / Pad Mixerből / File Browserből | KÉSZ | `kick_AppProgramComponent.cpp:55-58`, `kick_PadMixer.cpp:176-179`, `kick_fileBrowser.cpp:1758-1771` |
| Minta-lista a projekt-poolból + „Clear Assignment” | KÉSZ | `AS.cpp:35-36,143-157` |
| Előhallgatás a lista kijelölésére (Data Dial vagy érintés) | RÉSZLEGES / BUGOS | `AS.cpp:65-73,288-293`; B3, B4 |
| 16 pados rács, pad-feliratok (minta neve), keret a kiosztottakon | KÉSZ (Drum, Clip) | `AS.cpp:165-200` |
| Rács Keygroup programnál | BUGOS | `AS.cpp:184-191` — `for i = 0..maxNote` (nem `minNote`-tól); B5 |
| Bank −/+ és HW bank A–H | KÉSZ | `AS.cpp:728-746,758-760,802-813` |
| Hozzárendelés: dupla koppintás / Shift+pad / Data Dial nyomás | KÉSZ (dupla koppintás MPC-n igazolt; a másik kettő NEM IGAZOLT, G-277 „nem vizsgált”) | `AS.cpp:48-63,324-328`; `42cfdc3c` |
| Hozzárendelés **rétegre** (Layer 1–4) | HIÁNYZIK | minden hívás `Layer = 0` (`AS.cpp:110`, `EN/interface/source/mt_audio_interface.h:220`); WP-G G-279 |
| Törlés („Clear Assignment” + ERASE) | RÉSZLEGES / BUGOS | `AS.cpp:100-106,303-322`; `EN/audio_program/source/mt_audio_iprogram.cpp:103-110` (teljes minta-csomópont, `nullptr` undo); B2 |
| Hardveres pad-ütés hangja | KÉSZ | `AS.cpp:349-368` |
| Pad-LED visszajelzés (kiosztott/üres, kiemelt) | RÉSZLEGES MPC-n | B6 |
| Program-típus támogatás | BUGOS | Audio/Midi/Cv/Plugin: a függvény csendben semmit nem csinál (`AS.cpp:92-94`); Audio-nál a gomb mégis látszik; B7 |
| Audition-beállítás (be/ki, hangerő, Auto) | HIÁNYZIK | WP-G G-275 (`keresve: nincs találat`); előhallgatás hangereje a globális `settings.preview.gain` (`AS.cpp:292`) |
| „Delete Sample” a pool-ból | HIÁNYZIK | WP-G G-281 |
| Stream From Disk / Load To Memory | HIÁNYZIK | WP-G G-282/G-283; nincs disk streaming (`research/codebase-survey.md` §1) |
| Track mező (sáv választása a Sample Assign fülön) | HIÁNYZIK | WP-G G-272 |
| Pool-böngészés söprés/Data Dial/−/+ gombbal | RÉSZLEGES (Data Dial: kód szerint igen; −/+ nincs) | `AS.cpp:340-347`; G-273 „nem vizsgált” |
| Visszalépés a Browserbe a Sample Assign újraérintésére | NEM VIZSGÁLT | G-284 |
| Halott kód: `DrumAssignSampleContent`, `KeysAssignSampleContent`, `SamplesList`, `KeysPanel`, `KeysBottomControls`, `sendAssignedSamplesAtStartup`, `startup()`, `KickHome::AssignSampleStartUp` | STUB (nem használt) | `AS.h:57-146` és `AS.cpp:373-706` (`#if NOT_USED`), `AS.h:159` (`startup`), `kick_AppHome.cpp:426-429`; az `AssignSampleStartUp()`-nak nincs hívója |

## 3. Mi nem működik / hibák

**B1 (P0) Fókusz nélküli üzenet nullptr-hívást okoz.** `AS.cpp:370-371`: a gomb/knob/pad-ágak után `else ui::getComponentInFocus()->handleCommand( f_HwMsg );` — a `getComponentInFocus()` `nullptr`-t adhat (a hívó megőrzi az utolsó érintett vezérlőt, de ha az érintés nem vezérlőre esett, a kiválasztás törlődik: `M/kick_ui_controls/source/kick_ControlBase.h:184-210`; MPC-n a rejtett ablak miatt a JUCE-fókusz sosem létezik, `:163-168`). A KickHome fókusz nélkül a *képernyőnek* adja a navigációs üzeneteket (`kick_AppHome.cpp:573-585`), köztük a touch strip „Kick slider” üzeneteit (`DclContractsHelper::isSliderMessage`, `dcl_contracts_helper.cpp:68,90`). Hatás: ha a strip slider-módban van (a híd 0. módja, `A/kick/kick_hardware_bridge.h:887`), és az Assign Sample nyitva van fókuszált vezérlő nélkül, a strip érintése összeomlást okoz. Reprodukció: Assign Sample megnyitása → dial-/lista-érintés nélkül strip mozgatása slider-módban (a pontos MPC-viselkedés NEM ELLENŐRIZHETŐ KÓDBÓL; a nullptr-ág a kódból egyértelmű).

**B2 (P1) A „törlés” a teljes padot tönkreteszi, és nem visszavonható.** `removeSampleFromProgram(prog, pad)` → `MTIProgram::removeSample(Note)`: a pad teljes `MTISample` csomópontját eltávolítja a fából `nullptr` UndoManagerrel (`EN/audio_program/source/mt_audio_iprogram.cpp:103-110`) → minden réteg, burkoló/LFO/gain/mute group beállítás elvész, Undo nem hozza vissza. A WP-G G-280 az Akai „Clear Pad” viselkedést *a pad kijelölése után a mintát* törli. ERASE-nál `padIndex = pads->getHighlightedPad()` lehet `−1` (nincs kiemelés): `removeSampleFromProgram(prog, −1)` és `setPadDataMessage(ledIndex = −1 − bankOffset, …)` (`AS.cpp:297-312`) — a −1 hatása a motorban (`iSampleMap.find(−1)`) ártalmatlan, de a pad-LED parancs érvénytelen indexet kap.

**B3 (P1) Az előhallgatás útvonala törékeny és az UI-szálon fut.** `AS.cpp:65-73`: a listaelem *neve* (fájlnév) lesz `juce::DirectoryIterator` *wildcard-mintának*, rekurzívan, a mintakönyvtáron (`findFileInDirectory`, `:76-88`) minden kijelölés-változáskor, szinkron módon az üzenetszálon (Data Dial gyors pörgetésnél soronként). Következmények: (a) azonos nevű fájl más mappában → az **első találat szól, nem a hozzárendelendő** (a hozzárendelés a pool-index szerinti `getAsset(Samples, idx − 1)`-et használja, `:109`); (b) a név `*`, `?`, `,`, `;` karaktert tartalmazva mintaként értelmeződik (JUCE wildcard); (c) nagy mintakönyvtáron (MPC pendrive) a bejárás akadást okoz. Javítás: a pool-index → teljes útvonal (`getAsset`) közvetlenül.

**B4 (P2) Előhallgatás hangereje és állapota.** `createPreview(f, settings.preview.gain)` (`:292`); az előhallgatás a képernyő elhagyásakor leáll (`:158-159`), de pad-ütésnél (HW) a program hangja és az előhallgatás egyszerre szólhat (nincs `stopPreview` a pad-ágon, `:349-368`).

**B5 (P1) Keygroup: hibás pad-kiosztás megjelenítés.** `AS.cpp:184-191`: ha `minNote != maxNote`, a ciklus `0..maxNote`-ig minden padra kiírja a minta nevét (`minNote`-tól kellene); mivel az alapértelmezett zóna 0..127 (`EN/audio_engine/engine/mt_audio_interface_types.cpp:133-134`), egy mintával minden pad (mind a 128) „kiosztott”-nak látszik; a `refreshLEDs` Keygroup-ágban bármely minta jelenléte az összes 16 padot „kiosztott”-ra színezi (`:212-235`). Hatás: Keygroupnál nem látszik, melyik padhoz (zónához) tartozik minta; a zóna (min/max/root) a Program Edit-ben sem szerkeszthető (lásd `program-edit.md`, PE-24).

**B6 (P1) MPC-n a pad-LED visszajelzés hiányos.** A híd a UI-ból érkező DCL parancsok közül csak két típust dolgoz fel: pad-aftertouch-enable és `EN_PAD_SET_COLOUR_STATE` (`A/kick/kick_hardware_bridge.h:137-160`). Az Assign Sample az egyedi `setPadDataMessage`-t (= `EN_PAD_SET_SINGLE_PAD`, `dcl_contracts_helper.cpp:205-208`) használja a hozzárendelés/törlés/kiemelés visszajelzéséhez (`AS.cpp:103-119,314,…`; `kick_fileUtils.cpp:134-165`) → MPC-n **eldobódik**. A teljes bankot beállító `setPadColourAndStateInfoMessage` (`refreshLEDs`, `AS.cpp:202-270`) viszont feldolgozódik, de a hozzárendelés utáni async frissítés csak a UI-t frissíti (`refresh()`, `:121-124`), a LED-eket nem (`refreshLEDs` csak megnyitáskor és bank-váltáskor fut, `:154-155`, `:806-811`). Hatás: MPC-n hozzárendelés után a pad színe nem változik, a kiemelt (kijelölt) pad nincs jelölve a hardveren, amíg bankot nem váltasz.

**B7 (P1) Típus-kezelés: Audio „assign” gomb halott.** A Program fülön az Audio programnál is látszik az `assign` (`kick_AppProgramComponent.cpp:274-275`), de `assignSampleIntoPad` csak Drum/Clip/Keygroup típusra ír (`AS.cpp:94`), egyéb típusnál **visszajelzés nélkül** kilép. Továbbá `ae.getSelectedProgramData()->type` (`:92`) nullptr-ellenőrzés nélkül (a `refreshLEDs` ellenőriz, `:209`).

**B8 (P2, gyanú) A pad-csoport HW-kezelője halmozódik.** `KickPadButtonGroupComponent::visibilityChanged` (`kick_padButtonGroup.cpp:684-746`) minden megjelenéskor új lambdát `connect`-el a `hardwareInputTriggered` jelre, elrejtéskor `disconnect(handle_Bank)`-et hív egy *új* lambda-példánnyal. A `sigslot` lambdánál a hívó-operátor típusa alapján azonosít (`M/3rdParty/signal/signal.hpp:355-377,1628-1634`), így a lekapcsolás vagy az összes azonos típusú (= minden pad-csoport) kezelőt leveszi, vagy semmit — mindkettő hibás: vagy a megjelenések számával nő a duplikált `triggerClick` (több `OnButtonClicked`/`onShiftPadPressed`/`setHighlightLED` egy ütésre), vagy egy másik képernyő pad-csoportjának HW-kezelője megszűnik. NEM ELLENŐRIZHETŐ KÓDBÓL, melyik; MPC-n: Assign Sample 5× megnyitása után egy Shift+pad hányszor hív hozzárendelést (naplóban).

**B9 (P2) Kisebb.** A bank-nyíl `leftNav` kezelője az indexet 0-nál `setBank(0)`-val újrahívja (`:730-731`), felesleges DCL/LED-forgalom; az `OnDoubleClicked` lambdában nem használt `index` (`:50`); a `fileList.setContent(files, getSelectedIndex())` megnyitáskor a korábbi kijelölést tartja, de a pad-kiemelés `lastPressedPad = −1` (`:151,156`) → a kijelölt listaelem és a kiemelt pad nincs szinkronban; a `getAsset()` referenciát ad lock-on kívülre (`EN/audio_file_manager/source/mt_audio_asset_manager.cpp:308-318`).

## 4. Tervben / hiányzik (az Akai MPC Standalone-hoz képest, a docs-ból ismert tervek)

(A kézikönyv nincs a repóban; a lista a WP-G-re és a tervekre épül.)
- **Audition-gomb** (be/ki, hangerő, Auto): G-275. **Delete Sample**: G-281. **Stream From Disk / Load To Memory**: G-282/G-283. **Track mező** (sáv választása): G-272. **Húzás-és-ejtés** minta→pad, Shift+húzás réteg: G-278/G-279. **Content-gombok a Browserben** (Samples/Vault, Demos, My Files, Splice): G-268..G-271 (Splice: `NEM-KICK`; lásd `research/cloud-sources.md`, `04-screen-audit.md` §7).
- **Sounds Mode** (beépített hangszerek/presetek; Plugin-presetek): G-285..G-287; a Sample Assign nem helyettesíti.
- **Réteg-választás** (1–4) a hozzárendeléskor és a velocity-tartomány: a Program Edit Layer Mix/Velocity funkcióihoz tartozik (`program-edit.md` PE-20).
- **Minta-forrás bővítés** (felhőmappa, WebDAV, helyi Splice-mappa): `research/sample-sources.md`, `research/cloud-sources.md`, `04-screen-audit.md` §7 — az Assign Sample listája ma csak a *projekt-pool*; új forrás a File Browseren át érkezik.
- **Undo** a hozzárendelésre/törlésre: az `addSample` UndoManagert használ-e, NEM ELLENŐRIZHETŐ KÓDBÓL (`removeSample` nem, B2).

## 5. Javaslataim (improvementek, indoklással)

1. **Réteg-választó a hozzárendeléshez** (Layer 1–4 gombok a lista felett, alap: 1): a 4-rétegű modell és a „Layer Play/Velocity” funkciók csak így tölthetők fel a UI-ból; a zongora-terv (02) velocity-rétegeihez elengedhetetlen. Együtt a Program Edit Samples lapjával (PE-20).
2. **Törlés = csak a réteg (`removeSample(Note, Layer)`, `mt_audio_iprogram.cpp:395-399`) + Undo**; teljes pad-törlés külön, megerősített művelet. Indoklás: a Clear nem veszítheti el a pad összes beállítását.
3. **Előhallgatás index→útvonal alapon, háttérszálon**, a mintakönyvtár bejárása nélkül; a gyors Data Dial pörgetésnél csak a *megállt* kijelölés szóljon (debounce ~150 ms). Indoklás: MPC-n a bejárás a pendriveon lassú; a duplikált nevek hibáját (B3a) is megszünteti.
4. **MPC-s LED-visszajelzés**: a hozzárendelés/törlés után `refreshLEDs()`, és a kiemelt pad visszajelzése a hídban feldolgozott `EN_PAD_SET_COLOUR_STATE` útján (az egyedi pad-parancs a hídban nem megy át, B6). Indoklás: az Akai-szerű „pad-ra ütök, rendelek” munkafolyamat vak, ha a pad-LED nem frissül.
5. **Keygroup-nézet**: zóna-sáv/tartomány-kijelzés (min–root–max) a pad-rácson, a kiosztás helyes megjelenítése (B5).
6. **Típus-visszajelzés**: Audio/Midi/CV/Plugin programon az `assign` gomb rejtett, vagy üzenet („ehhez a programtípushoz nincs minta-hozzárendelés”).
7. **Érintés-ergonómia (MPC 1024×600)**: a lista sorai ≥ 48 px, a dupla koppintás helyett opcionális „Hozzárendel” gomb (a dupla koppintás 450 ms/24 px ablaka ujjal kényelmetlen, de igazolt működő út); a kijelölt pad és a kijelölt minta egyszerre látszik a fejlécben.
8. **Teljesítmény**: `getAssetNames` a megnyitáskor ismételten másol (`AS.cpp:149-150`), a lista nagy pool-nál virtualizálandó (a `KickList` minden elemből gombot épít, `kick_listbox.cpp:102-116`).
9. **Tesztelhetőség**: a hozzárendelés/törlés logikát (típus-ellenőrzés, index-számítás, bank-eltolás) tegyük tiszta függvénybe (nincs JUCE-/DCL-függés), mert most `assignSampleIntoPad` közvetlenül a DCL-t hívja.

## 6. Hardveres és unit-tesztek

**Ami most fedi:** az Assign Sample-re **nincs** egység- vagy smoke-teszt. `Kick/tests/platform/AppSmoke.{h,py}` csak CLAP-program létrehozást fed; `PadColourMapTests.cpp` a híd pad-szín táblájának leképezését. Hardveres igazolás: a dupla koppintás-hozzárendelés MPC-n (commit `42cfdc3c`, `known-issues-and-roadmap.md` §1). A WP-G sorai G-276 „OK”, G-273/277/278/279/284 „NEM VIZSGÁLT”.

**Javasolt egységtesztek:** (1) `assignSampleIntoPad` index-számítás (bank-eltolás, tartomány); (2) előhallgatás-útvonal: duplikált fájlnév, speciális karakterek; (3) Keygroup pad-felirat tartomány (`minNote..maxNote`); (4) törlés réteg szinten + Undo; (5) nullptr-biztonság: minden `IScreen::handleCommand` ág fókusz nélkül (strip-slider, ismeretlen típus).

**MPC-n ellenőrzendő lista:**
- [ ] Program fül → Assign → lista megjelenik (a projekt-pool), „Clear Assignment” az élen.
- [ ] Lista-érintés / Data Dial → előhallgatás hallható; gyors Data Dial pörgetés nem akad (pendrive).
- [ ] Pad koppintás → kiemelés; **dupla koppintás** → hozzárendelés (igazolt); Shift+pad; Data Dial nyomás (G-277).
- [ ] Hozzárendelés után a pad-LED színe frissül-e bankváltás nélkül (B6).
- [ ] ERASE gomb kiemelt pad nélkül nem fagy/omlik (B2).
- [ ] Strip mozgatása slider-módban az Assign Sample-ön, fókusz nélkül nem omlik össze (B1).
- [ ] Keygroup program: kiosztott pad-ek megjelenése több mintával (B5).
- [ ] Audio program `assign` gomb viselkedése (B7).
- [ ] Bank A–H HW gombbal és −/+ nyíllal; a File Browser-ugrás és vissza.
- [ ] 5× megnyitás után Shift+pad egyszeri hozzárendelés (B8).

## 7. TODO

Sorrend: hibák → hiányzó funkciók → javaslatok → tesztek.

- [ ] **AS-01** (P0, S) `handleCommand` záró ága: `getComponentInFocus()` null-védelem (és ismeretlen üzenet figyelmen kívül hagyása) — `AS.cpp:370-371`, `M/kick_ui_controls/source/kick_ControlBase.h:205-210`.
- [ ] **AS-02** (P1, S) `assignSampleIntoPad`/ERASE: `padIndex < 0` védelem, `getSelectedProgramData()` null-ellenőrzés, érvénytelen LED-index nem megy ki — `AS.cpp:92,297-312`.
- [ ] **AS-03** (P1, M) Törlés rétegszinten + Undo (`removeSample(Note, Layer)`), teljes pad-törlés külön megerősítéssel — `AS.cpp:100-106`, `EN/audio_program/source/mt_audio_iprogram.cpp:103-110,395-399`.
- [ ] **AS-04** (P1, M) Előhallgatás index→útvonal (`getAsset`) alapon, debounce-szal, háttérben; `findFileInDirectory` megszüntetése — `AS.cpp:65-88`. Függ: –
- [ ] **AS-05** (P1, M) Pad-LED frissítés hozzárendelés/törlés után MPC-n: `refreshLEDs()` a `callAsync`-ban és a kiemelt pad jelzése `EN_PAD_SET_COLOUR_STATE`-tel (vagy a híd fogadja az egyedi pad-parancsot) — `AS.cpp:121-124,202-270`, `A/kick/kick_hardware_bridge.h:137-160`, `kick_fileUtils.cpp:134-165`.
- [ ] **AS-06** (P1, S) Keygroup pad-kiosztás megjelenítés `minNote..maxNote`-ra, LED-ek zóna szerint — `AS.cpp:184-191,212-235`.
- [ ] **AS-07** (P1, S) Típus-visszajelzés: Audio/Midi/Cv/Plugin → rejtett `assign` vagy üzenet — `AS.cpp:92-94`, `kick_AppProgramComponent.cpp:259-287`.
- [ ] **AS-08** (P2, M) A pad-csoport HW-kezelőjének életciklusa: egyszeri `connect` + `sigslot::connection` eltárolása/lekapcsolása (nem új lambda `disconnect`) — `M/kick_ui_components/source/kick_padButtonGroup.cpp:684-746`. Hatással van a Pad Mixer/Pad Mute/Pad Color képernyőkre is → közös javítás.
- [ ] **AS-09** (P2, S) Kisebb: `leftNav` 0-nál ne hívjon `setBank`-ot, nem használt változók, `getAsset` lock-on kívüli referencia — `AS.cpp:50,730-731`, `mt_audio_asset_manager.cpp:308-318`.
- [ ] **AS-10** (P1, M) Réteg-választó a hozzárendeléshez (1–4) + a pad-rácson a rétegek jelzése. Függ: AS-03; közös tervezés `program-edit.md` PE-20-szal.
- [ ] **AS-11** (P1, M) Audition-beállítás (be/ki, hangerő), Delete Sample (megerősítéssel, használt minta védelme), Track mező — WP-G G-275, G-281, G-272. Függ: AS-04.
- [ ] **AS-12** (P2, L) Minta-forrás integráció (felhő/WebDAV/helyi mappa) a File Browser forrás-interfészén át; az Assign Sample marad a projekt-pool nézet — `research/sample-sources.md`, `research/cloud-sources.md`. Függ: `04-screen-audit.md` §7 M0.
- [ ] **AS-13** (P2, M) Húzás-és-ejtés minta→pad (és Shift+húzás réteg) — WP-G G-278/G-279. Függ: AS-10.
- [ ] **AS-14** (P2, S) Érintés-ergonómia: ≥ 48 px lista-sor, opcionális „Hozzárendel” gomb, kijelölt pad+minta a fejlécben.
- [ ] **AS-15** (P2, S) Halott kód eltávolítása (`#if NOT_USED` blokk, `startup`, `AssignSampleStartUp`) **owner jóváhagyásával** — `AS.h:57-146`, `AS.cpp:373-706`, `kick_AppHome.cpp:426-429`.
- [ ] **AS-20** (P1, M) Egységtesztek (index/bank-számítás, előhallgatás-útvonal, Keygroup tartomány, réteg-törlés+Undo, null-biztonság) — `Kick/tests/platform/`.
- [ ] **AS-21** (P1, S) MPC hardveres ellenőrzőlista (6. fejezet) beemelése a `HW_TEST_CHECKLIST.md`-be és lefuttatása (G-273/G-277/G-284 lezárása).
