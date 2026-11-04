# File Browser — review és TODO

Állapot: **statikus kódolvasás** (build/futtatás/hardver nélkül; az MPC-hez nem csatlakoztam), 2026-11-04, ág `claude/kick-plans`. A „KÉSZ” itt azt jelenti: van bekötött handler az engine-ig, nem azt, hogy hardveren igazolt. Ahol a régi docs (pl. `WP-G.md` sorszámai/sorszámok) és a kód ellentmond, a **kód az igazság**; a `WP-G.md` fájl:sor hivatkozásai a jelenlegi HEAD-hez képest elavultak. Rövidítések: `M/` = `Kick/modules/`, `A/` = `Kick/apps/kick/`, `FB/` = `M/kick_ui/modes/filebrowser/`, `bridge` = `A/kick_hardware_bridge.h`. Prioritás: **P0** összeomlás / adatvesztés / biztonság, **P1** hibás vagy hiányzó alapfunkció, **P2** javítás / kényelem. Méret: S ≤ fél nap, M ≤ 2 nap, L > 2 nap.

## 1. Áttekintés

**Mit csinál.** Háromoszlopos fájlböngésző (`FB/kick_fileBrowser.{h,cpp}`, 2050 + 377 sor): bal oldalon a *Places* lista (Internal = `/data/projects`, Factory Sample Library = `/data/sounds`, Demos = `/data/demos`, futás közben hozzáadott SD / USB), jobb oldalon a fájllista (név, dátum, idő, méret; a mappák mérete aszinkron számolódik), felül útvonal-/keresősor + Select / Delete (Select módban Cut / Copy / Paste / Rename), alul szűrők (Project / Sample / All), Sample Assign, Load, Load All, Play, előhallgatás-mód (OFF / AUTO / MAN) és hangerő. A fejlécben 5 „kedvenc mappa” gomb és a New Project gomb. Projektet (`.kickPrj`) dupla koppintással tölt be, mintát a pool-ba / padra tölt (Load), a bal alsó „Sample Assign” az `AppAssignSample` képernyőt nyitja.

**Elérés.**
- Menü → „Browser” (csempe 0: `M/kick_ui/dialogs/kick_AppMenuOptions.cpp:85`).
- Projects képernyő fejléce, File Browser gomb (`M/kick_ui/recent_project/kick_RecentProject.cpp:76-78`); felső sáv Browse gomb (`M/kick_ui/dialogs/kick_AppTopbar.cpp:96`).
- MPC Key 61: **SOUNDS** gomb → File Browser (`bridge:1171-1181`; a Sounds Mode nem létezik, `KEY61_DOC_VS_KICK.md` 26. sor); **Shift+MENU** → File Browser (`bridge:1162-1168`); **Shift+SOUNDS** → ugrás a következő hozzárendelt kedvenc mappára (`bridge:1174-1178`, `FB/kick_fileBrowser.cpp:76-84, 957-973`). DCL `EN_BROWSE` → `kick_AppHome.cpp:502` (a Key 61-en nincs ilyen fizikai gomb). Mód-LED: SOUNDS (`bridge:284`).
- Fontos: a bridge-ből közvetlenül nyíló SOUNDS / Shift+MENU **megkerüli** a `prjLoaded` kaput (`kick_AppHome.cpp:487`), tehát projekt nélkül is megnyílik (a Load / Load All / Sample Assign ilyenkor tiltott: `FB/kick_fileBrowser.cpp:1233-1238`).

**Hardver-kötések a képernyőn.**
| Vezérlő | Mit tesz itt | Kód | Hardveres állapot |
|---|---|---|---|
| Data Dial forgatás | a jobb lista sorát lépteti (`right.table->knobRotated`); AUTO előhallgatásnál minden lépés új előhallgatást indít | `FB/kick_fileBrowser.cpp:1271-1274, 650-675`; fókusz: `:513` | Data Dial általában igazolt (0a42bfdb), a FB-n nem mért |
| Data Dial nyomás / ENTER | `onRowDoubleClicked(kijelölt sor)`: mappába belép, projektet betölt, mintát betölt | `:1276-1279, 345-348, 625-646` | nem mért |
| −/+ | a fókuszált tábla léptetése (`KickControlBase::handleCommand`) | `M/kick_ui_controls/source/kick_ControlBase.h:57-62` | nem mért |
| PLAY / PLAY START / STOP | `setSequencePlay` / `setSequencePause` a kiválasztott sorozaton | `:1203-1213` | nem mért ezen a képernyőn |
| Pad (16) | élő MIDI a **kiválasztott trackre** (`bankOffset + pad`) + a pad lesz a Load **célpadja** (zöld LED) | `:1178-1202`; `FB/kick_fileUtils.cpp:134-165` | pad-ütés általában igazolt (RPT L119); a FB-on belüli viselkedés nem mért; **a felengedés nem jut el** (3. B5) |
| Bank A–D / Shift E–H | `ae.setBank` (KickHome) — a Load célpad-bankját is ez adja | `kick_AppHome.cpp:523-530` | A–D LED igazolt |
| Shift+SOUNDS | következő kedvenc mappa | `bridge:1174-1178` | nem mért |
| Q-Link, Touch Strip | nincs kötés | — | — |
| Vissza / feljebb | **nincs hardveres gomb**, csak a képernyőn lévő Back (érintés) | `:144-169` | — |

**Érintés.** Egy koppintás kijelöl, **dupla koppintás** belép / betölt (450 ms / 24 px küszöb, `KEY61` bridge; MPC-n a Sample Assign dupla koppintása igazolt: 42cfdc3c). Select módban minden koppintás kapcsolja a sor kijelölését. A modális ablakok (Eject, Delete, Rename, Format) csak érintéssel zárhatók (`bridge:893-905`: csak a Timing Correct ablaknak van hardveres zárása).

**Mount-pontok.** Mért (`KEY61_HW_BEHAVIOUR.md` 6. szakasz, 2026-11-03): SD = `/sdcard`, USB-pendrive = `/media/PenDrive`. A kód a `/proc/mounts`-ból keres (`FB/kick_fileUtils.cpp:15-31`): SD = az első `/dev/mmcblk1*` eszköz, USB = az első `/dev/sd*` eszköz (az MPC `sdb`-től számoz: `:58` megjegyzése); a `…/EFI` mount-pontokat kihagyja. Tartalék: `/mnt//sdcard`, `/mnt//flash` (dupla perjel, Behringer-lemezkép-örökség, `:35, 39`). A felismerés csak Linuxon fut (`#if LINUX`, `FB/kick_fileBrowser.cpp:1480-1559`): macOS-en a külső meghajtók nem jelennek meg.

**Mappaméret (friss, aszinkron).** A mappasor mérete kezdetben „…”, majd egy **egyszálú** `juce::ThreadPool` számolja (`FolderSizeJob`, `:680-721, 738-789`). Az eredmény `FolderSizeCache`-be kerül (`FB/kick_folderSizeHelper.h:21-58`: mutex-szel védett `útvonal → (méret, mtime)`, a mappa **saját mtime-ja** az érvényesség kulcsa); a navigáció új „generációt” nyit (`GenerationTracker`, `:67-101`), a futó munka a következő iterációnál kilép (`calculateFolderSize` `-1`-et ad: `FB/kick_fileUtils.cpp:69-91`), az eredmény `callAsync`-kel kerül a sorba (`SafePointer`-rel védve). A számítás egyetlen rekurzív `RangedDirectoryIterator` menet (a korábbi exponenciális változat javítva: build `688245fb`, `580620c3`).

