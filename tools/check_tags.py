#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
check_tags.py - Cross-check source code coverage tags against
use_case_*.c traceability, per CLAUDE.md R24.

A "tag" is a comment of the form:

    /* -- [MODULE]N. <comment> -- */

placed in a src/*.c (or win/*.c, linux/*.c) file to mark a block of
code that must be exercised by a test. use_case_*.c files re-use the
same tag text in two places:

  1. In the "Code Coverage:" section of a test function's docstring
     header (as a bullet list, without the surrounding /* */).
  2. Inline, immediately upstream of an "INTENT[...]" block, inside
     the test function's body.

This script performs 7 checks (see CLAUDE.md R24):

  [0] Every "/* -- [MODULE]N. ... -- */" tag is well-formed (closing
      '--' present) - a malformed one silently swallows source code
      up to the next tag's closing '-- */' otherwise.
  [1] INT-xxx-yyy and TC-xxx-yyy identifiers are unique across *all*
      use_case_*.c files: an INT id labels exactly one INTENT[...]
      block, a TC id is declared by exactly one INTENT chain and used
      as an inline UC_TEST/UC_CHECK label at most once.
  [2] Every tag defined in the source tree appears verbatim in at
      least one use_case_*.c file (informational: R24 is applied
      progressively, one tag at a time).
  [3] For every test function with a "Code Coverage:" header, each
      listed tag is (a) recopied inline in the function body and
      (b) upstream of at least one INTENT[...] block in that body.
  [4] Every tag used inline in a use_case_*.c file maps to exactly
      one source location, with identical (whitespace-normalized)
      text.
  [5] Heuristic ALERT (not a hard failure) when an INTENT's
      description shares little vocabulary with the tag(s) it
      follows - flagged for manual review.
  [6] Coverage report per module and per use_case file. Only
      use_case_00.c and use_case_01.c currently apply the full R24
      strategy (Code Coverage header + inline tag + INTENT linking);
      other use_case_*.c files are reported informationally.

Usage:
    python tools/check_tags.py [--src DIR [DIR ...]] [--uc DIR]
                                [--module MODULE] [--alert-threshold F]
                                [--strict] [-v]

