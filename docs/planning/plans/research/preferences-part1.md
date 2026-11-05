# Preferences / Settings képernyő - 1. rész (oldalak 1-4), kódból

Forrás: `Kick/modules/kick_ui_settings/*`, `Kick/modules/hardware/source/kick_Hwpref.*`, `Kick/modules/engine/project/kick_settings.*`, `kick_xml_manager.*`. Útvonalak a `Kick/modules/` alatt, ha nincs más jelezve. Csak olvasás; futtatás nem történt (minden állítás statikus kódolvasás; ahol hardver/rootfs kell: NEM ELLENŐRIZHETŐ KÓDBÓL).

## 0. Oldalak regisztrálási sorrendje (`kick_ui_settings/kick_ui_settings.cpp:88-96`, címkék `kick_ui_settings.h:81`)
0 Info, 1 Project Defaults, 2 Hardware, (SettingsAudio KIKOMMENTEZVE :93, a fájl üres stub: `source/kick_ui_settings_audio.cpp:33-38`), 3 MIDI (`SettingsMidiSync`), 4 CV, 5 Ethernet, 6 WiFi. Ebből 1-4. oldal = Info, Project Defaults, Hardware, MIDI.
Közös: az oldalt 1 Hz-es timer frissíti (`refresh()`, `startTimerHz(1)` a `visibilityChanged`-ben, kick_ui_settings.cpp:188) - nyitáskor az első `refresh()` csak ~1 s után fut, addig a konstruktor-alapértékek látszanak (pl. Bit Depth "24", Threshold "9" a modell "16"/"4" helyett). A Settings képernyő a `kick_AppHome.cpp:283-291` szálkészlet-jobjában, indulás után jön létre (egyszer), a konstruktorok mellékhatásai (autosave, fényerő, időzóna, pad-beállítás) ekkor futnak. Perzisztencia: `preferences.settingsprj` (`kick_settings.h:18-19`), útvonal Linuxon `/data/prefs` ha létezik, különben `/home/Kick/prefs` (`common/source/kick_paths.cpp` MTPrefs ág), `KICK_DATA_ROOT` felülírja; a prefs-fájl mindig autosave-el ~1,5 s késéssel (`engine/project/kick_xml_manager.cpp:181-184`, `preferences ||`).

