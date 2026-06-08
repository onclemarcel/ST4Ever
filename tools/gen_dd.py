#!/usr/bin/env python3
"""
gen_dd.py - Generate "4 - DD.md" (Detailed Description) from ST4Ever sources.

Extracts per-module:
  - Role        : file-level header comment from .h
  - Public API  : docstrings before st_error_t / void declarations in .h
                  (format: '* func() - desc' per CLAUDE.md §4.6)
  - Internal    : static functions with their inline comment from .c
  - Dependencies: #include directives from .c
  - Call graph  : caller -> callees extracted from function bodies,
                  rendered as ASCII tree

Usage:
  python tools/gen_dd.py [options]

Options:
  --src        DIR    Source directory (default: src/)
  --plat       DIR    Platform directory (default: win/)
  --out        FILE   Output file (default: 4 - DD.md)
  --mods       a,b,c  Comma-separated module base-names to include
                      (default: all .h/.c pairs found in --src)
  --depth      N      Max call-tree depth (default: 3)
  --no-tree           Omit the call-graph section
  --check-reqs FILE   Cross-check @req tags against 2-SR.md Impl. column.
                      Reports: tags in code missing from SR, REQs in SR
                      with Impl. column but no matching @req tag in code.
"""

import argparse
import os
import re
import sys
from collections import defaultdict, OrderedDict

# ---------------------------------------------------------------------------
# Regex patterns
# ---------------------------------------------------------------------------

# File-level header block: first /* ... */ before #ifndef
RE_FILE_HEADER = re.compile(
    r'^\s*/\*(.*?)\*/',
    re.DOTALL
)

# Individual REQ identifier: REQ-XXX-NNN
RE_REQ_ID = re.compile(r'REQ-[A-Z]+-\d+')

# §4.6 docstring: /* \n * func() - desc \n ... */ immediately before a decl
# Captures: (func_name, one_line_desc, full_body)
RE_DOCSTRING = re.compile(
    r'/\*\s*\n'                      # opening /*
    r'\s*\*\s*(\w+)\(\)\s*-\s*'     # * func() -
    r'(.+?)\n'                       # one-line description
    r'(.*?)'                         # body (parameters / returns)
    r'\*/',                          # closing */
    re.DOTALL
)

# Public function declaration (non-static, ends with ;)
RE_FUNC_DECL = re.compile(
    r'^(?!.*\bstatic\b)'             # not static
    r'(st_error_t|void|int|size_t|st_bool_t|st_u8_t|st_u16_t|st_u32_t'
    r'|st_i32_t|st_u64_t|[\w_]+\s*\*?)'
    r'\s+(\w+)\s*\([^)]*\)\s*;',
    re.MULTILINE
)

# Static function definition in .c
RE_STATIC_FUNC = re.compile(
    r'^static\s+[\w_\s\*]+?\s+(\w+)\s*\(',
    re.MULTILINE
)

# Inline comment above a static function (/* ... */ or // ...)
RE_INLINE_COMMENT = re.compile(
    r'/\*\s*\n?\s*\*?\s*(.*?)\s*\*?\s*\*/',
    re.DOTALL
)

# #include line
RE_INCLUDE = re.compile(r'^\s*#\s*include\s+([<"][^>"]+[>"])', re.MULTILINE)

# Function call inside a body: word followed by (
RE_CALL = re.compile(r'\b([a-z_][a-z0-9_]*)\s*\(')