## 2. Funkciók és állapotuk

| Funkció | Állapot | Bizonyíték |
|---|---|---|
| Places: Internal / Factory Library / Demos | KÉSZ | `FB/kick_fileBrowser.cpp:19-25` |
| SD / USB felismerés (1 Hz, Linux) | RÉSZLEGES | `:1463-1561`; `kick_fileUtils.cpp:15-60`; csak az első eszköz, csak Linux; törékeny sorindex-logika (B16) |
| **Eject** | STUB / BUGOS | csak a listasort rejti + jelzőt állít, nincs `sync`/`umount` (`:408-443`; `kick_fileUtils.cpp:90-101`, `kick_fileUtils.h:20-21`) |
| **Format** | STUB | a gomb nincs hozzáadva (`:1870` megjegyzés), a popup callbackje csak törli magát (`:1830-1836`) |
| Content / Expansion fülek | STUB | a gombok megjelennek, nincs `onClick` (grep: `left.content`, `left.expansion` nincs a `.cpp`-ben); WP-G G-234/235 |
| Fájllista (név, dátum, idő, méret) | RÉSZLEGES | `:522-621`; **rendezetlen**, rejtett fájlokkal (B7); oszlopfejléc nem kattintható (G-240) |
| Belépés mappába (dupla koppintás / Dial) | KÉSZ | `:625-646, 1120-1143` |
| Vissza (feljebb) gomb | BUGOS | a gyökér-őr csak az Internalra jó (B2): `:16, 1133, 1154` |
| Szűrők Project / Sample / All | RÉSZLEGES | `:99-142, 1058-1106`; a Project/Sample szűrő **rekurzív és szinkron** (B8) |
| Keresés | RÉSZLEGES | `:1078-1093`; rekurzív, szinkron, csak név (kiterjesztés nélkül) |
| Projekt betöltése (dupla koppintás) | RÉSZLEGES | `:636-642` — nincs `isProjectLoading` őr, versenyhelyzet az engine `dirList`-tel (B13) |
| Load (minta → pool + pad) | BUGOS | `:210-212`; `kick_fileUtils.cpp:239-278`; visszatérési kód téves értelmezése (B6), projektfájlra „cannot be loaded” (B10) |
| Load All | RÉSZLEGES | `:214-216`; az **engine teljes `dirList`-jét** tölti (nem a szűrt listát), csak az első mintát rendeli padhoz (`kick_fileUtils.cpp:191-236`) |
| Sample Assign gomb | KÉSZ | `:1769-1771` → `MenuAssignSample` |
| Előhallgatás OFF / AUTO / MAN, Play, hangerő | RÉSZLEGES | `:196-243, 1160-1169`; a mód-gomb visszaállítása hibás (B9) |
| Select mód + Cut / Copy / Paste | RÉSZLEGES | `:246-332, 365-384`; felülírás-védelem nélkül, UI-szálon (B3) |
| Rename | RÉSZLEGES | `:975-1056`; validáció nélkül (B15) |
| Delete (megerősítéssel) | **BUGOS** (UB) | `:1352-1419`; a megerősítő lambdák a függvény lokális `popup` változóját hivatkozás szerint fogják (B1) |
| New Project (fejléc) | RÉSZLEGES | `:171-193`; név-validáció / duplikátum-ellenőrzés nélkül (B14) |
| Kedvencek 1–5 (+ Shift+SOUNDS) | RÉSZLEGES | `:60-84, 889-973`; sorindex-alapú (B11) |
| Mappaméret (async, cache, generáció) | KÉSZ (fenntartásokkal) | `:680-789`; `kick_folderSizeHelper.h`; 3. B19 |
| HW pad → élő MIDI + cél-pad | BUGOS | `:1178-1202` (B5) |
| HW Play/Stop | KÉSZ | `:1203-1213` |
| Hangerő-popup bezárása kattintásra | RÉSZLEGES | `:1311-1321` (képernyő- vs. lokális koordináta, B17) |
| Betöltő animáció (bal sor váltás) | STUB-közeli | `:476-483` vs. `:1122` — az animációt a következő sor `cancelFolderSizeCalculations()` azonnal leállítja |
| Cloud / hálózati források (Splice, Drive, WebDAV) | HIÁNYZIK (terv) | `research/cloud-sources.md`, `04-screen-audit.md` §7 |
| Halott kód | — | `FolderSizeCalculator`, `startFolderSizeCalculation`, `FileBrowser::addMultipleAssets`, `FileBrowser::createProject`, `fileList`, `static int c` (`:5-6, 395, 791-887, 1421-1443`, `kick_fileBrowser.h:293-310`) |

## 3. Mi nem működik / hibák

**B1 — Delete megerősítés: lebegő hivatkozás (UB), P0.** `deleteSystem()` a `popup` *lokális* változót `[&]` capture-rel fogja a `button1/2.onClick` lambdákba (`FB/kick_fileBrowser.cpp:1390-1409`), amik csak a függvény *visszatérése után*, a felhasználó koppintására futnak. A `popup` pointer-változó ekkor már érvénytelen stack-hely → a `popup->close()` és az azt követő `selectedFile` bejárás + `deleteRecursively()` tetszőleges viselkedés (összeomlás vagy — mivel a stack-hely gyakran érintetlen marad — „véletlenül működik”). Hatás: Delete közben összeomlás vagy rossz objektumon `close()`; a törlés lefutása nem garantált. Reprodukció: bármely fájl kijelölése → Delete → Yes (ASAN / optimalizált build a legbiztosabb). Javítás: érték szerinti capture + a modális callback `R == APPLY` ágán törölni (mint a Rename: `:1045-1053`).

**B2 — Gyökér-szökés és védelem nélküli rendszerfájlok, P0.** A „gyökér” az Internal szülője (`root = ae.getInternalDir().getParentDirectory()` = `/data`, `:16`); a Back gomb csak akkor tiltott, ha `parent == root` (`:1133, 1154`), `exitDirectory()` maga nem ellenőriz (`:1145-1158`). SD-n (`/sdcard`, szülő `/`) vagy USB-n (`/media/PenDrive`, szülő `/media`) a Back **kilép a külső meghajtóból**, `/`-ig lehet fölmenni (`/data`, `/etc`, …). A Delete / Cut / Rename / Paste csak a Factory és Demos soron tiltott (`:453-466`, `rowId == 1 || 2`), a többin — és a fölmászás után bárhol — engedélyezett. A `checkIsCurrProj()` csak a *fájlnév alapnevét* hasonlítja a jelenlegi projekt nevéhez (`:1323-1338`): egy ugyanolyan nevű másik fájl hamisan tiltott, a jelenlegi projektet *tartalmazó mappa* (vagy `/data/projects` maga) törölhető. Hatás: az MPC rendszer-/felhasználói adatainak törlése egy érintéssel (az app root-ként fut az MPC-n — N/E, de a `/data` írható). Javítás: Place-enkénti gyökér (FB-02) + védett útvonal-lista.

