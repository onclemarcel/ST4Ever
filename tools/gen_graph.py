#!/usr/bin/env python3
"""
gen_graph.py - Build a project-wide architecture graph for ST4Ever.

Reuses gen_dd.py's per-module extraction (process_module) and adds:
  - Global variables per module (declaration + readers/writers across
    the whole project, including platform .c and use_cases/*.c)
  - Cross-module call graph (caller module -> callee module, with the
    concrete function pairs that make up each module-to-module arrow)
  - Use-case coverage: for every src/ function, which use_case_NN.c
    files call it (directly) -> test coverage signal

Output: a single JSON file consumed by gen_html.py for the interactive
diagram, and reusable for any other reporting need.

Usage:
  python tools/gen_graph.py [--src DIR] [--plat DIR] [--uc DIR] [--out FILE]
"""

import argparse
import os
import re
import sys
import json
from collections import defaultdict, OrderedDict

import gen_dd  # reuse existing regex-based extraction, unmodified


# ---------------------------------------------------------------------------
# Global variable extraction
# ---------------------------------------------------------------------------

# File-scope variable declaration (static or not), single decl per line.
# Matches:  static st_bool_t g_trace_bOpen = ST_FALSE;
#           static struct gui_window_s *g_gui_aptWnd[GUI_MAX_WINDOWS];
#           static const renderer_color_t g_tv_clrBg = {...};
RE_GLOBAL_DECL = re.compile(
    r'^(static\s+)?'                       # optional static
    r'((?:const\s+)?[A-Za-z_][\w\s]*?\*?)'  # type (greedy-safe, non-static kw checked below)
    r'\s+(g_[A-Za-z0-9_]+)'                 # name: must start with g_
    r'(\s*\[[^\]]*\])?'                     # optional array dims
    r'\s*(?:=|;)',                          # assignment or end of decl
    re.MULTILINE
)

# Words that can precede 'g_xxx' but are not real types we care to record
# beyond using the captured text as-is; no exclusion needed since we anchor
# on the g_ prefix which is project convention for globals.

C_KEYWORDS = gen_dd.C_KEYWORDS  # reuse same exclusion set for call detection


def extract_global_decls(text):
    """
    Return OrderedDict: var_name -> {'is_static': bool, 'type': str, 'is_const': bool}
    Scans top-level (file-scope) declarations only — heuristic: line starts
    at column 0 with optional 'static', matched via MULTILINE anchors which
    is consistent with the project's formatting convention (no indentation
    for file-scope decls).
    """
    result = OrderedDict()
    for m in RE_GLOBAL_DECL.finditer(text):
        is_static = bool(m.group(1))
        raw_type  = m.group(2).strip()
        name      = m.group(3)
        is_const  = 'const' in raw_type
        if name not in result:
            result[name] = {
                'is_static': is_static,
                'type': raw_type,
                'is_const': is_const,
            }
    return result


def extract_global_usage(body, global_names):
    """
    Return (reads, writes) sets of global names referenced in a function body.
    Heuristic distinguishing write vs read:
      - write: name appears as left-hand side of an assignment operator
               (=, +=, -=, *=, /=, |=, &=, ^=, <<=, >>=, ++, --) not preceded
               by ==, !=, <=, >=
      - read : any other occurrence
    This is a heuristic (no full expression parser) but adequate for a
    visualization / navigation aid rather than a correctness tool.
    """
    if not global_names:
        return set(), set()

    # Strip string/char literals and comments to avoid false matches
    cleaned = re.sub(r'"(?:[^"\\]|\\.)*"', '""', body)
    cleaned = re.sub(r"'(?:[^'\\]|\\.)*'", "''", cleaned)
    cleaned = re.sub(r'//[^\n]*', '', cleaned)
    cleaned = re.sub(r'/\*.*?\*/', '', cleaned, flags=re.DOTALL)

    reads = set()
    writes = set()

    # Assignment-style write detection: name [optional array/member access] <assign-op>
    # Build one alternation regex for efficiency rather than looping per-name.
    names_alt = '|'.join(re.escape(n) for n in global_names)
    re_name = re.compile(r'\b(' + names_alt + r')\b')

    re_write = re.compile(
        r'\b(' + names_alt + r')\b'
        r'(?:\s*(?:\[[^\]]*\]|\.\w+|->\w+))*'   # optional index/member chain
        r'\s*(\+\+|--)?'                          # postfix inc/dec (treated below)
        r'\s*(=(?!=)|\+=|-=|\*=|/=|\|=|&=|\^=|<<=|>>=)'
    )
    re_incdec_pre = re.compile(
        r'(\+\+|--)\s*(' + names_alt + r')\b'
    )

    for m in re_name.finditer(cleaned):
        reads.add(m.group(1))   # provisional; refine below

    for m in re_write.finditer(cleaned):
        writes.add(m.group(1))
    for m in re_incdec_pre.finditer(cleaned):
        writes.add(m.group(2))
    # also catch postfix ++ / -- : name++ / name--
    re_incdec_post = re.compile(r'\b(' + names_alt + r')\b\s*(\+\+|--)')
    for m in re_incdec_post.finditer(cleaned):
        writes.add(m.group(1))

    reads -= writes  # a name that's written is not reported as "just read"
    # (it may also be read, e.g. `g_x += 1`, but for a navigation diagram
    # "this function writes it" is the more useful/actionable signal)
    return reads, writes


