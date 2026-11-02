#!/usr/bin/env python3
"""Build a Kick project "Piano" with one multisampled Keygroup program from the MPC factory "F9 Grand Piano" WAVs.

Usage: make_piano_project.py <template.kickPrj> <wav-dir> <out-dir> [--name Piano]
  template.kickPrj  any Kick project (Demo 01): its buses, header and footer are reused
  wav-dir           folder with 'Inst-Piano-F9 Grand Piano-<note> <name> <velocity>.WAV' (note 24..96 every 3rd, velocity 046/084/105/127)
  out-dir           receives '<name>.kickPrj' and 'samples/' (copied, renamed GrandPiano_<note>_<vel>.wav)
Each root note becomes a clip (keyNote = root, zone = root-1..root+1, outer zones extended to 0 and 127) with four layers selected
by velocity (layerPlay = VELOCITY). The program is type 1 (Keygroup); one track plays it.
"""
import glob, os, re, shutil, sys, wave

tmpl, wavdir, out = sys.argv[1:4]
name = sys.argv[sys.argv.index("--name") + 1] if "--name" in sys.argv else "Piano"
VEL = [("046", 0, 64), ("084", 65, 94), ("105", 95, 116), ("127", 117, 127)]
os.makedirs(os.path.join(out, "samples"), exist_ok=True)

roots = {}
for f in glob.glob(os.path.join(wavdir, "*Grand Piano-*.WAV")):
    m = re.search(r"Grand Piano-(\d+) \S+ (\d+)\.WAV$", f)
    roots.setdefault(int(m.group(1)), {})[m.group(2)] = f
notes = sorted(roots)

assets, clips, sid = [], [], 100
for i, n in enumerate(notes):
    lo = 0 if i == 0 else n - 1
    hi = 127 if i == len(notes) - 1 else n + 1
    layers = []
    for vel, vs, ve in VEL:
        src = roots[n][vel]
        rel = f"samples/GrandPiano_{n:03d}_{vel}.wav"
        shutil.copyfile(src, os.path.join(out, rel))
        with wave.open(src) as w: frames = w.getnframes()
        sid += 1
        assets.append(f'      <FILE path="{rel}">\n        <SLICE id="SL_{sid}" name="GrandPiano_{n:03d}_{vel}" loopStart="0" loopEnd="{frames}"/>\n      </FILE>')
        layers.append(f'            <LAYER source="{rel}" slice="0" velocityStart="{vs}" velocityEnd="{ve}"/>')
    clips.append(f'''        <CLIP keyNote="{n}" minNote="{lo}" maxNote="{hi}" samplePlay="0" layerPlay="1" gain="1.00" pan="0.00">
          <LAYERS>
{chr(10).join(layers)}
          </LAYERS>
          <SPLAY/>
          <SMUTE/>
          <LFO/>
          <PITCHENV/>
          <FILTERENV/>
          <AMPENV/>
        </CLIP>''')

x = open(tmpl, encoding="utf-8").read()
x = re.sub(r'<ROOT name="[^"]*"', f'<ROOT name="{name}"', x, count=1)
x = re.sub(r"<ASSETS>.*?</ASSETS>", lambda m: "<ASSETS>\n" + "\n".join(assets) + "\n    </ASSETS>", x, flags=re.S)
prog = f'<PROGRAMS>\n    <PROGRAM name="Grand Piano" type="1" id="GrandPno">\n      <INSERTS/>\n      <CLIPS>\n' + "\n".join(clips) + '\n      </CLIPS>\n    </PROGRAM>\n  </PROGRAMS>'
x = re.sub(r"<PROGRAMS>.*?</PROGRAMS>", lambda m: prog, x, flags=re.S)
seq = ('<TRACKS>\n        <TRACK id="TRK_01" name="Piano" program="GrandPno" length="8" isSeqLength="1" level="100" pan="0" mute="0" solo="0">\n'
       '          <NOTELIST/>\n        </TRACK>\n      </TRACKS>')
x = re.sub(r"<TRACKS>.*?</TRACKS>", lambda m: seq, x, flags=re.S)
open(os.path.join(out, f"{name}.kickPrj"), "w", encoding="utf-8").write(x)
print(f"{name}: {len(notes)} samples x {len(VEL)} layers, notes {notes[0]}..{notes[-1]}")
