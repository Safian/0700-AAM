# MPC Key 61: gomb-LED protokoll és színek (mért, 2026-11-01)

Mérés: a Kick app az MPC-n (`safian/main` `b8ee63a6`, ARMv7), a LED-ek a Mac kamerájával (Photo Booth) figyelve, sötétített szobában. A gombokat a `/data/kick_cmd` FIFO `btn <NÉV>` parancsaival kapcsoltam. A kamera felbontása a halvány LED-ek finom színeihez kevés: ahol nem volt egyértelmű, "nem eldönthető"-t írok.

## 1. Protokoll (a döntő lelet)
A gomb-LED-eket a Port 1-re (controls, `hw:0,0,1`) írt **Control Change** vezérli: `0xB0 <gomb-note> <érték>` (a gomb note-száma a CC-szám). A `0x90 <note> <érték>` / `0x80 <note> 0` forma nem működik (a STOP LED nem aludt ki, a gombok a firmware állapotában ragadnak). Ez az alap a kódban (`led_map.h`); a korábbi `KICK_AKAI_LED_MODE` kapcsoló és a `note`/`noteoff0` ág törölve.

## 2. Megfigyelt színek (`cc` mód)
| Gomb (note) | Állapot | Szín |
|---|---|---|
| STOP (81) | bekapcsolva (megállított lejátszás) | fehér, erős |
| PLAY (82) | lejátszás | zöld, erős |
| PLAY START (83) | (alap) | zöld |
| AUTOMATION (75) | 1. nyomás | piros |
| AUTOMATION (75) | 2. nyomás | zöld |
| Pad bank A (35) | indulási állapot | fehér |
| MAIN (52) | indulási állapot | fehér (kamerán nem egyértelmű) |
| Pad-ek | alap, bank A | többszínű (a program/track színe); FULL LEVEL, bank C után egységesen rózsaszín-magenta |

Nem eldönthető a kamerából (felbontás/látószög): REC (73) és OVERDUB (80) két színe, a bank B–D gombok színei, TC (12), METRONOME (112), NOTE REPEAT (11), FULL LEVEL (39), ARP/LATCH és az oktáv-gombok (a bal oldali oszlop nincs a képben), LOCATE (70). Ezekhez közelebbi kamera vagy szemrevételezés kell.

## 3. Kiegészítés (kamerás és fotós mérés, `cc` mód, 2026-11-01)
| Gomb | Szín / állapot |
|---|---|
| STOP | fehér-kékes, erős |
| PLAY | zöld (lejátszáskor) |
| PLAY START | zöld, mindig (a Kick nem vezérli) |
| `<`, `>`, `<<`, `>>`, `–`, `+` | kék-ciánkék, mindig (a Kick nem vezérli) |
| REC, OVERDUB | halvány szürke a firmware alapfénye; a Kick a szekvencia-felvétel állapotát követi (REC élesítve piros / felvétel villog, OVERDUB zöld: kamerás ellenőrzés nyitva) |
| MIX / PAD MIX | fehér / piros |
| MUTE / PAD MUTE | fehér / piros |
| GRID / STEP SEQ | világít (a két állapot színe nem különíthető el) |
| AUTOMATION | zöld = Read, piros = Write, sötét = Off |
| KEYBOARD CONTROL | rózsaszínes-piros |
| SAMPLE EDIT / SAMPLER | piros árnyalat / fehér (nem biztos) |
| Pad bank A–D | mindig csak a kiválasztott, fehér |
| TAP TEMPO (GLOBAL), LATCH, TOUCH FX, TOUCH STRIP | kékes-fehér, bekapcsolva |
| OCT + | zöldes-fehér, bekapcsolva |
| SHIFT | bal mindig világít (piros felirat), jobb csak Shift-nyomásra |
| Q-Link 1 LED | piros, mindig |
| Oktáv-kijelző (−3 … +3) | két piros pont (−1, 0), mindig |
| Kerekek | kék háttérvilágítás, mindig |
