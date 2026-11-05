# Menu (HomeOptions) — review és TODO

Állapot: **statikus kódolvasás** (build/futtatás/hardver nélkül; az MPC-hez nem csatlakoztam), 2026-11-04, ág `claude/kick-plans`. A „KÉSZ” itt azt jelenti: van bekötött handler a képernyőig, nem azt, hogy hardveren igazolt. Ahol a régi docs (`wp-g.md`) és a kód ellentmond, a **kód az igazság**. Rövidítések: `M/` = `Kick/modules/`, `A/` = `Kick/apps/kick/`, `bridge` = `A/kick_hardware_bridge.h`, `MENU/` = `M/kick_ui/dialogs/`, `MC` = `M/kick_ui_components/source/kick_menuComponent.{h,cpp}`. Prioritás: **P0** összeomlás / adatvesztés / biztonság, **P1** hibás vagy hiányzó alapfunkció, **P2** javítás / kényelem. Méret: S ≤ fél nap, M ≤ 2 nap, L > 2 nap.

## 1. Áttekintés

**Mit csinál.** Az alkalmazás mód-menüje: 4×4-es csempe-rács, 16 mód (`MENU/kick_AppMenuOptions.{h,cpp}`, 107 + 46 sor, a rács a közös `ui::KickMenuComponent`: `MC`). A csempék **csak ikonok** (az `addOption( { név, ikon } )` neve a téma `mainmenu/*.svg` fájlnevének felel meg; nincs szöveges felirat). Egy csempe megnyomása `sigScreenOpen( <képernyő> )` (`MENU/kick_AppMenuOptions.cpp:81-106`). A képernyő a felső sávval (`requiresControlBar() == true`) és állapotsorral jelenik meg (`.h:25-26`).

**Elérés.**
- Felső sáv „Menu” gomb (`MENU/kick_AppTopbar.cpp:88`) — a felső sáv csak a HomeListView-n és a Menün látszik.
- MPC Key 61: **MENU** gomb (`bridge:1162-1169`; Shift+MENU = File Browser); DCL `EN_MENU` → `kick_AppHome.cpp:555`. Mód-LED: MENU (`bridge:285`).
- A hardveres MENU / SOUNDS közvetlenül a `sigScreenClose`-t hívja (`bridge:1162-1181`), és így **megkerüli** a `KickHome::handleCommand` `prjLoaded` kapuját (`kick_AppHome.cpp:487`) — projekt nélkül is megnyílik.

**Csempe-térkép és Key 61 elérés.**
| # | Csempe (ikon-név) | Képernyő | Egyéb MPC-hardveres út | Megjegyzés |
|---|---|---|---|---|
| 0 | Browser | `MenuFileBrowser` | SOUNDS, Shift+MENU | `bridge:1171-1181` |
| 1 | Track Mixer | `MenuChannelMixer` | MIX | `bridge:1243-1249` |
| 2 | Track Mutes | `MenuTrackMute` | MUTE | `bridge:1257-1263` |
| 3 | Sequences | **`MenuNextSequence`** | **nincs** (a `HW_BTN_NEXT_SEQ` csak `kick_cmd`-ból érhető: `bridge:1366, 1987`; az `akai_map.h`-ban nincs) | a csempe neve „Sequences”, a képernyő „Next Sequence” |
| 4 | listedit | `MenuListEdit` | TC | `kick_AppHome.cpp:518-519` |
| 5 | Padmixer | `MenuPadMixer` | Shift+MIX | `bridge:1243-1247` |
| 6 | Padmutes | `MenuPadMute` | Shift+MUTE | `bridge:1257-1261` |
| 7 | songs | `MenuSong` | **nincs** | — |
| 8 | Coloreditor | `MenuColorEdit` | **nincs** (a Program opciók is nyitják: `M/kick_ui/program_view/source/kick_AppProgramOptions.cpp:67`) | — |
| 9 | Sampler | `MenuSampler` | Shift+SAMPLE EDIT | `bridge:1296` |
| 10 | Sample editor | `MenuSampleEdit` | SAMPLE EDIT | `bridge:1298` |
| 11 | Program edit | `MenuProgramEdit` | EDIT | `bridge:1225` |
| 12 | Padnotes | `MenuPadPerform` | KEYBOARD CONTROL, Shift+ARP | `bridge:1028, 1289` |
| 13 | Looper | `MenuLooper` | **nincs** (a `bridge`-ben csak a LED-környezet említi: `:244`) | — |
| 14 | Qlinks | `MenuKnobEdit` | Shift+Q-LINK | `bridge:1198` |
| 15 | Midicontrol | `MenuMidiControl` | Shift+KEYBOARD CONTROL | `bridge:1287` |