## 1. Info (`source/kick_ui_settings_info.cpp`)
| Vezérlő | Mit csinál / hova | Hatás | Állapot |
|---|---|---|---|
| Logo (`KickImage "LOGO"`) | téma-kép `Sim KICK logo.png` (`Kick/assets/Default/theme.xml:679`) | csak kép | KÉSZ |
| Serial Number | `HwPref::getDevSerial()` olvassa `/sys/devices/soc0/serial_number` (Hwpref.cpp:251), különben "000000000" | nem `hardwareEnabled`-függő; csak a konstruktorban (:78); `refresh()` üres (:101) | RÉSZLEGES (MPC-n a fájl létezése NEM ELLENŐRIZHETŐ) |
| Firmware Version | `/etc/version.txt` első sora, regex `^[a-zA-Z]+ \d+\.\d+\.\d+$` (Hwpref.cpp:262-300), különben "0.0.0" | csak kijelzés | RÉSZLEGES (Behringer-formátum; MPC-n valószínűleg "0.0.0") |
| Control Version | `HwPref::surfaceVersion`, csak `KickHome::handleCommand` állítja `EN_SYS_GET_SW_VER`-re (`kick_ui/dialogs/kick_AppHome.cpp:455-458`) | az `apps/`-ban nincs küldő -> üres; az oldal sosem frissül, ha később érkezne | STUB-ÜRES |
| Build Date | `#if defined(TIMESTAMP) && (TIMESTAMP > 0)` (:93) | a `TIMESTAMP` csak CMake `option` (`Kick/CMakeLists.txt:30`), C-makró sehol (nincs `-DTIMESTAMP`), `timestamp.h`-t a modul nem include-olja -> az `else` ág: `__DATE__ :: __TIME__` (az adott fordítási egységé, git hash/ág NINCS) | BUGOS GYANÚ: a WP-G G-053 "git hash látszik" állítása valószínűleg téves |
| Factory Reset | megerősítő popup -> `HwPref::factoryReset()` = `/bin/rm -rf /home/Kick/projects* /home/Kick/prefs` (Hwpref.cpp:29,136-139) | `hardwareEnabled()`-hez kötött: a telepített MPC-n `KICK_NO_HARDWARE=1` (build-recipe.md:39, pendrive-install.md:118, tools/tkgl_bootstrap:286) -> **némán nem csinál semmit**; ha engedélyezett: MPC-n a valódi adat `/data/projects`,`/data/prefs`, tehát nem azt törli; nincs újraindítás/újratöltés, nincs visszajelzés | MPC-n HATÁSTALAN; BUGOS GYANÚ (útvonal) |
| Firmware Update | `sigScreenOpen(SWUpdateView)` (:60) | gombnyomás KÉSZ; a frissítés `/usr/bin/swupdate -i /tmp/fwupdate/Kick.swu -v &` (hardware/source/kick_FirmwareUpdate.cpp:444-445), `reboot -nf` (kick_ui/swupdate/kick_SWUpdateScreen.cpp:126), `mount -o loop -t ext4` (Hwpref.cpp:228); mind `hardwareEnabled()`-hez kötött (kick_FirmwareUpdate.cpp:431) | MPC-n HATÁSTALAN (Behringer-image) |
| Restore Defaults | megerősítés -> `getSettings().makeDefaults()` (:74) = `projects/hardware/eth/cv.makeDefaults()` (`engine/project/kick_settings.cpp:61-67`), `removeAllProperties` (`mt_audio_ids.h:336`) | nem érinti: midiControl, wifi, metronome, browser, cvclib; nem alkalmazza újra a futó állapotot (fényerő, időzóna, pad-küszöbök, a statikus autosave-flag/idő csak konstruktorban / vezérlő-váltáskor íródnak); törli a `projects.currentProject` és `isAutoLoad` property-ket is (ugyanaz a fa) | RÉSZLEGES |
Popup: `ConfirmationPopup{..., false, false}` (a saját Yes/No rejtve, a `showOptions` Yes/No-t ad, utils.cpp:38-48); a callback minden kimenetnél `delete p` (a tulajdonos NEM ELLENŐRIZHETŐ - dupla felszabadítás kockázat).