# C keywords / macros to exclude from call detection
C_KEYWORDS = frozenset({
    'if', 'else', 'for', 'while', 'do', 'switch', 'case', 'return',
    'break', 'continue', 'sizeof', 'typeof', 'goto', 'default',
    'typedef', 'struct', 'enum', 'union', 'static', 'extern',
    'memset', 'memcpy', 'memmove', 'malloc', 'free', 'realloc',
    'printf', 'fprintf', 'sprintf', 'snprintf', 'strlen', 'strcpy',
    'strncpy', 'strcmp', 'strncmp', 'strcat', 'strncat', 'strstr',
    'fopen', 'fclose', 'fread', 'fwrite', 'fseek', 'ftell', 'fflush',
    'assert', 'abort', 'exit',
    # LOG macros expand to trace_log — skip the macro names
    'LOG_TRACE', 'LOG_INFO', 'LOG_ERROR', 'LOG_TODO',
    'TEST_ASSERT', 'TEST_SKIP', 'TEST_MANUAL',
})


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def extract_req_tags(raw_comment):
    """
    Extract list of REQ-XXX-NNN identifiers from an @req tag in raw comment text.
    Handles multi-line continuation (lines starting with ' *      ').
    Returns a list of strings like ['REQ-CPU-010', 'REQ-CPU-011'].
    """
    reqs = []
    idx = raw_comment.find('@req')
    while idx >= 0:
        rest = raw_comment[idx + 4:]
        # @req block ends at next blank comment line or next @tag
        end_markers = []
        m_blank = re.search(r'\n\s*\*?\s*\n', rest)
        m_at    = re.search(r'\n\s*\*?\s*@', rest)
        if m_blank:
            end_markers.append(m_blank.start())
        if m_at:
            end_markers.append(m_at.start())
        end   = min(end_markers) if end_markers else len(rest)
        block = rest[:end]
        reqs += RE_REQ_ID.findall(block)
        idx   = raw_comment.find('@req', idx + 4)
    return reqs


def strip_comment_stars(text):
    """Remove leading ' * ' from each line of a block-comment body."""
    lines = []
    for line in text.splitlines():
        line = re.sub(r'^\s*\*\s?', '', line)
        lines.append(line.rstrip())
    # Drop leading/trailing blank lines
    while lines and not lines[0].strip():
        lines.pop(0)
    while lines and not lines[-1].strip():
        lines.pop()
    return '\n'.join(lines)


def extract_file_header(text):
    """Return the file-level header comment as clean text, or ''."""
    m = RE_FILE_HEADER.search(text)
    if not m:
        return ''
    raw = m.group(1)
    return strip_comment_stars(raw)


def extract_docstrings(text):
    """
    Return dict: func_name -> {'desc': str, 'params': [(name, dir, desc)],
                                'returns': [str]}
    """
    result = OrderedDict()
    for m in RE_DOCSTRING.finditer(text):
        name   = m.group(1)
        desc   = m.group(2).strip()
        body   = m.group(3)

        params  = []
        returns = []

        # Parse Parameters block
        pm = re.search(r'Parameters:(.*?)(?:Returns:|$)', body, re.DOTALL)
        if pm:
            for line in pm.group(1).splitlines():
                line = re.sub(r'^\s*\*\s*', '', line).strip()
                pm2 = re.match(r'(\w+)\s+\[([^\]]+)\]\s*:\s*(.*)', line)
                if pm2:
                    params.append((pm2.group(1), pm2.group(2), pm2.group(3)))

        # Parse Returns block
        rm = re.search(r'Returns:(.*?)$', body, re.DOTALL)
        if rm:
            for line in rm.group(1).splitlines():
                line = re.sub(r'^\s*\*\s*', '', line).strip()
                if line:
                    returns.append(line)

        reqs = extract_req_tags(m.group(0))
        result[name] = {'desc': desc, 'params': params, 'returns': returns,
                        'reqs': reqs}
    return result


def extract_public_decls(text):
    """Return list of public function names declared in a header."""
    names = []
    for m in RE_FUNC_DECL.finditer(text):
        names.append(m.group(2))
    return names


def find_function_body(text, func_name):
    """
    Locate the body of a function definition named func_name in .c text.
    Returns the body string (between first { and matching }), or ''.
    Uses Allman-style: opening { is on its own line after the signature.
    """
    # Find the function signature
    pat = re.compile(
        r'\b' + re.escape(func_name) + r'\s*\([^)]*\)\s*\n\{',
        re.MULTILINE
    )
    m = pat.search(text)
    if not m:
        # Also try multi-line parameter list
        pat2 = re.compile(
            r'\b' + re.escape(func_name) + r'\s*\(',
            re.MULTILINE
        )
        m2 = pat2.search(text)
        if not m2:
            return ''
        # Find the opening brace after the match
        rest = text[m2.start():]
        brace_pos = rest.find('\n{')
        if brace_pos < 0:
            return ''
        start = m2.start() + brace_pos + 2
    else:
        start = m.end()

    # Walk forward tracking brace depth
    depth = 1
    i = start
    while i < len(text) and depth > 0:
        c = text[i]
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
        i += 1
    return text[start:i - 1]


