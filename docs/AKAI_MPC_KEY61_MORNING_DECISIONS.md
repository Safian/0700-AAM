# Reggeli átnézés: éjszakai döntések és dummy elemek

A felhasználó utasítása: ne álljunk meg apró döntés miatt; ha UI-elem kell, **dummyt** teszünk be (működő hely, látható jelölés, minimális viselkedés), reggel véglegesítjük. Minden worker és az orchestrator ide jegyzi (sorokat hozzáfűzve):

| Dátum | Terület | Döntés / dummy | Indok | Hol (fájl) | Reggeli teendő |
|---|---|---|---|---|---|

## Már meghozott döntések (round 1)
- Sustain és pitch bend csak az élő bemenetre hat (Behringer-elv): megvalósítva (`noteProcess(msgs, live)`).
- RECALL: tick vagy wall-clock eseményenként; megállítva rögzített hosszú felvétel a track hosszára hajtogatva.
- Song módban a seek tiltott.
- Globális TC kapcsoló csak az Apply-ra érvényesül.
- `KICK_TOUCH_SINGLE_TAP_LISTBOX` CMake opció (alap: ki); az MPC-s build ezt BE kell kapcsolja.
