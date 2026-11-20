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
    id: "fm1-plugin-in", name: NAME, manufacturer: "ATLATL", type: "input", state: "disconnected",
    connection: "open", onmidimessage: null, onstatechange: null,
    open() { return Promise.resolve(input); }, close() { return Promise.resolve(input); },
    addEventListener(t, f) { if (t === "midimessage") listeners.push(f); },
    removeEventListener(t, f) { const i = listeners.indexOf(f); if (i >= 0) listeners.splice(i, 1); },
  };
  const output = {
    id: "fm1-plugin-out", name: NAME, manufacturer: "ATLATL", type: "output", state: "disconnected",
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

  /* the bar: ATLATL's mark (tools/make_logo.py writes it between the markers) and its colours: the night, the dark
     squares' blue-black, turquoise and green */
  const MARK = /*brand:mark*/`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 238 238" role="img" aria-label="ATLATL"><defs><filter id="glow" x="-30%" y="-30%" width="160%" height="160%" color-interpolation-filters="sRGB"><feGaussianBlur stdDeviation="3" result="b"/><feMerge><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge></filter></defs><rect x="0.00" y="0.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="22.00" y="0.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="44.00" y="0.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="66.00" y="0.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="88.00" y="0.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="110.00" y="0.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="132.00" y="0.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="154.00" y="0.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="176.00" y="0.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="198.00" y="0.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="220.00" y="0.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="0.00" y="22.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="22.00" y="22.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="44.00" y="22.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="66.00" y="22.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="88.00" y="22.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="110.00" y="22.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="0.00" y="44.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="22.00" y="44.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="44.00" y="44.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="66.00" y="44.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="88.00" y="44.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="110.00" y="44.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="154.00" y="44.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="176.00" y="44.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="198.00" y="44.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="0.00" y="66.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="22.00" y="66.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="44.00" y="66.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="66.00" y="66.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="88.00" y="66.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="110.00" y="66.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="154.00" y="66.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="198.00" y="66.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="0.00" y="88.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="22.00" y="88.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="44.00" y="88.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="66.00" y="88.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="88.00" y="88.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="110.00" y="88.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="154.00" y="88.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="0.00" y="110.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="22.00" y="110.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="44.00" y="110.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="66.00" y="110.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="88.00" y="110.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="154.00" y="110.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="176.00" y="110.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="198.00" y="110.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="220.00" y="110.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="0.00" y="132.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="22.00" y="132.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="44.00" y="132.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="66.00" y="132.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="132.00" y="132.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="154.00" y="132.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="176.00" y="132.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="198.00" y="132.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="220.00" y="132.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="0.00" y="154.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="22.00" y="154.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="44.00" y="154.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="110.00" y="154.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="132.00" y="154.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="154.00" y="154.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="176.00" y="154.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="198.00" y="154.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="220.00" y="154.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="0.00" y="176.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="22.00" y="176.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="88.00" y="176.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="110.00" y="176.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="132.00" y="176.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="154.00" y="176.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="176.00" y="176.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="198.00" y="176.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="220.00" y="176.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="0.00" y="198.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="66.00" y="198.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="88.00" y="198.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="110.00" y="198.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="132.00" y="198.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="154.00" y="198.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="176.00" y="198.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="198.00" y="198.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="220.00" y="198.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="0.00" y="220.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="22.00" y="220.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="44.00" y="220.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="66.00" y="220.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="88.00" y="220.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="110.00" y="220.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="132.00" y="220.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="154.00" y="220.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="176.00" y="220.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="198.00" y="220.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><rect x="220.00" y="220.00" width="18.00" height="18.00" rx="3.00" fill="#17172a"/><g filter="url(#glow)"><rect x="132.00" y="22.00" width="18.00" height="18.00" rx="3.00" fill="#ffffff"/><rect x="154.00" y="22.00" width="18.00" height="18.00" rx="3.00" fill="#ffffff"/><rect x="176.00" y="22.00" width="18.00" height="18.00" rx="3.00" fill="#ffffff"/><rect x="198.00" y="22.00" width="18.00" height="18.00" rx="3.00" fill="#ffffff"/><rect x="220.00" y="22.00" width="18.00" height="18.00" rx="3.00" fill="#ffffff"/><rect x="132.00" y="44.00" width="18.00" height="18.00" rx="3.00" fill="#ffffff"/><rect x="220.00" y="44.00" width="18.00" height="18.00" rx="3.00" fill="#ffffff"/><rect x="132.00" y="66.00" width="18.00" height="18.00" rx="3.00" fill="#ffffff"/><rect x="176.00" y="66.00" width="18.00" height="18.00" rx="3.00" fill="#ffffff"/><rect x="220.00" y="66.00" width="18.00" height="18.00" rx="3.00" fill="#ffffff"/><rect x="132.00" y="88.00" width="18.00" height="18.00" rx="3.00" fill="#ffffff"/><rect x="176.00" y="88.00" width="18.00" height="18.00" rx="3.00" fill="#ffffff"/><rect x="198.00" y="88.00" width="18.00" height="18.00" rx="3.00" fill="#ffffff"/><rect x="220.00" y="88.00" width="18.00" height="18.00" rx="3.00" fill="#ffffff"/><rect x="110.00" y="110.00" width="18.00" height="18.00" rx="3.00" fill="#39ff14"/><rect x="132.00" y="110.00" width="18.00" height="18.00" rx="3.00" fill="#39ff14"/><rect x="88.00" y="132.00" width="18.00" height="18.00" rx="3.00" fill="#ff6a00"/><rect x="110.00" y="132.00" width="18.00" height="18.00" rx="3.00" fill="#ff6a00"/><rect x="66.00" y="154.00" width="18.00" height="18.00" rx="3.00" fill="#00f5d4"/><rect x="88.00" y="154.00" width="18.00" height="18.00" rx="3.00" fill="#00f5d4"/><rect x="44.00" y="176.00" width="18.00" height="18.00" rx="3.00" fill="#ffe600"/><rect x="66.00" y="176.00" width="18.00" height="18.00" rx="3.00" fill="#ffe600"/><rect x="22.00" y="198.00" width="18.00" height="18.00" rx="3.00" fill="#b026ff"/><rect x="44.00" y="198.00" width="18.00" height="18.00" rx="3.00" fill="#b026ff"/></g></svg>`/*/brand:mark*/;
  const css = `
#fm1bar{position:fixed;top:0;left:0;right:0;z-index:2147483000;display:flex;flex-direction:column;gap:5px;
 padding:6px 12px;box-sizing:border-box;background:#07070d;color:#d6d6e6;font:12px/1 system-ui,sans-serif;
 border-bottom:1px solid #22223a;box-shadow:0 1px 0 #00f5d422;color-scheme:dark}
#fm1bar .brand{display:flex;align-items:center;gap:7px;margin-right:4px}#fm1bar .brand svg{height:22px;width:22px;display:block}
#fm1bar .brand b{font:700 14px/1 "Barlow Condensed",system-ui,sans-serif;letter-spacing:.18em;color:#fff;
 text-shadow:0 0 6px #00f5d488}#fm1bar .brand i{font:500 12px/1 "Barlow Condensed",system-ui,sans-serif;font-style:normal;
 letter-spacing:.1em;color:#00f5d4}
#fm1bar label{display:flex;align-items:center;gap:5px;white-space:nowrap;color:#9a9ab0;letter-spacing:.03em}
#fm1bar select,#fm1bar input[type=number]{-webkit-appearance:none;appearance:none;background:#17172a;color:#eeeefa;
 border:1px solid #2a2a46;border-radius:4px;font:inherit;padding:2px 6px;height:22px;box-sizing:border-box;outline:none}
#fm1bar select:focus,#fm1bar input:focus,#fm1bar button:focus-visible{border-color:#00f5d4;box-shadow:0 0 0 1px #00f5d455}
#fm1bar select{padding-right:18px;background-image:linear-gradient(45deg,transparent 50%,#00f5d4 50%),
 linear-gradient(135deg,#00f5d4 50%,transparent 50%);background-position:calc(100% - 10px) 9px,calc(100% - 6px) 9px;
 background-size:4px 4px;background-repeat:no-repeat}#fm1bar option{background:#17172a;color:#eeeefa}
#fm1bar input[type=number]{width:48px}#fm1bar input[data-k=lat]{width:60px}
#fm1bar input[type=checkbox]{accent-color:#39ff14}
#fm1bar .st{margin-left:auto;display:flex;align-items:center;gap:6px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
#fm1bar .dot{width:8px;height:8px;border-radius:2px;background:#ff3b5c;box-shadow:0 0 5px #ff3b5c99}
#fm1bar .dot.on{background:#39ff14;box-shadow:0 0 6px #39ff14cc}#fm1bar .warn{color:#ffe600}
#fm1bar .r{display:flex;align-items:center;gap:14px;min-height:22px}#fm1bar .cap{display:none}
#fm1bar.capture .cap{display:flex}#fm1bar.capture .direct{display:none}
#fm1bar button{background:#17172a;color:#00f5d4;border:1px solid #00f5d466;border-radius:4px;font:inherit;
 letter-spacing:.06em;padding:3px 10px;cursor:pointer}#fm1bar button:hover{background:#00f5d41a}#fm1bar .ro{color:#9a9ab0}`;
  let clicked = false;
  const el = (h) => { const t = document.createElement("template"); t.innerHTML = h.trim(); return t.content.firstChild; };
  function build() {
    if (document.getElementById("fm1bar")) return;
    const st = document.createElement("style");
    st.textContent = css;
    document.head.appendChild(st);
    const bar = el(`<div id="fm1bar"><div class="r"><span class="brand">${MARK}<b>ATLATL</b><i>FM-1</i></span>
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