def extract_static_functions(text):
    """
    Return list of (func_name, brief_desc, reqs) for all static functions in .c.
    brief_desc and reqs are taken from the /* ... */ comment immediately before.
    """
    result = []
    # Find all static function definitions with their position
    for m in RE_STATIC_FUNC.finditer(text):
        name = m.group(1)
        if name in ('main',):
            continue
        # Look for a comment block just before this definition
        before = text[:m.start()]
        # Grab up to 12 lines before
        lines_before = before.rstrip().rsplit('\n', 12)
        desc = ''
        reqs = []
        # Scan backwards for a comment
        block = '\n'.join(lines_before)
        cm = list(RE_INLINE_COMMENT.finditer(block))
        if cm:
            raw_match = cm[-1]
            raw = raw_match.group(1).strip()
            # Extract @req tags from the full raw comment block
            reqs = extract_req_tags(raw_match.group(0))
            # Take first meaningful line as description
            for raw_line in raw.splitlines():
                raw_line = raw_line.strip()
                raw_line = re.sub(r'^\*+\s*', '', raw_line).strip()
                # Skip separator lines (----, ===, etc.)
                if re.match(r'^[-=*]{4,}$', raw_line):
                    continue
                # Skip @req lines
                if raw_line.startswith('@req'):
                    continue
                if raw_line:
                    # Strip redundant "func_name() - " prefix
                    raw_line = re.sub(r'^\w+\(\)\s*-\s*', '', raw_line)
                    desc = raw_line
                    break
        result.append((name, desc, reqs))
    return result


def extract_calls_from_body(body, known_funcs):
    """
    Return set of function names called in body that are in known_funcs.
    Strips string literals and line comments first.
    """
    # Remove string literals
    cleaned = re.sub(r'"(?:[^"\\]|\\.)*"', '""', body)
    # Remove line comments
    cleaned = re.sub(r'//[^\n]*', '', cleaned)
    # Remove block comments
    cleaned = re.sub(r'/\*.*?\*/', '', cleaned, flags=re.DOTALL)

    calls = set()
    for m in RE_CALL.finditer(cleaned):
        fn = m.group(1)
        if fn in C_KEYWORDS:
            continue
        if fn in known_funcs:
            calls.add(fn)
    return calls


def extract_includes(c_text):
    """Return (internal_list, system_list) of #include targets."""
    internal = []
    system   = []
    for m in RE_INCLUDE.finditer(c_text):
        inc = m.group(1)
        if inc.startswith('"'):
            internal.append(inc.strip('"'))
        else:
            system.append(inc.strip('<>'))
    return internal, system


# ---------------------------------------------------------------------------
# Call graph rendering
# ---------------------------------------------------------------------------

def build_call_graph(all_funcs, c_text):
    """
    Build call graph: func_name -> set of called func_names (within all_funcs).
    """
    graph = {}
    for name in all_funcs:
        body = find_function_body(c_text, name)
        calls = extract_calls_from_body(body, all_funcs) if body else set()
        calls.discard(name)   # no self-loops
        graph[name] = calls
    return graph


def render_tree(graph, roots, max_depth, prefix='', depth=0, visited=None):
    """Render ASCII call tree. Returns list of strings."""
    if visited is None:
        visited = set()
    lines = []
    for i, root in enumerate(sorted(roots)):
        is_last = (i == len(roots) - 1)
        connector = '└─ ' if is_last else '├─ '
        child_prefix = prefix + ('   ' if is_last else '│  ')

        if root in visited:
            lines.append(f'{prefix}{connector}{root}()  ↩')
            continue

        lines.append(f'{prefix}{connector}{root}()')

        if depth < max_depth - 1:
            visited.add(root)
            children = graph.get(root, set())
            if children:
                lines += render_tree(graph, children, max_depth,
                                     child_prefix, depth + 1,
                                     visited.copy())
    return lines


