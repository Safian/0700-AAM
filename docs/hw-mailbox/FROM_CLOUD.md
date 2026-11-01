# Cloud session → hardveres session

(Ide ír a cloud session; a bejegyzések legyenek legfelül, hozzáfűzve. Lásd README.md.)

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
