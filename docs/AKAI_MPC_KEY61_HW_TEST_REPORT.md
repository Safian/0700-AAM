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

## 2. kör (önállóan, a felhasználó nélkül)
Ág a helyi próbákkal: `hw-local-patches` (a `fork`-on; ideiglenes startup-patch, `seekSteps`, `[AKAI-BTN]`/`[AKAI-LED]` naplózás, `ledraw`/`aftertouch` ideiglenes parancsok). Build-recept: `docs/AKAI_MPC_KEY61_BUILD_RECIPE.md`.

### 7. hiba – spontán SIGSEGV üresjáratban (új)
A `KICK_AKAI_PORT0=0` módban futó példány (PID 29666, kb. 25 perc futás után, nem nyúlt hozzá senki) összeomlott: `/data/coredumps/Kick.2026-11-01T052810Z.core.zst`, `signal=11`. Backtrace: `Aura::Aura::processReplacing (this=0x3f2aa08, sampleFrames=0)` at `AudIO/AudIO/airwindows/src/Aura/AuraProc.cpp:78`, az audio szálban (a további frame-ek `??`, "corrupt stack?"). **Korlát:** a core az előző `-g` buildből való, a szimbólumokat az utána újraépített binaryval oldottam fel (az AudIO kód nem változott, a függvénynév valószínű, de a pontos sor nem biztos). Ok nem megállapítva; a `sampleFrames=0` hívás gyanús.

### (a) Indulási LED-bájtok (`initButtonLeds`, alapértelmezett mód, `[AKAI-LED]` napló)
A Port 1-re először mind a 36 LED-re `0x90 <note> 0x00` megy (led=0..35, `value=0`), majd STOP (note 81) `127`, bank A (note 35) `3`, MAIN (note 52) `127`. A felhasználó szerint ennek ellenére minden gomb világít, tehát a `0x90 <note> 0` nem kapcsolja ki a LED-eket (vagy a firmware alapból világít és ezt figyelmen kívül hagyja). Az alternatívák (`0x80 <note> 0`, `0x90` 1/2/3/4/127 értékekkel) bájtszinten előkészítve vannak (`echo "ledraw 90 52 03" > /data/kick_cmd`), de **a hatásuk nem ellenőrizhető**: nincs LED-visszaolvasás, a felhasználó alszik, a Macen csak az iPhone Continuity Camera van, azt nem használtam (nem a hardverre néz, és engedély nélkül nem fényképezek). **Fizikai próba kell.**

### (c) Pad-tartás ismétlése
A bridge aftertouch-kapuja (`m_pad_aftertouch_enabled`) kódban alapból `false`, a UI Hardware beállítása kapcsolja be. `echo "aftertouch 0|1" > /data/kick_cmd` ideiglenes parancs kész (`[CMD] aftertouch gate=0` a naplóban), de a nyomva tartott pad fizikai nyomás nélkül nem szimulálható. **Fizikai próba kell** (előbb kapu `0`, utána `1`, és figyelni, hogy a gyors ismétlés megszűnik-e).

## 3. kör (build `b8ee63a6`, saját patch nélkül, recept szerint)
Build: `docs/AKAI_MPC_KEY61_BUILD_RECIPE.md` (Release, `-DKICK_ENABLE_HARDWARE=ON -DKICK_TOUCH_SINGLE_TAP_LISTBOX=ON`, 16 MB). Indítás: `KICK_NO_HARDWARE=1 KICK_NO_SERIAL_DEVICE=1`. A build `1024x600`-ból skáláz `1280x800`-ra (`[BRIDGE] rendering 1024x600 to 1280x800`), az elrendezés ezért más, mint a korábbi buildeken (pl. a Program fül kb. x=440, y=145).

| Tétel | Állapot | Bizonyíték |
|---|---|---|
| Indulás kijelző nélkül, tmp patch nélkül | **OK** (az `179c99c9` guard működik) | folyamat fut, `[AKAI-CONTROLLER] LED mode: note (KICK_AKAI_LED_MODE=<unset>)`, `/data/kick_cmd` létrejött |
| SOUNDS gomb | **ROSSZ, összeomlik** (8. hiba) | lent |
| Injektált érintés | **ROSSZ** | `touch 440 145` (a Program fülön) után a két snapshot md5 azonos (`7a722d1fcbf3`); a korábbi `touch 351 109` az új elrendezésben nem találat volt, nem érvényes próba |
| `KICK_AKAI_LED_MODE` `cc` / `noteoff0` | **fizikai próba kell** (kamera reggel) | `note` mód naplózva; a másik két mód hatása csak szemmel/kamerával ellenőrizhető |
| `Aura::processReplacing` (7. hiba) újrateszt | **nem eldönthető** | az új buildnek még csak percnyi futása van, a korábbi összeomlás ~25 perc üresjárat után jött |

