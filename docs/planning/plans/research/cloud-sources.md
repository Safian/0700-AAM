> **SCOPE-VÁLTOZÁS (owner, 2026-11-13):** az Akai MPC támogatás megszűnt; a Kick csak mobil. Az ebben a fájlban szereplő Akai/MPC/Key 61-es tételek (hardveres próbák, Akai-paritás, MPC-build/deploy, pendrive, hardver-híd) **ARCHIVÁLTAK, NEM FEJLESZTENDŐK**; csak a mobilra/közös motorra vonatkozó tételek érvényesek. Lásd root `AGENTS.md` SCOPE.

# Felhőforrások a Kick fájlböngészőjében: Splice és Google Drive (kutatás, 2026-11-04)

Módszer: csak webes olvasás (WebSearch/WebFetch). Minden állítás mellett forrás-URL.
Jelölés: **[HIVATALOS]** = gyártói doksi/ToS közvetlen olvasása; **[KÖZÖSSÉGI]** = fórum/blog/harmadik fél, nem hivatalos;
**NEM ELLENŐRIZVE** = nem sikerült hivatalos forrásból megerősíteni. Az oldalakat egy kis modell foglalta össze
(WebFetch), ezért a döntő jogi mondatokat (Google ToS 4(b)(1), Splice ToS) a megvalósítás előtt egyszer emberi szemmel
érdemes újraolvasni.

---

## 1. Splice

