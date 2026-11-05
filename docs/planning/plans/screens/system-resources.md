# System Resources — review és TODO

Dátum: 2026-11-04. Ág: `claude/kick-plans`. Alap: **statikus kódolvasás** (nincs build/futtatás/MPC). „KÉSZ" = van bekötött logika, nem hardveren igazolt; ami kódból nem dönthető el: **N/E (NEM ELLENŐRIZHETŐ KÓDBÓL)**. `M/` = `Kick/modules/`, `A/` = `Kick/apps/`. Kiindulás: `docs/planning/plans/research/screen-inventory.md` §4.27, §6; `docs/hardware/key61/walkthrough/wp-g.md` G-025…G-038 (több „OK" ítélete a kód alapján cáfolható, lásd 3.).

## 1. Áttekintés

**Nem képernyő, hanem modális dialógus.** `kick_ui::KSystemResources` (`M/kick_ui_systemResources/kick_ui_systemResources.{h,cpp}`) halom-singleton (`Instance()`, `DeletedAtShutdown`: `.h:50-56`, `.cpp:15-20`), amit a `KickModeHeader` jobb szélső „Resources" ikonja nyit a `{198, 20, 604, 525}` ablakban (`M/kick_ui_header/header/kick_modeHeader.cpp:26-29, 81-96`). Tartalom: CPU, RAM, INTERNAL MEMORY, SDCARD, USB DRIVE csúszka (csak kijelző, `setEnabled(false)`), „Purge All", „Purge Unused", „OK". A mérés 1 Hz-es `juce::Timer`, amit a fejléc `start()`/`stop()` hív (`.cpp:91-99`, `kick_modeHeader.cpp:83-92`).

**Elérés:** csak azokon a képernyőkön, amelyek `KickModeHeader`-t használnak (Grid, Step Seq, Looper, Sampler, List Edit, Pad/Track Mute, Pad Mixer, Channel Mixer, File Browser, Settings, SW Update, …). **A két leggyakoribb képernyőn nem érhető el:** a `HomeListView` és a `HomeOptions` a felső sávot (`AppTopbar`) használja (`requiresControlBar()==true`: `M/kick_ui/dialogs/kick_AppMainMode.h:34`, `kick_AppMenuOptions.h:34`), amelyen nincs Resources-gomb (`kick_AppTopbar.cpp`: nincs ilyen elem). Hardver-gomb nincs (az Akai MPC-n sincs, ott a fejléc Mem/CPU jelzője nyitja).

**Bezárás:** OK gomb (`.cpp:80-82`, `exitDialog(APPLY)`), a dialógus X-e, vagy kívülre érintés (`dialogDisplayer::mouseDown`: `M/kick_ui_controls/source/kick_utils_popup.cpp:311-315`). Hardveres bezárás **nincs** (a Data Dial/ENTER/MAIN nem zárja; a `KSystemResources` nem `KickControlsProcessor`, így a `dialogDisplayer::handleCommand` `dynamic_cast`-ja nem talál fogyasztót: `kick_utils_popup.cpp:226-232`).

**Holt kód:** `Screen::SystemResources` (`M/kick_ui_common/source/kick_IScreen.h:63`) és a `KickSysResources : IScreen` (`kick_ui_systemResources.h:84-93`) nincs regisztrálva (`M/kick_ui/dialogs/kick_AppHome.cpp:286-314`); a `load(Screen::SystemResources)` csendben a `HomeListView`-ra esik vissza (`:354`). A smoke-képernyőkép-tábla viszont tartalmazza a `"SystemResources"` kulcsot (`A/kick/Main.cpp:167`) → a „SystemResources.png" valójában a főképernyőt örökíti meg.

## 2. Funkciók és állapotuk

