#!/usr/bin/env python3
"""
gen_html.py - Render an interactive architecture diagram from the JSON
produced by gen_graph.py.

Single self-contained HTML file, no server required.

Usage:
  python tools/gen_html.py [--graph project_graph.json] [--out arch.html]
  python tools/gen_html.py --positions st4ever_positions.json
"""

import argparse
import json


HTML_TEMPLATE = r"""<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="UTF-8">
<title>ST4Ever — Architecture interactive</title>
<script src="https://cdnjs.cloudflare.com/ajax/libs/cytoscape/3.28.1/cytoscape.min.js"></script>
<script src="https://unpkg.com/layout-base/layout-base.js"></script>
<script src="https://unpkg.com/cose-base/cose-base.js"></script>
<script src="https://unpkg.com/cytoscape-fcose/cytoscape-fcose.js"></script>
<style>
  :root {
    --bg:      #1a1d23;
    --panel:   #23262e;
    --border:  #3a3f4b;
    --text:    #d8dce2;
    --muted:   #8890a0;
    --accent:  #4fc3f7;
    --accent2: #ffb74d;
    --green:   #66bb6a;
    --red:     #ef5350;
    --left-w:  260px;
    --seq-w:   370px;
    --right-w: 390px;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    font-family: 'Segoe UI', system-ui, sans-serif;
    background: var(--bg); color: var(--text);
    height: 100vh; overflow: hidden;
  }
  #app { display: flex; height: 100vh; }

  /* ── Left panel ── */
  #left-panel {
    width: var(--left-w); flex: 0 0 var(--left-w);
    background: var(--panel); border-right: 1px solid var(--border);
    padding: 14px 12px; overflow-y: auto; font-size: 12px;
    display: flex; flex-direction: column;
  }
  #left-panel h1 {
    font-size: 14px; font-weight: 700; color: var(--accent);
    margin-bottom: 2px; letter-spacing: .02em;
  }
  .lp-subtitle { color: var(--muted); font-size: 10px; margin-bottom: 14px; }
  .lp-section  { margin-bottom: 14px; }
  .lp-section-title {
    font-size: 10px; font-weight: 600; letter-spacing: .08em;
    text-transform: uppercase; color: var(--muted);
    margin-bottom: 7px; padding-bottom: 4px;
    border-bottom: 1px solid var(--border);
  }
  .lp-input {
    width: 100%; background: #14161a; color: var(--text);
    border: 1px solid var(--border); border-radius: 4px;
    padding: 5px 8px; font-size: 12px;
  }
  .lp-input:focus { outline: none; border-color: var(--accent); }
  .lp-select {
    width: 100%; background: #14161a; color: var(--text);
    border: 1px solid var(--border); border-radius: 4px;
    padding: 4px 6px; font-size: 12px;
  }
  .radio-group { display: flex; flex-direction: column; gap: 5px; }
  .radio-group label {
    display: flex; align-items: center; gap: 6px;
    cursor: pointer; color: var(--text); font-size: 12px;
    padding: 3px 5px; border-radius: 4px;
  }
  .radio-group label:hover { background: #2d3340; }
  .radio-group input[type="radio"],
  .radio-group input[type="checkbox"] { accent-color: var(--accent); cursor: pointer; }
  .lp-row { display: flex; align-items: center; gap: 8px; margin-bottom: 5px; }
  .lp-row label { font-size: 11px; color: var(--muted); white-space: nowrap; }
  .lp-btn {
    background: #2d3340; color: var(--text);
    border: 1px solid var(--border); border-radius: 4px;
    padding: 5px 8px; font-size: 11px; cursor: pointer;
    width: 100%; text-align: left; margin-bottom: 4px;
  }
  .lp-btn:hover { background: #394257; }
  .lp-btn-row { display: flex; gap: 4px; }
  .lp-btn-row .lp-btn { flex: 1; text-align: center; margin-bottom: 0; }
  #pos-status {
    color: var(--muted); font-size: 10px; margin-top: 5px; min-height: 14px;
    line-height: 1.4; word-break: break-word;
  }
  .lp-spacer { flex: 1; }

  /* ── Canvas ── */
  #canvas-area { flex: 1 1 auto; position: relative; overflow: hidden; }
  #cy { width: 100%; height: 100%; background: var(--bg); }

  /* ── Legend ── */
  #legend {
    position: absolute; bottom: 10px; left: 10px; z-index: 10;
    background: var(--panel); border: 1px solid var(--border);
    border-radius: 6px; padding: 7px 12px; font-size: 11px; color: var(--muted);
    line-height: 2;
  }
  .legend-item { display: inline-flex; align-items: center; gap: 5px; margin-right: 12px; }
  .legend-dot  { width: 9px; height: 9px; border-radius: 50%; flex-shrink: 0; }

  /* ── Hover tooltip ── */
  #tooltip {
    position: fixed; pointer-events: none; z-index: 200;
    background: #14161a; border: 1px solid var(--accent);
    border-radius: 6px; padding: 9px 11px; font-size: 12px;
    max-width: 380px; display: none;
    box-shadow: 0 4px 20px rgba(0,0,0,.65); line-height: 1.45;
  }
  #tooltip h4 { color: var(--accent); font-size: 13px; margin-bottom: 5px; }
  #tooltip .row { margin: 2px 0; }
  #tooltip .muted { color: var(--muted); }
  #tooltip code {
    background: #1e2128; padding: 1px 4px; border-radius: 3px;
    font-family: 'Consolas', monospace; font-size: 11px;
  }
  #tooltip table { border-collapse: collapse; font-size: 11px; margin-top: 4px; width: 100%; }
  #tooltip td { padding: 2px 6px 2px 0; vertical-align: top; }
  #tooltip td:first-child { color: var(--muted); white-space: nowrap; min-width: 80px; }
  #tooltip .tag-req      { color: var(--green); }
  #tooltip .tag-cov-none { color: var(--red); }

  /* ── Sequence panel ── */
  #seq-panel {
    position: fixed;
    right: var(--right-w);
    top: 0;
    width: var(--seq-w);
    height: 100vh;
    background: #1b1f28;
    border-left: 1px solid var(--accent);
    border-right: 1px solid var(--border);
    display: none;            /* flex when open */
    flex-direction: column;
    z-index: 100;
    box-shadow: -6px 0 28px rgba(0,0,0,.55);
    font-size: 12px;
  }
  #seq-header {
    display: flex; align-items: flex-start; justify-content: space-between;
    padding: 11px 12px 9px;
    border-bottom: 1px solid var(--border);
    flex-shrink: 0; gap: 8px;
  }
  #seq-title-area { flex: 1; min-width: 0; }
  #seq-label {
    font-size: 9px; font-weight: 600; letter-spacing: .1em;
    text-transform: uppercase; color: var(--muted); display: block; margin-bottom: 3px;
  }
  #seq-fn {
    font-family: 'Consolas', monospace; font-size: 14px;
    font-weight: 700; color: var(--accent);
    white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
    display: block;
  }
  #seq-mod-badge {
    font-size: 10px; color: var(--muted); margin-top: 2px; display: block;
  }
  #seq-close {
    background: none; border: none; color: var(--muted);
    font-size: 18px; cursor: pointer; line-height: 1; padding: 0 2px; flex-shrink: 0;
  }
  #seq-close:hover { color: var(--text); }
  #seq-options {
    display: flex; align-items: center; gap: 10px;
    padding: 7px 12px;
    border-bottom: 1px solid var(--border);
    flex-shrink: 0; flex-wrap: wrap;
  }
  #seq-options label { font-size: 11px; color: var(--muted); white-space: nowrap; }
  #seq-options select {
    background: #14161a; color: var(--text);
    border: 1px solid var(--border); border-radius: 4px;
    padding: 2px 5px; font-size: 11px;
  }
  #seq-options input[type="checkbox"] { accent-color: var(--accent); }
  #seq-lib-status {
    font-size: 10px; color: #5a6070; font-style: italic;
    padding: 4px 12px; flex-shrink: 0; border-bottom: 1px solid var(--border);
    display: none;
  }
  #seq-body {
    flex: 1; overflow-y: auto; padding: 10px 10px 10px 12px;
  }
  #seq-body::-webkit-scrollbar { width: 7px; }
  #seq-body::-webkit-scrollbar-thumb { background: #3a3f4b; border-radius: 4px; }

  /* Tree */
  .tree-root { margin-bottom: 10px; padding-bottom: 10px; border-bottom: 1px solid var(--border); }
  .tree-root-fn {
    font-family: 'Consolas', monospace; font-size: 14px; font-weight: 700;
  }
  .tree-root-mod { font-size: 10px; color: var(--muted); margin-left: 6px; }
  .tree-root-desc { color: var(--muted); font-size: 11px; margin-top: 4px; line-height: 1.4; }
  .tree-line {
    font-family: 'Consolas', monospace; font-size: 12px;
    padding: 1px 0; white-space: nowrap; line-height: 1.6;
    overflow: hidden; text-overflow: ellipsis;
  }
  .tree-line:hover { background: #252932; border-radius: 3px; }
  .tree-pre   { color: #2e3548; user-select: none; }
  .tree-fn    { font-weight: 600; }
  .tree-pub   { color: var(--accent); }
  .tree-priv  { color: var(--accent2); }
  .tree-lib   { color: #757575; font-style: italic; }
  .tree-mod   { font-size: 10px; margin-left: 5px; }
  .tree-desc  {
    font-family: 'Segoe UI', sans-serif; font-size: 11px;
    color: #4a5468; margin-left: 5px; font-style: italic;
    font-weight: normal;
  }
  .tree-badge {
    font-size: 10px; border-radius: 3px; padding: 0 4px;
    margin-left: 5px; font-family: 'Segoe UI', sans-serif; font-weight: normal;
  }
  .tree-cycle   { background: #2d1f1a; color: #ff7043; }
  .tree-maxd    { color: var(--muted); }
  .tree-empty   { color: var(--muted); font-style: italic; padding: 8px 0; }
  .tree-section { color: #3a4052; font-size: 11px;
                  border-top: 1px solid #1e2330; margin: 6px 0 4px;
                  padding-top: 4px; font-family: 'Segoe UI', sans-serif; }

  /* Called-by section */
  .calledby-row {
    font-family: 'Consolas', monospace; font-size: 12px;
    padding: 2px 3px; border-radius: 3px; cursor: pointer;
    line-height: 1.65; display: flex; align-items: baseline;
    gap: 4px; flex-wrap: wrap;
  }
  .calledby-row:hover { background: #252932; }
  .calledby-arrow { color: #2e3548; flex-shrink: 0; }
  .calledby-sep   { color: #2e3548; font-size: 10px; }
  .calledby-cov {
    font-family: 'Segoe UI', sans-serif; font-size: 10px;
    margin-left: 3px; flex-shrink: 0;
  }
  .calledby-cov-ok   { color: var(--green); }
  .calledby-cov-none { color: #3a4052; }
  .calledby-uc-tag {
    font-family: 'Segoe UI', sans-serif; font-size: 10px;
    background: #1b3a28; color: #5a9e6e;
    border-radius: 3px; padding: 0 4px; margin-left: 2px;
  }
  .calledby-more { font-size: 10px; color: var(--muted); padding: 2px 4px; }

  /* Call-site context badges */
  .ctx-unconditional {
    font-family: 'Segoe UI', sans-serif; font-size: 10px;
    color: #3d5a3e; background: #1a2e1b; border-radius: 3px;
    padding: 0 5px; margin-left: 4px; white-space: nowrap;
  }
  .ctx-conditional {
    font-family: 'Consolas', monospace; font-size: 10px;
    color: #b08a3e; background: #2a2010; border-radius: 3px;
    padding: 0 5px; margin-left: 4px; white-space: nowrap;
    max-width: 220px; overflow: hidden; text-overflow: ellipsis;
    display: inline-block; vertical-align: middle;
  }
  .ctx-mixed {
    font-family: 'Segoe UI', sans-serif; font-size: 10px;
    color: #4a7a9b; background: #102030; border-radius: 3px;
    padding: 0 5px; margin-left: 4px; white-space: nowrap;
  }
  .ctx-cond-list {
    margin-left: 22px; margin-bottom: 2px;
  }
  .ctx-cond-item {
    font-family: 'Consolas', monospace; font-size: 10px;
    color: #8a7040; padding: 1px 0;
    white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
    max-width: 320px; display: block;
  }
  .ctx-cond-item .ctx-branch {
    color: #6080a0; margin-right: 4px;
  }
  .calledby-nav-hint {
    font-family: 'Segoe UI', sans-serif; font-size: 10px;
    color: #2e3548; margin-left: auto; flex-shrink: 0;
  }
  .calledby-row:hover .calledby-nav-hint { color: var(--accent); }

  /* Right panel */
  #right-panel {
    width: var(--right-w); flex: 0 0 var(--right-w);
    background: var(--panel); border-left: 1px solid var(--border);
    padding: 14px; overflow-y: auto; font-size: 13px;
  }
  #right-panel h2 { font-size: 16px; color: var(--accent); margin-bottom: 3px; }
  #right-panel h3 {
    font-size: 10px; color: var(--accent2);
    text-transform: uppercase; letter-spacing: .06em;
    margin: 14px 0 6px; padding-bottom: 4px;
    border-bottom: 1px solid var(--border);
  }
  #module-role {
    color: var(--muted); font-size: 12px;
    white-space: pre-wrap; margin-bottom: 8px; line-height: 1.5;
  }
  .stats-bar { display: flex; gap: 6px; margin-bottom: 8px; flex-wrap: wrap; }
  .stat-chip {
    background: #2d3340; border: 1px solid var(--border);
    border-radius: 12px; padding: 2px 9px; font-size: 11px; color: var(--muted);
  }
  .stat-chip b { color: var(--text); }
  .cov-badge { padding: 2px 9px; border-radius: 12px; font-size: 11px; font-weight: 600; }
  .cov-high { background: #1b4332; color: #66bb6a; }
  .cov-mid  { background: #3d2b00; color: #ffb74d; }
  .cov-low  { background: #3d0d0d; color: #ef5350; }
  .cov-na   { background: #2d3340; color: var(--muted); }
  .dep-row {
    display: flex; align-items: center; gap: 6px;
    padding: 3px 6px; font-size: 12px; border-radius: 3px;
    border-bottom: 1px solid #2a2d35;
  }
  .dep-row:hover { background: #2d3340; }
  .dep-arrow { color: var(--muted); flex-shrink: 0; }
  .dep-link {
    color: var(--accent); cursor: pointer; font-weight: 600;
    text-decoration: none; flex: 1;
  }
  .dep-link:hover { text-decoration: underline; }
  .pill {
    display: inline-block; padding: 1px 7px; border-radius: 10px;
    font-size: 10px; margin-left: 4px; background: #2d3340; color: var(--muted);
  }
  #fn-search {
    width: 100%; background: #14161a; color: var(--text);
    border: 1px solid var(--border); border-radius: 4px;
    padding: 4px 7px; font-size: 11px; margin-bottom: 6px;
  }
  #fn-search:focus { outline: none; border-color: var(--accent); }
  .fn-row {
    padding: 5px 6px; border-radius: 4px; cursor: pointer;
    border-bottom: 1px solid #2a2d35;
  }
  .fn-row:hover { background: #2d3340; }
  .fn-row.seq-active {
    background: #1a2535;
    border-left: 2px solid var(--accent);
    padding-left: 4px;
  }
  .fn-row.pub  .fn-name { color: var(--accent); }
  .fn-row.priv .fn-name { color: var(--accent2); }
  .fn-name { font-weight: 600; font-family: 'Consolas', monospace; font-size: 12px; }
  .fn-hint { float: right; font-size: 10px; color: var(--border); margin-top: 1px; }
  .fn-row:hover .fn-hint, .fn-row.seq-active .fn-hint { color: var(--accent); }
  .fn-desc  { color: var(--muted); font-size: 11px; margin-top: 2px; clear: both; }
  .tag-req  { color: var(--green); font-size: 10px; margin-left: 4px; }
  .tag-cov-ok       { color: var(--green); font-size: 10px; }
  .tag-cov-tc       { color: var(--accent); font-size: 10px; font-weight: 600; } /* TC-level = highest precision */
  .tag-cov-indirect { color: #5a9e6e;      font-size: 10px; } /* indirect via uncond. parent */
  .tag-cov-cond     { color: #8a7040;      font-size: 10px; } /* potential via cond. branch  */
  .tag-cov-none     { color: var(--red);   font-size: 10px; }
  .grow {
    font-size: 11px; color: var(--muted); padding: 4px 6px;
    border-bottom: 1px solid #2a2d35; cursor: default;
  }
  .grow:hover { background: #2d3340; }
  .grow code {
    font-family: 'Consolas', monospace; color: var(--accent2);
    font-size: 10px; background: #1e2128; padding: 1px 3px; border-radius: 2px;
  }
  .grow b { color: var(--text); }
  #empty-hint { color: var(--muted); font-style: italic; }
  ::-webkit-scrollbar { width: 8px; }
  ::-webkit-scrollbar-thumb { background: #3a3f4b; border-radius: 4px; }
</style>
</head>
<body>
<div id="app">

  <!-- ══ LEFT PANEL ══ -->
  <div id="left-panel">
    <h1>ST4Ever</h1>
    <div class="lp-subtitle" id="graph-stats">Architecture interactive</div>

    <div class="lp-section">
      <div class="lp-section-title">Rechercher</div>
      <input class="lp-input" id="filterBox" placeholder="module ou fonction…">
    </div>

    <div class="lp-section">
      <div class="lp-section-title">Disposition</div>
      <div class="radio-group">
        <label><input type="radio" name="layout" value="fcose" checked> fcose (force-directed)</label>
        <label><input type="radio" name="layout" value="concentric"> concentric (anneaux)</label>
      </div>
    </div>

    <div class="lp-section">
      <div class="lp-section-title">Couleur des nœuds</div>
      <div class="radio-group">
        <label><input type="radio" name="colorMode" value="coverage" checked> Couverture UC</label>
        <label><input type="radio" name="colorMode" value="layer"> Couche fonctionnelle</label>
        <label><input type="radio" name="colorMode" value="degree"> Connectivité</label>
      </div>
    </div>

    <div class="lp-section">
      <div class="lp-section-title">Taille des nœuds</div>
      <div class="radio-group">
        <label><input type="radio" name="sizeMode" value="degree" checked> Degré (connexions)</label>
        <label><input type="radio" name="sizeMode" value="functions"> Nb fonctions</label>
        <label><input type="radio" name="sizeMode" value="uniform"> Uniforme</label>
      </div>
    </div>

    <div class="lp-section">
      <div class="lp-section-title">Arêtes visibles</div>
      <select class="lp-select" id="edgeFilter">
        <option value="all" selected>Toutes</option>
        <option value="3">Trafic ≥ 3 appels</option>
        <option value="5">Trafic ≥ 5 appels</option>
        <option value="10">Trafic ≥ 10 appels</option>
      </select>
    </div>

    <div class="lp-section">
      <div class="lp-section-title">Flux de contrôle</div>
      <div class="lp-row">
        <label>Profondeur</label>
        <select class="lp-select" id="cfDepth" style="width:auto; flex:1;">
          <option value="2">2 niveaux</option>
          <option value="3" selected>3 niveaux</option>
          <option value="4">4 niveaux</option>
          <option value="5">5 niveaux</option>
        </select>
      </div>
      <label class="radio-group" style="display:flex; align-items:center; gap:6px; padding:3px 5px; border-radius:4px; cursor:pointer; margin-top:4px;">
        <input type="checkbox" id="cfShowLib">
        <span style="font-size:12px; color:var(--text);">Inclure appels lib/COTS</span>
      </label>
      <div style="font-size:10px; color:var(--muted); margin-top:4px; line-height:1.4;">
        Cliquer une fonction dans le bandeau droit pour afficher son arbre d'appels.
      </div>
    </div>

    <div class="lp-spacer"></div>

    <div class="lp-section">
      <div class="lp-section-title">Positions</div>
      <button class="lp-btn" id="exportPosBtn">💾 Exporter positions</button>
      <div class="lp-btn-row">
        <button class="lp-btn" id="importPosBtn">📂 Importer</button>
        <button class="lp-btn" id="resetPosBtn">↺ Recalculer</button>
      </div>
      <input type="file" id="importPosFile" accept=".json" style="display:none">
      <div id="pos-status"></div>
    </div>
  </div>

  <!-- ══ CANVAS ══ -->
  <div id="canvas-area">
    <div id="cy"></div>
    <div id="legend"></div>
    <div id="tooltip"></div>
  </div>

  <!-- ══ RIGHT PANEL ══ -->
  <div id="right-panel">
    <div id="side-content">
      <p id="empty-hint">Survolez ou cliquez un module pour afficher son détail.</p>
    </div>
  </div>

</div>

<!-- ══ SEQUENCE PANEL (fixed, overlays canvas right edge) ══ -->
<div id="seq-panel">
  <div id="seq-header">
    <div id="seq-title-area">
      <span id="seq-label">FLUX D'APPELS</span>
      <span id="seq-fn"></span>
      <span id="seq-mod-badge"></span>
    </div>
    <button id="seq-close" title="Fermer">✕</button>
  </div>
  <div id="seq-options">
    <label>Profondeur</label>
    <select id="seqDepthLocal">
      <option value="2">2</option>
      <option value="3" selected>3</option>
      <option value="4">4</option>
      <option value="5">5</option>
    </select>
    <label style="display:flex; align-items:center; gap:4px; cursor:pointer;">
      <input type="checkbox" id="seqLibLocal"> appels lib
    </label>
  </div>
  <div id="seq-lib-status"></div>
  <div id="seq-body"></div>
</div>

<script>
const GRAPH = __GRAPH_JSON__;
const SAVED_POSITIONS = __SAVED_POSITIONS_JSON__;

var modules   = GRAPH.modules;
var edges     = GRAPH.cross_module_edges;
var ucCov     = GRAPH.uc_coverage;
var tcIntents = GRAPH.tc_intents  || {};   // TC-tag -> {desc, req_tags, ufr_tags, tested_fns, uc_file}
var fnTcMap   = GRAPH.fn_tc_map   || {};   // func_name -> [TC-tag, ...]
var funcIndex = GRAPH.func_index;

// ─── Helpers ────────────────────────────────────────────────────────────────
function esc(s) {
  return (s||'').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}

// ─── Module metadata ─────────────────────────────────────────────────────────
function moduleCoverageRatio(base) {
  var mod = modules[base];
  var all = mod.pub_names.concat(mod.internal.map(function(x){ return x[0]; }));
  if (!all.length) return null;
  var n = 0;
  all.forEach(function(f){ if (ucCov[f] && ucCov[f].length) n++; });
  return n / all.length;
}
function moduleFuncCount(base) {
  return modules[base].pub_names.length + modules[base].internal.length;
}

// ─── Static indirect coverage ────────────────────────────────────────────────
// For a static function with no direct UC calls, infer coverage from its parent
// callers and their call-site context (unconditional = certain, conditional = potential).
//
// Returns one of:
//   {mode:'direct',      ucs:[...]}                   — has direct UC calls
//   {mode:'indirect',    ucs:[...], condUcs:[...]}    — via uncond. parent(s)
//   {mode:'conditional', ucs:[...]}                   — only via cond. branch(es)
//   {mode:'none',        ucs:[]}                      — no coverage path
function computeStaticIndirectCov(fnName, mod) {
  var direct = ucCov[fnName] || [];
  if (direct.length) return { mode: 'direct', ucs: direct, condUcs: [] };

  var ctx = (mod && mod.static_ctx && mod.static_ctx[fnName]) || [];
  if (!ctx.length) return { mode: 'none', ucs: [], condUcs: [] };

  var uncondSet = {}, condSet = {};
  ctx.forEach(function(entry) {
    var parentUCs = ucCov[entry.parent] || [];
    if (entry.uncond > 0) {
      parentUCs.forEach(function(uc) { uncondSet[uc] = true; });
    }
    if (entry.cond && entry.cond.length > 0) {
      parentUCs.forEach(function(uc) { condSet[uc] = true; });
    }
  });

  var indirectUCs = Object.keys(uncondSet);
  // Conditional-only UCs: those not already covered unconditionally
  var condOnlyUCs = Object.keys(condSet).filter(function(uc) {
    return !uncondSet[uc];
  });

  if (indirectUCs.length) {
    return { mode: 'indirect', ucs: indirectUCs, condUcs: condOnlyUCs };
  }
  if (condOnlyUCs.length) {
    return { mode: 'conditional', ucs: condOnlyUCs, condUcs: [] };
  }
  return { mode: 'none', ucs: [], condUcs: [] };
}

// ─── Layer classification ─────────────────────────────────────────────────────
var LAYER_DEF = {
  CPU:'emul', ST:'emul', shifter:'emul', linea:'emul', tos:'emul',
  disasm:'asm', asm:'asm', annotate:'asm',
  image_st:'disk', image_msa:'disk', image_annot:'disk',
  sector_analysis:'disk', prg:'disk',
  exec:'exec', exec_cpu:'exec', exec_mem:'exec',
  exec_asm:'exec', exec_screen:'exec', exec_mon:'exec',
  edit_txt:'edit', edit_hex:'edit', dir:'edit', mount:'edit',
  gui:'gui', renderer:'gui',
  trace:'infra', common:'infra', file:'infra', console:'infra',
  line:'app', prefs:'app', image:'app',
};
var LAYER_COLOR = {
  emul:'#ef5350', asm:'#26c6da', disk:'#ffca28', exec:'#ab47bc',
  edit:'#42a5f5', gui:'#66bb6a', platform:'#4db6ac',
  infra:'#78909c', app:'#ff7043', other:'#8890a0',
};
var LAYER_LABEL = {
  emul:'Émulation', asm:'Assembleur', disk:'Disque/Format', exec:'Exécution',
  edit:'Éditeurs', gui:'GUI/Rendu', platform:'Plateforme',
  infra:'Infrastructure', app:'Application', other:'Autre',
};
function inferLayer(base) {
  if (LAYER_DEF[base]) return LAYER_DEF[base];
  var b = base.toLowerCase();
  if (b.indexOf('win_') === 0 || b.indexOf('lx_') === 0) return 'platform';
  return 'other';
}

// ─── Degree data ──────────────────────────────────────────────────────────────
var degW = {};
Object.keys(modules).forEach(function(b){ degW[b] = 0; });
edges.forEach(function(e){
  var w = e.calls.length;
  degW[e.src] = (degW[e.src]||0) + w;
  degW[e.dst] = (degW[e.dst]||0) + w;
});
var maxDeg = 1;
Object.values(degW).forEach(function(v){ if (v > maxDeg) maxDeg = v; });

function degreeBucket(d) {
  if (d === 0) return 0;
  var r = d / maxDeg;
  if (r >= 0.6) return 5;
  if (r >= 0.35) return 4;
  if (r >= 0.18) return 3;
  if (r >= 0.06) return 2;
  return 1;
}

// ─── Color / size ─────────────────────────────────────────────────────────────
var colorMode = 'coverage', sizeMode = 'degree';

function coverageColor(ratio) {
  if (ratio === null) return '#8890a0';
  if (ratio >= 0.66) return '#66bb6a';
  if (ratio >= 0.33) return '#ffb74d';
  return '#ef5350';
}
function degreeColor(w) {
  var t = Math.min(1, w / maxDeg);
  var r = Math.round(79  + (239 -  79) * t);
  var g = Math.round(195 + ( 83 - 195) * t);
  var b = Math.round(247 + ( 80 - 247) * t);
  return 'rgb('+r+','+g+','+b+')';
}
function nodeColor(base) {
  if (colorMode === 'layer')  return LAYER_COLOR[inferLayer(base)] || '#8890a0';
  if (colorMode === 'degree') return degreeColor(degW[base]||0);
  return coverageColor(moduleCoverageRatio(base));
}
function nodeSize(base) {
  if (sizeMode === 'functions') {
    var cnt = moduleFuncCount(base);
    return Math.max(16, Math.min(50, 14 + cnt * 0.9));
  }
  if (sizeMode === 'uniform') return 22;
  return 18 + 30 * Math.min(1, (degW[base]||0) / maxDeg);
}

// ─── Dependency maps ──────────────────────────────────────────────────────────
var modCalls    = {};
var modCalledBy = {};
Object.keys(modules).forEach(function(b){ modCalls[b]=[]; modCalledBy[b]=[]; });
edges.forEach(function(e){
  modCalls[e.src].push({mod:e.dst, calls:e.calls});
  modCalledBy[e.dst].push({mod:e.src, calls:e.calls});
});

// ─── Per-function call index (unified: cross_module_edges + intra call_graph) ─
// Maps caller_func -> [{fn, mod}] for ALL project function calls.
var fnCallIndex = {};

// 1) Cross-module calls (authoritative for the callee module)
edges.forEach(function(e) {
  e.calls.forEach(function(pair) {
    var caller = pair[0], callee = pair[1], calleeMod = e.dst;
    if (!fnCallIndex[caller]) fnCallIndex[caller] = {};
    fnCallIndex[caller][callee] = calleeMod;
  });
});
// 2) Intra-module call_graph (fills any entry missing from cross_module_edges)
Object.keys(modules).forEach(function(modBase) {
  var cg = modules[modBase].call_graph || {};
  Object.keys(cg).forEach(function(caller) {
    (cg[caller]||[]).forEach(function(callee) {
      if (!fnCallIndex[caller]) fnCallIndex[caller] = {};
      if (fnCallIndex[caller][callee] === undefined) {
        fnCallIndex[caller][callee] = funcIndex[callee] || modBase;
      }
    });
  });
});

// ─── Reverse call index: callee -> [{fn, mod}] ────────────────────────────────
// Includes static functions (they appear in call_graph but not in cross_module_edges).
var fnCalledByIndex = {};

// ─── Return type descriptions ────────────────────────────────────────────────
var RT_DESC = {
  'st_error_t': 'ST_NO_ERROR on success, ST_ERROR on failure',
  'st_bool_t':  'ST_TRUE or ST_FALSE',
  'void':       '(none)',
  'int':        'int',
  'size_t':     'size_t (byte count)',
  'st_u8_t':    'unsigned 8-bit',
  'st_u16_t':   'unsigned 16-bit',
  'st_u32_t':   'unsigned 32-bit',
  'st_i32_t':   'signed 32-bit',
};
function returnTypeDesc(t) {
  if (!t) return '';
  var clean = t.trim().replace(/\s+/g, ' ');
  return RT_DESC[clean] !== undefined ? RT_DESC[clean] : clean;
}
Object.keys(fnCallIndex).forEach(function(caller) {
  var callerMod = funcIndex[caller] || null;
  if (!callerMod) {
    // Fallback: scan modules for this caller (handles edge cases)
    Object.keys(modules).forEach(function(mb) {
      if (!callerMod &&
          (modules[mb].pub_names.indexOf(caller) >= 0 ||
           modules[mb].internal.some(function(x){ return x[0] === caller; }))) {
        callerMod = mb;
      }
    });
  }
  Object.keys(fnCallIndex[caller]).forEach(function(callee) {
    if (!fnCalledByIndex[callee]) fnCalledByIndex[callee] = [];
    var dup = fnCalledByIndex[callee].some(function(c){ return c.fn === caller; });
    if (!dup) fnCalledByIndex[callee].push({ fn: caller, mod: callerMod || '?' });
  });
});

// ─── Graph stats ──────────────────────────────────────────────────────────────
var nMods  = Object.keys(modules).length;
var nEdges = edges.length;
var nFuncs = Object.keys(funcIndex).length;
document.getElementById('graph-stats').textContent =
  nMods+' modules · '+nFuncs+' fonctions · '+nEdges+' dépendances';

// ─── Cytoscape nodes ──────────────────────────────────────────────────────────
var nodes = Object.keys(modules).map(function(base){
  return { data: {
    id: base, label: base,
    col: nodeColor(base), sz: nodeSize(base),
    ring: degreeBucket(degW[base]||0), dw: degW[base]||0,
  }};
});

var edgeElements = edges.map(function(e, i){
  return { data: {
    id: 'e'+i, source: e.src, target: e.dst,
    weight: e.calls.length, calls: e.calls,
    curveDist: e.src < e.dst ? 40 : -40,
  }};
});

var cy = cytoscape({
  container: document.getElementById('cy'),
  elements: { nodes: nodes, edges: edgeElements },
  style: [
    { selector: 'node', style: {
        'background-color':'data(col)', 'label':'data(label)',
        'color':'#d8dce2', 'font-size':11, 'text-valign':'bottom',
        'text-margin-y':4, 'width':'data(sz)', 'height':'data(sz)',
        'border-width':2, 'border-color':'#1a1d23',
    }},
    { selector: 'edge', style: {
        'width':'mapData(weight,1,20,1,6)', 'line-color':'#555b68',
        'target-arrow-color':'#555b68', 'target-arrow-shape':'triangle',
        'curve-style':'unbundled-bezier',
        'control-point-distances':'data(curveDist)',
        'control-point-weights':[0.5], 'opacity':0.5,
    }},
    { selector: '.highlighted', style: {
        'line-color':'#4fc3f7','target-arrow-color':'#4fc3f7','opacity':1,'width':4,
    }},
    { selector: '.dimmed',     style: { 'opacity':0.1  }},
    { selector: 'node.dimmed', style: { 'opacity':0.12 }},
    { selector: 'node.lit',    style: { 'opacity':1, 'border-width':3, 'border-color':'#4fc3f7' }},
    { selector: 'edge.lit',    style: { 'opacity':1   }},
    { selector: 'node.pinned', style: { 'border-color':'#4fc3f7','border-width':4 }},
    { selector: 'edge.hidden', style: { 'display':'none' }},
  ],
  layout: { name: 'preset' },
  wheelSensitivity: 0.25, minZoom: 0.08, maxZoom: 5,
});

function refreshNodeData() {
  cy.nodes().forEach(function(n){
    var base = n.id();
    n.data('col', nodeColor(base));
    n.data('sz',  nodeSize(base));
  });
}

// ─── Layout ───────────────────────────────────────────────────────────────────
function seedHubPositions() {
  var sorted = cy.nodes().sort(function(a,b){ return b.data('dw')-a.data('dw'); });
  var hubs = Math.min(6, sorted.length);
  for (var i=0;i<hubs;i++){
    var a=(i/hubs)*2*Math.PI;
    sorted[i].position({x:30*Math.cos(a),y:30*Math.sin(a)});
  }
  for (var j=hubs;j<sorted.length;j++){
    var a2=((j-hubs)/Math.max(1,sorted.length-hubs))*2*Math.PI;
    var rr=250+40*((j-hubs)%3);
    sorted[j].position({x:rr*Math.cos(a2),y:rr*Math.sin(a2)});
  }
}
var fcoseOk = true;
function runFcose() {
  if (!fcoseOk) { runConcentric(); return; }
  seedHubPositions();
  try {
    cy.layout({
      name:'fcose', quality:'proof', randomize:false, animate:false,
      nodeSeparation:60,
      idealEdgeLength:function(e){ return 80+220/(1+e.data('weight')); },
      nodeRepulsion:function(n){ return 3500+n.data('dw')*25; },
    }).run();
  } catch(err) { fcoseOk=false; runConcentric(); }
}
function runConcentric() {
  cy.layout({
    name:'concentric', concentric:function(n){ return n.data('ring'); },
    levelWidth:function(){ return 1; }, minNodeSpacing:8,
    equidistant:false, animate:false, startAngle:0,
  }).run();
}
function applyLayout(name) {
  if (name==='fcose') runFcose(); else runConcentric();
  cy.fit(undefined,30);
  setStatus('Disposition recalculée ('+name+').');
}

// ─── Edge filter ──────────────────────────────────────────────────────────────
function applyEdgeFilter(val) {
  if (val==='all') { cy.edges().removeClass('hidden'); return; }
  var thr = parseInt(val,10);
  cy.edges().forEach(function(e){ e.toggleClass('hidden', e.data('weight')<thr); });
}

// ─── Position persistence ─────────────────────────────────────────────────────
var LS_KEY = 'st4ever_arch_pos_v1';
function applySavedPositions(posMap) {
  var n=0;
  Object.keys(posMap).forEach(function(id){
    var nd=cy.getElementById(id);
    if (nd&&nd.length){ nd.position(posMap[id]); n++; }
  });
  return n;
}
function getCurrentPositions() {
  var out={};
  cy.nodes().forEach(function(n){ out[n.id()]={x:n.position('x'),y:n.position('y')}; });
  return out;
}
function saveLS() {
  try { localStorage.setItem(LS_KEY, JSON.stringify(getCurrentPositions())); } catch(e){}
}
function setStatus(msg) {
  var el=document.getElementById('pos-status');
  if (el) el.textContent=msg;
}

var usedSaved=false;
if (SAVED_POSITIONS && Object.keys(SAVED_POSITIONS).length) {
  var n0=applySavedPositions(SAVED_POSITIONS);
  cy.fit(undefined,30);
  setStatus(n0+' positions chargées depuis le fichier.');
  usedSaved=true;
} else {
  try {
    var stored=localStorage.getItem(LS_KEY);
    if (stored){ var n1=applySavedPositions(JSON.parse(stored)); cy.fit(undefined,30); setStatus(n1+' positions restaurées.'); usedSaved=true; }
  } catch(e){}
}
if (!usedSaved) applyLayout('fcose');

cy.on('dragfree','node',function(){ saveLS(); setStatus('Position sauvegardée.'); });

// ─── Left panel events ────────────────────────────────────────────────────────
document.querySelectorAll('input[name="layout"]').forEach(function(r){
  r.addEventListener('change', function(){ applyLayout(this.value); });
});
document.querySelectorAll('input[name="colorMode"]').forEach(function(r){
  r.addEventListener('change', function(){ colorMode=this.value; refreshNodeData(); renderLegend(); });
});
document.querySelectorAll('input[name="sizeMode"]').forEach(function(r){
  r.addEventListener('change', function(){ sizeMode=this.value; refreshNodeData(); });
});
document.getElementById('edgeFilter').addEventListener('change', function(){
  applyEdgeFilter(this.value);
});
document.getElementById('exportPosBtn').addEventListener('click', function(){
  var blob=new Blob([JSON.stringify(getCurrentPositions(),null,1)],{type:'application/json'});
  var a=document.createElement('a'); a.href=URL.createObjectURL(blob);
  a.download='st4ever_positions.json';
  document.body.appendChild(a); a.click(); document.body.removeChild(a);
  URL.revokeObjectURL(a.href); setStatus('Positions exportées.');
});
document.getElementById('importPosBtn').addEventListener('click', function(){
  document.getElementById('importPosFile').click();
});
document.getElementById('importPosFile').addEventListener('change', function(e){
  var file=e.target.files[0]; if (!file) return;
  var reader=new FileReader();
  reader.onload=function(){
    try { var n2=applySavedPositions(JSON.parse(reader.result)); cy.fit(undefined,30); saveLS(); setStatus(n2+' positions importées.'); }
    catch(err){ setStatus('Erreur: JSON invalide.'); }
  };
  reader.readAsText(file); e.target.value='';
});
document.getElementById('resetPosBtn').addEventListener('click', function(){
  try { localStorage.removeItem(LS_KEY); } catch(e){}
  var sel=document.querySelector('input[name="layout"]:checked');
  applyLayout(sel?sel.value:'fcose');
});

// Sync left-panel depth/lib with seq panel local controls (and vice versa)
document.getElementById('cfDepth').addEventListener('change', function(){
  document.getElementById('seqDepthLocal').value = this.value;
  refreshSeqPanel();
});
document.getElementById('cfShowLib').addEventListener('change', function(){
  document.getElementById('seqLibLocal').checked = this.checked;
  refreshSeqPanel();
});
document.getElementById('seqDepthLocal').addEventListener('change', function(){
  document.getElementById('cfDepth').value = this.value;
  refreshSeqPanel();
});
document.getElementById('seqLibLocal').addEventListener('change', function(){
  document.getElementById('cfShowLib').checked = this.checked;
  refreshSeqPanel();
});

// ─── Legend ───────────────────────────────────────────────────────────────────
function renderLegend() {
  var el=document.getElementById('legend');
  var html='';
  if (colorMode==='coverage') {
    [['#66bb6a','Couverture ≥ 66%'],['#ffb74d','33–65%'],
     ['#ef5350','< 33%'],['#8890a0','Non mesuré']].forEach(function(it){
      html+='<span class="legend-item"><span class="legend-dot" style="background:'+it[0]+'"></span>'+esc(it[1])+'</span>';
    });
  } else if (colorMode==='layer') {
    var seen={};
    Object.keys(modules).forEach(function(b){
      var lay=inferLayer(b);
      if (!seen[lay]){ seen[lay]=true;
        html+='<span class="legend-item"><span class="legend-dot" style="background:'+(LAYER_COLOR[lay]||'#8890a0')+'"></span>'+esc(LAYER_LABEL[lay]||lay)+'</span>';
      }
    });
  } else {
    html='<span class="legend-item"><span class="legend-dot" style="background:rgb(79,195,247)"></span>Faible connexité</span>';
    html+='<span class="legend-item"><span class="legend-dot" style="background:rgb(239,83,80)"></span>Hub (fort)</span>';
  }
  el.innerHTML=html;
}
renderLegend();

// ─── Tooltip (hover) ──────────────────────────────────────────────────────────
var tooltip=document.getElementById('tooltip');
function showTooltip(cx,cy_,html_) {
  tooltip.innerHTML=html_;
  tooltip.style.left='-9999px'; tooltip.style.top='-9999px';
  tooltip.style.display='block';
  var tw=tooltip.offsetWidth, th=tooltip.offsetHeight;
  var vw=window.innerWidth, vh=window.innerHeight;
  var tx=cx+16, ty=cy_+12;
  if (tx+tw>vw-10) tx=cx-tw-16;
  if (ty+th>vh-10) ty=cy_-th-12;
  tooltip.style.left=Math.max(10,tx)+'px';
  tooltip.style.top=Math.max(10,ty)+'px';
}
function hideTooltip() { tooltip.style.display='none'; }
function moveTooltip(cx,cy_) {
  if (tooltip.style.display!=='block') return;
  var tw=tooltip.offsetWidth, th=tooltip.offsetHeight;
  var vw=window.innerWidth, vh=window.innerHeight;
  var tx=cx+16, ty=cy_+12;
  if (tx+tw>vw-10) tx=cx-tw-16;
  if (ty+th>vh-10) ty=cy_-th-12;
  tooltip.style.left=Math.max(10,tx)+'px';
  tooltip.style.top=Math.max(10,ty)+'px';
}
document.getElementById('canvas-area').addEventListener('mousemove', function(e){
  moveTooltip(e.clientX, e.clientY);
});

// ─── Node hover (highlight neighbourhood + tooltip) ───────────────────────────
cy.on('mouseover','node',function(evt){
  var base=evt.target.id(), mod=modules[base];
  var ratio=moduleCoverageRatio(base);
  var covTxt=ratio===null?'n/a':Math.round(ratio*100)+'%';
  var role1=(mod.role||'').split('\n')[0].slice(0,150);
  var out=(modCalls[base]||[]).length, inp=(modCalledBy[base]||[]).length;
  showTooltip(evt.originalEvent.clientX, evt.originalEvent.clientY,
    '<h4>'+esc(base)+'</h4>'+
    '<div class="row muted">'+esc(role1)+(role1.length===150?'…':'')+'</div>'+
    '<table>'+
    '<tr><td>API publique</td><td><b>'+mod.pub_names.length+'</b></td></tr>'+
    '<tr><td>Statiques</td><td><b>'+mod.internal.length+'</b></td></tr>'+
    '<tr><td>Globales</td><td><b>'+Object.keys(mod.globals).length+'</b></td></tr>'+
    '<tr><td>Couverture UC</td><td><b>'+esc(covTxt)+'</b></td></tr>'+
    (out?'<tr><td>→ appelle</td><td><b>'+out+' module(s)</b></td></tr>':'')+
    (inp?'<tr><td>← appelé par</td><td><b>'+inp+' module(s)</b></td></tr>':'')+
    '</table>'+
    '<div class="row muted" style="margin-top:5px;font-style:italic;">Clic pour épingler →</div>'
  );
  cy.elements().addClass('dimmed');
  evt.target.removeClass('dimmed').addClass('lit');
  evt.target.connectedEdges().removeClass('dimmed').addClass('highlighted lit');
  evt.target.neighborhood('node').removeClass('dimmed').addClass('lit');
});
cy.on('mouseout','node',function(){
  cy.elements().removeClass('dimmed highlighted lit');
  hideTooltip();
});

// ─── Edge hover ───────────────────────────────────────────────────────────────
cy.on('mouseover','edge',function(evt){
  var calls=evt.target.data('calls');
  var src=evt.target.data('source'), dst=evt.target.data('target');
  var rows=calls.slice(0,14).map(function(c){
    return '<div class="row"><code>'+esc(c[0])+'()</code> → <code>'+esc(c[1])+'()</code></div>';
  }).join('');
  var more=calls.length>14?'<div class="row muted">… +'+(calls.length-14)+' autres</div>':'';
  showTooltip(evt.originalEvent.clientX, evt.originalEvent.clientY,
    '<h4>'+esc(src)+' → '+esc(dst)+'</h4>'+
    '<div class="row muted">'+calls.length+' appel(s) :</div>'+rows+more);
  evt.target.addClass('highlighted');
  cy.elements().not(evt.target).addClass('dimmed');
});
cy.on('mouseout','edge',function(){
  cy.elements().removeClass('highlighted dimmed'); hideTooltip();
});

// ─── Right panel ──────────────────────────────────────────────────────────────
var sideContent=document.getElementById('side-content');
var pinnedBase=null, currentBase=null;

function covBadge(ratio) {
  if (ratio===null) return '<span class="cov-badge cov-na">UC: n/a</span>';
  var pct=Math.round(ratio*100);
  if (pct>=66) return '<span class="cov-badge cov-high">UC: '+pct+'%</span>';
  if (pct>=33) return '<span class="cov-badge cov-mid">UC: '+pct+'%</span>';
  return '<span class="cov-badge cov-low">UC: '+pct+'%</span>';
}

function renderModuleDetail(base) {
  currentBase=base;
  var mod=modules[base], ratio=moduleCoverageRatio(base);
  var html='<h2>'+esc(base)+'</h2>';
  var roleLines=(mod.role||'').split('\n').slice(0,4).join('\n');
  html+='<div id="module-role">'+esc(roleLines)+'</div>';
  // Count how many functions in this module have TC-level traceability
  var allFns = mod.pub_names.concat(mod.internal.map(function(x){ return x[0]; }));
  var tcFnCount = allFns.filter(function(fn){ return (fnTcMap[fn]||[]).length > 0; }).length;
  var tcTotal   = allFns.reduce(function(acc, fn){ return acc + (fnTcMap[fn]||[]).length; }, 0);

  html+='<div class="stats-bar">';
  html+='<span class="stat-chip"><b>'+mod.pub_names.length+'</b> pub</span>';
  html+='<span class="stat-chip"><b>'+mod.internal.length+'</b> static</span>';
  html+='<span class="stat-chip"><b>'+Object.keys(mod.globals).length+'</b> globales</span>';
  html+=covBadge(ratio);
  // TC chip — always visible: shows Phase 2 documentation progress
  // Blue = some TC traceability done, grey = Phase 2 pending
  var tcStyle = tcFnCount > 0
    ? 'border-color:var(--accent); color:var(--accent);'
    : 'border-color:var(--border); color:var(--muted);';
  var tcTitle = tcFnCount > 0
    ? tcFnCount+'/'+allFns.length+' fonctions avec traçabilite TC ('+tcTotal+' paires fn-TC)'
    : 'Phase 2 (INTENT/TC/REQ) non effectuee pour ce module';
  html+='<span class="stat-chip" title="'+esc(tcTitle)+'" style="'+tcStyle+'">'+
        '<b>'+tcFnCount+'/'+allFns.length+'</b> TC</span>';
  html+='</div>';

  // Dependencies
  var outDeps=(modCalls[base]||[]).slice().sort(function(a,b){ return b.calls.length-a.calls.length; });
  var inDeps=(modCalledBy[base]||[]).slice().sort(function(a,b){ return b.calls.length-a.calls.length; });
  if (outDeps.length||inDeps.length) {
    html+='<h3>Dépendances</h3>';
    if (outDeps.length) {
      html+='<div style="font-size:11px;color:var(--muted);margin-bottom:3px;">→ Appelle ('+outDeps.length+')</div>';
      outDeps.slice(0,10).forEach(function(d){
        html+='<div class="dep-row"><span class="dep-arrow">→</span><a class="dep-link" data-goto="'+esc(d.mod)+'">'+esc(d.mod)+'</a><span class="pill">'+d.calls.length+' appels</span></div>';
      });
      if (outDeps.length>10) html+='<div style="font-size:11px;color:var(--muted);padding:2px 6px;">… +'+(outDeps.length-10)+' autres</div>';
    }
    if (inDeps.length) {
      html+='<div style="font-size:11px;color:var(--muted);margin:8px 0 3px;">← Appelé par ('+inDeps.length+')</div>';
      inDeps.slice(0,10).forEach(function(d){
        html+='<div class="dep-row"><span class="dep-arrow">←</span><a class="dep-link" data-goto="'+esc(d.mod)+'">'+esc(d.mod)+'</a><span class="pill">'+d.calls.length+' appels</span></div>';
      });
      if (inDeps.length>10) html+='<div style="font-size:11px;color:var(--muted);padding:2px 6px;">… +'+(inDeps.length-10)+' autres</div>';
    }
  }

  var totalFns=mod.pub_names.length+mod.internal.length;
  if (totalFns>0) {
    html+='<h3>Fonctions ('+totalFns+') <span style="font-size:9px;color:var(--muted);font-weight:normal;">(clic → flux d\'appels)</span></h3>';
    if (totalFns>7) html+='<input id="fn-search" class="lp-input" placeholder="Rechercher une fonction…">';
    html+=mod.pub_names.map(function(n){ return renderFnRow(n,true,mod); }).join('');
    html+=mod.internal.map(function(x){ return renderFnRow(x[0],false,mod); }).join('');
  }
  html+=renderGlobals(mod.globals);
  sideContent.innerHTML=html;
}

function renderFnRow(name, isPub, mod) {
  var info = mod.docs[name];
  var ie   = mod.internal.find(function(x){ return x[0]===name; });
  var iSig = (mod.internal_sigs || {})[name];
  var desc = info ? info.desc : (ie ? ie[1] : '');
  var reqs = info ? info.reqs : (ie ? (ie[2]||[]) : []);
  var reqH = (reqs && reqs.length)
    ? '<span class="tag-req">'+esc(reqs.join(', '))+'</span>' : '';

  // Coverage badge — TC level > UC direct > indirect/conditional
  var covH = '';
  var fnTCs = fnTcMap[name] || [];
  var directCov = ucCov[name] || [];
  if (fnTCs.length) {
    // TC-level traceability (Phase 2 documentation done for this function)
    var tcLabel = fnTCs.length === 1
      ? fnTCs[0]
      : fnTCs.length <= 3 ? fnTCs.join(', ') : fnTCs[0] + ' +' + (fnTCs.length-1);
    covH = '<span class="tag-cov-tc">✓ ' + esc(tcLabel) + '</span>';
  } else if (directCov.length) {
    // Direct UC calls (no TC annotation yet)
    covH = '<span class="tag-cov-ok">✓ '+directCov.length+' UC</span>';
  } else if (!isPub) {
    // Static function: compute indirect coverage via parent call contexts
    var icov = computeStaticIndirectCov(name, mod);
    if (icov.mode === 'indirect') {
      // Covered via unconditional parent(s) — certain
      covH = '<span class="tag-cov-indirect">~ '+icov.ucs.length+' UC</span>';
      if (icov.condUcs && icov.condUcs.length) {
        // Some additional conditional-only paths
        covH += ' <span class="tag-cov-cond">≈ +'+icov.condUcs.length+'</span>';
      }
    } else if (icov.mode === 'conditional') {
      // Only reachable via conditional branches — potential
      covH = '<span class="tag-cov-cond">≈ '+icov.ucs.length+' UC</span>';
    } else {
      covH = '<span class="tag-cov-none">✗</span>';
    }
  } else {
    // Public function with no direct UC calls
    covH = '<span class="tag-cov-none">✗</span>';
  }

  // Signature hint for static functions (return type + param count)
  var sigHint = '';
  if (!isPub && iSig) {
    var pCount = iSig.params ? iSig.params.length : 0;
    var pStr   = pCount === 0 ? 'void' : pCount === 1 ? '1 param' : pCount+' params';
    sigHint = ' <span style="font-size:10px;color:#3a4a5a;font-family:\'Consolas\',monospace;">'+
              esc(iSig.return_type||'void')+' ('+pStr+')</span>';
  }

  return '<div class="fn-row '+(isPub?'pub':'priv')+'" data-kind="fn" data-fn="'+esc(name)+'">'+
    '<span class="fn-hint">⊕</span>'+
    '<span class="fn-name">'+esc(name)+'()</span>'+reqH+' '+covH+sigHint+'<br>'+
    '<span class="fn-desc">'+esc(desc||'—')+'</span>'+
    '</div>';
}

function renderGlobals(globals) {
  var names=Object.keys(globals);
  if (!names.length) return '';
  var rows=names.map(function(n){
    var g=globals[n];
    var w=g.writers.length?'<b>W:</b> '+esc(g.writers.map(function(f){return f+'()';}).join(', ')):'';
    var r=g.readers.length?'<b>R:</b> '+esc(g.readers.map(function(f){return f+'()';}).join(', ')):'';
    return '<div class="grow" data-kind="gv" data-gv="'+esc(n)+'">'+
      '<code>'+esc(n)+'</code><span class="pill">'+esc(g.type)+'</span>'+
      (g.is_const?'<span class="pill">const</span>':'')+
      ((w||r)?'<div style="margin-top:2px;">'+w+(w&&r?' &nbsp; ':'')+r+'</div>':'')+
      '</div>';
  }).join('');
  return '<h3>Variables globales ('+names.length+')</h3>'+rows;
}

// ─── Side panel hover tooltips ────────────────────────────────────────────────
function buildFnTooltip(fnName) {
  var mod  = modules[currentBase]; if (!mod) return '';
  var info = mod.docs[fnName];                                   // from .h docstring
  var ie   = mod.internal.find(function(x){ return x[0]===fnName; }); // inline comment
  var iSig = (mod.internal_sigs || {})[fnName];                  // extracted signature

  var desc     = info ? info.desc     : (ie ? ie[1]    : '');
  var reqs     = info ? info.reqs     : (ie ? (ie[2]||[]) : []);
  var isStatic = !!ie && !info;

  var html = '<h4>'+esc(fnName)+'() '+
    '<span class="muted" style="font-size:11px;">'+(isStatic ? 'static' : 'API publique')+'</span></h4>';
  if (desc) html += '<div class="row">'+esc(desc)+'</div>';
  if (reqs && reqs.length) html += '<div class="row tag-req">'+esc(reqs.join(', '))+'</div>';

  // ── Parameters ──────────────────────────────────────────────────────────────
  var hasDocParams = info && info.params && info.params.length > 0;
  var hasSigParams = !hasDocParams && iSig && iSig.params && iSig.params.length > 0;

  if (hasDocParams) {
    // Full docstring params: [name, dir, description]
    html += '<div class="row muted" style="margin-top:6px;"><b>Paramètres :</b></div><table>';
    info.params.forEach(function(p) {
      html += '<tr>'+
        '<td><code>'+esc(p[0])+'</code></td>'+
        '<td><span class="muted">('+esc(p[1]||'')+')</span></td>'+
        '<td>'+esc(p[2]||'')+'</td>'+
        '</tr>';
    });
    html += '</table>';
  } else if (hasSigParams) {
    // Extracted signature params: {name, type, dir} — no text description, show type
    html += '<div class="row muted" style="margin-top:6px;">'+
      '<b>Paramètres</b> <span style="font-size:10px;">(extraits de la signature C)</span> :'+
      '</div><table>';
    iSig.params.forEach(function(p) {
      html += '<tr>'+
        '<td><code>'+esc(p.name)+'</code></td>'+
        '<td><span class="muted">('+esc(p.dir)+')</span></td>'+
        '<td><code style="color:var(--accent2);font-size:10px;">'+esc(p.type)+'</code></td>'+
        '</tr>';
    });
    html += '</table>';
  } else if (isStatic && !iSig) {
    html += '<div class="row muted" style="margin-top:5px; font-size:11px;">Paramètres : non disponibles (régénérer gen_graph.py)</div>';
  }

  // ── Return type ─────────────────────────────────────────────────────────────
  if (info && info.returns && info.returns.length) {
    html += '<div class="row muted" style="margin-top:4px;"><b>Retour :</b></div>';
    info.returns.forEach(function(r) { html += '<div class="row">'+esc(r)+'</div>'; });
  } else if (iSig && iSig.return_type) {
    var rdesc = returnTypeDesc(iSig.return_type);
    html += '<div class="row muted" style="margin-top:4px;"><b>Retour :</b> '+
      '<code style="color:var(--accent2);font-size:10px;">'+esc(iSig.return_type)+'</code>'+
      (rdesc !== iSig.return_type.trim() ? ' <span class="muted">— '+esc(rdesc)+'</span>' : '')+
      '</div>';
  }

  // ── Globals ──────────────────────────────────────────────────────────────────
  var writes = [], reads = [];
  Object.keys(mod.globals).forEach(function(g) {
    if      (mod.globals[g].writers.indexOf(fnName) >= 0) writes.push(g);
    else if (mod.globals[g].readers.indexOf(fnName) >= 0) reads.push(g);
  });
  if (writes.length || reads.length) {
    html += '<div class="row muted" style="margin-top:6px;"><b>Globales :</b></div>';
    if (writes.length) html += '<div class="row">W: '+writes.map(function(g){ return '<code>'+esc(g)+'</code>'; }).join(', ')+'</div>';
    if (reads.length)  html += '<div class="row">R: '+reads.map(function(g){ return '<code>'+esc(g)+'</code>'; }).join(', ')+'</div>';
  }

  // ── TC / UC coverage ─────────────────────────────────────────────────────────
  var fnTCs = fnTcMap[fnName] || [];
  if (fnTCs.length) {
    // TC-level traceability: show each TC tag with its intent description
    html += '<div class="row muted" style="margin-top:6px;"><b>✓ Traçabilité TC (' +
      fnTCs.length + ') :</b></div>';
    html += '<table style="margin-top:3px;">';
    fnTCs.slice(0, 6).forEach(function(tc) {
      var ti = tcIntents[tc];
      var desc = ti ? ti.desc.slice(0, 70) + (ti.desc.length > 70 ? '…' : '') : '';
      var reqs = ti && ti.req_tags && ti.req_tags.length
        ? ' <span class="tag-req">' + esc(ti.req_tags.slice(0,2).join(', ')) + '</span>' : '';
      html += '<tr>'+
        '<td style="color:var(--accent);font-weight:600;white-space:nowrap;">' + esc(tc) + reqs + '</td>'+
        '<td style="color:var(--muted);">' + esc(desc) + '</td>'+
        '</tr>';
    });
    if (fnTCs.length > 6) {
      html += '<tr><td colspan="2" style="color:var(--muted);">… +' + (fnTCs.length-6) + ' autres</td></tr>';
    }
    html += '</table>';
  }

  // Also show UC-level as context (where TC coverage exists, UC files are the source)
  var icov = computeStaticIndirectCov(fnName, mod);

  if (!fnTCs.length && icov.mode === 'direct') {
    // Direct calls from use_cases/ but no TC annotation yet
    html += '<div class="row muted" style="margin-top:6px;"><b>✓ UC (sans INTENT) :</b> '+
      esc(icov.ucs.slice(0,8).join(', '))+(icov.ucs.length>8?' (+' + (icov.ucs.length-8)+')':'')+
      '</div>';
    html += '<div class="row muted" style="font-size:10px;">Phase 2 non effectuée pour ce module — INTENT/TC non renseignés.</div>';
  } else if (!fnTCs.length && icov.mode === 'indirect') {
    // Covered via unconditional parent(s) — reliable
    html += '<div class="row tag-cov-indirect" style="margin-top:6px;">'+
      '~ couverture via flux principal ('+icov.ucs.length+' UC) :</div>';
    html += '<div class="row muted">'+esc(icov.ucs.slice(0,6).join(', '))+(icov.ucs.length>6?'…':'')+'</div>';
    if (icov.condUcs && icov.condUcs.length) {
      html += '<div class="row tag-cov-cond" style="margin-top:3px;">'+
        '≈ potentiel via branche conditionnelle (+'+icov.condUcs.length+' UC)</div>';
    }
  } else if (icov.mode === 'conditional') {
    // Only reachable through conditional branches
    html += '<div class="row tag-cov-cond" style="margin-top:6px;">'+
      '≈ potentiel via branche conditionnelle ('+icov.ucs.length+' UC) :</div>';
    html += '<div class="row muted">'+esc(icov.ucs.slice(0,6).join(', '))+(icov.ucs.length>6?'…':'')+'</div>';
    html += '<div class="row muted" style="font-size:10px;">La TC du parent doit activer la bonne branche.</div>';
  } else {
    html += '<div class="row tag-cov-none" style="margin-top:5px;">Aucun chemin de couverture détecté.</div>';
  }

  html += '<div class="row muted" style="margin-top:5px;font-style:italic;font-size:11px;">Cliquer pour ouvrir le flux d\'appels ⊕</div>';
  return html;
}

function buildGvTooltip(gvName) {
  var mod=modules[currentBase]; if (!mod) return '';
  var g=mod.globals[gvName]; if (!g) return '';
  var html='<h4><code>'+esc(gvName)+'</code></h4>';
  html+='<div class="row muted">'+esc(g.type)+(g.is_const?' (const)':'')+(g.is_static?' (static)':'')+'</div>';
  html+='<div class="row" style="margin-top:6px;"><b>Écrit par :</b> '+(g.writers.length?esc(g.writers.map(function(f){return f+'()';}).join(', ')):'<span class="muted">jamais</span>')+'</div>';
  html+='<div class="row"><b>Lu par :</b> '+(g.readers.length?esc(g.readers.map(function(f){return f+'()';}).join(', ')):'<span class="muted">jamais</span>')+'</div>';
  return html;
}

sideContent.addEventListener('mouseover', function(e){
  var row=e.target.closest('[data-kind]'); if (!row) return;
  var html='';
  if (row.dataset.kind==='fn') html=buildFnTooltip(row.dataset.fn);
  else if (row.dataset.kind==='gv') html=buildGvTooltip(row.dataset.gv);
  if (html) showTooltip(e.clientX, e.clientY, html);
});
sideContent.addEventListener('mousemove', function(e){ moveTooltip(e.clientX, e.clientY); });
sideContent.addEventListener('mouseout', function(e){
  var row=e.target.closest('[data-kind]');
  if (!row||row.contains(e.relatedTarget)) return;
  hideTooltip();
});
sideContent.addEventListener('click', function(e){
  var depLink=e.target.closest('.dep-link');
  if (depLink&&depLink.dataset.goto){ gotoModule(depLink.dataset.goto); return; }
  var fnRow=e.target.closest('.fn-row');
  if (fnRow&&fnRow.dataset.fn&&currentBase){ openSeqPanel(fnRow.dataset.fn, currentBase); return; }
});
sideContent.addEventListener('input', function(e){
  if (e.target.id==='fn-search') {
    var q=e.target.value.toLowerCase();
    document.querySelectorAll('.fn-row').forEach(function(r){
      r.style.display=(!q||r.dataset.fn.toLowerCase().indexOf(q)>=0)?'':'none';
    });
  }
});

// ─── Node click (pin) ────────────────────────────────────────────────────────
cy.on('click','node',function(evt){
  if (pinnedBase) cy.getElementById(pinnedBase).removeClass('pinned');
  pinnedBase=evt.target.id();
  evt.target.addClass('pinned');
  renderModuleDetail(pinnedBase);
});
cy.on('click',function(evt){
  if (evt.target===cy){
    if (pinnedBase) cy.getElementById(pinnedBase).removeClass('pinned');
    pinnedBase=null; currentBase=null;
    sideContent.innerHTML='<p id="empty-hint">Survolez ou cliquez un module pour afficher son détail.</p>';
  }
});

function gotoModule(base) {
  var nd=cy.getElementById(base);
  if (!nd||!nd.length) return;
  cy.animate({center:{eles:nd},zoom:cy.zoom()},{duration:350});
  if (pinnedBase) cy.getElementById(pinnedBase).removeClass('pinned');
  pinnedBase=base; nd.addClass('pinned');
  renderModuleDetail(base);
}

// ─── Filter box ───────────────────────────────────────────────────────────────
document.getElementById('filterBox').addEventListener('input', function(e){
  var q=e.target.value.trim().toLowerCase();
  if (!q){ cy.elements().removeClass('dimmed'); return; }
  cy.nodes().forEach(function(n){
    var base=n.id(), mod=modules[base];
    var names=[base].concat(mod.pub_names).concat(mod.internal.map(function(x){ return x[0]; }));
    n.toggleClass('dimmed', !names.some(function(nm){ return nm.toLowerCase().indexOf(q)>=0; }));
  });
  cy.edges().forEach(function(ed){
    ed.toggleClass('dimmed', ed.source().hasClass('dimmed')||ed.target().hasClass('dimmed'));
  });
});

// ═══════════════════════════════════════════════════════════════════════════════
// ─── SEQUENCE / CALL-TREE PANEL ───────────────────────────────────────────────
// ═══════════════════════════════════════════════════════════════════════════════

var seqActiveFn = null, seqActiveMod = null;

// Check if ext_calls data is present in JSON
var hasExtCalls = Object.keys(modules).some(function(b){
  return modules[b].ext_calls && Object.keys(modules[b].ext_calls).length > 0;
});

// ─── Build call tree ──────────────────────────────────────────────────────────
var treeNodeCount = 0;
var MAX_TREE_NODES = 200;

function buildCallTree(fnName, modBase, depth, visitedArr) {
  var node = { name: fnName, mod: modBase, isLib: false, children: [],
               cycle: false, maxDepth: false };
  treeNodeCount++;
  if (treeNodeCount > MAX_TREE_NODES) {
    node.maxDepth = true; return node;
  }
  if (visitedArr.indexOf(fnName) >= 0) {
    node.cycle = true; return node;
  }
  if (depth <= 0) {
    node.maxDepth = true; return node;
  }
  var newVisited = visitedArr.concat([fnName]);
  var calls = fnCallIndex[fnName];
  if (calls) {
    Object.keys(calls).forEach(function(callee) {
      node.children.push(buildCallTree(callee, calls[callee], depth-1, newVisited));
    });
  }
  var cfShowLib = document.getElementById('seqLibLocal').checked;
  if (cfShowLib) {
    var mod = modules[modBase];
    var extC = (mod && mod.ext_calls && mod.ext_calls[fnName]) || [];
    extC.forEach(function(libName) {
      node.children.push({ name: libName, mod: 'extern', isLib: true, children: [] });
    });
  }
  return node;
}

// ─── Render one tree node ─────────────────────────────────────────────────────
function renderTreeNode(node, originMod, prefix, isLast) {
  var connector = isLast ? '└── ' : '├── ';
  var childPfx  = prefix + (isLast ? '    ' : '│   ');

  var fnClass = 'tree-fn ';
  var modBadge = '';
  if (node.isLib) {
    fnClass += 'tree-lib';
    modBadge = ' <span class="tree-mod" style="color:#424242;">extern</span>';
  } else {
    var mod = modules[node.mod];
    var isPub = mod && mod.pub_names.indexOf(node.name) >= 0;
    fnClass += isPub ? 'tree-pub' : 'tree-priv';
    if (node.mod && node.mod !== originMod) {
      var layColor = LAYER_COLOR[inferLayer(node.mod)] || '#8890a0';
      modBadge = ' <span class="tree-mod" style="color:'+layColor+'">'+esc(node.mod)+'</span>';
    }
  }

  var descHtml = '';
  if (!node.isLib && !node.cycle && !node.maxDepth) {
    var modInfo = modules[node.mod];
    if (modInfo && modInfo.docs[node.name] && modInfo.docs[node.name].desc) {
      var d = modInfo.docs[node.name].desc.slice(0, 55);
      descHtml = ' <span class="tree-desc">'+esc(d)+(d.length===55?'…':'')+'</span>';
    }
  }

  var badges = '';
  if (node.cycle)    badges += ' <span class="tree-badge tree-cycle">↩ cycle</span>';
  if (node.maxDepth) badges += ' <span class="tree-badge tree-maxd">…</span>';

  var html = '<div class="tree-line">'+
    '<span class="tree-pre">'+esc(prefix+connector)+'</span>'+
    '<span class="'+fnClass+'">'+esc(node.name)+'()</span>'+
    modBadge+descHtml+badges+
    '</div>';

  if (!node.cycle && !node.maxDepth) {
    node.children.forEach(function(child, i){
      html += renderTreeNode(child, originMod, childPfx, i===node.children.length-1);
    });
  }
  return html;
}

// ─── Build and render full tree for a function ────────────────────────────────
function renderSeqTree(fnName, modBase) {
  var depth = parseInt(document.getElementById('seqDepthLocal').value, 10) || 3;
  var mod = modules[modBase];
  var isPub = mod && mod.pub_names.indexOf(fnName) >= 0;
  var fnClass = isPub ? 'tree-pub' : 'tree-priv';
  var layColor = LAYER_COLOR[inferLayer(modBase)] || '#8890a0';
  var rootDesc = (mod && mod.docs[fnName] && mod.docs[fnName].desc) || '';
  var html = '';

  // ── Section "Appelé par" ────────────────────────────────────────────────────
  // Build a call-context lookup from static_ctx (Python pre-computed data).
  // static_ctx[fnName] = [{parent, uncond, cond: [{branch, condition}]}]
  // Only available for static functions (isPub === false).
  var ctxByParent = {};   // parent_name -> {uncond, cond}
  if (!isPub && mod && mod.static_ctx && mod.static_ctx[fnName]) {
    mod.static_ctx[fnName].forEach(function(entry) {
      ctxByParent[entry.parent] = { uncond: entry.uncond, cond: entry.cond || [] };
    });
  }

  var callers = (fnCalledByIndex[fnName] || []).slice();
  // Sort: unconditional callers first, then by UC coverage count, then alpha
  callers.sort(function(a, b) {
    var aCtx  = ctxByParent[a.fn], bCtx = ctxByParent[b.fn];
    var aUnc  = aCtx ? aCtx.uncond : 0, bUnc = bCtx ? bCtx.uncond : 0;
    if (bUnc !== aUnc) return bUnc - aUnc;  // unconditional first
    var ca = (ucCov[a.fn]||[]).length, cb = (ucCov[b.fn]||[]).length;
    if (cb !== ca) return cb - ca;
    return a.fn < b.fn ? -1 : 1;
  });

  if (callers.length > 0) {
    // Direct UC coverage of THIS function
    var directCov = ucCov[fnName] || [];
    var covSummary = directCov.length
      ? '<span class="calledby-cov calledby-cov-ok">✓ '+directCov.slice(0,4).join(', ')+(directCov.length>4?' (+' +(directCov.length-4)+')':'')+'</span>'
      : '<span class="calledby-cov calledby-cov-none">✗ aucun appel direct use_cases/</span>';
    html += '<div style="margin-bottom:8px;">'+
      '<div style="font-size:10px;color:var(--muted);margin-bottom:3px;font-weight:600;letter-spacing:.06em;text-transform:uppercase;">Couverture directe</div>'+
      covSummary+'</div>';

    html += '<div style="font-size:10px;color:var(--muted);margin-bottom:4px;font-weight:600;letter-spacing:.06em;text-transform:uppercase;">'+
      'Appelé par ('+callers.length+') <span style="font-weight:normal;text-transform:none;letter-spacing:0;">— clic pour naviguer</span></div>';

    callers.slice(0, 25).forEach(function(c) {
      var cmod   = modules[c.mod];
      var cIsPub = cmod && cmod.pub_names.indexOf(c.fn) >= 0;
      var cClass = cIsPub ? 'tree-pub' : 'tree-priv';
      var cColor = LAYER_COLOR[inferLayer(c.mod)] || '#8890a0';
      var cov    = ucCov[c.fn] || [];
      var ctx    = ctxByParent[c.fn];   // may be undefined for pub callers

      // UC coverage tags
      var covHtml = '';
      if (cov.length) {
        covHtml = cov.slice(0, 4).map(function(uc){
          return '<span class="calledby-uc-tag">'+esc(uc)+'</span>';
        }).join(' ');
        if (cov.length > 4) covHtml += ' <span class="calledby-cov calledby-cov-ok">(+' + (cov.length-4)+')</span>';
      } else {
        covHtml = '<span class="calledby-cov calledby-cov-none">✗</span>';
      }

      // Call-site context badge
      var ctxBadge = '';
      var ctxDetail = '';
      if (ctx !== undefined) {
        var hasUncond = ctx.uncond > 0;
        var hasCond   = ctx.cond.length > 0;
        if (hasUncond && !hasCond) {
          ctxBadge = '<span class="ctx-unconditional">flux principal</span>';
        } else if (hasUncond && hasCond) {
          ctxBadge = '<span class="ctx-mixed">mixte</span>';
        } else if (!hasUncond && hasCond) {
          // Show up to 2 conditions inline
          var shown = ctx.cond.slice(0, 2);
          ctxBadge = shown.map(function(cs) {
            var label = cs.condition
              ? esc(cs.branch) + ' (' + esc(cs.condition.slice(0, 45)) + (cs.condition.length > 45 ? '…' : '') + ')'
              : esc(cs.branch);
            return '<span class="ctx-conditional">'+label+'</span>';
          }).join('');
          if (ctx.cond.length > 2) {
            ctxBadge += '<span class="ctx-mixed">+' + (ctx.cond.length-2) + '</span>';
          }
        }
      }

      // Static caller badge
      var isStatic = cmod && !cIsPub;
      var staticBadge = isStatic ? '<span class="calledby-sep">s</span>' : '';

      html += '<div class="calledby-row" data-cbfn="'+esc(c.fn)+'" data-cbmod="'+esc(c.mod)+'">'+
        '<span class="calledby-arrow">←</span>'+
        staticBadge+
        '<span class="tree-fn '+cClass+'">'+esc(c.fn)+'()</span>'+
        '<span class="tree-mod" style="color:'+cColor+';">'+esc(c.mod)+'</span>'+
        '<span class="calledby-sep">·</span>'+
        covHtml+
        ctxBadge+
        '<span class="calledby-nav-hint">⊕</span>'+
        '</div>';
      html += ctxDetail;
    });
    if (callers.length > 25) {
      html += '<div class="calledby-more">… +'+(callers.length-25)+' autres appelants</div>';
    }
  } else {
    var directCov2 = ucCov[fnName] || [];
    var hint = directCov2.length
      ? '<span class="calledby-cov calledby-cov-ok">✓ appelé directement par use_cases/ : '+directCov2.slice(0,4).join(', ')+(directCov2.length>4?' …':'')+'</span>'
      : '<span class="calledby-cov calledby-cov-none">Point d\'entrée racine ou non appelé détecté.</span>';
    html += '<div style="margin-bottom:6px; font-size:11px;">'+hint+'</div>';
  }

  // ── Root + call tree ────────────────────────────────────────────────────────
  html += '<div style="border-top:1px solid #1e2330; margin: 8px 0 6px; padding-top:6px;">'+
    '<div style="font-size:10px;color:var(--muted);margin-bottom:4px;font-weight:600;letter-spacing:.06em;text-transform:uppercase;">Appels descendants (prof. '+depth+')</div>'+
    '<div class="tree-root">'+
    '<span class="tree-root-fn '+fnClass+'">'+esc(fnName)+'()</span>'+
    ' <span class="tree-root-mod" style="color:'+layColor+';">'+esc(modBase)+'</span>'+
    (rootDesc?'<div class="tree-root-desc">'+esc(rootDesc)+'</div>':'')+
    '</div>';

  treeNodeCount = 0;
  var tree = buildCallTree(fnName, modBase, depth, []);
  if (treeNodeCount > MAX_TREE_NODES) {
    html += '<div class="tree-empty">⚠ Arbre tronqué (> '+MAX_TREE_NODES+' nœuds)</div>';
  } else if (tree.children.length === 0) {
    html += '<div class="tree-empty">Aucun appel de projet détecté.</div>';
  } else {
    tree.children.forEach(function(child, i){
      html += renderTreeNode(child, modBase, '', i===tree.children.length-1);
    });
  }
  html += '</div>';

  // ── Lib/COTS calls ──────────────────────────────────────────────────────────
  var cfShowLib = document.getElementById('seqLibLocal').checked;
  if (cfShowLib && hasExtCalls && mod && mod.ext_calls && mod.ext_calls[fnName]) {
    var extList = mod.ext_calls[fnName];
    html += '<div class="tree-section">Appels lib/COTS directs ('+extList.length+') :</div>';
    extList.forEach(function(libName){
      html += '<div class="tree-line">'+
        '<span class="tree-pre">  </span>'+
        '<span class="tree-fn tree-lib">'+esc(libName)+'()</span>'+
        ' <span class="tree-mod" style="color:#424242;">extern</span>'+
        '</div>';
    });
  } else if (cfShowLib && !hasExtCalls) {
    html += '<div class="tree-section" style="color:#5a4040;">Données lib/COTS absentes — relancer gen_graph.py pour les générer.</div>';
  }

  return html;
}

// ─── Open / close / refresh seq panel ────────────────────────────────────────
function openSeqPanel(fnName, modBase) {
  seqActiveFn = fnName; seqActiveMod = modBase;

  // Highlight the active fn-row
  document.querySelectorAll('.fn-row').forEach(function(r){
    r.classList.toggle('seq-active', r.dataset.fn === fnName);
  });

  // Header
  var mod = modules[modBase];
  var layColor = LAYER_COLOR[inferLayer(modBase)] || '#8890a0';
  document.getElementById('seq-fn').textContent = fnName + '()';
  document.getElementById('seq-mod-badge').innerHTML =
    '<span style="color:'+layColor+';">'+esc(modBase)+'</span>';

  // Lib-status line
  var cfShowLib = document.getElementById('seqLibLocal').checked;
  var libStatus = document.getElementById('seq-lib-status');
  if (cfShowLib && !hasExtCalls) {
    libStatus.textContent = 'Appels lib non disponibles — relancer gen_graph.py --out project_graph.json';
    libStatus.style.display = 'block';
  } else {
    libStatus.style.display = 'none';
  }

  document.getElementById('seq-body').innerHTML = renderSeqTree(fnName, modBase);
  document.getElementById('seq-panel').style.display = 'flex';
  hideTooltip();
}

function closeSeqPanel() {
  document.getElementById('seq-panel').style.display = 'none';
  document.querySelectorAll('.fn-row.seq-active').forEach(function(r){ r.classList.remove('seq-active'); });
  seqActiveFn = null; seqActiveMod = null;
}

function refreshSeqPanel() {
  if (!seqActiveFn || !seqActiveMod) return;
  openSeqPanel(seqActiveFn, seqActiveMod);
}

document.getElementById('seq-close').addEventListener('click', closeSeqPanel);

// Click on a calledby-row → navigate to that caller's seq tree
document.getElementById('seq-body').addEventListener('click', function(e) {
  var cbRow = e.target.closest('.calledby-row');
  if (!cbRow) return;
  var targetFn  = cbRow.dataset.cbfn;
  var targetMod = cbRow.dataset.cbmod;
  if (!targetFn || !targetMod) return;

  // If the caller is in a different module from what's pinned in the right panel,
  // update the right panel so function rows + fn-search match the new context.
  if (targetMod !== currentBase && modules[targetMod]) {
    if (pinnedBase) cy.getElementById(pinnedBase).removeClass('pinned');
    pinnedBase = targetMod;
    cy.getElementById(targetMod).addClass('pinned');
    renderModuleDetail(targetMod);
    // Smoothly center the graph on that module
    var nd = cy.getElementById(targetMod);
    if (nd && nd.length) cy.animate({center:{eles:nd}}, {duration:350});
  }

  openSeqPanel(targetFn, targetMod);
});
</script>
</body>
</html>
"""


