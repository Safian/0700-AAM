#!/usr/bin/env python3
"""Generates the static HTML mockups of the Kick Piano UI (design study only).

Usage:  python3 build.py        (writes *.html next to this file)
The generated HTML is plain and can also be edited by hand; this script only keeps the
88-key keyboard geometry, SVG knob arcs and repeated widgets consistent.
"""
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))

# ---------------------------------------------------------------- icons
LOGO = ('<svg viewBox="0 0 32 32" aria-hidden="true"><rect x="1" y="3" width="30" height="26" rx="4" fill="#ffb608"/>'
        '<g fill="#1b1304"><rect x="5" y="7" width="3.2" height="12" rx=".6"/><rect x="9.6" y="7" width="3.2" height="12" rx=".6"/>'
        '<rect x="16.8" y="7" width="3.2" height="12" rx=".6"/><rect x="21.4" y="7" width="3.2" height="12" rx=".6"/>'
        '<rect x="5" y="21" width="22" height="2.4" rx="1.2"/></g></svg>')
I_PREV = '<svg viewBox="0 0 16 16"><path d="M10 3 5 8l5 5"/></svg>'
I_NEXT = '<svg viewBox="0 0 16 16"><path d="m6 3 5 5-5 5"/></svg>'
I_SAVE = '<svg viewBox="0 0 16 16"><path d="M3 2.5h8.5L13.5 4.5V13a.5.5 0 0 1-.5.5H3a.5.5 0 0 1-.5-.5V3a.5.5 0 0 1 .5-.5Z"/><path d="M5 2.5v3.5h5V2.5M5 13.5V9.5h6v4"/></svg>'
I_CARET = '<svg viewBox="0 0 12 12"><path d="m2 4 4 4 4-4"/></svg>'
I_INFO = '<svg viewBox="0 0 16 16"><circle cx="8" cy="8" r="6"/><path d="M8 7.2V11M8 5v.2"/></svg>'
I_PEDAL = '<svg viewBox="0 0 18 14"><path d="M2 12h14M5 12 7 3h4l2 9"/></svg>'
I_DL = '<svg viewBox="0 0 16 16"><path d="M8 2.5v8M4.5 7.5 8 11l3.5-3.5M3 13.5h10"/></svg>'
I_DL_TAG = I_DL.replace('<svg', '<svg width="12" height="12"')
I_GRAND = '<svg class="ico" viewBox="0 0 30 22"><path d="M3 17V9c0-3 3-5 7-5h9c5 0 8 3 8 6 0 3-3 4-7 4l-5 2v3M3 17h12"/></svg>'
I_UPRIGHT = '<svg class="ico" viewBox="0 0 30 22"><rect x="6" y="3" width="18" height="16" rx="1"/><path d="M6 13h18M11 13v6M15 13v6M19 13v6"/></svg>'
I_ELEC = '<svg class="ico" viewBox="0 0 30 22"><rect x="3" y="7" width="24" height="9" rx="1.5"/><path d="M6 7V4h18v3M7 16v3M23 16v3"/></svg>'

# ---------------------------------------------------------------- helpers
def tip(t):
    return f' data-tip="{t}"' if t else ''


def arc_d(r=20, cx=24, cy=24, a0=-135, a1=135):
    def p(a):
        t = math.radians(a)
        return cx + r * math.sin(t), cy - r * math.cos(t)
    (x0, y0), (x1, y1) = p(a0), p(a1)
    return f'M{x0:.2f} {y0:.2f}A{r} {r} 0 1 1 {x1:.2f} {y1:.2f}'


ARC = arc_d()


def valtxt(v, lo, hi, unit, dec, sign):
    x = lo + (hi - lo) * v
    s = f'{x:.{dec}f}'
    if sign and x > 0:
        s = '+' + s
    return s + unit


def knob(label, v, lo=0, hi=100, unit='', dec=0, t='', disabled=False, bip=False, default=None, sign=False):
    default = v if default is None else default
    if bip:
        a = (v - .5) * 100
        frm = 50 if a >= 0 else 50 + a
        dash = f'stroke-dasharray:{max(abs(a), .01):.2f} 200;stroke-dashoffset:{-frm:.2f}'
    else:
        dash = f'stroke-dasharray:{v * 100:.2f} 200'
    dis = ' aria-disabled="true"' if disabled else ''
    sg = ' data-sign="1"' if sign else ''
    return (f'<div class="ctl knob{" bip" if bip else ""}" role="slider" tabindex="0" aria-label="{label}" aria-valuemin="{lo}" aria-valuemax="{hi}" '
            f'aria-valuenow="{lo + (hi - lo) * v:.{dec}f}" aria-valuetext="{valtxt(v, lo, hi, unit, dec, sign)}" data-v="{v}" data-def="{default}" data-min="{lo}" data-max="{hi}" '
            f'data-unit="{unit}" data-dec="{dec}"{sg}{dis}{tip(t)}>'
            f'<div class="dial"><svg viewBox="0 0 48 48" aria-hidden="true"><path class="k-track" pathLength="100" d="{ARC}"/>'
            f'<path class="k-val" pathLength="100" d="{ARC}" style="{dash}"/></svg>'
            f'<div class="cap"><i class="ptr" style="--rot:{-135 + 270 * v:.1f}deg"></i></div></div>'
            f'<span class="val">{valtxt(v, lo, hi, unit, dec, sign)}</span><span class="lab">{label}</span></div>')


