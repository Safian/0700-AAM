# Preferences / Settings - 2. fel (CV, Ethernet, WiFi + rendszer-reteg)

Datum: 2026-11-04, worktree `kick-plans`. Csak kodolvasas, nem futtattam semmit. Jeloles: KESZ / RESZLEGES / STUB / BUG? (konkret ok). `M/` = `Kick/modules/`. A "NEM ELLENORIZHETO" azt jelenti: kodbol nem latszik (pl. az MPC rootfs tartalma).

## 0. Oldalsorrend (`kick_ui_settings.cpp:88-96`, `kick_ui_settings.h:81`)
1 Info, 2 Project Defaults, 3 Hardware, (SettingsAudio kikommentezve :93, ures stub), 4 MIDI, 5 CV, 6 Ethernet, 7 WiFi.
A Settings a HomeApp indulaskor lazan, thread-poolbol hozza letre (`kick_AppHome.cpp:291`), vagyis MINDEN oldal konstruktora lefut bootkor.
Az aktualis oldal `refresh()`-e 1 Hz-en fut a message threaden (`kick_ui_settings.cpp:150-152,188`), az oldalvaltas nem hiv azonnali refresht.

## 1. Kritikus, az egesz retegre ervenyes
1. **MPC-n a board-parancsok mind no-opok.** `hardwareEnabled()` (`M/hardware/source/kick_HardwareMode.h:5-12`) false, ha `KICK_NO_HARDWARE=1`; az MPC bootstrap ezt exportalja (`docs/hardware/key61/tools/tkgl_bootstrap:286`, BUILD_RECIPE:39, pendrive-install:118). A screen-inventory 5. szakasza ("executed, KICK_ENABLE_HARDWARE=ON") ezert felrevezeto: a flag csak build-ido, futasidoben kikapcsolt. Hatas: brightness, datum/ido, timezone, eth up/down, DHCP, setIP, factory reset, poweroff, Wi-Fi enable/connect, swupdate - mind semmit nem csinal az MPC-n a szallitott inditassal. Nincs jelzes a UI-n (a vezerlok "mukodnek", a hatas hianyzik).
2. **Prefs-ut vs factory reset.** Linuxon a prefs `/data/prefs`, ha letezik, kulonben `/home/Kick/prefs` (`M/common/source/kick_paths.cpp:~136-150`; MPC-n `/data/prefs` van, pendrive-install:47). A factory reset parancs `rm -rf /home/Kick/projects* /home/Kick/prefs` (`kick_Hwpref.cpp:29`) tehat az MPC-n sem a prefs-et, sem a projekteket nem torli (es no-op is, ld. 1.). A Wi-Fi fajlok (`/home/Kick/wifi_credentials.txt`, `wpa_supplicant.conf`, `kick_WifiFirmware.cpp:31-32`) a resetet akkor is tulelik.
3. **Eth IP-ertek ujsor-hiba.** `getIP` az `ip -4 addr show ... | awk` kimenetet nyersen adja (`kick_Hwpref.cpp:759-764`), a vegen `\n` -nel. A `refresh()` ezt a prefsbe is menti (`etnernet.cpp:129`), majd a bootkori `update()` ebbol epiti: `ifconfig eth0 <ip>\n netmask <mask>` (`kick_Hwpref.cpp:662`) - az `system()` shell a `netmask ...` sort kulon parancskent futtatja (nem letezo parancs), tehat a netmask nem ervenyesul. Ugyanez az ujsor-mechanizmus parancs-injekcio is lehet (ld. 6.).