### 8. hiba – SOUNDS gomb összeomlás kijelző nélkül (`b8ee63a6` is)
`btn SOUNDS` (és a fizikai SOUNDS, note 121) után SIGSEGV. Backtrace (Release, szimbólum nélküli frame-ek, de a függvénynevek olvashatók):
`juce::Component::getParentMonitorArea() const` ← `setBoundsRelative(float×4)` ← `ui::FileLeftEntry::resized()` ← `FileBrowser::setupProvider()` lambda ← `KickTableView::refresh()` ← `FileBrowser::visibilityChanged()` ← `Component::setVisible` ← `KickHome::handleAsyncUpdate()`. Ugyanaz a gyökérok, mint az indulásnál: üres `Desktop::getDisplays()`, null dereferencia. Az `179c99c9` guard csak a `Main.cpp`-t védi. Régebbi `-g` buildnél ugyanez: `kick_fileBrowser.cpp:1650`.

### LED-megfigyelés (`KICK_AKAI_LED_MODE=note`, alapértelmezett) – **javítva**
Az app indításkor `0x90 <note> 0`-t küld mind a 36 LED-re, majd bekapcsolja a STOP-ot (127), az A bankot (3) és a MAIN-t (127) (`[AKAI-LED]` napló). A felhasználó megfigyelései változtak: korábban "mind világít", a 3. körben STOP, MAIN, SOUNDS; a végső pontosítása szerint **csak a STOP világított, a többi nem** (a korábbi "mind világít" téves volt). A fotók csillogása (üveg-/lakkvisszaverődés) nem megbízható bizonyíték, ezért **ebből a körből nem vonok le LED-következtetést**; a korábbi "FULL LEVEL, TAP TEMPO, TOUCH FX, SOUNDS, UNDO is világít" állítás visszavonva. A halvány színes gombok (PLAY zöld, REC piros stb.) az eszköz alapfénye. A `0x90 <note> 0` hatása és az alternatívák (`KICK_AKAI_LED_MODE`) a felhasználó reggeli szemrevételezésére várnak.

Próba reggelre (az MPC-n a telepített build futásidőben kapcsol, nincs külön build): leállítás `kill -TERM <pid>`, indítás `cd /data && KICK_AKAI_LED_MODE=<note|cc|noteoff0> KICK_NO_HARDWARE=1 KICK_NO_SERIAL_DEVICE=1 setsid /data/Kick > /data/kick.log 2>&1 < /dev/null &`; a naplóban `LED mode: <mód>`, a STOP LED-nek indításkor világítania, a többinek nem szabad. Utána `echo "snapshot /data/x.ppm" > /data/kick_cmd` felesleges, a LED-et csak szem látja.

### LED-protokoll: a kamerás mérés eredménye (4. kör)
Photo Booth kamerával, `btn PLAY` utáni LED-változással (STOP-nak ki, PLAY-nek be kell kapcsolnia): **`KICK_AKAI_LED_MODE=cc` működik; `note` (alap) és `noteoff0` nem** (a STOP nem aludt ki). A gombok LED-jét tehát `0xB0 <gomb-note> <érték>` vezérli. A `note` módban a gombok a firmware állapotában ragadnak (a korábbi "minden világít" ebből). Részletek és színek: `docs/akai_mpc_hardware/KEY61_LED_COLOURS.md`. **Javaslat: az alap legyen `cc`.**

## 5. kör (`safian/main` `ea7f2d68`, `-g` build, tartalék kijelzővel)
Build: `-g`, `-DKICK_TOUCH_SINGLE_TAP_LISTBOX=ON`; a docker-build `-j4`-nél memóriahiány miatt megszakadt (`Killed signal terminated program cc1plus`, `audio_core.cpp`), `-j2`-vel lefutott. Indítás: `KICK_NO_HARDWARE=1 KICK_NO_SERIAL_DEVICE=1`.

