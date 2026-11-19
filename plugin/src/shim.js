/* SPDX-License-Identifier: GPL-3.0-only */
/* Runs in the plugin's window before web/editor.html: the editor's WebMIDI is the plugin's link to the FM-1 (one
   port, shared with the plugin's own requests: link.cpp), and a bar on top holds this instance's settings. */
(function () {
  "use strict";
  if (window.__fm1Shim) return;
  window.__fm1Shim = true;
  const NAME = "FM-1 (plugin)";
  const listeners = [];
  let up = false;                                   /* the link has the FM-1 */
  const input = {
    id: "fm1-plugin-in", name: NAME, manufacturer: "skiff-atlatl", type: "input", state: "disconnected",
    connection: "open", onmidimessage: null, onstatechange: null,
    open() { return Promise.resolve(input); }, close() { return Promise.resolve(input); },
    addEventListener(t, f) { if (t === "midimessage") listeners.push(f); },
    removeEventListener(t, f) { const i = listeners.indexOf(f); if (i >= 0) listeners.splice(i, 1); },
  };
  const output = {
    id: "fm1-plugin-out", name: NAME, manufacturer: "skiff-atlatl", type: "output", state: "disconnected",
    connection: "open", onstatechange: null,
    open() { return Promise.resolve(output); }, close() { return Promise.resolve(output); },
    send(d) { if (up) window.fm1Send(Array.from(d)); }, clear() {},
    addEventListener() {}, removeEventListener() {},
  };
  const access = {
    sysexEnabled: true, inputs: new Map(), outputs: new Map(), onstatechange: null,
    addEventListener() {}, removeEventListener() {},
  };
  const setUp = (u) => {
    if (u === up) return;
    up = u;
    for (const p of [input, output]) p.state = u ? "connected" : "disconnected";
    if (u) { access.inputs.set(input.id, input); access.outputs.set(output.id, output); }
    else { access.inputs.clear(); access.outputs.clear(); }
    for (const p of [input, output]) if (access.onstatechange) access.onstatechange({ port: p });
  };
  Object.defineProperty(navigator, "requestMIDIAccess", { configurable: true, value: () => Promise.resolve(access) });
  window.__fm1In = (msgs) => {
    for (const m of msgs) {
      const ev = { data: Uint8Array.from(m), timeStamp: performance.now() };
      if (input.onmidimessage) input.onmidimessage(ev);
      for (const f of listeners) f(ev);
    }
  };

  /* the bar */
  const css = `
#fm1bar{position:fixed;top:0;left:0;right:0;z-index:2147483000;display:flex;flex-direction:column;gap:4px;
 padding:5px 12px;box-sizing:border-box;background:#15171c;color:#d8dbe2;font:12px/1 system-ui,sans-serif;border-bottom:1px solid #2a2e37}
#fm1bar b{letter-spacing:.06em;color:#fff}#fm1bar label{display:flex;align-items:center;gap:5px;white-space:nowrap}
#fm1bar{color-scheme:dark}#fm1bar select,#fm1bar input[type=number]{-webkit-appearance:none;appearance:none;
 background:#22252d;color:#e8eaf0;border:1px solid #363b46;border-radius:4px;font:inherit;padding:2px 6px;height:22px;box-sizing:border-box}
#fm1bar select{padding-right:18px;background-image:linear-gradient(45deg,transparent 50%,#9aa0ad 50%),
 linear-gradient(135deg,#9aa0ad 50%,transparent 50%);background-position:calc(100% - 10px) 9px,calc(100% - 6px) 9px;
 background-size:4px 4px;background-repeat:no-repeat}#fm1bar option{background:#22252d;color:#e8eaf0}#fm1bar input[type=number]{width:48px}#fm1bar .st{margin-left:auto;display:flex;
 align-items:center;gap:6px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}#fm1bar .dot{width:8px;height:8px;
 border-radius:50%;background:#c0392b}#fm1bar .dot.on{background:#2ecc71}#fm1bar .warn{color:#f5b041}
#fm1bar .r{display:flex;align-items:center;gap:14px;min-height:22px}#fm1bar .cap{display:none}
#fm1bar.capture .cap{display:flex}#fm1bar input[data-k=lat]{width:60px}#fm1bar button{background:#2c3140;color:#e8eaf0;
 border:1px solid #434a5a;border-radius:4px;font:inherit;padding:3px 10px;cursor:pointer}#fm1bar .ro{color:#9aa0ad}
#fm1bar.capture .direct{display:none}`;
  let clicked = false;
  const el = (h) => { const t = document.createElement("template"); t.innerHTML = h.trim(); return t.content.firstChild; };
  function build() {
    if (document.getElementById("fm1bar")) return;
    const st = document.createElement("style");
    st.textContent = css;
    document.head.appendChild(st);
    const bar = el(`<div id="fm1bar"><div class="r"><b>FM-1</b>
      <label title="The FM-1 track this instance plays and edits (MIDI channel = the track)">TRACK
        <select data-k="track">${[1, 2, 3, 4, 5, 6].map((n) => `<option value="${n - 1}">T${n}</option>`).join("")}</select></label>
      <label title="The DAW's notes for this track go to the FM-1"><input type="checkbox" data-k="notes">NOTES</label>
      <label title="Send the DAW's MIDI clock and transport (one instance only; the FM-1's CLK on USB)"><input type="checkbox" data-k="clock">CLOCK</label>
      <label class="direct" title="Pass this plugin's audio input (the FM-1's USB audio) to its output"><input type="checkbox" data-k="monitor">MONITOR</label>
      <label title="Shift the notes and clock against the DAW (ms)">OFFSET<input type="number" min="-50" max="50" step="1" data-k="offset"></label>
      <label title="The FM-1's MIDI port">PORT<select data-k="port"></select></label>
      <label title="DIRECT: the FM-1's sound on the DAW's own input, no latency. CAPTURE: the plugin takes the FM-1's 8 USB channels itself and reports its latency, so the DAW lines the sound up with the notes">MODE
        <select data-k="mode"><option value="0">DIRECT</option><option value="1">CAPTURE</option></select></label>
      <span class="st"><span class="dot"></span><span class="txt">…</span></span></div>
      <div class="r cap"><label title="TRACK: this track's own channel (3-8: before the shared delay, reverb, chorus). MAIN: the master (1-2), everything">SOURCE
        <select data-k="source"><option value="0">TRACK</option><option value="1">MAIN</option></select></label>
      <label title="The FM-1's USB audio input (8 channels)">INPUT<select data-k="input"></select></label>
      <label title="The latency reported to the DAW (ms); 0: AUTO, from the round trip">LATENCY<input type="number" min="0" max="1000" step="1" data-k="lat" placeholder="AUTO"></label>
      <button data-act="measure" title="Plays a note on this track and times its sound coming back: the round trip. Use a sound with a quick attack">MEASURE</button>
      <span class="ro"></span></div></div>`);
    document.body.prepend(bar);
    const pad = () => { document.body.style.setProperty("padding-top", bar.offsetHeight + "px", "important"); };
    new ResizeObserver(pad).observe(bar);
    pad();
    bar.addEventListener("click", (e) => {
      if (e.target.dataset.act === "measure") window.fm1Set("measure", "1");
    });
    bar.addEventListener("change", (e) => {
      const k = e.target.dataset.k;
      if (!k) return;
      const v = e.target.type === "checkbox" ? (e.target.checked ? "1" : "0") : String(e.target.value);
      window.fm1Set(k, v);
    });
  }
  function show(s) {
    const bar = document.getElementById("fm1bar");
    if (!bar) return;
    const q = (k) => bar.querySelector(`[data-k="${k}"]`);
    const focus = document.activeElement;
    for (const k of ["notes", "clock", "monitor"]) if (q(k) !== focus) q(k).checked = !!s[k];
    if (q("track") !== focus) q("track").value = String(s.track);
    if (q("offset") !== focus) q("offset").value = String(s.offset);
    const fill = (sel, list, cur) => {
      if (sel === focus) return;
      const want = ["", ...list];
      if (cur && !want.includes(cur)) want.push(cur);
      if (sel.options.length !== want.length || [...sel.options].some((o, i) => o.value !== want[i]))
        sel.replaceChildren(...want.map((n) => { const o = document.createElement("option"); o.value = n; o.textContent = n || "AUTO"; return o; }));
      sel.value = cur;
    };
    fill(q("port"), s.ports, s.wantPort);
    for (const k of ["mode", "source"]) if (q(k) !== focus) q(k).value = String(s[k]);
    bar.classList.toggle("capture", s.mode === 1);
    if (s.mode === 1) {
      fill(q("input"), s.inputs || [], s.input);
      if (q("lat") !== focus) q("lat").value = s.lat ? String(s.lat) : "";
      const c = s.cap || {};
      const m = ["", " · measuring…", " · measuring…", " · measured", " · nothing heard: a louder sound, a quicker attack?"][s.measure] || "";
      let r = `RT ${s.rt.toFixed(1)} ms · `;
      r += `LATENCY ${(s.latActual || s.latNext).toFixed(1)} ms`;
      if (s.latActual > 0 && Math.abs(s.latNext - s.latActual) > 0.5) r += ` (${s.latNext.toFixed(1)} after the DAW restarts the plugin)`;
      r += c.running ? ` · ${c.channels} ch ${c.rate / 1000} kHz` : ` · ${c.error || "no input"}`;
      if (c.running && c.error) r += ` (${c.error})`;
      bar.querySelector(".ro").textContent = r + m;
    }
    bar.querySelector(".dot").classList.toggle("on", s.connected && s.replying);
    let t = s.connected ? `${s.port}${s.version ? " · " + s.version : ""}` : (s.error || "looking for the FM-1…");
    if (s.connected && !s.replying) t += " · no reply yet";
    const txt = bar.querySelector(".txt");
    txt.textContent = t;
    txt.className = "txt";
    if (s.mismatch) { txt.textContent += " · another firmware: parameters not synced"; txt.className = "txt warn"; }
    if (s.recallBlocked) { txt.textContent += " · the saved sound is another engine's: not recalled"; txt.className = "txt warn"; }
  }
  async function poll() {
    try {
      const s = JSON.parse(await window.fm1Status());
      show(s);
      setUp(!!s.connected);
      if (s.connected && s.replying && !clicked) {   /* the editor connects by itself, once (then it follows) */
        const b = document.getElementById("connect");
        if (b) { clicked = true; b.click(); }
      }
    } catch (e) { /* (the window closing) */ }
    setTimeout(poll, 500);
  }
  const start = () => { build(); poll(); };
  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", start); else start();
})();