## 2. CV oldal (5.) - `kick_ui_settings_cv.cpp`, `M/hardware/source/kick_I2c.cpp`
| Vezerlo | Allit / perzisztal | Hatas | Allapot |
|---|---|---|---|
| CV-1..8 Start Note (8 lista, 128 jegyzetnev, alap 0) :41-48,57-88 | `settings.cv.setNoteStart` (`CV<n>` property a `PREFCV` ala, `kick_settings.cpp:284-300`) + `CV::setChannelNoteMap` | a CV::setVoltage kivonja a start note-ot (`kick_I2c.cpp:163-164`) | BUG? (lent) |
| CV CALIBRATION gomb :89 | `sigScreenOpen(CVClib)` | CVClib kepernyo (4 sor: -, +, Reset, offset) :193-292 | RESZLEGES |
| CVClib -/+/Reset :248-277 | `settings.cvclib.setOffset` (`CLIB<i>`, int) + `CV::calibrateCV` (DAC azonnali iras, minden csatornara `writeAllChannelVoltage`) | elo hangolas | BUG? |
| CVClib APPLY :279-292 | `CV::applyCalibration` -> `claibValue[61]` tabla | csak ez toltja a notehoz tartozo DAC-tablat | RESZLEGES |

Hibagyanuk (kodbol, hardveren nem mert - TODO T19/H18: CV nincs hardveren tesztelve):
- **Bootkor nincs kalibracio-betoltes.** `applyCalibration` egyetlen hivoja az APPLY gomb (`rg`: nincs mas hivo). A `claibValue[61]` static nullara inicializalt (`kick_I2c.h:85`, `CV() {}`), ezert APPLY nelkul `setVoltage` 0 DAC-erteket ir minden jegyzetre (`kick_I2c.cpp:168`). A mentett `CLIB*` offsetek bootkor nem ervenyesulnek. (Csak a step-feszultseg - velocity/modwheel/gate - fuggetlen: `stepToDac`, tesztelt `CvScaleTests.cpp`.)
- **Start note bootkor szinten nem erkezik meg:** `setChannelNoteMap` csak az oldal `refresh()`-eben fut (:94-127), tehat amig a CV oldalt nem nyitottak meg, minden start note 0.
- **Port-indexeles eltolodas:** `setChannelNoteMap` a `toChannelmap(i)` (CVPortIndex {-1,4,5,6,7,0,1,2,3}, `kick_I2c.h:83`) kulcsra ir (0. index -> 255), de `getChannelNoteMap(ch)` a nyers csatornaszamra olvas (`kick_I2c.cpp:136,163,298-301`). A program portja 1-alapu (`mt_audio_cv_program.h:39`, alap 1), `setVoltage` pedig `isPositiveAndBelow(ch, 8)`-ot ellenoriz -> a 8. CV port nem cimezheto, a UI "CV-7" start note a program 1. portjat adja, stb. Plusz a program sajat `CVStartNote`-ja is levonodik (`mt_audio_cv_program.cpp:58`) -> dupla offset.
- **calibrateCV index:** csak 1..4-et fogad (`kick_I2c.cpp:244`), a UI sorindexe 0..3 (:202-204, a cimke "0 V".."3 V") -> a 0. sor sosem hat, a 4. feszultseg (4 V) nem allithato; az `applyCalibration` viszont `calib[i+1]`-be tesz (a cimke es a tablaindex 1 V-tal eltol).
- **Negativ offset:** `static_cast<uint16_t>(double<0)` (`:287`) nem definialt viselkedes; az `applyCalibration` int-aritmetikaja veletlenul visszaadja. Nincs tartomany-korlat a -/+ -ra.
- **cv5..cv7 vs cv0..cv4:** az utolsok `index`-et mentenek, az elsok `getNoteNumber(str)`-t (:57-88); sharp nevekkel azonos, de `_preferFlatNames` eseten `getNoteNumber("Db")` = -1 (`kick_scales.h:309`, `findIndex`) -> -1 start note mentodik.
- **Szal:** `channelNoteMap` `std::map<uint8_t,int>`; a UI szal 1 Hz-en ir (`:98...`), a motor szal `operator[]`-vel olvas/beszur (`getChannelNoteMap`) -> adatverseny. Minden refresh `sendNotification`-nel is kuld (felesleges ismetlodes).
- `KickCVClib::isValid` a `PREFCV`-t nezi, nem `PREFCVCLIB`-et (`kick_settings.cpp:307`) - kicsi, hatastalan.
- A `settings.cv.*` / `cvclib.*` fogyasztoja kizarolag ez az oldal (rg: nincs mas olvaso). I2C: `/dev/i2c-3`, cim 0x0c (`kick_I2c.cpp:36`), a `hardwareEnabled()` kapu mogott (MPC-n nem nyilik meg).

