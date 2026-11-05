# Kick (MPC Key 61 + desktop): ismert hibák, hiányzó/tervezett funkciók, igazolt állapot

Gyűjtés dátuma: 2026-11-04, ág `claude/kick-plans` (HEAD `b650370e`). Csak dokumentumokból és git-történetből; kódot, hardvert nem futtattam. Jelölés: **HIBA** / **HIÁNYZIK** / **IGAZOLT** (hardveren vagy tesztben mért) / **KÓD-ONLY** (megépítve, hardveren nem igazolt).
Forrás-rövidítések (mind `docs/` alatt): TODO = `AKAI_MPC_KEY61_TODO.md`; RPT = `AKAI_MPC_KEY61_HW_TEST_REPORT.md`; MD = `AKAI_MPC_KEY61_MORNING_DECISIONS.md`; CHK = `AKAI_MPC_KEY61_HW_TEST_CHECKLIST.md`; R1/R2 = `_REVIEW_ROUND1/2.md`; DVK = `akai_mpc_hardware/KEY61_DOC_VS_KICK.md`; BEH = `.../KEY61_HW_BEHAVIOUR.md`; STRIP = `.../KEY61_TOUCH_STRIP_MODES.md`; LED = `.../KEY61_LED_COLOURS.md`; TO/FROM = `hw-mailbox/TO_CLOUD.md`/`FROM_CLOUD.md`; WP-D/WP-G = `akai_mpc_hardware/walkthrough/`.

## 0. Figyelmeztetés: elavult / egymásnak ellentmondó sorok a dokumentumokban
- TODO.md §1-§3 (L17-L154) még a d9a48e38-i statikus audit állapotát tükrözi; a §5-§8 (L156-L237) frissebb. A §2 "❌" sorok jó része azóta megépült (T7-T13, T24-T34).
- TODO L133 "Main Volume ... hardveren nem igazolt": elavult, lásd 898a122a (a hangerő követi a gombot, MPC-n mérve).
- TO (utolsó bejegyzések) "Main Volume nem szabályoz hallható hangerőt", "Full/Half Level nincs a pad-ütésre", "Pad Mixer nem frissül élőben": három külön commit javította (898a122a, 0947f1f5, 88e6670c; mind 2026-11-02).
- RPT L197 (13. hiba) és L250: "3 strip-mód, 9 hiányzik": elavult. MD L118-L132 (Y4): 8 mód épült (Kick slider, Pitch Bend, Pad Level, Mod Wheel, Track Level, Sustain, Expression, Off).
- TODO L12-L13 (Data Dial "❌ nyitott"): TO "DATA DIAL ZÁRVA" és 0a42bfdb szerint zárt.
- HARDWARE_REVIEW.md ("v2.0 HITELESÍTVE ÉS KÉSZ") TODO.md L37 szerint tervként olvasandó, nem állapotként.
- WP-D D-031 "túlállított" (commit b642c3b1).