**Nincs a menüben** (más úton érhető): Grid (GRID), Step Sequencer (Shift+GRID), XY / Touch FX (TOUCH FX, Shift+EDIT), Preferences (felső sáv fogaskerék; **Shift+METRONOME** = CONFIG → `EN_PREF`: `bridge:1014-1018`), Projects (felső sáv „New File”), Assign Sample, Inserts, SW Update, Process Edit. A `Screen::CVClib` regisztrált (`kick_AppHome.cpp:313`), de a `Kick/modules` és `Kick/apps` `grep`-je **nem talál nyitó hívást** — árva képernyő vagy más úton nyílik; NEM ELLENŐRIZHETŐ KÓDBÓL.

**Hardver-kötések a Menün.**
| Vezérlő | Mit tesz | Kód | Hardveres állapot |
|---|---|---|---|
| Data Dial forgatás, −/+ | a kiemelt csempe lineáris léptetése (nincs fel/le sor, nincs körbefordulás) | `MC:175-185`; `KickControlBase::handleCommand` | nem mért |
| Data Dial nyomás / ENTER | a kiemelt csempe `triggerClick()` | `MC:187-193` | nem mért |
| Pad (16) | csempe megnyitása: `idx = 15 - (pad % 16)` | `MC:195-201` | nem mért; elrendezés-gyanú (B3) |
| PLAY / PLAY START / STOP | `ae.setSequencePlay` / `setSequencePause` | `MENU/kick_AppMenuOptions.cpp:60-75` | nem mért ezen a képernyőn |
| Bank gombok, Full/Half Level | `KickHome` kezeli | `kick_AppHome.cpp:523-540` | A–D LED igazolt |
| Q-Link, Touch Strip | nincs kötés | — | — |
Érintés: egy koppintás csempét nyit (`KickImageButton::onClick`, `MC:96-103`).

## 2. Funkciók és állapotuk

| Funkció | Állapot | Bizonyíték |
|---|---|---|
| 16 csempe → a megfelelő képernyő megnyitása | KÉSZ | `MENU/kick_AppMenuOptions.cpp:81-106`; a 16 ikon mindkét témában létezik (`Kick/assets/{Default,Proper}/mainmenu/*.svg`; a név → fájl leképezés `Theme::getDrawable`-ben — N/E) |
| Data Dial / ± / ENTER navigáció | RÉSZLEGES | `MC:175-193`; lineáris, 16 detent a végigjáráshoz |
| Pad → csempe | RÉSZLEGES (elrendezés-gyanú) | `MC:195-201` (B3) |
| Play / Play Start / Stop a Menün | KÉSZ | `kick_AppMenuOptions.cpp:60-75` |
| Visszatérés az előző módba (MENU újra) | **HIÁNYZIK / BUGOS** | `MENU/kick_AppTopbar.cpp:88`; `bridge:1162-1169`; `kick_AppHome.cpp:212-231` (B1) |
| Vissza gomb a Menüben | HIÁNYZIK | a Menü a felső sávot használja (nincs `KickModeHeader`) |
| Projekt nélküli védelem | HIÁNYZIK | `bridge:1162-1181` kapu-megkerülés (B2) |
| Szöveges csempe-feliratok | HIÁNYZIK | `addOption( { "listedit", "listedit" } )` stb. — nincs `Name`-ből felirat |
| Teljes képernyő-lefedés (Grid, Step Seq, XY, Preferences, Projects, Save, Sounds, Inserts, …) | HIÁNYZIK | lásd 1. |
| Több oldal / nyilak, átrendezés, Reset Mode Menu, gyorsparancs-panel | HIÁNYZIK | WP-G G-005, G-010, G-011, G-012 |
| Menü-eszköztár (Sync mező, Help/QR, Undo History, hangoló, MIDI-monitor ikon, Controller Mode) | HIÁNYZIK | WP-G G-013 – G-024 |
| Mód-LED (MENU) | KÉSZ | `bridge:285` |
| Kiemelés (hover) a navigáció pozíciójából | KÉSZ | `MC:164-173` |