**B3 — Fájlműveletek felülírás-védelem és szálkezelés nélkül, P0.** Paste: `moveFileTo` / `copyDirectoryTo` / `copyFileTo` a cél létezésekor **felülír** (JUCE: a cél törlődik), megerősítés nélkül (`:304-319`); mappa önmagába másolása nem tiltott; a másolás/áthelyezés (SD → belső, több száz MB) az **UI szálon** fut, folyamatjelző nélkül → az MPC kijelzője és a Data Dial lefagy, de a hangmotor is függhet a lemez I/O-tól. Az USB/SD-re írás után nincs `sync`. Reprodukció: két azonos nevű fájl Cut/Paste a másik mappába.

**B4 — Eject nem valódi eltávolítás, P1.** Az `ejectCallback` csak a `deviceData`/`row` sort törli és `setSdCardEjectStatus(true)`-t állít (`:408-443`); a háttérben az eszköz tovább mountolva marad, a lista addig nem mutatja, amíg fizikailag el nem húzzák (`timerCallback`: a „nincs SD” ág nullázza a jelzőt, `:1517-1518`). A felhasználó hiheti, hogy a kártya kiírva — **adatvesztés kockázata** (a FB maga ír a meghajtóra: Paste, Rename, Delete, és a projekt autosave, ha a projekt az SD-n van). WP-G G-257 (Format) és az Eject az Akai-nál `Shift+Format Drive`.

**B5 — HW pad felengedés elvész → beragadó hang, P1.** A `handleCommand` pad ága `isPadButtonMessage && getPadState(...)` feltételű (`:1178`); a híd a felengedést `BTN_RELEASED = 0`-val küldi (`bridge:1798-1812`; `M/contracts/dcl/dcl_contracts.h:216`), ezért a `getPadState` hamis → a `noteOff` ág (`:1196-1197`) **soha nem fut**. Sustain-es / loopolt / Keygroup programnál a hang a következő ütésig szól. (A PadColor és PadMixer helyesen kezeli a felengedést: nincs külső `getPadState` feltétel.) Reprodukció: Keygroup programú trackkel a FB-n pad nyomás és elengedés.

**B6 — `addAsset` visszatérési kód téves értelmezése, P1.** `if( !ae.addAsset(...) )` (`FB/kick_fileUtils.cpp:253`) a `0`-t hibának, minden mást sikernek veszi; az enum: `AssetAlreadyExists = 0, AssetAdded = 1, AssetRemoved = 2, AssetInvalid = 3` (`M/engine/audio_file_manager/source/mt_audio_asset_manager.h:27-32`). Az `AssetInvalid` (pl. 0 bájtos fájl: `mt_audio_asset_manager.cpp:178` `file.getSize()`; vagy projekt nélkül: `state.isValid()` hamis) *sikernek* számít → `addSampleToProgram` nem létező assettel / `-1` programmal (`:262-265`). Minden hiba „This Sample has already Loaded” felirattal jelenik meg. Emellett a dekódolás a `callAsync` miatt az **UI szálon** fut (`:241`), az animáció közben fagy; a `sampleLoaded` sima `bool`, a worker (`:277`) és az UI szál is írja (adatverseny; a státuszsori „has Loaded” elveszhet).

**B7 — A lista rendezetlen, rejtett fájlokkal, P1.** `updateDirectories` = `curDir.findChildFiles(findFilesAndDirectories, false)` (`M/engine/audio_file_manager/source/mt_audio_asset_manager.cpp:445`) — nincs rendezés (a sorrend a fájlrendszeré: ext4-en hash-sorrend), nincs rejtett-fájl szűrés (`.DS_Store`, `System Volume Information`, `._*`, `$RECYCLE.BIN`), nincs „mappák elöl”. Az Akai böngésző név / méret / dátum szerint rendez (G-240).

**B8 — Szinkron, rekurzív keresés / szűrő + minden sorhoz komponens, P1.** `fileExtension()` (Project / Sample szűrő) `findChildFiles(…, true /*rekurzív*/, "*")`-t hív az UI szálon (`:1100`), a keresés ugyanígy az egész almappa-fát bejárja és minden találat nevét `std::string`-gé alakítja (`:1083-1089`). SD-n (22 GB, `kick_fileUtils.cpp:62` megjegyzés) ez perceket is vihet, közben a UI áll. A `KickTableView::refresh()` ráadásul **minden sorhoz** létrehoz egy komponenst (`M/kick_ui_controls/source/kick_table.cpp:340-366`; nincs virtualizáció), és a sor-szolgáltató soronként `getLastModificationTime()` + `getSize()` stat-ot hív (`:589-598`) és minden mappához sorba állít egy méret-számítást — a látható és nem látható sorokra egyformán.

**B9 — Előhallgatás-mód gomb állapota elcsúszik, P1.** Rejtéskor `auditionButton.setButtonText("OFF")`, `settings.preview.isOn = false` (`:1258-1261`), de a `KickMultiSelectTabBarButton::currentSelection` nem áll vissza (csak a `clearSelection()` tenné, `M/kick_ui_controls/source/kick_MultiSelectTabBarButton.h:17`). Visszatérés után a felirat „OFF”, a belső index pl. 1 → az első koppintás „MAN”-ra léptet („AUTO” kimarad). Reprodukció: AUTO mód → képernyőváltás → vissza → koppintás.

**B10 — Load gomb / Dial / dupla koppintás a nem megfelelő fájlon vagy projekt nélkül, P1.** A Load a kijelölt *bármilyen* fájlt `loadSample`-nek adja (`:210-212`): projektfájlon vagy mappán „Selected file cannot be loaded” (`kick_fileUtils.cpp:270-274`); projektet csak dupla koppintás tölt (Akai: a Load tölti). A projekt-ellenőrzés (`isProjectLoaded`) csak a gombok tiltásában él (`:1233-1238`); a Data Dial nyomás / dupla koppintás ugyanezt a `loadSample`-t hívja tiltás nélkül (`:644, 1276-1279`), ami projekt nélkül B6 szerint „sikeresnek” látszik. A cél-pad = `lastPressedPad + bankOffset` (`kick_fileUtils.cpp:262-265`); `lastPressedPad` globális statikus (`:7`), kezdetben `-1` → 0, **csak HW pad-ütés állítja** (`setHighlightLED`) — érintéssel vagy asztali gépen nincs módja célpadot választani, minden Load ugyanarra a padra (A01 / aktuális bank 1. pad) kerül. A NextSequence bankváltása is nullázza (`kick_nextSequence.cpp:225`). Hogy az `addSampleToProgram` a padon lévő mintát felülírja-e: NEM ELLENŐRIZHETŐ KÓDBÓL (engine-oldali olvasás hiányzik).

