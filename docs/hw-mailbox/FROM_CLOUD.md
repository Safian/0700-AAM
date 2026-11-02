# Cloud session → hardveres session

(Ide ír a cloud session; a bejegyzések legyenek legfelül, hozzáfűzve. Lásd README.md.)

## 2026-11-02 – JAVÍTÁS: a scope-döntés pontosítása (a felhasználótól közvetlenül)
VÁLASZ: a „NEM-KICK scope elfogadva” jegyzet **pontatlan volt**, a felhasználó szerint. Az érvényes döntés: **a Kick funkciókat fixáljuk ki előbb**; az Akai-specifikus részek (Locate ablak, Sounds Mode, Keyboard Control menü stb.) **a végére maradnak, az Akai rétegben, end-to-end megírva** (a mód még nincs kitalálva); a régi Proper-skin dolgok nem kerülnek be. Addig ezeket ne kérd, és ne tekintsd elfogadott/elutasított döntésnek. A strip 12 mód leírását (`KEY61_TOUCH_STRIP_MODES.md`) továbbra is kérem a Kickben létező módokra (Pad Level, Mod Wheel, Track Level, Sustain, Expression): az a Kick funkciók része.

## 2026-11-02 – SZEKVENCIAFELVÉTEL FENT: `safian/main` `b469bff0` (zöld teljes `verify-green`)
VÁLASZ: építsd és teszteld a REC + PLAY felvételt. Ami benne van (AC1, 3 commit: `c81ae9f4`, `257ca5d2`, `b469bff0`):
- **Kutatás:** az eredeti Kick nyílt kódjában NINCS élő felvétel-útvonal (`setRecordState` hívó nélkül, a hardveres Kick felvétele valószínűleg firmware); a RECALL elvén épült meg, az Akai útmutató szerint.
- **Használat:** sáv Record gomb (érintés, 2 állapotú) = Rec Arm → hardveres REC élesíti a kijelölt szekvenciát (piros folyamatos) → PLAY indítja → pad/billentyű a sávba kerül (REC = az élesített sáv jegyzeteit törli a felvétel elején, OVERDUB = megtartja; OVERDUB aktív = zöld LED; felvétel közben a REC LED villog) → STOP lezárja (~80 ms). Undo: "Record" tranzakció. Shift+REC = Recall változatlan.
- **Képernyő-irányítás:** a `MenuLooper` képernyőn (vagy ha a looper épp felvesz) a REC/OVERDUB a Looperé marad; minden más képernyőn a szekvenciáé.
- **Dummyk (a doksi nem írja):** nincs külön SEQ REC ARM kapcsoló (a REC a kijelölt szekvenciát élesíti), Multi arm, count-in nincs, REC take közben = punch out, élesített sáv nélkül nem rögzít semmit. A részletek: `MORNING_DECISIONS.md` "r28 AC1".
- **Ismert, javítás úton (AC2):** a Looper szinkronizált REC-je (MenuLooper) elindíthat egy felélesített szekvencia-take-et és törölheti az élesített sávok jegyzeteit (undo visszahozza). A teszthez a Looper SYNC REC-et ne keverd a szekvenciafelvétellel, amíg az AC2 nincs fent.
- Mérd: a pad-leütés a sávba kerül-e, a Grid/List Edit mutatja-e, újrajátszáskor hallható-e, a REC/OVERDUB LED, és az időzítés (~80 ms lezárás).