## 3. Mi nem működik / hibák

**B1 — A Menü nem „toggle”, a Back sem a Menühöz tér vissza, P1.** (a) A felső sáv Menu gombja és a hardveres MENU is `sigScreenClose( HomeOptions )`-t küld (`MENU/kick_AppTopbar.cpp:88`, `bridge:1162-1169`). Ha a Menü már nyitva van, ez ugyanazt a képernyőt tölti újra (`kick_AppHome.cpp:212-231`: `NewScrn != Invalid` → `loadScreen = HomeOptions`; a verem tetejét csak a *jelenlegivel* azonosat pörgeti ki) — az Akai szerint a Menu gomb újbóli megnyomása visszavisz az előző módba (WP-G G-004), a WP-G „OK” ítélete a kódra nem igaz. (b) A `sigScreenOpen` nem tesz a verembe, ha az aktuális képernyő a Menü vagy a HomeListView (`kick_AppHome.cpp:200-204`): Menü → csempe → képernyő → *Back* a **HomeListView**-ra, nem a Menüre ugrik. A Menüből indított munkafolyamat ezért mindig a főnézetben végződik.

**B2 — Kapu-megkerülés: Menü és csempék projekt nélkül, P1.** A `KickHome::handleCommand` a hardveres gombokat csak `prjLoaded && !isProjectLoading` esetén dolgozza fel (`kick_AppHome.cpp:487`), de a bridge MENU / SOUNDS / MAIN gombjai közvetlenül `sigScreenClose`-t hívnak (`bridge:1162-1197`), a csempék `sigScreenOpen`-t, és a `KickHome::load` nem ellenőriz projektet (`kick_AppHome.cpp:346-353`). Projekt nélkül (pl. hiányzó Demo 01, sikertelen betöltés) vagy betöltés közben (`isProjectLoading`) olyan képernyők nyílnak, amelyek betöltött projektet feltételeznek (Sampler, Program Edit, Pad Mixer, …) — a null-védelmek ezeken nem vizsgáltak: NEM ELLENŐRIZHETŐ KÓDBÓL, hogy összeomlik-e. A `MenuItem::enabled` mező létezik (`MC:31`), de az `AppMenuOptions` nem használja.

**B3 — Pad → csempe leképezés vélhetően vízszintesen tükrözött MPC-n, P1 (MPC-n ellenőrizendő).** `idx = 15 - (PadNo % 16)` (`MC:197-198`). Az Akai pad-számozás: Pad 1 = A01 = index 0 (`A/hardware/akai_map.h:20-42`), az MPC-n a Pad 1 a bal alsó pad (4×4, soronként balról jobbra, alulról felfelé — az MPC-konvenció, a fizikai elrendezés itt nem mért). A csempék balról jobbra, *fentről lefelé* számozódnak, tehát a pad (sor `r` alulról, oszlop `c`) csempéje `(3 − r)·4 + c`; a kód `15 − (4r + c) = 4·(3 − r) + (3 − c)` → az oszlop tükrözött (`c → 3 − c`). Pl. bal alsó pad (Pad 1) → jobb alsó csempe (Midicontrol), bal felső pad (Pad 13) → jobb felső csempe (Sequences). A Behringer-hardver pad-számozásához készült. Nincs határellenőrzés (`model[navGroup]->items[idx]`; 16 csempénél nem fut ki, de általánosítva igen). A hívó állapota: csak `BTN_PRESSED`-re nyit (helyes).