## 3. Ethernet oldal (6.) - `kick_ui_settings_etnernet.cpp`
| Vezerlo | Perzisztencia | Hatas | Allapot |
|---|---|---|---|
| ETHERNET ENABLED/DISABLED :48,80-87 | `eth.enabled` (`status`) | `ifconfig eth0 up/down` (`kick_Hwpref.cpp:617-634`) | BUG?: a `refresh()` 1 s-onkent felulirja a `/sys/class/net/eth0/operstate=="up"` ertekkel (:116-119), ami kabel nelkul "down" -> a kapcsolo visszaugrik, a mentett ertek is felulirodik |
| MODE MANUAL/AUTOMATIC :52,89-104 | `eth.mode` ("MANUAL" alap, `kick_settings.cpp:271`) | `udhcpc -i eth0 &` / `killall udhcpc` | **BUG (megerositve):** a kapcsolo ON-szoveg = "MANUAL" (`kick_buttons.cpp:95-113`, `etnernet.h:46`); `useDhcp = toggleState` -> ON (MANUAL) eseten `startDhcp` + mezok tiltva, mode="MANUAL" (:91-98) - megfordtott. A kapcsolo kezdoallapota sosem a mentett modbol jon (`initializeInterfaceState` nem allitja) |
| IP / SUBNET / GATEWAY :56-72 | `eth.ipAddress/subnetMask/gateway` | `setIP` -> `ifconfig eth0 ip netmask mask` (`:662`) | RESZLEGES |
| DNS :73-76,127 | SEMMI | SEMMI (`update()` nem hasznalja; `refresh` fixen "0.0.0.0") | **STUB (megerositve)** |

- **Gateway nem ervenyesul:** `setIP(interface, ip, subnet, gateway)` a `gateway` parametert sosem hasznalja (`kick_Hwpref.cpp:658-668`) - nincs `route add default gw`.
- **Gateway mezo:** az Ethernet oldal helyes (`gateway.setText(info.gateway)` :126). A leltar "gateway a subnetet mutatja" allitasa a WiFi oldalra igaz (ld. 4.).
- **Refresh felulirja a kezi szerkesztest:** 1 Hz-en az elo ertekekkel (:123-131) es a prefs-et is atirja a lekerdezett (nem a kert) ertekekre -> gateway mentve "0.0.0.0" lesz (mert nincs beallitva), kezi mod konfig nem marad meg.
- Bootkor (`initializeInterfaceState`, :134-152) minden kezi/alap konfigra lefut `ifconfig eth0 192.168.0.1 netmask 255.255.255.0` + `killall udhcpc` (alap IP `kick_settings.cpp:272-274`) - Behringer boardon is felulirna a halozatot.
- Validacio: az `IPv4Editor` 0..255 szamokat enged (`kick_IPv4Editor.cpp`, `jlimit`), nincs maszk/gateway-konzisztencia ellenorzes. `KickIPv4::setIPAddress` hibas (csak [0]-t masol, :`kick_IPv4Editor.cpp` setIPAddress), a szerkeszto mindig 0.0.0.0-rol indul (az `editorValues` sosem szinkron a kijelzett szoveggel).
- `getIP` 3 blokkolo process-t indit (`ip addr`, `ifconfig eth0`, `ip route show`, `popen`) a message threaden masodpercenkent; `getSubnetMask` (ioctl) es `getGateway` (popen) NINCS `hardwareEnabled()` kapu mogott, az `ip addr` igen -> MPC-n az IP mezo ures, a maszk/gateway valos.
- Fuggo parancsok: `ifconfig` (up/down, setIP, getIP), `ip` (addr, route), `awk`, `udhcpc`, `killall`, `ethtool` (csak `isEthernetCableConnected`, nem hasznalt), `/sys/class/net/eth0/operstate`. Interfesz-nev fixen `eth0`.