## 1. IGAZOLT (hardveren mért vagy host-teszttel fedett)
| Terület | Állapot | Forrás |
|---|---|---|
| Indulás kijelző nélkül (tartalék Desktop-display, SOUNDS összeomlás javítva) | IGAZOLT | RPT L54, L62-L78, `179c99c9` |
| Érintés (fizikai + injektált; fülváltás, Step Seq csúszka, tekerő; `delivery=manual`) | IGAZOLT | RPT L199-L211, c4ec7f35 |
| Dupla koppintás (450 ms/24 px) -> mouseDoubleClick (Sample Assign pad-hozzárendelés MPC-n) | IGAZOLT | 42cfdc3c |
| Gombok megérkeznek; gombpárok a várt képernyőt nyitják: EDIT/XYFX, GRID/STEP SEQ, MIX/PAD, MUTE/PAD, SAMPLE EDIT/SAMPLER, KEYBOARD CONTROL/MIDI CTRL, SOUNDS, UNDO/REDO, AUTOMATION | IGAZOLT | RPT L105-L120, L237 |
| LED-protokoll `cc` (`B0 <note> <érték>`); `note` és `noteoff0` nem működik | IGAZOLT | LED L5-L14, RPT L69-L70 |
| LED-ek: STOP, PLAY, bank A-D (csak a kiválasztott, fehér), MIX/MUTE (fehér/piros), AUTOMATION (zöld=Read, piros=Write, sötét=Off), TAP TEMPO, LATCH, TOUCH FX/STRIP, OCT+, két Shift LED szinkronban | IGAZOLT | RPT L239; LED L33-L53 |
| Kick-first hideg indulás: a gyári MPC start-up SysEx-sorozat (Identity Request + 19 config SysEx) küldése után a vezérlő Private módban van (Shift Port 1-en, LED világít) | IGAZOLT | BEH L81-L103, 2b16e00f |
| Középső gombblokk (Full Level, 16 Level, Erase, Copy, Menu, Sounds, Main, Undo, Tap Tempo) hideg indítás után | IGAZOLT | BEH L119, fcdfda15 |
| Full Level = 127, Shift+Full = Half Level 64 (fizikai ütés, LED) | IGAZOLT (hideg indítás után, felhasználó) | 0947f1f5, BEH L119 |
| Pad: egyszeri trigger nyomva tartva (aftertouch-csomag kikapcsolva); pad-színek bankonként | IGAZOLT | RPT L119, L240 |
| Touch strip bemenet CC33+CC34 14 bit, Note 78 érintés | IGAZOLT | BEH L17, RPT L188 |
| Data Dial: CC100 relatív, Track képernyőn lépteti a trackeket; 83 dial-csomag a DCL-ig | IGAZOLT | TO "DATA DIAL ZÁRVA", TODO L90 |
| Data Dial / +/- az utoljára érintett vezérlőt állítja minden képernyőn (Pad Mixer A01 Level mérve) | IGAZOLT | 0a42bfdb |
| Main Volume: SysEx `F0 47 7F 4B 5B 00 01 <v> F7` (v 0x00..0x75) -> Kick master szint; a hangerő követi a gombot (bus bypass javítva) | IGAZOLT (commit-üzenet szerint MPC-n mérve) | 3f4c8321, 898a122a, BEH L14 |
| Megállított állapotú seek (`BAR_NEXT` x2 -> `3:01:000`, injektált); `<<`/`>>` lejátszás közben és megállítva | IGAZOLT (injektált) | RPT L243, TODO L167 |
| Szekvenciafelvétel pad-ütéssel: 6 fizikai A1 jegyzet a List Editben (velocity 32-127); billentyű-felvétel injektálva (REC törli, OVERDUB megtartja, 1 ütem pontosság); REC/OVERDUB/PLAY/STOP LED jó (felhasználó) | IGAZOLT | TO "SZEKVENCIAFELVÉTEL MÉRVE", "DATA DIAL ZÁRVA" |
| TC ablak hardveres zárása: ENTER = Apply | IGAZOLT (injektált) | RPT L257, cd75e237 |
| Strip Pad Level mód (A02 0,70 -> 0,87) | IGAZOLT (injektált) | TO L15 |
| Strip Touch Strip gomb LED kialszik Off módban | IGAZOLT (felhasználó) | BEH L29 |
| Keygroup: egy minta = kromatikus hangszer C3 gyökérrel; hangmagasság "kb. jó" a billentyűzeten; fizikai hang-ellenőrzés | IGAZOLT részben (owner build 46); hangzás build 49-en nincs | e1448d49, 277f03c5 |
| Piano projekt betöltése a fájlböngészőből nem omlik össze (ASAN Mac + MPC); project-váltás összeomlás javítva | IGAZOLT (összeomlás nincs); hangzás nem | 8ea4af11 |
| Host-tesztek (Kick/tests/platform, 33 fájl): akai_map, led_map, strip_modes, pitch bend, sustain (+stress), seek, arp, TC, automation, seq record, main volume, relative CC, MIDI parser, CV scale, touch space, stb. Nem találtam kikapcsolt/skipped/DISABLED tesztet; a CMake csak `if(APPLE)` (Carbon, keyboard detector) és JUCE-hiány feltételeket használ | IGAZOLT (host) | `Kick/tests/platform/*`, TODO T21 |
| Zárt kód-hibák (review): F1-F17, G1-G17 nagy része javítva; R2 "Az első kör javításainak állapota" szerint hiányos volt F2, F3, F4, F12 (G2, G1, G5/G6, G15 követte) | KÓD-ONLY (host-teszt + review) | R1, R2 L33-L40 |