# ---------------------------------------------------------------------------
# Cross-module call graph
# ---------------------------------------------------------------------------

def build_function_index(modules):
    """
    Return dict: func_name -> module_base
    For functions defined in multiple modules (shouldn't happen, but be
    defensive), the first one found wins and a warning is printed.
    """
    index = {}
    for mod in modules:
        names = set(mod['pub_names']) | {n for n, _, _ in mod['internal']}
        for n in names:
            if n in index and index[n] != mod['base']:
                print(f"  WARNING: function '{n}' defined in both "
                      f"'{index[n]}' and '{mod['base']}'", file=sys.stderr)
                continue
            index[n] = mod['base']
    return index


def build_cross_module_edges(modules, func_index):
    """
    For every function body in every module, find calls to functions that
    live in a *different* module. Returns:
      edges: dict (src_module, dst_module) -> list of (caller_func, callee_func)
    """
    edges = defaultdict(list)
    all_funcs = set(func_index.keys())

    for mod in modules:
        base = mod['base']
        c_text = ''
        if mod['c_path'] and os.path.isfile(mod['c_path']):
            c_text += open(mod['c_path'], encoding='utf-8', errors='replace').read()
        if mod['plat_c_path'] and os.path.isfile(mod['plat_c_path']):
            c_text += '\n' + open(mod['plat_c_path'], encoding='utf-8', errors='replace').read()
        if not c_text:
            continue

        local_funcs = set(mod['pub_names']) | {n for n, _, _ in mod['internal']}
        for fn in local_funcs:
            body = gen_dd.find_function_body(c_text, fn)
            if not body:
                continue
            calls = gen_dd.extract_calls_from_body(body, all_funcs)
            for callee in calls:
                dst_mod = func_index.get(callee)
                if dst_mod and dst_mod != base:
                    edges[(base, dst_mod)].append((fn, callee))

    return edges


# ---------------------------------------------------------------------------
# Use-case coverage
# ---------------------------------------------------------------------------

def build_uc_coverage(uc_dir, func_index):
    """
    Return dict: func_name -> list of uc_file_base (e.g. 'use_case_01')
    that contain at least one direct call to that function.
    """
    coverage = defaultdict(list)
    if not uc_dir or not os.path.isdir(uc_dir):
        return coverage

    all_funcs = set(func_index.keys())
    uc_files = sorted(f for f in os.listdir(uc_dir) if f.endswith('.c'))

    for fname in uc_files:
        uc_base = os.path.splitext(fname)[0]
        path = os.path.join(uc_dir, fname)
        text = open(path, encoding='utf-8', errors='replace').read()
        # Strip literals/comments once, then scan whole file (UCs are
        # essentially one big main() with helper statics) for calls.
        cleaned = re.sub(r'"(?:[^"\\]|\\.)*"', '""', text)
        cleaned = re.sub(r"'(?:[^'\\]|\\.)*'", "''", cleaned)
        cleaned = re.sub(r'//[^\n]*', '', cleaned)
        cleaned = re.sub(r'/\*.*?\*/', '', cleaned, flags=re.DOTALL)

        called = set()
        for m in gen_dd.RE_CALL.finditer(cleaned):
            fn = m.group(1)
            if fn in gen_dd.C_KEYWORDS:
                continue
            if fn in all_funcs:
                called.add(fn)

        for fn in called:
            coverage[fn].append(uc_base)

    return coverage