| Funkció | Állapot | Bizonyíték |
|---|---|---|
| Dialógus megnyitása a fejlécből | KÉSZ (de a főképernyőről/menüből nem érhető el) | `kick_modeHeader.cpp:81-96`; `kick_AppTopbar.cpp` |
| 1 Hz-es frissítés (start/stop a megnyitáshoz kötve) | RÉSZLEGES: az első mérés 1 s késéssel jön; a mérés a **UI szálon** fut, `popen("df")`-fel | `.cpp:91-132` |
| CPU jelző | **BUGOS**: a `/proc/stat` *kumulatív* számlálóiból számol, nem különbséget vesz → boot óta mért átlag | `M/hardware/source/kick_Resorces.cpp:26-46` |
| RAM jelző | RÉSZLEGES/BUGOS GYANÚ: `sysinfo.freeram` (a page cache „használtnak" számít); nem-Linux/nem-Windows platformon a `sysRamInfo` inicializálatlan | `kick_Resorces.cpp:111-134`; `.cpp:104-110` |
| INTERNAL MEMORY (flash) | **BUGOS** MPC-n: fix `/home/Kick`; inicializálatlan `sysSizeInfo`; az érték akkor is kiíródik, ha `status==false` | `.h:79`; `.cpp:103-111`; `kick_Resorces.cpp:62-100` |
| SDCARD jelző | BUGOS GYANÚ MPC-n: `/dev/mmcblk1p1` eszközcsomópont `df`-fel (az SD az MPC-n `/sdcard`) | `.h:80`; `.cpp:112-121`; `kick_fileUtils.cpp:33-36` |
| USB DRIVE jelző | BUGOS MPC-n: `/dev/sda1` (az MPC a meghajtókat `sdb`-től számozza; a pendrive `/media/PenDrive`) | `.h:81`; `.cpp:122-131`; `kick_fileUtils.cpp:37-40, 54-58`; `hw-behaviour.md:123` |
| Purge Unused / Purge All | **STUB** (üres lambda; az ikonok látszanak) | `.cpp:78-79` |
| OK / X / kívül érintés | KÉSZ | `.cpp:80-82`; `kick_utils_popup.cpp:311-315` |
| Hardveres bezárás (ENTER/MAIN/Data Dial) | **STUB** (nincs) | `kick_utils_popup.cpp:226-232` |
| Sample-memória (cache) kijelzés | **HIÁNYZIK** | — |
| Memória-/tárhely-figyelmeztetés | **HIÁNYZIK** | — |
| Teszt | csak konstrukció (`KSystemResources::Instance()`); a képernyőkép-tábla hibás kulcs | `Kick/tests/platform/AppSmoke.h:13`; `A/kick/Main.cpp:167` |

## 3. Mi nem működik / hibák

**3.1 A CPU-jelző nem terhelést mér (P0).** `cpuInfo` az első `/proc/stat` sort olvassa, és `(totalTime - idle) / totalTime` arányt ad (`kick_Resorces.cpp:36-46`). A számlálók boot óta halmozódnak, ezért az érték a *teljes üzemidő átlaga*, percek múlva gyakorlatilag állandó; sem a lejátszás-, sem a betöltés-terhelés nem látszik. További pontatlanság: az `iowait` „foglaltnak" számít, a `guest`/`guest_nice` már benne van a `user`/`nice`-ban (dupla számolás). Reprodukció: bekapcsolás után 5 perc üresjárat, majd nehéz projekt lejátszása — az érték alig mozdul. Hatás: az audio-dropout diagnózisához használhatatlan (a doksi `wp-g.md` G-027 „OK"-ja téves).

**3.2 Inicializálatlan `sysSizeInfo` → véletlen érték az INTERNAL MEMORY-n (P0).** `mt::sysSizeInfo sInfo;` inicializálatlan (`kick_ui_systemResources.cpp:103`); a `sizeInfo` csak `status=false`-t állít be, ha az útvonal nem létezik (`kick_Resorces.cpp:64-67`), a hívó viszont feltétel nélkül `flash.setValue(sInfo.perSize)`-et hív (`:111`). Az SD/USB ágon a `sInfo` újrahasznosul: ha `status==true`, de a `df` kimenet egyetlen sora sem tartalmazza az útvonalat (`line.find(path)`, `kick_Resorces.cpp:87`), a `perSize` az *előző mérésé* marad (a flash értékét kapja az SD). Nem-Linux/nem-Windows platformon (macOS) a `ramInfo` meg sem tölti a struktúrát (`:111-134` nincs `#else`), ott a RAM-jelző szemét.

