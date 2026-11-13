> **ARCHIVÁLT, NEM FEJLESZTENDŐ, scope-on kívül (2026-11-13).** Az Akai MPC támogatás megszűnt (owner: "a kick-es akai fejlesztését el tudjuk engedni mostantól, az akaira fejlesztünk csak (a SynthyBoy-t)! max mobilra fogjuk folytatni"). Történeti hivatkozás: ne fejleszts rá, ne használd kiindulásnak. Az itt említett kódútvonalak/linkek elavultak lehetnek.

# Akai MPC Key 61 – ARMv7 build-recept (Docker, macOS gazdán)

A korábbi recept sehol nem volt rögzítve; ez a 2026-11-01-i hardveres próbánál ténylegesen használt, működő recept. A `kick-armv7-builder` image arm64 Debian (g++-arm-linux-gnueabihf, `:armhf` könyvtárak, gdb-multiarch, readelf).

## 0. A `kick-armv7-builder` image
Ha az image hiányzik (`docker images | grep kick-armv7`; a Docker VM újraépítése/takarítása törli): `docker build -t kick-armv7-builder -f docs/hardware/key61/tools/Dockerfile.armv7-builder docs/hardware/key61/tools` (Debian 12 bookworm arm64, GCC 12.2, cmake 3.25; kb. 1-2 perc). Inkrementális buildhez másold át egy korábbi `build-arm-cpm/`-et az új worktree-be (a konténerben az út mindig `/workspace`), és a **git által változtatott fájlokat `touch`-old meg** az átmásolt build készítési commitja óta (`git diff --name-only <commit> HEAD | xargs touch`), különben a régi objektum marad meg (2026-11-05: `undefined reference to mt::HwPref::setTimeZone` linkelési hiba).

## 1. Toolchain-fájl (`armv7.cmake`)
```cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)
set(CMAKE_C_FLAGS_INIT "-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard")
set(CMAKE_CXX_FLAGS_INIT "-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard")
set(CMAKE_FIND_ROOT_PATH /usr/arm-linux-gnueabihf)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
```

## 2. Docker parancs
`$W` = a checkout (worktree), `$TC` = a toolchain-fájl mappája, `$CACHE` = **írható** CPM-gyorsítótár (egy csak olvasható mountnál a CPM nem tud `cmake.lock`-ot írni, ezért másolat kell).
```bash
docker run --rm -v $W:/workspace -v $CACHE:/cache -v $TC:/tc:ro \
 -e PKG_CONFIG_LIBDIR=/usr/lib/arm-linux-gnueabihf/pkgconfig:/usr/share/pkgconfig -e PKG_CONFIG_SYSROOT_DIR=/ \
 kick-armv7-builder bash -c '
 set -e
 cmake -S Kick -B build-arm-cpm -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=/tc/armv7.cmake \
  -DKICK_ENABLE_HARDWARE=ON -DKICK_TOUCH_SINGLE_TAP_LISTBOX=ON \
  -DCPM_SOURCE_CACHE=/cache -DTIMESTAMP=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5
 cmake --build build-arm-cpm --target Kick -j4'
```
Bináris: `build-arm-cpm/apps/kick/Kick_artefacts/Release/Kick`. Hibakereséshez `-DCMAKE_CXX_FLAGS=-g` (kb. 190 MB-os bináris; core elemzése: `gdb-multiarch` ugyanebben az image-ben). A linkelés LTO miatt soros (128 LTRANS feladat), percekig tart: háttérben futtasd.

## 3. Opciók, megjegyzések
- `-DKICK_ENABLE_HARDWARE=ON` kötelező: a `OFF` build induláskor szegmentál (a kijelzőlista üres, lásd a riport 1. hibáját). Kikapcsolni futásidőben lehet: `KICK_NO_HARDWARE=1`, `KICK_NO_SERIAL_DEVICE=1`.
- `-DKICK_TOUCH_SINGLE_TAP_LISTBOX=ON`: a cloud session közlése szerint a F11 javítás (kick_listbox) óta kell, különben a legördülő lista nem nyílik egy érintésre. **Ezt a próbámnál nem használtam, és az én (4605b75e alapú) fámban az opció még nincs meg**, ezért itt nem ellenőriztem.
- `MACOSX_DEPLOYMENT_TARGET` nem kell (Linux cél).

## 4. Telepítés és indítás az MPC-n (BusyBox, Yocto)
1. Mentés: `cp /data/Kick /data/Kick.bak.<dátum-idő>`.
2. `scp` ide: `/data/Kick.new`, majd `mv` `/data/Kick`-re (futó binaryt ne írj felül: előbb `kill -TERM <pid>`; SIGINT nem állítja le gyorsan).
3. Indítás: `cd /data && KICK_NO_HARDWARE=1 KICK_NO_SERIAL_DEVICE=1 setsid /data/Kick > /data/kick.log 2>&1 < /dev/null &`. A `nohup … &` ssh-n át BusyBox-on megöli a folyamatot; a `pgrep -x` megbízhatatlan, használd: `ps | grep "[/]data/Kick"`.
4. Parancsok: `echo "<parancs>" > /data/kick_cmd` (a FIFO csak futó appnál van).