def fader_v(label, v, lo=-60, hi=6, unit=' dB', dec=1, t='', meter=True, mini=False, bip=False, default=None, sign=False, lvl=(.62, .55)):
    default = v if default is None else default
    if bip:
        fill = f'bottom:{min(v, .5) * 100:.1f}%;height:{abs(v - .5) * 100:.1f}%'
    else:
        fill = f'height:{v * 100:.1f}%'
    mt = ''
    if meter:
        mt = (f'<div class="f-meter" aria-hidden="true"><i style="--l:{lvl[0] * 100:.0f}%;--d:-.3s"></i><i style="--l:{lvl[1] * 100:.0f}%;--d:-.9s"></i></div>')
    center = '<div class="f-center"></div>' if bip else ''
    sg = ' data-sign="1"' if sign else ''
    return (f'<div class="ctl fader v{" mini" if mini else ""}{" bip" if bip else ""}" role="slider" tabindex="0" aria-orientation="vertical" aria-label="{label}" '
            f'aria-valuemin="{lo}" aria-valuemax="{hi}" aria-valuenow="{lo + (hi - lo) * v:.{dec}f}" aria-valuetext="{valtxt(v, lo, hi, unit, dec, sign)}" '
            f'data-v="{v}" data-def="{default}" data-min="{lo}" data-max="{hi}" data-unit="{unit}" data-dec="{dec}"{sg}{tip(t)}>'
            f'<span class="val">{valtxt(v, lo, hi, unit, dec, sign)}</span>'
            f'<div class="f-wrap">{mt}<div class="f-track"><div class="f-ticks"></div>{center}<div class="f-fill" style="{fill}"></div>'
            f'<div class="f-thumb" style="bottom:{v * 100:.1f}%"></div></div></div><span class="lab">{label}</span></div>')


def fader_h(label, v, lo=-60, hi=6, unit=' dB', dec=1, t=''):
    return (f'<div class="ctl fader h" role="slider" tabindex="0" aria-label="{label}" aria-valuemin="{lo}" aria-valuemax="{hi}" '
            f'aria-valuenow="{lo + (hi - lo) * v:.{dec}f}" aria-valuetext="{valtxt(v, lo, hi, unit, dec, False)}" data-v="{v}" data-def="{v}" data-min="{lo}" data-max="{hi}" '
            f'data-unit="{unit}" data-dec="{dec}" style="--w:{v * 100:.1f}%"{tip(t)}>'
            f'<span class="lab">{label}</span><div class="f-wrap"><div class="f-track"><div class="f-ticks"></div><div class="f-fill"></div><div class="f-thumb"></div></div></div>'
            f'<span class="val">{valtxt(v, lo, hi, unit, dec, False)}</span></div>')


def toggle(label, on=False, disabled=False, t='', sync=''):
    s = f' data-sync="{sync}"' if sync else ''
    d = ' aria-disabled="true"' if disabled else ''
    return (f'<button class="tog" type="button" aria-pressed="{str(on).lower()}"{d}{s}{tip(t)}><span class="led"></span>{label}</button>')


def pswitch(label, on=True):
    return f'<button class="pswitch" type="button" aria-pressed="{str(on).lower()}" aria-label="{label}" data-tip="{label}"></button>'


def panel(cls, pid, title, body, extra=''):
    return (f'<section class="panel {cls}" aria-labelledby="{pid}"><h2 class="ptitle" id="{pid}">{title}{extra}</h2>'
            f'<div class="pbody">{body}</div></section>')


# ---------------------------------------------------------------- keyboard (A0..C8)
def keyboard(pressed, sustained, labels=True):
    """pressed: {midi: velocity}; sustained: set(midi) (released but held by the sustain pedal)."""
    whites = [n for n in range(21, 109) if n % 12 not in (1, 3, 6, 8, 10)]
    assert len(whites) == 52
    w = 100 / 52
    out = []
    widx = {n: i for i, n in enumerate(whites)}

    def band(v):
        return 1 if v < 32 else 2 if v < 64 else 3 if v < 90 else 4 if v < 112 else 5

    def cls(n, base):
        c = f'k {base}'
        if n in pressed:
            c += f' on v{band(pressed[n])}'
        elif n in sustained:
            c += ' sus'
        if n == 60:
            c += ' mc'
        return c

    names = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
    for n in whites:
        label = ''
        if labels and n % 12 == 0:
            label = f'<span class="nm">C{n // 12 - 1}</span>'
        out.append(f'<div class="{cls(n, "w")}" data-n="{n}" style="left:{widx[n] * w:.4f}%" aria-label="{names[n % 12]}{n // 12 - 1}">{label}</div>')
    for n in range(21, 109):
        if n % 12 in (1, 3, 6, 8, 10):
            left = (widx[n - 1] + 1) * w - w * .62 / 2
            out.append(f'<div class="{cls(n, "b")}" data-n="{n}" style="left:{left:.4f}%" aria-label="{names[n % 12]}{n // 12 - 1}"></div>')
    return ('<div class="keys" tabindex="0" role="group" aria-label="Piano keyboard, 88 keys A0 to C8. Arrow keys move, Space or Enter plays, Page Up and Page Down jump an octave." '
            'data-tip="88-key keyboard|Click to play (higher on the key = harder). Arrow keys + Space from the keyboard.">'
            + ''.join(out) + '</div>')


