#!/usr/bin/env python3
"""
annotate_68k.py - Annotate patterns in 68000 disassembly file

Detection patterns follows the roadmap of GEN_DISASM.md, section §5

Detects:
  - PHASE 0 (Priority 0) : Make use of existing binary
      - Identify fixup addresses : they indicate absolute addresses that need
                                   relocation in memory - could be a function, 
                                   a global variable, a table, etc ...
      - Label fixup targets      : Identify the targets and label those pointed 
                                   by the longword with fix_<address>
      
  - PHASE A (Priority 1) :
      - Entry points (JSR/BSR) : identify JSR/BSR instructions and label the 
                                 pointed address with sub_<address> - overwrite
                                 fix_<address> in the JSR/BSR instruction, add a 
                                 second label in front of the pointed address to
                                 keep trace of the different steps of analysis
                                 
      - Branches (Bcc)         : Identify the conditional branches and label the
                                 pointed address with loc_<address> and the address
                                 in the Bxx instruction. Comment on status code
                                 when branching
                                 
      - Gotos (BRA)            : Same as branches above, but distinguishing them
                                 as pure unconditional 'goto' with label bra_<address>
                                 This may help spotting an area of data jumped by a 
                                 BRA instruction
                                 
      - Loops (DBRA/DBcc)      : TODO - Wait for implementation of DBxx in disassembler
"""

import re
import sys
import struct
from dataclasses import dataclass, field
from typing import Optional

TOS_MAGIC    = 0x601A
HEADER_SIZE  = 0x1C     # 28 bytes
ST4EVER_LINE = 70       # Estimated max line size : 70 chars - used in reconstruction
ST4EVER_INST = 34       # start of instruction inside a ST4Ever disassembly line

SUB_  = 'sub_'
FIX_  = 'fix_'
DAT_  = 'dat_'
BSS_  = 'bss_'
EXT_  = 'ext_'
LOC_  = 'loc_'
BRA_  = 'bra_'

g_report_fixups    = 0
g_report_calls     = 0
g_report_branches  = 0
g_report_bra       = 0

# =============================================================================
# 1. PHASE A : REGEXES CONTROL FLOW
# =============================================================================

# Entry points (JSR/BSR) : Capture the $(address) in group(1)
JSR_BSR_RE = re.compile(
    r'\b(?:JSR|BSR(?:\.S|\.W|\.L)?)\s+\$([0-9A-Fa-f]+|[A-Za-z_][A-Za-z0-9_.]*)',
    re.IGNORECASE
)

# Branchs (Bcc)          : use labels loc_XXXX
BCC_RE = re.compile(
    r'\b(?:(BEQ|BNE|BMI|BPL|BGT|BLT|BGE|BLE|BHI|BLS|BCS|BCC|BVS|BVC)(?:\.S|\.W|\.L)?)\s+\$([0-9A-Fa-f]+|[A-Za-z_][A-Za-z0-9_.]*)',
    re.IGNORECASE
)

# BRA (non-conditional) : use label bra_XXXX
BRA_RE = re.compile(
    r'\b(?:BRA(?:\.S|\.W|\.L)?)\s+\$([0-9A-Fa-f]+|[A-Za-z_][A-Za-z0-9_.]*)',
    re.IGNORECASE
)

# Detect ST4Ever disassembly line ($000000: 1234 1234 1234 Instruction)
# Group(1) = $(address):
# Group(2) = (xxxx xxxx? xxxx? xxxx?)
ST4_DISASM_RE = re.compile(
                  r"""^\s*\$([0-9A-Fa-f]+):\s+                      # Group 1
                  ((?:[0-9A-Fa-f]{4}(?:\s+[0-9A-Fa-f]{4})*))\s+""", # Group 2
                  re.VERBOSE | re.MULTILINE)                         


# =============================================================================
# 2. PHASE B: PATTERN RECOGNITION (MULTILINE ASSEMBLY PATTERNS (PROLOGUE/EPILOGUE)
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

# --- normalize the target address value
def normalize_target(t: str) -> str:
    return t.upper().lstrip('$').lstrip('0') or '0'