## 4. WiFi oldal (7.) - `kick_ui_settings_wifi.cpp`, `M/hardware/source/kick_WifiFirmware.cpp`
- **Gateway = subnet (megerositve):** `gateway.setText(info.subnet, ...)` `wifi.cpp:104`. BUG.
- **ENABLED kapcsolo csak a prefsbe ir** (:51-53); `handleEnabledClick()` (:86-95, `ip link set wlan0 up/down`) sehonnan nincs hivva; `initializeInterfaceState()` (:126, auto-connect) sem. A WP-G G-062 "OK" itelete cafolhato. A kapcsolo nem tukrozi a valos allapotot; bootkor nincs ujracsatlakozas.
- **SCAN :55-62:** `iwlist wlan0 scan` (`ChildProcess`, `kick_WifiFirmware.cpp:97-107`) a UI szalon, blokkol (<=10 s); csak ESSID+Frequency parse, rejtett SSID kimarad; a lista-frissito kod `#if 0` (:107-123).
- **SSID lista -> `settings.wifi.ssid` (:68); jelszo :64-67 -> `settings.wifi.pwd`** (`wifipasswd`, SZOVEGESEN a `preferences.settingsprj`-ben, `kick_settings.cpp:378-384`); a mezo nem maszkolt (nincs "Show"/maszk, WP-G G-065).
- **CONNECT :73-83 / `connectToWiFi` (`kick_WifiFirmware.cpp:169-200`):** `killall -9 wpa_supplicant dhclient`, `sleep(1)`, `wpa_supplicant -B -i wlan0 -c /home/Kick/wpa_supplicant.conf`, `sleep(2)`, `dhclient wlan0` - mind a UI szalon (>=3 s fagyas). A visszateres `result.isEmpty()` (az utolso parancs kimenete), nem valos allapot; a kapcsolo ("connect"/"disconnected") nem frissul a kapcsolat valos allapotabol.
- **Ismert jelszo felulirja az ujat:** ha az SSID mar szerepel a fajlban, a tarolt jelszot hasznalja, az ujonnan beirtat eldobja (:177-183) -> elgepelt jelszo nem javithato; nincs Forget.
- **Titkok / injekcio:** `wifi_credentials.txt` (`ssid:password`, append, nincs jogosultsag-beallitas, SSID-ben ':' eltori a parse-t), `wpa_supplicant.conf` (`replaceWithText`, mindig 1 halozat) - az SSID/jelszo idezet nelkul-escape nelkul kerul `ssid="..."` / `psk="..."` ala (`:220-228`) -> `"` + ujsor bevitelevel extra `network={}` blokk injektalhato. Csak WPA-PSK (`key_mgmt=WPA-PSK`): nyitott halozat nem megy, ures/<8 karakteres psk elbukik. A `ChildProcess` nem shell, ezert itt nincs shell-injekcio.
- `mt::WifiManager().getWifiInterface()` minden hivasnal ideiglenes objektumon at (:81-83, 91, 130) - a statikus pointer sosem szabadul (szivargo singleton), nullellenorzes nelkul.
- `desktop`: scan ures, connect nyomtat+true; MPC (NO_HARDWARE): `executeCommand` {} -> a lista ures, minden "sikeres". Interfesz fixen `wlan0`. Fuggo parancsok: `iwlist`, `ip link`, `killall`, `wpa_supplicant`, `dhclient` - MPC rootfs-ben NEM ELLENORIZHETO kodbol (dokumentum csak BusyBox/Yocto-t emliti).