PRESSED = {48: 70, 52: 78, 55: 74, 59: 96, 76: 118, 84: 22}
SUSTAINED = {45, 62, 67, 71}


def kbd_bar(sustain=True, compact=False):
    acts = ''.join(f'<i style="--h:{h}px"></i>' for h in (3, 6, 10, 4, 12, 7, 3, 8, 5, 11, 4, 3, 6, 9, 3, 2))
    leg = ('<div class="vlegend" aria-label="Velocity colours"><span class="lab">Vel</span><i class="v1"></i><i class="v2"></i><i class="v3"></i><i class="v4"></i><i class="v5"></i>'
           '<span class="lab">pp&nbsp;&rarr;&nbsp;ff</span></div>')
    return (f'<div class="kbd-bar"><div class="pedalind" data-on="{str(sustain).lower()}" data-tip="Sustain pedal (CC64)|Mirrors the incoming MIDI pedal or the Sustain button.">'
            f'{I_PEDAL}<span class="lab">Sustain</span><span class="pbar"><i style="--p:{100 if sustain else 0}%"></i></span><span class="val">{100 if sustain else 0}%</span></div>'
            f'{leg}'
            f'<div class="midi" data-tip="MIDI activity|Port: Kick MIDI In (example)"><span class="lab">MIDI</span><span class="led on"></span>'
            f'<span class="msg val">Ch1 &middot; Note On E5 &middot; vel 118</span><span class="acts" aria-hidden="true">{acts}</span></div></div>')


# ---------------------------------------------------------------- header / strips / footer
def header(compact=False, ram=True):
    stats = ('<div class="stats" role="group" aria-label="Engine status">'
             '<div class="stat" data-tip="Active voices|23 of 128 max polyphony"><span class="lab">Voices</span><span class="val">23 / 128</span><span class="bar"><i style="--p:18%"></i></span></div>'
             '<div class="stat cpu" data-tip="CPU load|Audio thread, 1 s average"><span class="lab">CPU</span><span class="val">12%</span><span class="bar"><i style="--p:12%"></i></span></div>'
             + ('<div class="stat ram" data-tip="Sample memory|Resident sample RAM of 8 GB budget"><span class="lab">RAM</span><span class="val">1.8 GB</span><span class="bar"><i style="--p:22%"></i></span></div>' if ram else '')
             + '</div>')
    return (f'<header class="hdr"><div class="brand">{LOGO}<span class="brand-name">KICK<b>PIANO</b></span></div>'
            f'<div class="preset" role="group" aria-label="Preset">'
            f'<button class="btn icon" type="button" data-act="prev" aria-label="Previous preset" data-tip="Previous preset|Shortcut: [">{I_PREV}</button>'
            f'<button class="preset-name" type="button" aria-haspopup="listbox" aria-label="Choose preset" data-tip="Preset browser"><span class="txts"><span class="cat">Concert Grand</span><span class="nm">Warm Studio Close</span></span>{I_CARET}</button>'
            f'<button class="btn icon" type="button" data-act="next" aria-label="Next preset" data-tip="Next preset|Shortcut: ]">{I_NEXT}</button>'
            f'<button class="btn icon" type="button" aria-label="Save preset" data-tip="Save preset|Ctrl/Cmd+S">{I_SAVE}</button></div>'
            f'<span class="sp"></span>'
            f'<div class="master">{fader_h("Master", .82, -60, 6, " dB", 1, "Master volume|Double-click resets to 0 dB")}</div>'
            f'{stats}'
            + (f'<a class="btn icon" href="about.html" aria-label="About and credits" data-tip="About, licence and sample credits">{I_INFO}</a>' if compact else '')
            + '</header>')


def loadstrip(state='ready'):
    if state == 'loading':
        return ('<div class="loadstrip" data-state="loading" role="status"><span class="dot"></span>'
                '<span>Loading sample set &ndash; Concert Grand</span><span class="prog" style="--p:64%"><i></i></span><span class="val">64% &middot; 3.1 / 4.8 GB</span>'
                '<span class="r">Streaming from SSD &middot; 42 MB/s &middot; playable while loading</span></div>')
    return ('<div class="loadstrip" data-state="ready" role="status"><span class="dot"></span>'
            '<span>Sample set ready &ndash; Concert Grand &middot; 16 velocity layers &middot; 4.8 GB</span><span class="r">Hybrid streaming (RAM preload + disk)</span></div>')


def footer():
    return ('<footer class="ftr"><span>v0.1.0-mock</span><span class="sep">|</span><span>CLAP</span><span class="sep">|</span><span>AGPL-3.0-or-later</span><span class="sep">|</span>'
            '<span class="attr">Samples: [Sample library name] by [Author], CC-BY 3.0 (placeholder)</span>'
            '<a href="about.html" data-tip="Credits, licences, version">About &amp; credits</a></footer>')