## 2. Project Defaults (`source/kick_ui_settings_defaults.cpp`) - modell: `KickProjectsDefault` (`kick_settings.cpp:87-114`, XML-csomópont PROJECTDEFAULTS)
Validáció: csak widget-tartomány (jlimit); hibakezelés nincs; visszaállítás: csak Info > Restore Defaults.
| Vezérlő | Property (alapérték) | Fogyasztó (grep az egész Kick/-ben) | Állapot |
|---|---|---|---|
| ON LAUNCH {Selector, New Project, Last Used} (:63) | `Onlaunch` ("Selector") | NINCS (csak deklaráció + attach). Indulás mindig RecentProject (`kick_AppHome.cpp:262,281`); auto-load külön kapcsoló a RecentProject fejlécen (`recent_project/kick_RecentProject.cpp:446`) | STUB (nincs hatás) |
| RECENT PROJECTS 1..5 (:66) | `MaxNumOfRecentProject` (5) | RecentProject.cpp:80,432; `kick_settings.cpp` `checkAlreadyLoadOrNot`/`refreshMaxNumOfRecentProj` | KÉSZ |
| AUTO SAVE enabled/disabled (:74) | `Autosave` (false) + `KickXML::setAutoSave` (STATIKUS, `kick_xml_manager.h:59,92-93`) | projekt-autosave (xml_manager.cpp:182-184) | KÉSZ, de: a konstruktor (:39-40) a mentett (alapból false) értékre állítja, felülírva a `KickSettings` ctor `setAutoSave(true)`-t (kick_settings.cpp:58) -> alapból a projekt-autosave KI van indulás után (viselkedés-megjegyzés) |
| SAVE AFTER {15 Sec,30 Sec,1 min,2 min,5 min} (:78-84) | `SaveAfter` ("30 Sec") + `setAutoSaveTime(ms)` (statikus) | autosave timer | KÉSZ (induláskor az aszinkron `setSelected(sendNotification)` alkalmazza, kick_listbox.cpp:551-565) |
| TEMPO 20..240 (:86) | `TimeTempo` (120) | csak a StepSequencer tartalék-kijelzése (`kick_stepSequencer.cpp:1093`); új projekt létrehozása NEM olvassa | STUB/HATÁSTALAN |
| BARS 1..100 (:90) | `TimeBars` (4) | NINCS | STUB |
| NUMERATOR 1..16 / DENOMINATOR {4,8,16,32} (:94-99) | `TimeNumerator`/`TimeDenominator` ("4"/"4") | csak Looper-felvevő (`looper/kick_LoopRecorder.cpp:260,268,278`) | RÉSZLEGES (projekt/szekvencia ütemmutatóra nem hat) |
| ROOT NOTE / SCALE (:101-107) | `rootNote`/`scale` (0/0) | NINCS (a `mt_audio_interface_types.cpp:385-386` más, projekt-szintű perform-állapot) | STUB |
| SEQUENCES 1..64 / TRACKS/SEQ 1..64 (:109-114) | `ProjectSequance`/`ProjectTrackSeq` (modell 1/1, UI-alap 4/4) | NINCS | STUB |
| BIT Depth {16,24} (:70) | `recordBitDeapth` ("16"; UI-alap "24") | NINCS | STUB |
| Naming {DISABLE, TIME STAMP, CUSTOM} + név-mező (:115-149) | `DefaultProjectStateName`, `DefaultProjectState` (alap 5, értékek 0-2; nincs fogyasztó), `DefaultProjectName` | a prefix az "új projekt" billentyűzet kezdőszövege (`kick_RecentProject.cpp:92`, `kick_fileBrowser.cpp:172`) | RÉSZLEGES; BUGOS GYANÚ: TIME STAMP egyszer fagyasztja az időbélyeget (:126-132, nem generál újat); CUSTOM minden kiválasztáskor "KICK_"-ra írja (:139); újraindítás után a `refresh()` nem állítja vissza a szerkeszthetőséget (:152-176, `KickLabel` alapból nem szerkeszthető, kick_label.cpp:54); a név-mező nincs szűrve (útvonal-karakterek) |
Desktop vs MPC: azonos (a modell nem platformfüggő). A TIME STAMP `juce::Time` helyi idő -> Linuxon a Hardware oldal által beállított `TZ` hat rá (lásd 3.).

