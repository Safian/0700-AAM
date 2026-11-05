# Képernyő-backlog (S-C, S-D, S-E, S-F maradékok)

A 22 képernyő-review dokumentum (2026-11-04, statikus kódolvasás) összevonva: csak a **nyitott** sorok és a körök végén írt állapot-szakaszok maradtak, a kész sorok és a review-leírások törölve. A sor-azonosítók leírása és a teljes review: `git show 4fa7f88f:docs/planning/plans/screens/<képernyő>.md`. Az S-A/S-B képernyők (assign-sample, sample-edit, sampler, channel-mixer, pad-mixer, pad-mute-track-mute) maradéka a [todo-master.md](todo-master.md) R-1/R-4 soraiban van.
A `G-xxx` hivatkozások a törölt `wp-g.md` / `wp-d.md` sorai (`git show 4fa7f88f:docs/hardware/key61/walkthrough/wp-g.md`).
Rövidítések: `M/` = `Kick/modules/`, `A/` = `Kick/apps/kick/`, `B:` / `bridge` = `Kick/apps/kick/kick_hardware_bridge.h`. `[ ]` nyitott, `[~]` részleges; a "S-C/R-8/R-11/S-F" szakaszok a körök végi állapotok, a "Kész" tételek mellett a nyitottak és az owner-döntések a mérvadók. Hardveres ellenőrzés: [hw-test-checklist.md](../hardware/key61/hw-test-checklist.md).

## Menu (HomeOptions) — review és TODO