# --- Parse .PRG/.TTP header in binary file to extract key addresses
def parse_tos_header(data: bytes) -> dict:
    
    # --- Check .PRG / .TTP header magic ---
    magic, = struct.unpack_from('>H', data, 0x00)
    if magic != TOS_MAGIC:
        raise ValueError(f"Invalid Magic in PRG/TTP: {magic:#06x} (expected {TOS_MAGIC:#06x})")

    text_size, = struct.unpack_from('>L', data, 0x02)
    data_size, = struct.unpack_from('>L', data, 0x06)
    bss_size,  = struct.unpack_from('>L', data, 0x0A)
    sym_size,  = struct.unpack_from('>L', data, 0x0E)
    
    # --- Check relocation table validity
    pos = HEADER_SIZE + text_size + data_size + sym_size
    if pos >= len(data):
        raise ValueError(f"Incorrect fixup table position ({pos} is larger than file size ({len(data)})")
    
    reloc_flag,= struct.unpack_from('>L', data, pos)
    fix_size   = len(data) - pos

    return {
        'text_size':  text_size,
        'data_size':  data_size,
        'bss_size':   bss_size,
        'sym_size':   sym_size,
        'fix_size':   fix_size,
        'has_reloc':  reloc_flag,
        'text_start': HEADER_SIZE,
        'data_start': HEADER_SIZE + text_size,
        'bss_start':  HEADER_SIZE + text_size + data_size,  
        'sym_start':  HEADER_SIZE + text_size + data_size,
        'fix_start':  HEADER_SIZE + text_size + data_size + sym_size,
    }
    
# --- Decode the fixup values = addresses that need to be relocated in memory
def decode_fixup_table(data: bytes, hdr: dict) -> list[int]:
    if not hdr['has_reloc']:
        return []

    pos = hdr['fix_start']
    if pos >= len(data):
        return []

    # First fixup - init resulting list
    first, = struct.unpack_from('>L', data, pos)
    pos += 4
    fixups = [first]
    current = first

    # next fixups are cumulated addresses from one to the next
    while pos < len(data):
        delta = data[pos]
        pos += 1

        if delta == 0:
            break           # End of Table
        if delta == 1:
            current += 254  # escape : add 254 bytes
            continue

        # Add delta to current value
        current += delta
        fixups.append(current)

    return fixups

    
# =============================================================================
# 4. PASS 0 — Use Binary fixup table to detect absolute addressing
# =============================================================================
def resolve_fixup_targets(data: bytes, hdr: dict) -> dict[int, dict]:
    text_end = hdr['text_size']
    data_end = text_end + hdr['data_size']
    bss_end  = data_end + hdr['bss_size']

    result = {}
    
    # List all addresses that need fixup from the fixup table
    # Addresses are those from the beginning of file
    fixups = decode_fixup_table(data, hdr)

    # now we need to resolve the fixup addresses relative to the target segment
    # knowing that offset where fixup applies is relative to the .TEXT segment 
    # We want to get the source address (where fixup applies) and the
    # target address (the value pointed by the absolute addres)
    # Depending on where the target is, the label will get a predefined prefix 
    count = 0
    for offset in fixups:
        file_offset = hdr['text_start'] + offset
        
        # Check if we are going beyond the size of file
        if file_offset + 4 > len(data):
            continue

        # This is the target value
        target, = struct.unpack_from('>L', data, file_offset)

        # Flag label prefix
        if target < text_end:
            prefix  = FIX_
        elif target < data_end:
            prefix  = DAT_
        elif target < bss_end:
            prefix  = BSS_
        else:
            prefix  = EXT_
        
        # Result consists in:
        # An array of {label, offset} indexed by the fixup offset (unique)
        #
        sTarget = f'{target:#06X}'
        result[offset] = {
            'label':    f"{prefix}{normalize_target(sTarget)}",
            'target':   target,
        }

    return result


# =============================================================================
# 5. PASS 1 — Control flow Targets  (JSR/BSR, Bxx, DBxx)
# =============================================================================

