# MPC Key 61: mit mond a dokumentáció, és mit csinál a Kick

**Szabály (felhasználó, 2026-11-01):** egy funkció fejlesztésénél előbb a dokumentációban keressük meg a leírását, és az ottani leírás alapján készítjük el. Hipotézis helyett tények: minden sor a dokumentáció állítása és a Kick **mért vagy kódból olvasott** állapota. Ahol nem néztük meg, az áll: "nem vizsgált".

Forrás: *MPC Standalone OS – User Guide v3.10* (528 oldal; Key 61 fejezet "Top Panel" 365–369. oldal; a számok a vezérlő-leírás sorszámai), *MPC Key 61 Quickstart Guide v1.2*. Lásd: [README.md](README.md). A tények a [HW_TEST_REPORT](hw-test-report.md) köreiből jönnek.

## 1. Vezérlők (v3.10 Key 61 vezérlő-leírás)

| # | Vezérlő | Dokumentáció | Kick (mért / kód) | Eltérés |
|---|---|---|---|---|
| 2–4 | Data Dial, –/+ | görgetés / érték; a dial megnyomása = Enter; –/+ érték léptetése | `jog`, `ENTER`, `PLUS/MINUS` kezelve (kód); a –/+ LED-je a firmware alapfénye (kék) | nem vizsgált: működés a képernyőn |
| 3, 5 | Q-Link gombok, Q-Link gomb | érintésérzékeny gombok, 4 oszlop; a gomb váltja az oszlopot (a fények mutatják); Shift+Q-Link: Q-Link Configuration ablak, még egyszer: vissza a főoldalra | Q-Link 1 forgatása CC16 relatív (mért); `QLINK` → `EN_LINK`, Shift → `MenuKnobEdit` (kód); az első oszlop-LED mindig piros | nem vizsgált: oszlopváltás, a Shift+Q-Link ablak |
| 6 | `<` / `>` | az audio pointert egy lépéssel mozgatja; Locate+: előző/következő esemény | `stepPlayhead` (TC rács), Locate+: `locateToEvent` (kód). **Megállítva semmi nem történik** (fizikai és injektált), lejátszás közben működik (mért) | a doksi szerint a lépés mértéke a **Locate ablak "Step" mezője** ("tap the Step and Bars arrow buttons to move the playhead by the specified amount"); a Kick a TC-rácsot használja. Megállított seek nem érvényesül (11. hiba) |
| 7 | Locate | megnyitja a **Locate ablakot** (Time fül: Bars:Beats:Ticks, Loop, Loop Start/End, Auto Record; Locators fül: 6 locator); nyomva tartva a `<` `>` `<<` `>>` másodlagos funkciói | nyomva tartás kezelve (`m_locate_pressed`); az ablak megnyitása: nem vizsgált | nem vizsgált: Locate ablak a Kickben |
| 8 | `<<` / `>>` | egy ütemmel balra/jobbra; Locate+: a szekvencia eleje/vége | `seekBars`, Locate+: bar 1 / utolsó ütem (kód); megállítva nincs hatás, lejátszás közben működik (mért) | megállított seek (11. hiba) |
| 9 | Rec / Recall | Rec = felvétel-élesítés; Play vagy Play Start indítja; ez törli a szekvencia eseményeit; egy kör után az Overdub bekapcsol. Shift+Rec = Recall (a felvétel nélkül játszott MIDI-hangok beszúrása) | `EN_RECORD` csomag; Shift: `recallLiveMidi` (kód); **a REC LED soha nincs vezérelve** (mért: halvány szürke élesítve is) | REC LED hiányzik; a felvétel-folyamat nem vizsgált |
| 10 | Overdub | Overdub felvétel a meglévő hangok törlése nélkül, felvétel előtt vagy közben | `EN_OVERDUB` csomag; **OVERDUB LED nincs vezérelve** (mért) | LED hiányzik |
| 11 | Stop / Return | Stop; **dupla nyomás = azonnal minden hang leállítása**; Shift+Stop = leállít és a playhead a Loop Startra | Stop és Shift+Stop leállít, a Shift a pozíciót 1. ütemre parkolja (kód); dupla nyomás: **nem vizsgált** | nem vizsgált: dupla nyomás; "Loop Start" vs "1. ütem" |
| 12 | Play | az audio pointer jelenlegi pozíciójától játszik | megállítva elléptetett pointerrel **a kezdetről indult** (mért, PLAY +4,5 s → `3:01:083`) | 11. hiba |
| 13 | Play Start | a szekvencia kezdőpontjától | `setSequencePlay(.., true, true)` (kód) | nem vizsgált |
| 14 | Edit / XYFX | **v3.10: "Track Edit Mode"** (a Quickstart v1.2: Program Edit); Shift+Edit: XYFX (XY pad, két effekt-paraméter) | EDIT fizikailag a Program Editet nyitja, Shift+EDIT az XY Effects képernyőt (mért) | az EDIT megfelelő képernyője a v3.10 szerint a Track Edit; a Kickben Track Edit nincs külön, "Program Edit" van |
| 15 | Grid / Step Seq | Grid View; Shift: Step Sequencer | Grid View / Step Sequencer (mért) | nincs |
| 16 | Mix / Pad | Track Mixer; Shift: Pad Mixer | Track Mixer / Pad Mixer; LED fehér / piros (mért) | nincs |
| 17 | Mute / Pad | Track Mute; Shift: Pad Mute | Track Mute / Pad Mute; LED fehér / piros (mért) | nincs |
| 18 | Automation Read/Write / Off | a globális automation Read ↔ Write váltása; Shift: a globális automation **letiltása vagy engedélyezése** | Read ↔ Write, **Shift = Off** (kód); LED zöld/piros/sötét (mért) | a doksi szerint a Shift **ki/be kapcsol**; a Kick csak kikapcsol (Off után a sima gomb Readre visz) |
| 19 | Keyboard Control / MIDI CTRL | **Keyboard Control menü** (Keyboard Input: Transpose, MIDI Channel, Aftertouch, Note On Velocity; Scale Filter; Internal/External Keyboard Routing; Keyboard Settings: Velocity Response/Curve/Gain, Black/White Key Sensitivity; Pedal Settings); Shift: MIDI Control Mode | KEYBOARD CONTROL a **Pad Perform (Pad Notes)** képernyőt nyitja; Shift: MIDI Control (mért) | **a Keyboard Control menü hiányzik** |
| 20 | Sample Edit / Sampler | Sample Edit Mode; Shift: Sampler | Sample Editor / Sampler (mért) | nincs |
| 21 | Timing Correct / On/Off | megnyitja a **TC ablakot**; Shift: a Timing Correct be/ki. A TC ablakot a doksi szerint **Do It-tal (alkalmaz) vagy Close-zal (mégse) kell bezárni, érintéssel** | a TC gomb megnyitja az ablakot (fizikai, mért); Shift+TC után az ablak "Global Timing Correct: Off"-ot mutatott (mért) | a hardveres bezárásról a doksi nem szól; érintés kell (érintés hiba) |
| 22 | Undo / Redo | Undo; Shift: Redo | fizikailag működik (felhasználó) | nincs |
| 23 | Shift | **nyomva tartva** működik; (v3.10 más MPC-knél) dupla nyomás: a másodlagos funkciójú gombok néhány másodpercre villognak; Shift-zárolást nem említ | bal SHIFT LED mindig, jobb csak Shift-nyomásra (mért); csak a note 50 vezérelt | LED eltérés; dupla nyomás nincs megvalósítva (nem vizsgált) |
| 24 | Menu / Browse | a mód-menü; Shift: Browser | `MENU` → `HomeOptions`, Shift → File Browser (kód); a Shift+MENU Browsert nyit | nem vizsgált: a menü tartalma |
| 25 | Main / Track | Main Mode; Shift: **Track View Mode** (a szekvencia sávjainak áttekintése, key range-ek) | MAIN → Home lista; Shift → "Track" fül (kód) | nem vizsgált: megegyezik-e a Track View Mode-dal |
| 26 | Sounds / Favorites | **Sounds Mode**: a beépített hangszerek és preset-ek böngészője; Shift: Favorites | a SOUNDS a **File Browsert** nyitja; Shift: Favorites a File Browserben (kód, mért) | a Sounds Mode hiányzik |
| 27 | Pads | sebesség- **és nyomásérzékeny**, a színek a funkciótól függnek | a pad egyszer játszik (mért); pad aftertouch DCL csomag kikapcsolva (cloud, `b0f532f8`) | pad nyomás: nincs használva |
| 28 | Pad Bank gombok | A–D; Shift: E–H; **dupla nyomás** is E–H | A–D: egyszerre egy világít (mért); Shift: bank 4+ (kód); dupla nyomás: nem vizsgált | nem vizsgált: E–H, dupla nyomás |
| 29 | Full Level / Half Level | Full Level: mindig 127; Shift: Half Level 64; **nyomva tartva egyéni "user level"** sebesség a Half Level helyett | Full/Half (kód) | nem vizsgált: user level |
| 30 | 16 Level / Notes | 16 Level: az utoljára ütött pad másolata mind a 16 padon, egy paraméter nő (Velocity, Tune, Filter, Layer, Attack, Decay); Shift: Notes mód (skálák/akkordok), Pad Perform ablak | `EN_16LEVELS`; Shift → Pad Perform (kód) | nem vizsgált |
| 31 | Erase | lejátszás közben nyomva tartva + pad/billentyű: az esemény törlése; megállítva: Erase ablak | `EN_ERASE` csomag (kód) | nem vizsgált |
| 32 | Copy / Delete | Copy: pad másolása (From Pad, To Pad, Do It/Cancel); Shift: Delete | `EN_COPY` / `EN_ERASE` (kód) | nem vizsgált |
| 33–35 | Touch Strip, Touch Strip/Config, Touch FX/Config | a gomb **12 módon** lép végig: Q-Link, Pitch Bend, Pad Level, Mod Wheel, Track Level, Sustain, Touch FX, Expression, Notes, Crossfader, Note Strum, Off; nyomva tartva a módok közül választás; Touch FX gomb: Touch FX mód; Shift+gomb: a mód beállításai; Touch Sense: érintésre Note On | a Kick **3 módot** ismer (slider, pitch bend, mod wheel); a strip bemenete működik (1040 nyers üzenet, CC33/CC34); Touch FX gomb → XY képernyő (kód) | **9 mód hiányzik**; a "Light LED" paraméterről a doksi: "it does not actually have a function on your MPC hardware" — a strip LED-sora nem vezérelhető |
| 36 | Note Repeat / Latch | Note Repeat nyomva tartva + pad: ismételt hang, a rate a tempón és a TC-n múlik; Shift+gomb: Latch | `EN_NOTE_REPEAT`, Shift: `EN_LATCH` (kód); LED világít (mért) | nem vizsgált: működés |
| 37 | Tap Tempo / Global | koppintás = tempó; nyomva tartva a dial állítja; Shift: **saját tempó = fehér, globális = piros** | `EN_TAP_TEMPO` / Shift: `EN_MASTER` (kód); a LED bekapcsolva kékes-fehér (mért) | a piros "globális" állapot nem vizsgált |
| 38 | Metronome / Config | be/ki; Shift vagy nyomva tartva: Click/Metro ablak | METRONOME (kód) | nem vizsgált |
| 39–40 | Arp, Latch | Arp be/ki (a tempó és az Time Division szerint); Latch: az arp a billentyűk felengedése után is megy | `updateArpLeds` (kód); LED világít (mért) | nem vizsgált: működés |
| 41 | Oct –/+ | oktáv; **az oktáv-fények (gomb alatt) a jelenlegi oktávot mutatják**; Shift: félhang; mindkettő nyomva + billentyű: a transzponálás erre; mindkettő nyomva és felengedve: reset | `KeyTranspose` (kód); az oktáv-sor kijelzése: a fotón két piros pont (−1, 0) alapból | nem vizsgált: oktáv-sor vezérlése, "mindkettő + billentyű" |
| 43–44 | Pitch/Mod kerék | pitch bend / moduláció | élő MIDI ch1 (kód); az első track hangereje megugrott (felhasználó); a cloud az okot a motor `reloadMidiMeter`-ében találta | javítás a cloudnál |
| 45 | Main Volume | a kimenetek és a fejhallgató hangereje; nem ír analóg/MIDI/szoftveres mivoltáról | SysEx a Port 1-en (mért: `F0 47 7F 4B 5B 00 01 <v> F7`, v 0x00..0x75); a Kick master szintjére kötve (AD1a, hardveren nem igazolt); korábbi "nem MIDI" állítás hibás volt | a doksi nem mond több; a `0x5B` parancsbájt jelentése ismeretlen |