**B11 — Kedvencek sorindex-alapúak, P1.** A `selectedLeftRow` a *bal lista sorindexe* mentéskor (`:936, 943-948`); SD/USB megjelenése/eltűnése eltolja a sorokat (USB a 3. vagy 4. helyen), így a kedvenc más Place-t jelöl ki, mint amiben az útvonal van. Nem létező útvonal (kihúzott USB) némán üres listát ad (`load(path)` → `ae.setCurrentDir`, nincs üzenet). Dupla koppintás töröl (G-244 szerint Akai: Shift+érintés rendel) — véletlen törlés veszélye.

**B12 — Select módban a kijelölés két helyen él, P1.** A `KickTableView::selectRow` Select módban *toggle* (`kick_table.cpp:426-455`), és a `knobRotated` is `selectRow`-t hív (`:300-306`): a Data Dial forgatása végigjelöli az érintett sorokat. A FB `right.provider.onRowSelected` ráadásul saját `selectedFile`-ban kapcsolgatja ugyanazt (`:653-659`), a programozott `selectRow(0)` (pl. `enterDirectory`, `:1139-1141`; Paste után) tovább kuszálja → a vizuális és a logikai kijelölés eltér. A Select-ből kilépés nem törli a `selectedFile`-t (`:376-383`), így a visszatérő Delete (látható) több fájlt törölhet, amit a felhasználó nem lát kijelölve; közben Select módban a Delete rejtett (`:373`), a többes törlés szövege (`:1380-1388`) tehát csak ezen a kerülőúton érhető el.

**B13 — Projektbetöltés közben az engine könyvtárlistája versenyez az UI-val, P1.** A betöltő szál `closeProject()` → `MTAudioAssetManager::close()` (`M/engine/audio_engine/engine/mt_audio_manager.cpp:312`) átírja a `curDir`-t és a `dirList`-et (`mt_audio_asset_manager.cpp:38-47`), miközben a FB az UI szálon `dirList` const-referenciát olvas (`kick_fileBrowser.cpp:6, 631`). Új projektnél a FB azonnal `enterDirectory(-1)`-et hív (`:186-188`) a `startLoading` után; a dupla koppintású betöltés (`:636-642`) nem ellenőrzi az `isProjectLoading`-ot (csak a Dial-ág: `:345-348`). Betöltés után a motor a böngésző-könyvtárat a Factory Library-ra állítja vissza, amit a FB csak a következő `visibilityChanged → load(pwd)` rendez újra (`:1227`). (Adatverseny-gyanú; N/E időzítés.)

**B14 — New Project a FB-ből: ellenőrzés nélkül, P1.** Üres név esetén a `getAppDirs(MTProjects) + "/" + ""` = a projektek gyökere → `createProject` `Samples`/`Export` mappát hoz létre benne és a `loadOrCreate` egy `projects.kickPrj`-t (`:171-193`; `kick_xml_manager.cpp:191-204`). Duplikátum-ellenőrzés nincs (a kód a Projects képernyő másik, ellenőrzött útját `RecentProject::createProject`, illetve a FB halott `createProject`-jét `:1421-1443` nem használja); a *lemezen* létező névvel a „New” egyszerűen megnyitja a meglévőt. Érvénytelen karakterek (`/`, `..`) nincsenek kiszűrve.

**B15 — Rename: ütközés és injektálás, P1.** A névütközés-ellenőrzés a *recent projektek* nevével hasonlít (`:978-992`), nem a célmappával (egy minta-átnevezés tiltott lehet egy azonos nevű projekt miatt, míg egy valódi ütközés — `!newFile.exists()` — csendben kimarad `:999`), de utána a `settings.projects.renameProject` és `enterDirectory` mindenképp lefut (`:1025-1026`). A kiterjesztést a rendszer a régi fájlból fűzi hozzá (`:994-995`): ha a felhasználó kiterjesztést is begépel → `x.wav.wav`; a név `getChildFile`-ba megy → `../x` kilép a mappából. A popup címe „Please Enter the name of the Project” fájl-átnevezésnél is (`:1043`).

**B16 — SD/USB sorkezelés törékeny, P2.** `deviceData.erase(begin() + sdCardRow)` — az `sdCardRow` csak a `cellComponent`-ben frissül (`:403-406`), egyébként `-1` (pl. ha a lista még nem renderelt) → `erase(begin()-1)` UB (`:1505, 1544`); a `size() == 4` szám-trükk a másik sor indexéhez (`:428-429, 436-437, 1509, 1547`) nem általánosítható; a `row` (útvonal-vektor) az első hozzáadáskori mount-pontot tárolja (`:1492, 1531`), a `mountPointOf` csak az *első* eszközt adja (több USB, több partíció nem látszik); az 1 Hz-es timer akkor is fut, ha a képernyő rejtett (`:8`), és `left.table->refresh()` / `selectRow(0)` is hívódik háttérben (eltávolításkor a rejtett FB a `row[0]`-t tölti, `:1513`).

**B17 — Hangerő-popup bezárása rossz koordinátarendszerben, P2.** `mouseDown` a `getMouseDownScreenPosition()`-t hasonlítja a komponens *szülő-relatív* határaihoz (`:1313-1319`); asztali gépen skálázott / eltolt ablaknál (100/120/140 % nézet) a popup rossz helyen zárul vagy nem zárul. MPC-n (rejtett ablak, 0,0 eltolás) nagy valószínűséggel jó; N/E.

**B18 — Az első Place-koppintás elnyelődhet, P2.** `visibilityChanged` `onFirstLaunch = true`-t állít, és a `left.table->selectRow(...)` hívás `onRowSelected`-jében hamisítja vissza (`:1223-1228, 485-488`). Ha a `KickTableView::refresh()` üres határok miatt kilép, vagy a `rowComponents` még üres (`kick_table.cpp:340-345, 428`), a `selectRow` nem hívja a callbacket, a jelző `true` marad és a következő *valódi* Place-választás betöltés nélkül nyelődik el. Gyanú (időzítés-függő), reprodukció: a képernyő első megnyitása után azonnal Place váltás.

**B19 — Mappaméret: elavuló cache, elveszett munka, láthatatlan sorok elsőbbsége, P2.** (a) A cache kulcsa a mappa *saját mtime-ja* (`kick_fileBrowser.cpp:603-607`): egy almappában történt változás (új felvétel `…/Samples/x.wav`-ba) nem módosítja a szülők mtime-ját → **elavult méret** látszik. (b) A félbehagyott számítás (navigáció) `-1`-gyel visszatér és semmi nem kerül a cache-be (`kick_fileUtils.cpp:69-91`, `:706`): SD-gyökérnél, ahol egy mappa percekig tart, az oda-vissza navigáló felhasználó soha nem kap eredményt. (c) Minden mappasorhoz (a nem láthatókhoz is) feladat kerül az egyszálú pool-ba (`B8`), a látható sorok várhatnak a láthatatlanokra. (d) `RangedDirectoryIterator` alapértelmezetten követi a szimbolikus linkeket (JUCE 7: `FollowSymlinks::yes`), hurkot a `shouldExit` fogja meg — FAT/exFAT-on nincs link, ext4 USB-n lehet; N/E. (e) A bal oldali betöltő animáció (`:476-483`) gyakorlatilag nem látszik: a `load → enterDirectory → cancelFolderSizeCalculations` azonnal meghívja a `sizeCalculationEnd`-et (`:724-731, 1122`). (f) A `FolderSizeTests.cpp` nincs a CMake-ben (lásd 6.).

