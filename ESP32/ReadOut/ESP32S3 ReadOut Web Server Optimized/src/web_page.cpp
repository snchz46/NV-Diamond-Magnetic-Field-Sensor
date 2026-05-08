/**
 * web_page.cpp — Embedded single-page plotter UI (HTML/CSS/JS in PROGMEM)
 *
 * The entire front-end is stored as a raw string literal in program flash so
 * it does not consume the ESP32's limited data RAM. It is served verbatim by
 * web_ui.cpp via server.send_P().
 *
 * Page structure
 * ──────────────
 *   #topbar          — Title + WebSocket connection indicator
 *   #metrics         — Live numeric readouts (raw, filtered, delta, RPM, dip)
 *                      and a mode selector (filt / raw / Δ)
 *   #settingsPanel   — Collapsible controls: Freeze, Clear, Export CSV,
 *                      Recalibrate, Autoscale Y, IIR α slider, Avg slider,
 *                      Threshold slider + numeric input, Apply button
 *   #plotWrap/#plot  — HiDPI <canvas> for the scrolling signal waveform
 *   #statusbar       — Current view statistics (points shown, window length)
 *   #instructions    — Inline usage guide and slider-effect explanations
 *
 * JavaScript architecture (single IIFE, no external libraries)
 * ─────────────────────────────────────────────────────────────
 *   Ring buffer  — 5000-point Float32/Float64/Uint8 typed arrays for t/v/dip.
 *                  Oldest entries are overwritten once full.
 *   View state   — viewSpan (visible points) + viewEnd (rightmost index).
 *                  The render loop uses these to slice the ring buffer.
 *   WebSocket    — Connects to ws://<host>:81/. Incoming JSON frames push to
 *                  the ring buffer; outgoing frames carry config changes.
 *   Input events — Mouse (drag/wheel) and touch (pan/pinch) both manipulate
 *                  viewSpan and viewEnd directly for zoom and pan.
 *   draw()       — requestAnimationFrame loop; re-renders the canvas every
 *                  frame. Handles autoscale Y, grid lines, signal line, dip
 *                  marker lines, and the frozen-view overlay.
 */

#include "web_page.h"

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>NV Quenching Plotter</title>
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<style>
/* ── Reset ─────────────────────────────────────────────────────────────── */
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{margin:0;padding:0;background:#111;color:#ddd;
          font-family:Arial,sans-serif;overscroll-behavior:none}

/* ── Top bar ────────────────────────────────────────────────────────────── */
#topbar{display:flex;align-items:center;justify-content:space-between;
        padding:8px 12px;background:#1b1b1b;border-bottom:1px solid #333}
