# Hardveres session → cloud session

## 2026-11-02 – MÉRÉS `f7e8846d` buildről (MPC Key 61): Main Volume NEM szabályozza a hallható hangerőt; strip módok; Full/Half Level
- **Main Volume (AD1a) – mért:** a fizikai gomb tekerése mozgatja a Track Mixer master Level-jét (a képernyőn látszik: 73→54→61→35…), az injektált SysEx is (`0x20` → 35, `0x75` → 127, `0x00` → 0). **De a hallható hangerő nem változik:** a master Level **0-n** állt (képernyőn 0, injektált SysEx), és a felhasználó **hallotta** az ütött padet. A felhasználó szerint a gomb tekerése nem, csak az ütés ereje változtatja a hangerőt. Tény: a `bus->gain` (output bus 0) értéke változik, de a kimenetet nem szabályozza. Nem tudom miért (a master meter L/R is végig üres, miközben a track mérője kitér; lehet, hogy a kimenet útja nem a bus 0 gainen megy, vagy az nem kerül alkalmazásra). Az ALSA kimenet (`card1`/ACVM, 4 csat., 44,1 kHz) a Kick tulajdona és fut. Kérem: nézzétek meg, melyik gain-lépcső szabályozza ténylegesen a kimenetet (vagy a Mixer Master csúszka sem változtat a hangerőn?). A gombbal a hardver analóg hangerejét az Akai OS valószínűleg külön állítja; ezt innen nem mértem.
- **Csend a munkamenet közben (nem reprodukáltam):** a pad-ütés (fizikai és injektált) egy ponton semmilyen jelet nem adott (a track mérője nem tért ki), miközben az app élt, az ALSA folyam futott, a master Level 127, a Pad Mixer szintek nem voltak nullák (A01 0,39; A02 0,70; A03 0,95). Újraindítás után a pad szólt. Előzmény: Main Volume lent, strip módok léptetése és húzás a módokban, REC/PLAY próbák. Az ok nem azonosított; ha a cloud talál olyan állapotot (strip mód, Sustain/CC64, Pad Level, pad-szint 0), ami a pad hangját elnémíthatja, az a gyanús pont.
- **Strip módok (`strip <v>` injektálással, mód lépteve):**
  - **Pad Level (2): működik** az adatban: A02 pad szintje 0,70 → 0,87 (`strip 110`). A **Pad Mixer nem frissül élőben**, csak újranyitáskor mutatja az új értéket (UI-hiány).
  - **Track Level (4): nem igazoltam.** A Track Mixer a "Programs" nézetet mutatta, a sáv szintje ott nem látszik; a legördülőt injektált érintéssel nem tudtam kinyitni. Kérem: Track Level módban a `setTrackLevel` hatása látszik-e a Track Mixerben (Tracks nézet)? Ezt kézi próbával a felhasználó el tudja dönteni.
  - **Pitch Bend / Mod Wheel / Sustain / Expression:** a DnB Kit dobprogram ezekre nem reagál, ezért hallható változás nincs (várt). A kimenő MIDI-t nem mértem.
  - **Touch Strip gomb LED:** a LED-írás működik: Off módban (7) a gomb LED-je kialszik (a felhasználó látta). A halvány/teljes fényerő nem különböztethető meg (a felhasználó szerint „nem változott"), tehát a hardver valószínűleg csak be/ki. A strip melletti piros LED-pöttyök az útmutató szerint nem vezérelhetők (a MIDI Control Mode "Light LED" leírása: nincs funkciója a hardveren); normál módban az útmutató róluk nem ír.
- **Full Level / Half Level (felhasználói kérés):** az útmutató (343. o.) szerint a **Full Level** gomb a padeket mindig **127**-es velocity-vel szólaltatja meg, a **Shift + gomb (Half Level)** **64**-gyel; a gomb nyomva tartva egyéni szint állítható. A hídban a gomb és a `setHalfLevel` állapot kezelve van, de a **valódi pad-ütés velocity-jére** nem találtam hatást (a `isHalfLevel()` olvasója csak a step sequencer; a hardver-drawer `isFullLevel()` a saját pad-ütéséhez). Kérés: a Full/Half Level legyen alkalmazva a fizikai pad-ütés velocity-jére.
- **Data Dial:** fizikai tekerés a Track képernyőn rendben (lásd előző bejegyzés).

## 2026-11-02 – DATA DIAL ZÁRVA (hardveren, build `1a5e4543`): a fizikai tárcsa működik
- **Mért:** fizikai tekerés a Track képernyőn a trackek között léptet (felhasználó). A naplóban 83 dial-csomag (`type 4`, `data[0]=0`, rotary) és 3 nyomás (`note=111` = ENTER) ért el a DCL üzenetkezelőjéig. A korábbi „dial nem működik” oka nem a hardver/adapter volt: a próba List Edit képernyőn zajlott (az én mérésem után ott maradt az app), és ott nem látszott változás.
- **Nyitott, kis dolog (nem akadály):** a **List Edit** képernyőn a dial tekerése látható változást nem okozott (6 esemény, nincs kijelölt sor a snapshoton). Nem tudom, hogy ez a tervezett viselkedés-e (a List Edit dial-használatát az útmutatóban nem vizsgáltam); ha a cloud tudja, hogy ott a dial mit vár (kijelölt sor léptetése?), jelezze.
- **Szekvenciafelvétel fizikai pad-ütéssel IGAZOLVA:** 6 felvett A1 jegyzet a List Edit-ben (idők 01:01:86–01:03:91, hossz 1–11 tick, velocity 32–127 változó: valódi ütések, nem rajzolt jegyzetek). A felvételt a PLAY indította (a REC élesítése megmaradt az előző REC + STOP után, a leírásotokkal egyezően).

## 2026-11-02 – SZEKVENCIAFELVÉTEL MÉRVE (build `1a5e4543`, ARMv7, MPC Key 61): működik élesített sávval
- **Felhasználó, fizikai:** REC LED világít, OVERDUB LED jó, PLAY és STOP LED jó. A saját pad-ütései nem kerültek a Gridbe; a naplóban nincs érintés a sáv Record (Rec Arm) gombján, a Grid-jegyzetek a ceruzával, ujjal kerültek be. Élesített sáv nélkül nincs felvétel (a leírásotok szerint), tehát ez valószínűleg nem hiba; a felhasználó reggel újrapróbálja élesítve.
- **Mért (injektált), élesített Drums sávval** (`touch` a sáv Record gombjára → a gomb piros; `btn REC`, `btn PLAY`, hangok, `btn STOP`):
  - `pad 1/3/5 100`: a sáv szintjelzője PLAY alatt mozgott, a régi jegyzetek (A8, A5, A1) eltűntek (a REC törli az élesített sáv jegyzeteit, ahogy leírtátok), új jegyzetek jelentek meg a Gridben.
  - `rawmidi 1 9F <30|34|37> 64` / `8F ... 00` (billentyű ch16, 2 s-onként): a List Edit 3 eseményt mutat: D1 `01:02:90` hossz 195, D5 `02:02:88` hossz 197, D8 `03:02:90` hossz 195, velocity 100 mindhárom. Az időzítés pontosan 1 ütem (2 s @120 BPM), a hossz ≈ 1 s (note-on → note-off), tehát a hang és a hossz helyes.
- **Megfigyelés (UI, nem felvétel):** a Grid View csak az A bank sorait mutatja (A1–A16, B1–B3); a billentyű-hangok a D bankba estek (D1, D5, D8), ezért a Grid üresnek látszott, miközben a List Edit mutatta őket. Ha a felhasználó pad-felvétele másik bankba esett, mint a Gridben látható, ugyanez adhat „nem került be" benyomást. Ezt reggel a fizikai pad-felvételnél nézem meg (melyik bank aktív).
- **Nem mértem még:** hallható-e a lejátszás (a felhasználó füle kell), a Looper SYNC PLAY/REC fázis, az Aura hosszú futás (az éjszakai stabilitás: az előző build végig élt, új coredump nincs), Shift+REC Recall.

## 2026-11-02 – KÉSZ: `docs/akai_mpc_hardware/KEY61_TOUCH_STRIP_MODES.md` (touch strip 12 mód, v3.10 útmutató 101–103. o.)
- Módonként: mit vezérel, Shift+gomb beállítások, Touch Sense (334. o.), a Kickben létező módok (Pad Level, Mod Wheel, Track Level, Sustain, Expression + Pitch Bend) külön táblában, a Kickben nem létező dolgok (Q-Link, Crossfader, Note Strum) külön listában; ami az útmutatóban nincs, **„nincs a doksiban”**.
- A lényeg az Y4-hez, csak amit az útmutató kimond: Mod Wheel = **CC1**, Expression = **CC11**, Pitch Bend: Relative Mode (a közép az első érintés helye) / fix közép, Bend Up/Down Range, Global Pitch Bend. **Nincs a doksiban:** a visszaugrás elengedéskor (pitch bend, mod wheel, expression), a Sustain be/ki küszöbe, a Pad Level / Track Level tartománya és skálája. Ezekhez nincs útmutató-fedezet, ne találd ki; ha kell, hardveres mérés (a felhasználóval) vagy kérdés a felhasználónak.
- Mért (nem doksi): a strip pozíciója CC33+CC34 (14 bit, ch1), érintés = Note 78 be/ki.

## 2026-11-02 – NYERS MIDI MÉRÉS (Key 61, fizikai vezérlők): Data Dial bemenete jó; a Main Volume SysExet küld, a Kick nem dolgozza fel
- **Módszer:** a Kick leállítva (az kizárólagosan foglalja a RawMIDI portokat), `amidi -p hw:0,0,<0|1|2> -d -t 100`, a felhasználó sorban végigment a vezérlőkön. Időbélyeg nincs, a vezérlők az üzenettípusukról azonosíthatók. Port 0 és Port 2: 0 üzenet; Port 1: 1609 üzenet. Az app a mérés után újraindítva (futó build: b65dc8c6 alapú, a d0ab8e46-ig tartó cloud-commitokat még nem építettem).
- **Mért üzenetek (mind Port 1):**
  - Data Dial forgatás: `B0 64 01` (jobbra) / `B0 64 7F` (balra), CC 100 relatív, ch1; nyomás: Note 111.
  - `+` = Note 54, `−` = Note 55 (be/ki).
  - Q-Link 1–4: CC 16–19 relatív ch1; Q-Link érintés: Note 84, 85, 86, 87 (be/ki, vel 127/0).
  - Touch strip: CC 33 + CC 34 ch1 (min 0, max 124/127).
  - Pitch wheel: Pitch Bend ch16 (0–16383); Mod wheel: CC 1 ch16; szusztain: CC 64 ch16.
  - Billentyűk: Note On/Off **ch16 Port 1-en** (Port 0 üres), vel 75–97; aftertouch: Channel Pressure ch16 (0–127).
  - Pad: Note 37 ch10 + Poly Aftertouch; Note 78 (be/ki) egy gombé, leképezését nem néztem.
  - **Main Volume (nem külön azonosítva, kizárásos alapon):** SysEx `F0 47 7F 4B 5B 00 01 <v> F7`, `v` = 0x00..0x75, lépésenként fel/le. Más, nem azonosított vezérlő a mérésben nem maradt.
- **Következmény 1 (Data Dial) – JAVÍTVA ugyanaznap:** a bemenet pontosan az, amit az adapter vár (`akai_mpc_key61_adapter.h:448`, CC 100 → `on_jog_dial` → `callAsync` → `handleJogDial`). Az első változatban azt írtam, hogy a hiba a `handleJogDial` utáni úton van; **ez nem igaz**: a futó appon az injektált `jog 1` (3×) a Track képernyőn a kijelölést Drums → Lead & FX-re léptette (snapshot előtte/utána, a státuszsor `TRACKS: Lead & FX`). A `jog` parancs ugyanazt a `handleJogDial`-t hívja, mint az adapter, a különbség csak a szálváltás (`callAsync`). Tehát **a Kick-oldali út működik; hogy a fizikai tárcsa miért nem hat, nyitott**: a felhasználó mikor és melyik képernyőn tapasztalta, még nincs rögzítve, és a fizikai tekerést futó appon még nem néztem (a mérés alatt az app állt). A következő lépés: tekerés futó appon a Track képernyőn + snapshot. A TODO "CC száma ❓" jelzése így is megszűnhet (mérve igaz).
- **Következmény 2 (Main Volume):** az adapter a Port 1 bejövő SysExet nem értelmezi (a kódban csak kimenő SysEx van: init, pad RGB). A korábbi "a Main Volume nem MIDI" állításom **hibás volt**: a gomb SysExet küld. Az érték a Kick master hangerejére köthető; a leképezés (a 0x00–0x75 tartomány, a 0x5B parancs jelentése) az útmutatóból nem ismert, ez mért adat, nem dokumentáció.
- **Nem mértem:** expression pedál (nincs a felhasználónál; az adapter CC 11-et kezel), a Q-Link érintés és a Note 78 kezelése a Kickben.
- Nyers napló (Port 1) a hardveres session scratchpadjében; kérésre feltöltöm.

## 2026-11-02 – TC hardveres zárás: ENTER = Apply HARDVEREN IGAZOLVA
- Mért (b65dc8c6, injektált): `btn TC` → List Edit; a `TC` gomb érintésére megnyílik a Timing Correct ablak; `btn ENTER` → bezárul (snapshot md5 `8b8986e1d151` → `a0104c4456d1`). A `MAIN`/Shift+STOP = Cancel nincs mérve (Shift injektálva nem megy).
- Megjegyzés: az injektált `btn TC` önmagában a List Edit-re visz (nem nyit ablakot); az ablak a List Edit `TC` gombjának érintésére nyílik.

## 2026-11-02 – VÉGLEGES HARDVERES JELENTÉS (a riport "VÉGLEGES ÖSSZEFOGLALÓ" szakasza, TODO T5/T7b/T14/T19/T32/T33 frissítve)
- **OK hardveren (b65dc8c6):** indulás kijelző nélkül, **érintés**, minden gomb és gombpár, **LED `cc`**, bank/Shift/AUTOMATION/TAP/LATCH/TOUCH FX/STRIP/OCT LED, pad egyszeri trigger, strip bemenet, SOUNDS, **megállított seek** (`BAR_NEXT` ×2 → `3:01:000`, az 11. hiba javítva).
- **Hiányzik:** **szekvenciafelvétel (REC + PLAY)** (kódtény: `setRecordState` nincs hívva); REC/OVERDUB LED (a felhasználó döntése: REC élesítve piros / felvételkor villog, OVERDUB aktív zöld); Keyboard Control menü, Sounds Mode, Locate ablak, 9 strip-mód.
- **Nem ellenőrzött:** a TC hardveres zárás (az injektált `btn TC` nem nyitja meg az ablakot; fizikai próba kell), a kerék/pedál hangerő-ugrás javítása, az Aura-hosszú futás (az éjszakai teszt reggel eredményt ad).
- A következő hardveres ellenőrzéshez a szekvenciafelvétel megépítése kell; ha fent van, hash-t kérek.

## 2026-11-02 – FELHASZNÁLÓI DÖNTÉSEK (éjjel)
1. **REC/OVERDUB LED – elfogadva:** a REC **élesítve = piros (folyamatos)**, **felvétel közben villog (piros)**; az OVERDUB **aktív = zöld**. (A doksi nem ír LED-színt; ez a felhasználó döntése.) Ez egészül ki a szekvenciafelvétel megépítésével (lásd az előző bejegyzés): a REC LED a **szekvencia** élesítési/felvételi állapotát kövesse, ne csak a looperét.
2. **TC ablak hardveres zárása – marad** (Kick-kiegészítés, nem doksi), de a felhasználó kéri, hogy írjuk le, mivel jár: lásd a riport/terv megjegyzést alább.
3. **A terv három döntése – elfogadva:** scope (Behringer KICK funkciók, MPC-specifikus részek `NEM-KICK`), a bridge szétbontása engedélyezett, egy integrátor (a cloud session). Részletek: `docs/akai_mpc_hardware/WALKTHROUGH_PLAN.md` 6. szakasz.
4. **Upstream PR-ok:** a felhasználó szerint "majd ha végeztünk", nem most; nem nyitunk PR-t.
5. **Modellek az Ollamában/HF-ben:** a felhasználó megtartja (a két képmodell és a gemma3 más-más feladatra való); nem törlünk.
6. **TC hardveres zárás – következmények (a commitokból, tények):** ENTER/STOP = Apply, Shift+STOP/MAIN = Cancel, csak amíg a TC ablak nyitva és legfelső modális (`c44e9659`, `ab69a65f`); ilyenkor a STOP/MAIN nem az eredeti funkcióját végzi (megállítás / Home), nincs dedikált Back gomb az enumban, ezért a MAIN a Cancel; **a beágyazott lista-popup (TYPE/EVENTS) hardveres CANCEL-je nincs megoldva**, azt továbbra is csak érintéssel lehet zárni (UAF-kockázat miatt kihagyva); **a többi modális ablak** (numpad, tempo stb.) hardveren továbbra is befagyhat, az általánosítás külön döntés; hardveren a három commit **nincs kipróbálva**.

## 2026-11-02 – SZEKVENCIAFELVÉTEL (REC + PLAY) HIÁNYZIK – a doksi szerint kell (riport 14. kör)
- **Mért (b65dc8c6 build, MPC):** a sáv Record gombja élesítve, a lejátszás fut, a REC/OVERDUB gombesemény megérkezik (`[AKAI-BTN] note=73/80`), a pad-leütés a Grid kijelölését mozgatja, **de a hang-esemény nem kerül a sávba**, és a REC/OVERDUB LED nem világít.
- **Kódtények:** `RecordState`/`setRecordState` létezik a motorban (`mt_audio_sequence.h:100–103`), de nincs hívója; az `EN_RECORD`/`EN_OVERDUB` egyetlen fogyasztója a Looper (`kick_LoopRecorder.cpp:173–174`); nincs élő felvétel-útvonal pad/billentyű → élesített sáv.
- **Az útmutató szerint (v3.10, Key 61 vezérlő-leírás 9–10; "Recording a Drum Sequence"; Rec Arm Behavior):** SEQ REC ARM + sáv Rec Arm → hardveres REC élesít → PLAY/PLAY START indítja (count-in) → a REC törli, az OVERDUB megtartja a meglévő eseményeket; Shift+REC = Recall. A doksi LED-színt nem ír.
- **Kérés:** a funkciót az útmutató szerint kell megépíteni (nem hipotézis alapján); nyitott kérdés, ami a doksiban nincs: a REC/OVERDUB LED viselkedése (kérdezzétek a felhasználót). Teszt-forgatókönyv a hardveres oldalról: sáv Record gomb (érintés) → REC → PLAY → pad-leütések → STOP → a Grid/List Edit mutatja az új eseményeket, és újrajátszáskor hallhatók.

## 2026-11-02 – ÉRINTÉS HARDVEREN IGAZOLVA (`c4ec7f35`)
- **Működik:** injektált és fizikai érintés is. Napló: `[TOUCH] delivery=manual (peer=1 showing=0)`, `manual mouseDown/mouseUp delivered to 'Program'`, `Button released: 'Program'`; fülváltás (Track ↔ Program), Step Sequencer csúszka és track-sor tekerő működik (felhasználó). 104 érintés, 102 fizikai, `delivery=manual` 184, `peer` 0. Köszönjük a gyors javítást (Z2a/Z1a).
- **Pontosítás:** a `touch X Y` framebuffer-koordináta és a snapshot (ppm) tere ugyanaz (1280x800), a Program fül ott kb. **(440,145)**, nem (550,193).
- **Nem mértem még:** a Z1b OVERDUB/REC LED viselkedése (felvétel közben), a looper (Z2b, 22. kör), és az `0193d9dd` utáni commitok; ezeket a következő buildben nézem.

## 2026-11-02 – VÁLASZ: érintés-ok nyugta; a régi bináris "ellentmondás" kérdésére amit mértem
- Nyugta: a rejtett ablak + a `getComponentAt` `visibleFlag` feltétele + a G15 korai return magyarázza a mértet (a célpont helyes, a `mouseDown` nem érkezik meg). A javítás után építem, és a `[TOUCH] manual mouseDown delivered to 'Program'` sort és a fülváltást nézem.
- **A kérdésedre (a régi bináris kiosk/ablak) – mért tények, amik vannak:** a régi bináris X nélkül indult és 15+ másodpercig stabilan futott (nem omlott össze); a naplója szerint a `KickHome` gyerekei `vis=1` és a gyökér `bounds=(0,0,1280,800)`, tehát a komponensfa látható volt és 1280x800-as. A sztringek strippeltek (15,7 MB), a `setKioskModeComponent` hívás megléte a binárisból nem olvasható ki, ezért hogy a régi bináris hívta-e, **nem tudom**. Gyors, nem-build mérés amit tudok adni: a régi bináris `readelf`/`strings` kimenete vagy a nyitott fájlok (megvan: `/dev/fb0`, `/dev/input/event0`, hang- és MIDI-eszközök, nincs X-socket). Ha ennél mélyebb kell (pl. gdb a futó régi binárison), szóljatok.

## 2026-11-02 – VÁLASZ a mérési kérdésekre (régi bináris, mért): a régi UI gyökere 1280x800, az újaké 1024x600
**1. `/tmp/.X11-unix` a régi bináris futása alatt:** nem létezik (`No such file or directory`); nincs X/Xorg/Xvfb/weston/wayland folyamat; a régi folyamat környezetében nincs `DISPLAY`, `XDG_*`, `WAYLAND*`. Tehát a régi bináris is X nélkül futott.
**2. A régi bináris naplója (`/data/kick.log`, `Kick.bak.20261101-0412`):** nincs ablak/peer/kijelzőszerver sor; az induló sorok: `[AKAI-DISPLAY] Physical panel 800x1280 ... 90-degree CW matrix active for 1280x800 UI`, `[AKAI-TOUCH] Connected to ILI2117 (/dev/input/event0) -> Transform to 1280x800 UI active`, `[AKAI-CONTROLLER] RawMIDI ... STARTED (Mode 0x41 Private)`, `[HAL] KickHardwareBridge fully operational (Display 1280x800 @ 30FPS, Touch, ControlSurface, CmdFIFO)`.
**3. A döntő különbség a `[BRIDGE]` sorokban:**
- **Régi:** `[BRIDGE] frame=0 rendering 1280x800 to 1280x800, root bounds=(0,0,1280,800) children=32 currentScreen=1024`; a komponensek 1280x800-hoz igazítottak: `Topbar bounds=(0,0,1280,76)`, `Statusbar bounds=(0,736,1280,64)`, `AppTrack bounds=(10,60,1270,600)`. **A UI gyökere 1280x800, nincs skálázás a framebuffer és a UI között.**
- **Új** (`ea7f2d68` után): `[BRIDGE] frame=540 rendering 1024x600 to 1280x800, root bounds=(1,1,1022,598)`: a gyökér **1024x600** (a tartalék kijelző mérete), és a framebufferre skálázva rajzolódik; a hit-teszt a 1024x600 fában fut, `peer=1`.
- A régi kódban a `dispatchTouch` hit-tesztje közvetlenül a 1280x800-as fában (nincs skálázás a koordinátában). Az újban `ui = fb * ui / fb size`, a `touchSrcOk=0` és a `source=mouse(src0)` visszaesés.
**Mért tények, következtetés nélkül:** a régi bináris 1280x800-as gyökérrel és X nélkül működő érintéssel futott ugyanazon az eszközön; az újabbak 1024x600-as gyökérrel és tartalék-kijelzővel futnak, és bennük az érintés a helyes célpontot találja, de a felület nem reagál. A régi bináris naplójában nincs `[TOUCH]` (az akkori kódban nem volt ilyen napló).

## 2026-11-02 – ÉRINTÉS: az eredeti bináris működik ugyanazon az MPC-n, az újabb buildek nem (mért tények)
- **Mért:** az MPC-n a régi, eredeti bináris (`/data/Kick.bak.20261101-0412`, 15,7 MB, strippelt, GCC 12.2 armhf) **érintésre jól működik** (a felhasználó Track ↔ Program fület vált, az első demóban zongorázott a kijelzőn). Ugyanazon az eszközön, ugyanabban a környezetben (nincs `DISPLAY`, nincs X-socket a `/tmp/.X11-unix`-ban).
- Az eredeti bináris nyitott fájljai (`/proc/<pid>/fd`): `/dev/fb0`, `/dev/input/event0`, `/dev/snd/pcmC1D0p`, `/dev/snd/midiC0D0`, `/dev/snd/seq`.
- **Újabb buildek** (`ea7f2d68`, `fd2480de`, `0e6ea4b1`): az érintés **eljut az alkalmazásig és a koordináta helyes** (`[AKAI-TOUCH] raw=(395,1346) -> fb=(438,154)`, `[TOUCH] ... ui=(350,115) target=KickToggleButton 'Program' peer=1`), a "native peer path"/`source=mouse(src0)` útvonalon megy, **de a felület nem reagál** (injektált és fizikai érintésre sem).
- **A régi bináris kódja** (a `git show 9cf31dd9:Kick/apps/kick/kick_hardware_bridge.h`): `dispatchTouch` ugyanúgy `m_window->getPeer()->handleMouseEvent(InputSourceType::touch, x*1024/1280, y*600/800, ...)`-et hívott, tehát az **út ugyanaz volt, és működött**.
- **Az eredeti bináris sztringekből (következtetés, nem biztos):** tartalmazza a T8 oktáv-reset szöveget (`a49e4f38`, nov. 1. 01:29), de nem tartalmaz arp, recall, seekToBar, locateToEvent, KICK_AKAI_PORT0 szövegeket: valószínűleg egy T8-at tartalmazó ágból épült, a későbbi funkciók nélkül.
- **Következtetés (csak a tényekből):** az eszköz és a környezet rendben van; a különbség a régi build és az újabb kód/build között van. **Kérés:** nézzétek át a diffet a `a49e4f38` (régi állapot) és a mostani `main` között az érintés/ablak/peer útvonalon (`Main.cpp` induláskori ablak-létrehozás és kijelző, a fallback-display, `kick_hardware_bridge.h` `dispatchTouch`, `akai_mpc_key61_adapter.h` touch adapter, `touch_space.h`), és mondjátok meg, mi változott a peer létrehozásában vagy a touch-forrásban. Szükség esetén **én végzem a hardveres bisectet** (egy build kb. 15 perc, a felhasználó koppint): adjatok commit-listát.
- Az MPC-n most az eredeti bináris fut (érintés működik); az új build visszatölthető.

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
