# Terv 01 — Telepíthető pendrive (Kick az MPC Key 61-en, a felhasználóknak)

Állapot: terv (csak kutatás, kód nem változott). Mért adatok: az élő MPC Key 61-ről, 2026-11-04, csak olvasással.

## Kérdés
Lehet-e olyan pendrive-ot adni a usereknek, amiről az MPC elindítja a Kicket (vagy felmásolja magára és úgy indítja), anélkül, hogy kézzel kellene scp-zniük?

## Válasz röviden
**Igen, a pendrive-os indítás már ma működik, és ugyanerre építhető a letölthető csomag.** Teljes OS-boot USB-ről viszont nem reális (lásd C. opció).

## Mért tények (az MPC-n)
- Az `acvs` szolgáltatás az `/usr/bin/az01-launch-MPC`-t futtatja (rootfs: `/dev/mmcblk0p6`, **csak olvasható**).
- Ez a szkript az MPC-LiveXplore (TheKikGen) módosított változata: indításkor minden USB-lemezt `rw,exec`-kel felmountol (`edisksctl mount`), az **első** `<mount>/tkgl_bootstrap_[ProjectData]/scripts/tkgl_bootstrap` fájlt `sh`-val lefuttatja (előtte legenerálja a `scripts/tkgl_path` fájlt: `DEVICE`, `MOUNT_POINT`, `TKGL_ROOT/SCRIPT/BIN/LIB/CONF/LOGS`), majd (ha a bootstrap visszatér) elindítja a gyári `/usr/bin/MPC`-t.
- A mostani pendrive: `/dev/sdb1` EFI (vfat) + `/dev/sdb2` exFAT (`/media/PenDrive`, 7 GB). A bootstrap szkript (`scripts/tkgl_bootstrap`) ellenőrzi, hogy `/data/Kick` létezik-e, majd `exec /data/Kick` (`KICK_NO_HARDWARE=1 KICK_NO_SERIAL_DEVICE=1`). **A bináris tehát a belső flashen van (`/data`), nem a pendrive-on.** Pendrive nélkül a gyári MPC indul.
- `secure-boot: 0`, de van `az01-signed-fs` és `/data/az01-update.img` (167 MB) — a gyári frissítés aláírt.
- Bootloader: u-boot az eMMC-n (`mmcblk0p1`/`p3`, env: `p2`).

## Opciók

