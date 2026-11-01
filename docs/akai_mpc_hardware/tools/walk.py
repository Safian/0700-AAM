import json, sys, time, re
from mlx_lm import load, generate
src = open(sys.argv[1]).read()
wp, first = sys.argv[3], int(sys.argv[4])
model, tok = load("mlx-community/Qwen3.6-35B-A3B-8bit")
sysmsg = ("Te egy pontos dokumentum-elemző vagy. Csak a megadott szövegből dolgozol, semmit nem találsz ki. "
          "Kimenet: kizárólag egy JSON tömb, magyarázat és kódblokk nélkül.")
user = ("A szöveg az MPC v3.10 felhasználói útmutató egy része. Bontsd fel önálló, ellenőrizhető állításokra: "
        "minden olyan mondat/tétel, ami egy funkció működését, beállítását vagy viselkedését írja le. "
        'Soronként egy objektum: {"seq": <sorszám 1-től>, "page": <a szöveg oldalszáma, ha kiderül, különben null>, '
        '"topic": "<funkció/ablak/mező neve>", "claim": "<az állítás rövid, pontos parafrázisa, angolul vagy magyarul, a szövegből>", '
        '"kind": "<one of: button, field, behavior, ui, other>"}. '
        "Ne adj hozzá semmit, ami nincs a szövegben. A szöveg:\n\n---\n" + src)
msgs = [{"role": "system", "content": sysmsg}, {"role": "user", "content": user}]
prompt = tok.apply_chat_template(msgs, add_generation_prompt=True, tokenize=False, enable_thinking=False)
t0 = time.time()
out = generate(model, tok, prompt=prompt, max_tokens=9000, verbose=False)
dt = time.time() - t0
raw = re.sub(r"^```(?:json)?|```$", "", out.strip(), flags=re.M).strip()
try:
    rows = json.loads(raw)
    for i, r in enumerate(rows, 1): r["id"] = f"{wp}-{first + i - 1:03d}"
    open(sys.argv[2], "w").write(json.dumps(rows, ensure_ascii=False, indent=1))
    print("rows", len(rows), "secs", round(dt, 1))
except Exception as e:
    open(sys.argv[2] + ".raw", "w").write(out); print("JSON hiba", e, "secs", round(dt, 1))