def collect_targets(raw_lines: list) -> tuple[dict, dict, dict, dict]:
    
    calls    = {}
    branches = {}
    bras     = {}
    
    # --- Loop on all lines of files ---
    for line in raw_lines:

        # Remove any comment first (e.g. commented instruction...)
        s = strip_comment(line)

        # Look for matching JSR/BSR instruction
        m = JSR_BSR_RE.search(s)
        if m:
            target = m.group(1)
            m = ST4_DISASM_RE.search(s)
            if m:
                offset = int(normalize_target(m.group(1)), 16)
                calls[offset] = {
                    'label':   f"{SUB_}{normalize_target(target)}",
                    'target':  target,
                }
            
        # Look for matching conditional branch instruction
        m = BCC_RE.search(s)
        if m:
            type = m.group(1)
            target = m.group(2)
            m = ST4_DISASM_RE.search(s)
            if m:
                offset = int(normalize_target(m.group(1)), 16)
                branches[offset] = {
                    'label':   f"{LOC_}{normalize_target(target)}",
                    'target':  target,
                    'type'  :  type,
                }

        # Look for matching unconditional branch instruction
        m = BRA_RE.search(s)
        if m:
            target = m.group(1)
            m = ST4_DISASM_RE.search(s)
            if m:
                offset = int(normalize_target(m.group(1)), 16)
                bras[offset] = {
                    'label':   f"{BRA_}{normalize_target(target)}",
                    'target':  target,
                }

    return calls, branches, bras

# =============================================================================
# 6. Main annotation function
# =============================================================================