## 2. HIBA / nyitott ismert hibák
| # | Leírás | Forrás | Állapot |
|---|---|---|---|
| H1 | **Aura::processReplacing SIGSEGV** üresjáratban ~25 perc után (`sampleFrames=0`); gyökérok nincs megállapítva, G9 (DawInserts prepareToPlay) javítás fent, hosszú futásos újrateszt kell | RPT L41; R2 L9 (G9); FROM L12 pont 6 | HIBA / nem igazolt a javítás |
| H2 | **Pad "csendes állapot"** egy munkamenetben (pad-ütés nem adott jelet, újraindításra szólt); nem reprodukálva; gyanú: strip Sustain/Pad Level mód | TO "Csend a munkamenet közben"; FROM L12 pont 4 | HIBA (nem reprodukált) |
| H3 | **Pitch bend / mod wheel / pedál megugrasztja az 1. track hangerő-mérőjét** (G8: reloadMidiMeter); az újramérés hiányzik | RPT L32, L258 | HIBA / javítás újramérés nélkül |
| H4 | **List Edit képernyőn a Data Dial nem látszik hatni** (6 esemény, nincs kijelölt sor); nem tudni, tervezett-e | TO "Nyitott, kis dolog"; FROM L12 pont 5 | HIBA/nyitott |
| H5 | **Beágyazott lista-popup (TYPE/EVENTS) a TC ablakban hardverrel nem zárható**; a többi modális ablak (numpad, tempo) hardveren befagyhat; MAIN/Shift+STOP = Cancel nincs mérve | TO "TC hardveres zárás - következmények"; RPT L257 | HIBA |
| H6 | **Strip Track Level hatása igazolatlan** (Track Mixer "Tracks" nézetben nem néztük) | TO L16; FROM pont 3 | NEM IGAZOLT |
| H7 | **Looper SYNC fázis (Z2b)** mérése hiányzik; AC2: a Looper szinkronos REC elindíthat szekvencia-take-et | FROM L12 pont 6; FROM L32 ("Ismert, javítás úton (AC2)"), a2cf876a, 0193d9dd | NEM IGAZOLT |
| H8 | **Gomb-LED hiányok:** a `<`, `>`, `<<`, `>>`, `-`, `+`, PLAY START nincs a LED-táblában (firmware alapfény marad); LOCATE, COPY, ERASE, UNDO, TAP_TEMPO, METRONOME, TOUCH_STRIP LED nincs hajtva; ARP/LATCH/TC LED csak gombnyomásra frissül; oktáv-LED nem mutatja a félhangot; OCT LED-létra csak bool | RPT L160-L172; TODO T13 L173, §2.4 | HIBA/HIÁNYZIK |
| H9 | **Halvány (1) és teljes (127) LED fény szemmel nem különböztethető meg** a Touch Strip gombon (hardver gyakorlatilag be/ki) | BEH L13 | MÉRT korlát |
| H10 | **Port 0 init SysEx / Port 0 írás** (init SysEx + pad RGB mindkét portra): `KICK_AKAI_PORT0=0` összehasonlítás nincs mérve; Port 0 / Port 2 tartalék ág (d0-36 fallback) régi viselkedést tart életben | TODO L139-L140 (§3 pont 5-6), T5 L160 | NYITOTT döntés |
| H11 | **Kick-first indulás: nem tudni, melyik a 21 start-up üzenet közül a lényeges**; teljes sorozat küldése (biztonságos, nem szűkítve) | BEH L102 | ISMERETLEN (működik) |
| H12 | **Main Volume parancsbájt `0x5B` jelentése ismeretlen**; `0x76..0x7F` nem látott; mentődik-e a projekttel / melyik bus (mindig 1.) döntés kell; kettős hatás (firmware + SysEx) nem ismert | MD L98-L105 (AD1a) | DUMMY |
| H13 | **Pitch bend: lépcsőzés (blokkonként lép), pedál-állapot nem nullázódik programváltásra/stopra, sustain NOTE_OFF mintán nincs hatás**; Relative Mode / Bend Range / Global Pitch Bend ablak nincs | TODO T26 L216; MD Y4 | HIÁNYZIK/részleges |
| H14 | **Rejtett/ismeretlen hatású dummy-döntések** (strip: Sustain küszöb >=64, Pad/Track Level 1:1, elengedéskor marad; Pad Level = az utoljára ütött pad) | MD L118-L132 | DUMMY |
| H15 | **Hardkódolt útvonalak** (`hw:0,0,0..2`, `plughw:1,0`, `/dev/input/event0`); `device_probe.h` tartalékokkal enyhíti; ha valódi érintő-max != 2048 a skálázás változik (mért 2048x2048) | TODO L143, T17 L180; RPT L20 | RÉSZBEN javítva |
| H16 | **Audio-adapter (mpc_gui) buffer_size/latency/limiter**: csak az önálló `apps/mpc_gui`-t érinti; hard clip +-0,99, nincs limiter | TODO T18 L181, §2.5 L126-L127 | NYITOTT (csak ha mpc_gui cél) |
| H17 | **Audio bemenet (INPUT 1/2, +48V)**: a JUCE AudioDeviceManager nyitja az ALSA capture-t (kód szerint); eszközön nincs igazolva, hogy Looper/Sampler látja | TODO T15 L175, CHK §6 | NEM IGAZOLT |
| H18 | **CV/Gate** nincs hardveren tesztelve (szándékosan: I2C-re nem írunk); Akai-adapterben nincs; nem tudni, Linux-DAC vagy STM32 viszi | TODO T19 L182 | NEM TESZTELT |
| H19 | **Szálbiztonság/polling:** `set_led` és olvasó szál közös `m_out1` (T16-ban mutex), HAL hívások olvasó szálról (G1: callAsync); TC/RECALL tick-csúszás elfogadott (G11) | TODO L141-L142, R2 | KÓD-ONLY |
| H20 | **RECALL:** nincs UI/LED visszajelzés; futó szekvenciánál note-list verseny kockázat (`MTNoteList::addNote`) | TODO T28 L217 | KÓD-ONLY, hardveren nem próbált |
| H21 | **iOS szimulátor:** `UIScene life cycle is required` -> az app kilép indításkor (JUCE 7.0.12 legacy delegate) | AGENTS.md ("Known state") | HIBA |
| H22 | **Hiányzó release-tartozékok:** gyári minták, Myriad Pro font, `juce_serialport`; `KT_KickEM` szimulátor target `config.h` hiánya Linuxon | AGENTS.md "Known state"; FROM L12 pont 6 | ISMERT |
| H23 | **Hiányzó minta jelzése:** döntés van (egyszeri, nem blokkoló UI-figyelmeztetés), a UI-elem DUMMY/nincs megírva | MD L25; FROM pont 6 | HIÁNYZIK |
| H24 | **`fix/project-relative-samples` szándékosan kimaradt** (az élő fát írná át mentéskor); az upstream kérésére snapshot-elv köré újratervezendő | MD L57-L66 | DÖNTÉS függő |
| H25 | **Automation korlátok:** track PAN nem automatizálható (nincs fogyasztó); nincs lane-szerkesztő UI; Write nem undo-zható; szekvencia-másolás nem másolja a lane-t; lane-tickek nem skálázódnak hosszváltásra; nem perzisztens mód; az automation-lane működése hardveren nincs mérve; program-szintű pan lane (T35, `pan-lane-automation` ág) folyamatban | TODO T33 L220, T34 L237 | RÉSZBEN / HIÁNYZIK |
| H26 | **Timing Correct:** "Events: Range" nem támogatott; flag és paraméterek nem perzisztensek; rövid track Start-mód csavar; nincs TC LED; a TC gomb hatása a snapshotban nem látszik (ablakot a List Edit TC gombja nyitja) | TODO T30 L219; RPT L100 | RÉSZBEN |
| H27 | **MIDI program PAN -> CC10**: hardveren nem próbált; kezdő-CC10 lejátszáskor/betöltéskor nem megy ki | TODO T34 L237 | KÓD-ONLY |
| H28 | **`<<`/`>>` részletek** (hardveren nem próbált): lejátszás közbeni fizikai próba, ratchet/hosszú hang az ugrás fölött, parkolt seek nem látszik a globális playheaden, song módban nyers óra-tickek, részleges loop és audio trackek nincsenek elemezve; Shift+STOP RETURN playhead-visszaállítás (5cfd562d) hardveren nem mérve | TODO T7b L167, T31 L213 | NEM IGAZOLT |
| H29 | **Shift-lock / dupla nyomás** a v3.10 szerint (más MPC-k) a másodlagos funkciójú gombokat villogtatja; a Key 61 doksija nem említi; nincs megvalósítva | RPT L172, DVK L30 | HIÁNYZIK (döntés) |
| H30 | **Grid View csak az A bank sorait mutatja** (A1-A16, B1-B3): más bankba esett felvétel "nincs bent" benyomást kelt | TO "Megfigyelés (UI, nem felvétel)" | UI-hiány |

