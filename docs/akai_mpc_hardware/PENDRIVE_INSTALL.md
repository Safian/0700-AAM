# Behringer Kick – Akai MPC Key 61 Pendrive Telepítési és Használati Útmutató

Ez a dokumentum bemutatja, hogyan telepíthető, frissíthető és futtatható a **Behringer Kick** nyílt forráskódú DAW az **Akai MPC Key 61** szintetizátoron egy szabványos USB pendrive segítségével.

---

## 1. Működési elv és architektúra

A Kick pendrive-os indítási mechanizmusa az **MPC-LiveXplore** bootstrap hookra épül:
1. **Biztonságos és roncsolásmentes:** A gép gyári rootfs partíciója (`/dev/mmcblk0p6`) csak olvasható, az u-boot bootloader környezet és a gyári firmware érintetlen marad.
2. **Automatikus belső flash-telepítés (`/data`):**
   - Bekapcsoláskor a pendrive-on lévő `tkgl_bootstrap` szkript ellenőrzi az aktuális verziót (`VERSION`).
   - Ha a Kick még nincs a belső flashen, vagy a pendrive-on lévő verzió eltér a helyitől, **automatikusan és atomi módon** átmásolja a binárist és az erőforrásokat a belső `/data/Kick` helyre.
   - Ha a verzió egyezik, nem ír a flashre, hanem azonnal elindítja a belső `/data/Kick`-et.
3. **Kihúzható pendrive:** Mivel a Kick a belső eMMC flashről (`/data`) fut, a pendrive a betöltés után futás közben biztonságosan kihúzható!
4. **Kétutas indítás (Dual-boot):**
   - **Bedugott Kick pendrive-val bekapcsolva:** A Behringer Kick indul el.
   - **Pendrive nélkül bekapcsolva:** A gyári Akai MPC DAW indul el.

---

## 2. Futásidejű függőségek és a `/data` fájlrendszer

### Rendszer- és bináris függőségek
- **Platform:** ARMv7-A (32-bit hard-float, NEON SIMD), Linux kernel 3.10 / 4.x (Yocto / glibc).
- **Dinamikus könyvtárak:**
  - `libc.so.6`, `libstdc++.so.6`, `libgcc_s.so.1` (C/C++ futtatókörnyezet)
  - `libpthread.so.0` (POSIX szálkezelés)
  - `libm.so.6` (matematikai függvények)
  - `libdl.so.2` (dinamikus betöltő)
  - `librt.so.1` (valós idejű órák és időzítők)
  - `libasound.so.2` (ALSA hangrendszer, MPC belső audio kodek és MIDI portok)
- *Megjegyzés az elemzéshez (`readelf -d` / `ldd`):* A bináris dinamikus függőségeinek részletes ellenőrzése cross-compile környezetben (Docker `kick-armv7-builder`) vagy közvetlenül a hardveres terminálon végezhető el; macOS gazdagépen az ARM ELF közvetlenül nem futtatható.

### A `/data` könyvtár tartalma az Akai MPC-n
A Kick Linux alatt (a `Kick/modules/common/source/kick_paths.cpp` implementációja szerint) a következő útvonalakat használja:

| Útvonal | Típus | Leírás | Frissítéskor felülíródik? |
|---|---|---|---|
| `/data/Kick` | Végrehajtható fájl | A Kick DAW fő alkalmazás-binárisa | **Igen** (atomi csere, előtte `/data/Kick.prev` mentés) |
| `/data/Kick.version` | Szöveg | Az éppen telepített verzió és SHA256 ellenőrzőösszeg | **Igen** (a sikeres telepítés legutolsó lépéseként) |
| `/data/Kick.prev` | Végrehajtható fájl | Az előző működő Kick bináris biztonsági mentése | Automatikusan frissül |
| `/data/assets/` | Könyvtár | UI témák (`Default`, `Proper`), XML leírók, betűkészletek, ikonok | **Igen** (csomagból szinkronizálva) |
| `/data/templates/` | Könyvtár | Projekt sablonok (`Template 01` .. `05`) | **Igen** (csomagból szinkronizálva) |
| `/data/demos/` | Könyvtár | Demo projektek (`Demo 01` auto-load induláskor) | **Igen** (csomagból szinkronizálva) |
| `/data/sounds/` | Könyvtár | Hangminták és gyári audiofájlok | Csak kiegészítés (meglévő fájlokat nem töröl) |
| `/data/prefs/` | Könyvtár | Beállítások (`preferences.settingsprj`) | **NEM** (a felhasználói beállítások megmaradnak) |
| `/data/projects/` | Könyvtár | A felhasználó saját projektjei (`.kickPrj`) | **SOHA NEM NYÚL HOZZÁ** |
| `/data/kick_cmd` | FIFO (pipe) | Valós idejű parancssori és UI vezérlő csővezeték | Futásidőben jön létre |
| `/data/kick_screen.ppm` | Kép (PPM) | Framebuffer képernyőmentés távoli megfigyeléshez | Futásidőben frissül |
| `/data/kick.crash` | Szöveg | Összeomlás (SIGSEGV stb.) regiszter- és veremnapló | Csak hiba esetén |
| `/data/kick-boot.log` | Szöveg | A bootstrap indítási és telepítési naplója | Minden indításkor bővül |
| `/data/kick.log` | Szöveg | A Kick alkalmazás konzolkimenete és HAL naplója | Indításkor rotálódik (`.old`) |

---

## 3. Előfeltételek

