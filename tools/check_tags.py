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

This script performs 10 checks (see CLAUDE.md R24):

  [0] Every "/* -- [MODULE]N. ... -- */" tag is well-formed (closing
      '--' present) - a malformed one silently swallows source code
      up to the next tag's closing '-- */' otherwise.
  [1] INT-xxx-yyy and TC-xxx-yyy identifiers are unique across *all*
      use_case_*.c files: an INT id labels exactly one INTENT[...]
      block, a TC id is declared by exactly one INTENT chain and used
      as an inline UC_TEST/UC_CHECK label at most once.
  [2] Every tag defined in the source tree appears verbatim in at
      least one use_case_*.c file (informational: R24 is applied
      progressively, one tag at a time). A tag not referenced is
      reported as RATIONALE (documented as unreachable in an
      "Untestable tag rationale" comment block, CLAUDE.md R24.2) or
      MISSING (a true, un-analyzed gap) - see check [6] for the
      per-module tested/rationale/missing split.
  [3] For every test function with a "Code Coverage:" header, each
      listed tag is (a) recopied inline in the function body and
      (b) upstream of at least one INTENT[...] block in that body.
  [4] Every tag used inline in a use_case_*.c file maps to exactly
      one source location, with identical (whitespace-normalized)
      text.
  [5] Heuristic ALERT (not a hard failure) when an INTENT's
      description shares little vocabulary with the tag(s) it
      follows - flagged for manual review.
  [6] Coverage report per module and per use_case file, splitting
      each module's source tags into TESTED / RATIONALE / MISSING
      (TESTED + RATIONALE is expected to reach 100% - CLAUDE.md
      R24.2). Only use_case_00.c, use_case_01.c, use_case_02.c,
      use_case_03.c and use_case_04.c currently apply the full R24
      strategy (Code Coverage header + inline tag + INTENT linking);
      other use_case_*.c files are reported informationally.
  [7] Informational: static functions in src/win/linux with no
      preceding docstring comment (CLAUDE.md 4.6/4.7 expects one).
  [8] Informational: functions (static or public) whose body has
      neither a LOG_TRACE(...) call nor a "// No LOG_TRACE - <reason>"
      justification comment (see src/trace.c for the convention, and
      CLAUDE.md R22 for when omitting LOG_TRACE is legitimate).
  [9] Informational: for every static function whose preceding
      docstring was found by check [7], verify the docstring *fields*
      are actually correct (CLAUDE.md 4.6 + R24.2), not just present:
        - the first non-blank line reads "name() - <description>";
        - every "Parameters:" bullet name matches an actual signature
          parameter, and every signature parameter is documented;
        - a "Returns:" section is present and is not an obvious
          void/non-void mismatch (e.g. "Void - None" on a function
          that returns st_error_t);
        - if present, "This helper is currently called by:" lists
          exactly the callers found by scanning the same file for
          real call sites (a stale name, or a real caller missing
          from the list, is flagged).
  [10] Tag length: the "[MODULE]N. <text>" content of every source tag
       must not exceed --tag-max-len characters (default 90). A tag is
       a pointer to a chunk of code, not a summary of it - the function
       name does not need to be repeated in the text (the tag's own
       location already pins it down).
  [11] Tag placement: a tag must sit strictly inside a function's { }
       body (marking a chunk of code), never in the comment chain that
       precedes a function signature (its "header"/docstring) and never
       free-floating between two functions.

Usage:
    python tools/check_tags.py [--src DIR [DIR ...]] [--uc DIR]
                                [--module MODULE] [--alert-threshold F]
                                [--tag-max-len N] [--strict] [-v]

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

# The R24.2 "Untestable tag rationale" convention header (CLAUDE.md §5 R24)
# marking a comment block that documents, per-tag, why a source tag cannot
# be reached by any test in this binary (Category 3). Matched case-
# insensitively since it is prose, not a machine-format identifier.
RATIONALE_HEADER_RE = re.compile(r'untestable tag rationale', re.IGNORECASE)

# A bare "[MODULE]N" reference inside a rationale comment block - unlike
# TAG_RE, this is *not* the "-- ... --" tag-definition syntax, just a
# pointer to it in prose (e.g. "[DIR]46 (dir_node_load_children: ...)").
RATIONALE_TAG_REF_RE = re.compile(
    r'\[(?P<module>[A-Za-z_][A-Za-z0-9_]*)\](?P<num>\d+)\b')

DEFAULT_TAG_MAX_LEN = 90

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