def main():
    parser = argparse.ArgumentParser(
        description='Render interactive HTML architecture diagram from project_graph.json')
    parser.add_argument('--graph',     default='project_graph.json')
    parser.add_argument('--out',       default='architecture.html')
    parser.add_argument('--positions', default=None,
                        help='JSON file of {module:{x,y}} positions to bake in')
    args = parser.parse_args()

    with open(args.graph, encoding='utf-8') as f:
        graph = json.load(f)

    saved_positions = {}
    if args.positions:
        with open(args.positions, encoding='utf-8') as f:
            saved_positions = json.load(f)

    graph_json     = json.dumps(graph, ensure_ascii=False).replace('</script', '<\\/script')
    positions_json = json.dumps(saved_positions, ensure_ascii=False).replace('</script', '<\\/script')

    out_html = HTML_TEMPLATE.replace('__GRAPH_JSON__', graph_json)
    out_html = out_html.replace('__SAVED_POSITIONS_JSON__', positions_json)

    with open(args.out, 'w', encoding='utf-8') as f:
        f.write(out_html)

    print('Done. %s written (%s chars).' % (args.out, '{:,}'.format(len(out_html))))
    if args.positions:
        print('Baked in %d manual positions from %s.' % (len(saved_positions), args.positions))


if __name__ == '__main__':
    main()