## 5. HwPref rendszerreteg (`kick_Hwpref.cpp`) - ellenorzendo parancs/fajl lista
| Funkcio | Parancs / fajl (fajl:sor) | Megjegyzes |
|---|---|---|
| Fenyero | `echo N > /sys/devices/platform/backlight/backlight/backlight/brightness` (:849-852), min 20, felso korlat nincs (UI 0..100) | Behringer-ut; MPC bridge-ben nincs fenyero-kezeles (`rg backlight` ures) |
| Datum | `setDate`: a parancs maga `"2025-5-15"` (:941-944) - hianyzik a `date -s` | **BUG (megerositve)**, mindig hibazik |
| Ido | `setTime`: `"10:5:00"` (:959-962) - hianyzik `date -s`, a perc nincs 0-padolva | **BUG (megerositve)** |
| Datum kiolvasas | `setenv("TZ")` + `tzset` + `localtime` (:796-805) | A TZ-t az egesz processre allitja (kapu nelkul, szalbiztonytalan); alap tz "Asia/Kolkata" (`kick_settings.cpp:241`) |
| Idozona | `timedatectl set-timezone <str>` (:828) | systemd kell; a `str` prefsbol jon -> injekcio, ha a fajl manipulalt |
| Eth | `ifconfig eth0 up/down`, `ifconfig eth0 ip netmask mask`, `/usr/sbin/udhcpc -i eth0 &`, `killall udhcpc`, `ip -4 addr show`, `ip route show`, `ifconfig eth0`, `ethtool eth0`, `/sys/class/net/eth0/operstate` | ld. 3. |
| Factory reset | `/bin/rm -rf /home/Kick/projects* /home/Kick/prefs` (:29,136) | ld. 1/2.; nincs ujrainditas, a futo app utana ujra letrehozza a prefs-et |
| Poweroff | `/sbin/poweroff` (:30,141) | nincs UI hivo; a power-button szal (`run`, :39-84, `/dev/input/event2`) nincs inditva (`startThread` kikommentezve :36), nyitasi hibanal alvas nelkuli ciklus |
| Info | `/sys/devices/soc0/serial_number`, `/etc/version.txt` (regex `^name d.d.d$`), `df -h`, `/proc/mounts` ("flash" substring), `/mnt/flash` | MPC-n erteluktelen/N/E |
| SW update | `mkdir`, `mount -o loop -t ext4`, `tail -c +N file > /tmp/image/...`, `openssl enc -aes-256-cbc -pbkdf2 -d -k 'PJSDSOMKICK'` (:509, a kulcs forrasban + process-listaban), `/usr/bin/swupdate -i /tmp/fwupdate/Kick.swu -v &` (`kick_FirmwareUpdate.cpp:445`), `reboot -nf` (`kick_SWUpdateScreen.cpp:126`, kapu nelkul) | A fajlnevek idezet nelkul kerulnek a shellbe (USB fajlnev -> injekcio rootkent). `linuxUpdate` `-1` NO_HARDWARE-nel. Ket vegtelen `while(1)` var (`:453,467`) |
- `executeSetCommand` = `std::system` (shell); `executeCommand` = `popen`. A visszateres `-1`, ha NO_HARDWARE - a hivok ezt hibanak sem jelzik.

