# Terv 03 — Kick plugin-ökoszisztéma (saját Vital-alapú szintetizátor + telepíthető, harmadik féltől származó pluginok)

Állapot: terv, nyitott kutatással (lásd 02). Owner döntés (2026-11-04): az út lefektetett — Vital-alapú, Serum-minőségű saját szintetizátor; a fejlesztése közben dolgozzuk ki a teljes Kick-plugin infrastruktúrát; a letöltés MCP-szerű; mi adjuk a backendet; **harmadik felek is publikálhatnak**: aki megcsinál egy VST-instrumentet a megfelelő formátumban és kirakja egy linkre, az az appba behúzható.

## Célok
1. **Saját szintetizátor** a Vital (GPL-3.0, `mtytel/vital`) forrására építve, AGPL-3.0 alatt (GPLv3→AGPLv3 kombinálható). Új név és márka — munkacím: **SynthyBoy** (a magyar „szinti” szójáték; védjegy-/névütközés ellenőrzése még hátra van); a Vital gyári presetjei/hullámtáblái NEM használhatók (külön licenc) → saját gyári presetek és hullámtáblák kellenek. Cél: Serum-szintű hangminőség és bánásmód.
2. **A fejlesztés a platformot is megadja**: plugin-formátum, telepítő, betöltő, UI-beágyazás (`clap.gui`), állapot/preset-kezelés, ARMv7 build — ezeket a szintivel együtt építjük, nem külön.
3. **Telepítés linkről / backendből** (mint az MCP-szerverek): link beillesztése vagy behúzása → letöltés → ellenőrzés → telepítés.
4. **Nyitott ökoszisztéma**: bárki publikálhat egy plugint egy URL-re (manifest + bináris); a Kick betölti, a felhasználó jóváhagyásával.

## Csomag-formátum (javaslat)
`.kickplug` (zip) vagy önálló **manifest URL** (JSON), mezők:
- `id`, `name`, `version`, `author`, `license` (SPDX), `homepage`, `description`, `kind` (instrument/effect), `min_host_version`.
- `builds`: platformonként (`linux-armv7`, `linux-aarch64`, `linux-x86_64`, `macos-universal`, `windows-x64`) → `{url, sha256, size}`; a bináris `.clap` (a CLAP az elsődleges formátum, mert a host ezt tudja; VST3 csak CLAP-wrapperen át, külön döntés).
- `assets` (minták, presetek) külön, SHA256-tal, opcionálisan letölthetően.
- `permissions` (pl. hálózat: nincs; fájlrendszer: csak a saját mappa).
- `signature` (a publikáló kulcsa) + a publikáló kulcs-azonosítója.

## Telepítési folyamat
1. Bemenet: link beillesztése / behúzás / `kick://install?url=…` / QR a telefonról (az MPC-n nincs billentyűzet: a link a telefonról/gépről érkezik a Kick webes vezérlőfelületén vagy pendrive-on).
2. Manifest lekérése, séma- és aláírás-ellenőrzés, a platformnak megfelelő build kiválasztása.
3. **Bizalmi szint** megjelenítése: *Hivatalos* (a mi kulcsunkkal aláírt, a mi build-farmunkon fordított) / *Ellenőrzött publikáló* (regisztrált kulcs) / *Ismeretlen* (aláíratlan vagy ismeretlen kulcs → kifejezett, nem alapértelmezett jóváhagyás, figyelmeztetéssel).
4. Letöltés, SHA256-ellenőrzés, atomi telepítés `…/plugins/<id>/<version>/`, az előző verzió megtartása (visszaállítható), a Kick-nek a pendrive-os frissítéssel azonos elve (lásd 01).
5. Eltávolítás, frissítés-értesítés, visszagörgetés.

## Backend (mi szolgáltatjuk)
- **Index**: hivatalos + ellenőrzött publikálók listája (keresés, verziók, bizalmi szint, letöltésszám).
- **Build-farm**: ARMv7 (MPC), AArch64, x86_64 Linux, macOS, Windows buildek CI-ban; a hivatalos pluginokat mi írjuk alá.
- **Publikálói regisztráció**: kulcs-felvétel, jelentés/visszavonás (revocation list), takedown folyamat.
- Az index hosting-ja és a kulcsok kezelése biztonsági terv tárgya (nyitott: hol hosztoljuk, ki a kulcs-gazda).