# ---------------------------------------------------------------------------
# Global variable readers/writers (project-wide, incl. use_cases)
# ---------------------------------------------------------------------------

def build_global_usage_index(modules, uc_dir):
    """
    For each module, attach to every global var: readers[] and writers[]
    (function names), scoped to that module's own .c/.plat_c text — globals
    declared 'static' are not visible outside their translation unit, so
    cross-module usage is not expected/searched for static globals.
    Also scans use_cases/*.c bodies in case any global is exposed via a
    non-static declaration (rare, but kept generic).
    """
    for mod in modules:
        c_text = ''
        if mod['c_path'] and os.path.isfile(mod['c_path']):
            c_text += open(mod['c_path'], encoding='utf-8', errors='replace').read()
        if mod['plat_c_path'] and os.path.isfile(mod['plat_c_path']):
            c_text += '\n' + open(mod['plat_c_path'], encoding='utf-8', errors='replace').read()

        decls = extract_global_decls(c_text) if c_text else OrderedDict()
        global_names = set(decls.keys())

        usage = {name: {'readers': set(), 'writers': set()} for name in global_names}

        if global_names and c_text:
            local_funcs = set(mod['pub_names']) | {n for n, _, _ in mod['internal']}
            for fn in local_funcs:
                body = gen_dd.find_function_body(c_text, fn)
                if not body:
                    continue
                reads, writes = extract_global_usage(body, global_names)
                for g in reads:
                    usage[g]['readers'].add(fn)
                for g in writes:
                    usage[g]['writers'].add(fn)

        # Convert sets to sorted lists for JSON
        mod['globals'] = {
            name: {
                'is_static': decls[name]['is_static'],
                'type': decls[name]['type'],
                'is_const': decls[name]['is_const'],
                'readers': sorted(usage[name]['readers']),
                'writers': sorted(usage[name]['writers']),
            }
            for name in decls
        }


# ---------------------------------------------------------------------------
# Static function signature extraction
# ---------------------------------------------------------------------------

def _find_matching_paren(text, open_pos):
    """Return position of ) matching the ( at open_pos, or -1 if not found."""
    depth = 0
    i     = open_pos
    n     = len(text)
    while i < n:
        c = text[i]
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                return i
        elif c == '"':           # skip string literal bodies
            i += 1
            while i < n and text[i] != '"':
                if text[i] == '\\':
                    i += 1
                i += 1
        i += 1
    return -1


def _infer_param_direction(type_str, name):
    """
    Infer [in] / [out] from the C type and Hungarian-notation name.
    Convention (matches project R sections):
      - const T *  → [in]
      - T **       → [out]  (ppt prefix = output pointer-to-pointer)
      - T *        → [out]  (mutable pointer = output by convention)
      - non-pointer → [in]
    """
    is_ptr   = '*' in type_str
    is_const = bool(re.search(r'\bconst\b', type_str.split('*')[0]))

    if not is_ptr:
        return '[in]'
    if '**' in type_str or name.startswith('ppt'):
        return '[out]'
    if is_const:
        return '[in]'
    return '[out]'


def _parse_c_param_list(params_raw):
    """
    Parse a C parameter list string into a list of dicts:
        {'type': str, 'name': str, 'dir': str}
    Handles multi-line params and one level of nested parens (function ptrs).
    Returns [] for empty / void param lists.
    """
    params_raw = params_raw.strip()
    if not params_raw or params_raw.lower() == 'void':
        return []

    # Split on top-level commas only
    parts, depth, cur = [], 0, []
    for ch in params_raw:
        if ch == '(':
            depth += 1
        elif ch == ')':
            depth -= 1
        if ch == ',' and depth == 0:
            parts.append(''.join(cur).strip())
            cur = []
        else:
            cur.append(ch)
    if cur:
        parts.append(''.join(cur).strip())

    result = []
    for p in parts:
        p = ' '.join(p.split())                      # collapse whitespace
        p = re.sub(r'\[\d*\]\s*$', '', p).strip()   # strip trailing []
        if not p:
            continue

        # Last C identifier = parameter name
        m = re.search(r'\b([A-Za-z_]\w*)\s*$', p)
        if not m:
            result.append({'type': p, 'name': '?', 'dir': '[?]'})
            continue

        name      = m.group(1)
        type_part = p[:m.start()].strip()
        direction = _infer_param_direction(type_part, name)
        result.append({'type': type_part, 'name': name, 'dir': direction})

    return result