## 3. Hardware (`source/kick_ui_settings_hardware.cpp`) - modell `KickHardware` (`kick_settings.cpp:229-243`)
| Vezérlő | Property (alapérték) | Végrehajtás | Állapot |
|---|---|---|---|
| Thresold(sic)/Sensitivity 1..16, Curve 1..4 (:67-78) | `padThreshold`/`padSensitivity`/`padCurve` ("4"/"4"/"4"; UI-alap "9"/"9"/"2") | DCL `EN_PAD_SETTINGS [16,thr,sens,curve]` (`SendPadSettings` :139-147; ugyanez projekt-betöltéskor `kick_leds/kick_leds.cpp:346,355-363`). Behringer: a DCL-firmware kezeli (nincs a repóban: NEM ELLENŐRIZHETŐ). MPC híd: `onDclCommand` csak `EN_PAD_SET_AFTER_TOUCH`-ot és `EN_PAD_SET_COLOUR_STATE`-et dolgoz fel (`apps/kick/kick_hardware_bridge.h:137-160`), a többit eldobja | MPC-n HATÁSTALAN |
| VELOCITY / AFTERTOUCH 1..127 (:80-87) | `padVelocity`/`afterTouch` (127/127) | élő pad-mérő (`handleCommand` :203-221 `EN_PAD_STATE`), de a csúszka kézzel is húzható; minden érték a perzisztens fába íródik (:199-200), `refresh()` visszatölti; nincs fogyasztó | RÉSZLEGES (monitor; felesleges perzisztálás). MPC-n az aftertouch mindig 0: `kSendPadAftertouchPackets=false` (bridge.h:1773); az oldal láthatóság-váltásra `EN_PAD_SET_AFTER_TOUCH` ki/be (:224-234), a híd csak a flaget tárolja (:143-146) |
| BRIGHTNESS 0..100 (:93-96) | `screenBrightness` (100) | `HwPref::setBrightness`: min 20-ra vágva, `echo N > /sys/devices/platform/backlight/backlight/backlight/brightness` `std::system`-mel (Hwpref.cpp:841-852), csúszka-léptenként, UI-szálon; ctor-ban is (:132-133) | MPC-n HATÁSTALAN (`hardwareEnabled()` false; az útvonal Behringer-lemezkép - MPC-n NEM ELLENŐRIZHETŐ). A WP-G G-105 "pad fényerő" címkéje téves: képernyő-háttérfény |
| TAP Tempo LED enabled (:89) | `tapTempoLed` (false) | NINCS fogyasztó (sem `KickLEDs`, sem híd) | STUB |
| NO OF TAPS {1,2,4} (:127) | `noOftaps` ("2"; UI-alap "4") | NINCS; a `KickTempo` fix 3 koppintást használ (`kick_ui_controls/source/kick_Tempo.h:74`) | STUB |
| Day / "Minute"(sic: Month) / Year; Hour / Minute (:98-121) | nincs perzisztens; rendszeridő | `HwPref::setDate/setTime` minden mező-váltáskor: a parancs a puszta "2025-5-15" / "10:30:00" string, `date -s` NÉLKÜL (Hwpref.cpp:941-944, 959-962) -> futtatva hibás; `hardwareEnabled()`-hez kötött (MPC-n nem fut). Az oldal `refresh()` 1 Hz-en `updateDateTime()`-tal visszaírja a rendszeridőt (:153,181-191) -> a szerkesztés ≤1 s alatt visszaugrik. Az év-handler a getDaysInMonth(év, év) hívással mindig 31 napot ad (:110, helyes: :104); a hónap-címke "Minute" (hardware.h:56); az `setRange` nem vágja le a már beállított napot (`kick_number.h:150-157`); nincs hwclock-szinkron | BUGOS (nem működik sehol) |
| Time zone (350 elemű lista, `common/source/kick_Constants.h:110`) (:122-125) | `timeZone` ("Asia/Kolkata") | `timedatectl set-timezone <tz>` (Hwpref.cpp:828; string-összefűzés `system()`-be, a perzisztált értékből is: injekció-kockázat, ctor :134-135). Ezen felül, `getDateTime`-ban `setenv("TZ", tz)`+`tzset()` MINDEN frissítéskor Linuxon, NEM hardverfüggő (Hwpref.cpp:796-800) -> a folyamat helyi ideje (kijelzés, `juce::Time`) Asia/Kolkata alapból; zoneinfo megléte a rootfs-ben NEM ELLENŐRIZHETŐ | RÉSZLEGES; alapérték Behringer-régió |
Szálbiztonság: minden handler üzenetszálon; `AtomicString::operator()` nem atomi `juce::String`-re ad referenciát (`common/source/kick_atomics.h:236,250`), `localtime`/`setenv` nem szálbiztos.