## Biztonság (nem kompromisszum — a letöltött natív kód rootként futhat az MPC-n)
- Aláírás-ellenőrzés telepítés előtt **kötelező**; ismeretlen forrás csak felhasználói jóváhagyással.
- Hosszabb távú cél: a harmadik féltől származó pluginok **külön folyamatban, korlátozott jogosultsággal** futnak (nem root; seccomp/fájlrendszer-korlát; audió/MIDI megosztott memórián át) — egy hibás vagy rosszindulatú plugin ne vigye magával a Kicket/az MPC-t. Ez nagy munka, külön fázis, de a formátum és a host-API ehhez igazodjon már az elejétől.
- Az összeomlás-védelem: a pluginhiba ne vigye le az appot (a mostani összeomlás-naplózás és a megszakítás-kezelés mintájára).
- Licenc-ellenőrzés: a manifest `license` mezője kötelező; az AGPL-kötelezettségek (forrás elérhetősége) a hivatalos pluginoknál teljesülnek.

## Fázisok (sorrendben)
1. **Alap**: CLAP-plugin oldal + `clap.gui` a hostban (jelenleg nincs: lásd `research/codebase-survey.md`), `.kickplug` formátum és manifest-séma, helyi telepítő (fájlból).
2. **Vital-fork**: forrás átvétele AGPL-ként, átnevezés, saját gyári presetek/hullámtáblák, CLAP-célpont, UI-beágyazás; teljesítmény-mérés az MPC-n (ARMv7, 2 izolált mag) — minőségi fokozatok (voice-szám, oversampling, unison), ha kell.
3. **Telepítés linkről** + bizalmi szintek + aláírás-ellenőrzés.
4. **Backend v1**: index + build-farm, hivatalos katalógus (zongora a 02 szerint, szintetizátor).
5. **Nyitott publikálás**: publikálói regisztráció, dokumentáció, SDK/sablon harmadik feleknek.
6. **Sandbox** (külön folyamat, korlátozott jogosultság).

## Nyitott kérdések
- Elég-e a CLAP, vagy VST3 is kell (clap-wrapper; licenc — 02 kutatás)?
- Serum-szint: pontosan mit jelent funkcióban (wavetable-editor, spektrális warp, FX-lánc, mod-mátrix) — a Vital már sokat tud; a különbség/ többlet listája kell.
- MPC-n terhelhetőség (CPU/RAM) — hardveres mérés nélkül nem ígérhetünk.
- Backend hosztolás, kulcsgazda, publikálói szabályzat.
- Az AGPL következménye a harmadik féltől származó, zárt pluginokra (külön folyamat + CLAP-szintű API határ: nem „kombinált mű” — jogi ellenőrzés kell).

## Repo-szerkezet és formátumok (owner döntés, 2026-11-04)
- **Minden plugin külön repóban él** (vagy a Kick-repóba almodulként bekötve, de akkor is önállóan fejleszthetően): a plugin **a Kick forrása és információi nélkül** épül, tesztelhető és kiadható. A Kick csak a kész, aláírt csomagot (`.kickplug` manifest + bináris, lásd 03) fogyasztja — nincs forrás-függőség a plugin → Kick irányban.
- Ebből következik: a plugin repója önálló CMake-projekt, saját CI (Linux x86_64/ARMv7/AArch64, macOS, Windows), saját licenc (AGPL-3.0-or-later ott, ahol Kick- vagy Vital-eredetű kód van; egyébként szabadon), saját release-ek és saját mintakönyvtár-csomag. Közös részek (plugin-sablon, manifest-séma, build-/aláíró szkriptek, clap-wrapper-beállítás) egy külön **plugin-template / SDK repóban** élnek, amit a pluginok sablonként használnak.
- **Formátumok: minden pluginból egyszerre készül a VST3 és a CLAP** ugyanabból a kódból (CLAP-natív + clap-wrapper a VST3/AU-hoz, vagy JUCE + clap-juce-extensions; a választás a próbaépítés eredménye). A CLAP az elsődleges (a Kick csak ezt tölti be), a VST3 a DAW-os felhasználóknak.
- **Próbaépítés (spike) most:** egy minimális, hangot adó teszt-instrumentből (pl. szinusz/ egyszerű szintetizátor) induló önálló repo, amely egyetlen kódból VST3-at ÉS CLAP-ot épít, CI-vel és clap-validator/pluginval ellenőrzéssel. Ebből dől el a GUI-keretrendszer és a wrapper-út, mielőtt a zongorát/ SynthyBoy-t építjük rá. Nyitott: a repó neve és helye (GitHub-szervezet), a Vital-fork repója külön (GPL-3.0→AGPL, saját név).