**B20 — Egyéb, P2.** `sampleFileList` három helyen duplikálva (`kick_fileBrowser.h:241`, `kick_fileUtils.h:48`, engine `mt_audio_asset_manager.cpp:178`), és olyan kiterjesztéseket is elfogad (`mp3;wmv;asf;wm;wma`), amiket a Kick-build dekódolni tudhat-e, nem igazolt: a Kick CMake-jeiben nincs `JUCE_USE_MP3AUDIOFORMAT` / WMF definíció (grep), az alapértelmezés mindkettő kikapcsolt — NEM ELLENŐRIZHETŐ KÓDBÓL. A Demos Place indexe (`DEMOSROW == 2`) beégetett (`kick_fileBrowser.h:31`, `:639`). A fejléc beállítás-gombja el van rejtve (`:54`; Browser Options hiány: G-259).

## 4. Tervben / hiányzik (Akai MPC Standalone-hoz képest)

Forrás: `WP-G.md` (Browser: G-226 – G-284), `known-issues-and-roadmap.md` §3.1, `cloud-sources.md`, `04-screen-audit.md` §6–7. A jegyzett hiányok:
- **Content / Expansion fülek** (G-234, G-235, G-266 – G-270): Drums / Instruments / Samples / Demos / My Files gyorsparancsok; MPC Documents mappa (G-232). A Kick Places listája más (Internal / Factory / Demos).
- **Rendezés** oszlopfejléc-érintésre (G-240), **Open** gomb (G-242), **Shift+Load All** (G-247), **Drive Info** (G-256), **Format Drive** exFAT-ra (G-257), **hat szűrő** (projekt, minta, pattern, kit, plugin preset, összes; G-258), **Browser Options** (méret / módosítási dátum / létrehozási dátum oszlop kapcsoló, rekurzív keresés kapcsoló, Project és Track adatmappák elrejtése, nem használt minták törlése betöltéskor: G-259 – G-265).
- **Előhallgatás**: tartott Play (G-250), **Sync** és **Warp** (G-254, G-255), Auto-tartam.
- **Projektbetöltés előtt kérdés**, hogy bezárja-e a jelenlegit (G-246): a Kickben a `closeProject()` csendben ment és bezár (`kick_xml_manager.cpp:321-328`), mentési hiba esetén a betöltés meghiúsul (és csak a mentési-hiba popup jelzi).
- **Sounds Mode / Favorites** (G-285 – G-292): a SOUNDS gomb a FB-t nyitja; saját Sounds-nézet nincs.
- **Splice** (G-172, G-271): nincs hivatalos API / SDK; a Kick nem tud a Spliceról (`cloud-sources.md` §1.5). Terv: a felhasználó helyi `~/Splice` mappája helyi forrásként (desktop), MPC-re kézi átvitel.
- **Google Drive**: nincs közös Kick-kliens (Google ToS 4(b)(1) vs. AGPL). Járható utak: (1) nyilvános mappa + BYO API-kulcs, (2) WebDAV (`rclone serve webdav`), (3) BYO kliens-ID. A `FileBrowserSource` interfész (state, beginAuth, list, stat, fetch `.part` + Range-folytatás, changes, signOut), közös `ListingCache` / `ContentCache` / `DownloadQueue`; hálózati kód külön, alacsony prioritású szálakon, **soha nem az audió- vagy UI-szálon**, csak teljesen letöltött fájl töltődhet be. Sorrend: M0 interfész + helyi mappa forrás + cache; M1 WebDAV; M2 nyilvános Drive-mappa BYO kulccsal; M3 eszköz-folyam + OneDrive; M4 S3 / Dropbox. **Jogi átnézés kell** a megvalósítás előtt.
- A mostani FB a `juce::File` / engine `dirEnter` köré épült (`tempList` = `Array<File>`, `ae.dirEnter`, hangmintát `File`-ból tölt): a forrás-absztrakció **nem toldható rá**, a Places/`row`/`deviceData` párhuzamos vektorok és az engine-oldali könyvtárlista cseréje kell. A Kick-build `JUCE_USE_CURL=0` (`A/CMakeLists.txt:111`), tehát a HTTP-kliens külön függőség (libcurl vagy más), ARMv7 keresztfordítással.
- A régi docs két állítása elavult: a WP-G „G-198/G-200/G-238 stb. OK” sorok fájl:sor hivatkozásai nem egyeznek a jelenlegi forrással; a `known-issues` H24 („`fix/project-relative-samples` kimaradt”) a mentési útra már nem igaz: a projekt mentése a külső mintákat a projekt mappájába másolja (`M/engine/project/kick_xml_manager.cpp:273-292`) — hardveren nem igazolt, és a Load pillanatában a külső fájlra mutat az asset (SD / USB kihúzás = hiányzó minta mentés előtt).

## 5. Javaslataim

1. **Place-modell.** A párhuzamos `deviceData` / `row` / `sdCardRow` / `USBRow` vektorok helyett `std::vector<Place>{label, icon, root, kind, isRemovable, mountPath}`; a Back és a keresés a `Place::root`-on áll meg, a védett útvonalak (`/`, `/data`, `/data/projects`, `/data/sounds`, `/data/demos`, jelenlegi projektmappa és ősei) sosem törölhetők / átnevezhetők. Ez egyben a `FileBrowserSource` (felhő) előfeltétele. Indoklás: B2, B11, B16.
2. **Műveletek háttérszálon, megszakíthatóan** (copy / move / delete / zsák-számolás), egységes `FileOpQueue` folyamatjelzővel és „sync” lezárással; eredmény a státuszsorban. Indoklás: B3, UX, USB biztonság.
3. **Valódi Eject** (`sync` + `umount`, hibánál „busy” visszajelzés, az automounter viselkedésének figyelembevételével) és Format csak megerősítésekkel, külső eszközre, előbb `mkfs.exfat` meglétének felmérése az MPC-n (a hardveren *nem* végeztem). Addig az Eject gomb legyen „Unmount”, ami csak sikeres `umount` után tűnik el.
4. **Rendezés + szűrés + keresés** aszinkron, generációs lemondással (a mappaméret mintájára), inkrementális találatokkal, felső korláttal. Alapértelmezett szűrés az *aktuális mappában* (az Akai szerint), a rekurzív keresés kapcsolóval (G-263).
5. **Tábla-virtualizáció** (csak a látható sorok komponense) a közös `KickTableView`-ben — ugyanezt használja a ListEdit, Song, Next Sequence, Pad Mixer is.
6. **Mappaméret finomítás**: látható-sorok-elöl feldolgozás; a számítás részeredményeit tartsd meg (almappa-méretek cache-be); fájlművelet után az érintett útvonallánc cache-bejegyzéseinek `invalidate`-je; SD-gyökéren „méret kiszámítása” koppintásra. A számolás ne írjon cache-be hamis (megszakított) értéket — ma nem ír, jó.
7. **HW-ergonómia (MPC)**: Data Dial nyomás = belépés/betöltés (már van), **Shift+Dial nyomás vagy `<`/`−` = feljebb** (ma csak érintés), `+/−` oldalankénti lépés; Delete/Rename/Eject/Format modális ablakok zárása hardverrel (ENTER = Yes, MAIN = No) a `modal_dialog_keys.h` mintájára. Indoklás: `bridge:893-905` csak a TC ablaknál fogad HW zárást; ha az érintés nem elérhető (RPT 8. kör), az ablak az MPC-t lefagyasztja.
8. **Load pad-cél láthatóan**: a Load gomb mellett/felirat „→ A03” és *első szabad pad* mód; a Load All pool-ba tölt és nem rendel padot (Akai szerint a pool-ba kerül, G-247).
9. **Projekt betöltési hiba és kérdés**: lásd PRJ-05 / PRJ-09 (Projects képernyő) — a FB közös utat használjon, és mutassa a hibaüzenetet.
10. **Külső minta másolása betöltéskor** (kapcsoló): SD / USB mintát a projekt `Samples` mappájába másoljon azonnal, ne csak mentéskor, hogy a kihúzás ne hagyjon hiányzó mintát.
11. **Tesztelhetőség**: a mount-felismerés (`mountPointOf`), a fájlnév-validáció, a védett útvonal-ellenőrzés, a státusz-leképezés és a pad-állapot kezelés tiszta (JUCE-mentes) függvényekbe, host-tesztekkel (mint `kick_hw` mintái).
12. **Felhő-források**: a Places-listán belül külön „Cloud / Network” szakasz, amit a most dummy **Expansion** fül kaphat; M0-ban az asztali gépen a konfigurálható helyi gyökerek (`~/Splice`) — nincs hálózati kód, nincs jogi kockázat.

