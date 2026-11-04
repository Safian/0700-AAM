# SW Update (Firmware Update) — review és TODO

Dátum: 2026-11-04. Ág: `claude/kick-plans`. Alap: **statikus kódolvasás** (build, futtatás, MPC nélkül) + a repó dokumentumai. „KÉSZ" = van bekötött handler az engine-ig, nem hardveren igazolt. Ahol nem volt kódból eldönthető: **NEM ELLENŐRIZHETŐ KÓDBÓL (N/E)**. Rövidítés: `M/` = `Kick/modules/`, `A/` = `Kick/apps/`. Kiindulás: `docs/plans/research/screen-inventory.md` §4.26, §5; a kód az igazság, ahol a régi doksi (pl. `WP-G.md` G-050 „OK") ellentmond.

## 1. Áttekintés

**Két képernyő, egy folyamat** (a Behringer-lemezkép firmware-frissítője, az MPC-re átvéve):
- `SWUpdateView` (`M/kick_ui/swupdate/kick_SWUpdateView.{h,cpp}`) — a talált képfájlok listája (PC / USBDisk / SDCard), „Update" gomb soronként, megerősítő popup.
- `SWUpdateScreen` („SWUpdateWindow", `kick_SWUpdateScreen.{h,cpp}`) — „UPDATING FIRMWARE" folyamatjelző két progress-barral.
- Enum: `M/kick_ui_common/source/kick_IScreen.h:58-59`; lusta példányosítás: `M/kick_ui/dialogs/kick_AppHome.cpp:310-311`.

**Elérés:** kizárólag Preferences → Info → „Firmware Update" gomb (`M/kick_ui_settings/source/kick_ui_settings_info.cpp:60`, `sigScreenOpen(SWUpdateView)`). Nincs menü-csempe, felső sáv elem, hardver-gomb-kötés. Kilépés: fejléc Back (`kick_SWUpdateView.cpp:12`), „Go Back" (`:18-21`, ez még `USBFunctionFS::stop()`-ot is hív; a fejléc-Back nem). Hardveres kezelés **nincs**: `SWUpdateView::handleCommand` üres (`kick_SWUpdateView.h:92-94`), `requiresHandleCommand()` nincs felülírva (alap: false), így a Data Dial/ENTER semmit sem csinál. A `SWUpdateScreen` `requiresHandleCommand()==true`, de csak az MCU-frissítő DCL-csomagot (`isSoftwareUpdateMessage`) fogadja (`kick_SWUpdateScreen.cpp:230-259`).

**Az eredeti (Behringer) folyamat**, ahogy a kód leírja:
1. `ImageCheckingThread` (`kick_SWUpdateView.cpp:301-342`) → `FirmwareUpdate::startFirmwareScan` (`M/hardware/source/kick_FirmwareUpdate.cpp:740-754`): USB `/mnt/flash`, SD `/mnt/sdcard`, PC-app `/home/Kick/Firmware_Update` (`kick_Hwcommon.h:26-28`), `.img` fájlok verziófejléccel.
2. Verzióösszehasonlítás `/etc/version.txt`-vel (`kick_FirmwareUpdate.cpp:697-729`, `kick_Hwpref.cpp:273`).
3. Update gomb → `validateImage` → `openssl enc -aes-256-cbc … -k 'PJSDSOMKICK'` + `mount -o loop` (`kick_Hwpref.cpp:219-247, 495-530`) → `SWUpdateScreen` → `/usr/bin/swupdate -i /tmp/fwupdate/Kick.swu` + a `/tmp/swupdateprog` socket (`kick_FirmwareUpdate.cpp:429-561`) → opcionális MCU-hex átvitel DCL-en (`:146-331`) → „reboot -nf" (`kick_SWUpdateScreen.cpp:126`).

**Az MPC Key 61-en az igazi frissítési út más: a pendrive-os bootstrap.**
- A Kick a belső flashen (`/data/Kick`) fut; a `tkgl_bootstrap` (`docs/akai_mpc_hardware/tools/tkgl_bootstrap:116-259`) induláskor összeveti a pendrive `VERSION` fájlját a `/data/Kick.version`-nel, **eltérésnél** (fel- és lefelé is) atomi cserét csinál (`Kick.new` → SHA256 → `Kick.prev` mentés → `mv` → `Kick.version` utoljára), a felhasználói adatot (`/data/projects`, `/data/prefs`) nem bántja (`PENDRIVE_INSTALL.md` §5, `01-pendrive-image.md` DÖNTÉS). Az MPC rootfs csak olvasható, a gyári frissítés aláírt (`01-pendrive-image.md:17, :35`) — az OS-t a Kick **nem** frissítheti, és nem is szabad.
- A bootstrap mindig `KICK_NO_HARDWARE=1`-gyel indítja a Kicket (`tkgl_bootstrap:286`) → `hardwareEnabled()==false` (`M/hardware/source/kick_HardwareMode.h:5-12`) → a teljes Behringer-frissítő (`updateFirmware`, `linuxUpdate`, `MCUUpdate`, `decryptopenSSLImage`, `extractImage`, `USBFunctionFS::init`) **azonnal, némán visszatér** (`kick_FirmwareUpdate.cpp:113, 148, 431`; `kick_Hwpref.cpp` `if (!hardwareEnabled()) return`; `kick_UsbFunctionFS.cpp:77`).
- Az MPC rootfs-en nincs `swupdate`, `/etc/version.txt`, `/mnt/flash` (élő, csak olvasásos ellenőrzés: `04-screen-audit.md` §8.1; az MPC-n az SD `/sdcard`, a pendrive `/media/PenDrive`: `KEY61_HW_BEHAVIOUR.md:123`).

**Összkép:** a SW Update képernyő az MPC-n a pendrive-os frissítéssel **semmilyen kapcsolatban nincs**; a felhasználónak semmi nem mondja meg, milyen Kick-verzió fut, van-e frissebb a pendriveon, és hogyan kell alkalmazni. A két mechanizmus között az egyetlen közös pont a „Firmware Update" gomb félrevezető jelenléte.

## 2. Funkciók és állapotuk

| Funkció | Állapot | Bizonyíték |
|---|---|---|
| Belépés Info → „Firmware Update" | KÉSZ (bekötve), MPC-n **félrevezető** | `kick_ui_settings_info.cpp:60` |
| Forrás-keresés (SD/USB/PC) háttérszálon | RÉSZLEGES (Behringer); **MPC-n hatástalan**: a fix útvonalak nem léteznek (`/mnt/flash`, `/mnt/sdcard`), a PC-szkennelés 6×`sleep(1)` = ≥6 s várakozás | `kick_SWUpdateView.cpp:301-342`; `kick_FirmwareUpdate.cpp:778-795, 797-902`; `kick_Hwcommon.h:26-28` |
| Verzió-összehasonlítás | BUGOS GYANÚ MPC-n: `/etc/version.txt` hiányában `Error` → a kód „alacsonyabb verziót" könyvel el → soha nem kínál frissítést | `kick_FirmwareUpdate.cpp:697-729, 834-838`; `kick_Hwpref.cpp:273` |
| Lista (3 sor) + „Update" soronként | **BUGOS** (üres vektorba indexel, lásd 3.2) | `kick_SWUpdateView.cpp:93-181, 244-252` |
| Megerősítő popup („This will stop all the current running operations.") | KÉSZ (kinézet); szövege MPC-re hamis („will not be able to go back": van `Kick.prev`) | `kick_SWUpdateView.cpp:424-447` |
| „Apply" gomb a nézet alján | **STUB** (látszik, nincs `onClick`) | `kick_SWUpdateView.cpp:9, 51, 194` |
| Image-validálás (openssl-dekódolás + loop-mount) | RÉSZLEGES; **biztonsági probléma** (3.7) | `kick_Hwpref.cpp:219-247, 495-530`; `kick_Hwpref.h:188` |
| `swupdate` + folyamatjelző socket | RÉSZLEGES; **végtelen várakozások** (3.5); MPC-n a `swupdate` nincs | `kick_FirmwareUpdate.cpp:429-561` |
| MCU-frissítés DCL-en (hex-átvitel) | RÉSZLEGES; MPC-n értelmetlen (nincs Kick-MCU), állapotgép statikus változókkal, egyszer használható | `kick_FirmwareUpdate.cpp:146-331` |
| PC-s frissítés (USB FunctionFS `/dev/usb-utp0`) | RÉSZLEGES; MPC-n halott | `kick_UsbFunctionFS.cpp:77, 152-166` |
| Folyamatjelző képernyő (lépés, % ) | RÉSZLEGES; szálak közti adatverseny (3.4) | `kick_SWUpdateScreen.cpp:98-146` |
| Újraindítás sikeres frissítés után | **BUGOS GYANÚ**: `reboot -nf` (nincs `sync`, kényszerített) | `kick_SWUpdateScreen.cpp:126` |
| Hibajelzés | RÉSZLEGES: popupok vannak, de félreírt/helytelen ágak (3.6) | `kick_SWUpdateScreen.cpp:134-145, 178, 211-222` |
| Hardveres kezelés (Data Dial, ENTER, MAIN) | **STUB** | `kick_SWUpdateView.h:92-94` |
| Telepített Kick-verzió megjelenítése (`/data/Kick.version`) | **HIÁNYZIK** (a repóban nincs olvasója: `grep Kick.version Kick/` üres) | — |
| Pendrive-csomag felismerése, „Restart to install" | **HIÁNYZIK** | — |
| Visszalépés (`Kick.prev`) | **HIÁNYZIK** (a bootstrap menti, semmi sem használja) | `tkgl_bootstrap:208-213` |
| Tesztek | **HIÁNYZIK** (a SW Update kódnak nincs host- vagy smoke-tesztje) | `Kick/tests/platform/*` grep; `A/kick/Main.cpp:155-167` |

## 3. Mi nem működik / hibák

**3.1 Az MPC-n a képernyő funkcionálisan halott és hamis üzenetet ad (P0).** Út: Info → Firmware Update → `visibilityChanged` indítja a szkennelő szálat (`kick_SWUpdateView.cpp:215-255`) → ≥6 s „loading" animáció (`scanFirmwareOnPCApp`, `kick_FirmwareUpdate.cpp:778-795`) → az útvonalak nem léteznek, az összehasonlítás `Error` → üres lista → „You are on latest Version / No updates available" (`:23-31, 60-64`). A felhasználó azt hiszi, naprakész, miközben a pendriveon lehet újabb csomag. Ha mégis lenne image: `validateImage` → `decryptopenSSLImage` `false` (`hardwareEnabled()`) → `IMAGE_DYCRYPTION_E` → „Please place correct image on selected Drive" (`kick_SWUpdateView.cpp:134-137`). Az Info oldal „Firmware Version" mezője `/etc/version.txt` hiányában `0.0.0`-t mutat (`kick_Hwpref.cpp:262-271`, a `linuxFwInfo` alapértéke 0: `kick_Hwpref.h:104-110`). Reprodukció: MPC-n Preferences → Info → Firmware Update (a mérés N/E, kódból következik).

**3.2 `deviceData.clear()` az „Update" előtt → túlindexelés (P0).** A szkenneléskor a szál `onThreadEnd`-je két `callAsync`-et tesz sorba: #1 `refresh(); table->refresh(); setupProvider();` (feltölti a sorokat), #2 `deviceData.clear(); thread.reset();` (`kick_SWUpdateView.cpp:232-253`, különösen `:247`). A #2 közvetlenül a #1 után lefut, tehát a sorok kint maradnak a képernyőn, a `deviceData` viszont üres. Az „Update" érintésekor `deviceData[row].deviceName` (`:115`) üres vektorba indexel (UB, valószínű összeomlás); a tábla bármilyen újrarajzolásakor a sorok `getNumRows()==0` miatt el is tűnnek (`M/kick_ui_controls/source/kick_table.cpp:415-418`). Reprodukció: Linux-build `KICK_ENABLE_HARDWARE=ON`, `/mnt/flash/*.img` verziófejléccel → Update érintése; JUCE-mentes fixture-rel (a lista-életciklus kiemelve) egységteszttel is előállítható (SW-02).

**3.3 Lógó `[&]` referenciák és kézi `delete` (P0).** A `showDialog` visszahívásai `[&]`-val a *helyi* `warningPopup` változót fogják (`kick_SWUpdateView.cpp:143-150, 156-163`; `kick_SWUpdateScreen.cpp:123-132, 137-141`), a visszahívás viszont a függvény visszatérése után fut → lógó referencia, `delete warningPopup` UB. A `SWUpdatePopup` (`:172`) értékmásolt `popup`-ot használ (ez rendben van). A `FilePopup(info, true, false, false)` hívások 4. paramétere `int x` (`M/kick_ui_components/source/kick_WarningComponent.h:7`) — `bool`-t kap, így a szöveg `x=0/1`-re kerül a 100 helyett (a Behringer-féle más szignatúra maradványa; `kick_SWUpdateView.cpp:142, 155`, `kick_SWUpdateScreen.cpp:122, 136`).

**3.4 UI-hívások nem UI-szálról, megosztott állapot zár nélkül (P0).** A frissítő/USB szálak közvetlenül hívják a `SWUpdateScreen::getSwupdateStatus/getUsbupdateStatus`-t (`kick_FirmwareUpdate.cpp:249-251, 532-555`), azok pedig `showDialog`-ot, `closeScreen()`-et, `header.showBackButton(true)`-t (`kick_SWUpdateScreen.cpp:122-145, 215-222`) és `juce::String` tagokat (`_overallLabel`) írnak, amit a 10 Hz-es timer olvas (`:98-102`). Ugyanígy a statikus `PCVersion/USBVersion/SDCardVersion` (`kick_SWUpdateView.h:8-10`; **fejlécben `static`** → fordítási egységenként külön példány) a szálból íródik (`kick_SWUpdateView.cpp:314, 322, 330`). JUCE-ban ez assert/összeomlás-forrás.

**3.5 Végtelen várakozások és `exit(1)` (P1).** `linuxUpdate`: az első `while(1)` iterációnként `sleep(100)` (`kick_FirmwareUpdate.cpp:453-465`), ha a socket nem jelenik meg, a szál végleg ott marad; a második ciklus `connect_ipc(true)` hívása `do {…} while(true)` 1 ms-os újrapróbálással (`:53-65`) → ha a `swupdate` leáll, örökké kapcsolódni próbál. `connect_ipc(false)` `exit(1)`-et hív (`:62`) — a teljes alkalmazást lelövi (jelenleg nem hívott, de ott van). Nincs időkorlát, megszakítás, felhasználói visszajelzés. A `SWUpdateScreen`-en nincs kijárat: a fejléc Back rejtett (`kick_SWUpdateScreen.cpp:14`), a „Go Back" gombnak nincs mérete (`:95` kikommentezve). MPC-n a hardveres képernyőgombok ettől még navigálnak (`KickHome::handleCommand` nem modál-védett, `kick_AppHome.cpp:487`), de a futó „frissítést" nem állítják le.

**3.6 Hibás ágak és feliratok (P1).**
- `else if( status == IMAGE_EXTRACTION_E || mt::SwUpdateErrorCode_t::IMAGE_EXTRACTION_E )` — a második tag konstans, így mindig igaz: **minden** nem-`SW_FAILED` állapotra (a sikeresre is) „InValid Image.." kerül a feliratra (`kick_SWUpdateScreen.cpp:178`); a siker ág (`:182-206`) ettől még elindítja a frissítést.
- `U_FAILED` ágon a felirat „Download done.." (`:211`); `U_TIMEOUT` ágon `closeScreen()` az USB szálról, a popup `//add popup` megjegyzés (`:215-222`).
- `onThreadEnd` beállítása a `startThread()` **után** (`kick_SWUpdateView.cpp:230-232`): ha a szál előbb végez (macOS/nem Linux: a PC-szkennelés nem alszik, `kick_FirmwareUpdate.cpp:789-791`), a `std::function` üres marad, a `LoadAnimation` soha nem áll le; egyébként adatverseny a `std::function`-on.
- `isFirmwarePresent` `switch` `break` nélkül (átcsúszás, `kick_FirmwareUpdate.cpp:756-776`); `getFwFileVersion(PC)` a *útvonalat* adja verzió helyett (`:689-691`).
- Az `SWUpdateContent` `rowId`-ja a konstruktorban dől el, újrahasznosított komponensnél (`Existing != nullptr`) elavul (`kick_SWUpdateView.cpp:102-105, 396-399`) → rossz sorra hivatkozó Update.
- A tábla magassága 100 px (`:192`, `setRowHeight(100)`) → három eszközből egy látszik.
- Beégetett szövegek: `v1.21`, „Improves stability" (`kick_SWUpdateView.h:63`, `.cpp:385`).
- `updateFirmware` naplója félrevezető (`status != SUCCESS` ágon „SuccessFul", `kick_FirmwareUpdate.cpp:122-126`).

**3.7 Biztonság és adatvédelem (P0, ha a Behringer-folyamat marad).**
- A „titkosítás" jelszava a forrásban van (`decrptKey = "PJSDSOMKICK"`, `kick_Hwpref.h:188`), a repó AGPL → bárki készíthet érvényes képet; a kulcs a parancssorban (`-k '…'`, `kick_Hwpref.cpp:509`) látszik a folyamatlistában. Ez *tömörítés-szerű elrejtés*, nem aláírás-ellenőrzés.
- A parancsok shell-stringként épülnek (`tail -c +N <útvonal> > …`, `kick_Hwpref.cpp:471`; `mount -o loop` felhasználói médiából, `:228`), a fájlnév nincs idézve → parancs-injekció (a pendrive fájlneve).
- `reboot -nf`: `-n` nem szinkronizál, `-f` kényszerít → félig kiírt projekt/prefs a belső flashen az MPC-n (`kick_SWUpdateScreen.cpp:126`).
- A bootstrap *maga* is csak SHA256-integritást ad (ugyanabból a `VERSION` fájlból, ugyanarról a pendriveról): másolási hibát fog, hitelességet nem. Sőt: ha sem `sha256sum`, sem `shasum` nincs az MPC-n, vagy a `VERSION` nem tartalmaz `KICK_SHA256`-ot, az ellenőrzés **némán kimarad és a telepítés folytatódik** (`tkgl_bootstrap:57-70, 172-195`). N/E, hogy az MPC rootfs-én van-e sha256 eszköz.

**3.8 A bootstrap `Kick.prev` mentése megszakadt frissítés után felülíródik (P1, a pendrive-os út hibája).** Ha az `mv Kick.new Kick` (`tkgl_bootstrap:218`) után, de a `Kick.version` frissítése (`:249-250`) előtt megszakad az áram, a következő indításkor a verzió még eltér → újratelepítés → a `cp -f "$LOCAL_BIN" Kick.prev` (`:209`) most már az **új** bináris másolata lesz, az eredeti jó előző verzió elveszik. (Tesztelt forgatókönyvek: `test_tkgl_bootstrap.sh` Scenario 1-8 — megszakított frissítés nincs közöttük.)

**3.9 Tesztlefedettség a tévhitek miatt.** `WP-G.md` G-050 „OK", G-051 „nem vizsgált" — a kód alapján a képernyő MPC-n nem frissít. (A `screen-inventory.md` §4.26 ezt már „RÉSZLEGES"-ként jelöli.)

## 4. Tervben / hiányzik (az Akai MPC Standalone-hoz és a docs-hoz képest)

- **Akai:** Preferences → Info → Update: három mód — Online Update, USB Drive Update, Switch to Update Mode (`WP-G.md` G-051; a Kick megfelelője nem vizsgált). Az Akai a *saját OS-ét* frissíti, aláírt `az01-update.img`-gel (167 MB, `01-pendrive-image.md:17`); a Kick-nek erre nincs jogosultsága/szándéka. A Kick „frissítése" = a Kick-csomag cseréje (`/data/Kick`).
- **Terv 01 (`docs/plans/01-pendrive-image.md`):** az auto-telepítés megvalósult (`bdd6e744`, `6e222d80`); hátralévő: független ellenőrzés, `readelf -d`, release-csomagolás + CI (`dist/…zip` + SHA256), a hook meglétének ellenőrzése másik MPC-n, user-oldali próba. **Az alkalmazáson belüli oldal nincs a tervben** — ezt a dokumentum hozza be.
- **Hiányzik** (a `VERSION` fájl már hordozza az adatot: `VERSION`, `GIT_COMMIT`, `BUILD_DATE`, `KICK_SHA256`; `make_pendrive_package.sh:234-239`): telepített verzió (`/data/Kick.version`), a pendriveon talált csomag verziója és státusza (azonos / újabb / régebbi = visszalépés / sérült), „Restart to install", visszalépés `Kick.prev`-re, változásnapló, a Preferences→Info oldal valódi Kick-verziója és build-azonosítója (a mostani az `/etc/version.txt`-ből jön, MPC-n `0.0.0`).
- **`SwUpdateMode_t::OTA`** enum létezik (`kick_Hwcommon.h`, `getSwUpdateModeFromString` `kick_FirmwareUpdate.cpp:606-618`), megvalósítás nincs; online frissítés a `plans/research/architecture-and-publishing.md` és `cloud-sources.md` hálózati döntéseitől függ (MPC-n WiFi: iwd, a kód `wpa_supplicant`-ra épül: `04-screen-audit.md` §8.1).

## 5. Javaslataim

1. **A képernyő cseréje „Kick Update"-re (MPC-natív).** Három állapot egy nézetben: *Telepítve* (verzió, commit, build-dátum a `/data/Kick.version`-ből), *Pendrive* (a `<mount>/tkgl_bootstrap_[ProjectData]/VERSION` olvasása), *Művelet*. Állapotok: „Naprakész" / „Frissítés elérhető A→B" / „Eltérő (régebbi) csomag — visszalépés" / „Nincs pendrive" / „Sérült csomag (SHA256)". A művelet a **bootstrap-ot használja** (nem másol saját kóddal): mentés (dirty-projekt), majd újraindítás a pendrive-val a helyén. Indoklás: egyetlen telepítő-logika (tesztelt shell), nincs második frissítő; a felhasználó látja, mi fog történni.
2. **Pendrive-keresés a `/proc/mounts`-ból**, ahogy a fájlböngésző is teszi (`M/kick_ui/modes/filebrowser/kick_fileUtils.cpp:8-45`: `mountPointOf`, az MPC `/sdcard` + `/media/<label>`), nem fix útvonalakból. Háttérszálon, megszakíthatóan, rövid időkorláttal (nem 6 s `sleep`).
3. **Verzió-modell:** header-only `KickVersion` (KEY=VALUE `VERSION` fájl parser, összehasonlítás, az aktuális build beégetett verziója/commitja a CMake-ből). Az Info oldal „Firmware Version"/„Build Date" ebből táplálkozik (`kick_ui_settings_info.cpp:85-99`), az `/etc/version.txt` helyett MPC-n. Tesztelhető JUCE nélkül (a `Kick/tests/platform` mintája).
4. **Visszalépés:** a bootstrap kapjon `ROLLBACK`-triggert (pendrive-fájl vagy `/data/Kick.rollback` jelzőfájl), ami a `Kick.prev`/`Kick.version.prev`-et állítja vissza (mentve van, de senki nem használja: `tkgl_bootstrap:208-213`). Előtte javítani a 3.8-at (a `Kick.prev` csak akkor íródjon, ha a `Kick.version` az éppen futó binárishoz tartozik).
5. **Integritás + hitelesség:** a bootstrap utasítsa vissza a telepítést, ha nincs sha256 eszköz vagy hiányzik a `KICK_SHA256` (ne némán OK); döntés: aláírt `VERSION` (pl. ed25519, nyilvános kulcs a binárisban) — előfeltétel az MPC-n elérhető eszközök felmérése (N/E).
6. **A Behringer-folyamat sorsa — owner-döntés.** Ha marad (Behringer-lemezkép): `PlatformServices` mögé (lásd `04-screen-audit.md` §8.4), futásidejű jelzéssel, a 3.2-3.7 hibák javításával; ha nem marad: törölni (`swupdate`, MCU-hex, openssl, FunctionFS, `SWUpdateView/Screen`) — kevesebb kód, kevesebb támadási felület, a hardcode-olt kulcs eltűnik.
7. **Hardver/érintés:** Data Dial = lista, nyomás/ENTER = művelet, MAIN/Shift+STOP = mégse (a TC-ablak mintája: `A/kick/hardware/modal_dialog_keys.h`); nagy érintési célok (a mostani Update gomb a 100 px magas sorból 60 px, `kick_SWUpdateView.cpp:412`); megerősítés megadja a *tényleges* következményt („A Kick újraindul; a projektjeid és beállításaid megmaradnak; az előző verzió visszaállítható").
8. **Teljesítmény/biztonság:** frissítés tiltása lejátszás/felvétel/projektbetöltés közben; szinkron `sync` + `exit` (nem `reboot -nf`); az újraindítás módja hardveren ellenőrzendő (`systemctl restart acvs` a `deploy_mpc.sh` szerint; hogy a Kick folyamatból hívható-e, N/E).

## 6. Hardveres és unit-tesztek

**Ma:** a SW Update kódnak **nincs** host-tesztje és smoke-képernyőképe (a `A/kick/Main.cpp:155-167` tábla nem tartalmazza a `SWUpdateView/Window`-t). A pendrive-os bootstrapet a `docs/akai_mpc_hardware/tools/test_tkgl_bootstrap.sh` fedi (Scenario 1-8: friss telepítés, azonos VERSION, eltérő VERSION, sérült másolat, hiányzó bináris, felhasználói adat megőrzése, UNINSTALL, standalone uninstaller). Nincs: megszakított frissítés (3.8), hiányzó sha256 eszköz (3.7), rollback, két pendrive.

**Hiányzik (host):** `KickVersion` parser + állapotgép (naprakész/frissebb/régebbi/sérült/nincs pendrive) fixture-fájlokkal; pendrive-keresés `/proc/mounts` fixture-rel; a `deviceData`-életciklus (ha a Behringer-folyamat marad).

**Az MPC-n ellenőrzendő** (CHK-bővítés):
- [ ] Preferences→Info: a Kick-verzió/commit/build-dátum egyezik a `/data/Kick.version`-nel (nem `0.0.0`).
- [ ] Pendrive nélkül: „Nincs pendrive"; behúzva: a csomag verziója látszik ≤2 s alatt.
- [ ] Azonos / újabb / régebbi csomag: helyes állapotszöveg; „Restart to install" után a bootstrap-napló (`/data/kick-boot.log`) A→B-t mutat, a projektek és prefs érintetlenek.
- [ ] Visszalépés `Kick.prev`-re működik; megszakított frissítés (áramkihúzás a másolás közben) után a régi Kick indul, `Kick.prev` ép.
- [ ] Sérült/csonkolt `Kick` a pendriveon: a telepítés elmarad, a régi indul, a képernyő okot mutat.
- [ ] Lejátszás/felvétel közben a frissítés nem indítható.
- [ ] Modális ablak hardveres zárása a Kick Update-en (ENTER/MAIN), érintés-célok.

## 7. TODO

Függés-jelölés: `⟵` = előfeltétel. „Ha marad" = csak, ha SW-00 a Behringer-folyamat megtartását dönti.

- [ ] **SW-00** (P0, S) **Owner-döntés:** marad-e a Behringer-lemezkép frissítője (swupdate/MCU/openssl/FunctionFS)? Javaslat: nem marad (MPC-n nem fut, biztonsági teher, `04-screen-audit.md` §8.1). Ha nem: SW-10 törlést ír, SW-02…SW-07 elesik. — 3.1, 3.7
- [ ] **SW-01** (P0, S) A „Firmware Update" gomb ne vezessen a halott/hamis folyamatra az MPC-n: a gomb az SW-10 képernyőt nyissa; a régi `SWUpdateView/Window` MPC-n (és `!hardwareEnabled()` esetén) ne legyen elérhető. ⟵ SW-10 — `kick_ui_settings_info.cpp:60`, `kick_AppHome.cpp:310-311`
- [ ] **SW-02** (P0, M) *(ha marad)* `deviceData` életciklus: a sor *értékmásolatot* kapjon, a `clear()` ne előzze meg az Update-et; `rowId` frissítése újrahasznosításnál; szálbiztos átadás. — `kick_SWUpdateView.cpp:102-115, 244-252`
- [ ] **SW-03** (P0, M) *(ha marad)* Lógó `[&]`-k és kézi `delete` felváltása (`std::unique_ptr`/értékmásolt capture); `FilePopup` argumentumok javítása. — `kick_SWUpdateView.cpp:143-172`, `kick_SWUpdateScreen.cpp:122-141`
- [ ] **SW-04** (P0, M) *(ha marad)* A frissítő/USB szálak csak `MessageManager::callAsync`-on át érintsék a UI-t; az állapot (`_overallLabel`, verziók) `std::atomic`/mutex mögött; `static` verziók a fejlécből a `.cpp`-be, szálbiztosan. — `kick_SWUpdateScreen.cpp:98-228`, `kick_SWUpdateView.h:8-10`
- [ ] **SW-05** (P1, M) *(ha marad)* Időkorlát + megszakítás a `linuxUpdate` ciklusaira, `exit(1)` eltávolítása, kijárat a `SWUpdateScreen`-ről hibánál/időtúllépésnél. — `kick_FirmwareUpdate.cpp:36-70, 453-558`, `kick_SWUpdateScreen.cpp:14, 95`
- [ ] **SW-06** (P1, S) *(ha marad)* Apróbb hibák: `||`-konstans (`:178`), `U_FAILED` felirat (`:211`), `U_TIMEOUT` popup (`:222`), `onThreadEnd` a `startThread` előtt, `isFirmwarePresent` `break`-ek, tábla-magasság, „Apply" gomb törlése, beégetett „v1.21/Improves stability".
- [ ] **SW-07** (P0, M) *(ha marad)* Biztonság: kulcs ne legyen a forrásban/argv-ban, parancsok `execve`-szerű, idézett paraméterekkel (ne shell-string), `mount` kezelése, `reboot -nf` → `sync` + normál újraindítás. — `kick_Hwpref.h:188`, `kick_Hwpref.cpp:228, 471, 509`, `kick_SWUpdateScreen.cpp:126`
- [ ] **SW-08** (P1, S) Bootstrap: `Kick.prev` ne íródjon felül megszakított frissítés újrapróbálásakor; a `Kick.version` kiírása és a `Kick.prev`/`Kick.version.prev` párosítása. — `tkgl_bootstrap:208-213, 249-250`
- [ ] **SW-09** (P1, S) Bootstrap: hiányzó sha256-eszköz / hiányzó `KICK_SHA256` esetén a telepítés *utasítsa el* (ne némán OK); ⟵ az MPC rootfs-eszközeinek felmérése. — `tkgl_bootstrap:57-70, 172-195`
- [ ] **SW-10** (P0, L) „Kick Update" képernyő (MPC-natív): telepített/pendrive-os verzió, állapotok, „Restart to install", `Kick.prev` visszalépés-gomb, hibaokok; Data Dial/ENTER/MAIN kezelés; törli/lecseréli a Behringer-nézeteket (SW-00 szerint). ⟵ SW-11, SW-12, SW-13
- [ ] **SW-11** (P0, M) `KickVersion` (header-only): `VERSION`/`Kick.version` parser, összehasonlítás, beégetett build-azonosító; az Info oldal „Firmware Version/Build Date" ebből (G-049/G-053). — `kick_ui_settings_info.cpp:85-99`, `make_pendrive_package.sh:234-239`
- [ ] **SW-12** (P1, M) Pendrive-csomag keresése `/proc/mounts` alapján (a `mountPointOf` logika újrahasznosítása/kiemelése), háttérszálon, időkorláttal; több pendrive. — `kick_fileUtils.cpp:8-45`
- [ ] **SW-13** (P1, M) Újraindítás-folyamat: dirty-projekt mentése, lejátszás/felvétel/betöltés tiltása, kilépés/újraindítás módja; ⟵ hardveres ellenőrzés: `systemctl restart acvs` hívható-e a Kick folyamatból (N/E).
- [ ] **SW-14** (P1, M) Visszalépés: bootstrap `ROLLBACK`-trigger + `test_tkgl_bootstrap.sh` scenario + UI-gomb. ⟵ SW-08
- [ ] **SW-15** (P2, M) Változásnapló/megjegyzések a csomagban (`CHANGELOG`) és megjelenítése a frissítés előtt.
- [ ] **SW-16** (P2, L) Online frissítés (OTA): letöltés `.part`+hash, a bootstrappal azonos telepítő; ⟵ hálózat (iwd), `architecture-and-publishing.md` döntései, SW-00/SW-10; owner-döntés.
- [ ] **SW-17** (P2, S) Hitelesség: aláírt `VERSION` (ed25519), ellenőrzés a bootstrapban és az appban. ⟵ SW-09, az MPC eszközeinek felmérése
- [ ] **SW-30** (P0, M) Host-teszt `Kick/tests/platform`-ban: `KickVersion` parser + állapotgép (naprakész/frissebb/régebbi/sérült/nincs pendrive); `/proc/mounts` fixture. ⟵ SW-11, SW-12
- [ ] **SW-31** (P1, M) `test_tkgl_bootstrap.sh` bővítése: megszakított frissítés (`Kick.prev` ép), hiányzó sha-eszköz, rollback, két pendrive, FAT32/exFAT. ⟵ SW-08, SW-09, SW-14
- [ ] **SW-32** (P1, S) Smoke-képernyőkép a Kick Update képernyőről (a `Main.cpp:155-167` táblába), AppSmoke: megnyitás/zárás.
- [ ] **SW-33** (P0, M) MPC-s hardveres próba a 6. pont listájával, eredmény a `HW_TEST_REPORT`-ba. ⟵ SW-10
