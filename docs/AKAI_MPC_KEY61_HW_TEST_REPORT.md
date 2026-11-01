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

### LED-megfigyelés (felhasználói fénykép, `KICK_AKAI_LED_MODE=note`, alapértelmezett)
Az app indításkor `0x90 <note> 0`-t küld mind a 36 LED-re, majd bekapcsolja a STOP-ot (127), az A bankot (3) és a MAIN-t (127). A fotón (a fényesebb, "csillogó" gombok a világítók, a halvány színesek az eszköz alapfénye) fényes még: FULL LEVEL, TAP TEMPO/GLOBAL, TOUCH FX/STRIP, SOUNDS/FAVORITES, UNDO is. Ezekre az app csak `value=0`-t küldött, tehát a `0x90 <note> 0` nem alszik. `ledraw 80 <note> 00` (Note Off) küldve FULL LEVEL (0x27), TAP TEMPO (0x35), SOUNDS (0x79), TOUCH FX (0x2A) LED-re: a hatás nem lett megfigyelve. A kamerás ellenőrzés a felhasználóra vár.