# ---------------------------------------------------------------- panels (main layout)
def instrument_panel():
    cards = (
        '<div class="cards" role="radiogroup" aria-label="Instrument model">'
        f'<button class="card" role="radio" aria-checked="true" tabindex="0" type="button">{I_GRAND}<span class="t"><b>Concert Grand</b><span>9 ft &middot; 16 vel layers &middot; 4.8 GB</span></span><span class="tag">Loaded</span></button>'
        f'<button class="card" role="radio" aria-checked="false" tabindex="-1" type="button">{I_UPRIGHT}<span class="t"><b>Upright</b><span>Upright &middot; 12 vel layers &middot; 2.1 GB</span></span><span class="tag">Ready</span></button>'
        f'<button class="card" role="radio" aria-checked="false" aria-disabled="true" tabindex="-1" type="button" data-tip="Electric piano (example)|Not installed. Download from the sample manager.">{I_ELEC}<span class="t"><b>Electric (example)</b><span>Not installed &middot; 1.2 GB</span></span><span class="tag">{I_DL_TAG}</span></button>'
        '</div>')
    return panel('p-inst', 'h-inst', 'Instrument', cards)


def mic_panel():
    f = ''.join([
        fader_v('Close', .74, t='Close mics|Direct, detailed, dry. Double-click = 0 dB', lvl=(.62, .58)),
        fader_v('Player', .66, t='Player position|Ears-at-the-bench perspective', lvl=(.55, .5)),
        fader_v('Room', .52, t='Room mics|Hall ambience and decay', lvl=(.42, .4)),
        fader_v('Far', .28, t='Far mics|Distant audience perspective', lvl=(.25, .22)),
    ])
    return panel('p-mic', 'h-mic', 'Microphone mix', f'<div class="mics" role="group" aria-label="Microphone positions">{f}</div>')


def pedal_panel():
    body = ('<div class="col"><div class="pair">'
            + toggle('Sustain', True, t='Sustain pedal|Also follows CC64', sync='sustain')
            + toggle('Una corda', False, t='Una corda (soft pedal)|Also follows CC67')
            + '</div><div class="pair">'
            + knob('Resonance', .46, 0, 100, '%', 0, 'Sympathetic string resonance|Level when the damper pedal is down')
            + knob('Pedal noise', .30, 0, 100, '%', 0, 'Pedal noise level|Not available in this sample set', disabled=True)
            + '</div></div>')
    return panel('p-pedal', 'h-pedal', 'Pedals', body)


def tone_panel():
    body = ('<div class="pair">' + knob('Brightness', .62, 0, 100, '', 0, 'Brightness|Tilts the upper partials')
            + knob('Tone', .5, -6, 6, ' dB', 1, 'Tone|Warm &lt;-&gt; bright shelving', bip=True, sign=True) + '</div>')
    return panel('p-tone', 'h-tone', 'Tone', body)


def velocity_panel():
    curve = ('<div class="curve"><div class="plot">'
             '<svg viewBox="0 0 100 100" preserveAspectRatio="none" tabindex="0" role="slider" aria-label="Velocity curve" aria-valuemin="0" aria-valuemax="100" aria-valuenow="50" data-hy="42" '
             'data-tip="Velocity curve|Drag vertically. Arrow keys fine-tune, Home resets.">'
             '<path class="grid" d="M25 0V100M50 0V100M75 0V100M0 25H100M0 50H100M0 75H100"/>'
             '<path class="diag" d="M0 100 100 0"/><path class="cvfill" d=""/><path class="cv" d=""/>'
             '<line class="live" data-in="72" x1="72" x2="72" y1="100" y2="40"/>'
             '<circle class="hnd" cx="50" cy="42" r="4.2"/></svg>'
             '<div class="axes"><span class="lab">In (key vel)</span><span class="lab">Out</span></div></div>'
             '<div class="presets" role="group" aria-label="Curve presets">'
             '<button class="btn" type="button" data-curve="62" data-tip="Soft touch: lighter playing sounds louder">Soft</button>'
             '<button class="btn" type="button" data-curve="50">Linear</button>'
             '<button class="btn" type="button" data-curve="36" data-tip="Hard touch: needs a firmer attack">Hard</button></div></div>')
    return panel('p-vel', 'h-vel', 'Velocity', curve)


def tune_panel():
    body = ('<div class="stack"><div class="stepper"><button class="btn icon" type="button" data-step="-0.1" data-target="tune" aria-label="Lower tuning" data-tip="Tuning down 0.1 Hz">' + I_PREV + '</button>'
            '<div class="readout" data-tip="Master tuning (A4)|Hz, with cents offset"><span class="mono"><span id="tune">440.0</span> Hz</span><span class="lab mono" data-cents>+0.0 ct</span></div>'
            '<button class="btn icon" type="button" data-step="0.1" data-target="tune" aria-label="Raise tuning" data-tip="Tuning up 0.1 Hz">' + I_NEXT + '</button></div>'
            + knob('Stretch', .5, -100, 100, '%', 0, 'Stretch tuning|Widens bass and treble octaves', bip=True, sign=True)
            + '</div>')
    return panel('p-tune', 'h-tune', 'Tuning', body)


def room_panel():
    body = ('<div class="stack" style="align-self:stretch;justify-content:space-between"><div class="sel"><select aria-label="Reverb type" data-tip="Reverb type"><option>Concert Hall</option><option>Studio</option><option>Chamber</option><option>Cathedral</option><option>Off</option></select></div>'
            '<div class="pair">' + knob('Mix', .28, 0, 100, '%', 0, 'Reverb mix') + knob('Release', .4, 0, 100, '%', 0, 'Release noise|Level of the key-up mechanical noise') + '</div></div>')
    return panel('p-room', 'h-room', 'Room &amp; release', body, extra=pswitch('Reverb on'))