#topbar h2{margin:0;font-size:16px}
.ok{color:#7CFC00}.bad{color:#ff5f5f}

/* ── Metric strip ───────────────────────────────────────────────────────── */
#metrics{display:grid;grid-template-columns:repeat(6,1fr);
         background:#161616;border-bottom:1px solid #333}
.metric{padding:5px 3px;text-align:center;border-right:1px solid #282828}
.metric:last-child{border-right:none}
.mlabel{font-size:9px;opacity:.55;text-transform:uppercase;letter-spacing:.4px}
.mval{font-size:13px;font-weight:bold;margin-top:1px}
#dipVal.lit{color:#ff5f5f}

/* ── Settings toggle ────────────────────────────────────────────────────── */
#settingsToggle{width:100%;padding:9px 12px;background:#1b1b1b;
                border:none;border-bottom:1px solid #333;color:#ddd;
                font-size:13px;text-align:left;cursor:pointer;
                display:flex;align-items:center;justify-content:space-between}
#settingsToggle span{opacity:.55;font-size:11px}
/* starts OPEN — JS will close on toggle */
#settingsPanel{display:block;background:#1a1a1a;border-bottom:1px solid #333;padding:10px 12px}
#settingsPanel.closed{display:none}

/* ── Controls ───────────────────────────────────────────────────────────── */
.ctrl-row{display:flex;flex-wrap:wrap;gap:8px;align-items:center;margin-bottom:10px}
.ctrl-row:last-child{margin-bottom:0}

button,select{min-height:40px;background:#222;color:#ddd;border:1px solid #444;
              border-radius:8px;padding:6px 12px;font-size:13px;
              cursor:pointer;touch-action:manipulation}
button:active{background:#333}

input[type=checkbox]{width:22px;height:22px;cursor:pointer;accent-color:#5fd7ff}

input[type=range]{-webkit-appearance:none;appearance:none;height:6px;
                  border-radius:3px;background:#333;outline:none;border:none;
                  padding:0;width:100%;cursor:pointer;touch-action:manipulation}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;appearance:none;
  width:26px;height:26px;border-radius:50%;background:#5fd7ff;cursor:pointer}
input[type=range]::-moz-range-thumb{width:26px;height:26px;border-radius:50%;
  background:#5fd7ff;cursor:pointer;border:none}

.slider-wrap{flex:1;min-width:130px}
.slider-label{font-size:12px;opacity:.8;margin-bottom:5px}
.slider-hint{font-size:10px;color:#666;margin-top:3px}

#recalBtn{background:#443010;border-color:#a06010;color:#ffcc66}
#recalBtn.flash{background:#7CFC00;color:#000;border-color:#7CFC00}

/* ── Canvas ─────────────────────────────────────────────────────────────── */
#plotWrap{width:100%;touch-action:none;position:relative}
#plot{display:block;width:100%;background:#000;border-bottom:1px solid #222}

/* ── Status bar ─────────────────────────────────────────────────────────── */
#statusbar{padding:4px 10px;font-size:11px;color:#555;background:#111;
           border-bottom:1px solid #1a1a1a}

/* ── Instructions section ───────────────────────────────────────────────── */
#instructions{padding:14px 14px 20px;background:#111}

.instr-title{font-size:14px;font-weight:bold;color:#aaa;
             margin:0 0 10px;display:flex;align-items:center;gap:6px}

.instr-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(260px,1fr));gap:10px}

.instr-card{background:#1a1a1a;border:1px solid #2a2a2a;border-radius:10px;padding:12px}
.instr-card-title{font-size:12px;font-weight:bold;color:#5fd7ff;margin:0 0 8px;
                  display:flex;align-items:center;gap:5px}
.instr-card-title .icon{font-size:16px}
.instr-item{display:flex;gap:8px;margin-bottom:7px;align-items:flex-start}
.instr-item:last-child{margin-bottom:0}
.instr-key{background:#222;border:1px solid #444;border-radius:5px;
           font-size:10px;color:#ccc;padding:2px 6px;white-space:nowrap;
           flex-shrink:0;margin-top:1px;line-height:1.6}
.instr-desc{font-size:12px;color:#aaa;line-height:1.5}
.instr-desc b{color:#ddd}

.slider-explainer{margin-top:10px}
.slider-row{display:flex;gap:10px;align-items:flex-start;margin-bottom:9px;
            background:#1a1a1a;border:1px solid #2a2a2a;border-radius:10px;padding:10px}
.slider-row:last-child{margin-bottom:0}
.s-icon{font-size:20px;flex-shrink:0;margin-top:1px}
.s-body{}
.s-name{font-size:12px;font-weight:bold;color:#5fd7ff;margin-bottom:2px}
.s-text{font-size:12px;color:#aaa;line-height:1.5}
.s-effect{font-size:11px;color:#777;margin-top:3px;font-style:italic}
</style>
</head>
<body>

<!-- ── Top bar ── -->
<div id="topbar">
  <h2>&#x2022; NV Quenching</h2>
  <div>WS:&nbsp;<b id="ws" class="bad">offline</b></div>
</div>

<!-- ── Metric strip: live numeric readouts for raw, filtered, delta, RPM, dip ── -->
<div id="metrics">
  <div class="metric"><div class="mlabel">raw</div>  <div class="mval" id="rawVal">-</div></div>
  <div class="metric"><div class="mlabel">filt</div> <div class="mval" id="filtVal">-</div></div>
  <div class="metric"><div class="mlabel">delta</div><div class="mval" id="deltaVal">-</div></div>
  <div class="metric"><div class="mlabel">rpm</div>  <div class="mval" id="rpmVal">-</div></div>
  <div class="metric"><div class="mlabel">dip</div>  <div class="mval" id="dipVal">-</div></div>
  <!-- Mode selector determines which signal channel is plotted on the canvas -->
  <div class="metric"><div class="mlabel">mode</div>
    <select id="mode" style="min-height:unset;font-size:11px;padding:2px;
                             border-radius:4px;width:100%;border-color:#333">
      <option value="filtered">filt</option>
      <option value="raw">raw</option>
      <option value="delta">&#x394;</option>
    </select>
  </div>
</div>

<!-- ── Settings panel (starts open; JS toggles .closed class) ── -->
<button id="settingsToggle">&#x2699; Settings &amp; Controls <span id="toggleArr">&#x25B2;</span></button>
<div id="settingsPanel">

  <div class="ctrl-row">
    <button id="freeze">&#x23F8; Freeze</button>
    <button id="clear">&#x1F5D1; Clear</button>
    <a href="/export.csv" style="text-decoration:none"><button>&#x2B07; CSV</button></a>
    <button id="recalBtn">&#x21BA; Recalibrate</button>
    <label style="display:flex;align-items:center;gap:7px;font-size:13px;cursor:pointer">
      <input id="autoscale" type="checkbox" checked>Autoscale Y
    </label>
  </div>

  <div class="ctrl-row">
    <div class="slider-wrap">
      <div class="slider-label">IIR &#x3B1;: <b id="aVal">0.05</b></div>
      <input id="alpha" type="range" min="0.01" max="0.30" step="0.01" value="0.05">
      <div class="slider-hint">Smoothing strength</div>
    </div>
    <div class="slider-wrap">
      <div class="slider-label">Avg samples: <b id="avgVal">10</b></div>
      <input id="avg" type="range" min="1" max="16" step="1" value="10">
      <div class="slider-hint">Noise averaging</div>
    </div>
    <div class="slider-wrap">
      <div class="slider-label">Threshold: <b id="thrVal">-75</b>
        <!-- Numeric input allows precise values outside the slider's -150..0 range -->
        <input id="thrNum" type="number" min="-150" max="0" step="1" value="-75"
               style="width:70px;margin-left:6px;background:#1a1a1a;color:#ddd;
                      border:1px solid #444;border-radius:5px;padding:2px 5px;font-size:12px">
      </div>
      <input id="thr" type="range" min="-150" max="0" step="1" value="-75">
      <div class="slider-hint">Dip detection level (ADC counts, 1 ct &#x2248; 7.8 &#xB5;V)</div>
    </div>
  </div>

  <div class="ctrl-row">
    <button id="apply" style="flex:1;background:#1a3a1a;border-color:#2a6a2a;color:#7CFC00">
      &#x2713; Apply Settings
    </button>
  </div>
</div>

<!-- ── Plot canvas ── -->
<div id="plotWrap"><canvas id="plot"></canvas></div>
<div id="statusbar">Waiting for data&#x2026;</div>

<!-- ── Inline usage guide ── -->
<div id="instructions">

  <div class="instr-title">&#x1F4D6; How to use the plotter</div>

  <div class="instr-grid">

    <div class="instr-card">
      <div class="instr-card-title"><span class="icon">&#x1F4F1;</span> On a phone or tablet</div>
      <div class="instr-item">
        <span class="instr-key">1 finger drag</span>
        <span class="instr-desc"><b>Pan left / right</b> to scroll through older data</span>
      </div>
      <div class="instr-item">
        <span class="instr-key">Pinch in/out</span>
        <span class="instr-desc"><b>Zoom in / out</b> on the time axis &#x2014; spread fingers to see fewer points, pinch to see more history</span>
      </div>
      <div class="instr-item">
        <span class="instr-key">Pinch vertically</span>
        <span class="instr-desc"><b>Zoom the Y axis</b> (only works when Autoscale Y is turned off)</span>
      </div>
    </div>

    <div class="instr-card">
      <div class="instr-card-title"><span class="icon">&#x1F5A5;</span> On a desktop / laptop</div>
      <div class="instr-item">
        <span class="instr-key">Click &amp; drag</span>
        <span class="instr-desc"><b>Pan left / right</b> through the signal history</span>
      </div>
      <div class="instr-item">
        <span class="instr-key">Scroll wheel</span>
        <span class="instr-desc"><b>Zoom the time axis</b> &#x2014; scroll up to zoom in, down to zoom out</span>
      </div>
      <div class="instr-item">
        <span class="instr-key">Shift + scroll</span>
        <span class="instr-desc"><b>Zoom the Y axis</b> (only when Autoscale Y is off)</span>
      </div>
    </div>

    <div class="instr-card">
      <div class="instr-card-title"><span class="icon">&#x23F8;</span> Freeze &amp; Clear</div>
      <div class="instr-item">
        <span class="instr-key">Freeze</span>
        <span class="instr-desc">Stops the view from scrolling with new data &#x2014; you can explore old data while the sensor keeps recording. Tap again to resume live view.</span>
      </div>
      <div class="instr-item">
        <span class="instr-key">Clear</span>
        <span class="instr-desc">Erases all data on screen. The sensor keeps running &#x2014; a fresh trace starts immediately.</span>
      </div>
      <div class="instr-item">
        <span class="instr-key">&#x21BA; Recalibrate</span>
        <span class="instr-desc">Re-measures the zero baseline (takes ~0.3 s). Use this if the delta signal drifts far from zero over time.</span>
      </div>
    </div>

    <div class="instr-card">
      <div class="instr-card-title"><span class="icon">&#x1F4CA;</span> Reading the graph</div>
      <div class="instr-item">
        <span class="instr-key" style="background:#0a2a3a;border-color:#5fd7ff;color:#5fd7ff">&#x2015; blue line</span>
        <span class="instr-desc">The selected signal (filtered by default). Smooth bumps mean the signal is changing slowly.</span>
      </div>
      <div class="instr-item">
        <span class="instr-key" style="background:#3a0a0a;border-color:#ff5f5f;color:#ff5f5f">| red line</span>
        <span class="instr-desc">A detected <b>dip event</b> &#x2014; the signal dropped below the threshold. Each red line = one quench dip.</span>
      </div>
      <div class="instr-item">
        <span class="instr-key">Mode</span>
        <span class="instr-desc">Switch between <b>filt</b> (smoothed), <b>raw</b> (direct ADC), or <b>&#x394;</b> (change from baseline) using the mode selector in the metric bar above the graph.</span>
      </div>
    </div>

  </div><!-- /instr-grid -->

  <!-- Slider explainer cards -->
  <div class="instr-title" style="margin-top:16px">&#x1F39B; What the sliders do</div>
  <div class="slider-explainer">

    <div class="slider-row">
      <div class="s-icon">&#x1F30A;</div>
      <div class="s-body">
        <div class="s-name">IIR &#x3B1; &#x2014; Smoothing strength</div>
        <div class="s-text">Controls how quickly the filtered signal responds to changes.
          A <b>small &#x3B1; (e.g. 0.05)</b> produces a very smooth curve that reacts slowly &#x2014; good for seeing overall trends.
          A <b>large &#x3B1; (e.g. 0.25)</b> follows the raw signal closely but keeps more noise &#x2014; good for fast events.</div>
        <div class="s-effect">&#x2197; Higher &#x3B1; = faster but noisier &nbsp;|&nbsp; &#x2198; Lower &#x3B1; = smoother but slower</div>
      </div>
    </div>

    <div class="slider-row">
      <div class="s-icon">&#x1F9EE;</div>
      <div class="s-body">
        <div class="s-name">Avg samples &#x2014; Hardware averaging</div>
        <div class="s-text">The sensor reads the ADC this many times and averages the results <b>before</b> the filter sees the value.
          <b>More samples</b> = less electrical noise, but slower acquisition (the loop takes longer).
          <b>Fewer samples</b> = faster loop, but the raw trace will be noisier.</div>
        <div class="s-effect">&#x2197; More samples = quieter signal, slower loop &nbsp;|&nbsp; &#x2198; Fewer = faster, noisier</div>
      </div>
    </div>

    <div class="slider-row">
      <div class="s-icon">&#x26A0;</div>
      <div class="s-body">
        <div class="s-name">Threshold &#x2014; Dip detection level</div>
        <div class="s-text">When the signal drops <b>below this value</b> (relative to baseline), a red dip marker appears and the RPM counter ticks.
          The value is in <b>raw ADC counts</b> — with the current gain setting, 1 count &#x2248; 7.8 µV.
          To find a good threshold: watch the <b>delta</b> value (&#x394; in the metric bar) during a known dip. Set the threshold to about <b>half</b> of that dip depth.
          If you see too many false red lines, make the threshold more negative (e.g. &#x2212;100 instead of &#x2212;15). You can type a precise number directly into the box next to the slider.</div>
        <div class="s-effect">&#x2198; More negative = harder to trigger (fewer false dips) &nbsp;|&nbsp; &#x2197; Less negative = more sensitive</div>
      </div>
    </div>

  </div><!-- /slider-explainer -->
</div><!-- /instructions -->

<script>
(() => {
'use strict';

// ── DOM refs ──────────────────────────────────────────────────────────────
const wsEl      = document.getElementById('ws');
const rawVal    = document.getElementById('rawVal');
const filtVal   = document.getElementById('filtVal');
const deltaVal  = document.getElementById('deltaVal');
const rpmVal    = document.getElementById('rpmVal');
const dipVal    = document.getElementById('dipVal');
const modeEl    = document.getElementById('mode');
const autoscale = document.getElementById('autoscale');
const freezeBtn = document.getElementById('freeze');
const clearBtn  = document.getElementById('clear');
const recalBtn  = document.getElementById('recalBtn');
const alpha     = document.getElementById('alpha');
const avg       = document.getElementById('avg');
const thr       = document.getElementById('thr');
const aValEl    = document.getElementById('aVal');
const avgValEl  = document.getElementById('avgVal');
const thrValEl  = document.getElementById('thrVal');
const applyBtn  = document.getElementById('apply');
const canvas    = document.getElementById('plot');
const ctx       = canvas.getContext('2d');
const statusbar = document.getElementById('statusbar');
const settBtn   = document.getElementById('settingsToggle');
const settPanel = document.getElementById('settingsPanel');
const toggleArr = document.getElementById('toggleArr');

// ── Collapsible settings panel ────────────────────────────────────────────
// The panel is open by default (display:block in CSS).
// Clicking the toggle button adds/removes the .closed class.
settBtn.addEventListener('click', () => {
  const closing = !settPanel.classList.contains('closed');
  settPanel.classList.toggle('closed', closing);
  // Swap arrow glyph to indicate current collapsed/expanded state
  toggleArr.innerHTML = closing ? '&#x25BC;' : '&#x25B2;';
});

// ── HiDPI responsive canvas ───────────────────────────────────────────────
// Multiplying canvas pixel dimensions by devicePixelRatio prevents blurry
// rendering on retina / high-DPI screens, then the CSS size is kept at the
// logical pixel size so layout is unaffected.
let dpr = 1, cssW = 0, cssH = 0;

function resizeCanvas() {
  dpr  = window.devicePixelRatio || 1;
  cssW = canvas.parentElement.clientWidth;
  // Height scales with width up to a maximum of 520 px
  cssH = Math.min(520, Math.max(220, Math.round(cssW * 0.52)));
  canvas.style.height = cssH + 'px';
  canvas.width  = Math.round(cssW * dpr);
  canvas.height = Math.round(cssH * dpr);
  // Scale the drawing context so all draw calls use CSS pixel units
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
}
resizeCanvas();
window.addEventListener('resize', resizeCanvas);

// ── Ring buffer ───────────────────────────────────────────────────────────
// Three parallel typed arrays store time, signal value, and dip flag for
// each sample. Using typed arrays instead of object arrays keeps memory use
// low and iteration fast.
const N = 5000;  // Maximum number of samples retained in the browser
let tbuf = new Float64Array(N), vbuf = new Float32Array(N), dips = new Uint8Array(N);
let writeIdx = 0, totalCount = 0;

/** logicalLen() — Number of valid samples currently in the buffer (0–N). */
function logicalLen() { return Math.min(totalCount, N); }

/**
 * getPoint(li) — Retrieve sample at logical index li (0 = oldest).
 *
 * When the buffer has not yet wrapped, logical and physical indices are the
 * same. Once wrapped, the oldest physical entry is at writeIdx and we must
 * offset accordingly.
 */
function getPoint(li) {
  if (totalCount < N) return { t: tbuf[li], v: vbuf[li], d: dips[li] };
  const r = (writeIdx + li) % N;
  return { t: tbuf[r], v: vbuf[r], d: dips[r] };
}

/** pushSample() — Append a new sample, overwriting the oldest when full. */
function pushSample(t, v, d) {
  tbuf[writeIdx] = t; vbuf[writeIdx] = v; dips[writeIdx] = d ? 1 : 0;
  writeIdx = (writeIdx + 1) % N; totalCount++;
}

// ── View state ────────────────────────────────────────────────────────────
// viewSpan — how many logical samples are shown across the full canvas width.
// viewEnd  — the logical index of the rightmost visible sample.
// yCenter / ySpan — Y axis centre and total extent (used when autoscale is off).
let frozen = false;
let viewSpan = 2500, viewEnd = 0, yCenter = 0, ySpan = 300;

/**
 * clampView() — Enforce viewSpan and viewEnd bounds against actual buffer size.
 * Call this after any pan or zoom operation before drawing.
 */
function clampView() {
  const len = logicalLen(); if (len < 10) return;
  viewSpan = Math.max(50, Math.min(viewSpan, len));
  viewEnd  = Math.max(viewSpan, Math.min(viewEnd, len));
}

// ── Threshold slider ↔ number input sync ─────────────────────────────────
// The number input lets users type values outside the slider's -150..0 range.
// Both controls are kept in sync at all times.
const thrNum = document.getElementById('thrNum');
thrNum.addEventListener('input', () => {
  const v = Math.max(-5000, Math.min(0, parseInt(thrNum.value) || 0));
  thr.value = v; refreshLabels();
});

/** refreshLabels() — Update all slider label text to reflect current values. */
function refreshLabels() {
  aValEl.textContent   = parseFloat(alpha.value).toFixed(2);
  avgValEl.textContent = avg.value;
  thrValEl.textContent = thr.value;
  thrNum.value         = thr.value;
}
refreshLabels();
[alpha, avg, thr].forEach(e => e.addEventListener('input', refreshLabels));

// ── Freeze / Clear buttons ────────────────────────────────────────────────
freezeBtn.addEventListener('click', () => {
  frozen = !frozen;
  freezeBtn.innerHTML = frozen ? '&#x25B6; Unfreeze' : '&#x23F8; Freeze';
  // When unfreezing, snap the view to the latest data
  if (!frozen) viewEnd = logicalLen();
});

clearBtn.addEventListener('click', () => {
  // Reinitialise all buffers and reset view to defaults
  tbuf = new Float64Array(N); vbuf = new Float32Array(N); dips = new Uint8Array(N);
  writeIdx = 0; totalCount = 0; viewEnd = 0; viewSpan = 2500; yCenter = 0; ySpan = 300;
});

/**
 * flashRecal() — Temporarily change the Recalibrate button to a success/error
 * state then revert after 1.5 s.
 */
function flashRecal(msg) {
  recalBtn.innerHTML = msg; recalBtn.classList.add('flash');
  setTimeout(() => {
    recalBtn.innerHTML = '&#x21BA; Recalibrate';
    recalBtn.classList.remove('flash');
    recalBtn.disabled = false;
  }, 1500);
}

// Recalibrate button: POST to /recal via fetch (non-blocking for the browser)
recalBtn.addEventListener('click', () => {
  recalBtn.disabled = true; recalBtn.textContent = 'Recalibrating\u2026';
  fetch('/recal')
    .then(r => { if (!r.ok) throw new Error(); flashRecal('&#x2713; Done'); })
    .catch(() => { recalBtn.textContent = 'Failed'; recalBtn.disabled = false; });
});

// ── WebSocket connection ──────────────────────────────────────────────────
// Port 81 is the WebSocket server; port 80 is HTTP. Both run on the same ESP32.
const ws = new WebSocket('ws://' + location.hostname + ':81/');
ws.addEventListener('open',  () => { wsEl.textContent = 'online';  wsEl.className = 'ok';  sendCfg(); });
ws.addEventListener('close', () => { wsEl.textContent = 'offline'; wsEl.className = 'bad'; });

/** sendCfg() — Send the current slider values to the ESP32 as a config frame. */
function sendCfg() {
  if (ws.readyState !== 1) return;
  ws.send(JSON.stringify({ type:'cfg', alpha:parseFloat(alpha.value),
                           avg:parseInt(avg.value), thr:parseFloat(thr.value) }));
}
applyBtn.addEventListener('click', sendCfg);

ws.addEventListener('message', e => {
  const j = JSON.parse(e.data);

  // cfg_ack: update sliders to reflect the clamped values accepted by the ESP32
  if (j.type === 'cfg_ack') { alpha.value=j.alpha; avg.value=j.avg; thr.value=j.thr; refreshLabels(); return; }

  // recal_done: flash the button to acknowledge the completed recalibration
  if (j.type === 'recal_done') { flashRecal('&#x2713; Recalibrated'); return; }

  // Live sample frame: update metric strip and push to ring buffer
  rawVal.textContent   = j.raw.toFixed(1);
  filtVal.textContent  = j.filtered.toFixed(1);
  deltaVal.textContent = j.delta.toFixed(1);
  rpmVal.textContent   = j.rpm > 0 ? j.rpm.toFixed(2) : '-';
  dipVal.textContent   = j.dip ? 'DIP' : '-';
  dipVal.classList.toggle('lit', !!j.dip);  // Red highlight when dip is active

  // Push the channel selected by the mode dropdown (filtered / raw / delta)
  pushSample(j.t, j[modeEl.value], j.dip);
  if (!frozen) viewEnd = logicalLen();  // Auto-scroll to latest when not frozen
});

// ── Mouse interaction ─────────────────────────────────────────────────────
// Click-drag pans the view; scroll wheel zooms the time axis (Shift = Y axis).
let mouseDrag = false, mouseDragStartX = 0, mouseDragViewEnd = 0;

canvas.addEventListener('mousedown', ev => {
  mouseDrag = true; mouseDragStartX = ev.clientX; mouseDragViewEnd = viewEnd;
});
window.addEventListener('mouseup',   () => { mouseDrag = false; });
window.addEventListener('mousemove', ev => {
  if (!mouseDrag) return;
  clampView();
  // Convert pixel delta to sample-index delta proportional to current zoom level
  viewEnd = mouseDragViewEnd - (ev.clientX - mouseDragStartX) * (viewSpan / cssW);
  clampView();
});

canvas.addEventListener('wheel', ev => {
  ev.preventDefault();
  const zoom = ev.deltaY < 0 ? 0.85 : 1.15;  // Scroll up = zoom in (0.85×)
  if (ev.shiftKey) {
    // Shift+scroll: zoom Y axis (only when autoscale is disabled)
    if (!autoscale.checked) ySpan = Math.max(5, Math.min(ySpan * zoom, 50000));
  } else {
    // Normal scroll: zoom time axis, keeping the cursor position as anchor
    const len = logicalLen(); if (len < 10) return;
    clampView();
    const rect   = canvas.getBoundingClientRect();
    const frac   = Math.min(1, Math.max(0, (ev.clientX - rect.left) / cssW));
    const anchor = (viewEnd - viewSpan) + frac * viewSpan;  // Logical index under cursor
    viewSpan = Math.max(50, Math.min(Math.floor(viewSpan * zoom), len));
    viewEnd  = anchor + (1 - frac) * viewSpan;  // Keep anchor stationary
    clampView();
  }
}, { passive: false });

// ── Touch interaction ─────────────────────────────────────────────────────
// Tracks up to 2 simultaneous touches. One finger = pan; two fingers = pinch
// zoom (horizontal = time axis, vertical = Y axis).
let activeTouches = {}, lastPinchDist = null, panStartX = null, panViewEnd = null;

function touchCount() { return Object.keys(activeTouches).length; }

/** updateTouches() — Refresh the activeTouches map from a touch event. */
function updateTouches(ev) {
  const rect = canvas.getBoundingClientRect();
  for (const t of ev.changedTouches)
    activeTouches[t.identifier] = { x: t.clientX - rect.left, y: t.clientY - rect.top };
}

/**
 * getPinch() — Return distance and midpoint of the two active touch points,
 * plus a flag indicating whether the pinch axis is more vertical than horizontal.
 */
function getPinch() {
  const pts = Object.values(activeTouches);
  if (pts.length < 2) return null;
  const dx = pts[0].x - pts[1].x, dy = pts[0].y - pts[1].y;
  return { dist: Math.hypot(dx, dy), midX: (pts[0].x + pts[1].x) * 0.5,
           vertical: Math.abs(dy) > Math.abs(dx) };
}

canvas.addEventListener('touchstart', ev => {
  ev.preventDefault(); updateTouches(ev);
  if (touchCount() === 1) {
    // Single finger: begin a pan gesture
    panStartX = Object.values(activeTouches)[0].x; panViewEnd = viewEnd; lastPinchDist = null;
  } else if (touchCount() === 2) {
    // Two fingers: begin a pinch-zoom gesture
    const p = getPinch(); lastPinchDist = p ? p.dist : null; panStartX = null;
  }
}, { passive: false });

canvas.addEventListener('touchmove', ev => {
  ev.preventDefault(); updateTouches(ev);
  const len = logicalLen(); if (len < 10) return;

  if (touchCount() === 1 && panStartX !== null) {
    // Pan: shift viewEnd by the pixel delta mapped to sample-index space
    const dx = Object.values(activeTouches)[0].x - panStartX;
    viewEnd = panViewEnd - dx * (viewSpan / cssW);
    clampView();
  } else if (touchCount() === 2) {
    const p = getPinch();
    if (!p || lastPinchDist === null) { if (p) lastPinchDist = p.dist; return; }
    const ratio = lastPinchDist / p.dist;  // > 1 = fingers moving apart = zoom in
    if (!p.vertical) {
      // Horizontal pinch: zoom time axis around the midpoint between fingers
      clampView();
      const frac   = Math.min(1, Math.max(0, p.midX / cssW));
      const anchor = (viewEnd - viewSpan) + frac * viewSpan;
      viewSpan = Math.max(50, Math.min(Math.floor(viewSpan * ratio), len));
      viewEnd  = anchor + (1 - frac) * viewSpan;
      clampView();
    } else if (!autoscale.checked) {
      // Vertical pinch: zoom Y axis (only when autoscale is disabled)
      ySpan = Math.max(5, Math.min(ySpan * ratio, 50000));
    }
    lastPinchDist = p.dist;
  }
}, { passive: false });

canvas.addEventListener('touchend', ev => {
  for (const t of ev.changedTouches) delete activeTouches[t.identifier];
  lastPinchDist = null;
  // If one finger remains, transition seamlessly back to pan mode
  if (touchCount() === 1) { panStartX = Object.values(activeTouches)[0].x; panViewEnd = viewEnd; }
});
canvas.addEventListener('touchcancel', ev => {
  for (const t of ev.changedTouches) delete activeTouches[t.identifier];
  lastPinchDist = null; panStartX = null;
});

// ── Drawing ───────────────────────────────────────────────────────────────
/**
 * draw() — Render one frame and schedule the next via requestAnimationFrame.
 *
 * Rendering pipeline:
 *   1. Compute Y range (autoscale from visible data, or use yCenter/ySpan).
 *   2. Draw axes and grid lines with Y and X tick labels.
 *   3. Draw the signal line (blue) through all visible samples.
 *   4. Draw vertical dip markers (red) at each detected dip.
 *   5. Optionally draw the frozen-view overlay.
 *   6. Update the status bar text.
 */
function draw() {
  ctx.clearRect(0, 0, cssW, cssH);
  const len = logicalLen();
  if (len < 10) { requestAnimationFrame(draw); return; }

  if (viewEnd === 0) viewEnd = len;
  clampView();
  const viewStart = Math.max(0, viewEnd - viewSpan);

  // ── Y range ──────────────────────────────────────────────────────────────
  let min = Infinity, max = -Infinity;
  if (autoscale.checked) {
    // Scan visible samples to find the min/max, then add 15% padding
    for (let i = viewStart; i < viewEnd; i++) {
      const v = getPoint(i).v;
      if (v < min) min = v; if (v > max) max = v;
    }
    if (!isFinite(min) || !isFinite(max) || min === max) { min -= 1; max += 1; }
    const pad = (max - min) * 0.15 + 1e-6; min -= pad; max += pad;
    yCenter = (min + max) * 0.5; ySpan = max - min;
  } else {
    min = yCenter - ySpan * 0.5; max = yCenter + ySpan * 0.5;
    if (min === max) { min -= 1; max += 1; }
  }

  // ── Margins and coordinate helpers ───────────────────────────────────────
  const narrow = cssW < 420;  // Compact layout for small screens
  const mL = narrow ? 48 : 62, mR = 8, mT = 18, mB = narrow ? 30 : 40;
  const pw = cssW - mL - mR, ph = cssH - mT - mB;

  // Convert logical sample index → x pixel; signal value → y pixel
  const xPix = i   => mL + (i / Math.max(viewSpan - 1, 1)) * pw;
  const yPix = val => mT + (1 - (val - min) / (max - min)) * ph;

  // ── Axes ──────────────────────────────────────────────────────────────────
  ctx.strokeStyle = '#555'; ctx.lineWidth = 1;
  ctx.beginPath(); ctx.moveTo(mL,mT); ctx.lineTo(mL,mT+ph); ctx.lineTo(mL+pw,mT+ph); ctx.stroke();

  // ── Y grid + labels ───────────────────────────────────────────────────────
  const yTicks = narrow ? 4 : 6;
  ctx.font = (narrow ? 10 : 11) + 'px Arial';
  for (let k = 0; k <= yTicks; k++) {
    const frac = k / yTicks, y = mT + frac * ph, val = max - frac * (max - min);
    ctx.strokeStyle = '#1e1e1e'; ctx.lineWidth = 1;
    ctx.beginPath(); ctx.moveTo(mL,y); ctx.lineTo(mL+pw,y); ctx.stroke();
    ctx.fillStyle = '#777';
    const lbl = val.toFixed(1);
    ctx.fillText(lbl, mL - ctx.measureText(lbl).width - 3, y + 4);
  }

  // ── X time labels ─────────────────────────────────────────────────────────
  // Labels show elapsed seconds within the current view window
  const t0 = getPoint(viewStart).t;
  const t1 = getPoint(Math.min(viewEnd - 1, len - 1)).t;
  const xTicks = narrow ? 3 : 5;
  for (let k = 0; k <= xTicks; k++) {
    const frac = k / xTicks, x = mL + frac * pw;
    ctx.strokeStyle = '#1e1e1e'; ctx.lineWidth = 1;
    ctx.beginPath(); ctx.moveTo(x,mT); ctx.lineTo(x,mT+ph); ctx.stroke();
    const lbl = ((t1 - t0) * frac / 1000).toFixed(1) + 's';
    ctx.fillStyle = '#666';
    ctx.fillText(lbl, x - (k > 0 ? ctx.measureText(lbl).width / 2 : 0), mT + ph + (narrow ? 13 : 18));
  }

  // ── Signal line ───────────────────────────────────────────────────────────
  ctx.strokeStyle = '#5fd7ff'; ctx.lineWidth = narrow ? 1.5 : 2;
  ctx.beginPath();
  for (let i = 0; i < viewSpan; i++) {
    const p = getPoint(viewStart + i), x = xPix(i), y = yPix(p.v);
    if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
  }
  ctx.stroke();

  // ── Dip markers ───────────────────────────────────────────────────────────
  // Full-height vertical red lines at every sample where dip == 1
  ctx.strokeStyle = '#ff5f5f'; ctx.lineWidth = 1;
  for (let i = 0; i < viewSpan; i++) {
    const p = getPoint(viewStart + i);
    if (p.d) { const x = xPix(i); ctx.beginPath(); ctx.moveTo(x,mT); ctx.lineTo(x,mT+ph); ctx.stroke(); }
  }

  // ── Frozen overlay ────────────────────────────────────────────────────────
  if (frozen) {
    ctx.fillStyle = 'rgba(255,200,0,0.08)'; ctx.fillRect(mL,mT,pw,ph);
    ctx.fillStyle = '#ffcc00'; ctx.font = 'bold 13px Arial';
    ctx.fillText('FROZEN', mL + pw/2 - 28, mT + ph/2);
  }

  // ── Status bar ────────────────────────────────────────────────────────────
  const winSec = ((t1 - t0) / 1000).toFixed(2);
  statusbar.textContent = 'Showing ' + viewSpan + ' pts  \u2502  window: ' + winSec + 's  \u2502  total received: ' + totalCount + (frozen ? '  \u2744 FROZEN' : '  \u25CF live');

  requestAnimationFrame(draw);  // Schedule next frame
}

draw();  // Kick off the render loop
})();
</script>
</body>
</html>
)HTML";