def extract_static_signatures(c_text, func_names):
    """
    For each name in func_names, locate its definition in c_text and return:
        return_type (str), params list of {'type', 'name', 'dir'}
    Returns dict: func_name -> {'return_type': str, 'params': [...]}

    Strategy: find fname( ... ) { sequence in the comment-stripped source,
    then look backwards for the 'static' keyword and return type.
    """
    if not func_names:
        return {}

    # Strip string literals and comments once for the whole file
    cleaned = re.sub(r'"(?:[^"\\]|\\.)*"', '""', c_text)
    cleaned = re.sub(r"'(?:[^'\\]|\\.)*'", "''", cleaned)
    cleaned = re.sub(r'//[^\n]*',          '',   cleaned)
    cleaned = re.sub(r'/\*.*?\*/',         ' ',  cleaned, flags=re.DOTALL)

    result = {}
    for fname in func_names:
        pat = re.compile(r'\b' + re.escape(fname) + r'\s*\(')
        for m in pat.finditer(cleaned):
            open_pos  = m.end() - 1
            close_pos = _find_matching_paren(cleaned, open_pos)
            if close_pos < 0:
                continue

            # Confirm this is a *definition* (not a call): next non-space = {
            after = cleaned[close_pos + 1: close_pos + 50]
            if not re.match(r'\s*\{', after):
                continue

            # Return type: look backwards up to 400 chars for 'static' keyword
            before = cleaned[max(0, m.start() - 400): m.start()].rstrip()
            si     = before.rfind('static')
            if si < 0:
                continue   # not a static definition

            ret_raw  = before[si + len('static'):].strip()
            ret_type = re.sub(r'\s+', ' ', ret_raw).strip() or 'void'

            params = _parse_c_param_list(cleaned[open_pos + 1: close_pos])
            result[fname] = {'return_type': ret_type, 'params': params}
            break   # first definition wins

    return result


def build_internal_docs(modules):
    """
    Populate mod['internal_sigs'] = {func_name: {'return_type': str, 'params': [...]}}
    for every static function in every module by parsing the .c source.
    Public functions already have their signatures in mod['docs'] (from .h docstrings);
    this covers the static functions that have no .h entry.
    """
    for mod in modules:
        c_text = ''
        if mod['c_path'] and os.path.isfile(mod['c_path']):
            c_text += open(mod['c_path'], encoding='utf-8', errors='replace').read()
        if mod['plat_c_path'] and os.path.isfile(mod['plat_c_path']):
            c_text += '\n' + open(mod['plat_c_path'], encoding='utf-8',
                                  errors='replace').read()

        internal_names = {n for n, _, _ in mod['internal']}
        mod['internal_sigs'] = (
            extract_static_signatures(c_text, internal_names)
            if c_text else {}
        )


# ---------------------------------------------------------------------------
# Static function call-site context (conditional vs unconditional)
# ---------------------------------------------------------------------------