## 3. HIÁNYZIK: tervezett / kézikönyv szerinti funkciók
### 3.1 Akai-specifikus réteg (owner döntés 2026-11-02: a Kick-funkciók után, az Akai rétegben, end-to-end; a mód még nincs kitalálva)
Forrás: MD L92-L97; FROM L13.
- **Locate ablak** (Time: Bars:Beats:Ticks, Loop, Loop Start/End, Auto Record; Locators fül, 6 locator) - DVK L14, L51.
- **Sounds Mode** (beépített hangszerek/preset-ek); SOUNDS most a File Browsert nyitja; Shift+SOUNDS "kedvenc-hely ugrás" (57050685), nem Favorites nézet - DVK L33, L52.
- **Keyboard Control menü** (Transpose, MIDI Channel, Aftertouch, Note On Velocity, Scale Filter, Internal/External routing, Velocity Response/Curve/Gain, Black/White Key Sensitivity, Pedal Settings); a gomb most a Pad Perform-ot nyitja - DVK L26, RPT L97.
- **Menu tartalma** (Project ablak, módikon átrendezés, Reset Mode Menu, Help/QR, Undo History, hangoló, Sync mező (MIDI Clock/MTC/Link), Legal Info, Bluetooth, Wi-Fi Show/Forget/Reset, Audio Device választó, 32 In/Out, Disk Streaming) - WP-G (kb. 154 HIÁNYZIK, 62 ELTÉR, 47 OK sor, grep-es számolás).
- **Track View/Track Mixer** (WP-D: 51 HIÁNYZIK, 34 ELTÉR): Mixer Settings ikon, Solo/Cue mód, crossfader profil, metering módok, Track Settings ablak, Key Range/Learn, Output mező, Monitor Off/In/Auto/Merge, track pan, MIDI Filter (Note/Velocity Range, CC Filter), Track -/+, Duplicate Track; a Track Mixer sorlista (programonként), nem 8 oszlopos csík.
- Scope: a WALKTHROUGH_PLAN.md szerinti WP A-K végigvételéből csak WP-D és WP-G készült el (+ WP-B nyers JSON); WP-A,C,E,F,H,I,J,K nincs (`docs/akai_mpc_hardware/walkthrough/` csak WP-B.raw.json, WP-D.md, WP-G.md). Az 0. fázis blokkolói közül: érintés (kész), megállított seek (kész), "melyik gomb ér el az alkalmazásig" napló (`[AKAI-BTN]` megvan), bridge szétbontása területi fejlécekre (engedélyezett, hogy kész-e: nem ellenőriztem).