def find_function_body_ranges(text):
    """Returns a list of (body_open, body_end) pairs, one per top-level
    function definition in text (via find_function_defs) - body_open is
    the index of the opening '{', body_end the index just past the
    matching '}'. Used to tell whether a tag comment sits strictly
    inside a function's body (the only place check [11] allows one) as
    opposed to its header/docstring comment chain or a free-floating
    spot between two functions."""
    ranges = []
    for fdef in find_function_defs(text):
        body_end = find_matching_brace(text, fdef['body_open'])
        ranges.append((fdef['body_open'], body_end))
    return ranges


def pos_in_any_range(pos, ranges):
    for start, end in ranges:
        if start <= pos < end:
            return True
    return False


def _is_ident_char_at(s, idx):
    if idx < 0 or idx >= len(s):
        return False
    c = s[idx]
    return c.isalnum() or c == '_'


def find_switch_pre_case_zones(text):
    """For every 'switch (...) { ... }' compound statement in text,
    returns the (zone_start, zone_end) span running from the body's
    opening '{' up to its first 'case'/'default' label (at the switch's
    own brace depth - a label inside a nested block/switch doesn't
    count). A tag placed in that zone documents code that runs before
    any branch is selected - almost certainly meant for the case it
    actually precedes, and should be moved inside that case's block
    instead (check [11])."""
    masked = mask_comments_and_strings(text)
    n = len(masked)
    zones = []
    for m in re.finditer(r'\bswitch\s*\(', masked):
        depth = 1
        j = m.end()
        while j < n and depth > 0:
            if masked[j] == '(':
                depth += 1
            elif masked[j] == ')':
                depth -= 1
            j += 1
        k = j
        while k < n and masked[k] in ' \t\r\n':
            k += 1
        if k >= n or masked[k] != '{':
            continue  # single-statement switch body (no braces) - skip
        body_open = k
        body_end = find_matching_brace(text, body_open)
        depth = 0
        first_label = None
        p = body_open + 1
        while p < body_end:
            c = masked[p]
            if c == '{':
                depth += 1
            elif c == '}':
                depth -= 1
            elif depth == 0 and (
                    (masked.startswith('case', p)
                     and not _is_ident_char_at(masked, p - 1)
                     and not _is_ident_char_at(masked, p + 4))
                    or (masked.startswith('default', p)
                        and not _is_ident_char_at(masked, p - 1)
                        and not _is_ident_char_at(masked, p + 7))):
                first_label = p
                break
            p += 1
        if first_label is not None and first_label > body_open + 1:
            zones.append((body_open + 1, first_label))
    return zones


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


def scan_source_tags(files, tag_max_len=DEFAULT_TAG_MAX_LEN):
    """Returns (tags, malformed, too_long, misplaced):
      tags      dict {(module, num): [ {file, line, text}, ... ] }
      malformed list of {file, module, num, line, snippet}
      too_long  list of {file, module, num, line, content, length} -
                the "[MODULE]N. text" content exceeds tag_max_len chars
      misplaced list of {file, module, num, line, content, reason} -
                the tag does not sit strictly inside a function body
                (reason='outside_function'), or sits inside a switch's
                body before its first case/default label
                (reason='switch_pre_case') - check [11]
    """
    tags = defaultdict(list)
    malformed = []
    too_long = []
    misplaced = []
    for path in files:
        text = read_file(path)
        body_ranges = find_function_body_ranges(text)
        switch_zones = find_switch_pre_case_zones(text)
        for m in TAG_RE.finditer(text):
            module = m.group('module')
            num = int(m.group('num'))
            key = (module, num)
            norm_text = normalize(m.group('text'))
            line = line_of(text, m.start())
            tags[key].append({
                'file': path,
                'line': line,
                'text': norm_text,
            })
            content = '[%s]%d. %s' % (module, num, norm_text)
            if len(content) > tag_max_len:
                too_long.append({
                    'file': path, 'module': module, 'num': num,
                    'line': line, 'content': content,
                    'length': len(content),
                })
            if not pos_in_any_range(m.start(), body_ranges):
                misplaced.append({
                    'file': path, 'module': module, 'num': num,
                    'line': line, 'content': content,
                    'reason': 'outside_function',
                })
            elif pos_in_any_range(m.start(), switch_zones):
                misplaced.append({
                    'file': path, 'module': module, 'num': num,
                    'line': line, 'content': content,
                    'reason': 'switch_pre_case',
                })
        for bad in find_malformed_tags(text):
            bad['file'] = path
            malformed.append(bad)
    return tags, malformed, too_long, misplaced