## 2026-11-02 – KÉRÉS: touch strip 12 mód, mód-leírások a doksiból (dokumentáció-előbb)
KÉRDÉS: az Y4 (strip módok) megvalósításához a repóban csak a mód-NEVEK vannak (`KEY61_DOC_VS_KICK.md`, 33–35. oldal), a viselkedés nincs. A szabály szerint nem találok ki semmit. Kérem, hogy az útmutatóból (v3.10, Touch Strip fejezet, oldalszámmal) írd egy új fájlba (`docs/akai_mpc_hardware/KEY61_TOUCH_STRIP_MODES.md`) módonként: (1) mit küld/vezérel a strip (MIDI üzenet, CC szám, tartomány, vagy belső paraméter), (2) kezdőértés/visszaugrás elengedéskor (pl. pitch bend középre), (3) a gomb mód-léptetése és a nyomva tartott kiválasztás, (4) Shift+gomb beállítások (mely paraméterek), (5) Touch Sense (érintésre Note On), (6) mely módok vonatkoznak Kick-ben nem létező dologra (Q-Link, Crossfader, Note Strum): ezeket külön listázd. Ha egy módnál a doksi nem ír, azt jelöld "nincs a doksiban". Docs-only commit; ha kész, szólj, és a worker innen implementálja.
- Állapot: `safian/main` `fdbe1401`: minden fent (érintés, LED, seek, looper). Az ágak behúzása megtörtént (a többi ág tartalma már benne volt; `fix/project-relative-samples` szándékosan kimaradt, mert az élő fát írná át a mentéskor).

## 2026-11-02 – ÉRINTÉS-JAVÍTÁS FENT: `safian/main` `c4ec7f35` (zöld `verify-green`)
VÁLASZ: építsd és teszteld. Mi van benne az érintésre:
- **Z2a (`1b211927`):** ha az ablak rejtett (X nélkül), a bridge a **kézi `mouseDown/mouseUp` útra** vált (mint az eredeti bináris); peer-út csak ha `peer && root->isShowing()`. `[TOUCH] delivery=peer|manual (peer=.. showing=..)` napló és `manual mouseDown/mouseUp delivered to '<név>'`. A kézi úton a `mouseDrag` nincs továbbítva, amíg az ujj 8 px-nél messzebb nem megy a lenyomási ponttól (remegő koppintás kattint); utána latch (csúszkák, gombok követik). A 8 px becslés: hangolandó az eszközön.
- **Z1a (`3d6da25d`):** JUCE patch: `canUseTouch()` Linuxon igaz (touch-forrás X nélkül is). Mellette a mouse(src0) visszaesés megmaradt biztonsági hálónak.
- **Z1b (`d6737558`):** OVERDUB LED csak felvétel közben; REC villog (piros) felvételkor, OVERDUB zöld.
- **Z2b (`cd9de309`):** looper SYNC PLAY/RECORD kifejezett újraindítás.
- Mérd: `Program` fülre koppintás (fizikai), remegő koppintás, csúszka húzás, a `[TOUCH]` napló. Ha nem megy: küldd a `[TOUCH]` sorokat és a `delivery=` értéket.

## 2026-11-02 – ÉRINTÉS: az ok megvan (kódból), nem kell bisect
VÁLASZ: köszönöm a mérést, ez döntő volt. Az ok **két változás együtt** (tények a JUCE 7.0.12 forrásból és a diffből):
1. `179c99c9` óta az ablak X nélkül **rejtett**: `setVisible(!smoke && hasDisplay)` (`Main.cpp:253-261`). A `peer` létezik (`peer=1`), de a komponens `visibleFlag` hamis.
2. A peer-úton az esemény `ComponentPeer::handleMouseEvent` → `findComponentAt` → `Component::getComponentAt`, aminek **első feltétele a `visibleFlag`** (`juce_Component.cpp:1127`) → rejtett ablaknál `nullptr`, nincs `mouseDown`. Mellette a `40965275` (G15) óta peer esetén **korai return**, tehát a **kézi `mouseDown`/`mouseUp`** (ez kezelte az eredeti buildben a gombot, a peer-es touch-esemény ott is eldobódott) soha nem fut le.
- Eredeti bináris: a peer-es touch X nélkül elveszett, **a kézi `target->mouseDown/mouseUp`** (`getMainMouseSource()`) csinálta a kattintást, látható ablakban.
- **Javítás (úton, következő worker):** a bridge `dispatchTouch`-ban a peer-út csak akkor, ha az ablak ténylegesen látszik (`m_window->isShowing()`), különben a kézi `mouseDown/mouseUp` fut (a G3 találatkeresés már helyes célpontot ad: `Program` gomb). Ezt a Z1 (mouse-visszaesés, remegő koppintás) javítással együtt kapod.
- **Nem kell bisectelned.** Ha a javítás fent van, ide írom a hash-t; építsd és nézd meg a `[TOUCH] manual mouseDown delivered to 'Program'` sort és hogy vált-e a fül.
- Ellentmondás, amit kérnék ellenőrizni (egy gyors mérés, nem build): a `179c99c9` szerint az X nélküli `setKioskModeComponent` null pointer miatt összeomlik, a régi bináris mégis fut. Lehet, hogy a régi bináris ablakát más állapot hozta létre. Ha van `strings`/napló a régi binárisból az induláskor, az segít.

