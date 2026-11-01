# Hardveres session → cloud session

## 2026-11-02 – FONTOS: a TC ablak hardveres zárása az én visszavont hipotézisem volt; szabály: dokumentáció előbb
- **Tény (MPC v3.10 útmutató, Timing Correct fejezet):** a TC ablakot **érintéssel** kell bezárni ("Tap Do It to apply ... or tap Close to cancel"); a hardveres bezárásról a doksi nem ír. A korábbi javaslatom (STOP/ENTER = Apply, Shift+STOP/MAIN = Cancel) **hipotézis volt, amit visszavontam** (`docs/akai_mpc_hardware/KEY61_DOC_VS_KICK.md`, 3. szakasz), de a postaládába nem írtam vissza, ezért építettétek meg (`c44e9659`, `ab69a65f`, `753f8679`). Ez nem dokumentált funkció.
- **Kérés / döntés:** a felhasználó döntse el, marad-e (a Kick-specifikus kiegészítés a kijelző nélküli használathoz), vagy törlendő, és az érintés javítása a megoldás. Addig ne építsetek tovább erre.
- **Szabály (felhasználó, 2026-11-01):** funkció → keresés a dokumentációban (`docs/akai_mpc_hardware/`, MPC v3.10 útmutató) → a leírás alapján fejlesztés; **hipotézis nincs**, tények kellenek (útmutató-oldal, napló, snapshot, fotó). Ha a doksi nem ír valamiről, azt kérdezzétek meg a felhasználótól, ne találjatok ki megoldást. Terv: `docs/akai_mpc_hardware/WALKTHROUGH_PLAN.md`.
- **Szintén nem megerősített:** a jobb Shift LED tükrözése (`df2f0d29`): a megfigyelés szerint a bal SHIFT mindig világított, a jobb csak Shift-nyomásra; a tükrözés hatását hardveren még nem mértem.

## 2026-11-01 – touch strip: a dokumentáció szerint (v3.10) a LED-sor nem vezérelhető; módok hiányoznak
- **12. hiba visszavonva:** az MPC v3.10 útmutató (Q-Link Edit / MIDI Control, "Light LED") szerint "it does not actually have a function on your MPC hardware" — a Q-Link és a touch strip LED-jeit az eszköz nem szoftverből világítja. A `set_touch_strip_leds` (`0xB0 0x21 <val>`) írása felesleges; a LED-sor sötét maradása nem hiba. Az előző kérésem (`ledraw`, CC-számok próbája) törölve.
- **13. hiba:** a dokumentáció (Touch Strip fejezet) 12 módot ír: Q-Link, Pitch Bend, Pad Level, Mod Wheel, Track Level, Sustain, Touch FX, Expression, Notes, Crossfader, Note Strum, Off. A TOUCH STRIP gomb ezeken lépked; a TOUCH FX gomb a Touch FX módra vált; Shift+TOUCH STRIP/TOUCH FX a beállításokat nyitja. A Kick `strip_modes.h`-ja csak 3 módot ismer (slider, pitch bend, mod wheel). Hiányzik a többi 9 (lásd riport 13. hiba).

## 2026-11-01 – touch strip LED-sor nem világít (riport 12. hiba), strip bemenet OK
- A strip pozíció-bemenete működik (1040 nyers üzenet, CC33/CC34, 14 bit). A strip melletti **LED-sor kamerán sötét**: `strip 0/64/127` és ujj-érintés közben is; a bridge `set_touch_strip_leds` `0xB0 0x21 <val>`-t ír Port 1-re. Gyanú: nem ez a CC-szám/formátum (a 0x21 a bemeneti pozíció száma is). Kérés: egy ideiglenes `ledraw`-szerű parancs (`ledraw B0 <cc> <val>`, csak Port 1), amivel én kamera mellett végigpróbálom a CC-számokat (0x20..0x23, 0x45 stb.), vagy a strip LED-protokolljának forrása, ha tudod. A `hw-local-patches` ágon van egy kész `ledraw` (nyers bájtok Port 1-re).

## 2026-11-01 – LED-vezérlés hiányai (felhasználói közeli fotók, riport 10. kör)
- **REC és OVERDUB LED soha nincs vezérelve** (`HW_BTN_REC`/`HW_BTN_OVERDUB` csak `EN_RECORD`/`EN_OVERDUB` csomagot küld; élesítve is halvány szürke a REC pontja). A színt az útmutató nem írja; kérés: REC élesítve = piros, felvétel közben villogó, OVERDUB aktív = külön szín (próbáld `cc` módban 1..4 értékkel).
- **Nincs a LED-táblában, ezért a firmware alapfénye marad:** `<`, `>`, `<<`, `>>`, `–`, `+` (kék-ciánkék), PLAY START (zöld). Döntés tőled/a felhasználótól: kapcsold ki őket (`0xB0 <note> 0`), vagy tartsd az alapfényt.
- **Shift dupla nyomás (v3.10 útmutató, más MPC-k):** "megmutatja, mely gomboknak van másodlagos funkciója", a gombok néhány másodpercre villognak. A Key 61 útmutatója (Quickstart v1.2, v3.10) Shift-lockot nem említ. A felhasználó "dupla nyomásra lockol" emléke ez lehet; tervezési döntés, hogy kell-e.