Exit code: 0 always, unless --strict is given and at least one
ERROR-level finding was reported (ALERTs and informational coverage
gaps never affect the exit code).
"""

import argparse
import glob
import os
import re
import sys
from collections import defaultdict

# ---------------------------------------------------------------------------
# Regexes
# ---------------------------------------------------------------------------

# A single self-contained tag comment: /* -- [MODULE]N. <text> -- */
# Matches whether it is written on one line or wrapped over several
# (continuation lines keep their leading '*', stripped later). The
# negative lookahead forbids the text from crossing into another
# comment ('/*'): a tag missing its closing '--' would otherwise let
# the lazy '.*?' swallow everything up to the *next* tag's '-- */'
# (silently misreporting both tags). Such malformed tags are instead
# caught by find_malformed_tags() below.
TAG_RE = re.compile(
    r'/\*\s*--\s*\[(?P<module>[A-Za-z_][A-Za-z0-9_]*)\](?P<num>\d+)\.\s*'
    r'(?P<text>(?:(?!/\*).)*?)\s*--\s*\*/',
    re.DOTALL)

# Just the opening of a tag comment, used to detect malformed tags
# (missing closing '--') that TAG_RE deliberately refuses to match.
OPEN_TAG_RE = re.compile(
    r'/\*\s*--\s*\[(?P<module>[A-Za-z_][A-Za-z0-9_]*)\](?P<num>\d+)\.')

# An INTENT block: /* INTENT[chain]: <description> */
INTENT_RE = re.compile(
    r'/\*\s*INTENT\[(?P<chain>.*?)\]:\s*(?P<desc>.*?)\*/',
    re.DOTALL)

INTENT_ID_RE = re.compile(r'INT-[A-Za-z]+-\d+')

# TC reference inside an INTENT chain: "TC-STM-005...010" (inclusive
# range), "TC-TRC-011..012" (also a range - both '..' and '...' are
# used in the codebase), or "TC-XXX-001,004" (explicit list, per R14).
TC_CHAIN_REF_RE = re.compile(
    r'TC-(?P<module>[A-Za-z]+)-(?P<start>\d+)'
    r'(?P<rest>(?:\s*(?:\.\.\.|\.\.|,)\s*\d+)*)')

# A TC id used as an inline per-assertion label, e.g.
# UC_TEST("[N] (TC-STM-037) ...", ...) / UC_CHECK("(INT-EXE-002) [Chk]
# ..." ...). Restricted to UC_TEST(/UC_CHECK( so the mirrored id inside
# a "[SKIP] (TC-xxx-NNN) ..." printf() fallback isn't double-counted.
INLINE_TC_RE = re.compile(
    r'\bUC_(?:TEST|CHECK)\(\s*"[^"]*?\(TC-(?P<module>[A-Za-z]+)-(?P<num>\d+)\)')

CODE_COVERAGE_RE = re.compile(
    r'Code Coverage:(?P<body>.*?)(?:\*\s*Parameters:|\Z)',
    re.DOTALL)

STOPWORDS = {
    'the', 'a', 'an', 'is', 'are', 'to', 'of', 'if', 'and', 'or', 'in',
    'on', 'with', 'for', 'must', 'not', 'no', 'at', 'as', 'this', 'that',
    'it', 'be', 'by', 'from', 'when', 'then', 'its', 'was', 'were', 'so',
    'do', 'does', 'has', 'have', 'had', 'once', 'both', 'still', 'also',
    'given', 'any', 'all', 'same', 'call', 'check', 'checks', 'attempt',
    'attempting',
}


# ---------------------------------------------------------------------------
# Small helpers
# ---------------------------------------------------------------------------

def normalize(raw_text):
    """Collapse a (possibly multi-line, '*'-prefixed) comment fragment
    into a single-space-separated string, for identity comparison."""
    lines = raw_text.split('\n')
    cleaned = []
    for line in lines:
        stripped = line.strip()
        if stripped.startswith('*'):
            stripped = stripped[1:].strip()
        if stripped:
            cleaned.append(stripped)
    return re.sub(r'\s+', ' ', ' '.join(cleaned)).strip()


def parse_tc_chain_refs(chain_text):
    """Expand every TC-xxx-NNN reference in an INTENT chain string into
    the set of individual (module, num) it covers - '...'/'..' means an
    inclusive range, ',' means an explicit list (see R14)."""
    refs = set()
    for m in TC_CHAIN_REF_RE.finditer(chain_text):
        module = m.group('module')
        start = int(m.group('start'))
        rest = m.group('rest')
        nums = [start]
        parts = re.findall(r'(\.\.\.|\.\.|,)\s*(\d+)', rest) if rest else []
        if parts:
            if parts[0][0] in ('...', '..'):
                nums = list(range(start, int(parts[-1][1]) + 1))
            else:
                nums = [start] + [int(p[1]) for p in parts]
        for n in nums:
            refs.add((module, n))
    return refs


def stem(word):
    """Very small suffix stripper (not a real stemmer) so that morphological
    variants such as word/words or read/reading/read don't hide a real
    vocabulary match from the check-4 heuristic."""
    for suf in ('ing', 'edly', 'edness', 'ed', 'es', 's'):
        if word.endswith(suf) and len(word) - len(suf) >= 3:
            return word[:-len(suf)]
    return word


def tokenize(text):
    words = re.findall(r'[A-Za-z0-9_]+', text.lower())
    return {stem(w) for w in words if len(w) > 1 and w not in STOPWORDS}


def read_file(path):
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        return f.read()


def line_of(text, pos):
    return text.count('\n', 0, pos) + 1


def find_malformed_tags(text):
    """Detect '/* -- [MODULE]N. ...' openings that TAG_RE refused to
    match (usually a missing closing '--' before '*/'). Returns a list
    of dicts {module, num, pos, line, snippet}."""
    found = []
    for om in OPEN_TAG_RE.finditer(text):
        m = TAG_RE.match(text, om.start())
        if m is not None and m.start() == om.start():
            continue
        snippet = text[om.start():om.start() + 120].replace('\n', ' ')
        found.append({
            'module': om.group('module'),
            'num': int(om.group('num')),
            'pos': om.start(),
            'line': line_of(text, om.start()),
            'snippet': snippet.strip(),
        })
    return found


def find_matching_brace(text, open_pos):
    """text[open_pos] must be '{'. Returns index just past the matching
    '}', with a crude but sufficient C-comment/string/char scanner."""
    depth = 0
    i = open_pos
    n = len(text)
    in_str = in_chr = in_line_comment = in_block_comment = False
    while i < n:
        c = text[i]
        if in_line_comment:
            if c == '\n':
                in_line_comment = False
        elif in_block_comment:
            if c == '*' and i + 1 < n and text[i + 1] == '/':
                in_block_comment = False
                i += 1
        elif in_str:
            if c == '\\':
                i += 1
            elif c == '"':
                in_str = False
        elif in_chr:
            if c == '\\':
                i += 1
            elif c == "'":
                in_chr = False
        else:
            if c == '/' and i + 1 < n and text[i + 1] == '/':
                in_line_comment = True
                i += 1
            elif c == '/' and i + 1 < n and text[i + 1] == '*':
                in_block_comment = True
                i += 1
            elif c == '"':
                in_str = True
            elif c == "'":
                in_chr = True
            elif c == '{':
                depth += 1
            elif c == '}':
                depth -= 1
                if depth == 0:
                    return i + 1
        i += 1
    return n


_MASK_SENTINEL = '\x01'


def mask_comments_and_strings(text):
    """Return a same-length/same-position copy of text where every
    character belonging to a comment or a string/char literal
    (including its delimiters) is replaced by a sentinel byte that
    can never occur in real C source, and real newlines are kept so
    line numbers stay meaningful. Used to locate function boundaries
    without a fragile regex being confused by '(', ')', '{', '}'
    appearing inside comments or string literals."""
    n = len(text)
    out = list(text)
    i = 0
    in_str = in_chr = in_line_comment = in_block_comment = False
    while i < n:
        c = text[i]
        if in_line_comment:
            if c == '\n':
                in_line_comment = False
            else:
                out[i] = _MASK_SENTINEL
        elif in_block_comment:
            if c == '*' and i + 1 < n and text[i + 1] == '/':
                out[i] = _MASK_SENTINEL
                out[i + 1] = _MASK_SENTINEL
                in_block_comment = False
                i += 1
            elif c != '\n':
                out[i] = _MASK_SENTINEL
        elif in_str:
            if c == '\\' and i + 1 < n:
                out[i] = _MASK_SENTINEL
                out[i + 1] = _MASK_SENTINEL
                i += 1
            else:
                out[i] = _MASK_SENTINEL
                if c == '"':
                    in_str = False
        elif in_chr:
            if c == '\\' and i + 1 < n:
                out[i] = _MASK_SENTINEL
                out[i + 1] = _MASK_SENTINEL
                i += 1
            else:
                out[i] = _MASK_SENTINEL
                if c == "'":
                    in_chr = False
        else:
            if c == '/' and i + 1 < n and text[i + 1] == '/':
                in_line_comment = True
                out[i] = _MASK_SENTINEL
            elif c == '/' and i + 1 < n and text[i + 1] == '*':
                in_block_comment = True
                out[i] = _MASK_SENTINEL
            elif c == '"':
                in_str = True
                out[i] = _MASK_SENTINEL
            elif c == "'":
                in_chr = True
                out[i] = _MASK_SENTINEL
        i += 1
    return ''.join(out)


_SIG_ALLOWED = set(
    'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_* \t\r\n')
_CONTROL_KEYWORDS = {'if', 'for', 'while', 'switch', 'do', 'else',
                     'return', 'sizeof', 'defined'}
_IDENT_RE = re.compile(r'^[A-Za-z_]\w*$')


def find_function_defs(text):
    """Scan text (via its masked copy) for top-level `name(...) { ...`
    definitions - i.e. an opening brace at brace-depth 0, immediately
    (module whitespace) preceded by a ')', itself closing a parameter
    list preceded by an identifier that isn't a control-flow keyword.
    Returns a list of {name, sig_start, body_open} where body_open is
    the index of the opening '{' and sig_start is the index where the
    return-type/qualifier run before the function name begins (used
    to locate the docstring comment immediately preceding it)."""
    masked = mask_comments_and_strings(text)
    n = len(masked)
    depth = 0
    i = 0
    results = []
    while i < n:
        c = masked[i]
        if c == '{':
            if depth == 0:
                j = i - 1
                while j >= 0 and masked[j] in ' \t\r\n':
                    j -= 1
                if j >= 0 and masked[j] == ')':
                    pdepth = 1
                    k = j - 1
                    while k >= 0 and pdepth > 0:
                        if masked[k] == ')':
                            pdepth += 1
                        elif masked[k] == '(':
                            pdepth -= 1
                        k -= 1
                    paren_open = k + 1
                    m = paren_open - 1
                    while m >= 0 and masked[m] in ' \t\r\n':
                        m -= 1
                    name_end = m + 1
                    name_start = name_end
                    while name_start > 0 and (
                            masked[name_start - 1].isalnum()
                            or masked[name_start - 1] == '_'):
                        name_start -= 1
                    name = masked[name_start:name_end]
                    if (name and _IDENT_RE.match(name)
                            and name not in _CONTROL_KEYWORDS):
                        sig_start = name_start
                        while (sig_start > 0
                               and masked[sig_start - 1] in _SIG_ALLOWED):
                            sig_start -= 1
                        results.append({
                            'name': name,
                            'sig_start': sig_start,
                            'body_open': i,
                        })
            depth += 1
        elif c == '}':
            if depth > 0:
                depth -= 1
        i += 1
    return results


# ---------------------------------------------------------------------------
# Source-side scan: every /* -- [MODULE]N. text -- */ in src/win/linux
# ---------------------------------------------------------------------------

def gather_source_files(dirs):
    files = []
    for d in dirs:
        if not os.path.isdir(d):
            continue
        for root, _dirnames, names in os.walk(d):
            for name in names:
                if name.endswith('.c'):
                    files.append(os.path.join(root, name))
    return sorted(files)


def scan_source_tags(files):
    """Returns (tags, malformed):
      tags      dict {(module, num): [ {file, line, text}, ... ] }
      malformed list of {file, module, num, line, snippet}
    """
    tags = defaultdict(list)
    malformed = []
    for path in files:
        text = read_file(path)
        for m in TAG_RE.finditer(text):
            key = (m.group('module'), int(m.group('num')))
            tags[key].append({
                'file': path,
                'line': line_of(text, m.start()),
                'text': normalize(m.group('text')),
            })
        for bad in find_malformed_tags(text):
            bad['file'] = path
            malformed.append(bad)
    return tags, malformed


# ---------------------------------------------------------------------------
# use_case_*.c side scan
# ---------------------------------------------------------------------------

def gather_usecase_files(uc_dir):
    return sorted(glob.glob(os.path.join(uc_dir, 'use_case_*.c')))


class TestFunction:
    def __init__(self, name, path, body_start, body_end):
        self.name = name
        self.path = path
        self.body_start = body_start
        self.body_end = body_end
        self.doc_tags = {}      # (module, num) -> normalized text
        self.inline_tags = []   # list of dict {module,num,text,pos,line}
        self.intents = []       # list of dict {id, chain, desc, pos, line}
        self.upstream = defaultdict(set)  # intent_id -> set of (mod,num)
        self.tc_inline = []     # list of dict {module,num,line}


def parse_usecase_file(path):
    """Returns (functions, malformed):
      functions  list of TestFunction (functions whose preceding
                 docstring contains a 'Code Coverage:' section)
      malformed  list of {module, num, line, snippet} - inline tags
                 missing their closing '--' (see find_malformed_tags)
    """
    text = read_file(path)
    functions = []
    malformed = find_malformed_tags(text)
    for bad in malformed:
        bad['file'] = path

    # Comments in the *original* text, indexed by their end position,
    # so a function whose signature is immediately preceded (module
    # whitespace) by one can be matched in O(1).
    comment_by_end = {m.end(): m
                       for m in re.finditer(r'/\*.*?\*/', text, re.DOTALL)}

    for fdef in find_function_defs(text):
        preceding = comment_by_end.get(fdef['sig_start'])
        if preceding is None:
            continue
        cc_m = CODE_COVERAGE_RE.search(preceding.group(0))
        if not cc_m:
            continue

        body_start = fdef['body_open'] + 1  # just after the opening '{'
        body_end = find_matching_brace(text, fdef['body_open'])

        tf = TestFunction(fdef['name'], path, body_start, body_end)

        # --- doc tags (Code Coverage bullet list) ---
        cleaned = normalize(cc_m.group('body'))
        # The bullet text itself may legitimately contain "--" glued to a
        # word (e.g. "-t/--trace", "Reject --script ..."), so those are
        # not valid closing markers. The real closing "--" is always
        # followed by whitespace (next bullet/module line) or the end of
        # the list - never directly by another word character.
        for bm in re.finditer(
                r'--\s*\[(?P<module>[A-Za-z_][A-Za-z0-9_]*)\]'
                r'(?P<num>\d+)\.\s*(?P<text>.*?)\s*--(?=\s|\Z)',
                cleaned):
            key = (bm.group('module'), int(bm.group('num')))
            tf.doc_tags[key] = normalize(bm.group('text'))

        body_text = text[body_start:body_end]

        # --- inline tags within the function body ---
        for tm in TAG_RE.finditer(body_text):
            tf.inline_tags.append({
                'module': tm.group('module'),
                'num': int(tm.group('num')),
                'text': normalize(tm.group('text')),
                'pos': tm.start(),
                'line': line_of(text, body_start + tm.start()),
            })

        # --- INTENT blocks within the function body ---
        for im in INTENT_RE.finditer(body_text):
            id_m = INTENT_ID_RE.search(im.group('chain'))
            tf.intents.append({
                'id': id_m.group(0) if id_m else im.group('chain').strip(),
                'chain': im.group('chain').strip(),
                'desc': normalize(im.group('desc')),
                'pos': im.start(),
                'line': line_of(text, body_start + im.start()),
            })

        # --- TC ids used as inline per-assertion labels ---
        for tcm in INLINE_TC_RE.finditer(body_text):
            tf.tc_inline.append({
                'module': tcm.group('module'),
                'num': int(tcm.group('num')),
                'line': line_of(text, body_start + tcm.start()),
            })

        # --- segment inline tags into "upstream of next INTENT" ---
        events = (
            [('tag', t['pos'], (t['module'], int(t['num']))) for t in tf.inline_tags]
            + [('intent', i['pos'], i['id']) for i in tf.intents]
        )
        events.sort(key=lambda e: e[1])
        pending = set()
        for kind, _pos, payload in events:
            if kind == 'tag':
                pending.add(payload)
            else:
                tf.upstream[payload] |= pending
                pending = set()

        functions.append(tf)

    return functions, malformed


# ---------------------------------------------------------------------------
# Report sections
# ---------------------------------------------------------------------------

def rel(path):
    return os.path.relpath(path).replace('\\', '/')


def check0_malformed_tags(malformed, out):
    out.append('')
    out.append('=' * 78)
    out.append('[0] Malformed tags (missing closing "--" before "*/")')
    out.append('=' * 78)
    if not malformed:
        out.append('')
        out.append('  None found.')
        return []
    out.append('')
    out.append('  These swallow everything up to the *next* "-- */" they')
    out.append('  find (including subsequent, otherwise well-formed tags)')
    out.append('  and were EXCLUDED from all other checks below - fix the')
    out.append('  comment first, then re-run.')
    for bad in malformed:
        out.append('')
        out.append('  ERROR   [%s]%d  %s:%d' %
                    (bad['module'], bad['num'], rel(bad['file']),
                     bad['line']))
        out.append('          %s...' % bad['snippet'])
    return malformed


def check1_id_uniqueness(functions_by_file, out, verbose):
    """INT-xxx-yyy and TC-xxx-yyy are meant to be unique identifiers,
    assigned once and never reused - across *all* use_case_*.c files,
    not just within one (R23: "les numéros TC sont séquentiels et
    continus entre fichiers"). This is easy to get wrong by hand once
    many files are involved, so check it mechanically:
      - an INT-xxx-yyy id must label exactly one INTENT[...] block;
      - a TC-xxx-yyy id must be *declared* (via a chain range/list) by
        exactly one INTENT[...] block;
      - a TC-xxx-yyy id must be used as an inline UC_TEST/UC_CHECK
        label at most once (the matching "[SKIP] (TC-xxx-yyy) ..."
        printf() fallback is not counted - it mirrors the same test,
        not a new one)."""
    out.append('')
    out.append('=' * 78)
    out.append('[1] INT-xxx-yyy / TC-xxx-yyy identifier uniqueness')
    out.append('=' * 78)

    int_ids = defaultdict(list)
    tc_declared = defaultdict(list)
    tc_inline = defaultdict(list)

    for path, functions in functions_by_file.items():
        for tf in functions:
            for intent in tf.intents:
                int_ids[intent['id']].append({
                    'file': path, 'func': tf.name, 'line': intent['line'],
                })
                for key in parse_tc_chain_refs(intent['chain']):
                    tc_declared[key].append({
                        'file': path, 'func': tf.name,
                        'line': intent['line'], 'intent': intent['id'],
                    })
            for t in tf.tc_inline:
                tc_inline[(t['module'], t['num'])].append({
                    'file': path, 'func': tf.name, 'line': t['line'],
                })

    errors = []

    n_ok = 0
    for int_id, occs in sorted(int_ids.items()):
        if len(occs) > 1:
            msg = ('ERROR   %-14s declared by %d different INTENT[...]'
                   ' blocks (must be unique)' % (int_id, len(occs)))
            errors.append(('INT', int_id, msg))
            out.append('')
            out.append('  ' + msg)
            for o in occs:
                out.append('            %s:%d  (%s)' %
                            (rel(o['file']), o['line'], o['func']))
        else:
            n_ok += 1
    if verbose:
        out.append('')
        out.append('  %d/%d INT-xxx-yyy id(s) unique' %
                    (n_ok, len(int_ids)))

    n_ok = 0
    for key, occs in sorted(tc_declared.items(), key=lambda kv: (kv[0][0], kv[0][1])):
        if len(occs) > 1:
            tag_label = 'TC-%s-%d' % key
            msg = ('ERROR   %-14s declared by %d different INTENT[...]'
                   ' chains (must be unique)' % (tag_label, len(occs)))
            errors.append(('TC-declared', tag_label, msg))
            out.append('')
            out.append('  ' + msg)
            for o in occs:
                out.append('            %s:%d  %s (%s)' %
                            (rel(o['file']), o['line'], o['intent'],
                             o['func']))
        else:
            n_ok += 1
    if verbose:
        out.append('')
        out.append('  %d/%d TC-xxx-yyy id(s) declared exactly once in an'
                    ' INTENT chain' % (n_ok, len(tc_declared)))

    n_ok = 0
    for key, occs in sorted(tc_inline.items(), key=lambda kv: (kv[0][0], kv[0][1])):
        if len(occs) > 1:
            tag_label = 'TC-%s-%d' % key
            msg = ('ERROR   %-14s used as an inline test label %d times'
                   ' (must label a single assertion)' %
                   (tag_label, len(occs)))
            errors.append(('TC-inline', tag_label, msg))
            out.append('')
            out.append('  ' + msg)
            for o in occs:
                out.append('            %s:%d  (%s)' %
                            (rel(o['file']), o['line'], o['func']))
        else:
            n_ok += 1
    if verbose:
        out.append('')
        out.append('  %d/%d TC-xxx-yyy id(s) used as a unique inline'
                    ' test label' % (n_ok, len(tc_inline)))

    if not errors:
        out.append('')
        out.append('  No duplicate INT-xxx-yyy or TC-xxx-yyy id found.')

    return errors


def check2_source_coverage(source_tags, uc_tag_index, out, verbose):
    out.append('')
    out.append('=' * 78)
    out.append('[2] Source tags not yet referenced in any use_case_*.c'
                ' (informational, R24 is applied progressively)')
    out.append('=' * 78)
    missing = []
    present = 0
    for key in sorted(source_tags, key=lambda k: (k[0], k[1])):
        occs = source_tags[key]
        loc = occs[0]
        if key in uc_tag_index:
            present += 1
            if verbose:
                out.append('  OK      [%s]%-3d  %s:%d' %
                            (key[0], key[1], rel(loc['file']), loc['line']))
        else:
            missing.append((key, loc))

    for key, loc in missing:
        out.append('  MISSING [%s]%-3d  %s:%-4d  "%s"' %
                    (key[0], key[1], rel(loc['file']), loc['line'],
                     loc['text']))

    total = len(source_tags)
    pct = (100.0 * present / total) if total else 0.0
    out.append('')
    out.append('  Total: %d/%d source tags referenced in use_case_*.c'
                ' (%.0f%%)' % (present, total, pct))
    return missing


def check3_function_consistency(functions_by_file, out, verbose):
    out.append('')
    out.append('=' * 78)
    out.append('[3] Test function header <-> inline tag <-> INTENT'
                ' consistency')
    out.append('=' * 78)
    errors = []
    for path in sorted(functions_by_file):
        functions = functions_by_file[path]
        if not functions:
            continue
        out.append('')
        out.append('  FILE %s' % rel(path))
        n_tags_total = 0
        n_tags_ok = 0
        for tf in functions:
            inline_keys = {(t['module'], t['num']) for t in tf.inline_tags}
            upstream_keys = set()
            for s in tf.upstream.values():
                upstream_keys |= s

            lines_for_func = []
            for key, doc_text in sorted(tf.doc_tags.items(),
                                         key=lambda kv: (kv[0][0], kv[0][1])):
                tag_label = '[%s]%d' % key
                n_tags_total += 1
                if key not in inline_keys:
                    msg = ('ERROR   %-10s declared in Code Coverage header'
                           ' but not recopied inline in the function body'
                           % tag_label)
                    errors.append((path, tf.name, key, msg))
                    lines_for_func.append('      ' + msg)
                elif key not in upstream_keys:
                    msg = ('ERROR   %-10s recopied inline but not placed'
                           ' upstream of any INTENT[...] block' % tag_label)
                    errors.append((path, tf.name, key, msg))
                    lines_for_func.append('      ' + msg)
                else:
                    inline_text = next(
                        t['text'] for t in tf.inline_tags
                        if (t['module'], t['num']) == key)
                    if inline_text != doc_text:
                        msg = ('WARN    %-10s header text differs from'
                               ' inline copy' % tag_label)
                        lines_for_func.append('      ' + msg)
                        lines_for_func.append(
                            '                header: "%s"' % doc_text)
                        lines_for_func.append(
                            '                inline: "%s"' % inline_text)
                    else:
                        n_tags_ok += 1
                        if verbose:
                            lines_for_func.append(
                                '      OK      %-10s' % tag_label)

            extra = inline_keys - set(tf.doc_tags)
            for key in sorted(extra):
                lines_for_func.append(
                    '      NOTE    [%s]%d inline in body but not listed in'
                    ' the Code Coverage header' % key)

            if lines_for_func:
                out.append('    FUNCTION %s()' % tf.name)
                out.extend(lines_for_func)
            elif verbose and tf.doc_tags:
                out.append('    FUNCTION %s() - all %d tag(s) OK' %
                            (tf.name, len(tf.doc_tags)))

        out.append('    -> %d/%d tag(s) OK in this file' %
                    (n_tags_ok, n_tags_total))
    return errors


def check4_usecase_to_source(source_tags, uc_tag_occurrences, out, verbose):
    out.append('')
    out.append('=' * 78)
    out.append('[4] use_case inline tag <-> unique source definition')
    out.append('=' * 78)
    errors = []
    for key in sorted(uc_tag_occurrences, key=lambda k: (k[0], k[1])):
        occs = uc_tag_occurrences[key]
        tag_label = '[%s]%d' % key
        srcs = source_tags.get(key)
        if not srcs:
            msg = ('ERROR   %-10s used in use_case_*.c but not defined in'
                   ' any source .c file' % tag_label)
            errors.append((key, msg))
            out.append('  ' + msg)
            for o in occs:
                out.append('            at %s:%d' %
                            (rel(o['file']), o['line']))
            continue
        if len(srcs) > 1:
            msg = ('ERROR   %-10s defined in %d different source'
                   ' locations (ambiguous)' % (tag_label, len(srcs)))
            errors.append((key, msg))
            out.append('  ' + msg)
            for s in srcs:
                out.append('            %s:%d' % (rel(s['file']), s['line']))
            continue

        src = srcs[0]
        mismatches = [o for o in occs if o['text'] != src['text']]
        if mismatches:
            msg = ('ERROR   %-10s text differs from source definition'
                   % tag_label)
            errors.append((key, msg))
            out.append('  ' + msg)
            out.append('            source : %s:%d  "%s"' %
                        (rel(src['file']), src['line'], src['text']))
            for o in mismatches:
                out.append('            use_case: %s:%d  "%s"' %
                            (rel(o['file']), o['line'], o['text']))
        elif verbose:
            out.append('  OK      %-10s %s:%d <-> %s:%d' %
                        (tag_label, rel(occs[0]['file']), occs[0]['line'],
                         rel(src['file']), src['line']))
    return errors


def check5_heuristic_alerts(functions_by_file, source_tags, out, threshold):
    out.append('')
    out.append('=' * 78)
    out.append('[5] Heuristic alerts: INTENT wording vs. tagged source code')
    out.append('    (manual review - not a hard failure)')
    out.append('=' * 78)
    alerts = []
    info_no_tag = 0
    for path, functions in sorted(functions_by_file.items()):
        for tf in functions:
            for intent in tf.intents:
                tag_keys = tf.upstream.get(intent['id'], set())
                if not tag_keys:
                    info_no_tag += 1
                    continue
                ref_text = ' '.join(
                    source_tags[k][0]['text']
                    for k in tag_keys if k in source_tags)
                intent_tokens = tokenize(intent['desc'])
                ref_tokens = tokenize(ref_text)
                if len(intent_tokens) < 3 or not ref_tokens:
                    continue
                overlap = intent_tokens & ref_tokens
                ratio = len(overlap) / len(intent_tokens)
                if ratio < threshold:
                    tag_labels = ', '.join('[%s]%d' % k for k in
                                            sorted(tag_keys))
                    out.append('')
                    out.append('  ALERT %s:%d  %s  upstream tags: %s' %
                                (rel(path), intent['line'], intent['id'],
                                 tag_labels))
                    out.append('        overlap %.0f%% (< %.0f%% threshold)'
                                ' - please review manually' %
                                (ratio * 100, threshold * 100))
                    out.append('        INTENT : "%s"' % intent['desc'])
                    out.append('        tag(s) : "%s"' % ref_text)
                    alerts.append((path, tf.name, intent['id'], ratio))

    if info_no_tag:
        out.append('')
        out.append('  (%d INTENT block(s) have no upstream tag at all -'
                    ' not flagged, see check [2])' % info_no_tag)
    if not alerts:
        out.append('')
        out.append('  No wording-overlap alerts below the %.0f%% threshold.'
                    % (threshold * 100))
    return alerts


def check6_coverage_report(source_tags, uc_tag_index, functions_by_file,
                            uc_files, strategy_files, out):
    out.append('')
    out.append('=' * 78)
    out.append('[6] Coverage report')
    out.append('=' * 78)

    by_module_total = defaultdict(int)
    by_module_covered = defaultdict(int)
    for key in source_tags:
        by_module_total[key[0]] += 1
        if key in uc_tag_index:
            by_module_covered[key[0]] += 1

    out.append('')
    out.append('  %-14s %10s %12s %8s' %
                ('MODULE', 'SRC TAGS', 'REFERENCED', 'COV %'))
    for module in sorted(by_module_total):
        total = by_module_total[module]
        covered = by_module_covered[module]
        pct = 100.0 * covered / total if total else 0.0
        out.append('  %-14s %10d %12d %7.0f%%' %
                    (module, total, covered, pct))

    out.append('')
    out.append('  %-32s %10s %8s' % ('USE_CASE FILE', 'FUNCTIONS', 'STRATEGY'))
    n_legacy = 0
    for path in uc_files:
        functions = functions_by_file.get(path, [])
        n_with_cc = sum(1 for tf in functions if tf.doc_tags)
        applies = 'R24 full' if os.path.basename(path) in strategy_files \
            else ('partial' if n_with_cc else '-')
        if applies == '-':
            n_legacy += 1
            continue
        out.append('  %-32s %10d %8s' %
                    (os.path.basename(path), n_with_cc, applies))
    if n_legacy:
        out.append('  (+ %d other use_case_*.c file(s) with no'
                    ' "Code Coverage:" header yet - pre-R24)' % n_legacy)

    out.append('')
    out.append('  NOTE: per CLAUDE.md R24, only use_case_00.c and'
                ' use_case_01.c currently implement the full')
    out.append('        tag traceability strategy (Code Coverage header +'
                ' inline tag + INTENT linking).')
    out.append('        Other use_case_*.c files are scanned for inline'
                ' tag reuse only, informationally.')


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description='Check /* -- [MODULE]N. <comment> -- */ traceability '
                     'between src/*.c and use_cases/use_case_*.c '
                     '(CLAUDE.md R24).')
    parser.add_argument('--src', nargs='+', default=['src', 'win', 'linux'],
                         help='Source directories to scan for tag'
                              ' definitions (default: src win linux)')
    parser.add_argument('--uc', default='use_cases',
                         help='Directory containing use_case_*.c files'
                              ' (default: use_cases)')
    parser.add_argument('--module', default=None,
                         help='Restrict all checks to a single MODULE name'
                              ' (e.g. ST, TRACE)')
    parser.add_argument('--alert-threshold', type=float, default=0.15,
                         help='Vocabulary-overlap ratio below which an'
                              ' INTENT is flagged for manual review'
                              ' (default: 0.15)')
    parser.add_argument('--strict', action='store_true',
                         help='Exit with status 1 if any ERROR was found')
    parser.add_argument('-v', '--verbose', action='store_true',
                         help='Also print OK entries, not just issues')
    parser.add_argument('--strategy-files', nargs='+',
                         default=['use_case_00.c', 'use_case_01.c'],
                         help='use_case_*.c basenames considered to fully'
                              ' apply the R24 strategy (default:'
                              ' use_case_00.c use_case_01.c)')
    args = parser.parse_args()

    src_files = gather_source_files(args.src)
    uc_files = gather_usecase_files(args.uc)

    if not src_files:
        print('No source .c files found under: %s' % ' '.join(args.src),
              file=sys.stderr)
        return 2
    if not uc_files:
        print('No use_case_*.c files found under: %s' % args.uc,
              file=sys.stderr)
        return 2

    source_tags, malformed_src = scan_source_tags(src_files)

    functions_by_file = {}
    malformed_uc = []
    for path in uc_files:
        functions, bad = parse_usecase_file(path)
        functions_by_file[path] = functions
        malformed_uc.extend(bad)

    if args.module:
        source_tags = {k: v for k, v in source_tags.items()
                        if k[0] == args.module}
        malformed_src = [b for b in malformed_src
                          if b['module'] == args.module]
        malformed_uc = [b for b in malformed_uc
                         if b['module'] == args.module]
        for path, functions in functions_by_file.items():
            for tf in functions:
                tf.doc_tags = {k: v for k, v in tf.doc_tags.items()
                               if k[0] == args.module}
                tf.inline_tags = [t for t in tf.inline_tags
                                   if t['module'] == args.module]

    # Global index of use_case inline tags: (module,num) -> [occurrences]
    uc_tag_occurrences = defaultdict(list)
    for path, functions in functions_by_file.items():
        for tf in functions:
            for t in tf.inline_tags:
                uc_tag_occurrences[(t['module'], t['num'])].append({
                    'file': path,
                    'line': t['line'],
                    'text': t['text'],
                })
    uc_tag_index = set(uc_tag_occurrences)

    out = []
    out.append('ST4Ever tag traceability check')
    out.append('  source dirs : %s  (%d files, %d tags)' %
                (', '.join(args.src), len(src_files), len(source_tags)))
    out.append('  use_case dir: %s  (%d files)' % (args.uc, len(uc_files)))
    if args.module:
        out.append('  module filter: %s' % args.module)

    errors0 = check0_malformed_tags(malformed_src + malformed_uc, out)
    errors1 = check1_id_uniqueness(functions_by_file, out, args.verbose)
    check2_source_coverage(source_tags, uc_tag_index, out, args.verbose)
    errors2 = check3_function_consistency(functions_by_file, out,
                                           args.verbose)
    errors3 = check4_usecase_to_source(source_tags, uc_tag_occurrences, out,
                                        args.verbose)
    alerts4 = check5_heuristic_alerts(functions_by_file, source_tags, out,
                                       args.alert_threshold)
    check6_coverage_report(source_tags, uc_tag_index, functions_by_file,
                            uc_files, set(args.strategy_files), out)

    n_errors = len(errors0) + len(errors1) + len(errors2) + len(errors3)
    out.append('')
    out.append('=' * 78)
    out.append('SUMMARY: %d error(s), %d alert(s) for manual review' %
                (n_errors, len(alerts4)))
    out.append('=' * 78)

    print('\n'.join(out))

    if args.strict and n_errors > 0:
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