**B4 — Lineáris navigáció, P2.** A Data Dial és a −/+ is csak ±1 detent (`MC:179`), a rács 4 oszlopú: a 4×4 elrendezésben nincs sor-ugrás (pl. Shift+Dial = ±4), és nincs körbefordulás; az utolsó csempe után a további forgatás elakad. A kiemelt pozíció a `navSelect` (privát), a `show()` nullázza (`MC:57-64`), de az `AppMenuOptions` nem hívja — a pozíció megmarad a két megnyitás között (jó), viszont soha nem tér vissza az *aktuális* módhoz.

**B5 — Csempék szöveg nélkül, rossz hiba-viselkedés, P2.** Az ikon-név és a csempe-név azonos és nem egységes („listedit”, „songs”, „Padnotes”, „Qlinks”, „Midicontrol” vs. „Track Mixer”, „Sample editor”). A név → SVG-fájl leképezés (`trackmixers.svg`, `program edits.svg`) a téma-betöltőben él; hiányzó ikon esetén a viselkedés (üres gomb?) NEM ELLENŐRIZHETŐ KÓDBÓL. A „Sequences” csempe a Next Sequence-t nyitja — a felhasználó a sorozatlistát (Main mód / Sequence tab) várná.

**B6 — Halott hívások, P2.** `visibilityChanged()` csak továbbhív (`kick_AppMenuOptions.cpp:77-79`); `getSelectedId()`/`selectedId` nincs használatban (`.h:19, 37`); az `onMenuOptionClicked` a `Group` és a `Name` paramétert nem használja (`:50-52`), a `//hide();` kikommentezett (`MC:101`).

## 4. Tervben / hiányzik (Akai MPC Standalone-hoz képest)

Forrás: `wp-g.md` (G-001 – G-024, G-044, G-285 – G-286), `doc-vs-kick.md` (24. sor: „Menu / Browse: a mód-menü; Shift: Browser — nem vizsgált: a menü tartalma”), `known-issues-and-roadmap.md` §3.1 („Menu tartalma”).
- **Menü-tartalom**: Project ablak (G-008, G-033), New Project (G-006), Save (G-007), Preferences gomb a Menüben (G-009, G-044), Sounds (G-286), Browser, módok.
- **Oldalak és testreszabás**: oldalnyilak (G-005), húzással átrendezés (G-010), három pont → Reset Mode Menu (G-012), fekete fogantyú → bal oldali 5 mód gyorspanel (G-011).
- **Eszköztár a Menü fölött**: Sync mező — MIDI Clock / MTC / Ableton Link (G-013, G-014 NEM-KICK), Help + QR (G-015), Undo History (G-016 – G-018), hangoló (G-019 – G-021), MIDI-monitor ikonok (G-022), Controller Mode (G-024 NEM-KICK), projekt-idő számláló pull-down (G-206, G-207). 
- **Rendszer-erőforrás ablak** (G-025 – G-032): a Kickben a fejléc gombjáról nyílik; Purge-gombok üresek (`kick_ui_systemResources.cpp:78-79`).
- **Hiányzó Key 61 elérések**: a Next Sequence, Song, Color Edit, Looper képernyőknek nincs hardveres gombja a Kick Key 61-es kiosztásában (`akai_map.h`), csak a Menü csempéje. Az Akai-féle elérésük **NEM ELLENŐRIZHETŐ**: az útmutató erre vonatkozó fejezetei (WP-B: Next Sequence, Song, Looper; WP-C: Pad Color) még nincsenek feldolgozva (`walkthrough-plan.md`).
- A Menü a Kickben nem listáz Grid / Step Seq / XY / Preferences / Projects-et; az Akai menü a **rendelkezésre álló módokat, projektinfót, beállításokat, mentési és betöltési lehetőségeket** mutatja (G-001).

## 5. Javaslataim