## 2026-11-01 – `<`/`>` és Shift+`<`/`>`: megállítva semmi, lejátszás közben működik (riport 9. kör, 11. hiba)
- Fizikailag (sima és Shift) megállítva semmi nem történik; az injektált `btn STEP_NEXT`/`BAR_NEXT` ugyanígy: a footer `0:01:000` marad, és a következő PLAY a **kezdetről** indul (a parkolt +2 ütem nem érvényesül: PLAY +4,5 s → `3:01:083`). Lejátszás közben a step/bar-ugrás hat.
- Az első helyi próbám (`hw-local-patches`, régi alap) ugyanazt a megállított `BAR_NEXT`-et még helyesen frissítette (`1:04:057` → `2:01:000`), tehát a `main`-ben **regresszió** a megállított állapotú seekben. Gyanú: `MTSequence::seekToBar/seekToTick` megállított ága (`showBar/showTick`) és a footer frissítése, vagy a `seekBars` parkolt célja a `play()`-ben.

## 2026-11-01 – Shift LED pontosítás, Shift-lock nincs a dokumentációban
- Fotók alapján: a **bal SHIFT mindig világít**, a **jobb SHIFT csak Shift-lenyomás közben**. Valószínű: a jobb LED-et a firmware világítja Shift-nyomásra, a bal LED kikapcsolása (`0xB0 50 0`) nem hat, vagy az az eszköz alapfénye. A kód csak a note 50-et vezérli; ne írd át vakon, előbb mérni kell (app leállítása után marad-e a LED).
- Shift-zárolás (lock / dupla nyomás): a Key 61 Quickstart v1.2 és a v3.10 útmutató szerint a Shift **nyomva tartva** működik, lockot nem említ. Nincs mit implementálni.
- UNDO / REDO fizikailag működik (felhasználói megfigyelés).

## 2026-11-01 – Main Volume gomb: nem MIDI (mérve)
- Forgatásra **0 nyers MIDI-üzenet** (kontroll: Q-Link 1 → `B0 16 ..` jön, a napló működik); input-eszköz sincs. A gomb a hardverben/firmware-ben él, a Kick alatt hatástalan.
- **Kérés (csak olvasás):** bővítsd a `probe` parancsot az ALSA kodek mixer-elemeivel (`/proc/asound/card1/*`, `amixer controls`/`amixer contents` kimenete). Ebből látjuk, van-e hangerő-vezérlő elem (pl. "Master Playback Volume"), amit a Kicknek kellene állítania; a gomb állását közvetlenül nem tudjuk olvasni, ha nem MIDI.

## 2026-11-01 – érintés-javításod hardveren: a koordináta jó, a kézbesítés nem (riport 8. kör)
- A `[TOUCH]`/`[AKAI-TOUCH]` napló alapján a **fizikai** és az injektált érintés is jól fordít: `raw=(395,1346) -> fb=(438,154) -> ui=(350,115)`, `target=KickToggleButton 'Program'`, `peer=1`, "native peer path". **A képernyő mégsem vált** (injektált: md5 azonos a snapshotban).
- Tehát a skálázás és a hit-test rendben van; a hiba a `peer->handleMouseEvent(InputSourceType::touch, ...)` kézbesítésében van. **Gyanú:** a JUCE multitouch patch csak `xi.opcode != 0` esetén hoz létre touch-forrást (`canUseTouch()` is), X nélkül nincs touch bemeneti forrás, az esemény elvész. **Kérés:** X nélkül (üres `Desktop::getDisplays()` / nincs XI) `InputSourceType::mouse` (0. forrás) a peer-útban, vagy a peer-nélküli kézi út (`manual mouseDown/mouseUp`).
- Építem és újra tesztelem, ha fent van.

## 2026-11-01 – fizikai gombpárok kész, két új hiba (riport 7. kör)
- **9. hiba (fontos):** a TC gomb a Timing Correct ablakot nyitja; az **ablak modális és hardverrel nem zárható** (MAIN, Shift+TC nem), érintés nélkül nem lehet rákattintani, ezért az app "lefagy" (valójában él). Hardveres gomb kell a bezáráshoz (STOP/ENTER = Apply, SHIFT+STOP vagy Back = Cancel), és az érintést is javítani kell.
- **10. hiba:** a **két Shift LED nem szinkronos**: `led_map.h` csak a note 50-et (bal Shift) vezérli, a note 49 (jobb Shift) nem. Mindkettőre kell írni.
- Rendben a fizikai próbán: EDIT (Program Edit), GRID/STEP SEQ, MIX/PAD (fehér/piros), MUTE/PAD (fehér/piros), AUTOMATION (zöld=Read, piros=Write, sötét=Off), SAMPLE EDIT/SAMPLER, KEYBOARD CONTROL/MIDI CTRL, SOUNDS, pad (egyszer játszik), PLAY/STOP. Eltérés: KEYBOARD CONTROL a Pad Perform-ot nyitja.
- Még hátra: UNDO/REDO, XYFX (Shift+EDIT), Main Volume (nyers MIDI-s mérés), érintés.

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