def annotate_file(data: bytes, raw_lines: list, min_confidence: float = 0.0) -> list:
    
    fix_targets:    set = set()    # Used as unique key for targets
    call_targets:   set = set()    # Used as unique key for targets
    branch_targets: set = set()    # Used as unique key for targets
    bra_targets:    set = set()    # Used as unique key for targets
   
    global g_report_fixups
    global g_report_calls
    global g_report_branches
    global g_report_bra
    
    # Gather a table of all addresses where a disassembled instruction is seen
    addresses: set = set()
    for i, line in enumerate(raw_lines):
        s = line.strip()
        # Check if current line is an ST4Ever disassembled line
        m_addr = ST4_DISASM_RE.match(s)
        if m_addr:
            sAddress = m_addr.group(1)
            addresses.add(int(normalize_target(sAddress), 16))

    # --- PASS 0 : Detect fixup of absolute addresses
    # --------------------------------
    header = parse_tos_header(data)
    print_intro(header)
    fix_resolved = resolve_fixup_targets(data, header)
    for offset in fix_resolved.keys():
        fix_targets.add(fix_resolved[offset]['target'])
    
    g_report_fixups = len(fix_targets)
    # --------------------------------
    # --- End of PASS 0 => fix_targets contains the list of addresses of fixup targets
    
    # --- PASS 1 : Collect existing JSR/BSR calls, Bxx
    calls, branches, bras = collect_targets(raw_lines)
    for call in calls.keys():
        call_targets.add(calls[call]['target'].lstrip('0') or '0')
    for branch in branches.keys():
        branch_targets.add(branches[branch]['target'].lstrip('0') or '0')
    for bra in bras.keys():
        bra_targets.add(bras[bra]['target'].lstrip('0') or '0')
        
    g_report_calls    = len(call_targets)
    g_report_branches = len(branch_targets) 
    g_report_bra      = len(bra_targets) 
    
    
    # --- Reconstruction ---
    result = []
    
    for i, line in enumerate(raw_lines):
        s = line.strip()

        # Check if current address is a fixup target
        m_addr = ST4_DISASM_RE.match(s)
        if m_addr:

            # Reformat the ST4Ever disassembly line
            # Add 4 spaces in front
            s = "    " + strip_comment(s)
            
            # Get instruction only in current line
            sInstruction = s[4 + m_addr.end():].strip()
            
            # Get address in the current line
            sAddress = m_addr.group(1).lstrip('0') or '0'
            
            
            # ---- FIXUPS ----
            # Find if fixup address is on this line
            sLabel   = f'\n{FIX_}{normalize_target(sAddress)} ; [fixup]\n'
            iAddress = int(normalize_target(sAddress), 16)
            is_fixup = 0
            longword_to_fixup = m_addr.group(2).split()
            
            # Check if current address is a fixup target
            if (iAddress in fix_targets):
                s = sLabel + s
                is_fixup = 1
            
            for i in range(iAddress, (iAddress + (2 * len(longword_to_fixup)))):
                if fix_resolved.get(i): 
                    sComment = f'[fixup ${i:#06x}]'
                    sMnemonic = f'DC.L'
                    if (is_fixup):
                        # Special case where this address is the fixup address & target
                        s = sLabel
                        s += f'    ${m_addr.group(1)}:'
                        s += f'  {m_addr.group(2):<20}'
                        s += f'{sMnemonic:<9}${iAddress:08X}'
                        s += ((ST4EVER_LINE - (len(s) - len(sLabel))) * ' ') + " ; "
                        s += sComment
                    else:
                        
                        # --- Replace absolute addresses
                        sTarget = f'{fix_resolved[i]['target']:08X}'
                        s = s.replace(f'${sTarget}', f'{FIX_}{normalize_target(sTarget)}')

                        # --- Add comments
                        if (ST4EVER_LINE - len(s)) < 0:     # Case where line is bigger
                            s += " ; "
                        else:
                            s += ((ST4EVER_LINE - len(s)) * ' ') + " ; "
                        
                        # --- Check that target exists (it should...) ---
                        iTarget = int(normalize_target(sTarget), 16)
                        if iTarget not in addresses: 
                            s += sComment + '(??)'
                        else:
                            s += sComment
            # ---- FIXUPS ----
        
            # ---- JSR/BSR ----
            # Check if current address is a JSR/BSR sub target
            if (sAddress in call_targets):
                s = '\n' + f'{SUB_}{normalize_target(sAddress)} ; [call]' + \
                    ('\n' if s[0] != '\n' else '') + s
                
            
            # Check if the JSR/BSR is on this line and update the target value by
            # the sub_call
            if iAddress in calls.keys(): 
                
                # --- Replace target value by sub_label
                s = s.replace(f'${calls[iAddress]['target']}', f'{calls[iAddress]['label']}')
                
                pos = s.find('$')               
                last_fix = s.rfind(FIX_)
                if last_fix != -1 and last_fix > pos:
                    s = s[:last_fix] + SUB_ + s[last_fix + len(SUB_):]

                # --- Add comments
                # We must filter labels & existing comments, based on position of '$'
                if (s.rfind(';') < pos):
                    s += ((ST4EVER_LINE - 4 - (len(s) - pos)) * ' ') + " ;"
                
                sComment = f' [call]'
                # --- Check that target exists (it should...) ---
                iTarget = int(normalize_target(calls[iAddress]['target']), 16)
                if iTarget not in addresses: 
                    s += sComment + '(??)'
                else:
                    s += sComment
                
                
            # ---- JSR/BSR ----
        
            # ---- Conditional Bxx  ----
            # Check if current address is a branch 'loc' target
            if (sAddress in branch_targets):
                s = '\n' + f'{LOC_}{normalize_target(sAddress)} ; [branch]' + \
                    ('\n' if s[0] != '\n' else '') + s
                
            
            # Check if the Bxx is on this line and update the target value by
            # the loc_call
            if iAddress in branches.keys(): 
                
                # --- Replace target value by sub_label
                s = s.replace(f'${branches[iAddress]['target']}', \
                              f'{branches[iAddress]['label']}')
                
                # --- Add comments
                # We must filter labels & existing comments, based on position of '$'
                pos = s.find('$')               
                if (s.rfind(';') < pos):
                    s += ((ST4EVER_LINE - 4 - (len(s) - pos)) * ' ') + " ;"
                
                sComment = f' [branch]'
                # --- Check that target exists (it should...) ---
                iTarget = int(normalize_target(branches[iAddress]['target']), 16)
                if iTarget not in addresses: 
                    s += sComment + '(??)'
                else:
                    s += sComment
                
            # ---- Conditional Bxx  ----
        
            # ---- BRA  ----
            # Check if current address is a branch 'loc' target
            if (sAddress in bra_targets):
                s = '\n' + f'{BRA_}{normalize_target(sAddress)} ; [branch]' + \
                    ('\n' if s[0] != '\n' else '') + s
                
            
            # Check if the Bxx is on this line and update the target value by
            # the loc_call
            if iAddress in bras.keys(): 
                
                # --- Replace target value by sub_label
                s = s.replace(f'${bras[iAddress]['target']}', \
                              f'{bras[iAddress]['label']}')
                
                # --- Add comments
                # We must filter labels & existing comments, based on position of '$'
                pos = s.find('$')               
                if (s.rfind(';') < pos):
                    s += ((ST4EVER_LINE - 4 - (len(s) - pos)) * ' ') + " ;"
                
                sComment = f' [goto]'
                # --- Check that target exists (it should...) ---
                iTarget = int(normalize_target(bras[iAddress]['target']), 16)
                if iTarget not in addresses: 
                    s += sComment + '(??)'
                else:
                    s += sComment
                
            # ---- BRA  ----
        
            # Add a final \n
            s += "\n"
            result.append(s)
            
        else:
            result.append(line)
        
    return result

