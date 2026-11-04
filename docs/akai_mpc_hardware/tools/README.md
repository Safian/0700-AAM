# Eszközök (MPC Key 61 hardveres munka)

| Fájl | Mire való |
|---|---|
| `tkgl_bootstrap` | POSIX sh bootstrap szkript az MPC-hez: automatikus atomi telepítés a belső `/data` tárhelyre, verzió-összehasonlítás, indítás és fallback a gyári MPC-re. |
| `make_pendrive_package.sh` | Letölthető pendrive kiadási csomag előállítása (`dist/kick-mpc-key61-<verzió>.zip` + SHA256 ellenőrzőösszeg fájl). |
| `uninstall_kick.sh` | Kick eltávolító szkript: visszaállítja a gépet a gyári állapotba (`/data/Kick*` törlése, felhasználói projektek megőrzése). |
| `test_tkgl_bootstrap.sh` | Offline tesztcsomag: a bootstrap, a csomagoló és az uninstaller funkcióinak szimulált és automatizált tesztelése (linting, telepítés, frissítés, visszalépés, korrupció-védelem). |
| `build-armv7.sh`, `armv7.cmake` | ARMv7 build Dockerben (`W=<checkout> CACHE=<cache> [DEBUG=1] bash build-armv7.sh`); recept: [../../AKAI_MPC_KEY61_BUILD_RECIPE.md](../../AKAI_MPC_KEY61_BUILD_RECIPE.md). `JOBS=2` az alap (4-nél a konténer memóriája elfogy). |
| `pdf2txt.swift` | a teljes PDF szöveggé (macOS PDFKit; `swiftc -O pdf2txt.swift -o pdf2txt`) |
| `pdfpages.swift` | a PDF oldalanként (`<mappa>/NNN.txt`); a v3.10 útmutatónál a PDF-oldalszám egyezik a nyomtatott oldalszámmal |
| `pilot_wpa.py` | a Key 61 vezérlő-leírás (45 tétel) JSON-sorokká helyi modellel (lásd [../LOCAL_MODEL_PILOT.md](../LOCAL_MODEL_PILOT.md)) |
| `walk.py` | egy oldaltartomány állítás-lap sorokká helyi modellel: `python walk.py <be.txt> <ki.json> <WP-betű> <első-sorszám>` (MLX, `mlx-community/Qwen3.6-35B-A3B-8bit`, `enable_thinking=False`) |

A `walkthrough/WP-B.raw.json` a **nyers, nem ellenőrzött** helyi-modell kimenet a Locate/TC/Metronome oldalakra (71–74): az `oldal` mező sok sorban `null`, a sorokat a Key 61-specifikusságra és a Kick-megfelelőre még át kell nézni.