# ---------------------------------------------------------------------------
# Source-side scan: function-level docstring / LOG_TRACE hygiene
# (CLAUDE.md R24 follow-up: "toutes les fonctions static ont bien un
# docstring ?" / "toutes les fonctions ont bien un LOG_TRACE ou une
# justification pour ne pas en avoir ?")
# ---------------------------------------------------------------------------

# The explicit justification convention already used in trace.c, e.g.:
#   // No LOG_TRACE - as we are currently writing a log message
NO_LOG_TRACE_RE = re.compile(r'//\s*No\s+LOG_TRACE\b', re.IGNORECASE)


class SourceFunction:
    def __init__(self, name, path, line, is_static):
        self.name = name
        self.path = path
        self.line = line
        self.is_static = is_static
        self.has_doc = False
        self.has_log_trace = False
        self.has_justification = False

        # -- docstring field consistency (CLAUDE.md 4.6 / R24.2, check [9]) --
        self.doc_raw = None            # concatenated raw preceding comment(s)
        self.sig_params = []           # parameter names from the real signature
        self.return_type = ''          # normalized return type (e.g. "st_error_t")
        self.is_void = False
        self.doc_name_ok = False       # "name() - description" first line found
        self.doc_params_missing = []   # sig params not documented
        self.doc_params_extra = []     # documented params not in the signature
        self.doc_returns = None        # raw "Returns:" text, or None if absent
        self.doc_returns_mismatch = False
        self.doc_calledby = None       # list of names, or None if section absent
        self.doc_calledby_stale = []   # listed but never found calling
        self.doc_calledby_missing = [] # real callers not listed
        self.actual_callers = set()    # names of functions that really call it
        self.self_recursive = False


def gather_preceding_comments(text, comments, sig_start):
    """comments: list of (start, end) tuples for every /* ... */ block in
    `text`. Returns the concatenated raw text (in source order) of every
    comment block that immediately precedes sig_start - chained backwards
    through as many comments as are separated only by whitespace (this is
    what lets a docstring split across a main comment + a "-- [MOD]N. --"
    tag comment + a trailing "This helper is currently called by:"
    comment, as seen in dir.c, still be read as one unit). Returns None
    if no comment immediately precedes sig_start."""
    candidates = [c for c in comments if c[1] <= sig_start]
    if not candidates:
        return None
    candidates.sort(key=lambda c: c[1])
    chain = []
    cursor = sig_start
    for start, end in reversed(candidates):
        if end > cursor:
            continue
        if text[end:cursor].strip() != '':
            break
        chain.append((start, end))
        cursor = start
    if not chain:
        return None
    chain.reverse()
    return ''.join(text[s:e] for s, e in chain)


def split_top_level(s, sep=','):
    """Split s on `sep`, ignoring occurrences nested inside ()/[] (needed
    for parameter lists containing function-pointer or array-of-array
    declarators, e.g. "char (*aaszPaths)[ST_MAX_PATH]")."""
    parts = []
    depth = 0
    cur = []
    for ch in s:
        if ch in '([':
            depth += 1
        elif ch in ')]':
            depth -= 1
        if ch == sep and depth == 0:
            parts.append(''.join(cur))
            cur = []
        else:
            cur.append(ch)
    parts.append(''.join(cur))
    return parts


def extract_param_name(chunk):
    """Best-effort extraction of the declared identifier out of a single
    parameter declaration chunk (e.g. "const char *szPath" -> "szPath",
    "char (*aaszPaths)[ST_MAX_PATH]" -> "aaszPaths"). Returns None for an
    empty chunk or a lone "void"."""
    chunk = chunk.strip()
    if chunk in ('', 'void'):
        return None
    m = re.search(r'\(\s*\*+\s*([A-Za-z_]\w*)\s*\)', chunk)
    if m:
        return m.group(1)
    chunk = re.sub(r'\[[^\]]*\]\s*$', '', chunk).strip()
    m = re.search(r'([A-Za-z_]\w*)\s*$', chunk)
    return m.group(1) if m else None


def parse_param_names(params_str):
    params_str = params_str.strip()
    if params_str == '' or params_str == 'void':
        return []
    names = []
    for chunk in split_top_level(params_str, ','):
        name = extract_param_name(chunk)
        if name:
            names.append(name)
    return names