## 6. Hardware oldal (3.) a rendszerhez es az MPC hidhoz (reszleges, az 1-4. masik felmero)
- Pad threshold/sensitivity/curve (alap "4"/"4"/"4", `kick_settings.cpp:237-241`): `SendPadSettings` DCL csomag `{16, thr, sens, curve}` (`hardware.cpp:139-147`), bootkor, valtaskor es projektbetoltesnel (`M/kick_leds/kick_leds.cpp:358-363`). **MPC-n elvesz:** a hid `onDclCommand` csak `EN_PAD_SET_AFTER_TOUCH`-ot es `EN_PAD_SET_COLOUR_STATE`-et dolgoz fel (`apps/kick/kick_hardware_bridge.h:138-160`); `EN_PAD_SETTINGS` nincs fogyasztoja az `apps/`-ban (rg). A `sendMessageToHardware` a soros frontpanelre megy (`input_events.cpp:76-82`, MPC-n `KICK_NO_SERIAL_DEVICE=1`). A WP-G G-107/108 "OK" itelet ezert cafolhato. Pad-velocity gorbe az MPC-n csak Full/Half Level (`pad_level.h`, `bridge.h:1746`).
- Aftertouch kapcsolo: `enableAfterTouchMessage` -> `m_pad_aftertouch_enabled` (`bridge.h:143-146`), de `kSendPadAftertouchPackets = false` (:1774), ezert a VELOCITY/AFTERTOUCH meroszlak (`hardware.cpp:206-222`) MPC-n csak az ujjal huzott beallitassal valtoznak; a pad-velocity elo mero mukodik. Ezek a "mero" ertekek a prefsbe is menetenek minden padutesnel (`UpdatePadInfo` :199-203) -> felesleges autosave.
- **Datum/ido UI:** a `date_YYYY` kezelo a **evet adja honapkent** a `getDaysInMonth`-nek (`hardware.cpp:110`, vs :104) -> 31 nap mindig; a "Day/Minute/Year" cimke a honapra "Minute" (`hardware.h:56`); a `setDate/setTime` hibas (5.); a `refresh()` 1 Hz-en visszairja az uj ertekeket a rendszerorabol (:153,181-190) -> a szerkesztes elvesz. WP-G G-122..G-124 "OK" itelet cafolhato.
- Tap tempo LED / taps: csak a prefsbe iras, nincs fogyaszto (`rg tempoLEDStatus|numTaps`: csak settings) -> STUB (WP-G G-115/116 "OK" - cafolhato).

## 7. Perzisztencia (`M/engine/project/kick_settings.*`, `kick_xml_manager.*`)
- Fajl: `<prefs>/preferences.settingsprj` (`kick_settings.h:18-20`, `kick_settings.cpp:44`), XML ValueTree; gyoker `PREFERENCES`; reszfak: `PROJECTDEFAULTS, PREFMIDICONTROL, ETHERNET, PREFCV, PREFHARDWARE, BROWSERFAVOURITE, PREFCVCLIB, METRONOME, WIFI` (:36-50).
- Mentes: valtozasra `valueTreeChanged` -> 1,5 s debounce autosave (`kick_xml_manager.cpp:171-185`, prefs mindig autosave), atomi `TemporaryFile` iras, csak ha a tartalom valtozott (:296-311). A `getSettings()` singleton `new`-val keszul, sosem szabadul (`kick_settings.cpp:26`), nincs kilepeskori kenyszeritett `save()` (rg) -> az utolso <1,5 s valtozas elveszhet leallitaskor/kill-nel. `onSaveFailure` a prefs-nel nincs bekotve -> csendes hiba.
- Betoltes: ha a fajl letezik es nem ures, `ValueTree::fromXml`; ha serult (nem parse-olodik), `failSave("Project XML could not be read")`, a `vt` ures marad, az `onLoad` nem fut -> a settings reszfak ervenytelenek, a getterek alapertekeket adnak, a setterek no-opok, nincs felajanlott javitas/ujraletrehozas, a serult fajl sem csereldik (vagyis a beallitasok a session alatt nem mentodnek).
- Verziozas: a `version="1.0"` csak irodik (:203), sosem olvasodik, nincs migracio / sema-ellenorzes.
- Alapertek-visszaallitas (`makeDefaults`, `kick_settings.cpp:61-67`): csak `projects, hardware, eth, cv` (property-torles); NEM erinti `wifi` (jelszo!), `cvclib` (kalibracio), `midiControl`, `metronome`, `browser`-t. Nem tolt vissza semmit a hardverre (fenyero, pad), a UI a kovetkezo 1 Hz refreshnel frissul.
- Szalbiztonsag: UI/message thread; az `Atomic<>` ertekek `CachedValue`-k - tobb szal olvasasa nem bizonyitott; a CV tabla verseny (2.).
- Jogosultsag: nincs `chmod`; a WiFi jelszo a prefs fajlban es a `wifi_credentials.txt`-ben is olvashato.