### A. Csak-pendrive csomag a meglévő bootstrap-hookra (AJÁNLOTT)
A pendrive tartalma: `tkgl_bootstrap_[ProjectData]/{scripts/tkgl_bootstrap, bin/Kick, assets/…}`.
A `tkgl_bootstrap` két mód közül választ (a `tkgl_path` változókkal, nem fix útvonalakkal):
1. ~~Futtatás a pendrive-ról~~ — elvetve (owner döntés, lásd lent).
2. **Telepítés a belsőre + onnan futás — ez az alapmód** (verzió-eltérésre frissít, lásd „DÖNTÉS").
- Kell hozzá: a Kick által használt `assets`, `templates`, `samples` felderítése és csomagolása; írható állapot (projektek, napló) helye (`/data` vagy a pendrive).
- Előny: nem nyúl a rendszerhez, nem igényel új gyári-OS fejlesztést.

### B. Előfeltétel: a hook megléte a usernél — NYITOTT KÉRDÉS
A hook a **rootfs-ben** van (`az01-launch-MPC` módosítva, dátum 2025-03-03). Nem tudjuk, hogy ez gyári firmware-tartalom-e, vagy ezen a gépen telepítették (az `az01-update.img`-ben a `tkgl_bootstrap` szöveget nem találtam, de a kép lehet tömörített/titkosított → nem perdöntő). Ha egy gyári gépen nincs hook, a usernek egyszeri **hook-telepítés** kell (MPC-LiveXplore telepítő), ami a rootfs-t módosítja → kockázat/felelősség/dokumentáció kérdése.
- Teendő: ellenőrizni gyári, frissen frissített (vagy másik) MPC-n; MPC-LiveXplore README és licenc áttekintése.

### C. Teljes OS-boot USB-ről / saját firmware-image — NEM AJÁNLOTT
Az u-boot env módosítása (`mmcblk0p2`) téglásodási kockázat; a gyári frissítés aláírt (`az01-signed-fs`); saját rootfs-képet Akai-kód nélkül nem terjeszthetünk. Csak akkor érdemes újra elővenni, ha az A/B nem járható.

## „Image" vs zip
A hook exFAT/FAT32-t is mountol, ezért **elég egy zip**, amit a user bármilyen pendrive gyökerébe kicsomagol. Egy nyers `.img` (GPT, EFI + exFAT) csak kényelmi extra (Etcher), plusz hiba-forrás (méretezés, partíció-azonosítás) — a zip az elsődleges.

## Lépések (sorrendben)
1. Futásidejű függőségek felmérése: `readelf -d Kick`/`ldd` (statikus-e), `/data/assets|templates|samples|Settings` mit olvas, mit ír.
2. `tkgl_bootstrap` újraírása: futtatás a pendrive-ról (`$TKGL_BIN`), fallback a gyári MPC-re; napló a pendrive `logs/`-ába.
3. Release-script: ARM build → csomag (`dist/kick-mpc-key61-<verzió>.zip`) + SHA256 + verzió-fájl.
4. Telepítés-a-belsőre mód (opcionális) és eltávolítás/„vissza a gyári MPC-hez" leírás.
5. Hook megléte gyári gépen (B. pont) — dönteni a telepítő mellékeléséről vagy hivatkozásról.
6. Tesztek valódi hardveren: tiszta pendrive; pendrive nélkül (gyári MPC indul); exFAT és FAT32; a stick kihúzása futás közben; hideg indítás (adapter küldi a gyári init-SysEx-et, lásd `KEY61_HW_BEHAVIOUR.md`).
7. User-dokumentáció (letöltés, kicsomagolás, bedugás, visszaállítás) + jogi: csak Kick és saját szkriptek kerülnek a csomagba, Akai-fájlok nem.

## DÖNTÉS (owner, 2026-11-04): alapmód = automatikus telepítés a belsőre, onnan futás
Boot után a `tkgl_bootstrap`:
1. Ha `/data/Kick` **nincs** telepítve → felmásolja a pendrive-ról, majd onnan (belső flashről) indítja.
2. Ha telepítve van → **csak akkor** frissíti, ha a pendrive-os verzió **eltér** a helyitől (nem csak újabb: eltérés = csere, így a visszalépés is működik). Egyező verziónál nem ír a flashre, simán indít.
3. Mindig a belső `/data/Kick`-et futtatja (nem a pendrive-ról) — a pendrive a telepítés után kihúzható.

Részletek a megvalósításhoz:
- Verzió-azonosító: a csomagban `VERSION` fájl (verzió + build hash + a bináris SHA256), a helyi párja `/data/Kick.version`. Összehasonlítás a `VERSION` fájl tartalmával, nem időbélyeggel (FAT/exFAT időbélyeg megbízhatatlan).
- Atomi csere: másolás `/data/Kick.new`-ba → SHA256 egyezés a `VERSION`-nel → `Kick.prev` mentés → `mv` → `Kick.version` frissítése **utoljára**. Félbeszakadt másolás (kihúzott stick, áramszünet) soha nem hagy hibás `/data/Kick`-et.
- Hibakezelés: sikertelen telepítés/ellenőrzés → a korábbi `/data/Kick` marad és indul; ha nincs semmi futtatható → `exit 0`, a gyári MPC indul.
- Az `assets/templates/samples` ugyanígy verziózva másolódik (a felhasználó saját projektjeit/mintáit **nem** írja felül: csak a csomag által szállított fájlokat).
- Naplózás: `/data/kick-boot.log` (és a pendrive `logs/`), benne „telepítve / frissítve A→B / változatlan".
- Visszaállítás a gyári MPC-hez: leírás + `uninstall` szkript (törli `/data/Kick*`-et; a hook marad).

## DÖNTÉS (owner, 2026-11-04): hook-telepítő
Egyelőre **nem mellékeljük**, a dokumentáció csak hivatkozik az MPC-LiveXplore-ra. A tényleges helyzetet (gyári gépen megvan-e a hook) egy **másik Akai MPC-n** ellenőrizzük majd; az eredmény alapján döntünk újra.
- Teendő (nyitott, owner hardvere): másik MPC-n `cat /usr/bin/az01-launch-MPC` (megvan-e a `tkgl_bootstrap` rész), firmware-verzió (`az01-info`), és hogy a gépen valaha telepítettek-e hookot.

## Még döntendő
- (nincs nyitott döntés; a fenti ellenőrzés után változhat)