def extract_signature_info(sig_text, name):
    """Returns (params_str, return_prefix) for a function named `name`
    whose full declarator text (return type + qualifiers + name +
    parameter list) is sig_text. Returns (None, None) if `name(` can't
    be located or its parameter list is unbalanced."""
    m = re.search(r'\b' + re.escape(name) + r'\s*\(', sig_text)
    if not m:
        return None, None
    paren_start = m.end() - 1
    depth = 0
    end = None
    for i in range(paren_start, len(sig_text)):
        c = sig_text[i]
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                end = i
                break
    if end is None:
        return None, None
    return sig_text[paren_start + 1:end], sig_text[:m.start()]


def normalize_return_type(prefix):
    p = re.sub(r'\bstatic\b', '', prefix)
    return re.sub(r'\s+', ' ', p).strip()


def parse_doc_sections(doc_raw, name):
    """Parses a (possibly multi-comment-block) docstring into its
    conventional fields (CLAUDE.md 4.6 / R24.2):
      name_ok   : True if some line reads "name() - description" (searched
                  across every gathered comment block, not just the very
                  first line - gather_preceding_comments() may have chained
                  in a leading "/* ==== Section banner ==== */" comment
                  ahead of the real docstring, which must not be mistaken
                  for a missing description line)
      params    : dict {param_name: raw bullet line} found under
                  "Parameters:"
      returns   : raw text collected under "Returns:", or None if that
                  section is absent
      calledby  : list of names (or the literal 'itself') found under
                  "This helper is currently called by:", or None if
                  that section is absent entirely
    """
    lines = []
    for chunk in re.findall(r'/\*(.*?)\*/', doc_raw, re.DOTALL):
        for raw_line in chunk.split('\n'):
            s = raw_line.strip()
            if s.startswith('*'):
                s = s[1:].strip()
            lines.append(s)
        lines.append('')

    name_re = re.compile(r'^' + re.escape(name) + r'\s*\(\)\s*-')
    name_ok = any(name_re.match(s) for s in lines if s)

    def find_section(pred):
        for idx, s in enumerate(lines):
            if pred(s.strip().lower()):
                return idx
        return None

    params = {}
    idx_params = find_section(lambda s: s == 'parameters:')
    if idx_params is not None:
        for s in lines[idx_params + 1:]:
            ls = s.strip()
            if ls == '':
                continue
            low = ls.lower()
            if low == 'returns:' or low.startswith(
                    'this helper is currently called by'):
                break
            m = re.match(r'^([A-Za-z_][A-Za-z0-9_]*)\s*\[', ls)
            if m:
                params[m.group(1)] = ls
            elif re.match(r'^[A-Za-z_ ]+:$', ls):
                break

    returns_text = None
    idx_returns = find_section(lambda s: s == 'returns:')
    if idx_returns is not None:
        collected = []
        for s in lines[idx_returns + 1:]:
            ls = s.strip()
            low = ls.lower()
            if low.startswith('this helper is currently called by'):
                break
            if re.match(r'^[A-Za-z_ ]+:$', ls):
                break
            collected.append(ls)
        returns_text = ' '.join(c for c in collected if c).strip()

    calledby = None
    idx_cb = find_section(
        lambda s: s.startswith('this helper is currently called by'))
    if idx_cb is not None:
        calledby = []
        for s in lines[idx_cb + 1:]:
            ls = s.strip()
            if ls == '':
                continue
            m = re.match(r'^-+\s*([A-Za-z_][A-Za-z0-9_]*)\s*\(\)', ls)
            if m:
                calledby.append(m.group(1))
            elif re.match(r'^-+\s*itself\b', ls, re.IGNORECASE):
                calledby.append('itself')

    return {
        'name_ok': name_ok,
        'params': params,
        'returns': returns_text,
        'calledby': calledby,
    }