def eq_panel():
    body = ('<div class="minis">'
            + fader_v('Comp', .35, 0, 100, '', 0, 'Compressor amount|Gentle bus compression', meter=False, mini=True)
            + fader_v('Low', .5, -12, 12, '', 0, 'Low shelf, dB', meter=False, mini=True, bip=True, sign=True)
            + fader_v('Mid', .56, -12, 12, '', 0, 'Mid band, dB', meter=False, mini=True, bip=True, sign=True)
            + fader_v('High', .46, -12, 12, '', 0, 'High shelf, dB', meter=False, mini=True, bip=True, sign=True) + '</div>')
    return panel('p-eq', 'h-eq', 'Comp / EQ', body, extra=pswitch('Comp and EQ on'))


# ---------------------------------------------------------------- page shell
def page(title, body_class, plugin_html, demobar_extra='', back=None):
    return f'''<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{title} - Kick Piano UI mockup</title>
<link rel="stylesheet" href="style.css">
<script>if(/[?&]embed/.test(location.search))document.documentElement.classList.add('embed')</script>
</head>
<body{(" data-back=" + chr(34) + back + chr(34)) if back else ""}>
<div class="demobar"><a href="index.html">&larr; Index</a><span>{title}</span><span class="sp"></span>{demobar_extra}</div>
<main class="stage">
{plugin_html}
</main>
<script src="ui.js" defer></script>
</body>
</html>
'''


def size_bar(heights, default):
    pressed = ' aria-pressed="true"'
    btns = ''.join(f'<button type="button" data-size="{w}" data-h="{h}"{pressed if w == default else ""}>{w}</button>' for w, h in heights)
    return f'<span class="grp">Width {btns}</span>'


# the main view may be taller at 640 px width (the sections scroll below ~840 px; the real plugin would switch to the compact layout)
SIZE_BAR = size_bar([(640, 720), (960, 600), (1280, 800)], 960)
SIZE_BAR_C = size_bar([(640, 400), (960, 600), (1280, 800)], 640)
THEME_BAR = ('<span class="grp">Theme <button type="button" data-theme-set="dark"{d}>Dark</button><button type="button" data-theme-set="light"{l}>Light</button></span>')


def main_view(theme, loading):
    body = (instrument_panel() + mic_panel() + pedal_panel() + tone_panel() + velocity_panel() + tune_panel() + room_panel() + eq_panel())
    plugin = (f'<div class="plugin" data-theme="{theme}">{header()}{loadstrip("loading" if loading else "ready")}'
              f'<div class="main">{body}</div>'
              f'<div class="kbd">{kbd_bar(True)}{keyboard(PRESSED, SUSTAINED)}</div>{footer()}</div>')
    bar = THEME_BAR.format(d=' aria-pressed="true"' if theme == 'dark' else '', l=' aria-pressed="true"' if theme == 'light' else '') + SIZE_BAR
    return page(f'Main view, {theme}', '', plugin, bar)


def compact_view():
    def tp(i, label, content, sel=False):
        return (f'<div class="tabpanel" role="tabpanel" id="tp-{i}" aria-labelledby="t-{i}"{"" if sel else " hidden"}>{content}</div>')

    cards = instrument_panel()
    vel = velocity_panel()
    inst = f'<div style="display:grid;grid-template-columns:1.25fr 1fr;gap:8px;width:100%">{cards}{vel}</div>'
    mics = mic_panel().replace('p-mic', 'p-mic-c')
    ped = ('<section class="panel" aria-labelledby="h-pedal-c"><h2 class="ptitle" id="h-pedal-c">Pedals</h2><div class="pbody" style="gap:16px">'
           + toggle('Sustain', True, sync='sustain', t='Sustain pedal') + toggle('Una corda', False, t='Una corda')
           + knob('Resonance', .46, 0, 100, '%', 0, 'Pedal resonance') + knob('Pedal noise', .3, 0, 100, '%', 0, 'Not available in this sample set', disabled=True) + '</div></section>')
    tone = ('<section class="panel" aria-labelledby="h-tone-c"><h2 class="ptitle" id="h-tone-c">Tone &amp; tuning</h2><div class="pbody">'
            + knob('Brightness', .62, 0, 100, '', 0, 'Brightness') + knob('Tone', .5, -6, 6, ' dB', 1, 'Tone', bip=True, sign=True)
            + '<div class="stepper" style="width:110px"><div class="readout"><span class="mono"><span id="tune">440.0</span> Hz</span><span class="lab mono" data-cents>+0.0 ct</span></div>'
            + '<div style="display:flex;flex-direction:column;gap:2px"><button class="btn icon" type="button" data-step="0.1" data-target="tune" aria-label="Raise tuning">' + I_NEXT.replace('m6 3 5 5-5 5', 'm3 10 5-5 5 5') + '</button>'
            + '<button class="btn icon" type="button" data-step="-0.1" data-target="tune" aria-label="Lower tuning">' + I_PREV.replace('M10 3 5 8l5 5', 'm3 6 5 5 5-5') + '</button></div></div>'
            + knob('Stretch', .5, -100, 100, '%', 0, 'Stretch tuning', bip=True, sign=True) + knob('Release', .4, 0, 100, '%', 0, 'Release noise') + '</div></section>')
    room = ('<section class="panel" aria-labelledby="h-room-c"><h2 class="ptitle" id="h-room-c">Room, comp &amp; EQ</h2><div class="pbody" style="gap:12px"><div class="stack" style="max-width:130px"><span class="lab">Reverb</span>'
            '<div class="sel"><select aria-label="Reverb type"><option>Concert Hall</option><option>Studio</option><option>Chamber</option><option>Cathedral</option><option>Off</option></select></div></div>'
            + knob('Mix', .28, 0, 100, '%', 0, 'Reverb mix')
            + '<div class="minis">' + fader_v('Comp', .35, 0, 100, '', 0, 'Compressor', meter=False, mini=True) + fader_v('Low', .5, -12, 12, '', 0, 'Low', meter=False, mini=True, bip=True, sign=True)
            + fader_v('Mid', .56, -12, 12, '', 0, 'Mid', meter=False, mini=True, bip=True, sign=True) + fader_v('High', .46, -12, 12, '', 0, 'High', meter=False, mini=True, bip=True, sign=True) + '</div></div></section>')
    names = ['Instrument', 'Mics', 'Pedals', 'Tone', 'Room']
    tabs = ''.join(f'<button class="tab" role="tab" type="button" id="t-{i}" aria-controls="tp-{i}" aria-selected="{str(i == 1).lower()}" tabindex="{0 if i == 1 else -1}">{n}</button>' for i, n in enumerate(names))
    panels = ''.join([tp(0, 'Instrument', inst), tp(1, 'Mics', mics, True), tp(2, 'Pedals', ped), tp(3, 'Tone', tone), tp(4, 'Room', room)])
    plugin = (f'<div class="plugin compact" data-theme="dark">{header(compact=True, ram=False)}'
              f'<div class="tabs" role="tablist" aria-label="Sections">{tabs}</div>{panels}'
              f'<div class="kbd">{kbd_bar(True, True)}{keyboard(PRESSED, SUSTAINED, labels=False)}</div></div>')
    return page('Compact view, 640 x 400', '', plugin, THEME_BAR.format(d=' aria-pressed="true"', l='') + SIZE_BAR_C)