def find_entry_points(graph):
    """Functions not called by any other function in the graph = roots."""
    all_funcs = set(graph.keys())
    called = set()
    for callees in graph.values():
        called |= callees
    roots = all_funcs - called
    return roots if roots else all_funcs


# ---------------------------------------------------------------------------
# Module processing
# ---------------------------------------------------------------------------

def process_module(base_name, h_path, c_path, plat_c_path, max_depth, no_tree):
    """
    Process one module and return a dict with all extracted information.
    """
    h_text  = open(h_path,  encoding='utf-8', errors='replace').read() \
              if h_path and os.path.isfile(h_path) else ''
    c_text  = open(c_path,  encoding='utf-8', errors='replace').read() \
              if c_path and os.path.isfile(c_path) else ''
    pc_text = open(plat_c_path, encoding='utf-8', errors='replace').read() \
              if plat_c_path and os.path.isfile(plat_c_path) else ''

    combined_c = c_text + '\n' + pc_text

    # Role
    role = extract_file_header(h_text) or extract_file_header(c_text)

    # Public API (from header)
    docs   = extract_docstrings(h_text)
    decls  = extract_public_decls(h_text)
    # Keep declaration order; fall back to docs order for undeclared
    pub_names = list(OrderedDict.fromkeys(decls + list(docs.keys())))

    # Static / internal functions (from .c)
    internal = extract_static_functions(combined_c)
    # Deduplicate by name (platform .c may repeat some helpers)
    seen_int = set()
    internal_dedup = []
    for name, desc, reqs in internal:
        if name not in seen_int:
            seen_int.add(name)
            internal_dedup.append((name, desc, reqs))
    internal = internal_dedup

    # Dependencies
    h_inc_i, h_inc_s = extract_includes(h_text)
    c_inc_i, c_inc_s = extract_includes(combined_c)
    # Merge, deduplicate
    int_deps = list(OrderedDict.fromkeys(h_inc_i + c_inc_i))
    sys_deps = list(OrderedDict.fromkeys(h_inc_s + c_inc_s))
    # Remove self-include
    int_deps = [d for d in int_deps if d != f'{base_name}.h']

    # Call graph
    all_func_names = set(pub_names) | {n for n, _, _r in internal}
    call_graph = {}
    if not no_tree and combined_c:
        call_graph = build_call_graph(all_func_names, combined_c)

    return {
        'base'       : base_name,
        'h_path'     : h_path,
        'c_path'     : c_path,
        'plat_c_path': plat_c_path,
        'role'       : role,
        'docs'       : docs,
        'pub_names'  : pub_names,
        'internal'   : internal,
        'int_deps'   : int_deps,
        'sys_deps'   : sys_deps,
        'call_graph' : call_graph,
        'all_funcs'  : all_func_names,
    }


# ---------------------------------------------------------------------------
# Markdown rendering
# ---------------------------------------------------------------------------