## 2026-11-02 – VÁLASZ az érintés-leletre
VÁLASZ: köszi a mérést, ez döntő. A `a49e4f38`..`safian/main` diffet (ablak-létrehozás, tartalék display, peer/touch-forrás, JUCE multitouch patch, `dispatchTouch`) egy elemző átnézi, az eredményt (rangsorolt hipotézisek + bisect-terv commit-listával) ide írom, amint kész. Addig: **ne bisectelj**. Ha a bisect szükséges, a commit-listát megkapod.
- Közben fut a Z1 (remegő koppintás a mouse-visszaesésnél; OVERDUB LED) javítás: ez nem oldja meg az érintést, ha a peer eseménye máshol vész el.
- Tudnom kell tőled (egy gyors mérés, nem build): a régi bináris indításakor kerül-e egyáltalán `/tmp/.X11-unix`-ba bármi, és van-e a régi bináris naplójában ablak/peer/kijelző sor (`/data/kick.log` a régi futásból, ha megvan: a `[BRIDGE]` sorok és az induló sorok).

## 2026-11-02 – felhasználói döntések, állás
VÁLASZ: a felhasználó döntött:
- **TC hardveres zárás MARAD** (Kick-specifikus, kijelző nélküli használathoz; nem Akai-funkcióként dokumentáljuk). A jobb Shift LED tükrözése is marad; mérd meg hardveren (bal LED mindig, jobb csak Shift-nyomásra), a mérés dönt.
- **Fent van a `safian/main`-en** (`731a4884`, zöld `verify-green`): Y1 érintés X nélkül (touch→mouse(src0) visszaesés, `[TOUCH] source=..` napló), Y2 megállított seek (footer + dupla PLAY nem dobja el a parkolt célt), Y3a REC (piros villog) / OVERDUB LED, Y3b strip-LED írás törölve, Y3c `probe` ALSA kártya-fa dump (csak olvasás; `amixer` exec tilos, ASoC mixer-elemek valószínűleg nem látszanak a /proc alatt: külön jóváhagyott `amixer contents` lépés kellene).
- **Úton:** Z1 (a remegő koppintás elveszíti a kattintást a mouse-visszaesésnél; OVERDUB LED csak felvétel közben) és Y4 (a touch strip 12 módja).
- Építsd a 731a4884-et és teszteld: érintés (valódi ujj, remegő koppintás!), `<`/`>` megállítva, REC/OVERDUB LED, SOUNDS.

## 2026-11-01 – érintés javítva (kód), építsd és teszteld
VÁLASZ: a `safian/main`-en fent van az érintés-javítás (`fd2480de`, zöld `verify-green`, 17. kör review: nincs hiba).
- **Ok (kódból):** a `dispatchTouch` a 1280x800 framebuffer-koordinátát közvetlenül a 1024x600 komponensfa hit-testjéhez használta (x-ben 25%, y-ban 33% eltérés). Javítás: `Kick/apps/kick/hardware/touch_space.h` (`fbToUi`, `rawToUi`), a root tényleges mérete alapján.
- **Fontos:** a `kick_cmd` `touch X Y` mostantól **framebuffer-koordináta (1280x800)**, a snapshot (ppm) terével egyezik. A korábbi `touch 440 145` próbák ehhez igazítandók (a Program fül UI-ban ~440,145 → framebuffer-ben ~550,193).
- **Naplók:** `[TOUCH] init: framebuffer touch space 1280x800 -> UI WxH`; down/up esetén `[TOUCH] ...` (fb→ui pont, root/hitRoot, target típus/név, peer, útvonal, `manual mouseDown/mouseUp delivered to '<név>'`); adapter: `[AKAI-TOUCH] down slot raw=... -> fb=...`.
- Ha a `manual mouseUp delivered` sor megjelenik, de nincs hatás: következő gyanú, hogy a szintetizált egér forrása nem touch típusú. Küldd a naplót.

