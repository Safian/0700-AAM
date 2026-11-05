/* Kick Piano mockup - minimal demo behaviour (tooltips, control states, key press demo).
   No audio. Everything degrades to the static markup if this file does not load. */
(function () {
  'use strict';
  var $ = function (s, r) { return (r || document).querySelector(s); };
  var $$ = function (s, r) { return Array.prototype.slice.call((r || document).querySelectorAll(s)); };
  var clamp = function (v, a, b) { return Math.min(b, Math.max(a, v)); };
  var plugin = $('.plugin');
  if (!plugin) return;

  /* ---------- tooltip (hover + keyboard focus) ---------- */
  var tip = document.createElement('div');
  tip.className = 'tip'; tip.setAttribute('role', 'tooltip'); tip.id = 'tip';
  plugin.appendChild(tip);
  var tipTimer;
  function showTip(el) {
    var t = el.getAttribute('data-tip');
    if (!t) return;
    var parts = t.split('|');
    tip.innerHTML = '';
    tip.appendChild(document.createTextNode(parts[0]));
    if (parts[1]) { var s = document.createElement('small'); s.textContent = parts[1]; tip.appendChild(s); }
    var pr = plugin.getBoundingClientRect(), r = el.getBoundingClientRect();
    tip.style.left = '0px'; tip.style.top = '0px';
    var w = tip.offsetWidth, h = tip.offsetHeight;
    var x = clamp(r.left - pr.left + r.width / 2 - w / 2, 4, pr.width - w - 4);
    var y = r.top - pr.top - h - 8;
    if (y < 4) y = r.bottom - pr.top + 8;
    tip.style.left = x + 'px'; tip.style.top = y + 'px';
    tip.classList.add('show');
  }
  function hideTip() { clearTimeout(tipTimer); tip.classList.remove('show'); }
  plugin.addEventListener('mouseover', function (e) {
    var el = e.target.closest('[data-tip]'); if (!el) return;
    clearTimeout(tipTimer); tipTimer = setTimeout(function () { showTip(el); }, 450);
  });
  plugin.addEventListener('mouseout', function (e) { if (e.target.closest('[data-tip]')) hideTip(); });
  plugin.addEventListener('focusin', function (e) {
    var el = e.target.closest('[data-tip]');
    if (el && e.target.matches(':focus-visible')) showTip(el);
  });
  plugin.addEventListener('focusout', hideTip);
  document.addEventListener('keydown', function (e) { if (e.key === 'Escape') hideTip(); });
  plugin.addEventListener('pointerdown', hideTip);

  /* ---------- value controls: knob, fader (v/h/mini) ---------- */
  function fmt(el, v) {
    var min = +el.dataset.min, max = +el.dataset.max, dec = +(el.dataset.dec || 0);
    var x = min + (max - min) * v;
    var s = x.toFixed(dec);
    if (el.dataset.sign && x > 0) s = '+' + s;
    return s + (el.dataset.unit || '');
  }
  function paint(el, v) {
    el.dataset.v = v;
    var out = el.querySelector('.val');
    var txt = fmt(el, v);
    if (out) out.textContent = txt;
    el.setAttribute('aria-valuenow', (+el.dataset.min + (+el.dataset.max - +el.dataset.min) * v).toFixed(+(el.dataset.dec || 0)));
    el.setAttribute('aria-valuetext', txt);
    if (el.classList.contains('knob')) {
      var arc = el.querySelector('.k-val');
      var bip = el.classList.contains('bip');
      if (bip) { /* bipolar arc: from 12 o'clock */
        var a = (v - 0.5) * 100, from = a >= 0 ? 50 : 50 + a;
        arc.style.strokeDasharray = Math.max(Math.abs(a), 0.01) + ' 200';
        arc.style.strokeDashoffset = -from;
      } else arc.style.strokeDasharray = (v * 100) + ' 200';
      el.querySelector('.ptr').style.setProperty('--rot', (-135 + 270 * v) + 'deg');
    } else if (el.classList.contains('v') || el.classList.contains('mini')) {
      var fill = el.querySelector('.f-fill'), th = el.querySelector('.f-thumb');
      if (el.classList.contains('bip')) {
        fill.style.bottom = Math.min(v, .5) * 100 + '%'; fill.style.height = Math.abs(v - .5) * 100 + '%';
      } else fill.style.height = v * 100 + '%';
      th.style.bottom = v * 100 + '%';
    } else if (el.classList.contains('h')) {
      el.style.setProperty('--w', v * 100 + '%');
    }
  }
  function ctlInit(el) {
    var v = +el.dataset.v;
    paint(el, v);
    if (el.getAttribute('aria-disabled') === 'true') return;
    var kind = el.classList.contains('knob') ? 'knob' : el.classList.contains('h') ? 'h' : 'v';
    var track = function () { return el.querySelector('.f-track'); };
    el.addEventListener('pointerdown', function (e) {
      if (e.button !== 0) return;
      el.setPointerCapture(e.pointerId); el.classList.add('drag'); el.focus({ preventScroll: true });
      var y0 = e.clientY, v0 = +el.dataset.v;
      function setFromPointer(ev) {
        if (kind === 'knob') {
          paint(el, clamp(v0 + (y0 - ev.clientY) / (ev.shiftKey ? 600 : 160), 0, 1));
        } else {
          var r = track().getBoundingClientRect();
          paint(el, clamp(kind === 'h' ? (ev.clientX - r.left) / r.width : 1 - (ev.clientY - r.top) / r.height, 0, 1));
        }
      }
      if (kind !== 'knob') setFromPointer(e);
      function mv(ev) { setFromPointer(ev); }
      function up() { el.classList.remove('drag'); el.removeEventListener('pointermove', mv); el.removeEventListener('pointerup', up); el.removeEventListener('pointercancel', up); }
      el.addEventListener('pointermove', mv); el.addEventListener('pointerup', up); el.addEventListener('pointercancel', up);
    });
    el.addEventListener('dblclick', function () { paint(el, +el.dataset.def); });
    el.addEventListener('wheel', function (e) { e.preventDefault(); paint(el, clamp(+el.dataset.v - Math.sign(e.deltaY) * 0.02, 0, 1)); }, { passive: false });
    el.addEventListener('keydown', function (e) {
      var s = e.shiftKey ? 0.1 : 0.01, v = +el.dataset.v, n = v;
      if (e.key === 'ArrowUp' || e.key === 'ArrowRight') n = v + s;
      else if (e.key === 'ArrowDown' || e.key === 'ArrowLeft') n = v - s;
      else if (e.key === 'PageUp') n = v + 0.1;
      else if (e.key === 'PageDown') n = v - 0.1;
      else if (e.key === 'Home') n = 0; else if (e.key === 'End') n = 1;
      else if (e.key === 'Enter') n = +el.dataset.def; else return;
      e.preventDefault(); paint(el, clamp(n, 0, 1));
    });
  }
  $$('.ctl[data-v]').forEach(ctlInit);

  /* ---------- toggles, radio cards, tabs, preset stepping ---------- */
  var sustainSync = [];
  $$('.plugin [aria-pressed]').forEach(function (b) {
    if (b.getAttribute('aria-disabled') === 'true' || b.hasAttribute('data-nostate')) return;
    b.addEventListener('click', function () {
      var on = b.getAttribute('aria-pressed') !== 'true';
      b.setAttribute('aria-pressed', on);
      if (b.dataset.sync === 'sustain') setSustain(on);
    });
  });
  $$('[role="radiogroup"]').forEach(function (g) {
    var items = $$('[role="radio"]', g);
    function sel(it) {
      if (it.getAttribute('aria-disabled') === 'true') return;
      items.forEach(function (o) { o.setAttribute('aria-checked', o === it); o.tabIndex = o === it ? 0 : -1; });
    }
    items.forEach(function (it) {
      it.addEventListener('click', function () { sel(it); });
      it.addEventListener('keydown', function (e) {
        var i = items.indexOf(it), d = (e.key === 'ArrowDown' || e.key === 'ArrowRight') ? 1 : (e.key === 'ArrowUp' || e.key === 'ArrowLeft') ? -1 : 0;
        if (!d) return; e.preventDefault();
        for (var k = 1; k <= items.length; k++) {
          var n = items[(i + d * k + items.length * 4) % items.length];
          if (n.getAttribute('aria-disabled') !== 'true') { sel(n); n.focus(); break; }
        }
      });
    });
  });
  var tabs = $$('[role="tab"]');
  function selTab(t) {
    tabs.forEach(function (o) {
      var on = o === t; o.setAttribute('aria-selected', on); o.tabIndex = on ? 0 : -1;
      var p = document.getElementById(o.getAttribute('aria-controls')); if (p) p.hidden = !on;
    });
  }
  tabs.forEach(function (t, i) {
    t.addEventListener('click', function () { selTab(t); });
    t.addEventListener('keydown', function (e) {
      var d = e.key === 'ArrowRight' ? 1 : e.key === 'ArrowLeft' ? -1 : 0;
      if (!d) return; e.preventDefault();
      var n = tabs[(i + d + tabs.length) % tabs.length]; selTab(n); n.focus();
    });
  });
  var presets = [['Concert Grand', 'Warm Studio Close'], ['Concert Grand', 'Bright Concert Hall'], ['Upright', 'Felt Intimate'], ['Upright', 'Jazz Club'], ['Electric (example)', 'Vintage Tines']];
  var pi = 0;
  function setPreset(i) {
    pi = (i + presets.length) % presets.length;
    var c = $('.preset-name .cat'), n = $('.preset-name .nm');
    if (c && n) { c.textContent = presets[pi][0]; n.textContent = presets[pi][1]; }
  }
  var pv = $('[data-act="prev"]'), nx = $('[data-act="next"]');
  if (pv) pv.addEventListener('click', function () { setPreset(pi - 1); });
  if (nx) nx.addEventListener('click', function () { setPreset(pi + 1); });
  $$('[data-step]').forEach(function (b) {
    b.addEventListener('click', function () {
      var out = document.getElementById(b.dataset.target); if (!out) return;
      var v = Math.round((parseFloat(out.textContent) + parseFloat(b.dataset.step)) * 10) / 10;
      out.textContent = clamp(v, 415, 466).toFixed(1);
      var ct = $('[data-cents]'); if (ct) { var c = 1200 * Math.log(v / 440) / Math.LN2; ct.textContent = (c >= 0 ? '+' : '') + c.toFixed(1) + ' ct'; }
    });
  });

  /* ---------- velocity curve editor ---------- */
  var updateLive = function () {};
  var cv = $('.curve svg');
  if (cv) {
    var hnd = $('.hnd', cv), path = $('.cv', cv), fill = $('.cvfill', cv), hy = +cv.dataset.hy;
    var live = $('.live', cv);
    var setCurve = function (y) {
      hy = clamp(y, 8, 92); cv.dataset.hy = hy;
      var cy = 2 * hy - 50;
      path.setAttribute('d', 'M0,100 Q50,' + cy + ' 100,0');
      fill.setAttribute('d', 'M0,100 Q50,' + cy + ' 100,0 L100,100 Z');
      hnd.setAttribute('cy', hy);
      cv.setAttribute('aria-valuenow', Math.round(100 - hy));
      updateLive(liveIn);
    };
    updateLive = function (inPct) {
      liveIn = inPct;
      if (!live) return;
      var t = inPct / 100, cy = 2 * hy - 50, y = (1 - t) * (1 - t) * 100 + 2 * (1 - t) * t * cy;
      live.setAttribute('x1', inPct); live.setAttribute('x2', inPct); live.setAttribute('y1', 100); live.setAttribute('y2', y);
    };
    var liveIn = live ? +live.dataset.in : 0;
    setCurve(hy);
    var drag = function (e) { var r = cv.getBoundingClientRect(); setCurve((e.clientY - r.top) / r.height * 100); };
    cv.addEventListener('pointerdown', function (e) { cv.setPointerCapture(e.pointerId); drag(e); cv.onpointermove = drag; cv.onpointerup = function () { cv.onpointermove = null; }; });
    cv.addEventListener('keydown', function (e) {
      var s = e.shiftKey ? 8 : 2;
      if (e.key === 'ArrowUp') setCurve(hy - s); else if (e.key === 'ArrowDown') setCurve(hy + s); else if (e.key === 'Home' || e.key === 'Enter') setCurve(50); else return;
      e.preventDefault();
    });
    $$('[data-curve]').forEach(function (b) { b.addEventListener('click', function () { setCurve(+b.dataset.curve); }); });
  }

  /* ---------- keyboard + sustain + MIDI activity demo ---------- */
  var keys = $('.keys');
  var NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
  var sustainOn = $('[data-sync="sustain"]') ? $('[data-sync="sustain"]').getAttribute('aria-pressed') === 'true' : false;
  var ind = $('.pedalind');
  function setSustain(on) {
    sustainOn = on;
    $$('[data-sync="sustain"]').forEach(function (b) { b.setAttribute('aria-pressed', on); });
    if (ind) { ind.dataset.on = on; $('.pbar i', ind).style.setProperty('--p', on ? '100%' : '0%'); $('.val', ind).textContent = on ? '100%' : '0%'; }
    if (!on && keys) $$('.k.sus', keys).forEach(function (k) { k.classList.remove('sus'); });
    midiMsg('CC64 Sustain ' + (on ? 'On (127)' : 'Off (0)'));
  }
  var led = $('.midi .led'), msg = $('.midi .msg'), acts = $$('.midi .acts i'), ledT;
  function midiMsg(t) {
    if (!msg) return;
    msg.textContent = 'Ch1 · ' + t;
    if (led) { led.classList.add('on'); clearTimeout(ledT); ledT = setTimeout(function () { led.classList.remove('on'); }, 160); }
    if (acts.length) { var i = Math.floor(Math.random() * acts.length); acts[i].style.setProperty('--h', (4 + Math.random() * 10) + 'px'); }
  }
  function band(vel) { return vel < 32 ? 1 : vel < 64 ? 2 : vel < 90 ? 3 : vel < 112 ? 4 : 5; }
  function noteName(n) { return NAMES[n % 12] + (Math.floor(n / 12) - 1); }
  function press(k, vel) {
    k.classList.remove('v1', 'v2', 'v3', 'v4', 'v5', 'sus');
    k.classList.add('on', 'v' + band(vel));
    midiMsg('Note On ' + noteName(+k.dataset.n) + ' · vel ' + vel);
    updateLive(vel / 127 * 100);
  }
  function release(k) {
    k.classList.remove('on');
    if (sustainOn) k.classList.add('sus'); else k.classList.remove('v1', 'v2', 'v3', 'v4', 'v5');
    midiMsg('Note Off ' + noteName(+k.dataset.n));
  }
  if (keys) {
    var held = {};
    keys.addEventListener('pointerdown', function (e) {
      var k = e.target.closest('.k'); if (!k) return;
      var r = k.getBoundingClientRect();
      var vel = Math.round(clamp(24 + (e.clientY - r.top) / r.height * 110, 1, 127));
      held[e.pointerId] = k; press(k, vel); keys.setPointerCapture(e.pointerId);
      $$('.k.cur', keys).forEach(function (c) { c.classList.remove('cur'); });
    });
    var up = function (e) { var k = held[e.pointerId]; if (k) { release(k); delete held[e.pointerId]; } };
    keys.addEventListener('pointerup', up); keys.addEventListener('pointercancel', up);
    var cur = $('.k.mc', keys);
    keys.addEventListener('focus', function () { if (keys.matches(':focus-visible') && cur) cur.classList.add('cur'); });
    keys.addEventListener('blur', function () { $$('.k.cur', keys).forEach(function (c) { c.classList.remove('cur'); }); });
    keys.addEventListener('keydown', function (e) {
      if (!cur) return;
      var all = $$('.k', keys).sort(function (a, b) { return +a.dataset.n - +b.dataset.n; });
      var i = all.indexOf(cur), d = e.key === 'ArrowRight' ? 1 : e.key === 'ArrowLeft' ? -1 : e.key === 'PageUp' ? 12 : e.key === 'PageDown' ? -12 : 0;
      if (d) { e.preventDefault(); cur.classList.remove('cur'); cur = all[clamp(i + d, 0, all.length - 1)]; cur.classList.add('cur'); }
      else if ((e.key === ' ' || e.key === 'Enter') && !e.repeat) { e.preventDefault(); press(cur, 84); }
    });
    keys.addEventListener('keyup', function (e) { if ((e.key === ' ' || e.key === 'Enter') && cur) { release(cur); } });
  }

  /* ---------- demo-bar helpers (size / theme / download state) ---------- */
  $$('[data-size]').forEach(function (b) {
    b.addEventListener('click', function () {
      plugin.style.setProperty('--pw', b.dataset.size + 'px');
      if (b.dataset.h) plugin.style.setProperty('--ph', b.dataset.h + 'px');
      $$('[data-size]').forEach(function (o) { o.setAttribute('aria-pressed', o === b); });
    });
  });
  $$('[data-theme-set]').forEach(function (b) {
    b.addEventListener('click', function () {
      plugin.dataset.theme = b.dataset.themeSet;
      $$('[data-theme-set]').forEach(function (o) { o.setAttribute('aria-pressed', o === b); });
    });
  });
  var dl = $('.dl');
  if (dl) $$('[data-dlstate]').forEach(function (b) {
    b.addEventListener('click', function () {
      dl.dataset.state = b.dataset.dlstate;
      $$('[data-dlstate]').forEach(function (o) { o.setAttribute('aria-pressed', o === b); });
    });
  });
  if (dl) $$('.dl [data-setstate]').forEach(function (b) {
    b.addEventListener('click', function () { dl.dataset.state = b.dataset.setstate; $$('[data-dlstate]').forEach(function (o) { o.setAttribute('aria-pressed', o.dataset.dlstate === dl.dataset.state); }); });
  });
  document.addEventListener('keydown', function (e) {
    if (e.key === 'Escape' && document.body.dataset.back) location.href = document.body.dataset.back;
  });
})();