def md_module(mod, max_depth, no_tree):
    """Return Markdown string for one module."""
    lines = []
    base = mod['base']

    # Title
    files = []
    if mod['h_path']:     files.append(f'`src/{base}.h`')
    if mod['c_path']:     files.append(f'`src/{base}.c`')
    if mod['plat_c_path']:
        plat_dir = os.path.dirname(mod['plat_c_path']).replace('\\', '/')
        plat_dir = os.path.basename(plat_dir)
        files.append(f'`{plat_dir}/{os.path.basename(mod["plat_c_path"])}`')

    lines.append(f'## Module: `{base}`  —  {", ".join(files)}\n')

    # Role
    lines.append('### Role\n')
    if mod['role']:
        for line in mod['role'].splitlines():
            lines.append(line)
    else:
        lines.append('*(no header comment found)*')
    lines.append('')

    # Public API
    lines.append('### Public API\n')
    if mod['pub_names']:
        lines.append('| Function | Description | REQ |')
        lines.append('|---|---|---|')
        for name in mod['pub_names']:
            info = mod['docs'].get(name)
            desc = info['desc'] if info else '—'
            reqs = info['reqs'] if info else []
            req_str = ', '.join(f'`{r}`' for r in reqs) if reqs else '—'
            lines.append(f'| `{name}()` | {desc} | {req_str} |')
    else:
        lines.append('*(no public functions)*')
    lines.append('')

    # Parameters + Returns detail (collapsed per function)
    has_detail = any(
        mod['docs'].get(n) and
        (mod['docs'][n]['params'] or mod['docs'][n]['returns'])
        for n in mod['pub_names']
    )
    if has_detail:
        lines.append('#### API Detail\n')
        for name in mod['pub_names']:
            info = mod['docs'].get(name)
            if not info:
                continue
            if not info['params'] and not info['returns']:
                continue
            lines.append(f'<details><summary><code>{name}()</code></summary>\n')
            if info['params']:
                lines.append('**Parameters:**\n')
                lines.append('| Name | Dir | Description |')
                lines.append('|---|---|---|')
                for pname, pdir, pdesc in info['params']:
                    lines.append(f'| `{pname}` | `[{pdir}]` | {pdesc} |')
                lines.append('')
            if info['returns']:
                lines.append('**Returns:**\n')
                for r in info['returns']:
                    lines.append(f'- {r}')
                lines.append('')
            lines.append('</details>\n')

    # Key Internal Functions
    lines.append('### Key Internal Functions\n')
    orphans = []
    if mod['internal']:
        lines.append('| Function | Description | REQ |')
        lines.append('|---|---|---|')
        for name, desc, reqs in mod['internal']:
            desc_str = desc if desc else '—'
            req_str  = ', '.join(f'`{r}`' for r in reqs) if reqs else '—'
            lines.append(f'| `{name}()` | {desc_str} | {req_str} |')
            if not reqs:
                orphans.append(name)
    else:
        lines.append('*(none)*')
    lines.append('')

    # Public API orphans
    for name in mod['pub_names']:
        info = mod['docs'].get(name)
        if not info or not info.get('reqs'):
            orphans.append(name)

    if orphans:
        lines.append('### Untraced Functions (no @req tag)\n')
        lines.append('> These functions have no `@req` tag — '
                     'add one or mark as `@req —` if intentional.\n')
        for fn in orphans:
            lines.append(f'- `{fn}()`')
        lines.append('')

    # Dependencies
    lines.append('### External Dependencies\n')
    if mod['int_deps'] or mod['sys_deps']:
        if mod['int_deps']:
            lines.append('**Project headers:** ' +
                         ', '.join(f'`{d}`' for d in mod['int_deps']))
        if mod['sys_deps']:
            lines.append('**System headers:** ' +
                         ', '.join(f'`<{d}>`' for d in mod['sys_deps']))
    else:
        lines.append('*(none)*')
    lines.append('')

    # Call graph
    if not no_tree and mod['call_graph']:
        lines.append('### Call Graph\n')
        lines.append('```')
        cg = mod['call_graph']
        roots = find_entry_points(cg)
        # Also show public API entry points that may not be "roots" in the
        # strict sense (they're called from outside the module)
        pub_set = set(mod['pub_names'])
        shown_roots = (roots & pub_set) or roots
        tree_lines = render_tree(cg, shown_roots, max_depth)
        if tree_lines:
            lines += tree_lines
        else:
            lines.append('(no cross-function calls detected)')
        lines.append('```')
        lines.append('')

        # Full caller→callees table
        lines.append('#### Caller → Callees\n')
        lines.append('| Caller | Calls |')
        lines.append('|---|---|')
        for fn in sorted(cg.keys()):
            callees = sorted(cg[fn])
            if callees:
                callee_str = ', '.join(f'`{c}()`' for c in callees)
                lines.append(f'| `{fn}()` | {callee_str} |')
        lines.append('')

    lines.append('---\n')
    return '\n'.join(lines)


# ---------------------------------------------------------------------------
# Discovery
# ---------------------------------------------------------------------------

