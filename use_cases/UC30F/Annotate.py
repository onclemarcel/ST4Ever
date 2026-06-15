#!/usr/bin/env python3
"""
annotate_68k.py - Annotate patterns in 68000 disassembly file

Detection patterns follows the roadmap of GEN_DISASM.md, section §5

Detects:
  - PHASE A (Priority 1) :
      - Entry points (JSR/BSR) : use label sub_XXXX
      - Branchs (Bcc)          : use labels loc_XXXX
      - Loops (DBRA/DBcc)      : use labels loop_XXXX
  
  - End of subroutines (ST_OK / ST_ERROR / return D0)
  - Prologues / Epilogues of routines
"""

import re
import sys
from dataclasses import dataclass, field
from typing import Optional

count = 0

# =============================================================================
# 1. PHASE A : REGEXES CONTROL FLOW
# =============================================================================

# Entry points (JSR/BSR) : use label sub_XXXX
JSR_BSR_RE = re.compile(
    r'\b(?:JSR|BSR(?:\.S|\.W|\.L)?)\s+(\$[0-9A-Fa-f]+|[A-Za-z_][A-Za-z0-9_.]*)',
    re.IGNORECASE
)

# Branchs (Bcc)          : use labels loc_XXXX
BCC_RE = re.compile(
    r'\b(B(?:EQ|NE|MI|PL|GT|LT|GE|LE|HI|LS|CS|CC|VS|VC)'
    r'(?:\.S|\.W|\.L)?)\s+(\$[0-9A-Fa-f]+|[A-Za-z_][A-Za-z0-9_.]*)',
    re.IGNORECASE
)

# BRA (non-conditional) : use label bra_XXXX
BRA_RE = re.compile(
    r'\bBRA(?:\.S|\.W|\.L)?\s+(\$[0-9A-Fa-f]+|[A-Za-z_][A-Za-z0-9_.]*)',
    re.IGNORECASE
)

# Loops (DBRA/DBcc)      : use labels loop_XXXX
DBCC_RE = re.compile(
    r'\b(DB(?:RA|F|T|EQ|NE|MI|PL|GT|LT|GE|LE|HI|LS|CS|CC|VS|VC)'
    r'(?:\.W)?)\s+D[0-7]\s*,\s*(\$[0-9A-Fa-f]+|[A-Za-z_][A-Za-z0-9_.]*)',
    re.IGNORECASE
)

# Detect label on beginning of line (_label_: instruction)
LABEL_RE = re.compile(r'^([A-Za-z_.][A-Za-z0-9_.]*)\s*:')

# Detect ST4Ever disassembly line ($000000: 1234 1234 1234 Instruction)
ST4_DISASM_RE = re.compile(r'^\s*\$([0-9A-Fa-f]+):\s+(?:[0-9A-Fa-f]{4}(?:\s+[0-9A-Fa-f]{4})*)\s+')

# =============================================================================
# 2. PATTERN DEFINITIONS  
# =============================================================================

@dataclass
class Pattern:
    name:        str
    lines:       list         # regexes on stripped consecutives instructions
    annotation:  str
    confidence:  float        # 0.0 – 1.0