def loading_view():
    steps = ('<ol class="steps" aria-label="Setup steps"><li class="done">1 Choose</li><li class="now" style="--p:64%">2 Download</li><li>3 Verify</li><li>4 Index</li></ol>')
    item_main = ('<div class="dl-item"><div class="top"><span class="statusdot act"></span><b>Concert Grand</b><span class="val">3.1 / 4.8 GB</span></div>'
                 '<div class="bigbar" role="progressbar" aria-label="Concert Grand download" aria-valuemin="0" aria-valuemax="100" aria-valuenow="64" style="--p:64%"><i></i></div>'
                 '<div class="meta"><span class="val" data-only="downloading">42 MB/s</span><span class="val" data-only="downloading">ETA 0:41</span>'
                 '<span class="val" data-only="paused">Paused at 64%</span><span class="val" data-only="error">Stopped at 64%</span><span class="val">SHA-256 checked per file</span></div></div>')
    item_q = ('<div class="dl-item queued"><div class="top"><span class="statusdot wait"></span><b>Upright</b><span class="val">2.1 GB &middot; queued</span></div></div>')
    item_o = ('<div class="dl-item queued"><div class="top"><span class="statusdot wait"></span><b>Electric (example)</b><span class="val">1.2 GB &middot; optional, not selected</span></div></div>')
    banner = ('<div class="banner" role="alert" data-only="error"><b>Download interrupted.</b><span>The server could not be reached (timeout). Downloaded data is kept; Retry resumes from 64%.</span></div>')
    dl = (f'<section class="dl" data-state="downloading" aria-labelledby="dl-h"><div class="row">{LOGO.replace("<svg", "<svg width=36 height=36")}<div><h1 id="dl-h">Welcome &ndash; getting your piano ready</h1>'
          '<p>First launch: the sample library is downloaded once and stored locally. You can play as soon as the Concert Grand core layers are in.</p></div></div>'
          + steps + banner + item_main + item_q + item_o
          + '<div class="row"><span class="lab">Install to</span><span class="val" style="flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap">~/Library/Application Support/Kick Piano/Samples (example)</span>'
            '<button class="btn" type="button" data-tip="Choose another folder|Change before the download completes">Change&hellip;</button></div>'
          + '<div class="actions"><button class="btn" type="button" data-tip="Point to an existing sample folder instead of downloading">Use existing library&hellip;</button><span class="sp"></span>'
            '<button class="btn danger" type="button" data-only="downloading paused error">Cancel</button>'
            '<button class="btn" type="button" data-only="downloading" data-setstate="paused">Pause</button>'
            '<button class="btn" type="button" data-only="paused" data-setstate="downloading">Resume</button>'
            '<button class="btn primary" type="button" data-only="error" data-setstate="downloading">Retry</button>'
            '<button class="btn primary" type="button" disabled data-tip="Available once the core layers (20%) are installed">Start playing</button></div></section>')
    plugin = (f'<div class="plugin" data-theme="dark"><header class="hdr"><div class="brand">{LOGO}<span class="brand-name">KICK<b>PIANO</b></span></div><span class="sp"></span>'
              f'<span class="lab">First launch setup</span></header><div class="veil">{dl}</div>'
              '<footer class="ftr"><span>v0.1.0-mock</span><span class="sep">|</span><span>AGPL-3.0-or-later</span><span class="sep">|</span><span class="attr">Samples: [Sample library name] by [Author], CC-BY 3.0 (placeholder)</span></footer></div>')
    bar = ('<span class="grp">State <button type="button" data-dlstate="downloading" aria-pressed="true">Downloading</button><button type="button" data-dlstate="paused">Paused</button>'
           '<button type="button" data-dlstate="error">Error</button></span>' + SIZE_BAR)
    return page('First launch / sample download', '', plugin, bar)


