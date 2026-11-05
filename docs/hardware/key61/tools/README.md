# Eszközök (MPC Key 61 hardveres munka)

| Fájl | Mire való |
|---|---|
| `tkgl_bootstrap` | POSIX sh bootstrap szkript az MPC-hez: automatikus atomi telepítés a belső `/data` tárhelyre, verzió-összehasonlítás, indítás és fallback a gyári MPC-re. |
| `make_pendrive_package.sh` | Letölthető pendrive kiadási csomag előállítása (`dist/kick-mpc-key61-<verzió>.zip` + SHA256 ellenőrzőösszeg fájl). |
| `uninstall_kick.sh` | Kick eltávolító szkript: visszaállítja a gépet a gyári állapotba (`/data/Kick*` törlése, felhasználói projektek megőrzése). |
| `test_tkgl_bootstrap.sh` | Offline tesztcsomag: a bootstrap, a csomagoló és az uninstaller funkcióinak szimulált és automatizált tesztelése (linting, telepítés, frissítés, visszalépés, korrupció-védelem). |
| `build-armv7.sh`, `armv7.cmake` | ARMv7 build Dockerben (`W=<checkout> CACHE=<cache> [DEBUG=1] bash build-armv7.sh`); recept: [../build-recipe.md](../build-recipe.md). `JOBS=2` az alap (4-nél a konténer memóriája elfogy). |
| `pdf2txt.swift` | a teljes PDF szöveggé (macOS PDFKit; `swiftc -O pdf2txt.swift -o pdf2txt`) |
| `pdfpages.swift` | a PDF oldalanként (`<mappa>/NNN.txt`); a v3.10 útmutatónál a PDF-oldalszám egyezik a nyomtatott oldalszámmal |
| `pilot_wpa.py` | a Key 61 vezérlő-leírás (45 tétel) JSON-sorokká helyi modellel (lásd [../local-model-pilot.md](../local-model-pilot.md)) |
| `walk.py` | egy oldaltartomány állítás-lap sorokká helyi modellel: `python walk.py <be.txt> <ki.json> <WP-betű> <első-sorszám>` (MLX, `mlx-community/Qwen3.6-35B-A3B-8bit`, `enable_thinking=False`) |

## Bekapcsoló gomb és leállítás (R-12)
- A Kick a gombot maga kezeli: a hardver-híd mellett egy szál olvassa a `/dev/input/eventN` eszközt (KEY_POWER, value 1), a rövid
  nyomás a "Power" ablakot nyitja (Shut Down / Restart / Cancel); ugyanez a Menü "Power" csempéje (a gomb nélkül is mindig működik).
  Az eszköz a `/proc/bus/input/devices`-ből derül ki (`Kick/apps/kick/hardware/input_devices.h`); minden bemeneti eszköz felkerül a
  `kick.log`-ba (`[POWERKEY] input '...' ... key-power=.. score=..`). Felülírás: `KICK_POWER_KEY_DEVICE=/dev/input/eventN[,..]`.
- Kettős kezelés ellen (logind HandlePowerKey) a `tkgl_bootstrap` a Kicket `systemd-inhibit --what=handle-power-key`-jel indítja,
  ha az inhibit próbafutása sikerül (a gyári launcher is ezt teszi az MPC-vel); különben a régi sima `exec`. `KICK_NO_POWER_INHIBIT=1`
  kikapcsolja. D-Bus-os `org.freedesktop.login1.Inhibit('handle-power-key', ...)` lock a Kickből: nincs megvalósítva (D-Bus függőség),
  a wrapper ugyanazt adja.
- Leállítás: felvétel-kérdés, transzport/looper/felvevő leállítás, mentetlen-kérdés (Save / Don't save / Cancel), `closeProject`
  (ment), `sync`, `poweroff` / `reboot` (`kick_power_logic.h`, host-teszt: `PowerLogicTests.cpp`). "Restart Kick" (csak az alkalmazás)
  nincs: semmi nem indítja újra a Kicket, kilépés után az MPC a gyári appra / holt képernyőre esne.