| Tétel | Állapot | Bizonyíték |
|---|---|---|
| Indulás kijelző nélkül | OK | `[HAL] KickHardwareBridge fully operational (Display 1280x800 @ 30FPS, Touch, ControlSurface, CmdFIFO)` |
| 8. hiba: SOUNDS gomb | **JAVÍTVA** | `btn SOUNDS` után az app él (pid 24043), nincs új coredump, a snapshot a File Browsert mutatja |
| `btn MAIN` | OK | a snapshot md5 változik (`ba5adb80ea7f` → `ea52dd499aba`), a Home/Track képernyő jön |
| Injektált érintés | **MÉG ROSSZ** | négy érvényes célpont (Program fül, Sequence fül, Mix ikon, Program fül skálázott koordinátával `352 116`): a snapshot md5 mindig `ea52dd499aba`, nem változik; a naplóban `[CMD] touch ...` megjelenik |
| LED alap mód | még `note` (a `cc` alapra állítás ebben a buildben nincs) | `[AKAI-CONTROLLER] LED mode: note (KICK_AKAI_LED_MODE=<unset>)` |

Az érintésnél a fizikai próba még nem történt meg (a felhasználó nincs otthon). Gyanú: a `1024x600`-as UI (`[BRIDGE] rendering 1024x600 to 1280x800`) és a touch `1280x800` koordinátái között nincs skálázás a `dispatchTouch`-ban, de a skálázott koordináta sem hatott, ezért a hit-test vagy a peer nélküli kézi `mouseDown` útja is gyanús. Ellenőrizni kell, hogy a kézi eseményút egyáltalán eljut-e a `KickButton`-ig (napló a `dispatchTouch`-ban).

## 6. kör – a jobb oldali gombcsoport (EDIT/XYFX … UNDO/REDO), injektált (szoftveres) út
`btn <NÉV>` + `snapshot` (`safian/main` `ea7f2d68`, `-g` build). A fizikai nyomást a felhasználó külön végigpróbálta, de a leképezett gombokat az adapter nem naplózza (csak az unmapped-et), ezért az a naplóból nem igazolható; a Shift-kombinációk (XYFX, MIDI CTRL, ON/OFF, REDO, OFF) nem injektálhatók.

| Gomb (btn név) | Képernyő a snapshotban | Az útmutató szerint | Állapot |
|---|---|---|---|
| EDIT (`PROG_EDIT`) | injektálva: List Edit; **fizikai nyomásra: Program Edit** (felhasználói fotó) | Program Edit Mode | **OK fizikailag**; az injektált és a fizikai út eltér, ok nem tisztázott (elavult snapshot?) |
| GRID (`GRID`) | Grid View | Grid View | OK |
| STEP SEQ (`STEP_SEQ`) | Step Sequencer | Step Sequencer | OK |
| MIX (`TRACK_MIX`) | Track Mixer | Track Mixer | OK |
| PAD MIX (`PAD_MIX`) | Pad Mixer | Pad Mixer | OK |
| MUTE (`TRACK_MUTE`) | Track Mute | Track Mute | OK |
| PAD MUTE (`PAD_MUTE`) | Pad Mute | Pad Mute | OK |
| KEYBOARD CONTROL (`KEYBOARD_CONTROL`) | **Pad Notes (Pad Perform)** | Keyboard Control menü (billentyűzet és kerekek: sebesség, útvonal, MIDI, tartományok) | **eltér** |
| SAMPLE EDIT (`SAMPLE_EDIT`) | Sample Editor | Sample Edit Mode | OK |
| SAMPLER (`SAMPLER`) | Sampler | Sampler | OK |
| TIMING CORRECT (`TC`) | List Edit marad (md5 = UNDO utáni állapot) | Timing Correct ablak | **nem látható hatás** |
| UNDO (`UNDO`) | nincs képernyőváltás | Undo | OK (hatás nem ellenőrizve) |

Eltérések a cloud sessionnek: az EDIT fizikailag a Program Editet nyitja (**helyes**; az injektált List Edit eltérés nem valódi hiba); a KEYBOARD CONTROL a Pad Perform-ot nyitja, a Kickben nincs "Keyboard Control" menü; a TC gombnyomás hatása a snapshotban nem látszik (a Timing Correct ablak nem nyílik meg, vagy a globális TC csak a LED-et váltja).

