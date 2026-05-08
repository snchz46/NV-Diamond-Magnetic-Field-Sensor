#include "web_page.h"

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<title>NV Quenching Plotter</title>
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
  body{background:#111;color:#ddd;font-family:Arial;margin:16px}
  h2{margin:0 0 10px 0}
  canvas{background:#000;border:1px solid #333;border-radius:12px}
  .card{background:#1b1b1b;border:1px solid #333;border-radius:12px;padding:10px}
  .row{display:flex;gap:12px;flex-wrap:wrap;align-items:center}
  button,select,input{background:#222;color:#ddd;border:1px solid #444;border-radius:8px;padding:6px}
  .small{font-size:12px;opacity:.85}
  .ok{color:#7CFC00}.bad{color:#ff5f5f}
</style>
</head>

<body>
<h2>NV Quenching – Live</h2>

<div class="row">
  <div class="card">
    <div>WS: <b id="ws" class="bad">offline</b></div>
    <div>raw: <b id="raw">-</b></div>
    <div>filtered: <b id="filt">-</b></div>
    <div>delta: <b id="delta">-</b></div>
    <div>RPM: <b id="rpm">-</b></div>
    <div>dip: <b id="dip">-</b></div>
  </div>

  <div class="card">
    <div class="row">
      <label>Signal</label>
      <select id="mode">
        <option value="filtered">filtered</option>
        <option value="raw">raw</option>
        <option value="delta">delta</option>
      </select>

      <label>Autoscale</label>
      <input id="autoscale" type="checkbox" checked>

      <button id="freeze">Freeze</button>
      <button id="clear">Clear</button>
      <a href="/export.csv"><button>CSV</button></a>
    </div>

    <hr style="border:0;border-top:1px solid #333;margin:8px 0">

    <div class="row">
      <div>
        <div class="small">IIR α: <b id="aVal">0.15</b></div>
        <input id="alpha" type="range" min="0.01" max="0.30" step="0.01" value="0.15">
      </div>

      <div>
        <div class="small">Avg: <b id="avgVal">8</b></div>
        <input id="avg" type="range" min="1" max="16" step="1" value="8">
      </div>

      <div>
        <div class="small">Threshold: <b id="thrVal">-15</b></div>
        <input id="thr" type="range" min="-200" max="0" step="1" value="-15">
      </div>

      <button id="apply">Apply</button>
    </div>

    <div class="small" style="margin-top:6px">
      Wheel: zoom X · Drag: pan · Shift+wheel: zoom Y (autoscale off)
    </div>
  </div>
</div>

<canvas id="plot" width="1100" height="470"></canvas>

<script>
(() => {
  // --- UI refs ---
  const wsEl=document.getElementById("ws");
  const rawEl=document.getElementById("raw");
  const filtEl=document.getElementById("filt");
  const deltaEl=document.getElementById("delta");
  const rpmEl=document.getElementById("rpm");
  const dipEl=document.getElementById("dip");

  const mode=document.getElementById("mode");
  const autoscale=document.getElementById("autoscale");
  const freezeBtn=document.getElementById("freeze");
  const clearBtn=document.getElementById("clear");

  const alpha=document.getElementById("alpha");
  const avg=document.getElementById("avg");
  const thr=document.getElementById("thr");
  const aVal=document.getElementById("aVal");
  const avgVal=document.getElementById("avgVal");
  const thrVal=document.getElementById("thrVal");
  const applyBtn=document.getElementById("apply");

  const canvas=document.getElementById("plot");
  const ctx=canvas.getContext("2d");

  // --- ring buffer ---
  const N=5000;
  let tbuf=new Float64Array(N);
  let vbuf=new Float32Array(N);
  let dips=new Uint8Array(N);

  let writeIdx=0;           // next write position
  let totalCount=0;         // total samples received ever

  function logicalLen(){ return Math.min(totalCount, N); }

  // logicalIndex: 0..len-1 in chronological order (oldest->newest)
  function getPoint(logicalIndex){
    const len = logicalLen();
    if (len <= 0) return {t:0,v:0,d:0};

    // If not full yet, oldest is at 0
    if (totalCount < N){
      return { t: tbuf[logicalIndex], v: vbuf[logicalIndex], d: dips[logicalIndex] };
    }

    // If full, oldest is at writeIdx (because writeIdx points to where next write goes)
    const real = (writeIdx + logicalIndex) % N;
    return { t: tbuf[real], v: vbuf[real], d: dips[real] };
  }

  function pushSample(t, v, d){
    tbuf[writeIdx]=t;
    vbuf[writeIdx]=v;
    dips[writeIdx]=d?1:0;

    writeIdx = (writeIdx + 1) % N;
    totalCount++;
  }

  // --- view state ---
  let frozen=false;

  // X view in "logical indices"
  let viewSpan=1000;   // how many points shown
  let viewEnd=0;       // end index (exclusive) in logical space [0..len]

  // Y manual zoom (only when autoscale OFF)
  let yCenter=0, ySpan=300;

  // drag pan
  let dragging=false;
  let dragStartX=0;
  let dragStartViewEnd=0;

  function clampView(){
    const len = logicalLen();
    if (len < 10) return;
    viewSpan = Math.max(50, Math.min(viewSpan, len));
    viewEnd = Math.max(viewSpan, Math.min(viewEnd, len));
  }

  function refreshLabels(){
    aVal.textContent=parseFloat(alpha.value).toFixed(2);
    avgVal.textContent=avg.value;
    thrVal.textContent=thr.value;
  }
  refreshLabels();

  freezeBtn.onclick=()=>{
    frozen=!frozen;
    freezeBtn.textContent = frozen ? "Unfreeze" : "Freeze";
    if(!frozen){
      viewEnd = logicalLen(); // follow live again
    }
  };

  clearBtn.onclick=()=>{
    // hard reset client-side buffer
    tbuf=new Float64Array(N);
    vbuf=new Float32Array(N);
    dips=new Uint8Array(N);
    writeIdx=0;
    totalCount=0;
    viewEnd=0;
    viewSpan=1000;
    yCenter=0;
    ySpan=300;
  };

  // --- WebSocket ---
  const ws=new WebSocket(`ws://${location.hostname}:81/`);
  ws.onopen=()=>{ wsEl.textContent="online"; wsEl.className="ok"; sendCfg(); };
  ws.onclose=()=>{ wsEl.textContent="offline"; wsEl.className="bad"; };

  function sendCfg(){
    if(ws.readyState!==1) return;
    ws.send(JSON.stringify({
      type:"cfg",
      alpha:parseFloat(alpha.value),
      avg:parseInt(avg.value),
      thr:parseFloat(thr.value)
    }));
  }
  applyBtn.onclick=sendCfg;
  [alpha,avg,thr].forEach(e=>e.oninput=refreshLabels);

  ws.onmessage=e=>{
    const j=JSON.parse(e.data);

    if(j.type==="cfg_ack"){
      alpha.value=j.alpha; avg.value=j.avg; thr.value=j.thr;
      refreshLabels();
      return;
    }

    rawEl.textContent=j.raw.toFixed(1);
    filtEl.textContent=j.filtered.toFixed(1);
    deltaEl.textContent=j.delta.toFixed(1);
    rpmEl.textContent = (j.rpm>0 ? j.rpm.toFixed(2) : "-");
    dipEl.textContent = (j.dip ? "DIP" : "-");

    // Push into ring using selected mode value
    if(!frozen){
      pushSample(j.t, j[mode.value], j.dip);
      viewEnd = logicalLen(); // follow live
    } else {
      // even frozen, keep data arriving? For now: still store data (so CSV/time keeps moving),
      // but viewEnd doesn't follow.
      pushSample(j.t, j[mode.value], j.dip);
    }
  };

  // --- Zoom / Pan handlers ---
  canvas.addEventListener('mousedown', (ev)=>{
    dragging = true;
    dragStartX = ev.clientX;
    dragStartViewEnd = viewEnd;
  });
  window.addEventListener('mouseup', ()=> dragging=false);
  window.addEventListener('mousemove', (ev)=>{
    if(!dragging) return;
    clampView();
    const dx = ev.clientX - dragStartX;
    const ptsPerPx = viewSpan / canvas.width;
    viewEnd = dragStartViewEnd - dx*ptsPerPx;
    clampView();
  });

  canvas.addEventListener('wheel', (ev)=>{
    ev.preventDefault();
    const zoom = (ev.deltaY < 0) ? 0.85 : 1.15;

    if(ev.shiftKey){
      // Y zoom only if autoscale OFF
      if(!autoscale.checked){
        ySpan *= zoom;
        ySpan = Math.max(5, Math.min(ySpan, 50000));
      }
    } else {
      // X zoom
      const len = logicalLen();
      if(len < 10) return;

      // zoom around cursor position
      clampView();
      const rect = canvas.getBoundingClientRect();
      const x = ev.clientX - rect.left;
      const frac = Math.min(1, Math.max(0, x / canvas.width));
      const viewStart = viewEnd - viewSpan;
      const anchor = viewStart + frac*viewSpan;

      let newSpan = Math.floor(viewSpan * zoom);
      newSpan = Math.max(50, Math.min(newSpan, len));
      viewSpan = newSpan;

      // keep anchor point under cursor
      viewEnd = anchor + (1-frac)*viewSpan;
      clampView();
    }
  }, {passive:false});

  // --- Drawing ---
  function draw(){
    ctx.clearRect(0,0,canvas.width,canvas.height);

    const len = logicalLen();
    if(len < 10){
      requestAnimationFrame(draw);
      return;
    }

    if(viewEnd === 0) viewEnd = len;
    clampView();

    const viewStart = Math.max(0, viewEnd - viewSpan);

    // compute min/max for visible window
    let min=Infinity, max=-Infinity;
    if(autoscale.checked){
      for(let i=viewStart; i<viewEnd; i++){
        const v = getPoint(i).v;
        if(v<min) min=v;
        if(v>max) max=v;
      }
      if(!isFinite(min) || !isFinite(max) || min===max){ min-=1; max+=1; }
      const pad=(max-min)*0.15 + 1e-6;
      min-=pad; max+=pad;
      yCenter=(min+max)/2; ySpan=(max-min);
    } else {
      min = yCenter - ySpan/2;
      max = yCenter + ySpan/2;
      if(min===max){ min-=1; max+=1; }
    }

    // plot area with margins
    const w=canvas.width, h=canvas.height;
    const mL=70, mR=20, mT=20, mB=45;
    const pw=w-mL-mR, ph=h-mT-mB;

    const xPix = (i) => mL + (i/(viewSpan-1))*pw;
    const yPix = (val) => mT + (1 - (val-min)/(max-min))*ph;

    // axes
    ctx.strokeStyle="#555";
    ctx.lineWidth=1;
    ctx.beginPath();
    ctx.moveTo(mL,mT);
    ctx.lineTo(mL,mT+ph);
    ctx.lineTo(mL+pw,mT+ph);
    ctx.stroke();

    // grid + Y ticks
    ctx.font="12px Arial";
    ctx.fillStyle="#aaa";
    const yTicks=6;
    for(let k=0;k<=yTicks;k++){
      const frac = k/yTicks;
      const y = mT + frac*ph;
      const val = max - frac*(max-min);

      ctx.strokeStyle="#222";
      ctx.beginPath(); ctx.moveTo(mL,y); ctx.lineTo(mL+pw,y); ctx.stroke();

      ctx.fillStyle="#aaa";
      ctx.fillText(val.toFixed(1), 6, y+4);
    }

    // X time ticks
    const t0 = getPoint(viewStart).t;
    const t1 = getPoint(viewEnd-1).t;
    const xTicks=6;
    for(let k=0;k<=xTicks;k++){
      const frac = k/xTicks;
      const x = mL + frac*pw;

      ctx.strokeStyle="#222";
      ctx.beginPath(); ctx.moveTo(x,mT); ctx.lineTo(x,mT+ph); ctx.stroke();

      const tVal = t0 + frac*(t1-t0);
      const relS = (tVal - t0)/1000.0;
      ctx.fillStyle="#aaa";
      ctx.fillText(relS.toFixed(2)+" s", x-16, mT+ph+22);
    }

    // signal line
    ctx.strokeStyle="#5fd7ff";
    ctx.lineWidth=2;
    ctx.beginPath();
    for(let i=0;i<viewSpan;i++){
      const p = getPoint(viewStart+i);
      const x = xPix(i);
      const y = yPix(p.v);
      if(i===0) ctx.moveTo(x,y); else ctx.lineTo(x,y);
    }
    ctx.stroke();

    // dips
    ctx.strokeStyle="#ff5f5f";
    ctx.lineWidth=1;
    for(let i=0;i<viewSpan;i++){
      const p = getPoint(viewStart+i);
      if(p.d){
        const x = xPix(i);
        ctx.beginPath();
        ctx.moveTo(x,mT);
        ctx.lineTo(x,mT+ph);
        ctx.stroke();
      }
    }

    // status overlay
    ctx.fillStyle="#aaa";
    ctx.fillText(`points: ${viewSpan}   window: ${(t1-t0)/1000.0.toFixed(2)} s   ${frozen ? "FROZEN" : ""}`, mL, 14);

    requestAnimationFrame(draw);
  }

  draw();
})();
</script>
</body>
</html>
)HTML";