PATTERNS = [
    # --- Return of values ---
    Pattern(
        name="return_ok_moveq",
        lines=[r"MOVEQ\s+#0\s*,\s*D0", r"RTS"],
        annotation="; return ST_OK;  [MOVEQ #0,D0]",
        confidence=0.95,
    ),
    Pattern(
        name="return_error_moveq",
        lines=[r"MOVEQ\s+#-1\s*,\s*D0", r"RTS"],
        annotation="; return ST_ERROR;  [MOVEQ #-1,D0]",
        confidence=0.95,
    ),
    Pattern(
        name="return_ok_cmp",
        lines=[r"CMP(?:\.L|\.W|\.B)?\s+D0\s*,\s*D0", r"RTS"],
        annotation="; return ST_OK;  [CMP D0,D0 → Z=1]",
        confidence=0.85,
    ),
    Pattern(
        name="return_ok_clr",
        lines=[r"CLR(?:\.L|\.W|\.B)?\s+D0", r"RTS"],
        annotation="; return ST_OK;  [CLR D0]",
        confidence=0.90,
    ),
    Pattern(
        name="return_value_tst",
        lines=[r"TST(?:\.L|\.W|\.B)?\s+D0", r"RTS"],
        annotation="; return D0;  [TST D0 → CCR mis à jour]",
        confidence=0.80,
    ),
    Pattern(
        name="return_error_subq",
        lines=[r"SUBQ(?:\.L|\.W|\.B)?\s+#1\s*,\s*D0", r"RTS"],
        annotation="; return ST_ERROR;  [SUBQ #1,D0 from 0]",
        confidence=0.60,
    ),
    # --- Prologues ---
    Pattern(
        name="prologue_movem",
        lines=[r"MOVEM\.L\s+[A-Z0-9/\-]+\s*,\s*-\(SP\)"],
        annotation="; --- prologue: save registers ---",
        confidence=0.90,
    ),
    Pattern(
        name="prologue_link",
        lines=[r"LINK\s+A[0-7]\s*,\s*#-?\d+"],
        annotation="; --- prologue: stack frame ---",
        confidence=0.95,
    ),
    # --- Epilogues ---
    Pattern(
        name="epilogue_movem_rts",
        lines=[r"MOVEM\.L\s+\(SP\)\+\s*,\s*[A-Z0-9/\-]+", r"RTS"],
        annotation="; --- epilogue: restore registers ---",
        confidence=0.92,
    ),
    Pattern(
        name="epilogue_unlk_rts",
        lines=[r"UNLK\s+A[0-7]", r"RTS"],
        annotation="; --- epilogue: destroy frame ---",
        confidence=0.95,
    ),
    Pattern(
        name="epilogue_unlk_movem_rts",
        lines=[r"UNLK\s+A[0-7]",
               r"MOVEM\.L\s+\(SP\)\+\s*,\s*[A-Z0-9/\-]+",
               r"RTS"],
        annotation="; --- epilogue: destroy frame + restore registers ---",
        confidence=0.97,
    ),
]

# =============================================================================
# 3. Helpers
# =============================================================================

# --- Strip comment from the current line
def strip_comment(line: str) -> str:
    
    # find any comment...
    idx = line.find(';')
    # ... and strip the line form it
    return line[:idx].strip() if idx != -1 else line.strip()

# --- Select the instruction only on current line
def instruction_only(line: str) -> Optional[str]:
    
    # Strip comment (; xxx) from the current line
    s = strip_comment(line)
    if not s:
        return None
    
    # --- if label is alone, return None ---
    if re.match(r'^[A-Za-z_.][A-Za-z0-9_.]*:\s*$', s):
        return None

    # --- if Label: instruction -> remove label
    m = LABEL_RE.match(s)
    if m:
        rest = s[m.end():].strip()
        if not rest:
            return None
        s = rest

    # --- ST4Ever disassembly format : keep instruction only ---
    m2 = ST4_DISASM_RE.match(s)
    if m2:
        s = s[m2.end():].strip()
    return s.upper() if s else None

# --- normalize the target address value
def normalize_target(t: str) -> str:
    return t.upper().lstrip('$').lstrip('0') or '0'

# ---
def label_for(prefix: str, target: str) -> str:
    t = target.lstrip('$')
    
    # if a name already exists, keep it
    if re.match(r'^[A-Za-z_]', target):
        return target
    return f"{prefix}{t.upper()}"

# =============================================================================
# 4. PASS 1 — Control flow Targets  (JSR/BSR, Bxx, DBxx)
# =============================================================================

def collect_targets(raw_lines: list) -> tuple[set, set, set]:
    call_targets:   set = set()
    branch_targets: set = set()
    loop_targets:   set = set()

    global count

    # --- Loop on all lines of files ---
    for line in raw_lines:

        # Remove any comment first (e.g. commented instruction...)
        s = strip_comment(line)

        # Look for matching JSR/BSR instruction
        m = JSR_BSR_RE.search(s)
        if m:
            call_targets.add(normalize_target(m.group(1)))

        # Look for matching conditional branch instruction
        m = BCC_RE.search(s)
        if m:
            branch_targets.add(normalize_target(m.group(2)))

        # Look for matching unconditional branch instruction
        m = BRA_RE.search(s)
        if m:
            branch_targets.add(normalize_target(m.group(1)))

        # Look for matching loop instruction
        m = DBCC_RE.search(s)
        if m:
            loop_targets.add(normalize_target(m.group(2)))

    return call_targets, branch_targets, loop_targets

# =============================================================================
# 5. PASS 2 — Addresses/Labels Table 
# =============================================================================

def collect_existing_labels(raw_lines: list) -> dict:
    existing = {}
    for line in raw_lines:
        s = line.strip()
        m = LABEL_RE.match(s)
        if m:
            lbl = m.group(1)
            existing[normalize_target(lbl)] = lbl
    return existing

# =============================================================================
# 6. PASS 3 — Pattern matching 
# =============================================================================