## 7. kör – fizikai gombpárok (felhasználói fotók), `ea7f2d68` `-g` build, `cc` LED-mód
A felhasználó a fizikai gombokat (alap és Shift) megnyomta, és fotót küldött; az eredmény:

| Gomb / Shift-pár | Képernyő | LED (fotóról) | Állapot |
|---|---|---|---|
| EDIT | Program Edit | EDIT felirat világít | OK (az injektált List Edit nem valódi hiba) |
| GRID / +Shift (STEP SEQ) | Grid View / Step Sequencer | GRID világít mindkettőnél | OK; a halvány/erős különbség a fotón nem látszik |
| MIX / +Shift (PAD) | Track Mixer / Pad Mixer | **fehér / piros** | OK |
| MUTE / +Shift (PAD) | Track Mute / Pad Mute | **fehér / piros** | OK |
| AUTOMATION / +Shift (OFF) | – | **zöld = Read, piros = Write, sötét = Off** | OK a kód szerint; a fizikai váltogatást a felhasználó megerősítette |
| KEYBOARD CONTROL / +Shift (MIDI CTRL) | Pad Notes (Pad Perform) / MIDI Control | rózsaszínes-piros | az első **eltér** az útmutatótól (Keyboard Control menü), a második OK |
| SAMPLE EDIT / +Shift (SAMPLER) | Sample Editor / Sampler | piros árnyalat / fehér | OK; a két szín a fotón a kódhoz képest fordítottnak látszik (nem biztos) |
| TC / +Shift (ON/OFF) | Timing Correct ablak nyílik | – | **9. hiba** |
| SOUNDS | File Browser | – | OK; Shift+MENU (BROWSE) is a Browsert nyitja |
| Pad nyomva tartva | egyszer játssza | – | OK (5. hiba javítva) |
| PLAY / STOP | – | PLAY zöld, STOP kialszik / fehér | OK |

### 9. hiba – Timing Correct ablak modális, hardverrel nem zárható (érintés nélkül "lefagy")
A TC gomb megnyitja a Timing Correct ablakot. Mivel az érintés nem működik, a Cancel, Apply és X nem érhető el, és a hardveres gombok (MAIN, Shift+TC / ON/OFF) nem zárják be (a snapshot szerint az ablak MAIN után is nyitva marad, a háttér vált). Az app közben él (renderel, a `kick_cmd` válaszol, nincs coredump), a felhasználó mégis "lefagyásként" éli meg. Javítás: hardveres gomb zárja be az ablakot (pl. STOP/ENTER = Apply, SHIFT+STOP vagy Back = Cancel), és az érintés működjön. Újraindítással oldottam meg.

### 10. hiba – a két Shift LED nem szinkronos (pontosítva a felhasználó fotói alapján)
Megfigyelés: a **bal SHIFT mindig világít**, a **jobb SHIFT csak akkor, ha valamelyik Shift le van nyomva**; a működés mindkettőnél rendben. A Key 61 útmutató szerint a Shift **nyomva tartva** működik ("Press and hold this button ..."), Shift-zárolást (lock / dupla nyomás) sem a Quickstart v1.2, sem a v3.10 útmutató nem említ. Lehetséges ok: a jobb Shift LED-et a firmware világítja (nem a Kick), a bal LED kikapcsolása nem hat. Az eredeti leírás:
`led_map.h`: `HW_LED_SHIFT → note 50` ("left Shift; the right Shift (49) is not driven"). A bal Shift világít, a jobb nem; a felhasználó szerint a két Shift világítása nem szinkronos, a működés viszont igen. Javítás: a LED-et mindkét notera (49 és 50) írni.

### Main Volume
A forgatásra a hangerő nem változott; a TODO szerint analóg, MIDI-t nem küld, input-eszköz sincs. A nyers MIDI-s mérés (`[AKAI-RAW]`, ág: `hw-touch-diag`) a build után következik.

## 8. kör – érintés-javítás (`fd2480de`) hardveren, `[TOUCH]` napló
Build: `safian/main` `fd2480de` + `[AKAI-RAW]` napló (ág `hw-diag-raw`, csak az utóbbi tér el). A `[TOUCH] init: framebuffer touch space 1280x800 -> UI 1024x600` sor megjelenik.