## 6. Hardveres és unit-tesztek

**Ami ma fedi.**
- `Kick/tests/platform/FolderSizeTests.cpp`: a `FolderSizeCache` (get/put/invalidate/mtime-érvényesség) és a `GenerationTracker` host-tesztje, párhuzamosság- és lemondás-szimulációval. **Nincs regisztrálva a `Kick/tests/platform/CMakeLists.txt`-ben** (a „FolderSize” szó sehol nincs CMake-fájlban), a fejlécben kézi `g++` parancs van → a CI nem futtatja. A `calculateFolderSize`-t (valódi bejárás) nem teszteli.
- `AppSmoke.py` / `KICK_SMOKE_SCREENS`: a `MenuFileBrowser` képernyőkép készíthető (`A/Main.cpp:152-198`), de csak ha a környezeti változót megadják; az alapteszt nem járja végig.
- `PadColourMapTests`, `LedMapTests`, `AkaiMapTests`: a hídon lévő térképeket fedik, a FB-t nem.
- Nincs teszt: mount-felismerés, fájlműveletek, `addAsset` leképezés, kedvencek, Eject, rendezés, pad felengedés.

**MPC-n ellenőrzendők** (hardvert most nem használhatok; a `kick_cmd` `probe` csak olvasásra való, de ezt is a hardveres session végezze).
1. SD és USB felismerés: `/proc/mounts` tartalma (`/dev/mmcblk1p?` → `/sdcard`, `/dev/sdb?` → `/media/PenDrive`); több partíciós pendrive; a *boot-pendrive* is megjelenik-e USB-ként; kihúzás használat közben (`timerCallback`, B16).
2. **Back fölmenés**: `/sdcard` → `/` elérhető-e (B2); védett útvonalakon Delete tiltása javítás után.
3. Eject: marad-e mountolva (`/proc/mounts` az „Eject” után); `umount` jogosultság és az automounter reakciója; Format: `mkfs.exfat`/`mkfs.vfat` megléte.
4. Dupla koppintás mappára / projektre (450 ms); Select mód + Data Dial (B12); Shift+SOUNDS kedvencek (B11) USB-vel és anélkül.
5. Pad-ütés a FB-n: beragadó hang Keygroup programmal (B5); a zöld cél-pad LED megjelenik-e (a híd csak `EN_PAD_SET_COLOUR_STATE`-et dolgoz fel: `bridge:138-160` → az egyes pad-LED parancsok eldobódnak, így a `setHighlightLED` (`SET_SINGLE_PAD`) **MPC-n valószínűleg nem látszik** — N/E).
6. Nagy SD (22 GB) gyökér: a lista rendezettsége, a „…” méret mennyi ideig áll, a UI válaszkészsége kereséskor / szűréskor (B8, B19); memória (komponens/sor).
7. Load: formátumok (wav, aif, flac, ogg, mp3, wma) — melyiknél jön hiba (B20); nagy minta UI-fagyás (B6); felülírja-e a padot (B10).
8. Projekt betöltés a FB-ből SD/USB-ről; kihúzás után mentési-hiba popup; Demos-ról betöltve a mentés hibaüzenete (`Cannot save over factory demo/template. Please use Save As…` — Save As nincs).
9. Modális ablak (Delete/Eject) hardverrel nem zárható: érintés nélkül (RPT 8. kör) megáll-e.

## 7. TODO

Függőségek: FB-26 ← FB-02, FB-18, FB-10; FB-14 ← kick_table változtatás (közös komponens); FB-15 ↔ PRJ-01; FB-16 ↔ PRJ-08.

*Hibák*
- [ ] **FB-01** (P0, S) Delete megerősítés: lebegő `popup` hivatkozás megszüntetése (érték szerinti capture, törlés a modális callback `APPLY` ágán) — `FB/kick_fileBrowser.cpp:1390-1409`.
- [ ] **FB-02** (P0, M) Place-enkénti gyökér: a Back / `exitDirectory` / keresés ne léphessen ki a Place-ből; védett útvonal-lista (`/`, `/data`, `/data/projects`, `/data/sounds`, `/data/demos`, a jelenlegi projektmappa és ősei) a Delete / Cut / Rename / Paste előtt; `checkIsCurrProj` útvonal-alapú (ős/leszármazott is) — `:16, 1133, 1145-1158, 1323-1338, 453-466`.
- [ ] **FB-03** (P0, M) Paste / Rename / New: létező cél felülírásának tiltása vagy megerősítése („(2)” utótag), mappa önmagába másolásának tiltása, névvalidáció (`/`, `..`, vezérlőkarakter, FAT-tiltott karakterek, hossz, kiterjesztés-duplázás) — `:292-329, 975-1029`.
- [x] **FB-04** (P0, L) Fájlműveletek (copy / move / delete) háttérszálon, folyamatjelzővel, megszakítással, lezáró `sync`-kel; UI szál mentesítése. Függ: FB-03.  
  **R-6:** `kick_fileOps.h` (copy / move / delete, byte progress, cancel, partial target removed, sync at the end) on a `BackgroundTask` with a progress dialog; Paste, Delete, Duplicate and Save As all use it. Host tests with temp folders.
- [x] **FB-05** (P1, M) Eject = valódi `sync` + `umount` (platform-réteg mögött), „busy” hiba visszajelzés, a sor csak siker után tűnjön el; az `isSdCardEject` stat. bool a fejlécben (`kick_fileUtils.h:20-21`) megszüntetése — `:408-443, 1501-1518`.  
  **R-6:** Eject = `sync` + `umount2` (no lazy unmount) in a background task; busy / not mounted / failed answers; the row stays when it fails; an ejected drive stays hidden until it was unplugged (automounter). Needs the MPC: umount permission and the automounter reaction.