### 3.2 Touch strip (STRIP L30-L38; MD L118-L132)
- Megépült: Kick slider -> Pitch Bend -> Pad Level -> Mod Wheel -> Track Level -> Sustain -> Expression -> Off.
- HIÁNYZIK: **Q-Link**, **Touch FX** (strip-mód; a TOUCH FX gomb az XY képernyőt nyitja), **Notes**, **Crossfader**, **Note Strum**; Shift+gomb beállítóablakok (Pitch Bend Settings: Relative Mode, Bend Up/Down Range, Global Pitch Bend; Q-Link, Notes Config, Touch FX Setup); Preferences > Hardware alapértelmezett mód; hold-gesztus módválasztó a kijelzőn.
- Touch Sense (Note On érintésre, Low/High Range) beállítás nincs.
- A strip melletti LED-sor: a doksi szerint nincs szoftveres funkciója (nem hiba).

### 3.3 Szekvenciafelvétel (MD L68-L90, AC1)
- Megépült és mért (lásd 1.), de DUMMY-k: nincs külön SEQ REC ARM kapcsoló; Rec Arm Behavior (Single/Multi) fix Multi; élesített sáv nélkül nem rögzít; **nincs count-in** (a metronóm count-in beállításnak csak "Off" eleme van); nincs azonnali punch-in; "next bar" indításnál nem rögzít; REC közben a gomb kiléptet (punch out); nincs felvétel-quantize beállítás; REC/OVERDUB LED-szín felhasználói döntés (REC piros, felvételkor villog; OVERDUB zöld), kamerás ellenőrzés hiányzik (BEH/TO: LED jó a felhasználó szerint).