def scan_source_functions(files):
    """Returns a list of SourceFunction, one per top-level function
    definition found in `files` (via find_function_defs - the same
    brace/masking scanner used for use_case_*.c). For each function:
      - is_static         : 'static' appears in its signature text
      - has_doc           : a /* ... */ comment immediately precedes it
                             (module whitespace only in between)
      - has_log_trace     : its body contains a literal 'LOG_TRACE(' call
      - has_justification : its body contains a '// No LOG_TRACE - ...'
                             comment (see trace.c for the convention)
    Additionally populates the check-[9] docstring-field-consistency
    attributes documented on SourceFunction (doc_name_ok,
    doc_params_missing/extra, doc_returns/_mismatch, doc_calledby and
    its stale/missing diffs against actual_callers).
    """
    functions = []
    for path in files:
        text = read_file(path)
        comments = [(m.start(), m.end())
                    for m in re.finditer(r'/\*.*?\*/', text, re.DOTALL)]
        comment_by_end = {end: (start, end) for start, end in comments}
        masked = mask_comments_and_strings(text)
        fdefs = find_function_defs(text)

        file_functions = []
        bodies = []  # aligned with file_functions: masked body text
        for fdef in fdefs:
            sig_text = text[fdef['sig_start']:fdef['body_open']]
            sf = SourceFunction(fdef['name'], path,
                                 line_of(text, fdef['sig_start']),
                                 bool(re.search(r'\bstatic\b', sig_text)))
            sf.has_doc = fdef['sig_start'] in comment_by_end

            body_end = find_matching_brace(text, fdef['body_open'])
            body_text = text[fdef['body_open']:body_end]
            sf.has_log_trace = 'LOG_TRACE(' in body_text
            sf.has_justification = bool(NO_LOG_TRACE_RE.search(body_text))
            bodies.append(masked[fdef['body_open']:body_end])

            params_str, return_prefix = extract_signature_info(
                sig_text, fdef['name'])
            sf.sig_params = parse_param_names(params_str or '')
            sf.return_type = normalize_return_type(return_prefix or '')
            sf.is_void = (sf.return_type == 'void')

            doc_raw = gather_preceding_comments(text, comments,
                                                 fdef['sig_start'])
            sf.doc_raw = doc_raw
            if doc_raw:
                sec = parse_doc_sections(doc_raw, sf.name)
                sf.doc_name_ok = sec['name_ok']

                doc_param_names = set(sec['params'].keys())
                sig_param_set = set(sf.sig_params)
                sf.doc_params_missing = [p for p in sf.sig_params
                                          if p not in doc_param_names]
                sf.doc_params_extra = sorted(doc_param_names - sig_param_set)

                sf.doc_returns = sec['returns']
                if sf.doc_returns:
                    rt_lower = sf.doc_returns.lower().strip()
                    if sf.is_void:
                        sf.doc_returns_mismatch = (
                            'void' not in rt_lower and 'none' not in rt_lower)
                    else:
                        sf.doc_returns_mismatch = rt_lower in (
                            'void - none', 'void', 'none', '-')

                sf.doc_calledby = sec['calledby']

            file_functions.append(sf)

        for idx, sf in enumerate(file_functions):
            call_re = re.compile(
                r'(?<![A-Za-z0-9_])' + re.escape(sf.name) + r'\s*\(')
            for j, other in enumerate(file_functions):
                if not call_re.search(bodies[j]):
                    continue
                if j == idx:
                    sf.self_recursive = True
                else:
                    sf.actual_callers.add(other.name)

            if sf.doc_calledby is not None:
                named = {n for n in sf.doc_calledby if n.lower() != 'itself'}
                sf.doc_calledby_stale = sorted(named - sf.actual_callers)
                sf.doc_calledby_missing = sorted(sf.actual_callers - named)

        functions.extend(file_functions)
    return functions


# ---------------------------------------------------------------------------
# use_case_*.c side scan
# ---------------------------------------------------------------------------

def gather_usecase_files(uc_dir):
    return sorted(glob.glob(os.path.join(uc_dir, 'use_case_*.c')))


def find_rationale_tags(path, text):
    """Scan every /* ... */ comment block in a use_case_*.c file whose
    text contains the R24.2 "Untestable tag rationale" header for bare
    "[MODULE]N" references (RATIONALE_TAG_REF_RE) - tags documented as
    analyzed-but-unreachable (Category 3) rather than exercised by any
    test. Returns dict {(module, num): {'file', 'line'}} (first
    occurrence wins if a tag is somehow listed twice)."""
    result = {}
    for cm in re.finditer(r'/\*.*?\*/', text, re.DOTALL):
        block = cm.group(0)
        if not RATIONALE_HEADER_RE.search(block):
            continue
        for rm in RATIONALE_TAG_REF_RE.finditer(block):
            key = (rm.group('module'), int(rm.group('num')))
            if key not in result:
                result[key] = {
                    'file': path,
                    'line': line_of(text, cm.start() + rm.start()),
                }
    return result


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


def check10_tag_length(too_long, out, tag_max_len):
    out.append('')
    out.append('=' * 78)
    out.append('[10] Tag length (<= %d chars between the "--" markers)' %
                tag_max_len)
    out.append('=' * 78)
    if not too_long:
        out.append('')
        out.append('  None found.')
        return []
    out.append('')
    out.append('  A tag is a pointer, not a summary - no need to repeat the')
    out.append('  function name (the tag\'s location already pins it down),')
    out.append('  just enough text to recognize the code at a glance.')
    for v in too_long:
        out.append('')
        out.append('  ERROR   [%s]%-3d %s:%-4d  %d chars (max %d)' %
                    (v['module'], v['num'], rel(v['file']), v['line'],
                     v['length'], tag_max_len))
        out.append('          "%s"' % v['content'])
    return too_long