1. **Menü mint „mód-váltó + parancs-központ”.** Egy `ModeRegistry` (a `KickHome::screens_` regisztrációból generálva) határozza meg a csempéket: ikon, felirat, képernyő, *feltétel* (projekt szükséges?), hardveres megfelelő. A Menü így nem tartalmaz kézzel karbantartott 16-os `switch`-et, és a hiányzó képernyők (Grid, Step Seq, XY, Preferences, Projects, Save) hozzáadása egy sor. Indoklás: B2, B5, a regisztrációs és a menü-lista eltérése.
2. **Projekt-kapu a Menüben**: projekt nélkül a projektfüggő csempék szürkék (`MenuItem::enabled`) és a bridge-ből jövő MENU/SOUNDS ugyanazt a kaput használja, mint a `KickHome::handleCommand`. A Projects / Preferences / Browser / SW Update projekt nélkül is nyitható marad.
3. **Toggle viselkedés és Back-verem**: `lastNonMenuScreen` megjegyzése; második MENU → oda vissza; csempe → képernyő → Back → Menü (a verembe tegye a Menüt is). Indoklás: B1, Akai G-004.
4. **Hardveres út a hiányzó képernyőkhöz**: a Key 61-en nincs Next Sequence / Song / Color Edit / Looper gomb — a Menün át (Data Dial + ENTER, vagy **pad = csempe**) kell elérhetővé tenni, ezért a pad-leképezés (B3) és a Dial-navigáció (B4: Shift = sor) legyen megbízható; a csempe neve kerüljön az állapotsorba/hover-szövegbe, hogy a Dial-lal vakon se kelljen találgatni.
5. **Szöveges feliratok** a csempék alatt (téma-szín, kis betű) — hozzáférhetőség és hardveres navigáció (a kiemelt csempe neve olvasható legyen).
6. **Testreszabhatóság** (később, Akai-paritás réteg): oldalak, átrendezés, Reset Mode Menu — a settings fába mentett sorrend (G-005, G-010, G-012).
7. **Tesztelhetőség**: a csempe-index → képernyő leképezés tiszta függvénybe (`menuTileScreen( int )`), host-teszt, hogy minden index egy regisztrált képernyőt ad és nincs duplikátum; a pad → csempe leképezés külön tiszta függvény (`padToTile( pad )`) host-teszttel és az MPC pad-elrendezésével összehasonlítva. A 16 ikon-név létezésének ellenőrzése mindkét témában (asset-teszt).

## 6. Hardveres és unit-tesztek

**Ami ma fedi.**
- `AppSmoke.py` / `KICK_SMOKE_SCREENS`: `HomeOptions` képernyőkép készíthető (`A/Main.cpp:152-198`); az alapteszt nem járja végig, a csempék megnyitását nem ellenőrzi.
- `AkaiMapTests` (pad- és gombtérkép), `LedMapTests`, `PadColourMapTests`: a bridge térképeit fedik, a Menü-leképezést (`15 − pad`) nem.
- Nincs teszt: csempe → képernyő, Dial-navigáció, pad → csempe, MENU toggle, Menü projekt nélkül.
- A `doc-vs-kick.md` 24. sora szerint a Menü *tartalma* nem vizsgált.

**MPC-n ellenőrzendők.**
1. MENU gomb: megnyílik-e a Menü; második nyomás hatása (B1); Shift+MENU → File Browser.
2. Pad 1 / 4 / 13 / 16 melyik csempét nyitja (B3); a Menü pad-LED-jei (a híd az egyes pad-LED parancsokat eldobja: `bridge:138-160`, így a csempéknek nincs LED-visszajelzése — N/E).
3. Data Dial: 16 detent végig; ± gombok; Dial-nyomás a kiemelt csempén; a kiemelés látszik-e (a `setState( buttonOver )` az MPC témájában).
4. Projekt nélküli állapot szimulálása (hiányzó `/data/demos/Demo 01`): MENU → csempék → összeomlás-e (B2).
5. Minden csempe megnyit-e képernyőt (különösen Looper, Song, Color Edit, Sequences = Next Sequence) és hogyan lehet visszalépni (Back → HomeListView, B1).
6. Mind a 16 ikon megjelenik-e az MPC-n (téma `Default` / `Proper`).
7. PLAY / STOP a Menün (`kick_AppMenuOptions.cpp:60-75`) — a sorozat-indítás a kiválasztott sorozaton.