def about_view():
    rows = [
        ('Concert Grand', '[Sample library name]', '[Author / studio]', 'CC-BY 3.0'),
        ('Upright', '[Sample library name]', '[Author / studio]', 'CC-BY 3.0'),
        ('Electric (example)', '[Sample library name]', '[Author / studio]', '[licence]'),
    ]
    tr = ''.join(f'<tr><td>{a}</td><td class="ph">{b}</td><td class="ph">{c}</td><td><span class="badge">{d}</span></td></tr>' for a, b, c, d in rows)
    left = (f'<section class="panel id" aria-label="Kick Piano">{LOGO}<h1>KICK<b> PIANO</b></h1><span class="lab">Working title</span>'
            '<dl class="kv" style="margin-top:12px;text-align:left"><dt>Version</dt><dd class="mono">0.1.0-mock</dd><dt>Format</dt><dd>CLAP</dd><dt>Build</dt><dd class="mono">[git hash]</dd><dt>Engine</dt><dd>Kick sampler core</dd></dl>'
            '<div class="row" style="margin-top:12px"><button class="btn" type="button" data-tip="Copy version and build info to the clipboard">Copy info</button></div></section>')
    right = ('<div class="stackv">'
             '<section class="panel" aria-labelledby="h-lic"><h2 class="ptitle" id="h-lic">Licence</h2>'
             '<div class="lic"><span class="badge">AGPL-3.0-or-later</span><span class="textblock">Kick Piano is free software. You can redistribute and modify it under the terms of the GNU Affero General Public License.</span></div>'
             '<div class="row" style="margin-top:8px"><button class="btn" type="button">Licence text</button><button class="btn" type="button">Source code</button><button class="btn" type="button">Third-party licences</button></div></section>'
             '<section class="panel scroll" aria-labelledby="h-cr"><h2 class="ptitle" id="h-cr">Sample credits</h2>'
             '<p class="textblock" style="margin-bottom:8px">The piano sounds are recorded samples. Attribution is required by the licence and is shown here (placeholders).</p>'
             f'<table class="credits"><thead><tr><th scope="col">Instrument</th><th scope="col">Library</th><th scope="col">Author</th><th scope="col">Licence</th></tr></thead><tbody>{tr}</tbody></table>'
             '<div class="row" style="margin-top:12px"><button class="btn" type="button" data-tip="Copy the full attribution text for your project credits">Copy attribution</button></div></section>'
             '<section class="panel" aria-labelledby="h-3p"><h2 class="ptitle" id="h-3p">Built with</h2>'
             '<p class="textblock">[Third-party components and their licences, e.g. UI toolkit, CLAP SDK &ndash; to be filled from the dependency list]</p></section></div>')
    plugin = (f'<div class="plugin" data-theme="dark"><header class="hdr"><div class="brand">{LOGO}<span class="brand-name">KICK<b>PIANO</b></span></div><span class="sp"></span>'
              '<span class="lab">About &amp; credits</span><a class="btn" href="main-dark.html" data-tip="Back to the instrument|Esc">Close</a></header>'
              f'<div class="about">{left}{right}</div>'
              '<footer class="ftr"><span>v0.1.0-mock</span><span class="sep">|</span><span>AGPL-3.0-or-later</span></footer></div>')
    return page('About / credits', '', plugin, THEME_BAR.format(d=' aria-pressed="true"', l='') + SIZE_BAR, back='main-dark.html')