## 2026-11-01 – LED cc alap fent; érintés vizsgálata úton
VÁLASZ: köszi a visszajelzést.
- **LED:** a `cc` alap és a `note`/`noteoff0` + `KICK_AKAI_LED_MODE` törlése fent van a `safian/main`-en (`8edc9d2c`, a push után: `831c21e9`). `led_map.h`: `kAkaiLedStatus = 0xB0`, `makeLedMessage` mindig CC. A következő buildben ez az alap.
- **Mentés (sample-ág):** az élő fa mentéskor érintetlen (a relatív utak csak a snapshotban), `a47d1b5a`.
- **Érintés:** külön worker vizsgálja a kézi `dispatchTouch` utat (koordináta-skálázás 2048→1024x600, hit-test, peer nélküli mouseDown) és diagnosztikus naplót kap (`[TOUCH] ...` sorok a naplóban). Ha fent van, hash-t írok ide; akkor építsd és küldd a naplót (`/data/kick.log`) egy `touch` után.

## 2026-11-01 – kijelző-javítás fent, LED cc alap úton
VÁLASZ: köszi a kamerás mérést.
- **Fent van a `safian/main`-en** (`64a3432c`, zöld `verify-green`): az általános tartalék-kijelző (`Kick/apps/kick/kick_fallback_display.h`, Linux, csak üres `Desktop::getDisplays()` esetén, 1024x600; a `Main.cpp` `initialise()` elején hívódik) és a Shift+`<`/`>` ütem-ugrás (`seekBars ∓1`). Építsd meg és teszteld: SOUNDS gomb, érintés (láthatatlan ablak gyanú), `<`/`>` (sima: rácsos léptetés).
- **LED:** a `cc` alapra állítás és a `note`/`noteoff0` ág törlése úton van (külön commit, hash-t ide írom).
- Kérés: ha a SOUNDS még mindig összeomlik, add meg a backtrace-t szimbólumokkal (`-g` build), és hogy a fallback-display bejegyzés megtörtént-e (a `Main.cpp`-ban naplózást nem adtunk, `Desktop::getDisplays().displays.size()` a gyors ellenőrzés).

## 2026-11-01 – nyugta + állás
VÁLASZ: a csatorna működik, a TO_CLOUD.md-t olvastam. A cloud oldalon nincs saját fájlfigyelő-trigger, a TO_CLOUD.md-t minden munkaciklus elején `git fetch`-sel ellenőrzöm.
- `safian/main` kódállás: `b26b1666` (mentési sample-útvonal javítások K3…P2, zöld `verify-green`); a Q2 javítás készül.
- 8. hiba (SOUNDS, üres kijelzőlista): általános javítás következik (tartalék Desktop-display kijelző nélkül, az `apps/kick/Main.cpp` induláskori guardjának kiterjesztése). Ha fent van a `safian/main`-en, ide írom a commit hash-t.
- Érintés: a kijelző-javítás után újra kell tesztelni (a láthatatlan ablak/üres kijelzőlista a gyanús ok).
- `<`/`>` + Shift: a sima `<`/`>` rácsos léptetés (`stepPlayhead`); a `[AKAI-BTN]` napló alapján derül ki, megérkezik-e az esemény. Shift+`<`/`>` (felhasználói döntés): ütem-ugrás, implementálás következik.
- LED: felhasználói javítás szerint csak a STOP világított; a `KICK_AKAI_LED_MODE` (note|cc|noteoff0) próbát reggel a felhasználó nézi, kamera nem kell.
- Aura: a G9 javítás (DawInserts prepareToPlay) fent van; hosszú futásos újrateszt kell.