### 3.4 Gombok és vezérlők (DVK; TODO §2)
- **Locate** gomb: csak flag + `<`/`>`/`<<`/`>>` kombinációk; a Locate ablak nincs.
- **ARP:** valódi arpeggiator kész (Up/Down/UpDown/Random), de nincs UI/Q-Link a mód/ráta/oktáv/gate állításához; arp csak futó transporton lép; track-váltáskor a régi track tovább arpegiál; nincs ARP LED-frissítés track-váltásra; NOTE REPEAT-nek nincs fogyasztója a Kickben (TODO T24 L214).
- **Full/Half Level user level** (nyomva tartva egyéni sebesség) - nincs a kézikönyvben leírva, nincs megvalósítva (0947f1f5).
- **Stop dupla nyomás** = azonnali panic (DVK L18; RPT L254 "nem vizsgált"; minden STOP all-notes-off-ot küld).
- **Erase** (nyomva tartva + pad/billentyű; Erase ablak), **Copy/Delete** (From Pad/To Pad), **16 Level** (utolsó pad másolata mind a 16 padon), **Notes mód** (skálák/akkordok): `EN_*` csomag, működés "nem vizsgált" (DVK L36-L39); 16 LEVEL+Shift marad a MenuPadPerform (döntés).
- **Pad nyomásérzékenység** kikapcsolva (`kSendPadAftertouchPackets=false`); nyomás-only DCL-üzenet "reggeli teendő" (R2 L38-L40).
- **Q-Link érintés** szándékosan üres (`on_encoder_touch`), mert a Kicknek nincs knob-touch fogalma (T11).
- **Q-Link oszlopváltás, Shift+Q-Link Configuration ablak**: nem vizsgált (DVK L12).
- **Tap Tempo Shift** saját vs globális tempó piros/fehér állapot: nem vizsgált (DVK L42).
- **Metronome Shift/hold Click ablak, Menu tartalma, Track View Mode egyezés**: nem vizsgált.
- **Shift+TC / Shift+AUTOMATION:** Automation Shift most csak Off (a doksi szerint ki/be); TC+Shift a globális TC-t kapcsolja DCL nélkül (DVK L25, TODO T30).
- **Oktáv-LED** a félhang-transzponálást nem mutatja (T8).
- **Sustain:** a pedál az arp hangjaira nem hat; ONE_SHOT kihagyva; pedál csak élő bemenetre (Behringer-elv) (R1 döntés 1, R2 pont 7).
- **Pitch bend** csak keygroup és dob programra, fix +-2 félhang (TODO döntés 3).
- **Song módban a seek tiltott**; globális TC csak Apply-ra érvényesül (MD L10-L20).