## 2. Hardveres és szoftveres elemek, amiket a Kick még nem néz

- **Locate ablak** (v3.10, "Locate window"): idő/Loop/Auto Record/6 Locator.
- **Sounds Mode** (beépített hangszerek, preset-ek) és **Favorites**.
- **Keyboard Control menü**: billentyűzet-bemenet, skála-szűrő, routing, velocity response, pedál-beállítások.
- **Touch Strip módok**: 9 hiányzik.
- **Menu** (a mód-menü tartalma).

## 3. Mit tévesztettem el hipotézissel (visszavonva)
- **TC ablak bezárása hardverrel (STOP/ENTER = Apply):** hipotézis volt. A doksi szerint az ablakot **érintéssel** (Do It / Close) kell bezárni; a "lefagyás" oka az érintés hiánya. A riport 9. hibájának javítási javaslata ennek megfelelően: az érintés javítása.
- **Touch strip LED-sor CC-száma:** hipotézis volt; a doksi szerint a LED-ek nem vezérelhetők (12. hiba visszavonva).
- **Peer/multitouch magyarázat az érintés hibájára** (`InputSourceType::touch`, `xi.opcode`): hipotézis, nem tény; a mért tény: a nyers koordináta jó, a hit-test a helyes gombot találja, a képernyő nem vált.
