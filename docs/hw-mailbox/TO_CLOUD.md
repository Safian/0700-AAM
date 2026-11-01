# Hardveres session → cloud session

## 2026-11-01 – LED-protokoll eldőlt: `cc` (kamerával mérve)
- A `KICK_AKAI_LED_MODE` próba kamerával (Photo Booth) megtörtént az MPC-n (`b8ee63a6`): **`cc` működik**, `note` és `noteoff0` nem (PLAY-nél a STOP nem alszik ki). Javaslat: az alap legyen `cc`, a másik két ág törölhető. Részletek, színek: `docs/akai_mpc_hardware/KEY61_LED_COLOURS.md`; riport 4. kör.
- Új könyvtár: `docs/akai_mpc_hardware/` (hivatalos Akai útmutatók forráslinkjei és SHA-256, mért viselkedés). A PDF-ek helyben vannak, nincsenek commitolva (szerzői jog).
- Továbbra is nyitott: SOUNDS-összeomlás (a te általános javításodra vár), érintés, `<`/`>`.

## 2026-11-01 – nyitott tételek (az MPC-n `safian/main` `b8ee63a6` fut)
- 8. hiba: SOUNDS gomb összeomlás kijelző nélkül (`getParentMonitorArea` ← `setBoundsRelative` ← `FileLeftEntry::resized` ← `FileBrowser::visibilityChanged`). A te általános kijelző-javításod (kijelző nélkül tartalék Desktop-display) még nincs fent; ha bent van, szólj, építem és újratesztelem.
- Érintés nem működik injektálva sem (`touch` után azonos snapshot).
- `<`/`>` (note 68/69) funkciója: az Akai leírás szerint egy lépés; a fizikai gomb megállítva nem reagált. A `[AKAI-BTN]` naplózás csak a `hw-local-patches` ágon van.
- LED: a felhasználó szerint csak a STOP világít indítás után. `KICK_AKAI_LED_MODE` próba reggel (felhasználó nézi szemmel, kamera nincs).
- Aura (`sampleFrames=0`) újrateszt: hosszabb futás kell az új buildnél.
