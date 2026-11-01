# Helyi modell kísérlet: dokumentum → állítás-lap (WP-A, Key 61 vezérlő-leírás)

**Cél:** megmérni, hogy egy helyi modell megbízhatóan alakítja-e át az útmutató szövegét a [WALKTHROUGH_PLAN.md](WALKTHROUGH_PLAN.md) állítás-lap soraivá. A cél nem a modell képességeinek találgatása, hanem mérés a már kézzel ellenőrzött WP-A anyagon.

## Környezet (mért)
- Gép: Apple M5 Pro, 64 GB memória.
- Modell: `mlx-community/Qwen3.6-35B-A3B-8bit` (35 GB, MoE, kb. 3 milliárd aktív paraméter), `mlx-lm` 0.32.2 (Python 3.13 venv).
- Sebesség (rövid teszt): **64,3 token/s generálás, 37 GB csúcs-memória**. A gondolkodást ki kell kapcsolni (`enable_thinking=False`), különben gondolatmenet-szöveget is ír.

## Kísérlet
Bemenet: a Key 61 vezérlő-leírás (`Top Panel`, 1–45. tétel, 12 572 karakter). Feladat: tételenként JSON-objektum (`item`, `name`, `plain`, `shift`, `locate`, `other`), csak a szövegből. Szkript: [tools/pilot_wpa.py](tools/pilot_wpa.py). A bemeneti szöveg szerzői jog miatt nincs a repóban (kinyerés: lásd [README.md](README.md)).

## Eredmény
- **Futási idő: 62 másodperc** az egész leírásra (13 216 karakter prompt, 11 442 karakter kimenet).
- **45 sor, érvényes JSON, mind a 45 tétel megvan.**
- **Automatikus ellenőrzés a forrásszöveghez:** 132 mező-pár (Shift / Locate / dupla nyomás megléte tételenként), **131 egyezik**; az egyetlen eltérés (7. tétel, Locate) a regex szűk volta: a modell olvasata helyes (a Locate gomb tartása a `<` `>` `<<` `>>` másodlagos funkcióit aktiválja).
- **Kézi ellenőrzés:** 24. és 25. tétel (a szószám-ellenőrzés gyanúsnak jelölte) a forrással egyezik; a 6, 8, 11, 18, 21, 28, 29, 37, 41. tétel helyes.
- **Ismert hiány:** a 37. tételnél (Tap Tempo / Global) kimaradt a LED-szín ("saját tempó = fehér, globális = piros"). A modell nem fordít (angolul idéz), ez az állítás-lapnál nem baj.

## Következtetés (a mérésből)
Strukturált kinyerésre ez a modell ezen a gépen megfelelő: gyors és a mért pontosság magas. **Nincs bizonyíték** arra, hogy ügynökként (tool-hívások, C++ fejlesztés) is megbízható; az nincs mérve. A javasolt használat: szkript, oldaltartományonként, JSON kimenet, automatikus ellenőrzés a forrásszövegre, kézi mintavétel.

## Lemezhely-szabály
Egyszerre egy helyi modell; ami nem kell, azt törölni kell (a lemez véges). A modelleket a `~/.cache/huggingface/hub` tárolja (`du -sh ~/.cache/huggingface/hub/models--*`); törlés előtt a felhasználó engedélye kell, ha nem a feladat hozta létre.