1. **Akai MPC Key 61** szintetizátor (vagy más kompatibilis Akai Standalone eszköz).
2. **Bootstrap hook:** A gyári `/usr/bin/az01-launch-MPC` szkriptben aktív TheKikGen-féle `tkgl_bootstrap` meghívó hook.
   > [!NOTE]
   > A hook a [TheKikGen MPC-LiveXplore](https://github.com/TheKikGen/MPC-LiveXplore) nyílt forráskódú projektből származik. A fejlesztői döntés értelmében a hivatalos Kick csomag nem tartalmaz firmware-módosító telepítőt; a hook meglétét feltételezzük.
3. **USB Pendrive:**
   - Legalább 512 MB méretű USB flash meghajtó.
   - Ajánlott fájlrendszer: **exFAT** vagy **FAT32** (MBR vagy GPT partíciós táblával).

---

## 4. Lépésről lépésre: Telepítés és Első Indítás

### 1. Csomag letöltése
Töltsd le a legfrissebb kiadási csomagot:
- `kick-mpc-key61-<verzió>.zip`
- `kick-mpc-key61-<verzió>.zip.sha256`

Ellenőrizd a letöltött fájl épségét:
```bash
# macOS:
shasum -a 256 -c kick-mpc-key61-<verzió>.zip.sha256

# Linux:
sha256sum -c kick-mpc-key61-<verzió>.zip.sha256
```

### 2. Kicsomagolás a pendrive-ra
Csomagold ki az archívumot közvetlenül a pendrive **gyökérkönyvtárába**.
A pendrive gyökerében a következő struktúrának kell lennie:

```text
/Volumes/PenDrive/ (vagy /media/PenDrive/)
└── tkgl_bootstrap_[ProjectData]/
    ├── VERSION
    ├── bin/
    │   └── Kick
    ├── scripts/
    │   ├── tkgl_bootstrap
    │   └── uninstall_kick.sh
    ├── assets/
    │   ├── Default/
    │   └── Proper/
    ├── templates/
    ├── demos/
    └── logs/
```

> [!IMPORTANT]
> Ügyelj arra, hogy a `tkgl_bootstrap_[ProjectData]` mappa közvetlenül a pendrive gyökerében legyen, ne egy almappában!

### 3. Bedugás és első bekapcsolás
1. Kapcsold ki az MPC Key 61-et.
2. Csatlakoztasd a pendrive-ot a hangszer hátoldalán lévő USB-A aljzatok egyikébe.
3. Kapcsold be a hangszert.
4. **Mi történik az első indításkor?**
   - Az indítórendszer érzékeli a pendrive-on lévő csomagot.
   - A `tkgl_bootstrap` szkript leellenőrzi a bináris SHA256 ellenőrzőösszegét.
   - Átmásolja a fájlokat az internal flash `/data` partíciójára (`Kick`, `assets`, `templates`, `demos`).
   - Létrehozza a `/data/Kick.version` fájlt.
   - Elindítja a Kicket (`KICK_NO_HARDWARE=1 KICK_NO_SERIAL_DEVICE=1 exec /data/Kick`).
   - A kijelzőn megjelenik a Kick felülete és betöltődik a Demo 01 projekt.
5. A pendrive ezután a gép működése közben bármikor **kihúzható**.

---

## 5. Frissítés és Visszalépés (Update & Rollback)

A frissítés folyamata teljesen automatikus:
1. Töltsd le az új (vagy régebbi) verziójú `kick-mpc-key61-<új-verzió>.zip` csomagot.
2. Töröld le a pendrive-ról a régi `tkgl_bootstrap_[ProjectData]` mappát, és csomagold ki az újat.
3. Dugd be a pendrive-ot az MPC-be és kapcsold be.
4. A bootstrap észleli, hogy a pendrive `VERSION` tartalma eltér a belső `/data/Kick.version`-től:
   - A jelenlegi binárist elmenti: `/data/Kick.prev`.
   - Bemásolja az új binárist `/data/Kick.new` néven.
   - SHA256 egyezés esetén lecseréli a `/data/Kick`-et.
   - Frissíti az assets/templates/demos mappákat.
   - Frissíti a `/data/Kick.version` fájlt.
   - Elindítja az új verziót.
   - **A felhasználói projektekhez (`/data/projects`) és beállításokhoz nem nyúl.**

Védelmek (R-7):
- A csomag csak akkor települ, ha a `VERSION` tartalmaz `KICK_SHA256`-ot ÉS van `sha256sum`/`shasum` az MPC-n; különben a telepítés megtagad­tatik (a régi Kick marad, üres `/data`-nál a gyári MPC indul).
- Megszakadt frissítés (áramkimaradás a bináris cseréje után): a `/data/Kick.installing` jelző (a cél SHA256-ja) alapján a következő indítás felismeri, és a `Kick.prev` NEM íródik felül a félig telepített binárissal.
- **Visszalépés:** tegyél egy üres `ROLLBACK` fájlt a pendrive gyökerébe (vagy `/data/Kick.rollback` az appból) és indítsd újra az MPC-t: a bootstrap a `Kick.prev` + `Kick.version.prev` párost állítja vissza (SHA256-ellenőrzéssel; hiányzó vagy sérült `Kick.prev` esetén nem változtat semmit). A visszalépett csomag a `/data/Kick.rejected`-ben van megjegyezve, ezért ugyanarról a pendriveról nem települ újra, amíg más csomag nem kerül rá (vagy a `Kick.rejected` törlésre nem kerül).
- Kiadási jegyzet: `make_pendrive_package.sh --changelog <fájl>` a csomagba teszi a `CHANGELOG`-ot; a Kick Update képernyő az első 4 sorát mutatja telepítés előtt.

---

## 6. Visszaállítás a Gyári Állapothoz (Eltávolítás)

Ha vissza szeretnéd állítani az MPC Key 61-et a teljesen gyári működésre, három egyszerű lehetőség van:

### A. módszer: Gyors pendrive-os eltávolítás (Nem igényel számítógépet/SSH-t)
1. Csatlakoztasd a pendrive-ot a számítógépedhez.
2. Hozz létre egy üres fájlt `UNINSTALL` néven a pendrive gyökérkönyvtárában:
   ```bash
   touch /Volumes/PenDrive/UNINSTALL
   ```
3. Dugd be a pendrive-ot az MPC-be és kapcsold be.
4. A bootstrap észleli az `UNINSTALL` fájlt:
   - Törli a `/data/Kick` binárist, annak mentéseit és átmeneti fájljait.
   - Átnevezi a triggert `UNINSTALL.done`-ra.
   - Kilép (`exit 0`).
   - A gép automatikusan a gyári Akai MPC szoftvert indítja el!

### B. módszer: Eltávolítás parancssorból (SSH-n keresztül)
Ha be tudsz jelentkezni az MPC-re SSH-val (`root@192.168.0.30`):
```bash
# Alapértelmezett eltávolítás (binárisok és naplók törlése):
sh /media/PenDrive/tkgl_bootstrap_[ProjectData]/scripts/uninstall_kick.sh -y

# Teljes eltávolítás (az assets/templates/demos mappák törlésével együtt):
sh /media/PenDrive/tkgl_bootstrap_[ProjectData]/scripts/uninstall_kick.sh --all -y
```
Az uninstaller leállítja a futó Kick folyamatot és törli a fájlokat. Ezután:
```bash
systemctl restart acvs
```
vagy a hangszer újraindítása után a gyári Akai MPC DAW indul el.

### C. módszer: Kick pendrive kihúzása
Ha nem szeretnéd törölni a Kicket a belső memóriáról, de most a gyári Akai szoftvert szeretnéd használni:
- **Egyszerűen kapcsold be az MPC-t bedugott Kick pendrive nélkül.**
- Pendrive hiányában az indító hook azonnal a gyári `/usr/bin/MPC`-t futtatja.

---

## 7. Hibaelhárítás és Naplózás

### Hol találhatók a naplók?
1. **Telepítési és bootstrap napló:**
   - Belső flash: `/data/kick-boot.log`
   - Pendrive: `tkgl_bootstrap_[ProjectData]/logs/kick-boot.log`
2. **Kick alkalmazás naplója:**
   - `/data/kick.log` (és az előző futás naplója: `/data/kick.log.old`)
3. **Összeomlási napló:**
   - `/data/kick.crash` (ha memóriahiba vagy szegmentálás történne)

### Gyakori kérdések és hibák
- **Bekapcsoláskor a gyári Akai MPC indul el a Kick helyett:**
  - Ellenőrizd, hogy a pendrive helyesen van-e megformázva (exFAT / FAT32).
  - Ellenőrizd, hogy a `tkgl_bootstrap_[ProjectData]` mappa a gyökérben van-e, és létezik-e benne a `scripts/tkgl_bootstrap`.
  - Ellenőrizd a pendrive `logs/kick-boot.log` fájlját.
- **A képernyő fekete marad:**
  - Nézd meg a `/data/kick.log` fájl utolsó sorait SSH-n át:
    ```bash
    tail -n 30 /data/kick.log
    ```
  - Ellenőrizd, hogy a HAL híd elérte-e az operatív állapotot:
    `[HAL] KickHardwareBridge fully operational`.
- **Áramszünet vagy kihúzott pendrive telepítés közben:**
  - Az atomi cserének köszönhetően a Kick nem sérül meg: a `/data/Kick.new` fájl csak sikeres SHA256 ellenőrzés után váltja le a meglévőt, a `Kick.version` pedig a legvégén frissül.
  - A következő indításkor a folyamat automatikusan újrakezdődik.

---

## 8. Jogi nyilatkozat és Licenc

- **Behringer Kick:** GNU Affero General Public License v3.0 or later (AGPL-3.0-or-later). Eredeti szerző: MUSIC Tribe.
- **MPC-LiveXplore:** Copyright (c) TheKikGen Labs.
- **Védjegyek:** Az *Akai Professional*, *MPC*, *MPC Key 61* az inMusic Brands, Inc. védjegyei. A *Behringer*, *MusicTribe* a Music Tribe Global Brands Ltd. védjegyei. A projekt független nyílt forráskódú fejlesztés, semmilyen kapcsolatban nem áll az inMusic Brands céggel.
- A telepítőcsomag **kizárólag nyílt forráskódú fájlokat tartalmaz**, semmilyen jogvédett Akai szoftvert vagy binárist nem terjeszt.