## 7. TODO

Függőségek: MENU-04 ← MENU-01 (verem-modell); MENU-07 ← PRJ-14 (Save As), Settings; MENU-12 ← a közös képernyő-regisztráció (`kick_AppHome.cpp:262-315`).

*Hibák*
- [x] **MENU-01** (P1, S) Toggle és Back-verem: `lastNonMenuScreen`, a második MENU / Menu gomb visszatér, csempéből indított képernyő Back-je a Menüre — `MENU/kick_AppTopbar.cpp:88`, `bridge:1162-1169`, `kick_AppHome.cpp:196-231`. (WP-G G-004 „OK” ítélet helyesbítése.)
  **R-7:** verified in code: `ScreenNav` (kick_menu_logic.h) = MENU toggle + Back to the Menu, host-tested (S-D work).
- [x] **MENU-02** (P1, S) Projekt-kapu a Menüre és a csempékre (szürke projektfüggő csempék; a bridge MENU/SOUNDS/MAIN ugyanazt a kaput használja, mint a `handleCommand`) — `bridge:1162-1197`, `kick_AppHome.cpp:346-353, 487`, `MC:31`. MPC-n ellenőrizendő a tényleges összeomlás.
  **R-7:** verified in code: `itemNeedsProject` / `screenNeedsProject`, project-less tiles are grayed out and ignore touch, dial and pad; tile-open smoke below covers it (S-D work).
- [x] **MENU-03** (P1, S) Pad → csempe leképezés javítása az MPC pad-elrendezéséhez (és határellenőrzés); tiszta függvény + host-teszt — `MC:195-201`, `A/hardware/akai_map.h:20-42`. MPC-n ellenőrizendő (6./2.).
  **R-7:** `padToTile` (kick_menu_logic.h), host-tested; the pad layout itself still has to be checked on the MPC (MENU-16).
- [x] **MENU-04** (P2, S) Dial/± navigáció: Shift+Dial = sor-ugrás, körbefordulás, a kiemelés az aktuális módra áll be megnyitáskor — `MC:175-185, 57-64`.
  **R-7:** `stepTile`: wrap, Shift + dial = one row; the highlight starts on the mode the Menu was opened from.