## 4. MIDI (`source/kick_ui_settings_midi_sync.cpp`) - modell `KickMidiControl` (`kick_settings.cpp:248-262`)
| Vezérlő | Property | Hatás | Állapot |
|---|---|---|---|
| Input port lista (`getMidiDeviceNames(true)`, :65,68) | `MidiInputName` ("") | NINCS fogyasztó a Kick-ben (a portokat a `MTMidiDeviceManager` 1 Hz-es listafrissítése kezeli, `engine/audio_midi/source/midi_device_manager.cpp:60`, a beállítástól függetlenül) | STUB; BUGOS GYANÚ: `refresh()` 1 Hz-en `setListOptions(..., aktuális index)` -> `sendNotificationAsync` szinkron hívja az `onSelectionChanged`-et (`kick_listbox.cpp:102-114,155`): a mentett port-név felülíródik az épp kijelölttel (üres listánál ""), mielőtt a :101-es visszaállítás lefutna |
| Output port lista (:66,72) | `MidiOutputName` | NINCS; `refresh()` nem állítja vissza (csak az inputot, :101) | STUB; ugyanaz a felülírás |
| Input/Output TRACK, CONTROL kapcsolók (:47-61; mindhárom felirat azonos) | `midiInput/OutputTrack/ControlState` (false) | NINCS fogyasztó | STUB |
Nincs sync/clock/MMC/MTC vezérlő. `KickMidiControl::isValid()` az `IDs::MIDICONTROL`-t ellenőrzi, a `setup` az `IDs::PREFMIDICONTROL`-t használja (kick_settings.cpp:44,261) -> mindig false (latens). A MIDI-vezérlő térkép ugyanazt a fát használja (`modes/midicontrol/kick_MidiControlTabs.cpp:264,1634`).

## 5. Külső parancsok/fájlok - ellenőrzendő az MPC Key 61 rootfs-ben (BusyBox/Yocto, build-recipe.md:36)
Parancsok (mind `std::system`, mind `hardwareEnabled()`-hez kötve - a telepített MPC-n `KICK_NO_HARDWARE=1` => egyik sem fut):
- `/bin/rm -rf /home/Kick/projects* /home/Kick/prefs` (Hwpref.cpp:29)
- `timedatectl set-timezone <tz>` (:828) - systemd-s Behringer-image-ben van (`Linux/buildroot/configs/KTKick_defconfig:5`); MPC-n NEM ELLENŐRIZHETŐ
- `echo N > /sys/devices/platform/backlight/backlight/backlight/brightness` (:849)
- `<yyyy-m-d>` és `<h:m:00>` mint parancs (:941,959; szándék: `date -s`)
- (Info/Ethernet/Update, hivatkozva:) `/sbin/poweroff` (:30), `/usr/sbin/udhcpc -i eth0 &`, `killall udhcpc` (:125,132), `/usr/bin/swupdate -i /tmp/fwupdate/Kick.swu -v &` (kick_FirmwareUpdate.cpp:444), `reboot -nf` (kick_SWUpdateScreen.cpp:126), `mount -o loop -t ext4` (Hwpref.cpp:228)
Fájlok (nincs `hardwareEnabled` védelem, Linuxon mindig olvassa): `/sys/devices/soc0/serial_number` (:251), `/etc/version.txt` (:275), `/proc/mounts` (`isStorageconnected`), zoneinfo a `TZ`-hez. Írt: `/data/prefs/preferences.settingsprj` (pendrive-install.md:47).
Ellenőrző parancs az eszközön: `ls /sys/devices/soc0/serial_number /etc/version.txt /sys/devices/platform/backlight /usr/share/zoneinfo/Asia/Kolkata; which timedatectl date swupdate udhcpc poweroff`.

## 6. Desktop vs MPC
- macOS/desktop: `hardwareEnabled()` false (KICK_ENABLE_HARDWARE alapból OFF, `hardware/source/kick_HardwareMode.h`) -> minden shell-parancs no-op; nem-Linuxon a `TZ` sem állítódik; serial "000000000", fw "0.0.0".
- Linux desktop: shell no-op, DE a `TZ`-állítás és a sysfs-fájlolvasás él.
- MPC: build `KICK_ENABLE_HARDWARE=ON` (tools/build_arm.sh:49), futtatás `KICK_NO_HARDWARE=1` -> a screen-inventory.md:294 "executed" állítása a telepített konfigurációra téves (no-op). Pad-küszöb/érzékenység/görbe: a híd eldobja. Aftertouch: letiltva.