# =============================================================================
# 7. REPORT FUNCTIONS
# =============================================================================

def print_intro(header: dict) -> None:
    print(f"\n{'='*50}")
    print(f"  Binary infos")
    print(f"{'='*50}")
    print(f"  .TEXT start  : {header['text_start']:#06x}")
    print(f"  .TEXT size   : {header['text_size']:#06x}")
    print(f"  .DATA start  : {header['data_start']:#06x}")
    print(f"  .DATA size   : {header['data_size']:#06x}")
    print(f"  .BSS  start  : {header['bss_start']:#06x}")
    print(f"  .BSS  size   : {header['bss_size']:#06x}")
    print(f"  .SYM  start  : {header['sym_start']:#06x}")
    print(f"  .SYM  size   : {header['sym_size']:#06x}")
    print(f"  .FIX  start  : {header['fix_start']:#06x}")
    print(f"  .FIX  size   : {header['fix_size']:#06x}")
    print(f"  has relocation table  : " + 
            ("NO" if (header['has_reloc'] == 0) else "YES"))
    print(f"{'='*50}\n")

# ---
def print_fixup_report(resolved: dict) -> None:
    print(f"\n{'='*60}")
    print(f"  Resolved fixups : {len(resolved)}")
    print(f"{'='*60}")
    counts = {}
    for info in resolved.values():
        key = f"{info['seg_src']} → {info['seg_dst']}"
        counts[key] = counts.get(key, 0) + 1
    for k, v in sorted(counts.items()):
        print(f"  {k:<20} : {v:4d}")
    print(f"{'='*60}\n")

# ---
def print_report(raw_lines: list, annotated: list) -> None:
    print(f"\n{'='*50}")
    print(f"  Annotation Report")
    print(f"{'='*50}")
    print(f"  Fixups  (fix_ / dat_ / bss_ / ext_)  : {g_report_fixups:4d}")
    print(f"  JSR/BSR (sub_)  : {g_report_calls:4d}")
    print(f"  Bcc     (loc_)  : {g_report_branches:4d}")
    print(f"  BRA     (bra_)  : {g_report_bra:4d}")
    print(f"{'='*50}\n")

# =============================================================================
# 8. MAIN SCRIPT : READ CMD LINE, OPEN FILE, ANNOTATE, WRITE FILE, REPORT
# =============================================================================

def main():
    # Script usage
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <disasm.s> <bin.prg|ttp> [min_confidence 0.0-1.0]")
        sys.exit(1)

    # Arguments handling
    path     = sys.argv[1]
    bin      = sys.argv[2]
    min_conf = float(sys.argv[3]) if len(sys.argv) > 3 else 0.0

    # Read the complete input file line by line
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        raw_lines = f.readlines()
        
    # Read the binary file and parse the header
    with open(bin, 'rb') as f:
        data = f.read()
        
    # Annotate the lines 
    annotated = annotate_file(data, raw_lines, min_confidence=min_conf)

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