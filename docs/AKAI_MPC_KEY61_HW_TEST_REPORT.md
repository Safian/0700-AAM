# Akai MPC Key 61 – hardveres próba riport (2026-11-01)

Eszköz: MPC Key 61, `192.168.0.30`. Alap: `fork/safian/main` `4605b75e` (a SessionStart hook commitja) + **ideiglenes, nem commitolt** teszt-patch (lásd 1. hiba). Forráskódot ebben a körben nem javítottam; a hibák naplóbizonyítékkal itt vannak.

## Módszer és eltérések
- ARMv7 build a `kick-armv7-builder` image-ben. A korábbi build-recept **nincs sehol rögzítve**, ezért az image-ből állítottam össze: toolchain-fájl (`-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard`), `-DKICK_ENABLE_HARDWARE=ON`, `-g`.
- Telepítés: csak `/data/Kick`, előtte mentés: `/data/Kick.bak.20261101-0412`. I2C/CV-re nem írtam.
- Indítás: `KICK_NO_HARDWARE=1 KICK_NO_SERIAL_DEVICE=1`, külön futás `KICK_AKAI_PORT0=0`-val.
- Injektálás: `/data/kick_cmd` (`btn`, `pad`, `key`, `strip`, `jog`, `snapshot`, `rawmidi`, `touch`, `probe`). A `btn` azonnal pressel és release-el, ezért kombináció (SHIFT+STOP, LOCATE+STEP) így nem injektálható.

## Eredmény tételenként
| Tétel | Állapot | Bizonyíték |
|---|---|---|
| Indulás (HEAD) | **ROSSZ** | 1. hiba |
| Transport (PLAY/STOP) injektálva | OK | `[CMD] btn PLAY/STOP`, app fut tovább |
| `BAR_NEXT`/`BAR_PREV` injektálva | OK | `1:04:057` → `2:01:000` (snapshot) |
| LOCATE+STEP, SHIFT+STOP | **fizikai próba kell** | btn nem kombinálható |
| ARP/LATCH, TC, AUTOMATION, strip módok, METRONOME, bank, UNDO, GRID/MAIN, FULL_LEVEL, NOTE_REPEAT, OCT | OK (elfogadva, app stabil) | `[CMD]` sorok, nincs összeomlás; hatás nincs külön ellenőrizve |
| Pad / billentyű injektálva | OK (elfogadva) | `[CMD] pad 3 vel=100`, `key 64 vel=90` |
| Érintő | **nem működik** | 2. hiba; EVIOCGABS: `range 2048x2048` (`[AKAI-TOUCH]`) |
| Gomb-LED | **ROSSZ** | 4. hiba |
| Pad RGB, RECALL, MIDI program PAN→CC10 | **nem tesztelt** | sem fizikai, sem injektált próba nem készült |
| `KICK_AKAI_PORT0=0` összehasonlítás | részleges | a log igazolja a letiltást (`DISABLED`); pad/billentyű duplázódás **nem mérve** |
| Pitch bend / sustain | **ROSSZ** (felhasználói megfigyelés) | 6. hiba |

## Hibák (javítás a cloud sessionre vár)
1. **KRITIKUS – HEAD összeomlik induláskor kijelző nélkül.** Core + `-g` build: `AudioEngineApplication::initialise` (`Main.cpp:248`) → `juce::Desktop::setKioskModeComponent` → `Desktop::setKioskComponent` (`juce_Windowing_linux.cpp:609`, `getDisplayForRect(...)->totalArea`): a kijelzőlista üres (nincs X), null dereferencia. A régi (103 commit előtti) binary nem omlik össze. HARDWARE=OFF build és a `KICK_NO_HARDWARE`/`KICK_NO_SERIAL_DEVICE` is `rc=139`. A próbához ideiglenes patch kellett: kijelző nélkül `setBounds(0,0,1280,800)`, `setVisible(false)`, nincs kiosk (nem commitolva).
2. **Érintő nem reagál.** Fizikailag sem, és a bridge `touch 351 109` parancsa után sem változott a képernyő (két snapshot md5 azonos). Valószínű ok az 1. hiba patch-e (láthatatlan ablak, nincs peer), ezért a buillel nem választható szét; az 1. hiba javítása után újra kell tesztelni.
3. **`<` / `>` rossz funkció.** `akai_map.h`: note 68/69 → `STEP_PREV/NEXT`, ami `EN_MINUS/EN_PLUS`-t küld. Az Akai leírás szerint egy lépést mozgatja az audio pointert; Locate+ előző/következő esemény (a `<<`/`>>` egy ütem, Locate+ eleje/vége). Helyi, nem pusholt próba (`seekSteps`, TC `gridTicks`): injektált `btn STEP_NEXT` és `rawmidi 1 90 45 7F` is működik megállítva (2 lépés = +48 tick, `1:01:048`), de a **fizikai gomb megállítva nem reagál**, play után igen. A leképezett gombeseményeket az adapter nem naplózza (csak az unmapped-et), ezért nem tudni, megérkezik-e.
4. **Minden gomb-LED folyamatosan világít** (PLAY, STOP és a többi), `KICK_AKAI_PORT0=0` mellett is → nem a Port 0 init SysEx az ok. Gyanú: a LED-ki írás (`0x90 <note> 0`) hatástalan, vagy `initButtonLeds` nem éri el az eszközt.
5. **Pad nyomva tartva gyorsan ismétli a hangot.** Gyanú: poly aftertouch bekapcsolva → `on_pad_aftertouch` `EN_PAD_STATE BTN_PRESSED` újraindítja. Nem igazolt.
6. **Pitch bend / mod wheel / pedál megugrasztja az első track hangerejét** (felhasználói megfigyelés; nem tisztázott, a Level gomb vagy a mérő). A bridge élő MIDI-t küld ch1-re (pitchWheel, CC1, CC11, CC64).

## Nyitott
Fizikai próba kell: a 3. és 4. hiba pontos oka (gomb-naplózás kell), LOCATE+STEP, SHIFT+STOP, LED két színe, pad RGB, RECALL, PAN→CC10, érintő skálázás.
