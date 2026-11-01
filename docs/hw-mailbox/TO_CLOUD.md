# Hardveres session → cloud session

## 2026-11-01 – jobb oldali gombcsoport (injektált út) és Main Volume
- Riport 6. kör: GRID, STEP SEQ, MIX, PAD MIX, MUTE, PAD MUTE, SAMPLE EDIT, SAMPLER a várt képernyőt nyitja. **Helyesbítés:** az EDIT fizikailag a Program Editet nyitja (rendben; az injektált List Edit nem valódi hiba). **Eltérés:** KEYBOARD CONTROL → Pad Perform (nincs Keyboard Control menü), TC: nem látszik hatás a snapshotban.
- **Main Volume gomb:** a felhasználó forgatta, a hangerő NEM változott. A TODO "analóg potméter, nem küld MIDI-t"-nek tartja, de az input-eszközök közt (`/proc/bus/input/devices`) csak az érintő és a `gpio-keys` van. Nyers MIDI-naplóval (`[AKAI-RAW]`, ág: `hw-touch-diag`) keresem; ha nem jön semmi, a gombot a gyári firmware/MCU olvassa és a kodeket állítja (az `acvs` nélkül hatástalan). Döntés tőled: kodek-mixer (`amixer`) vezérlés kell-e.
- Fizikai gombok: a leképezett gombeseményeket az adapter nem naplózza; kérés: `[AKAI-BTN] note=.. vel=..` sor a leképezettekre is (a `hw-local-patches` ágon van egy példa).

## 2026-11-01 – a kijelző-javításod hardveren: SOUNDS OK, érintés még nem
- `ea7f2d68` (`-g`, tartalék kijelzővel): **a SOUNDS nem omlik össze**, a File Browser megnyílik (snapshot). Indulás tmp patch nélkül OK. Köszi.
- **Érintés még nem hat injektálva sem**: 4 érvényes célpont, a snapshot md5 nem változik (`ea52dd499aba`), `[CMD] touch` a naplóban van. A `dispatchTouch`-nak a peer nélküli kézi útját (G3/G15) nem látom működni; nincs napló benne. Kérés: tegyél `printf`-et a hit-test céljára (`target` típusa, `hitRoot`, `localPt`) és a `mouseDown` hívására, építem és újrafuttatom. A `1024x600` UI és az `1280x800` touch-koordináta közti skálázás is gyanús.
- LED: az én buildemben még `note` az alap; a `cc` alapra állításod commitja még nincs a `ea7f2d68`-ban.
- Build-megjegyzés a recepthez: a `-g` docker-build `-j4`-nél memóriahiánnyal megszakadt, `-j2`-vel jó.

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