_PLACEMENT_HINTS = {
    'outside_function': 'not strictly inside a function body (header/'
                         'docstring comment chain, or free-floating'
                         ' between two functions)',
    'switch_pre_case': 'inside a switch body but before its first'
                        ' case/default label - move it inside the'
                        ' case: block it documents',
}


def check11_tag_placement(misplaced, out):
    out.append('')
    out.append('=' * 78)
    out.append('[11] Tag placement (must be strictly inside a function body,'
                ' and inside a case: when the enclosing block is a switch)')
    out.append('=' * 78)
    if not misplaced:
        out.append('')
        out.append('  None found.')
        return []
    out.append('')
    out.append('  A tag marks a chunk of code, not a whole function or a')
    out.append('  whole switch - move it inside the { } body (or inside')
    out.append('  the specific case: it documents), immediately above the')
    out.append('  code it marks.')
    for v in misplaced:
        out.append('')
        out.append('  ERROR   [%s]%-3d %s:%-4d  %s' %
                    (v['module'], v['num'], rel(v['file']), v['line'],
                     _PLACEMENT_HINTS.get(v['reason'], '')))
        out.append('          "%s"' % v['content'])
    return misplaced


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


def check2_source_coverage(source_tags, uc_tag_index, rationale_tags,
                            out, verbose):
    out.append('')
    out.append('=' * 78)
    out.append('[2] Source tags not yet referenced in any use_case_*.c'
                ' (informational, R24 is applied progressively)')
    out.append('=' * 78)
    out.append('')
    out.append('  Tags already documented as unreachable (RATIONALE) are')
    out.append('  counted below but not listed - see check [6] for the')
    out.append('  tested/rationale/missing split, or -v to list them here.')
    missing = []
    rationale = []
    present = 0
    for key in sorted(source_tags, key=lambda k: (k[0], k[1])):
        occs = source_tags[key]
        loc = occs[0]
        if key in uc_tag_index:
            present += 1
            if verbose:
                out.append('  OK        [%s]%-3d  %s:%d' %
                            (key[0], key[1], rel(loc['file']), loc['line']))
        elif key in rationale_tags:
            rationale.append((key, loc))
        else:
            missing.append((key, loc))

    if verbose:
        for key, loc in rationale:
            rloc = rationale_tags[key]
            out.append('  RATIONALE [%s]%-3d  %s:%-4d  "%s"  (documented %s:%d)' %
                        (key[0], key[1], rel(loc['file']), loc['line'],
                         loc['text'], rel(rloc['file']), rloc['line']))

    for key, loc in missing:
        out.append('  MISSING   [%s]%-3d  %s:%-4d  "%s"' %
                    (key[0], key[1], rel(loc['file']), loc['line'],
                     loc['text']))

    total = len(source_tags)
    pct = (100.0 * present / total) if total else 0.0
    out.append('')
    out.append('  Total: %d/%d source tags referenced in use_case_*.c'
                ' (%.0f%%), %d documented as rationale, %d truly missing' %
                (present, total, pct, len(rationale), len(missing)))
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


def check7_static_docstrings(source_functions, out, verbose):
    out.append('')
    out.append('=' * 78)
    out.append('[7] static functions without a preceding docstring comment'
                ' (informational)')
    out.append('=' * 78)
    missing = [sf for sf in source_functions if sf.is_static and not sf.has_doc]
    n_static = sum(1 for sf in source_functions if sf.is_static)
    if not missing:
        out.append('')
        out.append('  None found (%d static function(s) checked).' % n_static)
        return missing
    for sf in sorted(missing, key=lambda s: (s.path, s.line)):
        out.append('  MISSING docstring   %s:%-4d  static %s()' %
                    (rel(sf.path), sf.line, sf.name))
    out.append('')
    out.append('  %d/%d static function(s) missing a docstring' %
                (len(missing), n_static))
    return missing