def _analyze_call_guard_at(text, call_pos):
    """
    Given cleaned source text and the position of a function call within it,
    determine whether the call is in the unconditional flow of the enclosing
    function or guarded by a control-flow construct.

    Algorithm: scan backward from call_pos to find the innermost enclosing {.
    Then inspect the token immediately before that { to identify the guard.

    Returns:
        {'is_conditional': bool, 'condition': str|None, 'branch': str}
    where branch is one of: 'unconditional', 'if', 'else if', 'else',
    'while', 'for', 'switch', 'do'.
    """
    depth = 0
    i     = call_pos - 1
    enc   = -1

    while i >= 0:
        c = text[i]
        if c == '}':
            depth += 1
        elif c == '{':
            if depth == 0:
                enc = i
                break
            depth -= 1
        i -= 1

    if enc < 0:
        return {'is_conditional': False, 'condition': None, 'branch': 'unconditional'}

    # What precedes the enclosing {?
    pre = text[:enc].rstrip()

    # Match control-flow keyword at the end of 'pre' (look at last ~300 chars).
    # Order matters: 'else if' before 'else', 'else if' before 'if'.
    window = pre[-300:] if len(pre) > 300 else pre
    kw = None
    for candidate in ('else if', 'else', 'if', 'while', 'for', 'switch', 'do'):
        pat = re.compile(r'\b' + candidate.replace(' ', r'\s+') + r'\b\s*$')
        m   = pat.search(window)
        if m:
            kw = candidate
            # After-keyword text: might have a (condition)
            after = window[m.end():].strip()
            break

    if kw is None:
        return {'is_conditional': False, 'condition': None, 'branch': 'unconditional'}

    # Extract condition expression for keywords that have one
    condition = None
    if kw not in ('else', 'do'):
        # Find everything between 'keyword' and '{' — should be '(...)'
        kw_end_in_pre = pre.rfind(kw) + len(kw)
        rest = pre[kw_end_in_pre:].strip()
        if rest.startswith('('):
            close = _find_matching_paren(rest, 0)
            if close > 0:
                raw = rest[1:close].strip()
                condition = re.sub(r'\s+', ' ', raw)[:120]

    is_cond = kw in ('if', 'else if', 'else', 'while', 'for', 'switch')
    return {'is_conditional': is_cond, 'condition': condition, 'branch': kw}


def _find_call_contexts_in_body(body, callee_name):
    """
    Find every call to callee_name inside body and return the list of guard
    contexts. Multiple calls from the same function body are all returned.
    """
    cleaned = re.sub(r'"(?:[^"\\]|\\.)*"', '""',    body)
    cleaned = re.sub(r"'(?:[^'\\]|\\.)*'", "''",    cleaned)
    cleaned = re.sub(r'//[^\n]*',          '',       cleaned)
    cleaned = re.sub(r'/\*.*?\*/',         ' ',      cleaned, flags=re.DOTALL)

    pat     = re.compile(r'\b' + re.escape(callee_name) + r'\s*\(')
    results = []
    for m in pat.finditer(cleaned):
        results.append(_analyze_call_guard_at(cleaned, m.start()))
    return results


def build_static_call_contexts(modules):
    """
    For every static function S in module M, find which other functions in M
    call it and classify each call site as unconditional (main flow) or
    conditional (inside if/while/for/switch/else). Static functions by
    definition can only be called from within their own translation unit.

    Stores: mod['static_ctx'] = {
        S_name: [
            {
                'parent':  P_name,
                'uncond':  int,    # count of unconditional call sites
                'cond':    [       # list of conditional call sites
                    {'branch': str, 'condition': str|None}
                ],
            }, ...
        ]
    }
    """
    for mod in modules:
        c_text = ''
        if mod['c_path'] and os.path.isfile(mod['c_path']):
            c_text += open(mod['c_path'], encoding='utf-8', errors='replace').read()
        if mod['plat_c_path'] and os.path.isfile(mod['plat_c_path']):
            c_text += '\n' + open(mod['plat_c_path'], encoding='utf-8',
                                  errors='replace').read()

        static_names = {n for n, _, _ in mod['internal']}
        all_local    = set(mod['pub_names']) | static_names
        cg           = mod.get('call_graph', {})

        static_ctx = {s: [] for s in static_names}

        for parent in all_local:
            callees = set(cg.get(parent, []))
            for s_name in static_names:
                if s_name == parent or s_name not in callees:
                    continue

                body = gen_dd.find_function_body(c_text, parent) if c_text else None
                if not body:
                    continue

                sites = _find_call_contexts_in_body(body, s_name)
                if not sites:
                    continue

                uncond_count = sum(1 for s in sites if not s['is_conditional'])
                cond_sites   = [
                    {'branch': s['branch'], 'condition': s['condition']}
                    for s in sites if s['is_conditional']
                ]

                static_ctx[s_name].append({
                    'parent': parent,
                    'uncond': uncond_count,
                    'cond':   cond_sites,
                })

        mod['static_ctx'] = static_ctx


# ---------------------------------------------------------------------------
# External (library / COTS) call extraction
# ---------------------------------------------------------------------------
# INTENT comment parsing from use_case_XX.c files
# ---------------------------------------------------------------------------

# Matches /* INTENT[...]: description */ — possibly multi-line
RE_INTENT_COMMENT = re.compile(
    r'/\*\s*INTENT\[([^\]]+)\]\s*:\s*(.*?)\*/',
    re.DOTALL
)