def discover_modules(src_dir, plat_dir, filter_mods):
    """
    Return list of (base_name, h_path, c_path, plat_c_path).
    Only modules that have at least a .h or a .c are included.
    """
    h_files = {os.path.splitext(f)[0]: os.path.join(src_dir, f)
               for f in os.listdir(src_dir) if f.endswith('.h')}
    c_files = {os.path.splitext(f)[0]: os.path.join(src_dir, f)
               for f in os.listdir(src_dir) if f.endswith('.c')}

    plat_c = {}
    if plat_dir and os.path.isdir(plat_dir):
        for f in os.listdir(plat_dir):
            if f.endswith('.c'):
                stem = os.path.splitext(f)[0]
                # Match by suffix: win_gui → gui, win_D2D → D2D
                for base in list(h_files.keys()) + list(c_files.keys()):
                    if stem.endswith(base) or stem.endswith(f'_{base}'):
                        plat_c[base] = os.path.join(plat_dir, f)
                        break

    all_bases = sorted(set(h_files) | set(c_files))

    # Exclude use_cases and main (not modules)
    excluded = {'main', 'use_cases', 'common'}
    all_bases = [b for b in all_bases if b not in excluded]

    if filter_mods:
        keep = set(filter_mods.split(','))
        all_bases = [b for b in all_bases if b in keep]

    modules = []
    for base in all_bases:
        modules.append((
            base,
            h_files.get(base),
            c_files.get(base),
            plat_c.get(base),
        ))
    return modules


# ---------------------------------------------------------------------------
# REQ cross-check
# ---------------------------------------------------------------------------

def parse_sr_reqs(sr_path):
    """
    Parse 2-SR.md and return a dict: req_id -> list of function names
    extracted from the Impl. column of REQ tables.

    Table format (from §4.6 CLAUDE.md R17):
      | REQ-XXX-NNN | description | UC/status | func1(), func2() |
    """
    reqs = {}
    try:
        text = open(sr_path, encoding='utf-8', errors='replace').read()
    except OSError as e:
        print(f'ERROR reading {sr_path}: {e}', file=sys.stderr)
        return reqs

    # Match table rows containing a REQ id
    RE_SR_ROW = re.compile(
        r'\|\s*(REQ-[A-Z]+-\d+)\s*\|'   # group 1: REQ id
        r'[^|]*\|'                        # description column
        r'[^|]*\|'                        # status/UC column
        r'([^|]*)\|',                     # group 2: Impl. column
    )
    for m in RE_SR_ROW.finditer(text):
        req_id   = m.group(1)
        impl_col = m.group(2).strip()
        # Extract function names: word chars followed by ()
        funcs = re.findall(r'(\w+)\(\)', impl_col)
        reqs[req_id] = funcs
    return reqs


