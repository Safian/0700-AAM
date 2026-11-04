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

## Platform-követelmény (owner, 2026-11-04): a pluginoknak (VST/ instrument és FX) a **telefonos appban** és az **Akai MPC-n (ARMv7)** is menniük kell
- **Célmátrix minden pluginra:** Linux ARMv7 hard-float (Akai MPC Key 61 — kötelező), Linux AArch64, Linux x86_64, macOS (arm64+x86_64), Windows x64, **iOS** és **Android** (a Kick mobilos appja), a VST3/CLAP asztali formátumok mellett.
- **Következmények a kódra:** a DSP-mag platform- és UI-független C++17 könyvtár (statikusan linkelhető, nincs dinamikus külső függőség a magban, nincs futásidejű memóriafoglalás/zár az audioszálon, NEON-barát, a CPU-költség mérve az MPC ARMv7-n és egy közepes telefonon); a GUI külön réteg, a mobilos/ MPC-s felület a Kick saját UI-jára képezhető (a mag paraméter-leírásokat ad, ne csak natív plugin-GUI-t). Egyetlen kódbázis, platformonként CMake-célok; a CI ARMv7-re (QEMU/ keresztfordítás) és iOS/Android build-ellenőrzésre is kiterjed.
- **Fontos terjesztési megkötés (ellenőrizendő, nem hivatalosan igazolt):** iOS-en az App Store-szabályok szerint letöltött **végrehajtható kód** (dinamikusan betöltött `.clap`/`.dylib`) nem engedett; a pluginokat **az appba statikusan beépítve** (vagy iOS-en AUv3-ként) lehet szállítani. Androidon a Play-szabályok ugyanígy korlátozhatják a letöltött natív kódot. Ezért a „linkről telepíthető plugin” (03) **MPC-n és asztalon** teljes értékű; **iOS/Android-on** a hivatalos pluginok (SynthyBoy, KickEQ, zongora) az app részeként készülnek, a harmadik féltől származó, letöltött natív plugin ott csak a platform-szabályok által engedett módon (vagy egyáltalán nem) lehetséges — jogi/ platform-szabály ellenőrzés kell, mielőtt ígérünk bármit. A Kick iOS-buildje most indításkor kilép (`AGENTS.md`, „Known state”), a mobil-út ettől függetlenül nyitott munka.

## DÖNTÉS (owner, 2026-11-04): hivatalos pluginok = az app része; tartalom telepítéskor töltődik le; törölhető és visszaállítható
- **Alapból az app része** a hivatalos plugincsomag (SynthyBoy, KickEQ, zongora, hivatalos FX-ek). A **telepítésnél (első indításkor/ a telepítő lépésében) töltődnek le** a nagy tartalmak (mintakészletek, presetek, hullámtáblák) a hivatalos backendről (03), SHA256-ellenőrzéssel és megszakítható/ folytatható letöltéssel.
- **Törölhető és visszaállítható:** a user a Plugin-kezelőből bármelyik hivatalos plugin tartalmát törölheti (tárhely felszabadítása), és egy gombbal visszaállíthatja (újraletöltés); a törlés nem érinti a projekteket (a projekt csak azonosítót+verziót hivatkozik; hiányzó tartalomnál „Visszaállítás” felugró és csendes fallback, nem összeomlás). A felhasználói presetek/ beállítások külön maradnak.
- **Platformonként:** MPC/asztal — a plugin (`.clap`) és a tartalom is letölthető/törölhető/visszaállítható. **Mobil (iOS/Android) — a plugin KÓDJA mindig az appban van** (statikusan beépítve, lásd a platform-követelményt: letöltött végrehajtható kód nem engedett), tehát a kód nem törölhető, legfeljebb **letiltható/elrejthető**; a **tartalom** (minták, presetek) letölthető, törölhető és visszaállítható. A letöltés adat, nem kód — ez összefér a platformszabályokkal (ellenőrizendő a végleges app-áruházi szabályokkal).
- Megvalósítás-vázlat: közös `ContentPack` fogalom a manifestben (id, verzió, méret, SHA256, licenc/ attribúció), telepítő-sor, tárhely-kijelzés, „telepítve/ hiányzik/ frissíthető” állapot, első indításkor ajánlott csomagok előre kijelölve (a user módosíthatja); a pendrive-os telepítő (01) ugyanezt a verzió-/hash-elvet használja az MPC-n.

## Áruházi egyeztetés (owner, 2026-11-04): a letölthető külön pluginok mobilon
Ha a végleges platformszabály-ellenőrzés szerint mobilon nem engedett a letöltött natív plugin, **felvesszük a kapcsolatot az áruházakkal** (Apple App Review/ Developer Relations, Google Play), hogy megértsük az indokot és engedményt kérjünk: az érv, hogy a zenei pluginok letöltése a platformok saját audio-ökoszisztémájának is javára válna (az Apple AUv3-világának is). Ha ragaszkodnak a korlátozáshoz, a hivatalos pluginok az app részei maradnak (lásd fent), és a külön letöltést **a szabályok által engedett úton** keressük.
- **Teendők:** (1) az Apple App Store Review Guidelines (különösen a letöltött/ értelmezett kód szabálya) és a Google Play szabályzat (letöltött futtatható kód) pontos, elsődleges forrásból való elolvasása és idézése; (2) kapcsolat-felvételi csatornák (Apple: App Review kérdés/ fellebbezés, Developer Forums; Google: Play Console support, policy kérdés); (3) jogi/ szabályzati vélemény; (4) írásos válasz archiválása.
- **Lehetséges technikai út, ha a natív letöltés tiltott marad (NEM ELLENŐRIZVE, kutatás kell):** a letölthető pluginok mobilon **értelmezett/ sandboxolt formában** futnak (pl. WebAssembly-interpreter vagy DSP-leíró nyelv, nem natív gépi kód), amit a szabályok egyes értelmezései megengedhetnek; ehhez a plugin-ABI-t úgy kell tervezni, hogy a DSP-mag ilyen futtatókörnyezetre is leképezhető legyen (a 02/03-ban rögzített platform- és UI-független mag ezt támogatja). Teljesítmény- és szabályzati kockázat: külön spike és előzetes áruházi egyeztetés kell, mielőtt ráépítünk.