- [x] **FB-06** (P1, S) HW pad felengedés: a `noteOff` elérhetővé tétele (a külső `getPadState` feltétel helyett pad-üzenet + állapot szerinti ág) — `:1178-1202`.  
  **R-6:** Pad press and release both handled; `HeldPads` ends the note that was started (also after a bank change); screen exit ends held notes.
- [x] **FB-07** (P1, S) `addAsset` státusz-leképezés (`AssetAdded` / `AlreadyExists` / `Invalid` külön üzenettel), `sampleLoaded` atomic, dekódolás az UI szálról le — `kick_fileUtils.cpp:239-278`.  
  **R-6:** `mapAssetResult` (AlreadyExists / Added / Removed / Invalid / no project / no program), decoding and copying on the worker, `sampleLoaded` atomic, the status line says what happened.
- [x] **FB-08** (P1, M) Rendezés (név / dátum / méret, mappák elöl), rejtett és rendszerfájlok szűrése, oszlopfejléc-koppintás — engine `mt_audio_asset_manager.cpp:445` vagy FB-oldal; G-240.  
  **R-6:** Own listing (`kick_fileList.h`): folders first, natural name / date / size order (column header tap), hidden / system files filtered (option).
- [x] **FB-09** (P1, M) Szűrő / keresés aszinkron (generációs lemondás), korlátos találati szám; Project/Sample szűrő alapból az aktuális mappában — `:1078-1106`.  
  **R-6:** Listing and search run on a worker, newest request wins, results capped at 500; filters work in the current folder; sub-folder search is an option.
- [x] **FB-10** (P1, M) Tábla-virtualizáció (csak látható sorok), a soronkénti stat lusta — `M/kick_ui_controls/source/kick_table.cpp:340-366`, `FB/kick_fileBrowser.cpp:522-621`. (Közös komponens: ListEdit, Song, NextSeq, PadMixer is érintett.)  
  **R-6:** `KickTableView::enableVirtualRows` (opt-in): only visible rows get components; no per-row stat (the listing carries size / date); painting only the clip.
- [x] **FB-11** (P1, S) Előhallgatás-mód reset: rejtéskor `clearSelection()` + `incrementSelection()` (OFF), vagy a mód megőrzése — `:1258-1261`.  
  **R-6:** Audition mode is reset consistently when the screen hides.
- [x] **FB-12** (P1, M) Load: projektfájl → projektként (kérdéssel), mappa → belépés; minta-Load csak betöltött projektnél (a Dial/dupla koppintás ágon is); cél-pad kijelzése + „első szabad pad” mód; Load All = a látható (szűrt) lista, pool-ba, nem csak az első padra — `:210-216, 625-646, 1276-1279`, `kick_fileUtils.cpp:191-236`.  
  **R-6:** Load: project folder / .kickPrj opens as a project, folder is entered, sample goes to the target pad (last pressed pad, else first free pad; shown in the status line), no project = message; Load All = the visible samples into the pool; Open enters a folder.
- [x] **FB-13** (P1, M) Kedvencek: Place-azonosító útvonal-prefix alapján (nem sorindex), nem létező útvonalra figyelmeztetés, Shift+érintés rendel / hosszú nyomás töröl (G-244) — `:889-973`.  
  **R-6:** Favourites keep a path; the place that holds it is selected whatever its row; a missing folder is reported; Shift+tap re-assigns, a long press clears (the double tap no longer clears).
- [x] **FB-14** (P1, M) Select mód: a programozott kijelölés elválasztása a toggle-tól (`setSelection(rows)` API), Dial-forgatás ne kapcsolgasson, Select-ből kilépéskor a kijelölés törlése, Delete látható Select módban, Select All — `kick_table.cpp:300-306, 426-455`, `FB/kick_fileBrowser.cpp:365-384, 650-675`. Függ: közös tábla.  
  **R-6:** Select mode with a cursor (`enableSelectCursor`): the Dial moves it, Dial press marks, Select All, Delete visible, leaving Select clears the marks.