- [~] **MENU-05** (P2, S) Csempe-feliratok és egységes ikon-/csempe-nevek; hiányzó ikon fallback — `kick_AppMenuOptions.cpp:27-45`, `MC:91-103`.
  **R-7:** The tile name is shown in the status bar (Box Info) while the dial / pad / touch highlight moves; the four text tiles (Save, Resources, Kick Update, Favorites) show their name. Not done: a caption under the icon tiles (the themes' icons have no text; a caption needs a design decision).
- [x] **MENU-06** (P2, S) Halott kód takarítás (`getSelectedId`, `selectedId`, üres `visibilityChanged`, `//hide()`) — `kick_AppMenuOptions.{h,cpp}`, `MC:101`.
  **R-7:** AppMenuOptions rewritten on the registry; the dead calls and members are gone.

*Hiányzó funkciók*
- [x] **MENU-07** (P1, M) Hiányzó útvonalak a Menüben/felső sávon: Grid, Step Sequencer, XY / Touch FX, Preferences, Projects, **Save / Save As** (PRJ-14), Sounds/Favorites, SW Update; az árva `CVClib` útvonal tisztázása (`kick_AppHome.cpp:313`). Függ: MENU-12.
  **R-7:** R-7: Favorites tile (the browser opens on its next assigned favourite). There is no separate Sounds screen in Kick: the SOUNDS button and the Browser tile open the same file browser. `Screen::CVClib` is not orphaned: Preferences > CV opens it (`kick_ui_settings_cv.cpp:93`; CV code is not touched here). Grid / Step Sequencer / XY / Preferences / Projects stay on their buttons and the top bar (the Menu keeps 16 pad-reachable mode tiles + commands).
- [ ] **MENU-08** (P2, M) Hardveres elérés a Key 61-en hiányzó képernyőkhöz (Next Sequence, Song, Color Edit, Looper): a Menün át Dial+ENTER és pad; esetleg Shift+gomb-kiosztás döntéssel (nincs szabad dedikált gomb) — döntés kell.
- [x] **MENU-09** (P2, M) Oldalak (nyilak), átrendezés, Reset Mode Menu, gyorsparancs-panel (G-005, G-010 – G-012); a sorrend a settings fában.
  **R-7:** Pages of 16 tiles (the 16 pads select the tiles of the shown page), page arrows, "Move tiles" (touch a tile, then its new place; Shift + Data Dial press toggles the mode, the dial carries the picked-up tile over page edges), "Reset menu"; the order is stored in `<prefs>/menu_layout.txt` and read leniently (`MenuLayout`, host-tested). Not done: the left quick panel of 5 modes (G-011) and dragging a tile with a finger (tap, tap is used instead). To check on the MPC: Shift + Data Dial press, pad mapping on page 2.
- [ ] **MENU-10** (P2, M) Menü-eszköztár: Sync mező (MIDI Clock / MTC; Link NEM-KICK), Help/QR, Undo History, hangoló, MIDI-monitor ikon (G-013 – G-022) — Akai-paritás réteg, owner döntés.
  **R-7:** Not done: owner decision (Akai parity layer; Link / controller mode are NEM-KICK). The pieces that exist elsewhere: Undo History (Main toolbar), tuner / Help / QR / MIDI monitor / Sync field do not exist. Needs: which of G-013..G-022 the owner wants on the Menu.

*Javaslatok*
- [x] **MENU-11** (P2, S) Az aktuális mód jelzése a Menüben (kiemelt csempe) és a név/hover-szöveg az állapotsorban Dial-navigáció közben.
  **R-7:** Dial / pad / touch highlight writes the tile name to the status bar.
- [x] **MENU-12** (P1, M) `ModeRegistry`: a csempék és a hardveres kötések egy forrásból (ikon, felirat, képernyő, feltétel, HW-megfelelő) — a 16-os `switch` kiváltása; a `KickHome` regisztráció és a Menü közös adatból.
  **R-7:** `tileDef<S>( item )` in kick_menu_logic.h is the one table (button name, theme icon, label, screen); the Menu, the tile gate, the tests and the smoke use it. The hardware button bindings stay in kick_hardware_bridge.h (R-3); the KickHome screen registration stays a separate list (screens are built lazily there).

*Tesztek*
- [x] **MENU-13** (P1, S) `menuTileScreen( int )` és `padToTile( int )` tiszta függvények + host-teszt (minden index érvényes, nincs duplikátum, MPC pad-elrendezés). Függ: MENU-03, MENU-12.
  **R-7:** MenuLogicTests (S-D) + registry and layout tests (R-7).
- [x] **MENU-14** (P2, S) Asset-teszt: mind a 16 ikon-név létezik mindkét témában (`Default`, `Proper`).
  **R-7:** MenuAssetTests: every icon of the registry exists in `assets/Default` and `assets/Proper` (theme.xml entry + file); registered as `kick-menu-assets`.
- [x] **MENU-15** (P2, S) AppSmoke: `HomeOptions` képernyőkép (`KICK_SMOKE_SCREENS=HomeOptions`), + csempe-megnyitás smoke (projekttel és anélkül).
  **R-7:** `KICK_SMOKE_MENUTILES=1` presses all 21 tiles (each on its page) without and with a project: grayed out and closed without a project, opens its screen with one; `ProjectsBrowserSmoke.py menutiles`, ctest `kick-menu-tiles-smoke`.
- [ ] **MENU-16** (P1, S) MPC hardveres ellenőrzőlista végrehajtása (6. szakasz) — a hardveres session végezze.
  **R-7:** Hardware session: see the R-7 entries of the hardware checklist (Menu pages / Move, Shift + Data Dial, pad mapping on page 2, status bar hover text).