def check8_log_trace_hygiene(source_functions, out, verbose):
    out.append('')
    out.append('=' * 78)
    out.append('[8] functions with neither a LOG_TRACE call nor a'
                ' "// No LOG_TRACE - <reason>" justification (informational)')
    out.append('=' * 78)
    out.append('')
    out.append('  Convention (see src/trace.c): a function with no LOG_TRACE')
    out.append('  call must explain why with a comment of the exact form')
    out.append('  "// No LOG_TRACE - <reason>" (R22: primitives called in a')
    out.append('  tight loop - paint/mutex/query - are the usual reason).')
    missing = [sf for sf in source_functions
               if not sf.has_log_trace and not sf.has_justification]
    if not missing:
        out.append('')
        out.append('  None found (%d function(s) checked).' %
                    len(source_functions))
        return missing
    for sf in sorted(missing, key=lambda s: (s.path, s.line)):
        kind = 'static' if sf.is_static else 'public'
        out.append('  MISSING LOG_TRACE   %s:%-4d  %s %s()' %
                    (rel(sf.path), sf.line, kind, sf.name))
    out.append('')
    out.append('  %d/%d function(s) missing LOG_TRACE or its justification' %
                (len(missing), len(source_functions)))
    return missing


def check9_docstring_fields(source_functions, out, verbose):
    out.append('')
    out.append('=' * 78)
    out.append('[9] static function docstring field consistency: name /'
                ' Parameters / Returns / "called by" (informational)')
    out.append('=' * 78)
    out.append('')
    out.append('  Convention (CLAUDE.md 4.6 / R24.2): a static function\'s')
    out.append('  docstring starts with "name() - description", documents')
    out.append('  every signature parameter under "Parameters:", documents')
    out.append('  "Returns:", and (R24.2) names its real callers under')
    out.append('  "This helper is currently called by:". Only functions')
    out.append('  already found to have a docstring by check [7] are')
    out.append('  examined here.')

    statics = [sf for sf in source_functions if sf.is_static and sf.doc_raw]
    n_clean = 0
    for sf in sorted(statics, key=lambda s: (s.path, s.line)):
        problems = []

        if not sf.doc_name_ok:
            problems.append(
                'first line does not read "%s() - <description>"' % sf.name)

        if sf.doc_params_missing:
            problems.append(
                'Parameters: missing %s' % ', '.join(sf.doc_params_missing))
        if sf.doc_params_extra:
            problems.append(
                'Parameters: documents unknown/stale param(s) %s' %
                ', '.join(sf.doc_params_extra))

        if sf.doc_returns is None:
            problems.append('Returns: section absent')
        elif sf.doc_returns_mismatch:
            problems.append(
                'Returns: text looks inconsistent with the actual return'
                ' type (%s)' % ('void' if sf.is_void else sf.return_type))

        if sf.doc_calledby is None:
            n_callers = len(sf.actual_callers) + (1 if sf.self_recursive
                                                    else 0)
            if n_callers:
                problems.append(
                    '"This helper is currently called by:" section absent'
                    ' although %d call site(s) found' % n_callers)
        else:
            if sf.doc_calledby_stale:
                problems.append(
                    '"called by" lists %s - not found calling this'
                    ' function' % ', '.join(sf.doc_calledby_stale))
            if sf.doc_calledby_missing:
                problems.append(
                    '"called by" is missing real caller(s) %s' %
                    ', '.join(sf.doc_calledby_missing))

        if problems:
            out.append('')
            out.append('  %s:%d  static %s()' %
                        (rel(sf.path), sf.line, sf.name))
            for p in problems:
                out.append('      WARN    %s' % p)
        else:
            n_clean += 1
            if verbose:
                out.append('  OK      %s:%-4d  %s()' %
                            (rel(sf.path), sf.line, sf.name))

    out.append('')
    out.append('  %d/%d documented static function(s) fully consistent' %
                (n_clean, len(statics)))
    return statics