## 7. Hiányok az Akai MPC Preferences-hez képest (wp-g.md G-xxx sorok)
- Info: Legal Info (G-052), Version Info Shift-nézet (G-053 ELTÉR), Reset után projekt-mentés felkínálása (G-048).
- Hardware: Pad Brightness valójában képernyő-fényerő (G-105/113), Dimming (G-112), 4 fokozatú fényerő (G-113), Curve A-D helyett 1-4 (G-109), Bank Button Press (G-117), Shift+Q-Link (G-118), Global Pitch Bend + Range (G-119/120), alapértelmezett Touch Strip mód (touch-strip-modes.md:8; MORNING_DECISIONS.md:122,132).
- Sequencer csoport teljesen hiányzik (G-126..136), benne Rec Arm / Solo Behavior (G-128/129; MORNING_DECISIONS.md:77).
- Project Defaults: Default Mode/Q-Link/Loop/Slice/Filter/Plugin/Monitor/Track Color/User Level (G-138..153), Project Load/Save: New Project Behavior, User Auto Load File, Template File, Temp File (G-154..158).
- MIDI/Sync: Global kapcsoló, Sync, MMC, MTC, Program Change, All Notes Off szűrő, port-átnevezés, reset, Enable MIDI Ports When Discovered, Control Mode Output (G-090..104).
- Audio-oldal (G-075..089), Bluetooth (G-073/074), General/Splice más felmérő területe.
- A WP-G "OK" sorai a hatást nem vizsgálták: G-107/108 (pad küszöb/érzékenység), G-122..124 (dátum/idő/zóna), G-137/141/143/144 (tempó, ütem, ütemmutató) kódból HATÁSTALAN/HIBÁS - javítandó a WP-G-ben.

## 8. Ismert problémák a dokumentumokban
screen-inventory.md:166-173,307-308 (setDate/setTime, év-handler, MIDI output nem áll vissza, visibilityChanged log); planning/plans/04-screen-audit.md:95-96 (pad-érzékenység hatástalan MPC-n, Behringer-parancsok); known-issues-and-roadmap.md:82 (Menü/Preferences hiányok), :88 (Hardware alap strip-mód), :101 (aftertouch kikapcsolva); todo.md §5 T10 (pad poly-AT a Hardware oldal flagjétől függ, később letiltva), T27/§6 #6 (EN_PREF Shift+METRONOME), T19 (CV/I2C); §7 (screenshot-segéd `HomePreferencesView`); §8 nem érinti. HW_TEST_REPORT: Preferences-oldal hardveres mérését nem találtam.
Új (jelen felmérés, nincs dokumentálva): Info Build Date/Control Version, Factory Reset MPC-n, Project Defaults fogyasztó nélküli mezői, MIDI port felülírás, TZ Asia/Kolkata folyamatszinten, Restore Defaults részlegessége, projekt-autosave alapból KI.

## 9. Tesztek
Nincs teszt, ami a Preferences oldalakat, a `HwPref`-et vagy a `KickSettings`-et fedné (grep `Kick/tests`, `Kick/clap_host/tests`: nulla találat). Közvetett: `clap_host/tests/PersistenceTests.cpp:31-167` a `KickXML` autosave-et teszteli (általános); `tests/platform/AppSmoke.py` `KICK_NO_HARDWARE=1`-gyel indítja az appot (a Settings létrejön, tartalmát nem ellenőrzi); `Main.cpp:158` smoke-screenshot `HomePreferencesView`. Javasolt: HwPref parancs-összeállítás egységteszt, makeDefaults lefedettség, MIDI refresh felülírás.