| Út | Eredmény |
|---|---|
| Injektált `touch 440 145` (framebuffer) | `ui=(352,108)`, `target=KickToggleButton 'Program' 133x44`, `peer=1`, "native peer path"; **a snapshot nem változik** (md5 `eec1c1112cb6` előtte és utána) |
| Fizikai koppintás a Program fülre | `[AKAI-TOUCH] down slot=0 raw=(395,1346) range=2048x2048 -> fb=(438,154)`, `ui=(350,115)`, `target=KickToggleButton 'Program'`, "native peer path"; **a képernyő nem vált** |

A koordináta-átvitel (nyers → framebuffer → UI) és a hit-test **helyes**; a hiba az esemény kézbesítésében van. Mindkét esetben a "native peer path" fut (`peer=1`), tehát a `peer->handleMouseEvent(InputSourceType::touch, ...)` hívás nem jut el a gombig. Gyanú: a JUCE multitouch patch (`juce_MouseInputSource`/`MouseInputSourceList`) csak `xi.opcode != 0` esetén hoz létre touch-forrást (`canUseTouch()` is `xi.opcode != 0`), X nélkül nincs touch bemeneti forrás, ezért az `InputSourceType::touch` esemény elvész. Javaslat: X nélkül `InputSourceType::mouse` (0. forrás) használata a peer-útban, vagy a peer-nélküli kézi útra váltás.

### Main Volume gomb – mérés (nyers MIDI, `[AKAI-RAW]`)
A gomb forgatására (oda-vissza) **0 nyers MIDI-üzenet** érkezett (sem Port 0, 1, 2 felől). Kontroll: a Q-Link 1 forgatása `[AKAI-RAW] port=1 status=B0 d0=16 d1=1` (relatív CC16), 108 sor, tehát a naplózás működik. A gomb input-eszköz sem (`/proc/bus/input/devices`: csak ILI2117 és `gpio-keys`). Következtetés: a Main Volume **nem MIDI és nem input-esemény**; a hangerőt a hardver/firmware kezeli (kodek vagy analóg fokozat), a Kick alatt (a gyári `acvs` nélkül) hatástalan, a felhasználó szerint a hangerő a gomb forgatására nem változik. Következő lépés (csak olvasás): a `probe` parancs bővítése az ALSA kodek mixer-elemeinek listázásával (`/proc/asound/card1/*`, `amixer controls`), hogy kiderüljön, van-e hangerő-vezérlő elem, amit a Kicknek kell állítania.

## 9. kör – `<`/`>` és Shift+`<`/`>` (fizikai és injektált), `fd2480de` + `[AKAI-RAW]`
A felhasználó fizikailag megnyomta a `<` és `>` gombot sima és Shift-es formában, **megállított állapotban** (nem indított lejátszást): semmi nem történt. Az injektált út ugyanezt mutatja a footer BAR:BEAT:TICK kijelzőjén:

| Lépés (injektált) | BAR:BEAT:TICK |
|---|---|
| kiinduló (megállítva) | `0:01:000` |
| 2× `STEP_NEXT` | `0:01:000` |
| `BAR_NEXT`, `STEP_PREV` | `0:01:000` |
| Home: 2× `BAR_NEXT` | `0:01:000` |
| `PLAY` (+4,5 s) | `3:01:083` (a kezdetről indult: 4,5 s ≈ 2,25 ütem; a parkolt +2 ütem **nem érvényesült**) |
| lejátszás közben `STEP_NEXT` | változik (a pozíció ugrik) |

**11. hiba:** megállított állapotban a seek (`<`, `>`, `<<`, `>>`, Shift+`<`/`>`) sem megjelenik a kijelzőn, sem a következő PLAY-nél nem érvényesül; lejátszás közben működik. Az első helyi próbánál (`hw-local-patches`, `4605b75e` alap) a megállított `BAR_NEXT` még frissítette a kijelzőt (`1:04:057` → `2:01:000`; a rawmidi-injektált `>` is `1:01:048`), tehát a `main`-ben valószínűleg regresszió van a megállított állapotú seekben (`MTSequence::showBar/showTick` és a footer frissítése, vagy a `seekBars` parkolt célja).

## 10. kör – LED-vezérlés hiányai (felhasználói közeli fotók + kód), `cc` LED-mód