def check6_coverage_report(source_tags, uc_tag_index, rationale_tags,
                            functions_by_file, uc_files, strategy_files, out):
    out.append('')
    out.append('=' * 78)
    out.append('[6] Coverage report')
    out.append('=' * 78)

    by_module_total = defaultdict(int)
    by_module_tested = defaultdict(int)
    by_module_rationale = defaultdict(int)
    for key in source_tags:
        by_module_total[key[0]] += 1
        if key in uc_tag_index:
            by_module_tested[key[0]] += 1
        elif key in rationale_tags:
            by_module_rationale[key[0]] += 1

    out.append('')
    out.append('  %-14s %9s %8s %10s %9s %8s' %
                ('MODULE', 'SRC TAGS', 'TESTED', 'RATIONALE', 'MISSING',
                 'COV %'))
    for module in sorted(by_module_total):
        total = by_module_total[module]
        tested = by_module_tested[module]
        rationale = by_module_rationale[module]
        missing = total - tested - rationale
        pct = 100.0 * (tested + rationale) / total if total else 0.0
        out.append('  %-14s %9d %8d %10d %9d %7.0f%%' %
                    (module, total, tested, rationale, missing, pct))
    out.append('')
    out.append('  COV % = (TESTED + RATIONALE) / SRC TAGS - expected to')
    out.append('  reach 100% once every tag is either exercised by a test')
    out.append('  or documented as unreachable (CLAUDE.md R24.2); MISSING')
    out.append('  is the true, un-analyzed gap.')

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
    out.append('  NOTE: per CLAUDE.md R24/R25, only use_case_00.c,'
                ' use_case_01.c, use_case_02.c, use_case_03.c and')
    out.append('        use_case_04.c currently implement the full tag'
                ' traceability strategy (Code Coverage header + inline')
    out.append('        tag + INTENT linking).')
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
    parser.add_argument('--tag-max-len', type=int,
                         default=DEFAULT_TAG_MAX_LEN,
                         help='Max length (chars) of a tag\'s'
                              ' "[MODULE]N. <text>" content (default: %d)'
                              % DEFAULT_TAG_MAX_LEN)
    parser.add_argument('--strict', action='store_true',
                         help='Exit with status 1 if any ERROR was found')
    parser.add_argument('-v', '--verbose', action='store_true',
                         help='Also print OK entries, not just issues')
    parser.add_argument('--strategy-files', nargs='+',
                         default=['use_case_00.c', 'use_case_01.c',
                                  'use_case_02.c', 'use_case_03.c',
                                  'use_case_04.c'],
                         help='use_case_*.c basenames considered to fully'
                              ' apply the R24 strategy (default:'
                              ' use_case_00.c use_case_01.c use_case_02.c'
                              ' use_case_03.c use_case_04.c)')
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

    source_tags, malformed_src, too_long, misplaced = scan_source_tags(
        src_files, args.tag_max_len)

    functions_by_file = {}
    malformed_uc = []
    rationale_tags = {}
    for path in uc_files:
        functions, bad = parse_usecase_file(path)
        functions_by_file[path] = functions
        malformed_uc.extend(bad)
        rationale_tags.update(find_rationale_tags(path, read_file(path)))

    if args.module:
        source_tags = {k: v for k, v in source_tags.items()
                        if k[0] == args.module}
        malformed_src = [b for b in malformed_src
                          if b['module'] == args.module]
        malformed_uc = [b for b in malformed_uc
                         if b['module'] == args.module]
        too_long = [v for v in too_long if v['module'] == args.module]
        misplaced = [v for v in misplaced if v['module'] == args.module]
        rationale_tags = {k: v for k, v in rationale_tags.items()
                           if k[0] == args.module}
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
    errors10 = check10_tag_length(too_long, out, args.tag_max_len)
    errors11 = check11_tag_placement(misplaced, out)
    errors1 = check1_id_uniqueness(functions_by_file, out, args.verbose)
    missing_tags = check2_source_coverage(source_tags, uc_tag_index,
                                           rationale_tags, out, args.verbose)
    errors2 = check3_function_consistency(functions_by_file, out,
                                           args.verbose)
    errors3 = check4_usecase_to_source(source_tags, uc_tag_occurrences, out,
                                        args.verbose)
    alerts4 = check5_heuristic_alerts(functions_by_file, source_tags, out,
                                       args.alert_threshold)
    check6_coverage_report(source_tags, uc_tag_index, rationale_tags,
                            functions_by_file, uc_files,
                            set(args.strategy_files), out)

    source_functions = scan_source_functions(src_files)
    if args.module:
        source_functions = [
            sf for sf in source_functions
            if os.path.splitext(os.path.basename(sf.path))[0].upper()
            == args.module.upper()]
    check7_static_docstrings(source_functions, out, args.verbose)
    missing_trace = check8_log_trace_hygiene(source_functions, out,
                                              args.verbose)
    check9_docstring_fields(source_functions, out, args.verbose)

    n_errors = (len(errors0) + len(errors1) + len(errors2) + len(errors3)
                + len(errors10) + len(errors11))
    out.append('')
    out.append('=' * 78)
    out.append('SUMMARY: %d error(s), %d alert(s) for manual review,'
                ' %d missing tag(s) [2], %d missing LOG_TRACE [8]' %
                (n_errors, len(alerts4), len(missing_tags),
                 len(missing_trace)))
    out.append('=' * 78)

    print('\n'.join(out))

    if args.strict and n_errors > 0:
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