- [x] **FB-15** (P1, M) Betöltés közbeni verseny: `isProjectLoading` őr a dupla koppintás ágon, `enterDirectory` ne fusson `loadProject` közben; az engine `curDir`/`dirList` védelme (mutex vagy üzenetszálra marshal) — `:186-188, 636-642`, `mt_audio_asset_manager.cpp:38-47, 445`. Kapcsolódik: PRJ-01.  
  **R-6:** The browser lists the file system itself (the engine's curDir / dirList are not used any more) and every project open goes through the one load path with its `isProjectLoading` guard.
- [x] **FB-16** (P1, S) New Project: a Projects képernyő ellenőrzött útjának használata (üres név, lemezen létező név, érvénytelen karakter); a halott `FileBrowser::createProject` törlése — `:171-193, 1421-1443`. Kapcsolódik: PRJ-08.  
  **R-6:** New project uses `project_actions::createProject` (checked name, disk check); the dead `createProject` is gone.
- [x] **FB-17** (P2, S) Rename: ütközés a *célmappában*, a `renameProject` csak sikeres mozgatás után, előtöltött név, helyes popup-cím — `:975-1056`.  
  **R-6:** Rename checks the target folder, keeps / does not double the extension, renames a project as a whole, updates the Recent list; the keyboard starts with the current name.
- [x] **FB-18** (P2, S) SD/USB lista: `-1` index UB, a `size()==4` trükk kiváltása (`Place` modell), több USB / partíció, a timer csak látható képernyőn — `:403-406, 428-439, 1463-1561`, `kick_fileUtils.cpp:15-60`.  
  **R-6:** `Place` model, `kick_mounts::parse` / `reconcile` (several drives / partitions), timer only on the visible screen, no `-1` erase.
- [x] **FB-19** (P2, S) Hangerő-popup bezárása: lokális koordináták (`e.getEventRelativeTo(this)`) — `:1311-1321`.  
  **R-6:** Volume popup closes by local coordinates.
- [x] **FB-20** (P2, S) `onFirstLaunch` jelző megbízhatóvá tétele (explicit „skip next load” token) — `:485-488, 1223-1228`.  
  **R-6:** No `onFirstLaunch` flag: places are selected silently (`selectRow(..., dontSendNotification)`).
- [x] **FB-21** (P2, S) Halott kód törlése (`FolderSizeCalculator`, `startFolderSizeCalculation`, `FileBrowser::addMultipleAssets`, `fileList`, `static int c`) és az egyetlen `sampleFileList` konstans (a tényleges dekóder-támogatás szerint) — `kick_fileBrowser.h:241, 293-310`, `kick_fileUtils.h:48`.  
  **R-6:** Dead code removed (`FolderSizeCalculator`, `addMultipleAssets`, `fileList`, `NewProjectPopupData`), one `sampleExtensions()` list in the browser.

*Hiányzó funkciók*
- [x] **FB-22** (P1, M) Format: megvalósítás (külső eszköz, kétlépcsős megerősítés, `mkfs` felmérése MPC-n) vagy a halott popup kód eltávolítása — `:1830-1836, 1870`.  
  **R-6:** Format: two confirmations, drive re-checked, unmount, `mkfs.exfat` / `mkfs.vfat` without a shell, background task. Linux only; needs a hardware test on the MPC (tool present? permission?) before it is trusted.
- [x] **FB-23** (P1, M) Content / Expansion fülek: gyorsparancsok (Drums / Instruments / Samples / Demos / My Files) vagy elrejtés (ma „halott gombok”); G-234/235/266-270.  
  **R-6:** Places / Content / Expansion tabs filter the left list (Content = factory library top folders, My Projects, Demos).
- [~] **FB-24** (P2, M) Browser Options (oszlop-kapcsolók, rekurzív keresés kapcsoló, adatmappák elrejtése), Open gomb, Drive Info, további szűrők (pattern, kit, plugin preset) — G-242, G-256, G-258 – G-265.  
  **R-6:** Browser Options (columns, sub-folder search, hide system files / project data folders, copy samples into the project, MIDI filter), Open button, Drive Info. Not done: filters for kits and plugin presets (Kick has no such file format).
- [~] **FB-25** (P2, M) Előhallgatás: tartott Play, Sync, Warp; nagy fájl háttérbetöltés — G-250, G-254, G-255.  
  **R-6:** Held Play (MAN mode) auditions while the finger is on the button. Not done: Sync / Warp (the engine preview has no tempo sync / time stretch: an audio feature, needs a decision) and background loading of a big file (the engine's `previewOn` call; needs a measurement on the MPC).
- [x] **FB-26** (P1, L) `FileBrowserSource` absztrakció + `LocalFolderSource` (a mostani logika átemelve) + `ListingCache`/`ContentCache` + konfigurálható helyi gyökerek (desktop `~/Splice`); hálózati szálmodell váz — `cloud-sources.md` §3. Függ: FB-02, FB-10, FB-18.  
  **R-6:** W2-C source layer + `+ Folder` (local folder location, e.g. `~/Splice`) next to `+ WebDAV`.
- [ ] **FB-27** (P2, L) WebDAV forrás (M1: PROPFIND, Range GET, TLS, `.part` folytatás); HTTP-kliens döntés (libcurl vs. más; `JUCE_USE_CURL=0`), ARMv7 keresztfordítás. Függ: FB-26. Jogi átnézés (libcurl/TLS licenc) előfeltétel.
- [ ] **FB-28** (P2, L) Nyilvános Drive-mappa + BYO API-kulcs (M2); OneDrive eszköz-folyam (M3). **Blokkoló: jogi átnézés** (Google ToS 4(b)(1) vs. AGPL, Splice ToS) — `cloud-sources.md` §3.6. Függ: FB-27.

*Javaslatok*
- [x] **FB-29** (P1, S) Hardveres ergonómia: feljebb (Shift+Dial / `−`), modális ablakok HW-zárása (ENTER/MAIN) Delete/Rename/Eject/Format-ra — `bridge:893-905`, `A/hardware/modal_dialog_keys.h`.  
  **R-6:** BACK / Shift+ENTER = folder up, +/- = page, Shift+Dial press; dialogs close from the hardware through the bridge keys (S-D).
- [x] **FB-30** (P2, S) Mappaméret: láthatók elöl, részeredmény-megőrzés, szülőlánc-invalidálás fájlművelet után, SD-gyökéren kézi „számolás” — `FB/kick_folderSizeHelper.h`, `:738-789`.  
  **R-6:** Sizes only for rows on screen, sub-folder sizes cached while scanning (partial work survives a cancel), caches invalidated after file operations, drive roots compute the selected folder only.
- [x] **FB-31** (P2, S) Üres mappa / hozzáférési hiba / „kártya eltávolítva” üzenet a státuszsorban; Load eredmény-üzenet.  
  **R-6:** Status line: empty folder, unreadable folder, search results / cap, load results, drive removed.
- [x] **FB-32** (P2, M) Külső minta másolása a projekt `Samples` mappájába betöltéskor (kapcsolóval) — `kick_xml_manager.cpp:273-292`.  
  **R-6:** Load copies a sample from outside the project into `<project>/samples` (option, default on; an identical copy is reused).

*Tesztek*
- [ ] **FB-33** (P1, S) `FolderSizeTests.cpp` felvétele a `Kick/tests/platform/CMakeLists.txt`-be (`add_test`), + a `calculateFolderSize` tmp-könyvtár-teszt (üres, mély, lemondás, symlink).
- [x] **FB-34** (P1, M) `mountPointOf` / SD-USB felismerés tiszta függvénybe + egységteszt (`/proc/mounts` minták: SD `/sdcard`, USB `/media/PenDrive`, `\040`, EFI, több partíció, hiányzó eszköz).  
  **R-6:** `kick_mounts.h` + `MountsTests.cpp` (SD, USB, several partitions, `\040`, EFI, duplicates, reconcile).
- [ ] **FB-35** (P1, M) Fájlművelet-logika (védett útvonalak, névvalidáció, felülírás-döntés) tiszta modulba + tmp-könyvtár egységteszt. Függ: FB-02, FB-03.
- [x] **FB-36** (P2, S) `addAsset`-státusz leképezés és pad-felengedés host-teszt. Függ: FB-06, FB-07.  
  **R-6:** `FbLogicTests.cpp`: asset status mapping, held pads / release, pad text, listing status.
- [x] **FB-37** (P2, S) AppSmoke: `MenuFileBrowser` képernyőkép fixture-könyvtárral (rendezett lista, rejtett fájlok nélkül).  
  **R-6:** `ProjectsBrowserSmoke.py browser` (`kick-browser-smoke`): fixture folder, expected sorted rows without hidden files, screenshots.
- [ ] **FB-38** (P1, M) MPC hardveres ellenőrzőlista végrehajtása (6. szakasz) — a hardveres session végezze.

## 8. R-6 hardware checklist (what only the MPC can show)
1. SD + USB: `/proc/mounts` shows up as places (several partitions, a name with a space); pull a drive while it is shown.
2. Eject: the row disappears only after a successful `umount`; with a project open on the drive: refused; a drive in use answers "busy"; the automounter does not bring it back.
3. Format (SD / USB): `mkfs.exfat` or `mkfs.vfat` exists? permission? The drive shows up again afterwards (or must be re-plugged).
4. Copy / move a few hundred MB between internal and SD: the screen and the Data Dial stay alive, progress moves, Cancel removes the unfinished item, no half file is left.
5. A folder with thousands of files: scrolling speed, memory; sorting by column tap; folder sizes only for visible rows; SD root sizes on selection.
6. Pad press / release in the browser with a Keygroup / looped program: no stuck note; bank change while a pad is held.
7. Load a wav (target pad shown in the status line), an empty / broken file (message), mp3 / wma (decoder support unknown), Load All; copy-into-project option with the stick pulled afterwards.
8. Favourites: assign with Shift+tap, recall with an SD card in a different row, long press clears.
9. Select mode with the Data Dial (cursor, press marks, All, Delete).