def index_view():
    cards = [
        ('main-dark.html', 'Main view - dark', '960 x 600 default, 640-1280 responsive', 960, 600),
        ('main-light.html', 'Main view - light (loading)', 'Same layout, light theme, sample set still loading', 960, 600),
        ('compact.html', 'Compact 640 x 400', 'Tabbed sections, shortened keyboard', 640, 400),
        ('loading.html', 'First launch / download', 'Sample download: downloading / paused / error', 960, 600),
        ('about.html', 'About / credits', 'Licence, sample attribution, version', 960, 600),
    ]
    items = ''
    for href, t, d, w, h in cards:
        sc = 400 / w
        items += (f'<div class="ic"><div class="thumb" style="width:400px;height:{round(h * sc)}px"><iframe src="{href}?embed" title="{t} (preview)" tabindex="-1" loading="lazy" '
                  f'style="width:{w}px;height:{h}px;transform:scale({sc:.4f})"></iframe></div><div class="cap"><a class="stretched" href="{href}"><b>{t}</b></a><span>{d}</span></div></div>')
    sw = [('#121212', 'bg-0'), ('#1d1d1d', 'bg-1'), ('#2a2a2a', 'panel'), ('#171717', 'well'), ('#3a3a3a', 'cap'), ('#ffb608', 'accent'), ('#00aaee', 'info'), ('#0fe389', 'ok'), ('#ff4a3d', 'error'),
          ('#3daeff', 'vel 1'), ('#1fd3a0', 'vel 2'), ('#ffd02a', 'vel 3'), ('#ff8a1a', 'vel 4'), ('#ff3b30', 'vel 5')]
    swh = ''.join(f'<li><i style="background:{c}"></i><b>{n}</b><span>{c}</span></li>' for c, n in sw)
    return f'''<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Kick Piano - UI mockup index</title>
<link rel="stylesheet" href="style.css">
<style>
 .hub{{max-width:1240px;margin:0 auto;padding:32px 24px 48px;color:#ddd}}
 .hub h1{{margin:0 0 4px;font-size:26px;letter-spacing:.14em}} .hub h1 b{{color:#ffb608}}
 .hub p{{color:#b2b2b2;max-width:70ch;margin:4px 0 24px}}
 .hub h2{{font-size:12px;letter-spacing:.14em;text-transform:uppercase;color:#9a9a9a;margin:32px 0 12px}}
 .grid{{display:flex;flex-wrap:wrap;gap:24px}}
 .ic{{position:relative;display:block;width:400px;color:inherit;border:1px solid #2a2a2a;border-radius:8px;background:#1a1a1a;overflow:hidden;transition:border-color .12s,transform .12s}}
 .ic:hover,.ic:focus-within{{border-color:#ffb608;transform:translateY(-2px)}}
 .stretched{{color:inherit;text-decoration:none;outline:none}} .stretched::after{{content:"";position:absolute;inset:0}}
 .ic:focus-within{{box-shadow:0 0 0 2px #00aaee}}
 .thumb{{position:relative;overflow:hidden;background:#0c0c0d}}
 .thumb iframe{{border:0;transform-origin:0 0;pointer-events:none;display:block}}
 .cap{{padding:10px 12px;display:flex;flex-direction:column;gap:2px}} .cap span{{color:#9a9a9a;font-size:12px}}
 .sw{{display:flex;flex-wrap:wrap;gap:8px;list-style:none;margin:0;padding:0}}
 .sw li{{width:104px;font-size:11px;color:#b2b2b2}} .sw i{{display:block;height:32px;border-radius:4px;border:1px solid #000;margin-bottom:4px}} .sw b{{display:block;color:#ddd}}
 .spec{{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:12px;font-size:13px;color:#b2b2b2}}
 .spec div{{padding:12px;border:1px solid #2a2a2a;border-radius:6px;background:#1a1a1a}}
 .spec code{{color:#ffb608}}
 .states{{display:flex;gap:16px;flex-wrap:wrap;align-items:center}}
</style>
</head>
<body>
<div class="hub">
<h1>KICK<b> PIANO</b></h1>
<p>Static design mockup of a piano CLAP plugin UI (working title). No audio engine. Click a card to open the variant; all variants are keyboard navigable (Tab, arrows, Esc). Thumbnails are live iframes.</p>
<div class="grid">{items}</div>
<h2>Palette (from Kick theme.xml, extended)</h2>
<ul class="sw">{swh}</ul>
<h2>Type and spacing</h2>
<div class="spec">
 <div><b>Sans</b>: Myriad Pro (Kick theme font) then system UI fonts. Labels 10-11 px caps +0.08em, body 12-14 px, values mono 11-13 px.</div>
 <div><b>Grid</b>: 4 px base; panel padding 8/12, gap 8 (12 at &ge;1100 px). Controls 28-32 px high, knob 40-62 px.</div>
 <div><b>Accent rule</b>: amber = value / active / selected; cyan = bipolar value, MIDI, focus; red only for clip/error.</div>
</div>
<h2>Control states</h2>
<div class="plugin" data-theme="dark" style="--pw:100%;aspect-ratio:auto;display:block;padding:16px;container-type:normal">
<div class="states">
 <span class="lab">Button</span><button class="btn" type="button">Normal</button><button class="btn" type="button" style="border-color:var(--accent);color:var(--accent-txt)">Hover</button><button class="btn on" type="button">Active</button><button class="btn" type="button" style="outline:2px solid var(--info);outline-offset:2px">Focus</button><button class="btn" type="button" disabled>Disabled</button><button class="btn primary" type="button">Primary</button>
 <span class="lab" style="margin-left:16px">Toggle</span>{toggle("Off", False)}{toggle("On", True)}{toggle("Disabled", False, True)}
 <span class="lab" style="margin-left:16px">Knob</span>{knob("Normal", .6, 0, 100)}{knob("Bipolar", .75, -100, 100, "%", 0, bip=True, sign=True)}{knob("Disabled", .4, 0, 100, disabled=True)}
</div></div>
</div>
<script src="ui.js" defer></script>
</body>
</html>
'''


def main():
    files = {
        'main-dark.html': main_view('dark', False),
        'main-light.html': main_view('light', True),
        'compact.html': compact_view(),
        'loading.html': loading_view(),
        'about.html': about_view(),
        'index.html': index_view(),
    }
    for name, html in files.items():
        with open(os.path.join(HERE, name), 'w', encoding='utf-8') as fh:
            fh.write(html)
        print('wrote', name, len(html))


if __name__ == '__main__':
    main()