def match_patterns(instr_index: list) -> dict:
    annotations = {}
    for i in range(len(instr_index)):
        for pat in PATTERNS:
            window = instr_index[i:i + len(pat.lines)]
            if len(window) < len(pat.lines):
                continue
            if all(
                re.match(regex, instr, re.IGNORECASE)
                for (_, instr), regex in zip(window, pat.lines)
            ):
                line_idx = window[0][0]
                conf_str = f"[conf:{pat.confidence:.0%}]"
                annotations[line_idx] = f"{pat.annotation} {conf_str}"
                break
    return annotations

# =============================================================================
# 7. PASS 4 — Rebuild of annotated files
# =============================================================================

def annotate_file(raw_lines: list, min_confidence: float = 0.0) -> list:
    
    # --- Collect existing JSR/BSR calls, Bxx, DBxx ---
    call_targets, branch_targets, loop_targets = collect_targets(raw_lines)
    # --- Collect existing labels ---
    existing_labels = collect_existing_labels(raw_lines)

    # --- Prioritize loops over branch over calls in case of multiple types
    def label_type(key: str) -> Optional[tuple]:
        if key in loop_targets:
            return ('loop_', 'loop')
        if key in branch_targets:
            return ('loc_',  'branch')
        if key in call_targets:
            return ('sub_',  'call')
        return None

    # --- Extract Instructions only and line number in file ---
    instr_index = []
    for i, line in enumerate(raw_lines):
        instr = instruction_only(line)
        if instr:
            instr_index.append((i, instr))
        

    # --- Pattern annotations ---
    # pattern_annots = match_patterns(instr_index)

    # --- Reconstruction ---
    result = []
    inserted_labels = set()   # évite les doublons

    for i, line in enumerate(raw_lines):
        s = line.strip()

        # Détection de l'adresse courante si la ligne EST un label ou une instr
        # On cherche si cette ligne correspond à une cible connue
        # Cas 1 : label existant
        m_lbl = LABEL_RE.match(s)
        if m_lbl:
            lbl = m_lbl.group(1)
            key = normalize_target(lbl)
            lt = label_type(key)
            if lt and key not in inserted_labels:
                prefix, kind = lt
                # Le label existe déjà, on ajoute juste un commentaire
                result.append(f"; [{kind}] {lbl}\n")
                inserted_labels.add(key)

        # Cas 2 : adresse numérique en début de ligne (format objdump)
        else:
            m_addr = ST4_DISASM_RE.match(s)
            if m_addr:
                addr = m_addr.group(1).lstrip('0') or '0'
                key = addr.upper()
                lt = label_type(key)
                if lt and key not in existing_labels and key not in inserted_labels:
                    prefix, kind = lt
                    lbl = label_for(prefix, addr)
                    result.append(f"\n{lbl}:   ; [{kind}]\n")
                    inserted_labels.add(key)

        # Annotation de pattern (insérée avant la ligne)
        #if i in pattern_annots:
        #    indent = '    '
        #    result.append(f"{indent}{pattern_annots[i]}\n")

        result.append(line)

    return result

# =============================================================================
# 8. REPORT STATISTICS
# =============================================================================

def print_report(raw_lines: list, annotated: list) -> None:
    call_t, branch_t, loop_t = collect_targets(raw_lines)
    print(f"\n{'='*50}")
    print(f"  Annotation Report")
    print(f"{'='*50}")
    print(f"  JSR/BSR  (sub_)  : {len(call_t):4d}")
    print(f"  Bcc/BRA  (loc_)  : {len(branch_t):4d}")
    print(f"  DBcc     (loop_) : {len(loop_t):4d}")
    print(f"{'='*50}\n")

# =============================================================================
# 9. MAIN SCRIPT : READ CMD LINE, OPEN FILE, ANNOTATE, WRITE FILE, REPORT
# =============================================================================

def main():
    # Script usage
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <disasm.s> [min_confidence 0.0-1.0]")
        sys.exit(1)

    # Arguments handling
    path     = sys.argv[1]
    min_conf = float(sys.argv[2]) if len(sys.argv) > 2 else 0.0

    # Read the complete input file line by line
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        raw_lines = f.readlines()

    # Annotate the lines 
    annotated = annotate_file(raw_lines, min_confidence=min_conf)

    # Change the output file name to <input>_annotated.S
    out_path = re.sub(r'\.([Ss])$', r'_annotated.\1', path)
    if out_path == path:
        out_path = path + '_annotated.s'

    # Write lines
    with open(out_path, 'w', encoding='utf-8') as f:
        f.writelines(annotated)

    # Report statistics
    print_report(raw_lines, annotated)
    print(f"  Annotated file → {out_path}")

if __name__ == "__main__":
    main()