## 8. Biztonsag - osszegzes
(a) Jelszo szovegesen 3 helyen (prefs, credentials fajl, wpa conf), reset nem torli; (b) `system()` shell string-osszefuzessel: `ifconfig` (IP/maszk prefsbol, ujsor a `getIP` kimenetben), `timedatectl` (idozona prefsbol), `tail/openssl/mount` (USB fajlnev) - a UI csak szamjegyet enged, de a `.settingsprj` vagy a pendrive manipulacioja root parancsfuttatashoz vezet; (c) WPA conf-injekcio (`"`/ujsor); (d) forrasban levo firmware-kulcs `PJSDSOMKICK`; (e) `/data/kick_cmd` FIFO 0666 (`bridge.h:~800`) - nem ezen oldalhoz tartozik, de a beallitasok injektalhatoak.

## 9. Mi hianyzik az Akai MPC Standalone Preferences-hez kepest (WP-G, doc-vs-kick)
Nincs: Bluetooth (G-073/074), Sequencer/General nezetek (G-126..136), Audio Device + 32 I/O + Disk Streaming + Export/Warp/BPM-detektalas (G-075..089), MIDI Sync/MTC/MMC/Clock fogadas-kuldes, port-atnevezes/reset (G-090..104), kepernyo-halvanyitas es 4 fokozatu fenyero (G-112/113), Ableton Link (NEM-KICK), Wi-Fi Show/Forget/Reset/halozat-info (G-065,066,068,069), Legal Info, Reset utan mentes-felajanlas (G-048), Hardware > touch strip alapmod (MORNING_DECISIONS:122,132), Keyboard velocity curve/gain/sensitivity (DOC_VS_KICK #19). Kick-only extra: CV oldal (a Key 61-en nincs CV; kodjuk a Behringer I2C-DAC-ot celozza).

## 10. Dokumentumok - ismert hibak / tevedesek
- `docs/planning/plans/research/screen-inventory.md` 4.7 jol nevezi: DNS nem ervenyesul, WiFi gateway=subnet, setDate/setTime hianyzo parancs, MIDI output port nem all vissza (**mindhaton megerositve kodbol**: `midi_sync.cpp:101` csak `inputList.setSelected`; a `midiControl.*` ertekeknek nincs fogyasztoja az UI-n kivul), az eveht-honap hiba (**megerositve**). A CV sor "N/E" volt; most feltarva. Javitando: "board-parancsok executed" -> NO_HARDWARE miatt no-op (1. pont).
- `known-issues-and-roadmap.md` (:62 H18: CV hardveren nem tesztelt; :81-83 Preferences-hianyok) es `todo.md` T27 (EN_PREF bekotes) - a Preferences belso hibait nem tartalmazza; WP-G G-046 "perzisztencia nem vizsgalt" - most vizsgalva (7.).
- WP-G "OK" itelet ellentmondasai: G-062 (WiFi enable), G-071/072 (Eth mod/mezok), G-107/108 (pad threshold/sens), G-115/116 (tap), G-122..124 (datum/ido).

## 11. Tesztek (Kick/tests/platform, `CMakeLists.txt`)
Fedezi: `CvScaleTests.cpp` (csak `stepToDac`); `SystemProbeTests.cpp` (csak `/dev/i2c-*` felsorolas); `AppSmoke.py` + `Main.cpp:158` (HomePreferencesView kepernyokep, nem funkcio-teszt). NEM fedezi: HwPref, WifiManager, Settings perzisztencia/makeDefaults, `CV::applyCalibration/setVoltage`, IPv4 editor, bridge `EN_PAD_SETTINGS`. Nincs tesztelt kod a fenti hibak egyikere sem.