**3.3 Az MPC-n a tár-útvonalak rosszak (P0).**
- `internalPath = "/home/Kick"` (`.h:79`; ugyanez a `kick_Hwpref.cpp:890` eszköz-infóban). Az MPC-n a Kick adatterülete a `/data` (`pendrive-install.md` §2; `M/common/source/kick_paths.cpp:139-151`: a `/home/Kick/…` csak tartalék). Hogy a `/home/Kick` létezik-e az MPC-n, N/E; ha nem, a jelző inicializálatlan értéket mutat (3.2), ha igen, nem a `/data` eMMC tele-ségét. Pedig a projektek, minták és (a zongora-tervvel, `02-piano-clap.md`) a nagy mintakönyvtárak a `/data`-n vannak — a legfontosabb adat hiányzik.
- `sdCardPath = "/dev/mmcblk1p1"`, `usbDiskPath = "/dev/sda1"` (`.h:80-81`). A fájlböngésző ezt már megoldotta: a `/proc/mounts` alapján keres (`M/kick_ui/modes/filebrowser/kick_fileUtils.cpp:8-50`; „the MPC numbers its drives from sdb"). A `df -h <eszközcsomópont>` viselkedése az MPC BusyBox-án N/E, és a `line.find(path)`-os szűrő a *Filesystem* oszlopra támaszkodik, ami nem biztos, hogy az átadott csomópont.
- A `sizeInfo` shell-parancsot épít (`"df -h " + path`, idézés nélkül) és `popen`-nel futtatja (`kick_Resorces.cpp:70-99`) — a **message szálon, másodpercenként háromszor**. ARM-on minden hívás `fork`; lassú vagy félig kihúzott USB-n blokkolhat (a dialógus ilyenkor lefagyni látszik). A méretek `getIntValue()`-val „1.2G" → 1-re csonkolódnak (`:92-94`; a `kick_Hwpref.cpp:882-900` eszköz-infó ezeket használja).

**3.4 A Purge gombok halottak, és az engine alatta sem működik (P0).** A kezelők üres lambdák (`kick_ui_systemResources.cpp:78-79`). Ami az engine-ben megvan: `ASoundCache::purge()` a ciklus első elemére `return it->purge()`-t hív, tehát legfeljebb egy mintát érint (`M/engine/audio_file_manager/source/audiofile_manager.cpp:51-57`); `ASound::purge()` csak `getReferenceCount()==0` esetén ürít (`audio_sound.cpp:54-60`), de a cache maga is tart referenciát (`juce::ReferenceCountedArray`, `audiofile_manager.h`), tehát minden gyorsítótárazott mintára ≥1 → **kódból következően soha nem üríti ki** (a lusta újratöltés `ASound::getData → reload`, `audio_sound.cpp:47-52`, viszont megvan). Kívülről senki nem hívja (`purge()`: csak `audio_sound_player.cpp:55`). Hatás: a felhasználó a memóriahiányt (nagy zongora-/mintaprojekt) nem tudja kezelni; az Akai-féle „Purge Unused/All" hiányzik (`wp-g.md` G-031/G-034).

**3.5 Megjelenítési hibák (P1).**
- `visibilityChanged` az SD/USB csúszkát is láthatóvá teszi (`kick_ui_systemResources.cpp:158-167`), a timer csak 1 s múlva rejti el → villanás; az első másodpercben a csúszkák a 0/előző állapotot mutatják (a `start()` csak a timert indítja: `:91-94`).
- A `paint()` fix koordinátás elválasztóvonalai: a második ferde (`367,180 → 584,182`, `:137-138`), az elrendezés mind fix pixel (`:143-155`) — nem skálázódik, nem igazodik a téma-változáshoz.
- A modul-fejléc másolt leírása („ID: kick_ui") hibás (`kick_ui_systemResources.h:30`) — kozmetikai, de a Projucer-jegyzékben félrevezető.
- A fejléc-kezelőben nem használt `onDialogClose` lambda (`kick_modeHeader.cpp:95-97`) — halott kód; a `stop()` a dialógus visszahívásán át fut (`:90`), ez rendben van.

**3.6 Elérhetőség és hardver (P1).** A főképernyőn (Sequence/Track/Program) és a Menüben nincs Resources-gomb (1.). Modális ablak lévén a hardveres gombok (MAIN/GRID/…) a háttérben váltják a képernyőt, miközben a dialógus nyitva marad (`KickHome::handleCommand` nem modál-védett: `kick_AppHome.cpp:487`); lásd `header-statusbar-popups.md` HDR-02.

## 4. Tervben / hiányzik (Akai MPC Standalone-hoz képest)

Az Akai MPC (WP-G, `docs/hardware/key61/walkthrough/wp-g.md`):
- **Fejléc Mem/CPU jelző, érintésre System Resources ablak** (G-209): CPU %, RAM, Drives (belső/külső szabad hely); a Kick-ben a dialógus csak a fejléc ikonjáról nyílik, a jelző nincs a fejlécen.
- **Purge Unused / Purge All** (G-031, G-034): a nem használt vagy az összes minta kivétele a *projektből/memóriából*; a Kick-ben stub.
- **Memory and Streaming** nézet, **Disk Streaming**, „Stream From Disk" minta-mód, hiányzó minták piros jelzéssel és később betölthetően (G-035…G-038): a Kick-ben nincs; a minták float32 sztereóban a RAM-ban élnek, streaming nincs (`known-issues-and-roadmap.md` §3.6, `02-piano-clap.md`) — a nagy mintakönyvtárak miatt ez a memória-nyomás fő oka lesz.
- Memóriahiány esetén tanácsok (G-030): nem kritikus.
- Akkumulátor-jelző (G-026): **NEM-KICK** (a Key 61-en nincs akku).
- `04-screen-audit.md` §6: „System Resources Purge" nyitott tétel.

## 5. Javaslataim

1. **Valódi mérők, UI-n kívül:** CPU a `/proc/stat` két mintájának *különbségéből*; RAM a `/proc/meminfo` `MemAvailable`-jéből; a **Kick folyamat** RSS-e (`/proc/self/statm`); az audio-callback terhelése (JUCE `AudioDeviceManager::getCpuUsage()`) és az xrun-számláló — ezek a Kick-felhasználónak többet mondanak, mint a rendszer-CPU. Indoklás: dropout-diagnózis, nagy projektek memóriatervezése.
2. **Tárak a mount-táblából és `statvfs()`-ből:** belső = `/data`, SD = `/sdcard`, USB = `/media/*` (a fájlböngésző `mountPointOf` logikájának kiemelésével), szabad hely MB/GB-ban, nem csak %-ban. `popen("df")` és az inicializálatlan struktúra megszűnik; nem létező tár *rejtve*, nem 0 és nem szemét.
3. **Mintavétel háttérszálon** (1 Hz, atomokba), a timer csak olvas; a `start()` azonnali első mérést ad; a láthatóság-logika egyszerűsödik (nincs villanás).
4. **Valódi Purge:** (a) `ASoundCache` szemantikája: „csak a cache tartja" (`refcount==1`) = kiüríthető; az összes elemen végig; (b) *nem RT szálról*, lejátszás/felvétel közben tiltva vagy megerősítéssel (a lusta újratöltés blokkoló fájlolvasás — nem szabad az audio-szálon); (c) „Purge Unused" = nincs pad/audio-track/looper hivatkozás; (d) visszajelzés („312 MB felszabadítva"); (e) a memóriasor mellett a *mintacache mérete* (ezért kell az engine-statisztika).
5. **Elérhetőség:** Resources-gomb a felső sávra és/vagy menü-csempe; hardveres bezárás a TC-ablak mintájára (általánosítva: `header-statusbar-popups.md` HDR-02): ENTER/Data Dial-nyomás = OK, MAIN = bezár.
6. **Státuszsor-integráció:** kis szabad hely (<5 % a `/data`-n) és magas RAM jelzése a `AppStatusbar::boxInfo`-ban; mentés előtti ellenőrzés (a `onProjectSaveFailed` már van: `kick_AppHome.cpp:320-325`).
7. **Teljesítmény/érintés:** a dialógus 604×525-ös fix elrendezése; relatív elrendezés (`setBoundsRelative`), érintési célok ≥44 px (a Purge/OK gombok 140×40).
8. **Tesztelhetőség:** a parserek (`/proc/stat`, `/proc/meminfo`, `/proc/mounts`) tiszta, JUCE-mentes függvények egy `A/kick/hardware/…h`-ban, fixture-fájlokkal (a `SystemProbeTests.cpp` és `FolderSizeTests.cpp` mintája).

## 6. Hardveres és unit-tesztek

**Ma:** nincs host-teszt a mérőkre; az `AppSmoke.h:13` csak példányosítja a dialógust. A smoke-képernyőkép-tábla `"SystemResources"` kulcsa a főképernyőt menti (`Main.cpp:167`, `kick_AppHome.cpp:354` visszaesés). `FolderSizeTests.cpp` a mappaméret-cache-t fedi, nem ezt.

**Hiányzik (host):** CPU-delta (két mintából, nulla-osztás, számláló-visszaugrás), `MemAvailable`-parser, mount-parser (a `/media/PenDrive`, `/sdcard`, szóközös címke `\040`), `statvfs`-wrapper hamis gyökérrel; Purge-szemantika (`ASoundCache`-teszt: cache-hivatkozás, aktív hivatkozás nem törlődik, újratöltés).

**Az MPC-n ellenőrzendő:**
- [ ] A dialógus megnyitható az összes képernyőről, beleértve a főképernyőt és a Menüt.
- [ ] CPU %: üresjáratban alacsony, lejátszáskor észrevehetően emelkedik, megállás után visszaesik.
- [ ] RAM: a `free`/`/proc/meminfo` `MemAvailable`-jével egyezik; nagy projekt betöltése után nő.
- [ ] A belső tár a `/data` (`df /data`) értékét mutatja; SD (`/sdcard`) és pendrive (`/media/PenDrive`) behúzáskor megjelenik, kihúzáskor eltűnik; nincs fagyás kihúzott/lassú USB-nél.
- [ ] Purge Unused/All: a felszabadított memória mérhető (`/proc/self/status` VmRSS), a lejátszás nem akad, a minták a következő használatkor visszatöltődnek, projekt-mentés után a fájlok épek.
- [ ] Bezárás: OK, X, kívülre érintés, hardveres gomb; a háttérben nem vált képernyőt.

## 7. TODO

- [ ] **SR-01** (P0, S) Inicializálatlan `sysSizeInfo` megszüntetése (alapértékek), a hívó a `status`-t ellenőrizze; nem-Linux platformokon a `ramInfo/sizeInfo/cpuInfo` mindig inicializál. — `kick_ui_systemResources.cpp:103-131`, `kick_Resorces.cpp:62-134`
- [ ] **SR-02** (P0, M) CPU-mérés különbségből (előző minta tárolása), `iowait`/`guest` helyes kezelése. — `kick_Resorces.cpp:26-46`
- [ ] **SR-03** (P0, M) MPC-s tárak: belső = `/data`, SD/USB a `/proc/mounts`-ból (a `kick_fileUtils.cpp:8-50` logika kiemelése közös segédbe), `statvfs()`, `popen("df")` törlése; nem létező tár rejtve. — `.h:79-81`, `kick_Resorces.cpp:62-100`, `kick_Hwpref.cpp:880-905`. ⟵ SR-20
- [ ] **SR-04** (P1, S) RAM: `MemAvailable` + Kick-folyamat RSS. — `kick_Resorces.cpp:111-134`
- [ ] **SR-05** (P1, M) Mintavétel háttérszálon (1 Hz, atomok), azonnali első mérés, villanásmentes láthatóság. — `kick_ui_systemResources.cpp:91-167`
- [ ] **SR-06** (P0, L) Valódi Purge Unused/All (`ASoundCache` szemantika, nem-RT, megerősítés, visszajelzés); `ASoundCache::purge` ciklus-hiba. ⟵ SR-07 — `kick_ui_systemResources.cpp:78-79`, `audiofile_manager.cpp:51-57`, `audio_sound.cpp:54-60`
- [ ] **SR-07** (P1, M) Engine-statisztika: gyorsítótárazott minták száma/mérete, „használt/nem használt" bontás; megjelenítés a dialógusban.
- [ ] **SR-08** (P1, S) Elérhetőség: Resources-gomb a felső sávon / menü-csempe. — `kick_AppTopbar.cpp`, `kick_AppMenuOptions.cpp`
- [ ] **SR-09** (P1, S) Hardveres bezárás (ENTER/Data Dial/MAIN) — az általános modális megoldás része. ⟵ HDR-02
- [x] **SR-10** (P2, M) Audio-terhelés (`getCpuUsage`), xrun-számláló a dialógusban.
  **R-7:** The dialog shows an AUDIO bar (callback load, JUCE `getCpuUsage`) with "dropouts: N" (device xrun count; -1 = not counted by the driver, shown without the number). Check on the MPC what the ALSA driver reports.
- [x] **SR-11** (P2, S) Kis szabad hely / magas memória figyelmeztetés a státuszsorban. ⟵ HDR-14
  **R-7:** Status bar standing text: "Low storage: N MB free" at >= 95 % used internal storage, "Low memory: N % used" at >= 92 % (`kick_sysinfo::resourceWarnings`, host-tested), checked every 5 s; priority audio fault > low storage > unsaved > low memory.
- [ ] **SR-12** (P2, S) Holt kód rendezése: `Screen::SystemResources`, `KickSysResources`, `onDialogClose` lambda, hibás modul-leírás; döntés: dialógus marad, enum törlendő. — `kick_IScreen.h:63`, `kick_ui_systemResources.h:30, 84-93`, `kick_modeHeader.cpp:95-97`
- [ ] **SR-13** (P2, S) Relatív elrendezés, nagyobb érintési célok, a ferde elválasztóvonal javítása. — `kick_ui_systemResources.cpp:134-156`
- [ ] **SR-20** (P0, M) Host-tesztek: CPU-delta, `MemAvailable`, mount-parser, `statvfs`-wrapper (JUCE-mentes fejléc + `Kick/tests/platform/*Tests.cpp`).
- [x] **SR-21** (P1, M) `ASoundCache` Purge host-teszt. ⟵ SR-06
  **R-7:** SoundCacheTests links the REAL ASoundCache (module audio_file_manager + juce_audio_formats): stats, unused = only the cache holds it, Purge Unused drops exactly those, a held sound keeps its data, reload after the purge, clear(); ctest `kick-sound-cache`.
- [ ] **SR-22** (P1, S) Smoke: a `Main.cpp:167` tábla javítása (a dialógus valódi képernyőképe), AppSmoke: megnyitás/zárás. ⟵ SR-12
- [ ] **SR-23** (P0, M) MPC-s hardveres próba a 6. pont listájával. ⟵ SR-01…SR-06