- [~] **MENU-05** (P2, S) Csempe-feliratok és egységes ikon-/csempe-nevek; hiányzó ikon fallback — `kick_AppMenuOptions.cpp:27-45`, `MC:91-103`.
  **R-7:** The tile name is shown in the status bar (Box Info) while the dial / pad / touch highlight moves; the four text tiles (Save, Resources, Kick Update, Favorites) show their name. Not done: a caption under the icon tiles (the themes' icons have no text; a caption needs a design decision).
- [ ] **MENU-08** (P2, M) Hardveres elérés a Key 61-en hiányzó képernyőkhöz (Next Sequence, Song, Color Edit, Looper): a Menün át Dial+ENTER és pad; esetleg Shift+gomb-kiosztás döntéssel (nincs szabad dedikált gomb) — döntés kell.
- [ ] **MENU-10** (P2, M) Menü-eszköztár: Sync mező (MIDI Clock / MTC; Link NEM-KICK), Help/QR, Undo History, hangoló, MIDI-monitor ikon (G-013 – G-022) — Akai-paritás réteg, owner döntés.
  **R-7:** Not done: owner decision (Akai parity layer; Link / controller mode are NEM-KICK). The pieces that exist elsewhere: Undo History (Main toolbar), tuner / Help / QR / MIDI monitor / Sync field do not exist. Needs: which of G-013..G-022 the owner wants on the Menu.
- [ ] **MENU-16** (P1, S) MPC hardveres ellenőrzőlista végrehajtása (6. szakasz) — a hardveres session végezze.
  **R-7:** Hardware session: see the R-7 entries of the hardware checklist (Menu pages / Move, Shift + Data Dial, pad mapping on page 2, status bar hover text).

## Projects (RecentProject) — review és TODO

- [ ] **PRJ-01** (P0, S) `LoadProjectAndAnimation::startLoading`: újrabelépés-védelem (`isProjectLoading` esetén elutasít / sorba tesz), a `proj` mező átadása a szálnak zár alatt; a hívók (FB dupla koppintás, csempék, autoload, long-press Load) közös belépési ponton át — `RP/kick_RecentProject_Utils.cpp:20-48`. Kapcsolódik: FB-15.
- [ ] **PRJ-02** (P0, S) Sablon → projekt: a cél létezésének ellenőrzése a *lemezen* (nincs felülírás), `sourceFiles.isEmpty()` védelem, üres név tiltása, hiba esetén a félkész projektmappa törlése — `RP/kick_RecentProject.cpp:158-170`.
- [ ] **PRJ-03** (P1, S) Data Dial / long-press Load: a csempe saját `path`-ját adja át (`recentItems[selectedIndex]` útvonal), ne `juce::File()`-t — `:376-381, 23-26`.
- [ ] **PRJ-08** (P1, M) New project: üres / érvénytelen karakterű / lemezen létező név tiltása (nem a recent listán); trim, hossz-korlát; közös a FB-vel — `RP/kick_RecentProject.cpp:107-130`. Kapcsolódik: FB-16.
- [ ] **PRJ-11** (P2, S) Csempe-élettartam: `SafePointer` a `callAfterDelay` lambdában; a `setNumOptions` sorrendje (előbb `projectList.clear()`) — `:215-226, 680-686`.
- [ ] **PRJ-25** (P1, M) MPC hardveres ellenőrzőlista végrehajtása (6. szakasz) — a hardveres session végezze.
  **R-7:** Hardware session: not done here. Steps: see the R-7 hardware checklist entries (recent list order after re-opening a project, template copy, rename / delete from the tile menu).

## File Browser — review és TODO

- [ ] **FB-01** (P0, S) Delete megerősítés: lebegő `popup` hivatkozás megszüntetése (érték szerinti capture, törlés a modális callback `APPLY` ágán) — `FB/kick_fileBrowser.cpp:1390-1409`.
- [ ] **FB-02** (P0, M) Place-enkénti gyökér: a Back / `exitDirectory` / keresés ne léphessen ki a Place-ből; védett útvonal-lista (`/`, `/data`, `/data/projects`, `/data/sounds`, `/data/demos`, a jelenlegi projektmappa és ősei) a Delete / Cut / Rename / Paste előtt; `checkIsCurrProj` útvonal-alapú (ős/leszármazott is) — `:16, 1133, 1145-1158, 1323-1338, 453-466`.
- [ ] **FB-03** (P0, M) Paste / Rename / New: létező cél felülírásának tiltása vagy megerősítése („(2)” utótag), mappa önmagába másolásának tiltása, névvalidáció (`/`, `..`, vezérlőkarakter, FAT-tiltott karakterek, hossz, kiterjesztés-duplázás) — `:292-329, 975-1029`.
- [~] **FB-24** (P2, M) Browser Options (oszlop-kapcsolók, rekurzív keresés kapcsoló, adatmappák elrejtése), Open gomb, Drive Info, további szűrők (pattern, kit, plugin preset) — G-242, G-256, G-258 – G-265.  
  **R-6:** Browser Options (columns, sub-folder search, hide system files / project data folders, copy samples into the project, MIDI filter), Open button, Drive Info. Not done: filters for kits and plugin presets (Kick has no such file format).
- [~] **FB-25** (P2, M) Előhallgatás: tartott Play, Sync, Warp; nagy fájl háttérbetöltés — G-250, G-254, G-255.  
  **R-6:** Held Play (MAN mode) auditions while the finger is on the button. Not done: Sync / Warp (the engine preview has no tempo sync / time stretch: an audio feature, needs a decision) and background loading of a big file (the engine's `previewOn` call; needs a measurement on the MPC).
- [~] **FB-27** (P2, L) WebDAV forrás (M1: PROPFIND, Range GET, TLS, `.part` folytatás); HTTP-kliens döntés (libcurl vs. más; `JUCE_USE_CURL=0`), ARMv7 keresztfordítás. Függ: FB-26. Jogi átnézés (libcurl/TLS licenc) előfeltétel.
  **R-7:** M1 exists (W2-C): WebDavSource on JUCE URL (PROPFIND, Range GET), shared cache with `.part` + Range resume, download queue, host + local-server tests; plain http:// works everywhere. Not done: https:// on Linux / MPC (JUCE_USE_CURL=0 has no TLS). Needs the owner: libcurl (or mbedTLS) yes / no after the licence review, then an ARMv7 cross build + a run on the MPC.
- [ ] **FB-28** (P2, L) Nyilvános Drive-mappa + BYO API-kulcs (M2); OneDrive eszköz-folyam (M3). **Blokkoló: jogi átnézés** (Google ToS 4(b)(1) vs. AGPL, Splice ToS) — `cloud-sources.md` §3.6. Függ: FB-27.
  **R-7:** Not done: blocked by the legal review (Google ToS 4(b)(1) vs AGPL, Splice ToS) in cloud-sources.md section 3.6, and it depends on https (FB-27).
- [ ] **FB-38** (P1, M) MPC hardveres ellenőrzőlista végrehajtása (6. szakasz) — a hardveres session végezze.
  **R-7:** Hardware session: not done here.

## Fejléc, állapotsor, popupok és dialógusok — review és TODO

- [ ] **HDR-01** (P0, M) Humanize idő/hossz/velocity képlet javítása, `eagerness` bekötése, `jlimit`, UI-ból Velocity + „only selected"; egy undo-tranzakció. ⟵ HDR-40 — `mt_audio_note.cpp:1617-1665`, `kick_GridEditOptions.{h,cpp}`
- [ ] **HDR-02** (P0, L) Általános modális hardver-útvonal: `dialogDisplayer` bérletet vesz (`claimHardwareInput`), ENTER/STOP/Data Dial-nyomás = Apply, MAIN/Shift+STOP = Cancel, Data Dial a tartalomnak; `KickHome::handleCommand` modál-őr; a TC beágyazott-popup szabály megmarad; `modal_dialog_keys.h` általánosítása. ⟵ HDR-41 — `kick_utils_popup.cpp:122-232`, `dcl_signals.cpp:37-59`, `kick_hardware_bridge.h:894-905`, `kick_AppHome.cpp:487`
- [~] **HDR-03** (P1, M) Popup-élettartam: tulajdonlás-segéd (`unique_ptr` a visszahívás minden ágán), `themeChanged` lekötés, a saját modálist zárja (`exitDialog(this)`), `getActiveTopLevelWindow()` null-őr, `FilePopup` argumentumok. — `kick_WarningComponent.cpp`, `kick_ui_settings_info.cpp:61-76, 107-135`, `kick_Locate.cpp:88, 118-122, 262, 279-283`, `kick_utils_popup.cpp:324-347`
  **R-7:** `mt::utils::showDialogOwned<T>` (content released on every exit), own-modal close and scoped theme slots in FilePopup / ConfirmationPopup / BBT / STP dialogs, null-safe popup pool. Not done: the ~50 other `new` + `delete` callers (Looper, Sampler, Mixers, ...) in other work areas move to the helper one by one.
- [ ] **HDR-05** (P1, M) BBT-popup → valódi Locate (`seekToBar/seekToTick`), 1-alapú kijelzés, tartományok, „dB"/„SEQ LENGTH" eltávolítása. — `kick_Locate.cpp:37-92, 156-176`, `mt_audio_interface.h:289-302`
- [ ] **HDR-06** (P1, M) STP-popup: visszaállítás minden CANCEL-en, lista-frissítés sequence-váltáskor, oszlopszám, Data Dial. — `kick_Locate.cpp:225-302`
- [~] **HDR-07** (P1, M) Metronome: `mode`/`style`/`countIn` bekötése, `MET_SEQ`, szinkron a `Show()`-ban, Shift+METRONOME = Click-ablak. — `mt_audio_metronome.h:42-62`, `kick_modeHeader.{h,cpp}:107-115, 304-362`, `kick_hardware_bridge.h:1013-1022`
  **R-7:** Rate list has SEQ (the click follows the time signature). Not done: Mode / Style / Count-In (hidden: the engine does not use them; the meaning of the modes is in the Akai manual, which is not in this worktree; Style needs click sounds = sound design; Count-In needs a pre-roll in the transport = record path of R-8) and the Shift+METRONOME Click window (button handler is in kick_hardware_bridge.h, R-3).
- [ ] **HDR-09** (P1, S) Numpad: `sign` újraengedése, tizedes (`dot`) a `float` ágon, `clr`/`del` jelentés, per-használat reset. — `kick_number.h:239-262, 338-355, 399, 434`
- [~] **HDR-12** (P1, M) Undo/Redo egy belépési pont, értesítés, History ablak, fülfrissítés, tiltás modális/felvétel alatt; az engine-vektorok igazítása létrehozás/törlés undo-nál (`04-screen-audit.md` §5, törölve: `git show 4fa7f88f:docs/planning/plans/04-screen-audit.md`). — `kick_AppMainMode.cpp:222-252, 326-355, 391-392`, `kick_hardware_bridge.h:1322-1329`
  **R-7:** ONE `UndoWatcher` (undo manager listener) announces "Undid: <step>" / "Redid: <step>" for every path and refreshes the active Main tab (pure classification, host-tested); the Main toolbar refuses while a take is recorded; Undo History window (HDR-21). Not done: refusing the hardware UNDO during a take (button handler in kick_hardware_bridge.h, R-3).
- [ ] **HDR-20** (P2, L) Akai Locate ablak (Time, Loop, Locators, Step, Auto Record). ⟵ HDR-05
  **R-7:** Not done (L): the Akai Locate window (Time, Loop, Locators, Step, Auto Record) needs engine locators (6 stored positions), loop-point UI and auto-record, and a layout decision; the BBT popup is a real locate now (HDR-05) and follows the time signature (HDR-04).
- [ ] **HDR-22** (P2, M) Count-In és Click-ablak teljes (Akai-paritás); TC-ikon + Automation-állapot a felső sávon (G-213/G-215). ⟵ HDR-07, HDR-08
  **R-7:** Not done: Count-In (record pre-roll in the transport, R-8 record path, audible check) and the Click window (Shift+METRONOME is in kick_hardware_bridge.h, R-3). The Automation state is shown on the header icon (dimmed when Off); a TC icon on the top bar is not done (TC = Timing Correct lives in List Edit, R-8).
- [ ] **HDR-24** (P2, M) Timing Correct: „Events: Range", perzisztens paraméterek, nyitás a Gridből (H26). — `kick_ListEdit.cpp`, `mt_audio_interface_types.h:735-738`
  **R-7:** Not done: it is all in `kick_ListEdit.cpp` / the timing-correct dialog (R-8 area). Needed: Events: Range (start / end position fields + `timingCorrectInfo` range ticks + engine), persistent TC parameters (settings tree), opening from the Grid.
- [ ] **HDR-40** (P0, M) Humanize host-teszt (tiszta függvény).
- [ ] **HDR-41** (P1, M) `ModalDialogKeysTests` általánosítása (táblázat: gomb × shift × hasApply × nested popup).
- [ ] **HDR-42** (P1, M) `TapTempo` és numpad-állapotgép host-tesztek.
- [ ] **HDR-45** (P0, M) MPC-s hardveres próba a 6. pont listájával, eredmény a `HW_TEST_REPORT`-ba; a `HW_TEST_CHECKLIST` §5 bővítése. ⟵ HDR-02

## SW Update (Firmware Update) — review és TODO

- [ ] **SW-00** (P0, S) **Owner-döntés (nincs döntés; R-7 hatás-térkép lent):** marad-e a Behringer-lemezkép frissítője (swupdate/MCU/openssl/FunctionFS)? Javaslat: nem marad (MPC-n nem fut, biztonsági teher, `04-screen-audit.md` (törölve: `git show 4fa7f88f:docs/planning/plans/04-screen-audit.md`) §8.1). Ha nem: SW-10 törlést ír, SW-02…SW-07 elesik. — 3.1, 3.7
  **R-7 impact map (nothing deleted):** if the Behringer image updater goes, these go with it: `Kick/modules/kick_ui/swupdate/kick_SWUpdateView.{h,cpp}`, `kick_SWUpdateScreen.{h,cpp}` (+ the `SWUpdateView` / `SWUpdateWindow` enum values and their registration in `kick_AppHome.cpp`), `Kick/modules/hardware/source/kick_FirmwareUpdate.{h,cpp}`, `kick_UsbFunctionFS.{h,cpp}`, the image decrypt / loop-mount code in `kick_Hwpref.{h,cpp}` (`decrptKey = "PJSDSOMKICK"`, `validateImage`, `decryptopenSSLImage`, `extractImage`), the `isHardwareControlAvailable() ? SWUpdateView : KickUpdate` branch in `kick_ui_settings_info.cpp`, the two `SWUpdateView` / `SWUpdateWindow` cases of `screenNeedsProject` in `kick_menu_logic.h` and its test stand-in enum. SW-02..07 are fixes of exactly that code and fall away with it. Nothing else references it (grep `SWUpdate`, `FirmwareUpdate`, `USBFunctionFS`).
- [ ] **SW-02** (P0, M) *(ha marad)* `deviceData` életciklus: a sor *értékmásolatot* kapjon, a `clear()` ne előzze meg az Update-et; `rowId` frissítése újrahasznosításnál; szálbiztos átadás. — `kick_SWUpdateView.cpp:102-115, 244-252`
  **R-7:** Not touched: depends on SW-00 (owner decision); see the impact map at SW-00.
- [ ] **SW-03** (P0, M) *(ha marad)* Lógó `[&]`-k és kézi `delete` felváltása (`std::unique_ptr`/értékmásolt capture); `FilePopup` argumentumok javítása. — `kick_SWUpdateView.cpp:143-172`, `kick_SWUpdateScreen.cpp:122-141`
  **R-7:** Not touched: depends on SW-00 (owner decision); see the impact map at SW-00.
- [ ] **SW-04** (P0, M) *(ha marad)* A frissítő/USB szálak csak `MessageManager::callAsync`-on át érintsék a UI-t; az állapot (`_overallLabel`, verziók) `std::atomic`/mutex mögött; `static` verziók a fejlécből a `.cpp`-be, szálbiztosan. — `kick_SWUpdateScreen.cpp:98-228`, `kick_SWUpdateView.h:8-10`
  **R-7:** Not touched: depends on SW-00 (owner decision); see the impact map at SW-00.
- [ ] **SW-05** (P1, M) *(ha marad)* Időkorlát + megszakítás a `linuxUpdate` ciklusaira, `exit(1)` eltávolítása, kijárat a `SWUpdateScreen`-ről hibánál/időtúllépésnél. — `kick_FirmwareUpdate.cpp:36-70, 453-558`, `kick_SWUpdateScreen.cpp:14, 95`
  **R-7:** Not touched: depends on SW-00 (owner decision); see the impact map at SW-00.
- [ ] **SW-06** (P1, S) *(ha marad)* Apróbb hibák: `||`-konstans (`:178`), `U_FAILED` felirat (`:211`), `U_TIMEOUT` popup (`:222`), `onThreadEnd` a `startThread` előtt, `isFirmwarePresent` `break`-ek, tábla-magasság, „Apply" gomb törlése, beégetett „v1.21/Improves stability".
  **R-7:** Not touched: depends on SW-00 (owner decision); see the impact map at SW-00.
- [ ] **SW-07** (P0, M) *(ha marad)* Biztonság: kulcs ne legyen a forrásban/argv-ban, parancsok `execve`-szerű, idézett paraméterekkel (ne shell-string), `mount` kezelése, `reboot -nf` → `sync` + normál újraindítás. — `kick_Hwpref.h:188`, `kick_Hwpref.cpp:228, 471, 509`, `kick_SWUpdateScreen.cpp:126`
  **R-7:** Not touched: depends on SW-00 (owner decision); see the impact map at SW-00.
- [ ] **SW-10** (P0, L) „Kick Update" képernyő (MPC-natív): telepített/pendrive-os verzió, állapotok, „Restart to install", `Kick.prev` visszalépés-gomb, hibaokok; Data Dial/ENTER/MAIN kezelés; törli/lecseréli a Behringer-nézeteket (SW-00 szerint). ⟵ SW-11, SW-12, SW-13
- [ ] **SW-11** (P0, M) `KickVersion` (header-only): `VERSION`/`Kick.version` parser, összehasonlítás, beégetett build-azonosító; az Info oldal „Firmware Version/Build Date" ebből (G-049/G-053). — `kick_ui_settings_info.cpp:85-99`, `make_pendrive_package.sh:234-239`
- [ ] **SW-12** (P1, M) Pendrive-csomag keresése `/proc/mounts` alapján (a `mountPointOf` logika újrahasznosítása/kiemelése), háttérszálon, időkorláttal; több pendrive. — `kick_fileUtils.cpp:8-45`
- [ ] **SW-13** (P1, M) Újraindítás-folyamat: dirty-projekt mentése, lejátszás/felvétel/betöltés tiltása, kilépés/újraindítás módja; ⟵ hardveres ellenőrzés: `systemctl restart acvs` hívható-e a Kick folyamatból (N/E).
  **R-7:** Not done: the restart method is undecided. To decide / measure on the MPC: whether `systemctl restart acvs` (or quitting Kick so the launcher restarts it) can be called from the Kick process and brings the bootstrap up again with the USB drive inserted. The screen documents the manual way (power cycle). The app side must also refuse while playing / recording / loading and ask to save a dirty project first.
- [~] **SW-14** (P1, M) Visszalépés: bootstrap `ROLLBACK`-trigger + `test_tkgl_bootstrap.sh` scenario + UI-gomb. ⟵ SW-08
  **R-7:** Bootstrap side done: a `ROLLBACK` file on the USB drive or `/data/Kick.rollback` restores `Kick.prev` (SHA256 checked against `Kick.version.prev`), the rolled-back package is remembered in `Kick.rejected` and not installed again from the same pendrive until a different package appears. Not done: the UI button (needs SW-13: how the app restarts).
- [ ] **SW-16** (P2, L) Online frissítés (OTA): letöltés `.part`+hash, a bootstrappal azonos telepítő; ⟵ hálózat (iwd), `architecture-and-publishing.md` döntései, SW-00/SW-10; owner-döntés.
  **R-7:** Not done: owner decision (network: iwd on the MPC, TLS client, the architecture-and-publishing decisions).
- [ ] **SW-17** (P2, S) Hitelesség: aláírt `VERSION` (ed25519), ellenőrzés a bootstrapban és az appban. ⟵ SW-09, az MPC eszközeinek felmérése
  **R-7:** Not done: needs a survey of the tools in the MPC rootfs (is there anything that verifies ed25519?) and a key decision; the placeholder trust store of the plugin work is not reused.
- [ ] **SW-30** (P0, M) Host-teszt `Kick/tests/platform`-ban: `KickVersion` parser + állapotgép (naprakész/frissebb/régebbi/sérült/nincs pendrive); `/proc/mounts` fixture. ⟵ SW-11, SW-12
- [~] **SW-31** (P1, M) `test_tkgl_bootstrap.sh` bővítése: megszakított frissítés (`Kick.prev` ép), hiányzó sha-eszköz, rollback, két pendrive, FAT32/exFAT. ⟵ SW-08, SW-09, SW-14
  **R-7:** test_tkgl_bootstrap.sh scenarios 9-11: interrupted update keeps `Kick.prev`, missing sha tool, missing KICK_SHA256, rollback (USB trigger, /data flag, refused cases, rejected package, new package), --changelog. Not done: two pendrives and FAT32 / exFAT (need the real drives).
- [ ] **SW-32** (P1, S) Smoke-képernyőkép a Kick Update képernyőről (a `Main.cpp:155-167` táblába), AppSmoke: megnyitás/zárás.
- [ ] **SW-33** (P0, M) MPC-s hardveres próba a 6. pont listájával, eredmény a `HW_TEST_REPORT`-ba. ⟵ SW-10

## System Resources — review és TODO

- [ ] **SR-01** (P0, S) Inicializálatlan `sysSizeInfo` megszüntetése (alapértékek), a hívó a `status`-t ellenőrizze; nem-Linux platformokon a `ramInfo/sizeInfo/cpuInfo` mindig inicializál. — `kick_ui_systemResources.cpp:103-131`, `kick_Resorces.cpp:62-134`
- [ ] **SR-02** (P0, M) CPU-mérés különbségből (előző minta tárolása), `iowait`/`guest` helyes kezelése. — `kick_Resorces.cpp:26-46`
- [ ] **SR-03** (P0, M) MPC-s tárak: belső = `/data`, SD/USB a `/proc/mounts`-ból (a `kick_fileUtils.cpp:8-50` logika kiemelése közös segédbe), `statvfs()`, `popen("df")` törlése; nem létező tár rejtve. — `.h:79-81`, `kick_Resorces.cpp:62-100`, `kick_Hwpref.cpp:880-905`. ⟵ SR-20
- [ ] **SR-04** (P1, S) RAM: `MemAvailable` + Kick-folyamat RSS. — `kick_Resorces.cpp:111-134`
- [ ] **SR-05** (P1, M) Mintavétel háttérszálon (1 Hz, atomok), azonnali első mérés, villanásmentes láthatóság. — `kick_ui_systemResources.cpp:91-167`
- [ ] **SR-06** (P0, L) Valódi Purge Unused/All (`ASoundCache` szemantika, nem-RT, megerősítés, visszajelzés); `ASoundCache::purge` ciklus-hiba. ⟵ SR-07 — `kick_ui_systemResources.cpp:78-79`, `audiofile_manager.cpp:51-57`, `audio_sound.cpp:54-60`
- [ ] **SR-07** (P1, M) Engine-statisztika: gyorsítótárazott minták száma/mérete, „használt/nem használt" bontás; megjelenítés a dialógusban.
- [ ] **SR-08** (P1, S) Elérhetőség: Resources-gomb a felső sávon / menü-csempe. — `kick_AppTopbar.cpp`, `kick_AppMenuOptions.cpp`
- [ ] **SR-09** (P1, S) Hardveres bezárás (ENTER/Data Dial/MAIN) — az általános modális megoldás része. ⟵ HDR-02
- [ ] **SR-12** (P2, S) Holt kód rendezése: `Screen::SystemResources`, `KickSysResources`, `onDialogClose` lambda, hibás modul-leírás; döntés: dialógus marad, enum törlendő. — `kick_IScreen.h:63`, `kick_ui_systemResources.h:30, 84-93`, `kick_modeHeader.cpp:95-97`
- [ ] **SR-13** (P2, S) Relatív elrendezés, nagyobb érintési célok, a ferde elválasztóvonal javítása. — `kick_ui_systemResources.cpp:134-156`
- [ ] **SR-20** (P0, M) Host-tesztek: CPU-delta, `MemAvailable`, mount-parser, `statvfs`-wrapper (JUCE-mentes fejléc + `Kick/tests/platform/*Tests.cpp`).
- [ ] **SR-22** (P1, S) Smoke: a `Main.cpp:167` tábla javítása (a dialógus valódi képernyőképe), AppSmoke: megnyitás/zárás. ⟵ SR-12
- [ ] **SR-23** (P0, M) MPC-s hardveres próba a 6. pont listájával. ⟵ SR-01…SR-06

## List Edit — review és TODO


| Tétel | Állapot |
|---|---|
| **LE-07** sor-virtualizálás | KÉSZ: a `KickTableView` opt-in virtuális sorait a List Edit táblára kötöttük (`enableVirtualRows`); csak a látható ablak (± 6 sor) kap komponenst. Mac smoke: 3057 eseménnyel `rows=3057 built=13` (`KICK_SMOKE_SEQUENCERS_BIG=1`, `KICK_SMOKE_LISTEDIT_ROWS`). A Pad cella már S-C-ben könnyű. MPC-s megnyitási idő: ember kell (HW lista R8-4) |
| **LE-21** nem-jegyzet események | RÉSZBEN, azzal ami a modellben van: a sáv **Level és Pan automáció-sávjának töréspontjai** (`AUTOLANE`, eddig csak felvétel/lejátszás) eseményként a listában: nézet-szűrő (View: All / Notes / Track Automation), Time (érintés = BBT ablak), paraméter, érték (Level 0..127, Pan -100..100), Delete, Insert (TYPE = Track Automation, PARAM, VALUE), Q-Link 3 = érték, csoportos Vel +/-, Nudge, undo. Motor: `MTAudioInterface::getAutomationEvents / setAutomationEvent / deleteAutomationEvent` (`MTTrack::setLanePoint` stb., a ValueTree + UndoManager útján, a lejátszás a meglévő figyelőn át követi). Smoke: insert / move / delete mind egy undo-lépés. **Nincs modell** (ezért nincs UI): aftertouch, pitch bend, CC, program change, channel pressure, mute-események, MPCe, tempó-események (Tempo fül), mixer / pad-mixer / insert-effekt automáció, jegyzet-modifier (Mod Type / Value). Ezek a motor hosszabb munkája, nem a List Edit-é |
| **LE-11** maradék | KÉSZ kód-oldalon: a beágyazott popup (TYPE / EVENTS / időosztás) nyitva a hardveres ENTER / STOP / Shift+STOP / MAIN **a popupot zárja** (a TC ablak marad nyitva, a következő nyomás zárja azt), `ListEditTimingCorrect::closeFromHardware`. A „Range” opció S-C-ben megszűnt. A Cancel és a popup-zárás mérése: ember kell (HW lista R8-5) |
| LE-26 Akai-hiánylista | lásd alább („Akai-összevetés, WP-E List Edit”) |
| LE-23 FILTER / Snap | a FILTER most a kézikönyv View menüje (3 nézet, a gomb felirata VIEW); a Snap gomb továbbra is rejtett (nincs funkció) |
| LE-24 / LE-25 / LE-42 / LE-43 | LE-24: híd (R-3); LE-25: owner-döntés; LE-42 / 43: ember kell (HW lista), LE-43 smoke: `KICK_SMOKE_SEQUENCERS_BIG=1 KICK_SMOKE_SCREENS=MenuListEdit` |

### Akai-összevetés, WP-E List Edit (MPC Standalone OS User Guide v3.10, 182–187. o.) — LE-26
| Kézikönyv | Kick |
|---|---|
| Time Counter | a státuszsáv BBT-je |
| View menü (All / Notes / Aftertouch / MIDI Automation / Track Automation / Pitch Bend / Control Change / Program Change / Ch Pressure / Mute / MPCe) | All / Notes / Track Automation KÉSZ; a többi eseménytípusnak nincs modellje |
| Tempo fül (tempóesemények) | NINCS (a tempó-sávnak nincs eseménymodellje) |
| Modifier ikon (Note Modifier Settings: Absolute / Relative) | NINCS (a jegyzetnek nincs modifier-modellje) — NEM-KICK, később |
| Gear: Hitting Pad Selects All Events / Auto-Advance on Step Record / Mod Type Selection / Auto-Scroll (Follow / Page / Off) | Hitting pad, Auto Advance (most van fogyasztója), Auto-Scroll KÉSZ; Mod Type Selection NINCS (nincs Mod Type) |
| Oszlopok: #, Time (azonos idő szürke), típus-ikon, Pad/Note, Length, CH, Velocity, Mod Type, Value, Prob, Rat | KÉSZ, kivéve: CH (a jegyzetnek nincs csatornája, a sávé van), Mod Type / Value (nincs modifier), típus-ikon (a Pad/Param oszlop szövege jelzi a típust) |
| Beszúrás: REC + pad; Insert ablak (BBT, Type: Note / Pad Automation / Mixer Automation / MIDI Automation, Pad / Parameter) | REC + pad KÉSZ; Insert: Notes és Track Automation (Level / Pan) KÉSZ; a többi típus: nincs modell |
| Szerkesztés: érték érintés + Data Dial / +/-; dupla érintés = numerikus billentyűzet; Shift+dial = TC szerinti hossz; Shift+dial a Pad/Note-on = pad-bank; Nudge gomb; Play (jegyzet lejátszása); Delete | Érték + Data Dial (a megérintett cellára a vezérlő sajátja; sorléptetés a dial alapja), Delete, Nudge (csoportos), **Play gomb** (a kijelölt jegyzet egyszer megszólal), **Shift+dial = a kijelölt események TC-osztásnyi eltolása**, **Shift+Q-Link 1 = pad-bank (±16), Shift+Q-Link 2 = hossz TC-osztásban** KÉSZ. **Nincs**: Mute/Solo On/Off kapcsoló (nincs Mute-esemény); a dupla érintés numerikus billentyűzete a KickNumber saját viselkedése (eszközön ellenőrizendő, HW lista R8-4). A hardveres kiosztás (Shift+Q-Link) a mi választásunk: a kézikönyv a cellát érintés után, Shift+dial-lal szerkeszti, a Kick dialja a sorokat lépteti |


Rövidítések (a repó gyökeréhez képest): `LE/` = `Kick/modules/kick_ui/modes/` (`kick_ListEdit.cpp/.h`); `B:` = `Kick/apps/kick/kick_hardware_bridge.h`; `Home:` = `Kick/modules/kick_ui/dialogs/kick_AppHome.cpp`; `Eng/` = `Kick/modules/engine/audio_engine/engine/`; `Ctl/` = `Kick/modules/kick_ui_controls/source/`; `RPT` = `docs/hardware/key61/hw-test-report.md`; `DVK` = `docs/hardware/key61/doc-vs-kick.md`; `KIR` = `docs/planning/plans/research/known-issues-and-roadmap.md`; `CHK` = `docs/hardware/key61/hw-test-checklist.md`.


## Step Sequencer — review és TODO


S-C (`d7076177`, `39846b60`): SS-01..06, 08..16, 18 (xN jelzés, pontos törlés), 21, 23, 33, 40, 43; a GRID SELECT kapcsoló (SS-31 egy része). R-8 (ez a kör):

| Tétel | Állapot |
|---|---|
| **SS-17** nem 4/4 | Host-teszt KÉSZ: `StepModelTests` 8 ütemmutató (2/4, 3/4, 5/4, 6/8, 7/8, 12/8, 7/4, 9/8) x 5 lépésméret (1/4..1/64), vegyes ütemhosszak, triplettek: lépésszám = ütemhossz / lépés (sosem az enum értéke), oldalszám, utolsó lépés, lejátszási pozíció. Motor-smoke: a nem 4/4 ütemmutatók elfogadva és megmaradnak. **Lelet:** ha a lépés nem osztja az ütemet (7/8 negyedekkel: 3 lépés = 288 / 336 tick), az ütem vége (48 tick) lépésként nem érhető el, ott a finomabb osztás kell (`uncoveredTicks`); a lejátszófej ott nem mutat lépést. Ember kell: egy valódi 3/4 és 7/8 projekt a készüléken (HW lista R8-1) |
| **SS-18** maradék | A beállítások ablak (a „Settings” gomb az alsó sávban: a fejléc jobb végét a TOTAL BARS / BPM / nézetváltó eltakarja) **Velocity** mezője: 0 = Auto (CLASSIC 127, GRID 100, mint eddig), 1..127 = minden bekapcsolt lépés alap-velocity-je mindkét nézetben (kézikönyv: „default step velocity”). **Owner-döntés:** az egységes alapérték (127 vagy 100 vagy más) — egy konstans: `step_model::kClassicDefaultVelocity` / `kGridDefaultVelocity`. Sormagasság: ember kell (lásd SS-31) |
| **SS-20** Akai-összevetés | alább (kézikönyv 177–181. o.) |
| **SS-24** Akai-paritás | A kézikönyv Step Sequencer fejezetében **nincs** lépéshossz / gate, másolás-beillesztés, véletlen-generálás: nincs mit megvalósítani. A fejezet tételeiből megvalósítva: Shift+pad = pad kijelölése, Q-Link 0-ra = lépés törlése, alap-Velocity, UNDO a státuszsávban. **Nincs megvalósítva:** Step Automation (a Level / Pan sáv lépésenkénti szerkesztése a lépés-nézetben: a StepComponent / StepPresets jegyzet-alapú és paraméter-enumos, a sáv-alapú lépés-adat külön L méretű egység; a sáv töréspontjai a List Edit-ben már szerkeszthetők), Track Length / SEQ mező (a sáv hossza a Main > Track fülön állítható), Seq/Gbl BPM-kapcsoló (nincs globális-tempó motor-oldal a lépés-nézethez), Clear Page (a Clear Bar az egész ütemre hat; 16T / 32T-nél a lap és az ütem eltér) |
| **SS-30** Q-Link kiosztás | JAVASLAT (UX-döntés, owner): a kézikönyv Screen módja szerint **a Q-Link gomb a lépésnek felel meg** (egy gomb = egy lépés). A Key 61-en 4 gomb van, ezért a **Q-Link gomb oszlopváltása** (SS-22, a híd, R-3) 4 gombos csoportokon át éri el a 16 lépést: a Step Sequencer már oldal-relatívan kezeli a `EN_KNOB_1..16`-ot, tehát ha a híd 16 gombot küld, nincs további UI-munka. Amíg ez nincs: Q-Link 1-4 = az oldal 1-4. lépése. Alternatíva (ha az oszlopváltás nem kell): Q-Link 1 = a kurzor-lépés velocity-je, 2 = probability, 3 = ratchet, 4 = eltolás, a kurzort a Data Dial lépteti — egy lépés mind a három értéke egyszerre elérhető, de eltér a kézikönyvtől. Ajánlás: a kézikönyv szerinti oszlopváltás (hidas munka), mert a Pad / Q-Link megfelelés ugyanaz, mint az MPC-n |
| **SS-31** maradék | A „Select” eszköz Shift nélkül KÉSZ (S-C). Ember kell: az ujj-pontosság mérése a 19 px sormagasságon; ha kevés, 8 sor / oldal (bank-fele váltó) és ≥ 36 px sor (HW lista R8-2). A mérésig a sormagasság nem változik |
| **SS-32** Undo a státuszsávban | KÉSZ: Clear Pad, Clear Bar, a TOTAL BARS rövidítése után a státuszsáv üzenete mellett UNDO gomb (8 mp, vagy a következő üzenetig); a Menü / Fejléc / Projects nincs érintve (`AppStatusbar::offerUndo`) |
| SS-07, SS-22 | a híd (R-3) |

### Akai-összevetés, WP-E Step Sequencer (MPC Standalone OS User Guide v3.10, 177–181. o.) — SS-20
| Kézikönyv | Kick |
|---|---|
| Track mező, Track Length mező + SEQ kapcsoló (független sávhossz) | Track mező KÉSZ; a Track Length / SEQ **nincs** a lépés-nézetben (a Main > Track fülön van) |
| BPM mező; Seq/Gbl kapcsoló (Shift+Tap) | BPM mező KÉSZ; Seq/Gbl **nincs** |
| Időszámláló | a státuszsáv BBT-je |
| Gear: Step Size (a TC értékét is állítja) | a TIME DIV választó (a kvantálást állítja) KÉSZ |
| Gear: Auto-Scroll Off / Page | a lap-követés a CLASSIC nézetben KÉSZ (S-C), kapcsoló nincs |
| Gear: Automation Unlocked / Locked to Step Length | NINCS (Step Automation nincs) |
| TC ikon (Timing Correct ablak) | a TC gomb a List Edit-en át (KIR) |
| Shift + Track -/+ = sáv választása | a TRACK lista |
| Shift + Clear = a szekvencia összes lépése; Shift + Clear Page = az oldal lépései | Clear Pad (a pad összes lépése) és Clear Bar (az ütemé) KÉSZ UNDO-val; Clear Page **nincs** (eltérő részletesség) |
| Lépések: a pad a lépés, a pad a velocity színével világít | a pad-LED-ek a MPC-n nem világítanak (híd, SS-07 / R-3) |
| Nagyobb osztásnál több pad-bank: a < > nyilak a lépésszámok mellett vagy a bank gombok | bank gombok KÉSZ (S-C: felfelé kerekített oldalszám); a < > nyilak: PAGE választó |
| Velocity: érintés a lépés-oszlopon, csúszka (összes lépés), Q-Link a lépéshez; **0 = törli a lépést** | érintés, csúszka, Q-Link KÉSZ; a 0-ra állítás törlése KÉSZ (R-8) |
| Step Automation (Parameter mező: Add New; rajzolás, Q-Link, Shift+Clear, nudge, csúszka, presets; sötét sáv = az utolsó aktív lépés értéke) | NINCS |


Rövidítések a hivatkozásokban (mind a repó gyökeréhez képest): `SSq/` = `Kick/modules/kick_ui/audio_components/step_sequencer/`; `B:` = `Kick/apps/kick/kick_hardware_bridge.h`; `Home:` = `Kick/modules/kick_ui/dialogs/kick_AppHome.cpp`; `Eng/` = `Kick/modules/engine/audio_engine/engine/`; `Ctl/` = `Kick/modules/kick_ui_controls/source/`; `RPT` = `docs/hardware/key61/hw-test-report.md`; `DVK` = `docs/hardware/key61/doc-vs-kick.md`; `KIR` = `docs/planning/plans/research/known-issues-and-roadmap.md`.


## Next Sequence / Song / Color Edit — review és TODO


S-C: NS-01..03, 06..09 (`dec5d617`, `d2d51ce2`), SG-01..08 (`10217009`), CE-01, 03..07 (`3f265a07`). R-8:

| Tétel | Állapot |
|---|---|
| **NS-05** Hold | KÉSZ a kézikönyv szerint: a Hold bekapcsolva a jelenlegi sorozat ismétlődik (a motor sora törlődik), a pad-nyomás csak **kijelöl** (zöld szegély), nem állít sorba; Next Bar / Sudden a kijelölésre vált (Hold alatt a kijelölés, egyébként a sorban álló); Hold kikapcsolása: normál működés, a kijelölés nem kerül sorba (a régi viselkedés a kijelölést kikapcsoláskor sorba tette: a kézikönyv szerint a pad-nyomást „ideiglenesen figyelmen kívül hagyja”). Next Bar / Sudden nem indítja újra a játszó sorozatot, ha nincs kijelölés. Host-teszt: `SequenceLogicTests` (setHold, pressPad, switchTarget, afterSudden) |
| NS Clear | A kézikönyv szerint a Clear a kijelölt sorozatot **törli a lejátszási listából**, csak ha az nem játszik (a régi Kick-viselkedés: a sorban álló eldobása, ez továbbra is elérhető: a játszó pad újra megnyomása). KÉSZ: kérdés (Igen / Nem), UNDO a státuszsávban, a játszó és az utolsó sorozat védett; az indexek igazítása host-tesztelt (`afterDelete`) |
| **NS-04** pad-LED irány | JAVASLAT (owner-döntés): **a híd tartson egy `PadLedState`-et** (16 pad: szín, állapot, villogás), amelyet a meglévő `SET_SINGLE_PAD` / `ALL_SINGLE_COLOUR` / `BLINKING` / `COLOR_MODE` parancsok írnak, a villogást a híd animálja; a `SET_COLOUR_STATE` marad az alapréteg. Ez egyszerre oldja NS-04, SG-10, CE-02, SS-07 és a többi `setPadDataMessage`-t használó képernyőt (AssignSample, Mute, Sampler); a képernyők nem változnak. Az alternatíva (a képernyők átállítása a 16 szín + állapot csomagra) villogás nélküli, és minden képernyőt érint. A híd az R-3 (agy) dolga, itt nem nyúltunk hozzá |
| NSC-01 Akai-összevetés | alább |
| **SG-09** Convert > Seq | NINCS motor-függvény (`songToSequence`): a dal lépések listája, a sorozat-lánc eseményekkel való egyesítése (sávonként a jegyzetek másolása eltolással, ütemmutató / tempó kezelés, „Track Status: Exclude / Include Muted Tracks”) a motor önálló munkája. A gomb nincs megjelenítve. Jelentve |
| SG Export | owner-döntés (a kézikönyv: Audio Mixdown ablak — az audió-exportot az R-2 / a mixdown-tervek kezelik) |

### Akai-összevetés, WP-B / WP-C (kézikönyv 316–317., 326–328., 336–337. o.) — NSC-01
**Next Sequence**
| Kézikönyv | Kick |
|---|---|
| Pad A01 = 1. sorozat; sárga = van sorozat, halvány sárga = üres; a kijelölt pad villogó zöld majd zöld | a képernyőn KÉSZ (szín-kiosztás eltér: narancs / zöld); a hardver pad-LED nem (NS-04) |
| Time Counter | a státuszsáv BBT-je |
| Hardver gomb: Next Seq (Key 61: Shift + Next Seq = Song) | NINCS Key 61 gomb-hozzárendelés a hídban (R-3) |
**Song**
| Kézikönyv | Kick |
|---|---|
| BPM mező + Seq/Gbl | NINCS (a dalnak nincs saját tempója a motorban); rejtett |
| Lépés-lista: sorszám, sorozat, ismétlés, tempó, ütemek; **Hold (legkisebb érték) = végtelen ismétlés** | sorszám, sorozat, ismétlés, tempó, ütemek KÉSZ; a végtelen ismétlés **nincs** (a motor lejátszási sora véges lépésekre számol; motor-munka) |
| A lépés sorozatának cseréje: sorozat-név érintés + Data Dial / +/- | KÉSZ (R-8: a kijelölt lépés sorozatát a Data Dial / +/- állítja; Shift vagy nincs kijelölés: a lépéseken lépked) |
| Convert > Seq (To Sequence, Track Status) | NINCS motor-függvény (SG-09) |
| Export = Audio Mixdown ablak | owner-döntés / mixdown |
| Dalba lépni lejátszás közben nem lehet | a Song képernyő a lejátszás alatt zárolja a táblát (S-C); a belépés lejátszás alatt nincs tiltva (nem vizsgált) |
**Pad Color**
| Kézikönyv | Kick |
|---|---|
| Track mező, Bank mező | program-választó, bank (◀ ▶, A-D / Shift E-H) KÉSZ |
| Alsó mező: pad fénymód: Off / Classic Velocity (pad-nyomásra velocity szín piros-sárga) / Fixed / Off->Velocity / Dim->Velocity / Bright->Velocity | A Kick: Pad Off (Dim / Off) x Pad On (Velocity / Full / Off) + Empty = 9 mód. Megfelelés: Off = Off/Off; Off->Velocity = Off/Velocity; Dim->Velocity = Dim/Velocity; Fixed ≈ Dim|Off / Full. **Nincs**: Classic Velocity (piros-sárga velocity skála) és Bright->Velocity (világos alapfény) — az MPC hardver pad-rétege, **NEM-KICK** (owner: később; a híd kezeli) |
| Empty Pads: off / dim / normal | Empty: Dim / Off KÉSZ; a „normal” (üres pad ugyanúgy világít) NINCS (MPC-specifikus pad-réteg) |
| Keygroup / plugin / MIDI / CV sávok **alapból a Pad Perform pad-szín kiosztását** használják; egyedi kiosztás a pad-terület megérintésével | JAVASLAT (owner-döntés): a Keygroup / MIDI programok alapszíne a Pad Perform színei: gyökér = piros, skála-hang = sötétsárga, a többi halvány (S-F: ugyanez a pad-LED logika); ma `defaultPadColours` = { Red } (csak az első pad piros, a többi 0 = sötétpiros). Ezt a `mt_audio_iprogram.h` tömbje cseréli, a program-típusonként |


**Az Akai-összevetésről.** Az Akai útmutató erre a három módra vonatkozó fejezeteit (WP-B: Next Sequence, Song, Looper; WP-C: Pad Color) a `walkthrough-plan.md` szerint még **nem** dolgozta fel senki (a `walkthrough/` mappában csak wp-b.raw.json, WP-D, WP-G van, és a nyers JSON-ban nincs „Next Sequence” / „Song” / „Color” találat). Ezért ahol az Akai-viselkedésről szólok, az **NEM ELLENŐRIZHETŐ KÓDBÓL / DOKUMENTUMBÓL**; az első TODO ennek pótlása (NSC-01).


## Looper — review és TODO


Kész: **LO-01, LO-04, LO-11** (R-2: `Recorder`, `AudioTests.cpp` 107-141: overdub-kimenet, gain/pan egyszer, egymenetes felvétel), **LO-07** UI-oldala (a Bars mező a motor `setLooperBars`-ába megy a hossz-váltáskor és a képernyő megnyitásakor: az export a hurok egész ütemeire kerekít; `<project>/samples`, egyedi név, ellenőrzött mentés, hibaüzenet: korábbi kör), **LO-09** (a skála a *szekvencia* taktusát követi, a tempó minden használatkor friss; `LooperLogicTests`), **LO-17** (L1 és L4: `AudioTests`; L3 hossz-összehasonlítás és a hurok-hossz/ütem számítás: `LooperLogicTests`; L2 flag-törlés: a motor-oldali `looperBusy = isRecording()` és a UI `stop()`), **LO-19** (WP-B Looper-szakasz átvétele a v3.10 Standalone OS Guide 338-341. oldalával, lent; MPC-s lista a checklistben).

Nyitott / döntés / blokkolt:
- **LO-10** (háttérben szóló looper, owner-döntés). A v3.10 kézikönyv nem mondja ki, hogy a Looper más módban is szól. Javaslat: opt-in beállítás ("Looper plays in the background"), alapértelmezés *ki* (mai viselkedés: elhagyáskor megáll); bekapcsolva a `setLooperEnable( visible )` helyett `setLooperEnable( true )` és a `stop( false )` elmarad. A SYNC már nem állítja meg a szekvenciát.
- **LO-13** (Key 61 gomb, owner-döntés). A hardver felirata a SAMPLER gombon "LOOP": javaslat: *Shift+SAMPLER = Looper* (ma a híd mindkét esetben a Samplert nyitja, `kick_hardware_bridge.h:1442`), LED-del a képernyőhöz. A híd módosítása R-3 terület.
- **LO-16** (gain -> threshold átnevezés, bemeneti trim, clip-jelzés): a csúszka a közös `KickIOComponent`-ben (`kick_IOComponent.h:93-95,139`) van, a Sampler is ezt használja (SA-10, másik worker). Javaslat: a komponens kap címke-API-t (`setGainTitle`), a Looper "THRESHOLD"-ot, a Sampler szintén; a trim és a clip-jelzés a `levelMeter` mellé.
- **LO-15** (SYNC-fázis és AC2 mérése): MPC, ember.

### Akai-paritás: a v3.10 Standalone OS Guide Looper-fejezete (338-341. oldal) és a Kick

| Akai (v3.10) | Kick |
|---|---|
| Mono / Stereo mező | a bemeneti módba olvad (`ae.getLooperRecordingMode`) |
| Inserts (legfeljebb négy), a felvételre hatnak | az Inserts képernyő a Looper gombról megvan; hogy a felvett hangba égnek-e, nem ellenőrzött (MPC-s lista) |
| Monitor In / Off, csak bemenet esetén (Resample-nél nem) | megvan; a "csak bemenetnél" korlát nincs ellenőrizve (TODO a kijelző-oldalon) |
| Peak hold törlése a mérőre koppintva | nincs (kicsi UI-feladat a `levelMeter`-ben, LO-16 mellé) |
| Record To: Play / Overdub | nincs mint választó; a REC / OVERDUB gombok és a küszöb adják a viselkedést (eltérés: nincs "felvétel-élesítés, aztán a transzport indít" mód) |
| Reverse: Sync be -> a hurok végén fordul, Sync ki -> azonnal | **eltérés**: Kick mindig azonnal fordít (`setLooperDirection`) |
| Export to Track / Shift+Export to Pad, "Keep or Discard Sample", Add Event | részleges: Kick programot és padot kér; az Add Event, az Assign track és a Root Note nincs |
| Looper megnyitása: Menu, vagy Shift+Sampler/Looper | Menu megvan; hardver-gomb LO-13 |

Akai-paritási teendők ebből: Reverse a hurok végén Sync alatt (motor), Record To választó, Peak hold törlés, Export-ablak (Add Event / Root Note), Monitor-korlát Resample-nél.


## Inserts — review és TODO


Kész (host-teszt: `AllEffectsTests`, `InsertStoreTests`, `BusInsertsTests`, `DawInsertsTests`, ASan+UBSan; motor-smoke: `KICK_SMOKE_FXLANES=1`, `FxLanesSmoke.h`): **INS-10** (a kapcsoló-paraméterek a *generált* `AudIO/AudIO/airwindows/airwindows_steps.h` táblából jönnek: az `AllEffectsTests` mind a 327 effekt minden paraméterén végigmegy, és kapcsolónak veszi, aminek a kijelzett szövege az egész tartományon legfeljebb 12 különböző érték, 43 ilyen paraméter van; a teszt elbukik, ha a bejegyzett tábla elavult; az `audioeffectx.h` név-találgatása törölve), **INS-12** (kedvencek a `insert_presets.txt`-ben a beállítás-mappában, olvasható kategória-nevek, leírás az `Airwindopedia`-ból: W1-D), **INS-14** (négy insert-lane + a Touch FX lane-ek: az Inserts képernyő forgatói Write módban a kijelölt track lane-jére írnak, ha a lánc annak a programja; Read módban az `MTAudioManager::applyAutomatedFx` játssza vissza a program insert-felületén át, a value tree-t nem érintve; Read elhagyásakor a tárolt értékek visszajönnek; a fejléc Automation-ikon be van kötve; egy lane egy paramétert hajt, a lane-csomópont `autoTarget`-je tárolja, másik paraméter felvétele új lane-t kezd), **INS-15** (felhasználói preset effektenként, tartósan; A/B slotonként és a mutatott láncra érvényesen; lánc-másolás/beillesztés), **INS-19** részben (host-oldali költség-tábla `kick_InsertCost.h`; a `Pockey` 0,68 valós idő a hoszton: "VERY HEAVY EFFECT" jelzés, a 8 további nehéz effekt "HEAVY EFFECT"; az ARMv7-mérés ember), **INS-21** (327 a `05-fx.md`-ben), **INS-22** (paraméter-söprés élő jel alatt, minden véglet véges, költség-mérés effektenként), **INS-23** (az insert-lánc a value tree-n át: nevek, köztük NC-17, bypass, paraméter, üres slot; a busz pre/post-fader beállítása is), **INS-25** (a lista a `HW_TEST_CHECKLIST`-ben). A S-E által jelzett **post-fader** opció: a busz Inserts képernyőjén PRE-FADER / POST-FADER kapcsoló, alapértelmezés pre-fader, a bus fa `postFaderInserts` tulajdonságában tárolt (tartós).

Nyitott: **INS-16** (Pad Mixer sor Inserts-gombja: a Pad Mixer másik worker fájlja, és a motorban nincs pad-szintű insert-lánc; a gomb a pad programjának láncát nyitná, `kick_program_mix.cpp:166` mintájára; owner-döntés, hogy pad-szintű lánc kell-e, vagy elég a program-szintű gomb).

Ismert korlát: ha a felhasználó Read-lejátszás közben forgat egy insert-gombot, a Write-mód híján is az automatizált pillanatnyi érték kerül a value tree-be (a UI a motor értékeit szerializálja); a tárolt alap ilyenkor elveszik.


## XY FX / Touch FX — review és TODO


Kész (host-teszt: `XyFxMapTests`, `MidiControlLogicTests`, `AutomationTests`, `TouchFxTests` ASan+UBSan; motor-smoke `KICK_SMOKE_FXLANES=1`): **XY-13** (a gesztus a kijelölt track három FX-lane-jére íródik: `TouchX`, `TouchY`, `TouchGate`; az eddigi `setAutomationEvent` hívások csendben elbuktak, mert a track csak Level és Pan lane-t ismert; Read módban az `applyAutomatedFx` játssza vissza, a kapu a `TouchGate` lane-ből jön, kapu-lane nélkül nyitva marad; csak a "Track" célra, busz-célra nincs track-lane; a fejléc Automation-ikon be van kötve), **XY-14** (a "MIDI CC" cél a MIDI Control XY fül motorját használja: `midictl::XyCcEngine`, ugyanaz a csatorna / CC / tartomány / némítás és ugyanaz a kimeneti port, csak változáskor küld), **XY-15** (cella-feliratok a `xy_fx_map` tiszta függvényéből, a cella közepének értékével; a régi, nem a tényleges X/Y skálát követő táblák törölve), **XY-16** (Custom Q-Link jelzés), **XY-17** (cella-címkék, `XyCcEngine`, TOUCHFX csomópont mentés/betöltés a projekttel, a Touch FX nem foglal memóriát az audio-szálon, `setGesture`; a strip `kTouchFx` mód a `StripModesTests`-ben), **XY-18** (a lista a checklistben).

Nyitott (nem R-11 tétel, a híd miatt): **XY-11**, **XY-12**.
Hang: a hatás host-teszttel igazolt, MPC-n és fülre nem hallgatva. Blokkonkénti idő (hoszt, ASan nélkül, 256 frame): 4-16 us/blokk módonként (a 48 kHz-es blokk-költségvetés 5333 us), allokáció az audio-szálon: 0 (számláló a tesztben).


## Knob Edit / Q-Links — review és TODO


Kész: **QL-10** (Screen mód: a Knob Edit jobb oldala a *menü alatti* képernyő négy Q-Link szerepét mutatja, minden sor az adott képernyő `knobCommand`-jából olvasva: Grid, XY, Looper, Sampler, Sample Edit (Chop), Pad Mixer, Track Mute, Pad Mute, Pad Perform, List Edit, Next Sequence / Song, Inserts; amit nem olvastam végig, azt a kártya kimondja, nem tippel; `QLinkLogicTests`), **QL-13** (a modell-mezők megvannak a `KickKnobControl`-ban: `defaultQLinkMode`, `qlinkPlayheadIncrement`, `swingControlAppliesTcOnRelease`, `showQLinkStatusWhenTouched`, `automationLaneFollowsQLinks`; a `kick_ui_settings` UI nem készült, az másik worker; **fogyasztó** ma csak a `defaultQLinkMode` van: a többit az illető funkció adja, ha elkészül).

Az agy-kör hibái, amiket javítottam: a Screen-mód szerep-tábla nem létező Q-Linkeket írt le (XY "Cutoff", Grid "Scroll X", alapértelmezett "Track Transpose") és soha nem kapta meg a képernyőt; a strip ciklusába két olyan új mód került (Q-Link, Crossfader), amit a híd nem kezel: a strip holt módba lépett volna.

Nyitott / blokkolt:
- **QL-11** (Q-Link érintés -> OSD): híd (`kick_hardware_bridge.h`, tilos); a híd a Note 84-87 érintést nulla-delta csomagként adja; kell egy új UI-esemény.
- **QL-12** (Touch Strip Q-Link / Crossfader): a tiszta matek kész és tesztelt (`strip_modes.h`: `qlinkFromPosition`, `crossfaderFromPosition`, `qlinkIndexFromPad`; a módok a ciklusból kívül, `kQLink = 10`, `kCrossfader = 11`). Híd-lépések (R-3 terület): (1) `kNumModes`-t növelni és a két módot `kOff` elé tenni, (2) `on_touch_strip` ágban `kQLink`: a pozíciót a kijelölt Q-Linkre (pad választja, `qlinkIndexFromPad`) *abszolút* értékként a Knob Edit hozzárendelésére küldeni, `kCrossfader`: `crossfaderFromPosition` -> a Crossfader célra, (3) `modeLedLevel` és a `[STRIP]` log szövege, (4) `StripModesTests` ciklus-listája.
- **QL-09** (hozzárendelés hatóköre, owner-döntés): ma globális (beállítás-fa). Javaslat: *program-szintű* hatókör (a Custom cím egy program paramétere: egy másik programra váltva a globális hozzárendelés rossz paramétert vezérelne), a Screen/Custom mód marad globális.
- **QL-14** (Akai-paritás): a 4 oszlopos bank (a Q-LINK gomb sort vált) QL-07-tel kész; a makró-vezérlők és a Learn nincs: javaslat: Learn = a Custom rács egy cellájának kijelölése után a következő megérintett UI-paraméter (a `ControlBase::selectControlFromTouch` már tudja) lesz a cella célja; ehhez a vezérlőknek paraméter-azonosítót kell kapniuk (WP-H végigvétele előbb).
- **QL-18**: ember, lásd a checklistet.


## MIDI Control — review és TODO


Kész (host-teszt `MidiControlLogicTests` ASan+UBSan, Mac build, képernyő-smoke): **MC-18** (a tanult sorok induláskor élnek: a képernyő az indításkor épül, a bemeneti portokat és a figyelő időzítőt az üzenetszál nyitja; 3 másodpercenként a port-lista összevetése: később csatlakoztatott eszköz megnyílik, eltűnt bezárul; `diffPorts` tesztelt; a kötések a beállítás-fából jönnek, nem a projektből, ezért projektbetöltéstől függetlenek), **MC-21** (a kimeneti port saját beállítás: `midiControlModeOutputPort`, a régi `HwOutputPort` csak átvétel; a Preferences-felület másik worker), **MC-22** (a tanuló-illesztő valódi motor: `midictl::MidiLearnEngine`, a tábla sorai kötésekké alakulnak, egy üzenet *minden* egyező sort megtalál, soronkénti flip-pel; az agy `ControllerMap` / `MidiLearnEngine` másodpéldánya, amelyet semmi nem használt, törölve: a vezérlő-adatmodell a meglévő, tesztelt `buttons()` / `controlListNames()` táblák), **MC-23** (nincs mit elrejteni: a hardver-gombok csak akkor mennek MIDI-be, amikor a MIDI Control képernyő nyitva van, ott pedig a `KickHome::handleCommand` a navigációt letiltja, `kick_AppHome.cpp:551`; háttérben nincs gomb -> MIDI út; az alternatív Shift-kombinációkhoz a híd kell), **MC-24** (egy sor törlése és a sor-törlés visszavonható, a státuszsor UNDO-gombjával; az utolsó bejövő üzenet a státuszsorban; a sorok 52 px magasak, mint a többi táblában), **MC-25** részben (a lista-frissítés csak a megváltozott sorra fut; a pad-ütés késleltetés mérése: ember), **MC-13** részben marad (a cél a névből oldódik fel; a track Pan és a Submix Volume motor-hívás híján a listából kivéve: ehhez motor-oldali setter kell, nem R-11 fájlkör), **MC-26** részben (a leképezés, az illesztő-motor, a port-diff, az XY-motor tesztelt; a perzisztencia-körút és a szál-stressz nem), **MC-27** (a lista a checklistben).

Nyitott / blokkolt:
- **MC-19** (strip-elem a listában): híd T12.
- **MC-20** (LED Midi Input / Local): a híd a UI LED-csomagokat eldobja (`kick_hardware_bridge.h:137-160`, tilos); a sor rejtve marad. Híd-lépés: a UI-ról jövő LED-csomagot a `set_led` táblára képezni, ha az érintett vezérlő LED-módja `Midi Input`.
- **MC-28** (TSAN a Learn úton): az ASan+UBSan futás a tiszta logikára kész; a *valódi* útvonal (MIDI-szál -> `callAsync` -> üzenetszál) külső forrást kér: ember (lásd a checklistet).


## Pad Perform / Notes / Keyboard Control — review és TODO

### Állapot (S-F, `sonnet/s-f-padperform`)

Kész és host-teszttel (`Kick/tests/platform/PadMapperTests.cpp`, `kick-pad-mapper`) + Mac smoke (`KICK_SMOKE_PROGRAMEDIT=1`, `MenuPadPerform`):
PP-01 (akkord-képletek: terc, 127 fölötti hang eldobva), PP-02 (a motor bankja az egyetlen igazság), PP-03 (Q-Link 1-4 = Scale, Root, Oktáv, Akkord/Mód; `type` lista törölve), PP-04 (held-note tábla, a note-off a lenyomáskor küldött hangokra megy), PP-05 (tárolt Off / Chromatic Chords mód Chromatic-ként játszik), PP-06 (Progression: diatonikus akkordok a skála fokain), PP-07 (az Octave minden bankon érvényes), PP-08 (a pad-note a track kimeneti csatornájára íródik át az engine-ben; az aftertouch ugyanígy, `mt_audio_track.cpp`), PP-09 (a régi leképezés és a halott kód törölve — a Progression/leképezés átírása miatt egyben), PP-20 (16 LEVEL: Velocity és Tune; a Layer / Filter / Attack / Decay a hangmotor hook-ja nélkül nem valósítható meg), PP-23 (képernyős pad-játék per ujj, velocity a pad-on belüli helyből), PP-30 (PadMapper + tesztek), PP-31 (ellenőrzőlista: `hw-test-checklist.md` 20-29; lefuttatása MPC-n: ember kell), PP-41 (Q-Link debounce 40 ms, a DCL pad-szín csomag csak változáskor).
PP-40 részben: akkord-/hangnév a képernyőn kész; a skála-fokszám a pad-feliraton és a hardver pad-LED nyomva tartáskor nincs.
Nyitva (nem kóddal dönthető / más hatókör): PP-21, PP-21a-e, PP-30b (Keyboard Control menü: owner-döntés a gomb-kiosztásról), PP-22 (pad-aftertouch: a híd `kSendPadAftertouchPackets` döntése), PP-24 (touch strip Notes mód: híd), PP-25 (ARP UI: terv).

### Állapot (R-10, `agy/progedit-r10`)

Kész, host-teszttel (`PadMapperTests.cpp`, `KeybedProcessorTests.cpp`) + Mac smoke (`KICK_SMOKE_PROGRAMEDIT=1`):
PP-25 (ARP / Latch UI a Pad Perform képernyőn: ARP és LATCH gomb, Mód / Ráta / Oktáv / Gate; ARP bekapcsolva a 4 Q-Link = Mód, Ráta, Oktáv, Gate; a vezérlők a kijelölt track arpeggiátorából töltődnek, új `getArpGate`; az arp a pad transzponált hangjait játssza, mert a track szintjén fog), PP-40 (a skála-fokszám a pad-feliraton „C-1 (1)”, a megnyomott pad a hardveren fehéren világít; akkord-/hangnév már megvolt), PP-22 / PP-24 (Pad Perform oldal: `padmap::SoundingPads` = a megnyomott hardver-pad tényleges hangjai, `padmap::StripNotes` = a bank 16 skála-hangja, lock-free, a hardver-szálról olvasható, host-teszt), PP-30b (`kick_keybed_processor.h`: csatorna, transzpozíció, aftertouch, velocity-görbe/gain/érzékenység, scale filter, routing, pedál; tiszta logika + teszt).
A két hídoldali bekötés (`kick_hardware_bridge.h`, tiltott fájl): lásd `REPORT.md` „ember kell / owner”.
PP-21, PP-21a-e: a Keyboard Control menü (gomb-kiosztás: owner-döntés) és a `KeybedProcessor` hídba kötése nyitva; a tiszta logika kész. A három skála (AncientChinese, Indian1, Persian) lépés-összege 12 (a hiányzó záró lépéssel), a teszt a valódi `kick_scales.h`-t olvassa.


## Program Edit — review és TODO

### Állapot (S-F, `sonnet/s-f-padperform`)

Kész (host-teszt: `LayerPlayTests.cpp`, `AudioSplitTests.cpp`; Mac smoke: `KICK_SMOKE_PROGRAMEDIT=1`, ProgramEditSmoke.h):
PE-01 (Clip: nem-tulajdonos pointer, a smoke Clip A/B váltást ellenőriz), PE-03 (a knob-csomag nem pad), PE-04 (MIDI CC-csúszkák a program fájára kötve, smoke), PE-05, PE-06 (Layer Play: Cycle körbejár, valódi réteg-szám, zárt velocity tartomány, Keygroup Cycle/Random), PE-07 (Keygroup Main: double pan/semi/fine, Sample Play címkék közös lista + `static_assert`, KEY TRACK elrejtve), PE-08 (layerPan float, Slice bekötve; a Start/End és Loop Start/End ugyanaz a modell-pár, az engine-nek csak egy van), PE-09, PE-10 (Split megvalósítva; Fade/Semi/Fine/TC/Don't Snap/Copy elrejtve, mert nincs mögöttük modell/motor; `pAudio` null-védelem, TRACK-lista `setSelectedTrack`), PE-11 (HW EDIT Plugin programnál megnyitja a CLAP-panelt), PE-12, PE-13, PE-14, PE-15, PE-20 részben (réteg velocity-tartomány a Layer Mix lapon; minta-hozzárendelés a 2-4. rétegre az Assign Sample réteg-választójával együtt: másik worker fájljai), PE-23 (a Keygroup Envelopes/LFO/Layer Mix rejtve marad, nem kapcsoltam vissza), PE-24 részben (Keygroup zóna min/max; Flatten/Convert döntés), PE-25 (Q-Link 1-4 = az oldal első négy forgatója), PE-26 (PAD lista érintéses kiválasztáshoz), PE-27 (háttérszálas hullámforma-betöltés, a hangcache zárolva), PE-30/31/32/33.
Drum Envelopes + LFO: elrejtve (`kDrumEnginePagesVisible = false`), mert a `DrumVoice` atomjai sosem kapcsolódnak a modellhez (B2).
Nyitva: PE-02 és PE-21 (hang-kötés és szűrő: hallgatásos MPC-teszt kell), PE-16 (halott kód törlése: owner jóváhagyás), PE-22 (Keygroup-motor: owner-döntés), PE-28 (`Kick/clap_host/*`: másik hatókör).

### Állapot (R-10, `agy/progedit-r10`)

Kész, host-teszttel / Mac smoke-kal (`KICK_SMOKE_PROGRAMEDIT=1`, `Kick/tests/platform/DrumVoiceSmoke.h`, `DrumVoiceDspTests.cpp`):
PE-02 és PE-21: a Drum hang a pad saját burkoló / LFO / szűrő atomjait olvassa (`MTDrumClip::voiceParams`, a hangszál csak pointert kap a note-on-nál, nincs ValueTree-listener a hangszálon); az `IDs::TYPE`/`type` egységes, a szűrő-env típusa `IDs::filterType` (nincs ütközés az ADSR `type`-pal); enable-kapcsoló az Envelopes (3 sor) és az LFO oldalon, új Filter oldal (típus / cutoff / rezonancia); a `kDrumEnginePagesVisible` flag megszűnt (az oldalak mindig láthatók), mert a bizonyító teszt a valódi `DrumVoice`-szal szintetikus mintán (220 Hz + 6 kHz) mér: amplitúdó-lefutás, pitch-esés, LFO-hullám, aluláteresztő/felüláteresztő, a szűrő-burkoló nyitja a cutoffot. A teszt közben talált és javított motor-hibák: (1) `sampleLength` mértékegysége (mp × 1/127, majd még egyszer 1/127 és a mintaráta: az Attack/Decay ezredmintányi volt); (2) az amplitúdó-burkoló modulátorainak (level/pan/velocity, nincs a modellben és a UI-ban) 0/1/1 alapértékét a kód −1/−0,98/−0,98-ra képezte: az Attack Hold-dá alakult és a pan eltolódott volna (kódolvasásból; a változtatás előtti állapotot nem mértem), most semlegesek; (3) a szűrő-burkoló depth-je csak az LFO-val szorozva hatott, a pitch-LFO a szűrőt vezérelte; (4) az LFO fázisát minden blokk újraindította (`setFrequency`); (5) `AudIO::ADSR`: a rövid szakasz exponenciális együtthatója és a szakaszváltás egy lépést tévesztett (4 mintás attack -80 dB-ről csak -44 dB-ig jutott), ezért az engedélyezett, Attack=0 burkoló majdnem néma volt; (6) a modell és az atomok sustain-alapértéke eltért (0 vs 127), az AD/AHD típus sustain-ja 0.
PE-20: a Samples lapok kiválasztott réteg-gombjának újabb megnyomása mintaválasztót nyit (2-4. réteg is; Drum és Keygroup), a „Clear Assignment” törli a réteget.
PE-24: Keygroup zóna min/max + gyökér-hang (Root Note) a Main lapon.
PE-28: CLAP-panel: nagyobb érintőcélok (sor 56 px, gombsor 52 px), az „Add folder” duplikátum-mentes (lista és `clap-paths.txt`). A natív FileChooser MPC-n és a panel megjelenése: ember kell (H5).
Nyitva: PE-16 (halott kód törlése: owner-jóváhagyás; lista: `REPORT.md`), PE-22 (Keygroup-motor: owner-döntés; a Keygroup Envelopes/LFO/Layer Mix rejtve marad), a Drum hallgatásos igazolás (MPC).