def check_reqs(modules, sr_path):
    """
    Cross-check @req tags in source against 2-SR.md Impl. column.
    Prints a report and returns True if no discrepancies found.
    """
    print(f'\n=== REQ cross-check against {sr_path} ===')

    # Collect all @req tags from code
    code_reqs = defaultdict(list)   # req_id -> [func_name, ...]
    for mod in modules:
        for name in mod['pub_names']:
            info = mod['docs'].get(name)
            for r in (info['reqs'] if info else []):
                code_reqs[r].append(name)
        for name, _desc, reqs in mod['internal']:
            for r in reqs:
                code_reqs[r].append(name)

    sr_reqs = parse_sr_reqs(sr_path)

    ok = True

    # Tags in code that are not in 2-SR.md at all
    missing_in_sr = sorted(set(code_reqs) - set(sr_reqs))
    if missing_in_sr:
        ok = False
        print('\n[WARN] @req tags in code not found in SR table:')
        for r in missing_in_sr:
            fns = ', '.join(code_reqs[r])
            print(f'  {r}  (used in: {fns})')

    # REQs in 2-SR.md Impl. column that have no matching @req tag in code
    missing_in_code = sorted(
        r for r, funcs in sr_reqs.items()
        if funcs and r not in code_reqs
    )
    if missing_in_code:
        ok = False
        print('\n[WARN] REQs with Impl. in SR but no @req tag in code:')
        for r in missing_in_code:
            print(f'  {r}  (SR says: {", ".join(sr_reqs[r])})')

    # REQs in 2-SR.md Impl. with mismatched function names
    mismatched = []
    for r in sorted(set(code_reqs) & set(sr_reqs)):
        sr_funcs   = set(sr_reqs[r])
        code_funcs = set(code_reqs[r])
        if sr_funcs and sr_funcs != code_funcs:
            mismatched.append((r, sr_funcs, code_funcs))
    if mismatched:
        print('\n[INFO] REQs with function name mismatch (SR vs code):')
        for r, sr_f, code_f in mismatched:
            print(f'  {r}')
            print(f'    SR  : {sorted(sr_f)}')
            print(f'    code: {sorted(code_f)}')

    if ok and not mismatched:
        total = len(sr_reqs)
        tagged = len(set(code_reqs) & set(sr_reqs))
        print(f'\n[OK] {tagged}/{total} REQs with Impl. are tagged in code.')
    print()
    return ok


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description='Generate 4-DD.md Detailed Description for ST4Ever')
    parser.add_argument('--src',   default='src',
                        help='Source directory (default: src/)')
    parser.add_argument('--plat',  default='win',
                        help='Platform directory (default: win/)')
    parser.add_argument('--out',   default='4 - DD.md',
                        help='Output file (default: "4 - DD.md")')
    parser.add_argument('--mods',  default='',
                        help='Comma-separated module names to include '
                             '(default: all)')
    parser.add_argument('--depth', type=int, default=3,
                        help='Max call-tree depth (default: 3)')
    parser.add_argument('--no-tree', action='store_true',
                        help='Omit call-graph section')
    parser.add_argument('--check-reqs', metavar='SR_FILE', default='',
                        help='Cross-check @req tags against 2-SR.md')
    args = parser.parse_args()

    src_dir  = args.src
    plat_dir = args.plat
    out_file = args.out

    if not os.path.isdir(src_dir):
        print(f'ERROR: source directory not found: {src_dir}', file=sys.stderr)
        sys.exit(1)

    modules = discover_modules(src_dir, plat_dir, args.mods.strip())
    if not modules:
        print('ERROR: no modules found.', file=sys.stderr)
        sys.exit(1)

    print(f'Generating {out_file} for {len(modules)} module(s)...')

    # Document header
    doc_lines = [
        '# ST4Ever — 4 - DD : Detailed Description',
        '',
        '> **Auto-generated** by `tools/gen_dd.py` — do not edit manually.',
        '> Re-run after a functional group of UCs is complete.',
        '',
        '## Table of Contents',
        '',
    ]
    for base, h, c, pc in modules:
        doc_lines.append(f'- [`{base}`](#module-{base.lower()})')
    doc_lines.append('')
    doc_lines.append('---')
    doc_lines.append('')

    # Per-module sections
    for base, h_path, c_path, plat_c_path in modules:
        print(f'  processing {base}...')
        try:
            mod = process_module(base, h_path, c_path, plat_c_path,
                                 args.depth, args.no_tree)
            doc_lines.append(md_module(mod, args.depth, args.no_tree))
        except Exception as exc:
            print(f'  WARNING: {base} failed: {exc}', file=sys.stderr)
            doc_lines.append(f'## Module: `{base}`\n\n*(processing error: {exc})*\n\n---\n')

    output = '\n'.join(doc_lines)

    with open(out_file, 'w', encoding='utf-8') as f:
        f.write(output)

    print(f'Done. {out_file} written ({len(output):,} chars, '
          f'{len(modules)} modules).')

    if args.check_reqs.strip():
        processed_mods = []
        for base, h_path, c_path, plat_c_path in modules:
            try:
                processed_mods.append(
                    process_module(base, h_path, c_path, plat_c_path,
                                   args.depth, no_tree=True)
                )
            except Exception:
                pass
        check_reqs(processed_mods, args.check_reqs.strip())


if __name__ == '__main__':
    main()