| Gomb | Fotón | Kód | Megjegyzés |
|---|---|---|---|
| STOP | fehér-kékes, erős | vezérelve (`HW_LED_STOP`) | OK |
| PLAY | zöld (lejátszáskor), különben halvány | vezérelve (`HW_LED_PLAY`) | OK |
| **REC** | halvány szürke pont, akkor is, ha a REC élesítve van (`btn REC` háromszor, a harmadik után) | **soha nincs vezérelve** (a `HW_BTN_REC` csak `EN_RECORD` csomagot küld) | **hiányzik** |
| **OVERDUB** | halvány szürke | **soha nincs vezérelve** (`EN_OVERDUB` csomag) | **hiányzik** |
| `<`, `>`, `<<`, `>>`, `–`, `+` | kék-ciánkék, mindig | nincs a LED-táblában (`led_map.h`) | a firmware alapfénye; az app nem kapcsolja ki |
| PLAY START | zöld, mindig | nincs a LED-táblában | a firmware alapfénye |
| Q-Link 1 LED | piros | – | a firmware alapfénye |
| SHIFT (bal / jobb) | bal mindig, jobb csak Shift-nyomásra | csak note 50 | lásd 10. hiba |

Az útmutató (Quickstart v1.2, v3.10) a REC/OVERDUB LED-színt nem írja le (a "lit red/white/amber" csak a Tap Tempónál szerepel), ezért a színeket a cloud sessionnek az eszközön (`KICK_AKAI_LED_MODE=cc`, érték 1..4) kell kipróbálnia. A Shift: az útmutató szerint nyomva tartva működik; a Shift **dupla nyomása** (v3.10, más MPC-knél) "megmutatja, mely gomboknak van másodlagos funkciója: a gombok néhány másodpercre villognak".

## 11. kör – bal oldal: LED-ek kamerával (Photo Booth) és touch strip (nyers MIDI)
A kamera a bal oldalra (kerekek, bal gombcsoport, pad bank, Main Volume) nézett; a `btn <NÉV>` után a zoomolt kép alapján:

| Gomb | Eredmény a kamerán |
|---|---|
| Pad bank A/E, B/F, C/G, D/H | `BANK_A..D`: mindig csak a kiválasztott világít, **fehér**, a C/G és D/H kicsit zöldesebb (kamera-színárnyalat) — OK |
| TAP TEMPO (GLOBAL) | világít, kékes-fehér |
| LATCH | világít, kékes-fehér |
| TOUCH FX (CONFIG) | világít, kékes |
| TOUCH STRIP (CONFIG) | világít, kékes |
| OCT + | világít, zöldes-fehér |
| Oktáv-kijelző (−3 … +3) | két piros pont (−1 és 0 körül) alapból, a firmware alapfénye |
| Kerekek (pitch/mod) | kék háttérvilágítás, mindig |

**Touch strip** (felhasználó: ujj fel-le csúsztatva): a nyers MIDI-naplóban **1040 új üzenet** (`[AKAI-RAW]` 108 → 1148): `port=1 status=B0 d0=33` (CC33, pozíció MSB) és `d0=34` (CC34, LSB) párokban, pozíció `d1=0..99` tartományban, felemeléskor `d0=33 d1=0`. A kód ezt kezeli (`on_touch_strip`, `on_touch_strip_fine`); a strip funkciója a felhasználó szerint működik. A touch strip LED-sora (a strip melletti ladder) nem lett külön megfigyelve.

### 12. hiba – a touch strip LED-sora nem világít
A strip melletti kis LED-sor a kamerán sötét maradt: `strip 0`, `strip 64`, `strip 127` (a bridge `set_touch_strip_leds(val)` → `0xB0 0x21 <val>`, Port 1) és érintés (a felhasználó ujja a stripen) közben is. A strip pozíció-bemenete működik (1040 nyers üzenet CC33/CC34). Gyanú: az Akai nem a CC33-ra (0x21) várja a LED-sor írását (a CC33 a bemeneti pozíció száma is), vagy a `cc` LED-mód `0xB0 <note> <érték>` konvenciójához más szám tartozik; az útmutató a strip LED-sorának vezérlését nem írja le. Mérés: a CC-szám és az érték-tartomány kipróbálása (pl. 0xB0 0x20..0x23, 0x45, érték 0..127, vagy a strip-módonként eltérő) a `ledraw` típusú parancs bővítésével kamera mellett.