# Matches TEST_ASSERT( — to find tested functions
RE_TEST_ASSERT = re.compile(r'\bTEST_ASSERT\s*\(')


def _expand_tags(prefix, chain_str):
    """
    Extract all tags with the given prefix from a chain string.
    Handles single tags (TC-CPU-001) and ranges (TC-CPU-001..003).
    Returns sorted, deduplicated list.
    """
    tags = []
    pat = re.compile(
        r'\b' + re.escape(prefix) + r'-([A-Z]+)-(\d+)(?:\.\.(\d+))?'
    )
    for m in pat.finditer(chain_str):
        letters  = m.group(1)
        start_n  = int(m.group(2))
        end_n    = int(m.group(3)) if m.group(3) else start_n
        width    = len(m.group(2))
        base     = f'{prefix}-{letters}-'
        for n in range(start_n, end_n + 1):
            tags.append(base + str(n).zfill(width))
    return sorted(set(tags))


def _parse_intent_chain(chain_str):
    """
    Parse the content of INTENT[...] brackets and return a dict of tag lists.
    """
    return {
        'int': _expand_tags('INT', chain_str),
        'tc':  _expand_tags('TC',  chain_str),
        'req': _expand_tags('REQ', chain_str),
        'ufr': _expand_tags('UFR', chain_str),
    }


def _extract_tested_fns_after_intent(text_after, func_index):
    """
    Find project function calls inside TEST_ASSERT() blocks that immediately
    follow an INTENT comment (up to the next INTENT or 2000 chars).
    """
    # Stop at the next INTENT to avoid mixing test cases
    next_intent = text_after.find('/* INTENT[')
    window = text_after[:min(next_intent if next_intent > 0 else len(text_after), 2000)]

    # Strip literals and line comments
    cleaned = re.sub(r'"(?:[^"\\]|\\.)*"', '""', window)
    cleaned = re.sub(r"'(?:[^'\\]|\\.)*'", "''", cleaned)
    cleaned = re.sub(r'//[^\n]*', '', cleaned)

    tested = set()
    for m in RE_TEST_ASSERT.finditer(cleaned):
        start = m.end() - 1
        end   = _find_matching_paren(cleaned, start)
        if end < 0:
            continue
        arg = cleaned[start + 1: end]
        for cm in gen_dd.RE_CALL.finditer(arg):
            fn = cm.group(1)
            if fn in func_index and fn not in gen_dd.C_KEYWORDS:
                tested.add(fn)
    return sorted(tested)


def parse_intent_comments(uc_dir, func_index):
    """
    Scan use_case_XX.c files for structured INTENT[...] comments and build:

    tc_intents : {tc_tag -> {desc, int_tags, req_tags, ufr_tags, uc_file, tested_fns}}
    fn_tc_map  : {func_name -> [tc_tag, ...]}

    Modules without INTENT comments (not yet through Phase 2 documentation) are
    simply absent from fn_tc_map — callers fall back to the UC-level coverage display.
    """
    tc_intents = {}
    fn_tc_map  = defaultdict(list)

    if not uc_dir or not os.path.isdir(uc_dir):
        return tc_intents, {}

    uc_files = sorted(f for f in os.listdir(uc_dir) if f.endswith('.c'))

    for fname in uc_files:
        uc_base = os.path.splitext(fname)[0]
        path    = os.path.join(uc_dir, fname)
        text    = open(path, encoding='utf-8', errors='replace').read()

        for m in RE_INTENT_COMMENT.finditer(text):
            chain_str = m.group(1)
            desc_raw  = m.group(2)

            # Clean multi-line description: strip leading ' * ' from each line
            desc_lines = [
                re.sub(r'^\s*\*\s?', '', line).strip()
                for line in desc_raw.split('\n')
            ]
            desc = ' '.join(l for l in desc_lines if l).strip()

            tags        = _parse_intent_chain(chain_str)
            tested_fns  = _extract_tested_fns_after_intent(text[m.end():], func_index)

            for tc_tag in (tags['tc'] or []):
                if tc_tag not in tc_intents:
                    tc_intents[tc_tag] = {
                        'desc':     desc,
                        'int_tags': tags['int'],
                        'req_tags': tags['req'],
                        'ufr_tags': tags['ufr'],
                        'uc_file':  uc_base,
                        'tested_fns': tested_fns,
                    }
                else:
                    # Same TC referenced in multiple INTENT blocks (e.g. multi-file):
                    # merge tested_fns
                    tc_intents[tc_tag]['tested_fns'] = sorted(
                        set(tc_intents[tc_tag]['tested_fns']) | set(tested_fns)
                    )

                for fn in tested_fns:
                    if tc_tag not in fn_tc_map[fn]:
                        fn_tc_map[fn].append(tc_tag)

    n_tc  = len(tc_intents)
    n_fn  = len(fn_tc_map)
    n_pairs = sum(len(v) for v in fn_tc_map.values())
    print(f"Parsed {n_tc} TC intents covering {n_fn} functions "
          f"({n_pairs} fn-TC pairs, from {len(uc_files)} use_case files).")

    return tc_intents, {k: sorted(v) for k, v in fn_tc_map.items()}


