# Tanácsok a fork-feltöltéshez és az upstream-munkához (másik agentnek)

Állapot: 2026-11-04. Ezek tapasztalatok a Key 61 / mobil munkából; amit nem ellenőriztem, azt jelölöm.

## Mi hol van
- `fork/safian/main` (a Key 61 / MPC munka és a tervek) és a `feat/mobile-slideout-ui` közös őse `df3e145b`. A mobil ág `safian/main`-hez képest 3 commitot tartalmaz (`git rev-list --left-right --count fork/safian/main...feat/mobile-slideout-ui` → 345 / 3), ezek között a hardverkinézet eltávolítása (`61425121`, `e1254f35`, 2026-11-04). A helyi mobil ág ennél előrébb jár, mint `fork/feat/mobile-slideout-ui` (`d390bb14`): **a két mobil commit nincs pusholva**.
- A mobil ágon csak a macOS `Kick` target fordulása és a smoke teszt (teljes képernyős Projects képernyő) lett ellenőrizve. **Az iOS-build, az iOS-szimulátor indulás, az iPad/iPhone kinézet és a külső MIDI-vezérlő nem ellenőrzött** (a UIScene javítócsomag a fában van, az indulás nincs újraellenőrizve).

## A push előtt
- `safian/main`-re több agent/munkamenet pusholhat egyszerre (2026-11-04-én a push elutasításba futott, 7 új doksi-commit jött): mindig `git fetch fork`, `git rev-list --left-right --count HEAD...fork/safian/main`, a saját commitot `git rebase fork/safian/main`-nel rakd rá, **force push soha**; push csak `git push fork HEAD:safian/main`, **az `origin` (MusicTribe) soha**.
- Fájlokat névvel `git add`-old (`git add -A` nem), a commit üzenet tartalmazza a kapukat és hogy mi nem lett ellenőrizve.

## Az upstream-triázs (`docs/UPSTREAM_TRIAGE.md`)
- Az agy (Gemini) állította elő; a commit-hasheket és a csoportosítást **én nem ellenőriztem soronként**, mintavétel nélkül ne hivatkozz rá. Az alap `8ea4af11` volt, azóta több mint 20 commit jött (log, aszinkron mappaméret, oktáv-akkord/LED-sor, pendrive-telepítő, mobil, tervek): frissíteni kell.
- A "nem upstreamelhető" listában a **mobil chassis** commitok szerepelnek. A hardverkinézet kivétele (2026-11-04) után a mobil app maradéka (teljes képernyős UI, külső MIDI) upstreamelhető irány lehet, a triázst újra kell osztani. A Proper skin (877 fájl) és a Behringer/KICK nevek és logók továbbra sem mehetnek upstreamre.
- Szabály az `AGENTS.md`-ből: egy PR = egy önálló változás, tiszta `main`-ből ágazva; a Linux/gyári hardver viselkedése nem változhat (`#if JUCE_IOS` / `JUCE_MAC`).

## Kapuk, amiket érdemes minden csomaghoz megismételni
- Tiszta logika: host-teszt a `Kick/tests/platform`-ban (`g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined`).
- Mac: `ninja -C build Kick` és a smoke teszt (`--smoke-test`); a projektváltás összeomlásnál az **ASAN-os Mac-build** (`-fsanitize=address`, `KICK_SMOKE_SWITCH="a.kickPrj,b.kickPrj"`) fogta meg a gyökérokot (hiányzó virtuális destruktor).
- MPC: ARM-build Dockerben (`kick-armv7-builder`, `-j2`), telepítés `deploy_mpc.sh`-val; összeomlásnál a `/data/kick.crash` (pc, lr, veremátfésülés) + `arm-linux-gnueabihf-addr2line`.
- Kézi (ember kell): hang (zongora több tartományban, velocity), LED-ek, fizikai gombok. Ezt egyszer, pontos lépésekkel kérd.
