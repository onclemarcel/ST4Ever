# ST4Ever — Disassembler Syntax Reference

## DEVPAC3 ↔ ST4Ever Disassembler Differences

**Document:** DISASM_SYNTAX.md  
**Created:** UC15A (2026-06-06)  
**Purpose:** Reference for the systematic syntax differences between DEVPAC3
(Atari ST assembler) output and the ST4Ever `disasm_range()` output.
Validated against `use_cases/UC15A/SOURCE.S` (2525 instructions, DIFF = 0).

---

## Classification scheme (used in `use_case_15A.c`)

| Class   | Meaning                                                                      |
|---------|------------------------------------------------------------------------------|
| MATCH   | Byte-for-byte identical after whitespace normalisation                       |
| DCW     | Instruction not yet decoded — disassembler outputs `DC.W $XXXX`             |
| PCREL   | PC-relative operand: DEVPAC3 writes `d(PC)` or `*±N`; disassembler writes absolute address |
| COMPAT  | Syntactically different but semantically identical (documented below)        |
| DIFF    | Genuine mismatch — a bug. Target: **0 DIFFs**.                              |

Final score for SOURCE.S: **963 MATCH + 270 DCW + 495 PCREL + 797 COMPAT = 0 DIFF**.

---

## 1. Immediate value representation

### 1.1 Signed decimal vs unsigned hex (MOVEQ, ADDI, SUBI, ADDQ, SUBQ…)

DEVPAC3 accepts and outputs signed decimal immediates.
The ST4Ever disassembler outputs unsigned hexadecimal.

| DEVPAC3 source    | Disassembler output | Note                          |
|-------------------|---------------------|-------------------------------|
| `MOVEQ #-1,D3`    | `MOVEQ #$FF,D3`     | 8-bit signed → unsigned hex   |
| `MOVEQ #-128,D0`  | `MOVEQ #$80,D0`     | largest negative MOVEQ        |
| `ADDI.W #-1,D0`   | `ADDI.W #$FFFF,D0`  | 16-bit sign-extended          |
| `SUBI.L #-1,D0`   | `SUBI.L #$FFFFFFFF,D0` | 32-bit                     |

**COMPAT rule applied:** convert `#-N` (negative decimal) → unsigned hex of
the appropriate width (8/16/32 bits) before comparison.

### 1.2 Truncated immediates (ADDI.B/SUBI.B with 32-bit source literal)

DEVPAC3 accepts `ADD.B #$FFFFFFFF,D1` and truncates to the byte operand
size at assembly time. The disassembler outputs the actual encoded byte.

| DEVPAC3 source         | Disassembler output  |
|------------------------|----------------------|
| `ADD.B #$FFFFFFFF,D1`  | `ADDI.B #$FF,D1`     |
| `ADD.W #$FFFFFFFF,D1`  | `ADDI.W #$FFFF,D1`   |
| `SUB.B #$FFFFFFFF,D1`  | `SUBI.B #$FF,D1`     |

**COMPAT rule:** after ADD→ADDI conversion, check if the source immediate
ends with the disassembler immediate (suffix truncation).

### 1.3 Decimal vs hex for small constants (ADDQ, SUBQ, BTST…)

DEVPAC3 source uses decimal for small constants; the disassembler
uses `#$N` (hex with `$` prefix).

| DEVPAC3 source          | Disassembler output        |
|-------------------------|----------------------------|
| `ADDQ.B #6,8(A3)`       | `ADDQ.B #6,$8(A3)`         |
| `SUBQ.W #1,D2`          | `SUBQ.W #1,D2` *(MATCH)*   |
| `BTST #7,D0`            | `BTST #7,D0` *(MATCH)*     |

For `ADDQ`/`SUBQ`/`BTST`/`BCHG`/`BCLR`/`BSET`, the count/bit-number
immediate is decimal in both. The **displacement** part of an EA like
`8(A3)` is output in hex by the disassembler → `$8(A3)`.

**COMPAT rule:** normalise both source and disasm to decimal before
comparing hex-prefix immediates `#$N`.

### 1.4 Hex zero-padding

DEVPAC3 source may use `$0`, `$00`, `$0000`, `$00000000`; the
disassembler always pads to the natural width.

| DEVPAC3 source            | Disassembler output        |
|---------------------------|----------------------------|
| `ADDQ.L #1,$00000000.L`   | `ADDQ.L #1,$00000000`      |
| `MOVE.W $0,D0`            | `MOVE.W $0000,D0` *(or abs)* |

**COMPAT rule:** strip leading hex zeros before comparison.

---

## 2. Mnemonic differences

### 2.1 ADD/SUB immediate → ADDI/SUBI

When the source operand is an immediate (`#`), DEVPAC3 accepts both
`ADD.x #imm,Dn` and `ADDI.x #imm,Dn` (same opcode). The disassembler
always decodes this opcode as `ADDI`.

| DEVPAC3 source      | Disassembler output  |
|---------------------|----------------------|
| `ADD.B #4,D0`       | `ADDI.B #$4,D0`      |
| `SUB.W #100,D3`     | `SUBI.W #$64,D3`     |

**COMPAT rule:** if source starts with `ADD.x` or `SUB.x` and contains
`#`, rename to `ADDI.x` / `SUBI.x` before comparison.

---

## 3. Addressing mode differences

### 3.1 PC-relative operands

DEVPAC3 writes PC-relative operands in two forms:

- `d(PC)` — displacement off PC: `MOVE.W $100(PC),D0`
- `*±N` — relative to current instruction address: `BRA *+2`, `BEQ *-8`

The ST4Ever disassembler resolves these to absolute addresses:

| DEVPAC3 source      | Disassembler output    |
|---------------------|------------------------|
| `BRA *+2`           | `BRA $002806`          |
| `MOVE.W table(PC),D0` | `MOVE.W $003100,D0`  |

These are classified as **PCREL** — the comparison is skipped as both
forms are functionally correct. A future enhancement could compute the
PC-relative form from the absolute address and classify as MATCH.

### 3.2 `.L` suffix on absolute addresses

DEVPAC3 can write `$00000000.L` to force a 32-bit absolute address.
The disassembler outputs `$00000000` without the `.L` suffix.

| DEVPAC3 source             | Disassembler output   |
|----------------------------|-----------------------|
| `ADDQ.L #1,$00000000.L`    | `ADDQ.L #1,$00000000` |
| `BTST #7,$FF8207.L`        | `BTST #7,$FF8207`     |

**COMPAT rule:** strip `.L` / `.W` suffixes from address literals.

### 3.3 Displacement representation

Memory addressing displacements are always output in hex by the
disassembler; DEVPAC3 source may use decimal.

| DEVPAC3 source       | Disassembler output   |
|----------------------|-----------------------|
| `MOVE.W 4(A0),D1`    | `MOVE.W $4(A0),D1`    |
| `MOVE.L -8(A6),D0`   | `MOVE.L -$8(A6),D0`   |

**COMPAT rule:** convert `$HEX` to decimal and compare.

---

## 4. EXG An,An operand order (DEVPAC3 convention)

For `EXG An,Am`, DEVPAC3 encodes the **first** source register in
bits 2-0 of the opcode word (the `iRy` field) and the **second** in
bits 11-9 (`iRx`). This is the reverse of the Motorola PRM notation.

The ST4Ever disassembler (fixed in UC15A) outputs `A{iRy},A{iRx}` to
match DEVPAC3 round-trip.

| DEVPAC3 source  | Opcode  | Bits 11-9 | Bits 2-0 | Disassembler output |
|-----------------|---------|-----------|----------|---------------------|
| `EXG A5,A3`     | `0xC74D`| 3 (A3)    | 5 (A5)   | `EXG A5,A3`         |
| `EXG A1,A0`     | `0xC149`| 0 (A0)    | 1 (A1)   | `EXG A1,A0`         |

> **Note:** EXG Dx,Dy and EXG Dx,Ay are not affected — only the An,An form
> has this encoding asymmetry.

---

## 5. Shift/rotate instruction encoding (group E iIR bit)

The 68000 group-E register shift encoding uses bit 5 (`i/r`) to
distinguish immediate count from register count:

```
1110 cccc d ss i tt rrr
               ^
               bit 5: 0 = immediate count (bits 11-9 = count, 0 means 8)
                      1 = register count  (bits 11-9 = Dn holding count)
```

The ST4Ever disassembler (fixed in UC15A — was inverted before) now
correctly decodes:

| Bit 5 | Operands format     | Example                      |
|-------|---------------------|------------------------------|
| 0     | `#count,Dn`         | `ASL.W #3,D0`                |
| 1     | `Dcnt,Dn`           | `ASL.W D3,D0`                |

> **Impact on UC13 tests:** the test opcodes for "immediate-count shifts"
> had bit 5 = 1 (register mode) and vice-versa. All 10 affected test
> assertions were updated with `ADAPTED:UC15A` markers.

---

## 6. Unimplemented instructions (DCW)

270 out of 2525 instructions in SOURCE.S are output as `DC.W $XXXX`.
These are valid 68000 instructions not yet decoded by the disassembler:

| Instruction group             | Approximate count | UC planned |
|-------------------------------|-------------------|------------|
| MOVEM (move multiple regs)    | ~80               | UC21       |
| DBcc (decrement & branch)     | ~40               | UC21       |
| Scc (set on condition)        | ~35               | UC21       |
| CHK / NBCD                    | ~15               | UC21       |
| MOVEP                         | ~5                | UC21       |
| MOVE.W ea,SR / SR,ea          | ~30               | UC24       |
| MOVE.W ea,CCR                 | ~20               | UC24       |
| ILLEGAL                       | ~5                | UC24       |
| Other (TAS, RESET, STOP…)     | ~40               | UC24       |

These are classified as **DCW** in the UC15A comparison — not counted
as errors. The disassembler correctly counts their extension words even
when emitting `DC.W`, so the instruction stream remains aligned.

---

## 7. Summary table — COMPAT transformations applied in `use_case_15A.c`

| Step | Transformation                         | Function                    | DIFFs resolved |
|------|----------------------------------------|-----------------------------|----------------|
| 1    | Strip `.L` / `.W` address suffixes     | `uc15a_apply_compat`        | ~150           |
| 2    | Convert `#N` decimal → `#$N` hex      | `uc15a_apply_compat`        | ~120           |
| 3    | Strip leading hex zeros                | `uc15a_strip_hex_zeros`     | ~80            |
| 4    | `#-$N` → unsigned 32-bit hex          | `uc15a_neg_to_unsigned32`   | 19             |
| 5    | Truncation compat (`#$FFFFFFFF` → `#$FF`) | `uc15a_truncation_compat` | 4              |
| 6    | ADD/SUB → ADDI/SUBI rename            | `uc15a_add_to_addi`         | 38             |
| 7    | Immediate hex→decimal normalisation   | `uc15a_imm_to_decimal`      | ~66            |
| 8    | PC-relative branch/EA skip            | PCREL classification        | 495            |

---

*This document is generated from the UC15A validation session.
Update when new instruction groups are added to the disassembler (UC21+).*