# ---------------------------------------------------------------------------

# Prefixes that identify project-specific macros rather than real function calls.
_MACRO_PREFIXES = (
    'LOG_', 'TEST_', 'ST_', 'GUI_', 'CPU_', 'EXEC_', 'IST_',
    'MFP_', 'YM_', 'CON_', 'LINEA_', 'DISASM_', 'ASM_', 'ANN_',
    'HEX_', 'MOUNT_', 'DIR_', 'EDIT_', 'FILE_', 'TRACE_',
    'FONT_', 'WIN_', 'LX_', 'PLAT_', 'SHIFT_', 'BOOT_', 'SECT_',
    'PRG_', 'IMG_', 'MSA_', 'ANNOT_', 'SECTOR_',
)


def _is_likely_macro(name):
    """Return True for ALL_CAPS identifiers or known project macro prefixes."""
    if name.isupper():
        return True
    for p in _MACRO_PREFIXES:
        if name.startswith(p):
            return True
    return False


def build_ext_calls_index(modules, func_index):
    """
    For each function in each module, extract calls to identifiers that are NOT
    in func_index (external library / COTS calls). Project macros are filtered
    out. Result stored as mod['ext_calls'] = {func_name: sorted([name, ...])}.
    """
    all_project = set(func_index.keys())

    for mod in modules:
        c_text = ''
        if mod['c_path'] and os.path.isfile(mod['c_path']):
            c_text += open(mod['c_path'], encoding='utf-8', errors='replace').read()
        if mod['plat_c_path'] and os.path.isfile(mod['plat_c_path']):
            c_text += '\n' + open(mod['plat_c_path'], encoding='utf-8',
                                  errors='replace').read()

        ext_calls = {}
        if c_text:
            local_funcs = (set(mod['pub_names'])
                           | {n for n, _, _ in mod['internal']})
            for fn in local_funcs:
                body = gen_dd.find_function_body(c_text, fn)
                if not body:
                    continue
                # Strip literals / comments before scanning
                cleaned = re.sub(r'"(?:[^"\\]|\\.)*"', '""', body)
                cleaned = re.sub(r"'(?:[^'\\]|\\.)*'", "''", cleaned)
                cleaned = re.sub(r'//[^\n]*', '', cleaned)
                cleaned = re.sub(r'/\*.*?\*/', '', cleaned, flags=re.DOTALL)

                lib_set = set()
                for m in gen_dd.RE_CALL.finditer(cleaned):
                    callee = m.group(1)
                    if callee in gen_dd.C_KEYWORDS:
                        continue
                    if callee in all_project:
                        continue
                    if _is_likely_macro(callee):
                        continue
                    if len(callee) < 3:
                        continue  # too short — likely a cast or local abbrev
                    lib_set.add(callee)

                if lib_set:
                    ext_calls[fn] = sorted(lib_set)

        mod['ext_calls'] = ext_calls


# ---------------------------------------------------------------------------
# Assembly
# ---------------------------------------------------------------------------

