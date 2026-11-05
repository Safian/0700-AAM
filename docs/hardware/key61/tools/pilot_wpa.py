import json, sys, time
from mlx_lm import load, generate
src = open(sys.argv[1]).read()
model, tok = load("mlx-community/Qwen3.6-35B-A3B-8bit")
sysmsg = ("Te egy pontos dokumentum-elemző vagy. Csak a megadott szövegből dolgozol, semmit nem találsz ki. "
          "Kimenet: kizárólag egy JSON tömb, magyarázat és kódblokk nélkül.")
user = ("A szöveg egy MPC Key 61 vezérlő-leírás (számozott tételek). Minden számozott tételhez adj egy objektumot: "
        '{"item": <szám>, "name": "<a vezérlő neve a szövegből>", "plain": "<mit csinál sima nyomásra, rövid, a szövegből>", '
        '"shift": "<Shift+ funkció vagy null>", "locate": "<Locate+ funkció vagy null>", "other": "<más (dupla nyomás, nyomva tartás), vagy null>"}. '
        "Csak a szövegben szereplő állításokat írd le.\n\n---\n" + src)
msgs = [{"role": "system", "content": sysmsg}, {"role": "user", "content": user}]
prompt = tok.apply_chat_template(msgs, add_generation_prompt=True, tokenize=False, enable_thinking=False)
t0 = time.time()
out = generate(model, tok, prompt=prompt, max_tokens=6000, verbose=False)
dt = time.time() - t0
open(sys.argv[2], "w").write(out)
print("prompt_chars", len(prompt), "out_chars", len(out), "secs", round(dt, 1))