### 1.1 Hivatalos nyilvános API/SDK/partneri program?
- **Nincs nyilvános fejlesztői API vagy SDK.** A Splice ToS "nem tárgyalja az API-kat", fejlesztői/SDK-feltételek nincsenek benne
  (https://www.splice.com/terms, "Last Updated: July 24, 2026"). Egyik Splice-oldalon (MCP-oldal, súgócikkek) sincs API-kulcs,
  rate limit vagy dev-doksi (https://splice.com/tools/mcp-server).
- **Nyilvános partneri program oldalt nem találtam** (keresés: "Splice partner program ..."). Ettől még vannak egyedi
  hardveres/szoftveres partnerségek (lent). Hivatalos partneri kapcsolatfelvétel: NEM ELLENŐRIZVE.
- **Új (2026): Splice MCP szerver (béta).** Blogposzt 2026-04-28: https://splice.com/blog/splice-sounds-in-claude-mcp/ ;
  súgó 2026-04-06: https://support.splice.com/en/articles/14442749-getting-started-with-the-splice-mcp-server-beta .
  URL: `https://mcp.splice.com/mcp`, streamable-HTTP + OAuth; "bármely, HTTP-alapú MCP-t támogató AI-eszköz csatlakozhat".
  Ez AI-asszisztensekhez (Claude, Cursor, Windsurf, VS Code) készült, nem általános fejlesztői API.
  Képességek: természetes nyelvű keresés (max 50 találat), stack-építés, megosztási link, letöltés kredittel (újra-letöltés ingyen),
  **100 letöltés / gördülő 24 óra** (súgócikk). Eszközök (harmadik fél leírása, nem Splice-oldal):
  describe_a_sound, create_stack, prompt_to_stack, share_stack, download_asset, update_stack
  (https://docs.scalekit.com/agentkit/connectors/splicemcp/ ; https://agentman.ai/agentskills/connections/mcp-server/splice) [KÖZÖSSÉGI].
  Nincs mappa-/könyvtárlistázó, kötegelt szinkron vagy metaadat-export eszköz; a "download_asset" egy tetszőleges útvonalra tölt (`download_path`) [KÖZÖSSÉGI].
  **Nem ismert, hogy a nem-AI kliens (pl. egy sampler) használata megengedett-e**: a Splice ezt sehol nem mondja ki és nem tiltja. NEM ELLENŐRIZVE.

### 1.2 Asztali app, helyi mappa, formátum, metaadat
- **Helyi mappa** [HIVATALOS]: macOS `/Users/<felhasználó>/Splice`; Windows a súgó szerint "C:\Documents\Splice" (szó szerint így írja;
  a keresési kivonat "Documents mappa"-t mond, tehát a valóságban valószínűleg `Dokumentumok\Splice`), a csomagok szerinti almappákkal
  (https://support.splice.com/en/articles/8652631-where-do-my-downloaded-samples-presets-midi-files-go).
  A Sounds **plugin** (béta) külön fix útvonalra is tölthet: macOS `Library/Splice/Plug-in/samples`, Windows `AppData\Roaming\Splice\Plug-in\samples`,
  nem módosítható (https://support.splice.com/en/articles/13007631-how-to-download-samples-in-the-splice-sounds-plugin-now-in-beta, keresési kivonat).
  A weboldalról letöltött minták a böngésző Letöltések mappájába kerülnek (ua. a fenti cikk).
- **Linux: nem támogatott** [HIVATALOS]: "Linux is not supported"; macOS 12+, Windows 10+
  (https://support.splice.com/en/articles/8652653-computer-and-operating-system-requirements-to-install-splice).
  Tehát a Kick Linux-desktop és MPC-ágán nincs Splice-app, ami a mappát feltöltené.
- **Formátum**: a Splice-nek leadott tartalom WAV, 24 bit, min. 44,1 kHz, -1 dB peak (https://creators.splice.com/audio, kivonat) [HIVATALOS, de a *letöltött* fájl formátumát
  a cikk nem rögzíti → NEM ELLENŐRIZVE].
- **Metaadat (BPM, kulcs, címkék)**: a Splice a saját library-adatbázisában tartja; fájlba ágyazott/fájlnévi BPM-kulcs-konvencióról
  nincs hivatalos állítás. Egy harmadik fél (Eagle "Splice Sample Sync") plugin a Splice adatbázisából olvassa a BPM/kulcs/tag/csomag-adatot
  (https://community-en.eagle.cool/plugin/d2996567-4a7c-4d8c-beaa-257187670fef) [KÖZÖSSÉGI]. Az adatbázis formátuma/útvonala nem dokumentált;
  **ennek olvasása nem hivatalos felület, nem szabad rá építeni** (ToS: kerülőút/reverse engineering tilos, ld. 1.3).
  A WAV-ba ágyazott metaadat (pl. smpl/acid/bext chunk) megléte: NEM ELLENŐRIZVE; a Kick saját BPM/kulcs-elemzése robusztusabb.

### 1.3 ToS / licenc (https://www.splice.com/terms, Last Updated 2026-07-24; Sounds Licensing FAQ: https://support.splice.com/en/articles/8652642-splice-sounds-licensing-faq.md)
- **Licenc**: nem-kizárólagos, perpetual, "licensed not sold"; felhasználás új felvételekben (New Recordings) és Creative Works-ben
  (játék, film/TV, social, live), kereskedelmi és nem kereskedelmi célra (ToS §3.1.1).
- **Tilos** (§3.1.1.3): a hangok önálló (izolált) mintaként/loopként való felhasználása/sublicencelése, versengő használat, új sample-packbe
  újraelosztás, újrafelvétel, "preview" hangok preview-n túli használata, **AI-/ML-tréning vagy forrásanyag** (§3.1.1.3(h)), előadó nevének/hasonmásának használata.
  Következmény a Kicknek: **a minták NEM kerülhetnek a Kick repóba/telepítőbe/presetcsomagba és nem terjeszthetők** (a Kick demó-/factory-tartalma nem lehet Splice-hang).
- **Automatizált hozzáférés** (§II.8(j)): botok, spiderek, scraperek, crawlerek, adatbányász eszközök nem scrape-elhetik/tölthetik le a weboldalakat;
  biztonsági funkciók kijátszása és reverse engineering tilos (§II.8(a)); hálózat/szerver zavarása tilos (§II.8(b)(iv)). Az API-kat a ToS nem említi.
- **Offline használat**: a ToS nem tárgyalja közvetlenül; a letöltött hangok újra-letölthetők extra kredit nélkül, amíg van fiók (§II.3).
  A Splice Instrument presetek csak a Splice szolgáltatáson keresztül használhatók (§5.3).
- **Előfizetés megszűnése** [HIVATALOS]: a letöltött hangokra a licenc örökre megmarad ("perpetual", a Licensing FAQ), új letöltéshez aktív előfizetés kell, a megmaradt kreditek 28 nappal a
  végső számlázási periódus után lejárnak; a könyvtár a desktop appban továbbra is böngészhető/újra-letölthető
  (https://support.splice.com/en/articles/8652638-if-i-cancel-my-subscription-do-i-lose-my-samples-and-credits.md).
  A ToS ezzel együtt a Create Tool felhasználói audióját törli 30 napon belül (§3.3.3) – a Kicknek ez nem releváns.
- **Hivatalos felületeken kívüli hozzáférés**: hivatalos az asztali app, a weboldal, a Sounds plugin (béta, VST3/AU), a DAW-integrációk és az MCP. Minden egyéb (belső HTTP API reverse-engineering, adatbázis-olvasás, scraping) a §II.8 alapján tiltott → **nem szabad**.

### 1.4 Ismert integrációk – hogyan csinálták
- **Akai MPC/Force (standalone)** – a leglényegesebb precedens: 2019 óta beépített Splice böngésző Wi-Fi-n, számítógép nélkül
  (https://www.akaipro.com/newsflash-splice-integration ; https://splice.com/partners/akai). Bejelentkezés: a súgó "code a splice.com/link-en" folyamatot ír
  (https://support.splice.com/en/articles/8652651-how-to-use-splice-sounds-with-akai-professional-s-standalone-series), a hibaelhárítás email+jelszó belépést említ
  (https://support.splice.com/en/articles/12466657-troubleshooting-your-splice-x-mpc-connection) – a kettő nem egyezik, ezért **a pontos hitelesítési mód NEM ELLENŐRIZVE**.
  A firmware a Splice tartalmát "Content > Splice" meghajtóként mutatja, almappákkal (By BPM / By Instrument / By Key / By Tag), a Key 61 érintett
  (https://support.akaipro.com/en/support/solutions/articles/69000824943-mpc-v2-11-5-release-notes, kivonat). Ez **partneri (zárt) integráció**, a háttér-API nem publikus.
  Következmény: ha a Kick leváltja az Akai appot az MPC Key 61-en, a gyári Splice-integráció elvész, és ugyanazt csak Splice-partnerséggel (levélben) lehetne újracsinálni.
- **Pioneer DJ Toraiz SP-16 / DJS-1000**: partnerségben épült felhőkapcsolt sampler (https://rekkerd.org/pioneer-dj-partners-with-splice-for-first-cloud-connected-sampler/) [KÖZÖSSÉGI].
- **Ableton Live 12.3**: Splice-böngésző a DAW-ban, a Splice "exkluzív in-DAW integrációjaként" (https://support.splice.com/en/articles/12062860-faq-splice-integration-with-ableton-live-12-3). Studio One, Pro Tools: hasonló, Splice által készített
  (https://splice.com/blog/splice-integration-studio-one/). Nincs harmadik félnek nyitott modell.
- **Splice Sounds plugin (VST3/AU, béta)**: "bármely DAW"-ba hozza a könyvtárat (https://splice.com/blog/splice-sounds-plugin). Linux-ARM sampleren nem futtatható.
- **Serato/Native Instruments Splice-integráció**: nem találtam. **iOS-es Splice app**: nem találtam. NEM ELLENŐRIZVE.

### 1.5 Következtetés (Splice)
1. **Engedett és észszerű**: a Kick *semmit nem tud a Spliceról*; a felhasználó a saját Splice-mappáját (desktop: `~/Splice`) hozzáadja kedvenc/könyvtár helyként, és a megszokott
   helyi forrásként (`LocalFolderSource`) böngészi, indexeli. Ez a sima fájlolvasás; a licenc szerinti használat (zenei produkció) engedett. Opció: mappa-megfigyelés
   (`juce::FileSystemWatcher`) az új letöltések automatikus beolvasásához. Splice-specifikus kód nincs, jogi kockázat minimális.
2. **MPC-n (Linux, nincs Splice app)**: a minták a felhasználó gépéről kerülnek át (USB/SFTP/WebDAV/saját szinkron). Ez *a felhasználó saját licencelt példányának* mozgatása a saját eszközei közt;
   a Kick nem terjeszti. Nincs hivatalos módszer a Splice-ból közvetlenül az MPC-re (a ToS nem tiltja a saját fájlok másolását, de erről hivatalos állásfoglalás nincs → NEM ELLENŐRIZVE).
3. **Hivatalos API hiányában nincs mit építeni**; az MCP szerver AI-asszisztensekre szól. Egy "Kick mint MCP-kliens" megoldás szürke zóna (nem AI-eszköz; béta; 100/nap korlát; nincs fejlesztői ToS) → csak a Splice írásos engedélyével.
4. **NEM szabad**: scraping, belső/nem dokumentált Splice HTTP API használata, a Splice adatbázis/gyorsítótár olvasása és sync, a minták Kick-csomagba tétele vagy elosztása, minták AI-tréningre használata
   (a Kick sem használhatja őket tanító adatként, pl. beépített hangtípus-felismerőhöz), a hitelesítési adatok tárolása/proxizása.
5. **Opcionális stratégiai lépés**: partneri megkeresés a Splice-nál (mint Akai/Pioneer), ha valódi MPC-s Splice-böngészőt akarunk. Ennek eredménye nem előre jelezhető; nincs nyilvános folyamat.

---

## 2. Google Drive

### 2.1 Eszköz-folyam (OAuth 2.0 for TV and limited-input devices)
Forrás: https://developers.google.com/identity/protocols/oauth2/limited-input-device [HIVATALOS]
- **Engedett hatókörök (kizárólag)**: `email`, `openid`, `profile`, `drive.appdata`, `drive.file`, `youtube`, `youtube.readonly`.
  **`drive.readonly`, `drive`, `drive.metadata.readonly` NEM engedett** az eszköz-folyamban. Ez a döntő korlát.
- Végpontok: `https://oauth2.googleapis.com/device/code` (POST: client_id, scope), polling `https://oauth2.googleapis.com/token` (`grant_type=urn:ietf:params:oauth:grant-type:device_code`),
  `interval` szerinti várakozás; `authorization_pending` (428), `slow_down` (403), `access_denied`, `admin_policy_enforced` stb. A device_code érvényessége az `expires_in` (példa: 1800 s).
- A polling kérés **client_secretet is kér**; a doksi szerint az eszközre terjesztett app nem tud titkot tartani; frissítésnél a secret opcionális. Refresh token mindig jár, amíg a felhasználó vissza nem vonja.
- A device-code kérések kvótája kliensenként korlátozott (403 `rate_limit_exceeded`), számot nem közölnek.
- Az "Allowed scopes for installed apps" azt jelenti, hogy az MPC-n a Drive-hozzáférés csak: (a) az app által létrehozott fájlok (`drive.file`), (b) az app privát adatmappája (`drive.appdata`).

### 2.2 `drive.file` valódi hatóköre – a fő akadály
- Hivatalos: a `drive.file` "non-sensitive", és csak azokhoz a fájlokhoz ad hozzáférést, amelyeket az app megnyitott vagy a felhasználó **a Google Pickerrel megosztott** az appal
  (https://developers.google.com/workspace/drive/api/guides/api-specific-auth). `drive.readonly`, `drive.metadata.readonly`, `drive` = **restricted**.
- A felhasználó *meglévő* Drive-mappáit (a hangmintái) ezért eszköz-folyammal `drive.file`-lal **nem lehet böngészni**. rclone doksija ugyanezt mondja: a `drive.file` "csak az rclone által létrehozott
  fájlokat látja; a webes felületen hozzáadottak rejtve maradnak" (https://rclone.org/drive/).
- **Picker**: webes (Picker API) vagy "desktop/mobil" változat (`trigger_onepick=true`, csak `drive.file`, nem kombinálható); a `picked_file_ids` az átirányítási URI-ra jön vissza
  (https://developers.google.com/workspace/drive/picker/guides/desktop-mobile-picker). Eszközről (billentyűzet nélkül, böngésző nélkül) nem használható; a Picker szükségességét az eszköz-folyam doksi nem említi.
- **Kiválasztott mappa gyerekei**: a hivatalos doksi erről nem szól. Közösségi jelentések szerint a kiválasztott mappa tartalmát `drive.file` nem olvashatja (csak a mappát, a benne létrehozást)
  (https://groups.google.com/g/google-apps-script-community/c/_W-NKbttfbo , https://kiwix.lema.org/stackoverflow.com_en_all_2023-11/questions/68462168/google-drive-api-permission-to-children-files) [KÖZÖSSÉGI, 2019-2024]. **Hivatalosan NEM ELLENŐRIZVE** (és magam sem próbáltam ki).
- Hogy egy Picker-ben (web/desktop kliens) adott `drive.file` jogosultság használható-e *másik* OAuth-klienssel (device kliens) kapott tokennel: a doksi nem mondja. NEM ELLENŐRIZVE.

### 2.3 Verifikáció és költség
- Non-sensitive hatókörökhöz (`drive.file`, `drive.appdata`) **nincs kötelező app-verifikáció**; a márkaverifikáció (név/logó a consent képernyőn) opcionális, "lighter-weight"
  (https://support.google.com/cloud/answer/13463073).
- Sensitive/restricted: kötelező verifikáció (demóvideó, indoklás, domain-ellenőrzés, adatvédelmi szabályzat), restricted esetén **éves biztonsági felülvizsgálat** Google által kijelölt harmadik féltől, LOV-val
  (https://support.google.com/cloud/answer/13465431 ; https://support.google.com/cloud/answer/13461325). Költség/átfutási idő **hivatalosan nem szerepel**. Harmadik fél árak (2024-es összehasonlítás):
  ~540 USD/alkalmazás (TAC Tier 2 alap) , 3000-4500 USD (Leviathan) (https://www.switchlabs.dev/post/casa-tier-2-tier-3-security-review-providers-pricing-and-the-cheapest-option ; https://leviathansecurity.com/programs/google-casa-cloud-application-security-assessment) [KÖZÖSSÉGI/ÜZLETI, elavulhat]. Átfutási idő: NEM ELLENŐRIZVE.
- Ellenőrizetlen (unverified) app sensitive/restricted hatókörrel: **100 új felhasználó összesen** a figyelmeztető képernyő után (https://support.google.com/cloud/answer/7454865).
- "Testing" kiadási állapot, külső felhasználó: a refresh token **7 napig** él (https://developers.google.com/identity/protocols/oauth2#expiration); ez a hivatalos doksiban is szerepel. Éles állapotban: 6 hónap használat nélkül, 100 refresh token / fiók / kliens.
- Google API Services User Data Policy: a Limited Use szabályok a sensitive/restricted adatokra; kötelező adatvédelmi szabályzat; éves biztonsági felülvizsgálat restricted-nél; **a szabályzat nem szól nyílt forrású appokról**
  (https://developers.google.com/terms/api-services-user-data-policy).

### 2.4 Nyílt forrású dilemma (AGPL) – client secret / client ID
- **Ellentmondás a Google saját doksijaiban**: a native-app doksi szerint "Desktop apps are public clients and cannot keep secrets confidential", PKCE kötelező
  (https://developers.google.com/identity/protocols/oauth2/native-app). A device-folyam doksi szerint a terjesztett app nem tud titkot tartani (lásd 2.1).
  **Ugyanakkor a Google API ToS 4(b)(1)**: "You will keep your credentials confidential", és: "Developer credentials may not be embedded in open source projects."
  (https://developers.google.com/terms , utolsó módosítás: 2021-11-09; a "developer credentials" definíciója: kulcsok, jelszavak, kliens ID-k.)
  → Egy AGPL-repóba **commitolt közös client ID/secret ütközhet a ToS-szel**; a ToS 2(e) szerint a nyílt licenc csak annyiban írja felül, amennyire kifejezetten ráutal. Jogi értelmezés: NEM ELLENŐRIZVE (ügyvéd).
- A gyakorlat: az **rclone** a megosztott client ID megszüntetését jelezte ("is being retired and will stop working during 2026"), és a saját client ID-t kéri (https://rclone.org/drive/#making-your-own-client-id).
  Saját projektben a személyes használat <100 felhasználónál verifikáció nélkül megy, "unverified app" figyelmeztetéssel; éles ("published") állapot kell a 7 napos token-lejárat ellen.
- Közös limit: a megosztott client ID-ra kliensenként ~10 tranzakció/mp áll (rclone doksi) – a közös kliens felhasználószám mellett gyorsan fojtott lenne.

### 2.5 Kvóták, letöltés, listázás, változáskövetés
- Kvóta [HIVATALOS] (https://developers.google.com/workspace/drive/api/guides/limits): 1 000 000 egység/perc/projekt, 325 000/perc/felhasználó/projekt; list=100 egység, get=5, letöltés (`alt=media`)=200 egység;
  napi 1 TB egress a számlázás előtt; 403 "User rate limit exceeded"/429 → exponenciális visszalépés (min(2^n + random_ms, max_backoff), tipikusan 32-64 s). Nincs külön letöltési kvóta.
- Letöltés: `files.get?alt=media`, **`Range` fejléc támogatott** (nem Workspace-exporthoz); Workspace-dokumentum export max 10 MB; "abusive" fájlhoz `acknowledgeAbuse=true`
  (https://developers.google.com/workspace/drive/api/guides/manage-downloads). Újrakezdés = saját Range-logika (a doksi nem ír le "resume" mechanikát).
- Mappa-listázás: `files.list` `q='<FOLDER_ID>' in parents and trashed=false`, lapozás `pageToken`-nel; `mimeType` szűrés (https://developers.google.com/workspace/drive/api/guides/ref-search-terms).
- Változáskövetés: `changes.getStartPageToken` + `changes.list` (polling); `changes.watch` HTTPS webhookot és érvényes TLS-tanúsítványt igényel, csatorna lejárata max. 1 hét (changes)
  (https://developers.google.com/workspace/drive/api/guides/manage-changes ; https://developers.google.com/workspace/drive/api/guides/push) → **az MPC-n/desktopon nem használható (nincs publikus HTTPS végpont), polling kell**.

### 2.6 Hozzáférési utak összevetése (Drive)
| # | Út | Hitelesítés | Verifikáció | Meglévő mappák? | Eszközről (nincs billentyűzet) | Megjegyzés |
|---|----|-------------|-------------|-----------------|--------------------------------|------------|
| A | **Nyilvános "bárki a linkkel" mappa + API-kulcs** | nincs OAuth, csak API-kulcs | nincs | igen, ha a tulaj megosztja | tökéletes (csak mappa-ID/link kell) | Hivatalos: "List files in a public folder", API-kulccsal (https://developers.google.com/workspace/drive/api/guides/search-files). Linkkel megosztott elemekhez `resourceKey` kellhet (`X-Goog-Drive-Resource-Keys`, https://developers.google.com/workspace/drive/api/guides/resource-keys). A közös API-kulcs nyílt forrású repóba nem tehető (ToS 4(b)(1) fenti) → **kulcs felhasználónként (BYO)**, konfigfájlból. Nyilvános tartalom = nincs adatvédelem. |
| B | **Eszköz-folyam + `drive.file`/`drive.appdata`** | device flow | nem kötelező | **nem** (csak app által létrehozott) | igen | Csak a Kick saját exportjainak/projektjeinek mentésére/szinkronjára jó, nem a meglévő minták böngészésére. |
| C | **Eszköz-folyam + Picker párosítás telefonról** | Picker oldal telefonon, token az eszközre | nem kötelező | csak fájlonként kiválasztva | részben | Saját relé-szolgáltatás kellene; mappa-tartalom-hozzáférés bizonytalan (2.2). Nem javasolt MVP-nek. |
| D | **drive.readonly saját (BYO) kliens ID-val, a tokent máshol szerzik** (rclone `authorize`-szerű) | desktop loopback+PKCE egy gépen, token átvitel | nem kötelező <100 usernél (unverified) | igen | token importálása fájlból | Személyes használatra. Nem eszköz-folyam (az nem engedi a hatókört). A felhasználó saját GCP-projektje. Token 7 nap lejár, ha "Testing" marad. |
| E | **Hivatalos, verifikált Kick-kliens `drive.readonly`-val** | web/desktop OAuth | restricted: verifikáció + éves külső felülvizsgálat | igen | nem eszköz-folyam | Pénz + folyamatos adminisztráció + ToS-ütközés az OSS-szel. **Nem javasolt.** |
| F | **A felhasználó saját relé-szolgáltatása**: `rclone serve webdav` a PC-n/NAS-on, a Kick WebDAV-forrása csatlakozik | rclone kezeli a Google-t; Kick: user/pass | nincs a Kick oldalán | igen | igen (URL+jelszó fájlban/érintő-billentyűzet) | Legtisztább: a Google-jogi teher a felhasználónál (saját rclone + saját client ID). `rclone serve webdav`: --user/--pass/--htpasswd, TLS --cert/--key, --read-only (https://rclone.org/commands/rclone_serve_webdav/). Range támogatás: a doksi nem említi → NEM ELLENŐRIZVE. A rclone ARMv7 Linux build megléte: NEM ELLENŐRIZVE (az install oldal csak amd64-et mutat). |
| G | **Drive for desktop szinkron-mappa** (desktopon) | Google kliens | nincs | igen | n/a (MPC-n nem lehet) | Helyi mappaként böngészhető. A Drive for desktop Linuxos hiánya/jelenléte: NEM ELLENŐRIZVE. |
| H | "uc?export=download" böngészős link-letöltés | nincs | nincs | — | — | Nem dokumentált, törékeny (vírusellenőrző közbeiktatás); **nem javasolt**. NEM ELLENŐRIZVE. |

**Következtetés (Drive)**: az "MPC-n billentyűzet nélkül, a felhasználó saját privát Drive mappáit böngészni" a hivatalos, gyártói útvonalakon **nem megoldható akadálymentesen**: az eszköz-folyam csak `drive.file`/`drive.appdata`-t enged, ami meglévő fájlokat nem lát;
a teljes olvasás `drive.readonly` restricted, éves külső biztonsági felülvizsgálattal. Észszerű, megvalósítható megoldás: **A (nyilvános mappa + BYO API-kulcs)** az MVP-ben, **F (rclone WebDAV)** a teljes privát Drive-hoz, **D** haladó desktop felhasználóknak.

### 2.7 Egyéb felhők ugyanazon modellre
- **OneDrive (Microsoft Graph)**: **van eszköz-folyam** (RFC 8628), személyes és munkahelyi fiókra (`/consumers`, `/common`), publikus kliens (nincs titok), `offline_access` refresh token; 15 perc a felhasználói kódra;
  letöltés `GET /me/drive/items/{id}/content` → 302 egy előre hitelesített URL-re, `Range` a letöltési URL-en; delegált `Files.Read`
  (https://learn.microsoft.com/en-us/entra/identity-platform/v2-oauth2-device-code ; https://learn.microsoft.com/en-us/graph/api/driveitem-get-content). A legjobban illeszkedő felhő az eszköz-modellhez. Delta/változáslekérés: a doksit nem néztem meg → NEM ELLENŐRIZVE.
- **Dropbox**: nincs eszköz-folyam a doksiban; **PKCE + "no redirect"** (a kód a Dropbox oldalán jelenik meg, a felhasználónak át kell másolnia az appba → billentyűzet/érintő-billentyűzet kell), `token_access_type=offline`, rövid életű tokenek
  (https://docs.dropboxapi.com/dropbox-api/docs/oauth). Lista/letöltés/delta API-k: nem néztem meg → NEM ELLENŐRIZVE.
- **S3 / S3-kompatibilis**: `ListObjectsV2` (max 1000 kulcs/oldal, `continuation-token`, `prefix`+`delimiter` a mappa-emulációhoz, ETag/Size/LastModified)
  (https://docs.aws.amazon.com/AmazonS3/latest/API/API_ListObjectsV2.html); hitelesítés: hozzáférési kulcs + SigV4 vagy előre aláírt URL, a felhasználó fájlból adja meg (nincs OAuth). Range GET: a doksit nem néztem meg → NEM ELLENŐRIZVE.
- **WebDAV / HTTP index**: általános (PROPFIND, GET+Range), lefedi a Nextcloudot, NAS-okat, rclone-t, saját szervert. RFC 4918 / Range támogatás szerverenként eltér: NEM ELLENŐRIZVE (nem néztem utána).

---

## 3. Javasolt architektúra

### 3.1 `FileBrowserSource` interfész (forrás-független)
```
Capabilities { List, Search, RangeRead, Changes, Write, Auth }   // forrásonként deklarált
struct Entry  { id, name, isDir, size, mtime, etag/revision, checksum?, mime?, optional<meta{bpm,key,tags}> }
struct Page   { vector<Entry> items; optional<string> nextToken; }

AuthState state();                                     // SignedOut / Pairing / SignedIn / NeedsReauth / Offline
optional<AuthChallenge> beginAuth();                   // device flow: userCode, verificationUrl, QR, expiresIn
Result<Page>  list(parentId, pageToken, CancelToken);
Result<Entry> stat(id, CancelToken);
Result<File>  fetch(id, destPath, optional<Range>, ProgressCb, CancelToken);   // .part + Range-folytatás + ellenőrzés + atomi átnevezés
optional<ChangeSet> changes(token, CancelToken);       // csak ahol van (Drive changes, Graph delta); különben TTL-es újralistázás
void signOut();                                        // token törlése + (Google) revoke
```
Közös réteg: `SourceRegistry`, `ListingCache` (TTL + etag), `ContentCache` (kulcs: forrás+id+revision/etag; LRU, felhasználói méretkorlát, pinelés "offline-ra"),
`DownloadQueue` (prioritás, szüneteltetés, korlátozott párhuzamosság: MPC-n 1-2).
Forrás-implementációk: `LocalFolderSource`, `WebDavSource`, `GoogleDrivePublicSource` (API-kulcs), `OneDriveSource`, `S3Source`, `DriveOAuthSource` (később).

### 3.2 Szálmodell
- A hálózati kód **külön szálkészletben** (1-2 szál MPC-n), alacsonyabb prioritással (nice), **soha nem az audió szálon és nem a UI szálon**. Az audió szál a hálózathoz nem nyúl, nincs zár/foglalás közös út.
- Az eredmény `MessageManager::callAsync`-kel a UI-ra; a mintabetöltés **csak teljesen letöltött, ellenőrzött fájlból** indul (nincs hálózati streaming az audió útvonalon).
- Lemezterhelés fékezése letöltés alatt (chunkonkénti írás, fsync csak a végén), hogy ne okozzon xrun-t az MPC lassú tárolóján.
- HTTP-kliens: libcurl (TLS + CA-köteg, redirect, Range, HTTP/2) – a `juce::URL` nem elég a megszakítható, folytatható letöltéshez. A választás javaslat, nem kutatási eredmény; ARMv7 keresztfordítás ellenőrzendő.

### 3.3 Token-tárolás
- Csak **refresh token**, minimális hatókör, hozzáférési token csak memóriában. Desktop: OS kulcstár (macOS Keychain, Windows Credential Manager, libsecret). MPC: nincs kulcstár → `0600` jogú fájl a felhasználói adat-partíción – titkosítatlan nyugalmi állapotban; ezt a felhasználónak kimondjuk. Kijelentkezéskor törlés + Google `https://oauth2.googleapis.com/revoke`.
- A kliens ID/kulcs **konfigurációból** (felhasználói fájl), nem a repóból (ToS 4(b)(1)).

### 3.4 Offline
- Hálózathiány = nem hiba: a böngésző cache-ből dolgozik, a nem cache-elt elemek halványak, "pinelhető offline"; a háttérben exponenciális visszalépés + jitter (Drive-doksi szerinti formula), 403/429 és `slow_down` kezelése, a lejárt/visszavont token → "NeedsReauth", a UI nem blokkol.

### 3.5 MVP és sorrend
1. **M0**: interfész + `LocalFolderSource` + gyorsítótár + szálmodell váz. Itt "megoldódik" a Splice is desktopon (`~/Splice` mappa hozzáadása; nincs Splice-kód). Hivatalos jogi lépés nincs.
2. **M1**: `WebDavSource` (user/pass, TLS, PROPFIND, Range GET). Lefed: NAS, Nextcloud, **rclone→Google Drive/OneDrive/Dropbox bármi** a felhasználó saját gépéről. Ez a teljes Drive-hozzáférés gyakorlati útja az MPC-n.
3. **M2**: `GoogleDrivePublicSource` (nyilvános mappa + BYO API-kulcs + `resourceKey`), nincs OAuth, nincs verifikáció.
4. **M3**: általános RFC 8628 eszköz-folyam + `OneDriveSource` (a legjobb illeszkedés); opcionálisan `drive.file`/`drive.appdata` a Kick saját exportjaihoz (csak ha írás/mentés-igény van).
5. **M4**: S3-forrás; Dropbox (PKCE + kézi kód) csak ha van igény; haladó: Drive `drive.readonly` BYO-klienssel (D út).
6. **Nem MVP**: hivatalos verifikált Google-kliens; Splice-integráció (csak partnerséggel).

### 3.6 Fennmaradó jogi/licenc kockázatok
1. **Google ToS 4(b)(1) vs AGPL-repó**: közös client ID/kulcs repóban tartása tiltott lehet → BYO-kulcs. Értelmezés: NEM ELLENŐRIZVE (jogász).
2. **Google API Services User Data Policy**: ha valaha restricted hatókör kerül a Kickbe → Limited Use, adatvédelmi szabályzat, éves külső felülvizsgálat. A javasolt MVP (A, F, B) ezt elkerüli. Ha a projekt adatot sosem küld Kick-szerverre, a "nincs saját szerver" állítás segíti a felülvizsgálatot – hivatalos mentesség nincs dokumentálva.
3. **Splice ToS**: scraping/reverse engineering tilos (§II.8); AI-tréning tilos (§3.1.1.3(h)); minta nem terjeszthető → nem lehet a Kick repóban/demóban/tesztadatban; a Kick logóval/névvel ne sugalljon Splice-partnerséget (hivatalos védjegyszabály: NEM ELLENŐRIZVE).
4. **AGPL-3.0 hálózati záradék** és külső szolgáltatások: a Kick forrása elérhető kell legyen a hálózaton használóknak is (csak ha módosított verziót hálózaton szolgáltat; egy lokális hangszerre ritkán érvényes) – licenc-értelmezés NEM ELLENŐRIZVE; harmadik fél könyvtárak (libcurl, TLS) licenckompatibilitása ellenőrizendő.
5. **Felhasználói adatvédelem**: nyilvános Drive-mappák tartalma bárkié; a BYO API-kulcs a felhasználó felelőssége (kvóta, visszaélés) – a Kick ne mentse fel naplóba.
6. **Hamis biztonságérzet**: az MPC-n a token fájlban van (nincs kulcstár) → fizikai hozzáférés = hozzáférés a felhőfiókhoz; ezért alapértelmezetten read-only hatókör.

## Források (összesítve)
Splice: https://www.splice.com/terms · https://support.splice.com/en/articles/8652642-splice-sounds-licensing-faq.md · https://support.splice.com/en/articles/8652638-if-i-cancel-my-subscription-do-i-lose-my-samples-and-credits.md · https://support.splice.com/en/articles/8652631-where-do-my-downloaded-samples-presets-midi-files-go · https://support.splice.com/en/articles/8652653-computer-and-operating-system-requirements-to-install-splice · https://support.splice.com/en/articles/14442749-getting-started-with-the-splice-mcp-server-beta · https://splice.com/tools/mcp-server · https://splice.com/blog/splice-sounds-in-claude-mcp/ · https://support.splice.com/en/articles/8652651-how-to-use-splice-sounds-with-akai-professional-s-standalone-series · https://splice.com/partners/akai · https://support.splice.com/en/articles/12062860-faq-splice-integration-with-ableton-live-12-3
Google: https://developers.google.com/identity/protocols/oauth2/limited-input-device · https://developers.google.com/identity/protocols/oauth2/native-app · https://developers.google.com/identity/protocols/oauth2#expiration · https://developers.google.com/workspace/drive/api/guides/api-specific-auth · https://developers.google.com/workspace/drive/picker/guides/desktop-mobile-picker · https://developers.google.com/workspace/drive/api/guides/limits · https://developers.google.com/workspace/drive/api/guides/manage-downloads · https://developers.google.com/workspace/drive/api/guides/manage-changes · https://developers.google.com/workspace/drive/api/guides/push · https://developers.google.com/workspace/drive/api/guides/search-files · https://developers.google.com/workspace/drive/api/guides/resource-keys · https://developers.google.com/terms · https://developers.google.com/terms/api-services-user-data-policy · https://support.google.com/cloud/answer/13463073 · https://support.google.com/cloud/answer/13461325 · https://support.google.com/cloud/answer/13465431 · https://support.google.com/cloud/answer/7454865 · https://rclone.org/drive/ · https://rclone.org/commands/rclone_serve_webdav/
Egyéb: https://learn.microsoft.com/en-us/entra/identity-platform/v2-oauth2-device-code · https://learn.microsoft.com/en-us/graph/api/driveitem-get-content · https://docs.dropboxapi.com/dropbox-api/docs/oauth · https://docs.aws.amazon.com/AmazonS3/latest/API/API_ListObjectsV2.html