### 3.5 Hardver I/O, amit az adapter nem néz (TODO §2.5)
- Audio kimenet: Main és Phones azonos jelet kap; Outputs 3/4 nincs; hard clip nincs limiter; latency-kérés (30 ms) ellentmond a "5.8 ms" logüzenetnek (csak mpc_gui).
- Multitouch: csak egy ujj bizonyított; "Multitouch (2 ujj)" CHK §5 próbálandó.
- DIN MIDI IN/OUT/THRU (a JUCE MIDI manager viszi?), USB-A/USB-B gadget: nem része az adapternek, nem vizsgált.
- **Controller Mode / telefon-tablet** használat (Kick telefonon + Key 61 mint kontroller): csak kutatás, hardveren nem mérve, a források ellentmondóak (`KEY61_USB_AND_PHONE.md`). Következő mérés: Key 61 Controller Mode + Mac.

### 3.6 Motor / hang (tervek, `docs/plans/`)
- **Terv 01 (telepíthető pendrive):** csak terv; a pendrive-os indulás ma működik; rootfs csak olvasható; hook-telepítő egyelőre csak hivatkozás (35fce454).
- **Terv 02 (zongora-CLAP):** a mostani `KeygroupProgram` nem elég zongorának: nincs release-ág, burkológörbe, szűrő, round-robin, voice stealing, loop-crossfade, streaming; velocity-rétegek max 4; generált projekt ONE_SHOT, ezért pedál/note-off hatástalan; minták float32 sztereóban a RAM-ban; Kicknek csak CLAP-host oldala van, nincs `clap.gui` (sem host, sem plugin oldalon); mintaforrás-döntés A+D ajánlott, nincs eldöntve (b650370e; plans/02 L7-L18). A multisample zónák/rétegek (b101e5b8) "Not yet auditioned on the keybed (build 49)".
- **Terv 03 (plugin-ökoszisztéma):** Vital-alapú saját szinti (munkacím SynthyBoy; név/védjegy-ütközés ellenőrzése hátra), `.kickplug`/manifest-URL, MCP-szerű letöltés, harmadik fél publikálás; "terv, nyitott kutatással".
- Lásd még: `research/codebase-survey.md`, `sample-sources.md`, `architecture-and-publishing.md`.

## 4. Felderítetlen / nem vizsgált (explicit "nem vizsgált" a forrásban)
- Kick/apps/*/README: nem létezik (`Kick/apps/` alatt nincs README; csak `kick/AGENTS.md` és `kick/CLAUDE.md`).
- `Kick/README.md`: "Desktop and emulator tests cannot certify board behavior" (L64) és a CLAP benchmark-eket a célhardveren kell lefuttatni a támogatott voice/buffer limitek előtt (L66). Nem ad külön hibalistát.
- `Kick/how-to/Kick_Compilation.txt`: build-leírás, nincs hibalista.
- Nem találtam `known issue`/`FIXME` jelölést a Kick/tests alatt; a tesztek nem fedik: a hidat integrációban (kick_hardware_bridge.h), a motor hangkimenetét, az ARMv7 viselkedést, a hardveres LED/pad RGB-t; ezek csak MPC-n mérhetők (CHK).