def build_project_graph(src_dir, plat_dir, uc_dir, max_depth=3):
    """
    Return the full project graph as a JSON-serializable dict:
    {
      'modules': {
         base: {
            'role', 'pub_names', 'docs' (per-func desc/params/returns/reqs),
            'internal' [ (name, desc, reqs) ],
            'globals' { name: {is_static, type, is_const, readers, writers} },
            'int_deps', 'sys_deps',
            'call_graph' (intra-module, from gen_dd),
         }, ...
      },
      'func_index': { func_name: module_base },
      'cross_module_edges': [
         {'src': mod_a, 'dst': mod_b, 'calls': [[caller, callee], ...]}
      ],
      'uc_coverage': { func_name: [uc_base, ...] }
    }
    """
    raw_modules = gen_dd.discover_modules(src_dir, plat_dir, '')
    modules = []
    for base, h_path, c_path, plat_c_path in raw_modules:
        try:
            mod = gen_dd.process_module(base, h_path, c_path, plat_c_path,
                                         max_depth, no_tree=False)
            modules.append(mod)
        except Exception as exc:
            print(f"  WARNING: {base} failed: {exc}", file=sys.stderr)

    print(f"Processed {len(modules)} modules.")

    func_index = build_function_index(modules)
    print(f"Indexed {len(func_index)} functions.")

    build_global_usage_index(modules, uc_dir)
    total_globals = sum(len(m['globals']) for m in modules)
    print(f"Found {total_globals} global variables.")

    cross_edges = build_cross_module_edges(modules, func_index)
    print(f"Found {len(cross_edges)} cross-module edges "
          f"({sum(len(v) for v in cross_edges.values())} call instances).")

    uc_coverage = build_uc_coverage(uc_dir, func_index)
    print(f"Coverage data for {len(uc_coverage)} functions "
          f"referenced from use_cases/.")

    build_ext_calls_index(modules, func_index)
    total_ext = sum(len(m['ext_calls']) for m in modules)
    print(f"External call data for {total_ext} functions "
          f"(library/COTS calls, macros filtered).")

    build_internal_docs(modules)
    total_sigs = sum(len(m['internal_sigs']) for m in modules)
    print(f"Extracted signatures for {total_sigs} static functions "
          f"(return type + params).")

    build_static_call_contexts(modules)
    total_ctx = sum(
        sum(len(v) for v in m['static_ctx'].values())
        for m in modules
    )
    print(f"Analysed {total_ctx} static call sites "
          f"(conditional vs unconditional).")

    tc_intents, fn_tc_map = parse_intent_comments(uc_dir, func_index)

    # --- Serialize ---
    out_modules = {}
    for mod in modules:
        out_modules[mod['base']] = {
            'role': mod['role'],
            'pub_names': mod['pub_names'],
            'docs': mod['docs'],
            'internal': mod['internal'],
            'internal_sigs': mod.get('internal_sigs', {}),
            'static_ctx':    mod.get('static_ctx',    {}),
            'globals': mod['globals'],
            'ext_calls': mod['ext_calls'],
            'int_deps': mod['int_deps'],
            'sys_deps': mod['sys_deps'],
            'call_graph': {k: sorted(v) for k, v in mod['call_graph'].items()},
            'h_path': mod['h_path'],
            'c_path': mod['c_path'],
            'plat_c_path': mod['plat_c_path'],
        }

    out_edges = []
    for (src, dst), calls in sorted(cross_edges.items()):
        out_edges.append({
            'src': src,
            'dst': dst,
            'calls': sorted(set(calls)),
        })

    return {
        'modules': out_modules,
        'func_index': func_index,
        'cross_module_edges': out_edges,
        'uc_coverage': {k: sorted(v) for k, v in uc_coverage.items()},
        'tc_intents':  tc_intents,
        'fn_tc_map':   fn_tc_map,
    }


def main():
    parser = argparse.ArgumentParser(
        description='Build project-wide architecture graph (JSON) for ST4Ever')
    parser.add_argument('--src',  default='src')
    parser.add_argument('--plat', default='win')
    parser.add_argument('--uc',   default='use_cases')
    parser.add_argument('--out',  default='project_graph.json')
    parser.add_argument('--depth', type=int, default=3)
    args = parser.parse_args()

    if not os.path.isdir(args.src):
        print(f'ERROR: source directory not found: {args.src}', file=sys.stderr)
        sys.exit(1)

    graph = build_project_graph(args.src, args.plat, args.uc, args.depth)

    with open(args.out, 'w', encoding='utf-8') as f:
        json.dump(graph, f, indent=1, ensure_ascii=False)

    print(f"Done. {args.out} written.")


if __name__ == '__main__':
    main()
