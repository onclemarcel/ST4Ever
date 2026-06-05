        ; SOURCE.S - 68000-only assembler/disassembler torture test
        ; Format : Devpac 3 / Atari ST
        ;
        ; Purpose:
        ; - Generate a .PRG binary for static analysis only (no execution).
        ; - Cover the full Motorola 68000 instruction set (no 68010+ opcodes).
        ; - Exercise all addressing modes allowed by the 68000 reference manual.
        ; - Validate that the disassembler handles addressing modes correctly
        ; and does not rely on per-mnemonic special casing.
        ;
        ; Assumptions:
        ; - Code is never executed: no register initialization, no valid stack,
        ; no valid memory layout.
        ; - The file must assemble cleanly (Devpac passes 1/2).
        ;
        ; Corner cases / deliberate choices:
        ; - 68010/68020+ instructions are excluded (BKPT, MOVEC, CAS, etc.).
        ; - Immediate values:
        ; * Always compatible with operand size (.b/.w/.l).
        ; * Boundary values and special patterns:
        ; #$00, #$7F, #$80, #$FF,
        ; #$0000, #$7FFF, #$8000, #$FFFF,
        ; #$00000000, #$7FFFFFFF, #$80000000, #$FFFFFFFF.
        ; * Occasional pseudo-random values.
        ; - Absolute short addresses:
        ; * 16-bit signed extension word.
        ; * Effective address = sign-extended 16-bit value.
        ; * Boundary values: $0000, $7FFF, $8000, $FFFF.
        ; - Absolute long addresses:
        ; * Full 32-bit values, same boundary/random strategy as immediates.
        ; - A7/SP:
        ; * Used only when required by the instruction or addressing mode.
        ; * Stack validity irrelevant (no execution).
        ; - PC / alignment:
        ; * Assembly-only target; runtime PC alignment not enforced.
        ; - Addressing modes:
        ; * All modes allowed by the 68000 manual are exercised at least once.
        ;
        ; Organization:
        ; - Grouped by instruction family.
        ;
        ; Note:
        ; - This file is NOT intended to run on an Atari ST.
        ; - Execution safety is intentionally ignored.

; ----------------------------------------------------------------------
; MOVE - Move data between registers and memory
; ----------------------------------------------------------------------
; Notes:
; - MOVE.<size> <ea>,<ea>
; Sizes: .b, .w, .l
; Allowed <ea>: all data-alterable modes (no An as destination)
;
; - MOVE.<size> <ea>,Dn
; Sizes: .b, .w, .l
; Allowed <ea>: all data addressing modes except An
;
; - MOVE.<size> Dn,<ea>
; Sizes: .b, .w, .l
; Allowed <ea>: all data-alterable modes (no An)
;
; - MOVEA.<size> <ea>,An
; Sizes: .w, .l
; Allowed <ea>: all address-producing modes
;
; - Absolute short is SIGNED 16-bit
; - Absolute long is full 32-bit

MOVE_SECTION:

; ======================================================================
; MOVE.b <ea>,Dn
; ======================================================================
MOVEB_ST:
        move.b d0,d1
        move.b (a0),d2
        move.b (a1)+,d3
        move.b -(a2),d4
        move.b 8(a3),d5
        move.b 16(a4,d5.w),d6
        move.b $0000.w,d7
        move.b $7FFE.w,d0
        move.b $1234.w,d3
        move.b $00000000.l,d4
        move.b $7FFFFFFE.l,d5
        move.b $80000000.l,d6
        move.b $FFFFFFFE.l,d7
        move.b $CAFEBABE.l,d0
		move.b *-0(pc),d1
MOVEB_0:move.b MOVEB_0(pc),d0
		move.b *+0(pc),d2
		move.b *+2(pc),d3
		move.b *-2(pc),d4		
		move.b *+32768(pc),d5
		move.b *-32766(pc),d6
		move.b *-0(pc,d0.l),d7
		move.b *+0(pc,a1.w),d0
        move.b *+2(pc,d2.l),d1
        move.b *-2(pc,d3.w),d2
		move.b *+128(pc,d3.l),d3
		move.b *-126(pc,a4.w),d4
		move.b MOVEB_ST(pc,a5.l),d5
        move.b MOVEB_END(pc,d6.w),d6
		move.b #0,d3
        move.b #1,d4
        move.b #$7F,d5
        move.b #$80,d6
        move.b #$FF,d7
		move.b #$FFFFFFFF,d1
MOVEB_END:


; ======================================================================
; MOVE.w <ea>,Dn
; ======================================================================
MOVEW_ST:
        move.w d0,d1
        move.w (a0),d2
        move.w (a1)+,d3
        move.w -(a2),d4
        move.w 8(a3),d5
        move.w 16(a4,d5.w),d6
        move.w $0000.w,d7
        move.w $7FFE.w,d0
        move.w $5678.w,d3
        move.w $00000000.l,d4
        move.w $7FFFFFFE.l,d5
        move.w $80000000.l,d6
        move.w $FFFFFFFE.l,d7
        move.w $DEADBEEE.l,d0
		move.w *-0(pc),d1
MOVEW_0: move.w MOVEW_0(pc),d0
		move.w *+0(pc),d2
		move.w *+2(pc),d3
		move.w *-2(pc),d4		
		move.w *+32768(pc),d5
		move.w *-32766(pc),d6
		move.w *-0(pc,d0.l),d7
		move.w *+0(pc,d1.w),d0
        move.w *+2(pc,d2.l),d1
        move.w *-2(pc,d3.w),d2
		move.w *+128(pc,d3.l),d3
		move.w *-126(pc,d4.w),d4
		move.w MOVEW_ST(pc,d5.l),d5
        move.w MOVEW_END(pc,a6.w),d6
		move.w #0,d3
        move.w #1,d4
        move.w #$FFFF,d5
        move.w #$8000,d6
        move.w #$7FFF,d7
		move.w #$FFFFFFFF,d1
MOVEW_END:


; ======================================================================
; MOVE.l <ea>,Dn
; ======================================================================
MOVEL_ST:
        move.l d0,d1
        move.l (a0),d2
        move.l (a1)+,d3
        move.l -(a2),d4
        move.l 8(a3),d5
        move.l 16(a4,a5.w),d6
        move.l $0000.w,d7
        move.l $7FFE.w,d0
        move.l $1112.w,d3
        move.l $00000000.l,d4
        move.l $7FFFFFFE.l,d5
        move.l $80000000.l,d6
        move.l $FFFFFFFE.l,d7
        move.l $2468ACE0.l,d0
		add.l *-0(pc),d1
MOVEL_0: add.l MOVEL_0(pc),d0
		add.l *+0(pc),d2
		add.l *+2(pc),d3
		add.l *-2(pc),d4		
		add.l *+32768(pc),d5
		add.l *-32766(pc),d6
		add.l *-0(pc,a0.l),d7
		add.l *+0(pc,d1.w),d0
        add.l *+2(pc,a2.w),d1
        add.l *-2(pc,d3.l),d2
		add.l *+128(pc,d3.l),d3
		add.l *-126(pc,d4.w),d4
		add.l MOVEL_ST(pc,a5.l),d5
        add.l MOVEL_END(pc,d6.w),d6
		add.l #0,d3
        add.l #1,d4
        add.l #$FFFF,d5
        add.l #$8000,d6
        add.l #$7FFF,d7
		add.l #$12345678,d0
		add.l #$FFFFFFFF,d1
MOVEL_END:


; ======================================================================
; MOVE.<size> Dn,<ea>
; ======================================================================

        move.b d0,(a0)
        move.b d1,(a1)+
        move.b d2,-(a2)
        move.b d3,8(a3)
        move.b d4,16(a4,d5.w)
        move.b d5,$0000.w
        move.b d6,$7FFE.w
        move.b d7,$1234.w
        move.b d0,$00000000.l
        move.b d1,$DEADBEEE.l

        move.w d2,(a0)
        move.w d3,(a1)+
        move.w d4,-(a2)
        move.w d5,8(a3)
        move.w d6,16(a4,d5.w)
        move.w d7,$0000.w
        move.w d0,$7FFE.w
        move.w d1,$4320.w
        move.w d2,$00000000.l
        move.w d3,$FFFFFFFE.l

        move.l d4,(a0)
        move.l d5,(a1)+
        move.l d6,-(a2)
        move.l d7,8(a3)
        move.l d0,16(a4,d5.w)
        move.l d1,$0000.w
        move.l d2,$7FFE.w
        move.l d3,$0ABC.w
        move.l d4,$00000000.l
        move.l d5,$CAFEBABE.l


; ======================================================================
; MOVE.<size> <ea1>,<ea2> (memory ↔ memory)
; ======================================================================

        move.b (a0),(a1)
        move.b (a2)+,8(a3)
        move.b -(a4),16(a5,d6.w)
        move.b $0000.w,(a6)
        move.b $1234.w,$00000000.l
		move.w $000FFFFE.l,$1234.w
MOVEBEA_0:		move.b MOVEBEA_0(pc),d4		
		move.b *-2(pc),d4		
		move.b *+32768(pc),d5
		move.b *-32766(pc),(a6)+
		move.b *+0(pc,d1.w),(a0)
        move.b *+2(pc,d2.l),32766(a1)
        move.b *+126(pc,d2.l),126(a1,a2.w)
        move.b *-2(pc,d3.w),-(a7)

        move.w (a0),(a1)
        move.w (a2)+,8(a3)
        move.w -(a4),16(a5,d6.w)
        move.w $0000.w,(a6)
        move.w $7FFE.w,$FFFFFFFE.l
		move.w $7FFFFFFE.l,$7FFE.w
MOVEWEA_0:		move.b MOVEWEA_0(pc),d4		
		move.w *-2(pc),d4		
		move.w *+32768(pc),d5
		move.w *-32766(pc),(a6)+
		move.w *+0(pc,d1.w),(a0)
        move.w *+2(pc,d2.l),32766(a1)
        move.w *+126(pc,d2.l),126(a1,a4.l)
        move.w *-2(pc,d3.w),-(a7)

        move.l (a0),(a1)
        move.l (a2)+,8(a3)
        move.l -(a4),16(a5,d6.w)
        move.l $0000.w,(a6)
        move.l $0ABC.w,$7FFFFFFE.l
		move.l $00000002.l,$0ABC.w
MOVELEA_0:		move.b MOVELEA_0(pc),d4
		move.l *-2(pc),d4		
		move.l *+32768(pc),d5
		move.l *-32766(pc),(a6)+
		move.l *+0(pc,d1.w),(a0)
        move.l *+2(pc,d2.l),32766(a1)
        move.l *+126(pc,d2.l),126(a1,d2.l)
        move.l *-2(pc,d3.w),-(a7)


; ======================================================================
; MOVEA.<w/l> <ea>,An
; ======================================================================
MOVEAW_ST:
        movea.w d0,a0
        movea.w (a0),a1
        movea.w (a1)+,a2
        movea.w -(a2),a3
        movea.w 8(a3),a4
        movea.w 16(a4,d5.w),a5
        movea.w $0000.w,a6
        movea.w $7FFE.w,a7
        movea.w $1234.w,a2
        movea.w $00000000.l,a3
        movea.w $7FFFFFFE.l,a4
        movea.w $80000000.l,a5
        movea.w $FFFFFFFE.l,a6
        movea.w $CAFEBABE.l,a7
		movea.w *-0(pc),a1
MOVEAW_0: movea.w MOVEAW_0(pc),a0
		movea.w *+0(pc),a2
		movea.w *+2(pc),a3
		movea.w *-2(pc),a4		
		movea.w *+32768(pc),a5
		movea.w *-32766(pc),a6
		movea.w *-0(pc,d0.l),a7
		movea.w *+0(pc,d1.w),a0
        movea.w *+2(pc,d2.l),a1
        movea.w *-2(pc,d3.w),a2
		movea.w *+128(pc,d3.l),a3
		movea.w *-126(pc,d4.w),a4
		movea.w MOVEAW_ST(pc,d5.l),a5
        movea.w MOVEAW_END(pc,d6.w),a6
		movea.w #0,a3
        movea.w #1,a4
        movea.w #$FFFF,a5
        movea.w #$8000,a6
        movea.w #$7FFF,a7
		movea.w #$FFFFFFFF,a1
MOVEAW_END:

MOVEAL_ST:
        movea.l d0,a0
        movea.l (a0),a1
        movea.l (a1)+,a2
        movea.l -(a2),a3
        movea.l 8(a3),a4
        movea.l 16(a4,d5.w),a5
        movea.l $0000.w,a6
        movea.l $7FFE.w,a7
        movea.l $0ABC.w,a2
        movea.l $00000000.l,a3
        movea.l $7FFFFFFE.l,a4
        movea.l $80000000.l,a5
        movea.l $FFFFFFFE.l,a6
        movea.l $DEADBEEE.l,a7
		movea.l *-0(pc),a1
MOVEAL_0: movea.l MOVEAL_0(pc),a0
		movea.l *+0(pc),a2
		movea.l *+2(pc),a3
		movea.l *-2(pc),a4		
		movea.l *+32768(pc),a5
		movea.l *-32766(pc),a6
		movea.l *-0(pc,d0.l),a7
		movea.l *+0(pc,d1.w),a0
        movea.l *+2(pc,d2.l),a1
        movea.l *-2(pc,d3.w),a2
		movea.l *+128(pc,d3.l),a3
		movea.l *-126(pc,d4.w),a4
		movea.l MOVEAL_ST(pc,d5.l),a5
        movea.l MOVEAL_END(pc,d6.w),a6
		movea.l #0,a3
        movea.l #1,a4
        movea.l #$FFFF,a5
        movea.l #$8000,a6
        movea.l #$7FFF,a7
		movea.l #$12345678,a0
		movea.l #$FFFFFFFF,a1
MOVEAL_END:

; ----------------------------------------------------------------------
; MOVE to/from SR, CCR, USP
; ----------------------------------------------------------------------
; Notes:
; - MOVE <ea>,SR : privileged, size = .w only
; Allowed <ea>: all data addressing modes except An
;
; - MOVE SR,<ea> : privileged, size = .w only
; Allowed <ea>: all data-alterable modes except An
;
; - MOVE <ea>,CCR : size = .w only
; Allowed <ea>: all data addressing modes except An
;
; - MOVE CCR,<ea> : NOT available on 68000 (68010+ only)
; Included optionally for completeness
;
; - MOVE USP,An : privileged
; - MOVE An,USP : privileged
; Only address registers allowed
;
; - Absolute short is SIGNED 16-bit
; - Absolute long is full 32-bit

MOVE_SR_CCR_USP_SECTION:

; ======================================================================
; MOVE <ea>,SR (to SR, .w only)
; ======================================================================

        move.w d0,SR
        move.w (a0),SR
        move.w (a1)+,SR
        move.w -(a2),SR
        move.w 8(a3),SR
        move.w 16(a4,d5.w),SR
        move.w $0000.w,SR
        move.w $7FFE.w,SR
        move.w $1234.w,SR
        move.w $00000000.l,SR
        move.w $7FFFFFFE.l,SR
        move.w $80000000.l,SR
        move.w $FFFFFFFE.l,SR
        move.w $CAFEBABE.l,SR


; ======================================================================
; MOVE SR,<ea> (from SR, .w only)
; ======================================================================

        move.w SR,d0
        move.w SR,(a0)
        move.w SR,(a1)+
        move.w SR,-(a2)
        move.w SR,8(a3)
        move.w SR,16(a4,d5.w)
        move.w SR,$0000.w
        move.w SR,$7FFE.w
        move.w SR,$1234.w
        move.w SR,$00000000.l
        move.w SR,$7FFFFFFE.l
        move.w SR,$80000000.l
        move.w SR,$FFFFFFFE.l
        move.w SR,$DEADBEEE.l


; ======================================================================
; MOVE <ea>,CCR (to CCR, .w only)
; ======================================================================

        move.w d0,CCR
        move.w (a0),CCR
        move.w (a1)+,CCR
        move.w -(a2),CCR
        move.w 8(a3),CCR
        move.w 16(a4,d5.w),CCR
        move.w $0000.w,CCR
        move.w $7FFE.w,CCR
        move.w $1358.w,CCR
        move.w $00000000.l,CCR
        move.w $7FFFFFFE.l,CCR
        move.w $80000000.l,CCR
        move.w $FFFFFFFE.l,CCR
        move.w $2468ACE0.l,CCR


; ======================================================================
; MOVE USP,<ea> and MOVE <ea>,USP (privileged)
; ======================================================================

        move.l USP,a0
        move.l USP,a1
        move.l USP,a2
        move.l USP,a7

        move.l a0,USP
        move.l a1,USP
        move.l a2,USP
        move.l a7,USP

; ----------------------------------------------------------------------
; LEA / PEA - Addressing mode coverage on 68000
; ----------------------------------------------------------------------
; Notes:
;   - LEA accepts ONLY address producing modes:
;       (An), (d16,An), (d8,An,Xn), absolute short, absolute long, (d16,PC), (d8,PC,Xn)
;     It does NOT accept:
;       Dn, (An)+, -(An), immediate, etc.
;
;   - PEA accepts ONLY:
;       (An), (d16,An), (d8,An,Xn), absolute short, absolute long, (d16,PC), (d8,PC,Xn)
;
;   - Absolute short is SIGNED 16 bit:
;       $7FFF = +32767
;       $8000 = -32768
;       $FFFF = -1
;
;   - Absolute long uses full 32 bit values.
;
;   - PC relative modes are included because they are legal for LEA/PEA
;     and are a common source of disassembler bugs.

LEA_SECTION:

        lea     (a0),a0
        lea     (a3),a1
        lea     (a6),a2
        lea     0(a0),a3
        lea     32767(a1),a4
        lea     -32768(a2),a5
        lea     -1(a3),a6
        lea     1234(a4),a7
        lea     8(a0,d0.w),a0
        lea     16(a1,d2.l),a1
        lea     -32(a2,d3.w),a2
        lea     64(a3,d7.l),a3
        lea     12(a4,d5.w),a4
        lea     $0000.w,a5
        lea     $7FFF.w,a6
        lea     $1358.w,a7
        lea     $00000000.l,a0
        lea     $7FFFFFFE.l,a1
        lea     $80000000.l,a2
        lea     $FFFFFFFE.l,a3
        lea     $2468ACE0.l,a4
LEA_ST:
		lea *-0(pc),a5
LEA_0:  lea LEA_0(pc),a6
		lea *+0(pc),a7
		lea *+2(pc),a0
		lea *-2(pc),a1
		lea *+32768(pc),a2
		lea *-32766(pc),a3
		lea *-0(pc,d0.l),a4
		lea *+0(pc,d1.w),a5
        lea *+2(pc,d2.l),a6
        lea *-2(pc,d3.w),a7
		lea *+128(pc,d3.l),a0
		lea *-126(pc,d4.w),a1
		lea *-126(pc,d4.w),a2
		lea LEA_ST(pc,d5.l),a3
        lea LEA_END(pc,d6.w),a4
LEA_END:

; ----------------------------------------------------------------------
; PEA - Push Effective Address
; ----------------------------------------------------------------------

PEA_SECTION:
        pea     (a0)
        pea     (a3)
        pea     (a6)
        pea     0(a0)
        pea     32767(a1)
        pea     -32768(a2)
        pea     -1(a3)
        pea     1234(a4)
        pea     8(a0,d0.w)
        pea     16(a1,d2.l)
        pea     -32(a2,d3.w)
        pea     64(a3,d7.l)
        pea     12(a4,d5.w)
        pea     $0000.w
        pea     $7FFF.w
        pea     $1358.w
        pea     $00000000.l
        pea     $7FFFFFFE.l
        pea     $80000000.l
        pea     $FFFFFFFE.l
        pea     $2468ACE0.l
PEA_ST:
		pea *-0(pc)
PEA_0:  pea PEA_0(pc)
		pea *+0(pc)
		pea *+2(pc)
		pea *-2(pc)		
		pea *+32768(pc)
		pea *-32766(pc)
		pea *-0(pc,d0.l)
		pea *+0(pc,d1.w)
        pea *+2(pc,d2.l)
        pea *-2(pc,d3.w)
		pea *+128(pc,d3.l)
		pea *-126(pc,d4.w)
		pea *-126(pc,d4.w)
		pea PEA_ST(pc,d5.l)
        pea PEA_END(pc,d6.w)
PEA_END:

; ----------------------------------------------------------------------
; MOVEQ - Move quick (8-bit immediate to data register)
; ----------------------------------------------------------------------
; Notes:
; - Syntax: MOVEQ #<imm8>,Dn
; - Size is always long (implicit .l)
; - Immediate is SIGNED 8-bit (-128 to +127)
; - Destination must be a data register (Dn)
; - No other addressing modes allowed
; - No size suffix

MOVEQ_SECTION:

        ; --- Boundary values ---
        moveq #0,d0 ; 0
        moveq #1,d1 ; +1
        moveq #127,d2 ; +127 (max positive)
        moveq #-1,d3 ; -1
        moveq #-128,d4 ; -128 (min negative)

        ; --- Pseudo-random values ---
        moveq #85,d5 ; 0x55
        moveq #-86,d6 ; -86 (0xAA)
        moveq #42,d7 ; 0x2A
        moveq #-42,d0 ; -42

        ; --- Full register coverage ---
        moveq #100,d1
        moveq #-100,d2
        moveq #64,d3
        moveq #-64,d4
        moveq #32,d5
        moveq #-32,d6
        moveq #7,d7


; ----------------------------------------------------------------------
; ADD / ADDQ / SUB / SUBQ - Arithmetic and quick add/sub on 68000
; ----------------------------------------------------------------------
; Notes:
; - Sizes: .b, .w, .l where allowed.
; - Addressing modes follow Motorola 68000 rules:
; * ADD/SUB: Dn <-> <ea>, An with ADD/SUBA/SUB/SUBA forms.
; * ADDQ/SUBQ: only certain <ea> (no immediate, no PC-relative, etc.).
; - Immediate values and absolute addresses use boundary and pseudo-random
; patterns as in the header.
; - This section does NOT cover ADDX/SUBX (handled separately).

ADD_SECTION:

        ; ================================================================
        ; ADD.b to data register from various sources
        ; ================================================================
ADDB_ST:
        add.b d0,d1 ; Dn -> Dn
        add.b d2,d3
        add.b (a0),d4 ; (An) -> Dn
        add.b (a1)+,d5 ; (An)+ -> Dn
        add.b -(a2),d6 ; -(An) -> Dn
        add.b 8(a3),d7 ; (d16,An) -> Dn
        add.b 16(a4,d5.l),d0 ; (d8,An,Xn) -> Dn
        add.b $0000.w,d1 ; abs short: +0
        add.b $7FFE.w,d2 ; abs short: +32767 (extension)
        add.b $1234.w,d5 ; abs short: pseudo-random
        add.b $00000000.l,d6 ; abs long: boundary low
        add.b $7FFFFFFE.l,d7 ; abs long: boundary high positive
        add.b $80000000.l,d0 ; abs long: boundary negative
        add.b $FFFFFFFE.l,d1 ; abs long: boundary high
        add.b $12345678.l,d2 ; abs long: pseudo-random
		add.b *-0(pc),d1
ADDB_0: add.b ADDB_0(pc),d0
		add.b *+0(pc),d2
		add.b *+2(pc),d3
		add.b *-2(pc),d4		
		add.b *+32768(pc),d5
		add.b *-32766(pc),d6
		add.b *-0(pc,d0.l),d7
		add.b *+0(pc,d1.w),d0
        add.b *+2(pc,d2.l),d1
        add.b *-2(pc,d3.w),d2
		add.b *+128(pc,d3.l),d3
		add.b *-126(pc,d4.w),d4
		add.b ADDB_ST(pc,d5.l),d5
        add.b ADDB_END(pc,d6.w),d6
		add.b #0,d3
        add.b #1,d4
        add.b #$7F,d5
        add.b #$80,d6
        add.b #$FF,d7
		add.b #$FFFFFFFF,d1
ADDB_END:


        ; ================================================================
        ; ADD.w to data register from various sources
        ; ================================================================
ADDW_ST:
        add.w d0,d1
        add.w d2,d3
        add.w (a0),d4
        add.w (a1)+,d5
        add.w -(a2),d6
        add.w 8(a3),d7
        add.w 16(a4,d5.w),d0
        add.w $0000.w,d1
        add.w $7FFE.w,d2
        add.w $5678.w,d5
        add.w $00000000.l,d6
        add.w $7FFFFFFE.l,d7
        add.w $80000000.l,d0
        add.w $FFFFFFFE.l,d1
        add.w $89ABCDEE.l,d2
		add.w *-0(pc),d1
ADDW_0: add.w ADDW_0(pc),d0
		add.w *+0(pc),d2
		add.w *+2(pc),d3
		add.w *-2(pc),d4		
		add.w *+32768(pc),d5
		add.w *-32766(pc),d6
		add.w *-0(pc,d0.l),d7
		add.w *+0(pc,d1.w),d0
        add.w *+2(pc,d2.l),d1
        add.w *-2(pc,d3.w),d2
		add.w *+128(pc,d3.l),d3
		add.w *-126(pc,d4.w),d4
		add.w ADDW_ST(pc,d5.l),d5
        add.w ADDW_END(pc,d6.w),d6
		add.w #0,d3
        add.w #1,d4
        add.w #$FFFF,d5
        add.w #$8000,d6
        add.w #$7FFF,d7
		add.w #$FFFFFFFF,d1
ADDW_END:


        ; ================================================================
        ; ADD.l to data register from various sources
        ; ================================================================
ADDL_ST:
        add.l d0,d1
        add.l d2,d3
        add.l (a0),d4
        add.l (a1)+,d5
        add.l -(a2),d6
        add.l 8(a3),d7
        add.l 16(a4,d5.l),d0
        add.l $0000.w,d1
        add.l $7FFE.w,d2
        add.l $4320.w,d5
        add.l $00000000.l,d6
        add.l $7FFFFFFE.l,d7
        add.l $80000000.l,d0
        add.l $FFFFFFFE.l,d1
        add.l $11223344.l,d2
		add.l *-0(pc),d1
ADDL_0: add.l ADDL_0(pc),d0
		add.l *+0(pc),d2
		add.l *+2(pc),d3
		add.l *-2(pc),d4		
		add.l *+32768(pc),d5
		add.l *-32766(pc),d6
		add.l *-0(pc,d0.l),d7
		add.l *+0(pc,d1.w),d0
        add.l *+2(pc,d2.l),d1
        add.l *-2(pc,d3.w),d2
		add.l *+128(pc,d3.l),d3
		add.l *-126(pc,d4.w),d4
		add.l ADDL_ST(pc,d5.l),d5
        add.l ADDL_END(pc,d6.w),d6
		add.l #0,d3
        add.l #1,d4
        add.l #$FFFF,d5
        add.l #$8000,d6
        add.l #$7FFF,d7
		add.l #$12345678,d0
		add.l #$FFFFFFFF,d1
ADDL_END:


        ; ================================================================
        ; ADD with destination in memory (Dn -> <ea>)
        ; ================================================================

        add.w d0,(a0)
        add.w d1,(a1)+
        add.w d2,-(a2)
        add.w d3,8(a3)
        add.w d4,16(a4,d5.w)
        add.w d5,$0000.w
        add.w d6,$7FFE.w
        add.w d1,$1234.w
        add.l d2,$00000000.l
        add.l d3,$7FFFFFFE.l
        add.l d4,$80000000.l
        add.l d5,$FFFFFFFE.l
        add.l d6,$12345678.l


ADDQ_SECTION:

        ; ------------------------------------------------------------------
        ; ADDQ - Quick add (3-bit immediate, encoded in opcode)
        ; ------------------------------------------------------------------
        ; Notes:
        ; - Allowed destinations: Dn, An (word/long), memory (except An,
        ; PC-relative, immediate).
        ; - Size .b/.w/.l where legal.

        ; ================================================================
        ; ADDQ.b
        ; ================================================================

        addq.b #1,d0
        addq.b #7,d1
        addq.b #8,d2 ; encoded as 0 (wrap) but still legal
        addq.b #3,(a0)
        addq.b #5,(a1)+
        addq.b #2,-(a2)
        addq.b #6,8(a3)
        addq.b #4,16(a4,d5.w)
        addq.b #1,$0000.w
        addq.b #7,$7FFE.w


        ; ================================================================
        ; ADDQ.w
        ; ================================================================

        addq.w #1,d3
        addq.w #7,d4
        addq.w #8,d5
        addq.w #2,(a0)
        addq.w #6,(a1)+
        addq.w #4,-(a2)
        addq.w #3,8(a3)
        addq.w #5,16(a4,d5.w)
        addq.w #1,$0000.w
        addq.w #7,$7FFE.w

        ; ADDQ.w to address registers (legal)
        addq.w #1,a0
        addq.w #7,a1
        addq.w #8,a2


        ; ================================================================
        ; ADDQ.l
        ; ================================================================

        addq.l #1,d6
        addq.l #7,d7
        addq.l #8,d0
        addq.l #2,(a3)
        addq.l #6,(a4)+
        addq.l #4,-(a5)
        addq.l #3,8(a6)
        addq.l #5,16(a0,d1.w)
        addq.l #1,$00000000.l
        addq.l #7,$7FFFFFFE.l
        addq.l #3,$80000000.l
        addq.l #5,$FFFFFFFE.l

        ; ADDQ.l to address registers (legal)
        addq.l #1,a3
        addq.l #7,a4
        addq.l #8,a5

		; ================================================================
        ; ADDI.b/w/l
        ; ================================================================
        addi.b #$FF,d6
        addi.w #$ABCE,d7
        addi.l #$12345678,d0
		
        addi.b #$7F,(a0)
        addi.w #$7BCE,(a1)
        addi.l #$72345678,(a2)

        addi.b #2,(a0)+
        addi.w #$0124,(a1)+
        addi.l #$CAFEDECA,(a2)+

        addi.b #-2,-(a0)
        addi.w #-16384,-(a1)
        addi.l #$FEDCBA00,-(a2)

        addi.b #$7F,8(a6)
        addi.b #$80,16(a0,d5.l)
        addi.w #$7FFF,8(a6)
        addi.w #$8000,16(a0,d2.w)
        addi.l #$7FFFFFFF,8(a6)
        addi.l #$80000000,16(a0,d1.l)

        addi.b #0,$0000.w
        addi.b #2,$7FFE.w
        addi.b #-2,$0ABC.w
		addi.b #127,$00000000.l
        addi.b #-128,$7FFFFFFE.l

        addi.w #0,$0000.w
        addi.w #2,$7FFE.w
        addi.w #-2,$0ABC.w
        addi.w #16384,$80000000.l
        addi.w #-16385,$FFFFFFFE.l

        addi.l #0,$0000.w
        addi.l #2,$7FFE.w
        addi.l #-2,$0ABC.w
		addi.l #$7FFFFFFF,$00000000.l
        addi.l #$80000000,$7FFFFFFE.l



SUB_SECTION:

        ; ------------------------------------------------------------------
        ; SUB - Subtract
        ; ------------------------------------------------------------------

        ; ================================================================
        ; SUB.b to data register
        ; ================================================================
SUBB_ST:
        sub.b d0,d1
        sub.b d2,d3
        sub.b (a0),d4
        sub.b (a1)+,d5
        sub.b -(a2),d6
        sub.b 8(a3),d7
        sub.b 16(a4,d5.w),d0
        sub.b $0000.w,d1
        sub.b $7FFE.w,d2
        sub.b $1234.w,d5
        sub.b $00000000.l,d6
        sub.b $7FFFFFFE.l,d7
        sub.b $80000000.l,d0
        sub.b $FFFFFFFE.l,d1
        sub.b $12345678.l,d2
		sub.b *-0(pc),d1
SUBB_0: sub.b SUBB_0(pc),d0
		sub.b *+0(pc),d2
		sub.b *+2(pc),d3
		sub.b *-2(pc),d4		
		sub.b *+32768(pc),d5
		sub.b *-32766(pc),d6
		sub.b *-0(pc,d0.l),d7
		sub.b *+0(pc,d1.w),d0
        sub.b *+2(pc,d2.l),d1
        sub.b *-2(pc,d3.w),d2
		sub.b *+128(pc,d3.l),d3
		sub.b *-126(pc,d4.w),d4
		sub.b SUBB_ST(pc,d5.l),d5
        sub.b SUBB_END(pc,d6.w),d6
		sub.b #0,d3
        sub.b #1,d4
        sub.b #$7F,d5
        sub.b #$80,d6
        sub.b #$FF,d7
		sub.b #$FFFFFFFF,d1
SUBB_END:


        ; ================================================================
        ; SUB.w to data register
        ; ================================================================
SUBW_ST:
        sub.w d0,d1
        sub.w d2,d3
        sub.w (a0),d4
        sub.w (a1)+,d5
        sub.w -(a2),d6
        sub.w 8(a3),d7
        sub.w 16(a4,d5.l),d0
        sub.w $0000.w,d1
        sub.w $7FFE.w,d2
        sub.w $5678.w,d5
        sub.w $00000000.l,d6
        sub.w $7FFFFFFE.l,d7
        sub.w $80000000.l,d0
        sub.w $FFFFFFFE.l,d1
        sub.w $89ABCDEE.l,d2
		sub.w *-0(pc),d1
SUBW_0: sub.w SUBW_0(pc),d0
		sub.w *+0(pc),d2
		sub.w *+2(pc),d3
		sub.w *-2(pc),d4		
		sub.w *+32768(pc),d5
		sub.w *-32766(pc),d6
		sub.w *-0(pc,d0.l),d7
		sub.w *+0(pc,d1.w),d0
        sub.w *+2(pc,d2.l),d1
        sub.w *-2(pc,d3.w),d2
		sub.w *+128(pc,d3.l),d3
		sub.w *-126(pc,d4.w),d4
		sub.w SUBW_ST(pc,d5.l),d5
        sub.w SUBW_END(pc,d6.w),d6
		sub.w #0,d3
        sub.w #1,d4
        sub.w #$FFFF,d5
        sub.w #$8000,d6
        sub.w #$7FFF,d7
		sub.w #$FFFFFFFF,d1
SUBW_END:


        ; ================================================================
        ; SUB.l to data register
        ; ================================================================
SUBL_ST:
        sub.l d0,d1
        sub.l d2,d3
        sub.l (a0),d4
        sub.l (a1)+,d5
        sub.l -(a2),d6
        sub.l 8(a3),d7
        sub.l 16(a4,d5.w),d0
        sub.l $0000.w,d1
        sub.l $7FFE.w,d2
        sub.l $4320.w,d5
        sub.l $00000000.l,d6
        sub.l $7FFFFFFE.l,d7
        sub.l $80000000.l,d0
        sub.l $FFFFFFFE.l,d1
        sub.l $11223344.l,d2
		sub.l *-0(pc),d1
SUBL_0: sub.l SUBL_0(pc),d0
		sub.l *+0(pc),d2
		sub.l *+2(pc),d3
		sub.l *-2(pc),d4		
		sub.l *+32768(pc),d5
		sub.l *-32766(pc),d6
		sub.l *-0(pc,d0.l),d7
		sub.l *+0(pc,d1.w),d0
        sub.l *+2(pc,d2.l),d1
        sub.l *-2(pc,d3.w),d2
		sub.l *+128(pc,d3.l),d3
		sub.l *-126(pc,d4.w),d4
		sub.l SUBL_ST(pc,d5.l),d5
        sub.l SUBL_END(pc,d6.w),d6
		sub.l #0,d3
        sub.l #1,d4
        sub.l #$FFFF,d5
        sub.l #$8000,d6
        sub.l #$7FFF,d7
		sub.l #$12345678,d0
		sub.l #$FFFFFFFF,d1
SUBL_END:


        ; ================================================================
        ; SUB with destination in memory (Dn -> <ea>)
        ; ================================================================

        sub.w d0,(a0)
        sub.w d1,(a1)+
        sub.w d2,-(a2)
        sub.w d3,8(a3)
        sub.w d4,16(a4,d5.l)
        sub.w d5,$0000.w
        sub.w d6,$7FFE.w
        sub.l d1,$1234.w
        sub.l d2,$00000000.l
        sub.l d3,$7FFFFFFE.l
        sub.l d4,$80000000.l
        sub.w d5,$FFFFFFFE.l
        sub.W d6,$12345678.l


SUBQ_SECTION:

        ; ------------------------------------------------------------------
        ; SUBQ - Quick subtract (3-bit immediate)
        ; ------------------------------------------------------------------

        ; ================================================================
        ; SUBQ.b
        ; ================================================================

        subq.b #1,d0
        subq.b #7,d1
        subq.b #8,d2
        subq.b #3,(a0)
        subq.b #5,(a1)+
        subq.b #2,-(a2)
        subq.b #6,8(a3)
        subq.b #4,16(a4,d5.w)
        subq.b #1,$0000.w
        subq.b #7,$7FFE.w
        subq.b #5,$0ABC.w
		subq.b #1,$00000000.l
        subq.b #7,$7FFFFFFE.l
        subq.b #3,$80000000.l
        subq.b #5,$FFFFFFFE.l


        ; ================================================================
        ; SUBQ.w
        ; ================================================================

        subq.w #1,d3
        subq.w #7,d4
        subq.w #8,d5
        subq.w #2,(a0)
        subq.w #6,(a1)+
        subq.w #4,-(a2)
        subq.w #3,8(a3)
        subq.w #5,16(a4,d5.w)
        subq.w #1,$0000.w
        subq.w #7,$7FFE.w
        subq.w #5,$0ABC.w
		subq.w #1,$00000000.l
        subq.w #7,$7FFFFFFE.l
        subq.w #3,$80000000.l
        subq.w #5,$FFFFFFFE.l

        ; SUBQ.w to address registers
        subq.w #1,a0
        subq.w #7,a1
        subq.w #8,a2


        ; ================================================================
        ; SUBQ.l
        ; ================================================================

        subq.l #1,d6
        subq.l #7,d7
        subq.l #8,d0
        subq.l #2,(a3)
        subq.l #6,(a4)+
        subq.l #4,-(a5)
        subq.l #3,8(a6)
        subq.l #5,16(a0,d1.w)
        subq.l #1,$0000.w
        subq.l #7,$7FFE.w
        subq.l #5,$0ABC.w
		subq.l #1,$00000000.l
        subq.l #7,$7FFFFFFE.l
        subq.l #3,$80000000.l
        subq.l #5,$FFFFFFFE.l


        ; SUBQ.l to address registers
        subq.l #1,a3
        subq.l #7,a4
        subq.l #8,a5

		; ================================================================
        ; SUBI.b/w/l
        ; ================================================================
        subi.b #$FF,d6
        subi.w #$ABCE,d7
        subi.l #$12345678,d0
		
        subi.b #$7F,(a0)
        subi.w #$7BCE,(a1)
        subi.l #$72345678,(a2)

        subi.b #2,(a0)+
        subi.w #$0124,(a1)+
        subi.l #$CAFEDECA,(a2)+

        subi.b #-2,-(a0)
        subi.w #-16384,-(a1)
        subi.l #$FEDCBA00,-(a2)

        subi.b #$7F,8(a6)
        subi.b #$80,16(a0,d5.l)
        subi.w #$7FFF,8(a6)
        subi.w #$8000,16(a0,d2.w)
        subi.l #$7FFFFFFF,8(a6)
        subi.l #$80000000,16(a0,d1.l)

        subi.b #0,$0000.w
        subi.b #2,$7FFE.w
        subi.b #-2,$0ABC.w
		subi.b #127,$00000000.l
        subi.b #-128,$7FFFFFFE.l

        subi.w #0,$0000.w
        subi.w #2,$7FFE.w
        subi.w #-2,$0ABC.w
        subi.w #16384,$80000000.l
        subi.w #-16385,$FFFFFFFE.l

        subi.l #0,$0000.w
        subi.l #2,$7FFE.w
        subi.l #-2,$0ABC.w
		subi.l #$7FFFFFFF,$00000000.l
        subi.l #$80000000,$7FFFFFFE.l


; ----------------------------------------------------------------------
; ADDA / SUBA - Add/Subtract to address registers
; ----------------------------------------------------------------------
; Notes:
; - ADDA.<size> <ea>,An
; Sizes: .w (sign-extended 16-bit), .l (32-bit)
; Allowed <ea>: all data/address modes except immediate for .w?
; Practically: Dn, (An), (An)+, -(An), (d16,An), (d8,An,Xn),
; abs.w, abs.l, #<imm> (encoded as ADDI to An)
;
; - SUBA.<size> <ea>,An
; Same rules as ADDA.
;
; - .w operands are SIGN-EXTENDED to 32-bit before addition/subtraction.
; - Absolute short is SIGNED 16-bit.
; - Absolute long is full 32-bit.

ADDA_SUBA_SECTION:

; ======================================================================
; ADDA.w <ea>,An (word, sign-extended)
; ======================================================================
ADDAW_ST:
        adda.w d0,a0
        adda.w d1,a1
        adda.w d7,a2

        adda.w (a0),a3
        adda.w (a1)+,a4
        adda.w -(a2),a5
        adda.w 8(a3),a6
        adda.w 16(a4,d5.w),a7

        adda.w $0000.w,a0 ; +0
        adda.w $7FFE.w,a1 
        adda.w $1358.w,a4 ; pseudo-random

        adda.w $00000000.l,a5 ; boundary low
        adda.w $7FFFFFFE.l,a6 ; boundary high positive
        adda.w $80000000.l,a7 ; boundary negative
        adda.w $FFFFFFFE.l,a0 ; boundary high
        adda.w $CAFEBABE.l,a1 ; pseudo-random

		adda.w *-0(pc),a1
ADDAW_0:adda.w ADDAW_0(pc),a0
		adda.w *+0(pc),a2
		adda.w *+2(pc),a3
		adda.w *-2(pc),a4		
		adda.w *+32768(pc),a5
		adda.w *-32766(pc),a6
		adda.w *-0(pc,d0.l),a7
		adda.w *+0(pc,d1.w),a0
        adda.w *+2(pc,d2.l),a1
        adda.w *-2(pc,d3.w),a2
		adda.w *+128(pc,d3.l),a3
		adda.w *-126(pc,d4.w),a4
		adda.w ADDAW_ST(pc,d5.l),a5
        adda.w ADDAW_END(pc,d6.w),a6
		adda.w #0,a3
        adda.w #1,a4
        adda.w #$FFFF,a5
        adda.w #$8000,a6
        adda.w #$7FFF,a7
		adda.w #$FFFFFFFF,a1
ADDAW_END:


; ======================================================================
; ADDA.l <ea>,An (long, full 32-bit)
; ======================================================================
ADDAL_ST:
        adda.l d0,a0
        adda.l d1,a1
        adda.l d7,a2

        adda.l (a0),a3
        adda.l (a1)+,a4
        adda.l -(a2),a5
        adda.l 8(a3),a6
        adda.l 16(a4,d5.w),a7

        adda.l $0000.w,a0 ; sign-extended word
        adda.l $7FFE.w,a1
        adda.l $2468.w,a4

        adda.l $00000000.l,a5 ; boundary low
        adda.l $7FFFFFFE.l,a6 ; boundary high positive
        adda.l $80000000.l,a7 ; boundary negative
        adda.l $FFFFFFFE.l,a0 ; boundary high
        adda.l $DEADBEEE.l,a1 ; pseudo-random

 		adda.l *-0(pc),a1
ADDAL_0:adda.l ADDAL_0(pc),a0
		adda.l *+0(pc),a2
		adda.l *+2(pc),a3
		adda.l *-2(pc),a4		
		adda.l *+32768(pc),a5
		adda.l *-32766(pc),a6
		adda.l *-0(pc,d0.l),a7
		adda.l *+0(pc,d1.w),a0
        adda.l *+2(pc,d2.l),a1
        adda.l *-2(pc,d3.w),a2
		adda.l *+128(pc,d3.l),a3
		adda.l *-126(pc,d4.w),a4
		adda.l ADDAL_ST(pc,d5.l),a5
        adda.l ADDAL_END(pc,d6.w),a6
		adda.l #0,a3
        adda.l #1,a4
        adda.l #$FFFF,a5
        adda.l #$8000,a6
        adda.l #$7FFF,a7
		adda.l #$12345678,a0
		adda.l #$FFFFFFFF,a1
ADDAL_END:



; ======================================================================
; SUBA.w <ea>,An (word, sign-extended)
; ======================================================================
SUBAW_ST:
        suba.w d0,a0
        suba.w d1,a1
        suba.w d7,a2

        suba.w (a0),a3
        suba.w (a1)+,a4
        suba.w -(a2),a5
        suba.w 8(a3),a6
        suba.w 16(a4,d5.w),a7

        suba.w $0000.w,a0 ; +0
        suba.w $7FFE.w,a1 
        suba.w $1358.w,a4 ; pseudo-random

        suba.w $00000000.l,a5 ; boundary low
        suba.w $7FFFFFFE.l,a6 ; boundary high positive
        suba.w $80000000.l,a7 ; boundary negative
        suba.w $FFFFFFFE.l,a0 ; boundary high
        suba.w $CAFEBABE.l,a1 ; pseudo-random

		suba.w *-0(pc),a1
SUBAW_0:suba.w SUBAW_0(pc),a0
		suba.w *+0(pc),a2
		suba.w *+2(pc),a3
		suba.w *-2(pc),a4		
		suba.w *+32768(pc),a5
		suba.w *-32766(pc),a6
		suba.w *-0(pc,d0.l),a7
		suba.w *+0(pc,d1.w),a0
        suba.w *+2(pc,d2.l),a1
        suba.w *-2(pc,d3.w),a2
		suba.w *+128(pc,d3.l),a3
		suba.w *-126(pc,d4.w),a4
		suba.w SUBAW_ST(pc,d5.l),a5
        suba.w SUBAW_END(pc,d6.w),a6
		suba.w #0,a3
        suba.w #1,a4
        suba.w #$FFFF,a5
        suba.w #$8000,a6
        suba.w #$7FFF,a7
		suba.w #$FFFFFFFF,a1
SUBAW_END:


; ======================================================================
; SUBA.l <ea>,An (long, full 32-bit)
; ======================================================================
SUBAL_ST:
        suba.l d0,a0
        suba.l d1,a1
        suba.l d7,a2

        suba.l (a0),a3
        suba.l (a1)+,a4
        suba.l -(a2),a5
        suba.l 8(a3),a6
        suba.l 16(a4,d5.w),a7

        suba.l $0000.w,a0 ; sign-extended word
        suba.l $7FFE.w,a1
        suba.l $2468.w,a4

        suba.l $00000000.l,a5 ; boundary low
        suba.l $7FFFFFFE.l,a6 ; boundary high positive
        suba.l $80000000.l,a7 ; boundary negative
        suba.l $FFFFFFFE.l,a0 ; boundary high
        suba.l $DEADBEEE.l,a1 ; pseudo-random

		suba.l *-0(pc),a1
SUBAL_0:suba.l SUBAL_0(pc),a0
		suba.l *+0(pc),a2
		suba.l *+2(pc),a3
		suba.l *-2(pc),a4		
		suba.l *+32768(pc),a5
		suba.l *-32766(pc),a6
		suba.l *-0(pc,d0.l),a7
		suba.l *+0(pc,d1.w),a0
        suba.l *+2(pc,d2.l),a1
        suba.l *-2(pc,d3.w),a2
		suba.l *+128(pc,d3.l),a3
		suba.l *-126(pc,d4.w),a4
		suba.l SUBAL_ST(pc,d5.l),a5
        suba.l SUBAL_END(pc,d6.w),a6
		suba.l #0,a3
        suba.l #1,a4
        suba.l #$FFFF,a5
        suba.l #$8000,a6
        suba.l #$7FFF,a7
		suba.l #$12345678,a0
		suba.l #$FFFFFFFF,a1
SUBAL_END:

; ----------------------------------------------------------------------
; CMP / CMPA / CMPI / CMPM - Compare instructions on 68000
; ----------------------------------------------------------------------
; Notes:
; - CMP.<size> <ea>,Dn
; Allowed sizes: .b, .w, .l
; Allowed <ea>: all data addressing modes except An
;
; - CMPA.<size> <ea>,An
; Allowed sizes: .w, .l
; Allowed <ea>: all address-producing modes
;
; - CMPI.<size> #imm,<ea>
; Allowed sizes: .b, .w, .l
; Allowed <ea>: all data-alterable modes (no An, no immediate)
;
; - CMPM.<size> (Ax)+,(Ay)+
; Allowed sizes: .b, .w, .l
; Only postincrement addressing mode
;
; - Absolute short is SIGNED 16-bit.
; - Absolute long is full 32-bit.
; - Immediate values use boundary and pseudo-random patterns.

CMP_SECTION:

; ======================================================================
; CMP.b <ea>,Dn
; ======================================================================
CMPB_ST:
        cmp.b d0,d1
        cmp.b (a0),d2
        cmp.b (a1)+,d3
        cmp.b -(a2),d4
        cmp.b 8(a3),d5
        cmp.b 16(a4,d5.w),d6
        cmp.b $0000.w,d7 ; abs short +0
        cmp.b $7FFE.w,d0 ; +32767
        cmp.b $1234.w,d3 ; pseudo-random
        cmp.b $00000000.l,d4 ; abs long low
        cmp.b $7FFFFFFE.l,d5 ; abs long high+
        cmp.b $80000000.l,d6 ; abs long negative
        cmp.b $FFFFFFFE.l,d7 ; abs long high
        cmp.b $12345678.l,d0 ; pseudo-random
		cmp.b *-0(pc),d1
CMPB_0: cmp.b CMPB_0(pc),d0
		cmp.b *+0(pc),d2
		cmp.b *+2(pc),d3
		cmp.b *-2(pc),d4		
		cmp.b *+32768(pc),d5
		cmp.b *-32766(pc),d6
		cmp.b *-0(pc,d0.l),d7
		cmp.b *+0(pc,d1.w),d0
        cmp.b *+2(pc,d2.l),d1
        cmp.b *-2(pc,d3.w),d2
		cmp.b *+128(pc,d3.l),d3
		cmp.b *-126(pc,d4.w),d4
		cmp.b CMPB_ST(pc,d5.l),d5
        cmp.b CMPB_END(pc,d6.w),d6
		cmp.b #0,d3
        cmp.b #1,d4
        cmp.b #$7F,d5
        cmp.b #$80,d6
        cmp.b #$FF,d7
		cmp.b #$FFFFFFFF,d1
CMPB_END:


; ======================================================================
; CMP.w <ea>,Dn
; ======================================================================
CMPW_ST:
        cmp.w d0,d1
        cmp.w (a0),d2
        cmp.w (a1)+,d3
        cmp.w -(a2),d4
        cmp.w 8(a3),d5
        cmp.w 16(a4,d5.w),d6
        cmp.w $0000.w,d7
        cmp.w $7FFE.w,d0
        cmp.w $5678.w,d3
        cmp.w $00000000.l,d4
        cmp.w $7FFFFFFE.l,d5
        cmp.w $80000000.l,d6
        cmp.w $FFFFFFFE.l,d7
        cmp.w $89ABCDEE.l,d0
		cmp.w *-0(pc),d1
CMPW_0: cmp.w CMPW_0(pc),d0
		cmp.w *+0(pc),d2
		cmp.w *+2(pc),d3
		cmp.w *-2(pc),d4		
		cmp.w *+32768(pc),d5
		cmp.w *-32766(pc),d6
		cmp.w *-0(pc,d0.l),d7
		cmp.w *+0(pc,d1.w),d0
        cmp.w *+2(pc,d2.l),d1
        cmp.w *-2(pc,d3.w),d2
		cmp.w *+128(pc,d3.l),d3
		cmp.w *-126(pc,d4.w),d4
		cmp.w CMPW_ST(pc,d5.l),d5
        cmp.w CMPW_END(pc,d6.w),d6
		cmp.w #0,d3
        cmp.w #1,d4
        cmp.w #$FFFF,d5
        cmp.w #$8000,d6
        cmp.w #$7FFF,d7
		cmp.w #$FFFFFFFF,d1
CMPW_END:


; ======================================================================
; CMP.l <ea>,Dn
; ======================================================================
CMPL_ST:
        cmp.l d0,d1
        cmp.l (a0),d2
        cmp.l (a1)+,d3
        cmp.l -(a2),d4
        cmp.l 8(a3),d5
        cmp.l 16(a4,d5.w),d6
        cmp.l $0000.w,d7
        cmp.l $7FFE.w,d0
        cmp.l $4320.w,d3
        cmp.l $00000000.l,d4
        cmp.l $7FFFFFFE.l,d5
        cmp.l $80000000.l,d6
        cmp.l $FFFFFFFE.l,d7
        cmp.l $11223344.l,d0
		cmp.l *-0(pc),d1
CMPL_0: cmp.l CMPL_0(pc),d0
		cmp.l *+0(pc),d2
		cmp.l *+2(pc),d3
		cmp.l *-2(pc),d4		
		cmp.l *+32768(pc),d5
		cmp.l *-32766(pc),d6
		cmp.l *-0(pc,d0.l),d7
		cmp.l *+0(pc,d1.w),d0
        cmp.l *+2(pc,d2.l),d1
        cmp.l *-2(pc,d3.w),d2
		cmp.l *+128(pc,d3.l),d3
		cmp.l *-126(pc,d4.w),d4
		cmp.l CMPL_ST(pc,d5.l),d5
        cmp.l CMPL_END(pc,d6.w),d6
		cmp.l #0,d3
        cmp.l #1,d4
        cmp.l #$FFFF,d5
        cmp.l #$8000,d6
        cmp.l #$7FFF,d7
		cmp.l #$12345678,d0
		cmp.l #$FFFFFFFF,d1
CMPL_END:


; ======================================================================
; CMPA.w <ea>,An
; ======================================================================
CMPAW_ST:
        cmpa.w d0,a0
        cmpa.w (a0),a1
        cmpa.w (a1)+,a2
        cmpa.w -(a2),a3
        cmpa.w 8(a3),a4
        cmpa.w 16(a4,d5.w),a5
        cmpa.w $0000.w,a6
        cmpa.w $7FFE.w,a7
        cmpa.w $1358.w,a2
        cmpa.w $00000000.l,a3
        cmpa.w $7FFFFFFE.l,a4
        cmpa.w $80000000.l,a5
        cmpa.w $FFFFFFFE.l,a6
        cmpa.w $2468ACE0.l,a7
		cmpa.w *-0(pc),a1
CMPAW_0:cmpa.w CMPAL_0(pc),a0
		cmpa.w *+0(pc),a2
		cmpa.w *+2(pc),a3
		cmpa.w *-2(pc),a4		
		cmpa.w *+32768(pc),a5
		cmpa.w *-32766(pc),a6
		cmpa.w *-0(pc,d0.l),a7
		cmpa.w *+0(pc,d1.w),a0
        cmpa.w *+2(pc,d2.l),a1
        cmpa.w *-2(pc,d3.w),a2
		cmpa.w *+128(pc,d3.l),a3
		cmpa.w *-126(pc,d4.w),a4
		cmpa.w CMPAW_ST(pc,d5.l),a5
        cmpa.w CMPAW_END(pc,d6.w),a6
		cmpa.w #0,a3
        cmpa.w #1,a4
        cmpa.w #$FFFF,a5
        cmpa.w #$8000,a6
        cmpa.w #$7FFF,a7
		cmpa.w #$FFFFFFFF,a0
CMPAW_END:


; ======================================================================
; CMPA.l <ea>,An
; ======================================================================
CMPAL_ST:
        cmpa.l d0,a0
        cmpa.l (a0),a1
        cmpa.l (a1)+,a2
        cmpa.l -(a2),a3
        cmpa.l 8(a3),a4
        cmpa.l 16(a4,d5.w),a5
        cmpa.l $0000.w,a6
        cmpa.l $7FFE.w,a7
        cmpa.l $4320.w,a2
        cmpa.l $00000000.l,a3
        cmpa.l $7FFFFFFE.l,a4
        cmpa.l $80000000.l,a5
        cmpa.l $FFFFFFFE.l,a6
        cmpa.l $11223344.l,a7
		cmpa.l *-0(pc),a1
CMPAL_0:cmpa.l CMPAL_0(pc),a0
		cmpa.l *+0(pc),a2
		cmpa.l *+2(pc),a3
		cmpa.l *-2(pc),a4		
		cmpa.l *+32768(pc),a5
		cmpa.l *-32766(pc),a6
		cmpa.l *-0(pc,d0.l),a7
		cmpa.l *+0(pc,d1.w),a0
        cmpa.l *+2(pc,d2.l),a1
        cmpa.l *-2(pc,d3.w),a2
		cmpa.l *+128(pc,d3.l),a3
		cmpa.l *-126(pc,d4.w),a4
		cmpa.l CMPAL_ST(pc,d5.l),a5
        cmpa.l CMPAL_END(pc,d6.w),a6
		cmpa.l #0,a3
        cmpa.l #1,a4
        cmpa.l #$FFFF,a5
        cmpa.l #$8000,a6
        cmpa.l #$7FFF,a7
		cmpa.l #$12345678,a0
		cmpa.l #$FFFFFFFF,a1
CMPAL_END:

; ======================================================================
; CMPI.<size> #imm,<ea>
; ======================================================================

        ; --- CMPI.b ---
        cmpi.b #$00,d0
        cmpi.b #$7F,(a0)
        cmpi.b #$80,(a1)+
        cmpi.b #$FF,-(a2)
        cmpi.b #$12,8(a3)
        cmpi.b #$34,16(a4,d5.w)
        cmpi.b #$00,$0000.w
        cmpi.b #$7F,$7FFF.w
        cmpi.b #$55,$1234.w
        cmpi.b #$AA,$00000000.l
        cmpi.b #$FF,$FFFFFFFE.l
        cmpi.b #$7F,$7FFFFFFE.l
        cmpi.b #$80,$80000000.l
        cmpi.b #$33,$12345678.l

        ; --- CMPI.w ---
        cmpi.w #$0000,d1
        cmpi.w #$7FFF,(a0)
        cmpi.w #$8000,(a1)+
        cmpi.w #$FFFF,-(a2)
        cmpi.w #$1357,8(a3)
        cmpi.w #$2468,16(a4,d5.l)
        cmpi.w #$0000,$0000.w
        cmpi.w #$7FFF,$7FFE.w
        cmpi.w #$AAAA,$1234.w
        cmpi.w #$0000,$00000000.l
        cmpi.w #$FFFF,$FFFFFFFE.l
        cmpi.w #$7FFF,$7FFFFFFE.l
        cmpi.w #$8000,$80000000.l
        cmpi.w #$BEEF,$12345678.l

        ; --- CMPI.l ---
        cmpi.l #$00000000,d2
        cmpi.l #$7FFFFFFF,(a0)
        cmpi.l #$80000000,(a1)+
        cmpi.l #$FFFFFFFF,-(a2)
        cmpi.l #$12345678,8(a3)
        cmpi.l #$89ABCDEF,16(a4,d5.w)
        cmpi.l #$00000000,$0000.w
        cmpi.l #$7FFFFFFF,$7FFE.w
        cmpi.l #$CAFEBABE,$1234.w
        cmpi.l #$00000000,$00000000.l
        cmpi.l #$FFFFFFFF,$FFFFFFFE.l
        cmpi.l #$7FFFFFFF,$7FFFFFFE.l
        cmpi.l #$80000000,$80000000.l
        cmpi.l #$DEADBEEF,$2468ACE0.l


; ======================================================================
; CMPM.<size> (Ax)+,(Ay)+
; ======================================================================
; Only postincrement addressing mode is allowed.

        ; --- CMPM.b ---
        cmpm.b (a0)+,(a1)+
        cmpm.b (a2)+,(a3)+
        cmpm.b (a4)+,(a5)+
        cmpm.b (a6)+,(a7)+
        cmpm.b (a7)+,(a0)+

        ; --- CMPM.w ---
        cmpm.w (a0)+,(a1)+
        cmpm.w (a2)+,(a3)+
        cmpm.w (a4)+,(a5)+
        cmpm.w (a6)+,(a7)+
        cmpm.w (a7)+,(a0)+

        ; --- CMPM.l ---
        cmpm.l (a0)+,(a1)+
        cmpm.l (a2)+,(a3)+
        cmpm.l (a4)+,(a5)+
        cmpm.l (a6)+,(a7)+
        cmpm.l (a7)+,(a0)+

; ----------------------------------------------------------------------
; AND / OR / EOR / NOT / TST - Logical operations on 68000
; ----------------------------------------------------------------------
; Notes:
; - Sizes: .b, .w, .l where allowed.
; - Addressing modes follow Motorola 68000 rules:
; * Register <-> memory forms for AND/OR/EOR.
; * NOT and TST operate on <ea> (no An, no PC-relative, no immediate).
; - Immediate logical ops to CCR/SR are handled in a separate SR/CCR section.

AND_SECTION:

        ; ================================================================
        ; AND.b to data register from various sources
        ; ================================================================

        and.b d0,d1 ; Dn -> Dn
        and.b d2,d3
        and.b (a0),d4 ; (An) -> Dn
        and.b (a1)+,d5 ; (An)+ -> Dn
        and.b -(a2),d6 ; -(An) -> Dn
        and.b 8(a3),d7 ; (d16,An) -> Dn
        and.b 16(a4,d5.w),d0 ; (d8,An,Xn) -> Dn
        and.b $0000.w,d1 ; abs short: +0
        and.b $7FFE.w,d2 ; abs short: +32766
        and.b $1234.w,d5 ; abs short: pseudo-random
        and.b $00000000.l,d6 ; abs long: boundary low
        and.b $7FFFFFFE.l,d7 ; abs long: boundary high positive
        and.b $80000000.l,d0 ; abs long: boundary negative
        and.b $FFFFFFFE.l,d1 ; abs long: boundary high
        and.b $12345678.l,d2 ; abs long: pseudo-random


        ; ================================================================
        ; AND.w to data register
        ; ================================================================

        and.w d0,d1
        and.w d2,d3
        and.w (a0),d4
        and.w (a1)+,d5
        and.w -(a2),d6
        and.w 8(a3),d7
        and.w 16(a4,d5.w),d0
        and.w $0000.w,d1
        and.w $7FFE.w,d2
        and.w $5678.w,d5
        and.w $00000000.l,d6
        and.w $7FFFFFFE.l,d7
        and.w $80000000.l,d0
        and.w $FFFFFFFE.l,d1
        and.w $89ABCDEE.l,d2


        ; ================================================================
        ; AND.l to data register
        ; ================================================================

        and.l d0,d1
        and.l d2,d3
        and.l (a0),d4
        and.l (a1)+,d5
        and.l -(a2),d6
        and.l 8(a3),d7
        and.l 16(a4,d5.l),d0
        and.l $0000.w,d1
        and.l $7FFE.w,d2
        and.l $1234.w,d5
        and.l $00000000.l,d6
        and.l $7FFFFFFE.l,d7
        and.l $80000000.l,d0
        and.l $FFFFFFFE.l,d1
        and.l $11223344.l,d2


        ; ================================================================
        ; AND with destination in memory (Dn -> <ea>)
        ; ================================================================

        and.b d0,(a0)
        and.b d1,(a1)+
        and.b d2,-(a2)
        and.b d3,8(a3)
        and.b d4,16(a4,d5.w)
        and.b d5,$0000.w
        and.b d6,$7FFF.w
        and.b d1,$1234.w

        and.w d2,$00000000.l
        and.w d3,$7FFFFFFE.l
        and.w d4,$80000000.l
        and.w d5,$FFFFFFFE.l
        and.w d6,$12345678.l


OR_SECTION:

        ; ------------------------------------------------------------------
        ; OR - Logical OR
        ; ------------------------------------------------------------------

        ; ================================================================
        ; OR.b to data register
        ; ================================================================

        or.b d0,d1
        or.b d2,d3
        or.b (a0),d4
        or.b (a1)+,d5
        or.b -(a2),d6
        or.b 8(a3),d7
        or.b 16(a4,d5.w),d0
        or.b $0000.w,d1
        or.b $7FFE.w,d2
        or.b $1234.w,d5
        or.b $00000000.l,d6
        or.b $7FFFFFFE.l,d7
        or.b $80000000.l,d0
        or.b $FFFFFFFE.l,d1
        or.b $12345678.l,d2


        ; ================================================================
        ; OR.w to data register
        ; ================================================================

        or.w d0,d1
        or.w d2,d3
        or.w (a0),d4
        or.w (a1)+,d5
        or.w -(a2),d6
        or.w 8(a3),d7
        or.w 16(a4,d5.l),d0
        or.w $0000.w,d1
        or.w $7FFE.w,d2
        or.w $5678.w,d5
        or.w $00000000.l,d6
        or.w $7FFFFFFE.l,d7
        or.w $80000000.l,d0
        or.w $FFFFFFFE.l,d1
        or.w $89ABCDEE.l,d2


        ; ================================================================
        ; OR.l to data register
        ; ================================================================

        or.l d0,d1
        or.l d2,d3
        or.l (a0),d4
        or.l (a1)+,d5
        or.l -(a2),d6
        or.l 8(a3),d7
        or.l 16(a4,d5.w),d0
        or.l $0000.w,d1
        or.l $7FFE.w,d2
        or.l $1234.w,d5
        or.l $00000000.l,d6
        or.l $7FFFFFFE.l,d7
        or.l $80000000.l,d0
        or.l $FFFFFFFE.l,d1
        or.l $11223344.l,d2


        ; ================================================================
        ; OR with destination in memory (Dn -> <ea>)
        ; ================================================================

        or.b d0,(a0)
        or.b d1,(a1)+
        or.b d2,-(a2)
        or.b d3,8(a3)
        or.b d4,16(a4,d5.l)
        or.b d5,$0000.w
        or.b d6,$7FFE.w
        or.b d1,$1234.w

        or.w d2,$00000000.l
        or.w d3,$7FFFFFFE.l
        or.w d4,$80000000.l
        or.w d5,$FFFFFFFE.l
        or.w d6,$12345678.l


EOR_SECTION:

        ; ------------------------------------------------------------------
        ; EOR - Exclusive OR
        ; ------------------------------------------------------------------

        ; ================================================================
        ; EOR with destination in memory (Dn -> <ea>)
        ; ================================================================

        eor.b d0,d1
        eor.b d2,d3
        eor.b d0,(a0)
        eor.b d1,(a1)+
        eor.b d2,-(a2)
        eor.b d3,8(a3)
        eor.b d4,16(a4,d5.w)
        eor.b d5,$0000.w
        eor.b d6,$7FFE.w
        eor.b d1,$1234.w
        eor.b d2,$00000000.l
        eor.b d3,$7FFFFFFE.l
        eor.b d4,$80000000.l
        eor.b d5,$FFFFFFFE.l
        eor.b d6,$12345678.l

        eor.w d0,d1
        eor.w d2,d3
        eor.w d0,(a0)
        eor.w d1,(a1)+
        eor.w d2,-(a2)
        eor.w d3,8(a3)
        eor.w d4,16(a4,d5.l)
        eor.w d5,$0000.w
        eor.w d6,$7FFE.w
        eor.w d1,$1234.w
        eor.w d2,$00000000.l
        eor.w d3,$7FFFFFFE.l
        eor.w d4,$80000000.l
        eor.w d5,$FFFFFFFE.l
        eor.w d6,$12345678.l

        eor.l d0,d1
        eor.l d2,d3
        eor.l d0,(a0)
        eor.l d1,(a1)+
        eor.l d2,-(a2)
        eor.l d3,8(a3)
        eor.l d4,16(a4,d5.w)
        eor.l d5,$0000.w
        eor.l d6,$7FFE.w
        eor.l d1,$1234.w
        eor.l d2,$00000000.l
        eor.l d3,$7FFFFFFE.l
        eor.l d4,$80000000.l
        eor.l d5,$FFFFFFFE.l
        eor.l d6,$12345678.l

NOT_SECTION:

        ; ------------------------------------------------------------------
        ; NOT - Logical complement
        ; ------------------------------------------------------------------
        ; NOT operates on <ea> (no An, no PC-relative, no immediate).

        not.b d0
        not.b d1
        not.b (a0)
        not.b (a1)+
        not.b -(a2)
        not.b 8(a3)
        not.b 16(a4,d5.l)
        not.b $0000.w
        not.b $7FFE.w
        not.b $1234.w
        not.b $00000000.l
        not.b $7FFFFFFE.l
        not.b $80000000.l
        not.b $FFFFFFFE.l
        not.b $2468ACE0.l

        not.w d2
        not.w d3
        not.w (a4)
        not.w (a5)+
        not.w -(a6)
        not.w 12(a0)
        not.w 24(a1,d2.l)
        not.w $0000.w
        not.w $7FFE.w
        not.w $5678.w
        not.w $00000000.l
        not.w $7FFFFFFE.l
        not.w $80000000.l
        not.w $FFFFFFFE.l
        not.w $2468ACE0.l

        not.l d4
        not.l d5
        not.l (a2)
        not.l (a3)+
        not.l -(a4)
        not.l 16(a5)
        not.l 32(a6,d3.w)
        not.l $0000.w
        not.l $7FFE.w
        not.l $5678.w
        not.l $00000000.l
        not.l $7FFFFFFE.l
        not.l $80000000.l
        not.l $FFFFFFFE.l
        not.l $2468ACE0.l


TST_SECTION:

        ; ------------------------------------------------------------------
        ; TST - Test operand
        ; ------------------------------------------------------------------
        ; TST operates on <ea> (no An, no PC-relative, no immediate).

        tst.b d0
        tst.b d1
        tst.b (a0)
        tst.b (a1)+
        tst.b -(a2)
        tst.b 8(a3)
        tst.b 16(a4,d5.w)
        tst.b $0000.w
        tst.b $7FFE.w
        tst.b $1234.w
        tst.b $00000000.l
        tst.b $7FFFFFFE.l
        tst.b $80000000.l
        tst.b $FFFFFFFE.l
        tst.b $2468ACE0.l

        tst.w d2
        tst.w d3
        tst.w (a4)
        tst.w (a5)+
        tst.w -(a6)
        tst.w 12(a0)
        tst.w 24(a1,d2.l)
        tst.w $0000.w
        tst.w $7FFE.w
        tst.w $5678.w
        tst.w $00000000.l
        tst.w $7FFFFFFE.l
        tst.w $80000000.l
        tst.w $FFFFFFFE.l
        tst.w $2468ACE0.l

        tst.l d4
        tst.l d5
        tst.l (a2)
        tst.l (a3)+
        tst.l -(a4)
        tst.l 16(a5)
        tst.l 32(a6,d3.w)
        tst.l $0000.w
        tst.l $7FFE.w
        tst.l $5678.w
        tst.l $00000000.l
        tst.l $7FFFFFFE.l
        tst.l $80000000.l
        tst.l $FFFFFFFE.l
        tst.l $2468ACE0.l


        andi.b #$00,d0
        andi.b #$FF,(a0)
		andi.b #$FF,CCR
        andi.w #$0000,d1
        andi.w #$FFFF,$0000.w
        andi.w #$1234,SR
		andi.l #$00000000,d2
        andi.l #$FFFFFFFF,$00000000.l

        ori.b #$00,d0
        ori.b #$FF,(a0)
        ori.b #$12,CCR
        ori.w #$0000,d1
        ori.w #$FFFF,$0000.w
		ori.w #$1234,SR
        ori.l #$00000000,d2
        ori.l #$FFFFFFFF,$00000000.l

        eori.b #$00,d0
        eori.b #$FF,(a0)
		eori.b #$12,CCR
        eori.w #$0000,d1
        eori.w #$FFFF,$0000.w
		eori.w #$1234,SR
        eori.l #$00000000,d2
        eori.l #$FFFFFFFF,$00000000.l


; ----------------------------------------------------------------------
; CLR / NEG / NEGX - Clear, negate, negate with extend
; ----------------------------------------------------------------------
; Notes:
; - CLR.<size> <ea>
; Clears operand to zero.
; Allowed sizes: .b, .w, .l
; Allowed <ea>: all data-alterable modes (no An, no immediate)
;
; - NEG.<size> <ea>
; Two's complement negate.
; Allowed sizes: .b, .w, .l
; Allowed <ea>: all data-alterable modes (no An, no immediate)
;
; - NEGX.<size> <ea>
; Two's complement negate with extend (X flag).
; Allowed sizes: .b, .w, .l
; Allowed <ea>: all data-alterable modes (no An, no immediate)
;
; - Absolute short is SIGNED 16-bit.
; - Absolute long is full 32-bit.

CLR_NEG_NEGX_SECTION:

; ======================================================================
; CLR.<size> <ea>
; ======================================================================

        ; --- CLR.b ---
        clr.b d0
        clr.b (a0)
        clr.b (a1)+
        clr.b -(a2)
        clr.b 8(a3)
        clr.b 16(a4,d5.w)
        clr.b $0000.w
        clr.b $7FFE.w
        clr.b $1234.w
        clr.b $00000000.l
        clr.b $7FFFFFFE.l
        clr.b $80000000.l
        clr.b $FFFFFFFE.l
        clr.b $2468ACE0.l

        ; --- CLR.w ---
        clr.w d1
        clr.w (a0)
        clr.w (a1)+
        clr.w -(a2)
        clr.w 12(a3)
        clr.w 24(a4,d5.l)
        clr.w $0000.w
        clr.w $7FFE.w
        clr.w $5678.w
        clr.w $00000000.l
        clr.w $7FFFFFFE.l
        clr.w $80000000.l
        clr.w $FFFFFFFE.l
        clr.w $13572468.l

        ; --- CLR.l ---
        clr.l d2
        clr.l (a0)
        clr.l (a1)+
        clr.l -(a2)
        clr.l 16(a3)
        clr.l 32(a4,d5.l)
        clr.l $0000.w
        clr.l $7FFE.w
        clr.l $5678.w
        clr.l $00000000.l
        clr.l $7FFFFFFE.l
        clr.l $80000000.l
        clr.l $FFFFFFFE.l
        clr.l $DEADBEEE.l


; ======================================================================
; NEG.<size> <ea>
; ======================================================================

        ; --- NEG.b ---
        neg.b d0
        neg.b (a0)
        neg.b (a1)+
        neg.b -(a2)
        neg.b 8(a3)
        neg.b 16(a4,d5.w)
        neg.b $0000.w
        neg.b $7FFE.w
        neg.b $1234.w
        neg.b $00000000.l
        neg.b $7FFFFFFE.l
        neg.b $80000000.l
        neg.b $FFFFFFFE.l
        neg.b $CAFEBABE.l

        ; --- NEG.w ---
        neg.w d1
        neg.w (a0)
        neg.w (a1)+
        neg.w -(a2)
        neg.w 12(a3)
        neg.w 24(a4,d5.l)
        neg.w $0000.w
        neg.w $7FFE.w
        neg.w $5678.w
        neg.w $00000000.l
        neg.w $7FFFFFFE.l
        neg.w $80000000.l
        neg.w $FFFFFFFE.l
        neg.w $13572468.l

        ; --- NEG.l ---
        neg.l d2
        neg.l (a0)
        neg.l (a1)+
        neg.l -(a2)
        neg.l 16(a3)
        neg.l 32(a4,d5.w)
        neg.l $0000.w
        neg.l $7FFE.w
        neg.l $5678.w
        neg.l $00000000.l
        neg.l $7FFFFFFE.l
        neg.l $80000000.l
        neg.l $FFFFFFFE.l
        neg.l $DEADBEEE.l


; ======================================================================
; NEGX.<size> <ea>
; ======================================================================

        ; --- NEGX.b ---
        negx.b d0
        negx.b (a0)
        negx.b (a1)+
        negx.b -(a2)
        negx.b 8(a3)
        negx.b 16(a4,d5.w)
        negx.b $0000.w
        negx.b $7FFE.w
        negx.b $1234.w
        negx.b $00000000.l
        negx.b $7FFFFFFE.l
        negx.b $80000000.l
        negx.b $FFFFFFFE.l
        negx.b $CAFEBABE.l

        ; --- NEGX.w ---
        negx.w d1
        negx.w (a0)
        negx.w (a1)+
        negx.w -(a2)
        negx.w 12(a3)
        negx.w 24(a4,d5.l)
        negx.w $0000.w
        negx.w $7FFE.w
        negx.w $5678.w
        negx.w $00000000.l
        negx.w $7FFFFFFE.l
        negx.w $80000000.l
        negx.w $FFFFFFFE.l
        negx.w $13572468.l

        ; --- NEGX.l ---
        negx.l d2
        negx.l (a0)
        negx.l (a1)+
        negx.l -(a2)
        negx.l 16(a3)
        negx.l 32(a4,d5.w)
        negx.l $00000000.l
        negx.l $7FFFFFFE.l
        negx.l $80000000.l
        negx.l $FFFFFFFE.l
        negx.l $DEADBEEE.l

; ----------------------------------------------------------------------
; EXG - Exchange registers
; ----------------------------------------------------------------------
; Notes:
; - Three legal forms on 68000:
; EXG Dx,Dy (data <-> data)
; EXG Ax,Ay (address <-> address)
; EXG Dx,Ay (data <-> address)
;
; - No size suffix.
; - No memory operands.
; - No immediate.
; - No PC-relative or other addressing modes.
; - Pure register-to-register exchange.

EXG_SECTION:

; ======================================================================
; EXG Dx,Dy (data register exchange)
; ======================================================================

        exg d0,d1
        exg d2,d3
        exg d4,d5
        exg d6,d7
        exg d7,d0 ; wrap-around pairing


; ======================================================================
; EXG Ax,Ay (address register exchange)
; ======================================================================

        exg a0,a1
        exg a2,a3
        exg a4,a5
        exg a6,a7
        exg a7,a0 ; wrap-around pairing


; ======================================================================
; EXG Dx,Ay (data <-> address exchange)
; ======================================================================

        exg d0,a0
        exg d1,a2
        exg d3,a4
        exg d5,a6
        exg d7,a7
        exg d6,a1 ; mixed pairing

; ----------------------------------------------------------------------
; MULS / MULU / DIVS / DIVU - Signed/Unsigned multiply and divide
; ----------------------------------------------------------------------
; Notes:
; - MULS.<w> <ea>,Dn : signed 16×16 → 32-bit result in Dn
; - MULU.<w> <ea>,Dn : unsigned 16×16 → 32-bit result in Dn
;
; - DIVS.<w> <ea>,Dn : signed 32/16 → quotient in low word, remainder in high word
; - DIVU.<w> <ea>,Dn : unsigned 32/16 → same layout
;
; - Size is always .w (operand is 16-bit)
; - Allowed <ea>: all data addressing modes except An
; - Absolute short is SIGNED 16-bit
; - Absolute long is full 32-bit

MULDIV_SECTION:

; ======================================================================
; MULS.w <ea>,Dn (signed multiply)
; ======================================================================
MULS_ST:
        muls.w d0,d1
        muls.w (a0),d2
        muls.w (a1)+,d3
        muls.w -(a2),d4
        muls.w 8(a3),d5
        muls.w 16(a4,d5.w),d6
        muls.w $0000.w,d7
        muls.w $7FFE.w,d0
        muls.w $3210.w,d2
        muls.w $00000000.l,d4
        muls.w $7FFFFFFE.l,d5
        muls.w $80000000.l,d6
        muls.w $FFFFFFFE.l,d7
        muls.w $CAFEBABE.l,d0
		muls.w *-0(pc),d1
MULS_0: muls.w MULS_0(pc),d0
		muls.w *+0(pc),d2
		muls.w *+2(pc),d3
		muls.w *-2(pc),d4		
		muls.w *+32768(pc),d5
		muls.w *-32766(pc),d6
		muls.w *-0(pc,d0.l),d7
		muls.w *+0(pc,d1.w),d0
        muls.w *+2(pc,d2.l),d1
        muls.w *-2(pc,d3.w),d2
		muls.w *+128(pc,d3.l),d3
		muls.w *-126(pc,d4.w),d4
		muls.w MULS_ST(pc,d5.l),d5
        muls.w MULS_END(pc,d6.w),d6
		muls.w #0,d3
        muls.w #1,d4
        muls.w #$FFFF,d5
        muls.w #$8000,d6
        muls.w #$7FFF,d7
MULS_END:

; ======================================================================
; MULU.w <ea>,Dn (unsigned multiply)
; ======================================================================
MULU_ST:
        mulu.w d0,d1
        mulu.w (a0),d2
        mulu.w (a1)+,d3
        mulu.w -(a2),d4
        mulu.w 8(a3),d5
        mulu.w 16(a4,d5.w),d6
        mulu.w $0000.w,d7
        mulu.w $7FFE.w,d0
        mulu.w $2468.w,d3
        mulu.w $00000000.l,d4
        mulu.w $7FFFFFFE.l,d5
        mulu.w $80000000.l,d6
        mulu.w $FFFFFFFE.l,d7
        mulu.w $DEADBEEE.l,d0
		mulu.w *+0(pc),d1
MULU_0: mulu.w MULU_0(pc),d0
		mulu.w *-0(pc),d2
		mulu.w *+2(pc),d3
		mulu.w *-2(pc),d4		
		mulu.w *+32768(pc),d5
		mulu.w *-32766(pc),d6
		mulu.w *-0(pc,d0.l),d7
		mulu.w *+0(pc,d1.w),d0
        mulu.w *+2(pc,d2.l),d1
        mulu.w *-2(pc,d3.w),d2
		mulu.w *+128(pc,d3.l),d3
		mulu.w *-126(pc,d4.w),d4
		mulu.w MULU_ST(pc,d5.l),d5
        mulu.w MULU_END(pc,d6.w),d6
		mulu.w #0,d3
        mulu.w #1,d4
        mulu.w #$FFFF,d5
        mulu.w #$8000,d6
        mulu.w #$7FFF,d7
MULU_END:

; ======================================================================
; DIVS.w <ea>,Dn (signed divide)
; ======================================================================
DIVS_ST:
        divs.w d0,d1
        divs.w (a0),d2
        divs.w (a1)+,d3
        divs.w -(a2),d4
        divs.w 8(a3),d5
        divs.w 16(a4,d5.w),d6
        divs.w $0000.w,d7
        divs.w $7FFE.w,d0
        divs.w $1356.w,d3
        divs.w $00000000.l,d4
        divs.w $7FFFFFFE.l,d5
        divs.w $80000000.l,d6
        divs.w $FFFFFFFE.l,d7
        divs.w $CAFEBABE.l,d0
		divs.w *+0(pc),d1
DIVS_0: divs.w DIVS_0(pc),d0
		divs.w *-0(pc),d2
		divs.w *+2(pc),d3
		divs.w *-2(pc),d4		
		divs.w *+32768(pc),d5
		divs.w *-32766(pc),d6
		divs.w *-0(pc,d0.l),d7
		divs.w *+0(pc,d1.w),d0
        divs.w *+2(pc,d2.l),d1
        divs.w *-2(pc,d3.w),d2
		divs.w *+128(pc,d3.l),d3
		divs.w *-126(pc,d4.w),d4
		divs.w DIVS_ST(pc,d5.l),d5
        divs.w DIVS_END(pc,d6.w),d6
		divs.w #0,d3
        divs.w #1,d4
        divs.w #$FFFF,d5
        divs.w #$8000,d6
        divs.w #$7FFF,d7
DIVS_END:

; ======================================================================
; DIVU.w <ea>,Dn (unsigned divide)
; ======================================================================
DIVU_ST:
        divu.w d0,d1
        divu.w (a0),d2
        divu.w (a1)+,d3
        divu.w -(a2),d4
        divu.w 8(a3),d5
        divu.w 16(a4,d5.w),d6
        divu.w $0000.w,d7
        divu.w $7FFE.w,d0
        divu.w $2468.w,d3
        divu.w $00000000.l,d4
        divu.w $7FFFFFFE.l,d5
        divu.w $80000000.l,d6
        divu.w $FFFFFFFE.l,d7
        divu.w $DEADBEEE.l,d0
		divu.w *+0(pc),d1
DIVU_0: divu.w DIVU_0(pc),d0
		divu.w *-0(pc),d2
		divu.w *+2(pc),d3
		divu.w *-2(pc),d4		
		divu.w *+32768(pc),d5
		divu.w *-32766(pc),d6
		divu.w *-0(pc,d0.l),d7
		divu.w *+0(pc,d1.w),d0
        divu.w *+2(pc,d2.l),d1
        divu.w *-2(pc,d3.w),d2
		divu.w *+128(pc,d3.l),d3
		divu.w *-126(pc,d4.w),d4
		divu.w DIVU_ST(pc,d5.l),d5
        divu.w DIVU_END(pc,d6.w),d6
		divu.w #0,d3
        divu.w #1,d4
        divu.w #$FFFF,d5
        divu.w #$8000,d6
        divu.w #$7FFF,d7
DIVU_END:


; ----------------------------------------------------------------------
; SHIFT / ROTATE instructions on 68000
; ----------------------------------------------------------------------
; Notes:
;   - Register shifts/rotates:
;       ASL/ASR, LSL/LSR, ROL/ROR, ROXL/ROXR
;       Count = #1–8 or Dn
;       Destination = Dn only
;       Sizes = .b, .w, .l
;
;   - Memory shifts:
;       ASL.w <ea>, ASR.w <ea>, LSL.w <ea>, LSR.w <ea>
;       Only .w size allowed
;       <ea> = any data alterable addressing mode
;       No rotate, no ROXL/ROXR, no count register, no immediate
;
;   - Absolute short uses signed 16 bit extension words.
;   - Absolute long uses full 32 bit values.

SHIFT_ROTATE_SECTION:

; ======================================================================
; REGISTER SHIFTS: ASL / ASR
; ======================================================================

        ; --- ASL.b with immediate count ---
        asl.b   #1,d0
        asl.b   #3,d1
        asl.b   #7,d2
        asl.b   #8,d3          ; encoded as 0 (wrap)

        ; --- ASL.b with register count ---
        asl.b   d0,d4
        asl.b   d1,d5
        asl.b   d2,d6

        ; --- ASL.w ---
        asl.w   #1,d0
        asl.w   #7,d1
        asl.w   d2,d3

        ; --- ASL.l ---
        asl.l   #1,d4
        asl.l   #7,d5
        asl.l   d6,d7

        ; --- ASR.b ---
        asr.b   #1,d0
        asr.b   #7,d1
        asr.b   d2,d3

        ; --- ASR.w ---
        asr.w   #1,d4
        asr.w   #7,d5
        asr.w   d6,d7

        ; --- ASR.l ---
        asr.l   #1,d0
        asr.l   #7,d1
        asr.l   d2,d3


; ======================================================================
; REGISTER SHIFTS: LSL / LSR
; ======================================================================

        ; --- LSL.b ---
        lsl.b   #1,d0
        lsl.b   #7,d1
        lsl.b   d2,d3

        ; --- LSL.w ---
        lsl.w   #1,d4
        lsl.w   #7,d5
        lsl.w   d6,d7

        ; --- LSL.l ---
        lsl.l   #1,d0
        lsl.l   #7,d1
        lsl.l   d2,d3

        ; --- LSR.b ---
        lsr.b   #1,d4
        lsr.b   #7,d5
        lsr.b   d6,d7

        ; --- LSR.w ---
        lsr.w   #1,d0
        lsr.w   #7,d1
        lsr.w   d2,d3

        ; --- LSR.l ---
        lsr.l   #1,d4
        lsr.l   #7,d5
        lsr.l   d6,d7


; ======================================================================
; REGISTER ROTATES: ROL / ROR
; ======================================================================

        ; --- ROL.b ---
        rol.b   #1,d0
        rol.b   #7,d1
        rol.b   d2,d3

        ; --- ROL.w ---
        rol.w   #1,d4
        rol.w   #7,d5
        rol.w   d6,d7

        ; --- ROL.l ---
        rol.l   #1,d0
        rol.l   #7,d1
        rol.l   d2,d3

        ; --- ROR.b ---
        ror.b   #1,d4
        ror.b   #7,d5
        ror.b   d6,d7

        ; --- ROR.w ---
        ror.w   #1,d0
        ror.w   #7,d1
        ror.w   d2,d3

        ; --- ROR.l ---
        ror.l   #1,d4
        ror.l   #7,d5
        ror.l   d6,d7


; ======================================================================
; REGISTER EXTENDED ROTATES: ROXL / ROXR
; ======================================================================

        ; --- ROXL.b ---
        roxl.b  #1,d0
        roxl.b  #7,d1
        roxl.b  d2,d3

        ; --- ROXL.w ---
        roxl.w  #1,d4
        roxl.w  #7,d5
        roxl.w  d6,d7

        ; --- ROXL.l ---
        roxl.l  #1,d0
        roxl.l  #7,d1
        roxl.l  d2,d3

        ; --- ROXR.b ---
        roxr.b  #1,d4
        roxr.b  #7,d5
        roxr.b  d6,d7

        ; --- ROXR.w ---
        roxr.w  #1,d0
        roxr.w  #7,d1
        roxr.w  d2,d3

        ; --- ROXR.l ---
        roxr.l  #1,d4
        roxr.l  #7,d5
        roxr.l  d6,d7


; ======================================================================
; MEMORY SHIFTS (ONLY ASL/ASR/LSL/LSR, ONLY .w)
; ======================================================================
; Allowed <ea>:
;   (An), (An)+, -(An), (d16,An), (d8,An,Xn),
;   absolute short (signed), absolute long

        ; --- ASL.w <ea> ---
        asl.w   (a0)
        asl.w   (a1)+
        asl.w   -(a2)
        asl.w   8(a3)
        asl.w   16(a4,d5.w)
        asl.w   $0000.w
        asl.w   $7FFE.w
        asl.w   $1234.w
        asl.w   $00000000.l
        asl.w   $7FFFFFFE.l
        asl.w   $80000000.l
        asl.w   $FFFFFFFE.l
        asl.w   $2468ACE0.l

        ; --- ASR.w <ea> ---
        asr.w   (a0)
        asr.w   (a1)+
        asr.w   -(a2)
        asr.w   8(a3)
        asr.w   16(a4,d5.w)
        asr.w   $0000.w
        asr.w   $7FFE.w
        asr.w   $5678.w
        asr.w   $00000000.l
        asr.w   $7FFFFFFE.l
        asr.w   $80000000.l
        asr.w   $FFFFFFFE.l
        asr.w   $13572468.l

        ; --- LSL.w <ea> ---
        lsl.w   (a0)
        lsl.w   (a1)+
        lsl.w   -(a2)
        lsl.w   8(a3)
        lsl.w   16(a4,d5.w)
        lsl.w   $0000.w
        lsl.w   $7FFE.w
        lsl.w   $0ABC.w
        lsl.w   $00000000.l
        lsl.w   $7FFFFFFE.l
        lsl.w   $80000000.l
        lsl.w   $FFFFFFFE.l
        lsl.w   $DEADBEEE.l

        ; --- LSR.w <ea> ---
        lsr.w   (a0)
        lsr.w   (a1)+
        lsr.w   -(a2)
        lsr.w   8(a3)
        lsr.w   16(a4,d5.w)
        lsr.w   $0000.w
        lsr.w   $7FFE.w
        lsr.w   $0ABC.w
        lsr.w   $00000000.l
        lsr.w   $7FFFFFFE.l
        lsr.w   $80000000.l
        lsr.w   $FFFFFFFE.l
        lsr.w   $CAFEBABE.l


; ======================================================================
; Branches / DBcc / Scc
; ======================================================================

DBCC_SCC_SECTION:

        bra LABEL_BRA
        bsr LABEL_BSR

        beq LABEL_BEQ
        bne LABEL_BNE
        bcs LABEL_BCS
        bcc LABEL_BCC
        bmi LABEL_BMI
        bpl LABEL_BPL
        bvs LABEL_BVS
        bvc LABEL_BVC
        bge LABEL_BGE
        blt LABEL_BLT
        bgt LABEL_BGT
        ble LABEL_BLE

LABEL_BRA:
        nop
LABEL_BSR:
        rts
LABEL_BEQ:
        nop
LABEL_BNE:
        nop
LABEL_BCS:
        nop
LABEL_BCC:
        nop
LABEL_BMI:
        nop
LABEL_BPL:
        nop
LABEL_BVS:
        nop
LABEL_BVC:
        nop
LABEL_BGE:
        nop
LABEL_BLT:
        nop
LABEL_BGT:
        nop
LABEL_BLE:
        nop

        dbra d0,LABEL_DBRA
        dbf d1,LABEL_DBF
        dbhi d2,LABEL_DBHI
        dbeq d3,LABEL_DBEQ
        dbne d4,LABEL_DBNE

LABEL_DBRA:
        nop
LABEL_DBF:
        nop
LABEL_DBHI:
        nop
LABEL_DBEQ:
        nop
LABEL_DBNE:
        nop

        st d0
        sf d1
        shi d2
        sls d3
        scc d4
        scs d5
        sne d6
        seq d7
        spl (a0)
        smi (a1)
        svc (a2)
        svs (a3)
        sge (a4)
        slt (a5)
        sgt (a6)
        sle (a0)

; ----------------------------------------------------------------------
; BRANCHES - BRA / BSR / Bcc / DBcc on 68000
; ----------------------------------------------------------------------
; Notes:
; - BRA / BSR:
; * 8-bit displacement (short) or 16-bit displacement (word).
; * Displacement is signed.
; - Bcc (conditional branches):
; * Same displacement rules as BRA.
; * Conditions: EQ, NE, CS, CC, MI, PL, VS, VC, HI, LS, GE, LT, GT, LE.
; - DBcc:
; * Uses a data register (Dn) and a 16-bit signed displacement.
; - Runtime PC alignment and actual target addresses are NOT validated
; (assembly-only target).

BRA_BSR_SECTION:

; ======================================================================
; BRA / BSR - Unconditional and subroutine branches
; ======================================================================

        ; --- BRA with 8-bit displacement ---
        bra *+2 ; small forward
        bra *-2 ; small backward
        bra *+126 ; near boundary
        bra *-128 ; boundary -128

        ; --- BRA with 16-bit displacement (word) ---
        bra.w *+0 
        bra.w *+32768 
        bra.w *-32766 
        bra.w *-2 

        ; --- BSR with 8-bit displacement ---
        bsr *+2
        bsr *-2
        bsr *+126
        bsr *-128

        ; --- BSR with 16-bit displacement ---
        bsr.w *+0
        bsr.w *+32768
        bsr.w *-32766
        bsr.w *-2


; ======================================================================
; Bcc - Conditional branches (8-bit and 16-bit)
; ======================================================================
; Conditions:
; EQ, NE, CS, CC, MI, PL, VS, VC,
; HI, LS, GE, LT, GT, LE

        ; --- 8-bit displacements ---
        beq *+0
        bne *-2
        bcs *+4
        bcc *-4
        bmi *+128
        bpl *-0
        bvs *+8
        bvc *-8
        bhi *+10
        bls *-10
        bge *+12
        blt *-12
        bgt *+14
        ble *-126
		beq *+16

        ; --- 16-bit displacements (word) ---
        beq.w *+0
        bne.w *+32768
        bcs.w *-32766
        bcc.w *-2
        bmi.w *+1234
        bpl.w *-1234
        bvs.w *+2000
        bvc.w *-2000
        bhi.w *+3000
        bls.w *-3000
        bge.w *+4000
        blt.w *-4000
        bgt.w *+5000
        ble.w *-5000


; ======================================================================
; DBcc - Decrement and branch on condition
; ======================================================================
; DBcc uses:
; dbEQ, dbNE, dbCS, dbCC, dbMI, dbPL, dbVS, dbVC,
; dbHI, dbLS, dbGE, dbLT, dbGT, dbLE
; with:
; DBcc Dn,<16-bit signed displacement>

        dbra d0,*+0 ; DBRA = DBcc with always true condition
        dbra d1,*-2
        dbra d2,*+100
        dbra d3,*-100

        dbeq d4,*+0
        dbne d5,*-2
        dbcs d6,*+32768
        dbcc d7,*-32766

        dbmi d0,*+300
        dbpl d1,*-300
        dbvs d2,*+400
        dbvc d3,*-400

        dbhi d4,*+500
        dbls d5,*-500
        dbge d6,*+600
        dblt d7,*-600
        dbgt d0,*+700
        dble d1,*-700


; ----------------------------------------------------------------------
; JMP / JSR - Control transfer instructions
; ----------------------------------------------------------------------
; Notes:
; - Syntax:
; JMP <ea>
; JSR <ea>
;
; - Allowed <ea> (control-alterable):
; (An)
; (d16,An)
; (d8,An,Xn)
; abs.w
; abs.l
;
; - NOT allowed:
; Dn
; An
; (An)+
; -(An)
; immediate
; PC-relative (68000)
;
; - Absolute short is SIGNED 16-bit.
; - Absolute long is full 32-bit.

JMP_JSR_SECTION:

; ======================================================================
; JMP <ea>
; ======================================================================

        jmp (a0)
        jmp (a1)
        jmp 8(a2)
        jmp 16(a3,d4.w)
        jmp 32(a4,d5.l)

        jmp $0000.w ; +0
        jmp $7FFE.w ; +32767
        jmp $1358.w ; pseudo-random

        jmp $00000000.l ; boundary low
        jmp $7FFFFFFE.l ; boundary high positive
        jmp $80000000.l ; boundary negative
        jmp $FFFFFFFE.l ; boundary high
        jmp $CAFEBABE.l ; pseudo-random


; ======================================================================
; JSR <ea>
; ======================================================================

        jsr (a0)
        jsr (a1)
        jsr 8(a2)
        jsr 16(a3,d4.w)
        jsr 32(a4,d5.l)

        jsr $0000.w ; +0
        jsr $7FFE.w ; +32767
        jsr $2468.w ; pseudo-random

        jsr $00000000.l ; boundary low
        jsr $7FFFFFFE.l ; boundary high positive
        jsr $80000000.l ; boundary negative
        jsr $FFFFFFFE.l ; boundary high
        jsr $DEADBEEE.l ; pseudo-random


; ----------------------------------------------------------------------
; BTST / BCHG / BCLR / BSET - Bit operations
; ----------------------------------------------------------------------
; Notes:
; - Two forms exist:
; 1) BTST/BCHG/BCLR/BSET Dn,<ea> (bit number in data register)
; 2) BTST/BCHG/BCLR/BSET #imm,<ea> (bit number immediate)
;
; - Allowed <ea> for register form:
; * Dn (register direct)
; * All data addressing modes (except An)
;
; - Allowed <ea> for immediate form:
; * All data-alterable modes (except Dn, An, immediate)
; * BTST #imm,Dn IS allowed (unlike BCHG/BCLR/BSET)
;
; - Sizes:
; * On Dn: always long (implicit)
; * On memory: always byte (implicit)
;
; - Absolute short is SIGNED 16-bit.
; - Absolute long is full 32-bit.

BITOPS_SECTION:

; ======================================================================
; BTST Dn,<ea> (bit number in register)
; ======================================================================

        btst d0,d1
        btst d2,(a0)
        btst d3,(a1)+
        btst d4,-(a2)
        btst d5,8(a3)
        btst d6,16(a4,d5.w)
        btst d7,$0000.w
        btst d0,$7FFE.w
        btst d3,$1234.w
        btst d4,$00000000.l
        btst d5,$7FFFFFFE.l
        btst d6,$80000000.l
        btst d7,$FFFFFFFE.l
        btst d1,$2468ACE0.l


; ======================================================================
; BTST #imm,<ea> (bit number immediate)
; ======================================================================

        btst #31,d0
		btst #16,d1
		btst #1,d2
        btst #0,(a0)
        btst #1,(a1)+
        btst #5,-(a2)
        btst #6,8(a3)
        btst #7,16(a4,d5.w)

        btst #1,$0000.w
        btst #2,$7FFF.w
        btst #6,$1234.w

        btst #7,$00000000.l
        btst #1,$7FFFFFFE.l
        btst #2,$80000000.l
        btst #4,$FFFFFFFE.l
        btst #0,$CAFEBABE.l


; ======================================================================
; BCHG Dn,<ea> (bit number in register)
; ======================================================================

        bchg d0,d1
        bchg d2,(a0)
        bchg d3,(a1)+
        bchg d4,-(a2)
        bchg d5,8(a3)
        bchg d6,16(a4,d5.w)
        bchg d7,$0000.w
        bchg d0,$7FFF.w
        bchg d3,$1234.w
        bchg d4,$00000000.l
        bchg d5,$7FFFFFFE.l
        bchg d6,$80000000.l
        bchg d7,$FFFFFFFE.l
        bchg d1,$2468ACE0.l


; ======================================================================
; BCHG #imm,<ea> (bit number immediate, memory only)
; ======================================================================

        bchg #31,d0
		bchg #16,d1
		bchg #1,d2
        bchg #0,(a0)
        bchg #1,(a1)+
        bchg #5,-(a2)
        bchg #6,8(a3)
        bchg #7,16(a4,d5.w)

        bchg #1,$0000.w
        bchg #2,$7FFF.w
        bchg #6,$1234.w

        bchg #7,$00000000.l
        bchg #1,$7FFFFFFE.l
        bchg #2,$80000000.l
        bchg #4,$FFFFFFFE.l
        bchg #0,$CAFEBABE.l


; ======================================================================
; BCLR Dn,<ea> (bit number in register)
; ======================================================================

        bclr d0,d1
        bclr d2,(a0)
        bclr d3,(a1)+
        bclr d4,-(a2)
        bclr d5,8(a3)
        bclr d6,16(a4,d5.w)
        bclr d7,$0000.w
        bclr d0,$7FFF.w
        bclr d3,$1234.w
        bclr d4,$00000000.l
        bclr d5,$7FFFFFFE.l
        bclr d6,$80000000.l
        bclr d7,$FFFFFFFE.l
        bclr d1,$2468ACE0.l


; ======================================================================
; BCLR #imm,<ea> (bit number immediate, memory only)
; ======================================================================

        bclr #31,d0
		bclr #16,d1
		bclr #1,d2
        bclr #0,(a0)
        bclr #1,(a1)+
        bclr #5,-(a2)
        bclr #6,8(a3)
        bclr #7,16(a4,d5.w)

        bclr #1,$0000.w
        bclr #2,$7FFF.w
        bclr #6,$1234.w

        bclr #7,$00000000.l
        bclr #1,$7FFFFFFE.l
        bclr #2,$80000000.l
        bclr #4,$FFFFFFFE.l
        bclr #0,$CAFEBABE.l


; ======================================================================
; BSET Dn,<ea> (bit number in register)
; ======================================================================

        bset d0,d1
        bset d2,(a0)
        bset d3,(a1)+
        bset d4,-(a2)
        bset d5,8(a3)
        bset d6,16(a4,d5.w)
        bset d7,$0000.w
        bset d0,$7FFF.w
        bset d3,$1234.w
        bset d4,$00000000.l
        bset d5,$7FFFFFFE.l
        bset d6,$80000000.l
        bset d7,$FFFFFFFE.l
        bset d1,$2468ACE0.l


; ======================================================================
; BSET #imm,<ea> (bit number immediate, memory only)
; ======================================================================

        bset #31,d0
		bset #16,d1
		bset #1,d2
        bset #0,(a0)
        bset #1,(a1)+
        bset #5,-(a2)
        bset #6,8(a3)
        bset #7,16(a4,d5.w)

        bset #1,$0000.w
        bset #2,$7FFF.w
        bset #6,$1234.w

        bset #7,$00000000.l
        bset #0,$7FFFFFFE.l
        bset #2,$80000000.l
        bset #4,$FFFFFFFE.l
        bset #1,$CAFEBABE.l

; ----------------------------------------------------------------------
; MOVEM - Move multiple registers
; ----------------------------------------------------------------------
; Notes:
; - Two directions:
; MOVEM.<size> <ea>,<reglist> (memory -> registers)
; MOVEM.<size> <reglist>,<ea> (registers -> memory)
;
; - Sizes: .w and .l only.
;
; - Allowed <ea> for memory -> registers:
; (An), (An)+, (d16,An), (d8,An,Xn), abs.w, abs.l
; NOT allowed: Dn, -(An), immediate
;
; - Allowed <ea> for registers -> memory:
; (An), -(An), (d16,An), (d8,An,Xn), abs.w, abs.l
; NOT allowed: Dn, (An)+, immediate
;
; - Register list examples:
; d0-d7/a0-a7, mixed, single, random patterns
;
; - Absolute short is SIGNED 16-bit.
; - Absolute long is full 32-bit.

MOVEM_SECTION:

; ======================================================================
; MOVEM.<size> <ea>,<reglist> (memory -> registers)
; ======================================================================

        ; --- MOVEM.w from memory ---
        movem.w (a0),d0-d7
        movem.w (a1),a0-a6
        movem.w (a2),d0-d3/a4-a6
        movem.w (a3)+,d1/d3/d5/d7
        movem.w 8(a4),d2-d4/a1/a7
        movem.w 16(a5,d6.w),d0/a0/a3/a5
        movem.w $0000.w,d0-d7/a0-a6
        movem.w $7FFE.w,d1/d4/d6/a2/a5
        movem.w $0ABC.w,d3/d5/a1/a7
        movem.w $0002.w,d0/d2/d4/d6/a0/a2/a4/a6
        movem.w $1234.w,d7/a7
        movem.w $00000000.l,d0-d7
        movem.w $7FFFFFFE.l,a0-a7
        movem.w $80000000.l,d0-d3/a4-a7
        movem.w $FFFFFFFE.l,d1/d3/d5/d7/a1/a3/a5/a7
        movem.w $CAFEBABE.l,d0/d2/d4/d6

        ; --- MOVEM.l from memory ---
        movem.l (a0),d0-d7
        movem.l (a1),a0-a6
        movem.l (a2),d0-d3/a4-a6
        movem.l (a3)+,d1/d3/d5/d7
        movem.l 8(a4),d2-d4/a1/a7
        movem.l 16(a5,d6.l),d0/a0/a3/a5
        movem.l $0000.w,d0-d7/a0-a6
        movem.l $7FFE.w,d1/d4/d6/a2/a5
        movem.l $0ABC.w,d3/d5/a1/a7
        movem.l $0002.w,d0/d2/d4/d6/a0/a2/a4/a6
        movem.l $1234.w,d7/a7
        movem.l $00000000.l,d0-d7
        movem.l $7FFFFFFE.l,a0-a7
        movem.l $80000000.l,d0-d3/a4-a7
        movem.l $FFFFFFFE.l,d1/d3/d5/d7/a1/a3/a5/a7
        movem.l $DEADBEEE.l,d0/d2/d4/d6


; ======================================================================
; MOVEM.<size> <reglist>,<ea> (registers -> memory)
; ======================================================================

        ; --- MOVEM.w to memory ---
        movem.w d0-d7,(a0)
        movem.w a0-a6,(a1)
        movem.w d0-d3/a4-a6,(a2)
        movem.w d1/d3/d5/d7,-(a3)
        movem.w d2-d4/a1/a7,8(a4)
        movem.w d0/a0/a3/a5,16(a5,d6.w)
        movem.w d0-d7/a0-a6,$0000.w
        movem.w d1/d4/d6/a2/a5,$7FFE.w
        movem.w d3/d5/a1/a7,$0ABC.w
        movem.w d0/d2/d4/d6/a0/a2/a4/a6,$0002.w
        movem.w d7/a7,$1234.w
        movem.w d0-d7,$00000000.l
        movem.w a0-a7,$7FFFFFFE.l
        movem.w d0-d3/a4-a7,$80000000.l
        movem.w d1/d3/d5/d7/a1/a3/a5/a7,$FFFFFFFE.l
        movem.w d0/d2/d4/d6,$CAFEBABE.l

        ; --- MOVEM.l to memory ---
        movem.l d0-d7,(a0)
        movem.l a0-a6,(a1)
        movem.l d0-d3/a4-a6,(a2)
        movem.l d1/d3/d5/d7,-(a3)
        movem.l d2-d4/a1/a7,8(a4)
        movem.l d0/a0/a3/a5,16(a5,d6.l)
        movem.l d0-d7/a0-a6,$0000.w
        movem.l d1/d4/d6/a2/a5,$7FFE.w
        movem.l d3/d5/a1/a7,$0ABC.w
        movem.l d0/d2/d4/d6/a0/a2/a4/a6,$0002.w
        movem.l d7/a7,$1234.w
        movem.l d0-d7,$00000000.l
        movem.l a0-a7,$7FFFFFFE.l
        movem.l d0-d3/a4-a7,$80000000.l
        movem.l d1/d3/d5/d7/a1/a3/a5/a7,$FFFFFFFE.l
        movem.l d0/d2/d4/d6,$DEADBEEE.l


; ----------------------------------------------------------------------
; TAS - Test and Set Operand
; ----------------------------------------------------------------------
; Notes:
; - Syntax: TAS <ea>
; - No size suffix (implicit byte operation).
; - Allowed <ea>: all data-alterable modes except An.
; * Dn
; * (An)
; * (An)+
; * -(An)
; * (d16,An)
; * (d8,An,Xn)
; * absolute short (signed 16-bit)
; * absolute long (full 32-bit)
; - NOT allowed:
; * An
; * immediate
; * PC-relative
; - Absolute short is SIGNED 16-bit.

TAS_SECTION:

; ======================================================================
; TAS <ea>
; ======================================================================

        ; --- Data register ---
        tas d0
        tas d1
        tas d7

        ; --- Address register indirect ---
        tas (a0)
        tas (a1)

        ; --- Postincrement ---
        tas (a2)+
        tas (a3)+

        ; --- Predecrement ---
        tas -(a4)
        tas -(a5)

        ; --- Displacement ---
        tas 8(a6)
        tas 16(a0)

        ; --- Indexed ---
        tas 12(a1,d2.w)
        tas 24(a2,d3.l)

        ; --- Absolute short (signed 16-bit) ---
        tas $0000.w ; +0
        tas $7FFE.w 
        tas $1358.w ; pseudo-random

        ; --- Absolute long (full 32-bit) ---
        tas $00000000.l ; boundary low
        tas $7FFFFFFE.l ; boundary high positive
        tas $80000000.l ; boundary negative
        tas $FFFFFFFE.l ; boundary high
        tas $2468ACE0.l ; pseudo-random



; ======================================================================
; TRAP / TRAPV
; ======================================================================

TRAP_SECTION:

        trap #0
        trap #1
        trap #15

        trapv

; ----------------------------------------------------------------------
; LINK / UNLK - Create and remove stack frames
; ----------------------------------------------------------------------
; Notes:
; - LINK An,#<disp16>
; Creates a stack frame.
; Displacement is SIGNED 16-bit.
; Only address registers allowed.
;
; - UNLK An
; Restores previous frame pointer.
; Only address registers allowed.
;
; - Absolute short is SIGNED 16-bit.
; - No other addressing modes allowed.

LINK_UNLK_SECTION:

; ======================================================================
; LINK An,#<disp16>
; ======================================================================

        link a0,#0 
        link a1,#-2
        link a2,#-100
        link a3,#-256
        link a4,#-4086
        link a5,#-8192
        link a6,#-16384
        link a7,#-32768


; ======================================================================
; UNLK An
; ======================================================================

        unlk a0
        unlk a1
        unlk a2
        unlk a3
        unlk a4
        unlk a5
        unlk a6
        unlk a7


; ----------------------------------------------------------------------
; CHK - Check register against bounds
; ----------------------------------------------------------------------
; Notes:
; - Syntax: CHK <ea>,Dn
; - Allowed size: .w only on 68000
; - Allowed <ea>: all data addressing modes except An
; - <ea> is compared against Dn; exceptions irrelevant here (no execution)
; - Absolute short is SIGNED 16-bit
; - Absolute long is full 32-bit

CHK_SECTION:

; ======================================================================
; CHK.w <ea>,Dn
; ======================================================================

        ; --- Register direct ---
        chk.w d0,d1
        chk.w d2,d3
        chk.w d4,d5
        chk.w d6,d7

        ; --- Address register indirect ---
        chk.w (a0),d0
        chk.w (a1),d1

        ; --- Postincrement ---
        chk.w (a2)+,d2
        chk.w (a3)+,d3

        ; --- Predecrement ---
        chk.w -(a4),d4
        chk.w -(a5),d5

        ; --- Displacement ---
        chk.w 8(a6),d6
        chk.w 16(a0),d7

        ; --- Indexed ---
        chk.w 12(a1,d2.w),d0
        chk.w 24(a2,d3.l),d1

        ; --- Absolute short (signed 16-bit) ---
        chk.w $0000.w,d2 ; +0
        chk.w $7FFE.w,d3 ; +32767
        chk.w $1358.w,d6 ; pseudo-random

        ; --- Absolute long (full 32-bit) ---
        chk.w $00000000.l,d7 ; boundary low
        chk.w $7FFFFFFE.l,d0 ; boundary high positive
        chk.w $80000000.l,d1 ; boundary negative
        chk.w $FFFFFFFE.l,d2 ; boundary high
        chk.w $2468ACE0.l,d3 ; pseudo-random


; ----------------------------------------------------------------------
; NBCD / SBCD - Decimal adjust and subtract BCD with extend
; ----------------------------------------------------------------------
; Notes:
; - NBCD.<size> <ea>
; Size: .b only
; Allowed <ea>: Dn or memory alterable (no An, no immediate)
; Uses X flag (not relevant here)
;
; - SBCD.<size> Dx,Dy
; Size: .b only
; Register-to-register form
;
; - SBCD.<size> -(Ax),-(Ay)
; Size: .b only
; Memory predecrement form
;
; - Absolute short is SIGNED 16-bit
; - Absolute long is full 32-bit

NBCD_SBCD_SECTION:

; ======================================================================
; NBCD.b <ea>
; ======================================================================

        ; --- NBCD on data registers ---
        nbcd d0
        nbcd d1
        nbcd d2
        nbcd d3
        nbcd d4
        nbcd d5
        nbcd d6
        nbcd d7

        ; --- NBCD on memory operands ---
        nbcd (a0)
        nbcd (a1)+
        nbcd -(a2)
        nbcd 8(a3)
        nbcd 16(a4,d5.w)

        ; --- NBCD on absolute short (signed) ---
        nbcd $0000.w
        nbcd $7FFE.w
        nbcd $1358.w

        ; --- NBCD on absolute long ---
        nbcd $00000000.l
        nbcd $7FFFFFFE.l
        nbcd $80000000.l
        nbcd $FFFFFFFE.l
        nbcd $2468ACE0.l


; ======================================================================
; SBCD.b Dx,Dy (register-to-register)
; ======================================================================

        sbcd d0,d1
        sbcd d2,d3
        sbcd d4,d5
        sbcd d6,d7
        sbcd d7,d0 ; wrap-around pairing


; ======================================================================
; SBCD.b -(Ax),-(Ay) (memory predecrement)
; ======================================================================

        sbcd -(a0),-(a1)
        sbcd -(a2),-(a3)
        sbcd -(a4),-(a5)
        sbcd -(a6),-(a7)
        sbcd -(a7),-(a0) ; wrap-around pairing

; ----------------------------------------------------------------------
; EXT / SWAP - Sign extension and word/longword swap
; ----------------------------------------------------------------------
; Notes:
; - EXT:
; EXT.w Dn : sign-extend byte -> word
; EXT.l Dn : sign-extend word -> long
; EXT.b does NOT exist on 68000.
; Operand must be a data register (Dn).
;
; - SWAP:
; SWAP Dn : swap upper/lower 16-bit words of Dn
; No size suffix.
; Operand must be a data register (Dn).
;
; - No memory operands allowed.
; - No immediate, no address registers, no addressing modes.

EXT_SWAP_SECTION:

; ======================================================================
; EXT - Sign extension
; ======================================================================

        ; --- EXT.w Dn (byte -> word) ---
        ext.w d0
        ext.w d1
        ext.w d2
        ext.w d3
        ext.w d4
        ext.w d5
        ext.w d6
        ext.w d7

        ; --- EXT.l Dn (word -> long) ---
        ext.l d0
        ext.l d1
        ext.l d2
        ext.l d3
        ext.l d4
        ext.l d5
        ext.l d6
        ext.l d7


; ======================================================================
; SWAP - Swap upper/lower 16 bits of Dn
; ======================================================================

        swap d0
        swap d1
        swap d2
        swap d3
        swap d4
        swap d5
        swap d6
        swap d7

; ----------------------------------------------------------------------
; MOVEP - Move Peripheral (Dx <-> (d16,An))
; ----------------------------------------------------------------------
; Notes:
; - Allowed sizes: .w and .l only.
; - Addressing mode: (d16,An) ONLY.
; - Displacement is a signed 16-bit word.
; - MOVEP moves bytes spaced by 2 bytes on the bus (not relevant here).

MOVEP_SECTION:

; ======================================================================
; MOVEP from register to memory: MOVEP.<size> Dn,(d16,An)
; ======================================================================

        ; --- MOVEP.w Dn,(d16,An) ---
        movep.w d0,0(a0)
        movep.w d1,32767(a1) ; boundary +32767
        movep.w d2,-32768(a2) ; boundary -32768
        movep.w d3,-1(a3) ; -1
        movep.w d4,1234(a4) ; pseudo-random

        ; --- MOVEP.l Dn,(d16,An) ---
        movep.l d5,0(a5)
        movep.l d6,32767(a6)
        movep.l d7,-32768(a0)
        movep.l d0,-1(a1)
        movep.l d1,2468(a2)


; ======================================================================
; MOVEP from memory to register: MOVEP.<size> (d16,An),Dn
; ======================================================================

        ; --- MOVEP.w (d16,An),Dn ---
        movep.w 0(a0),d0
        movep.w 32767(a1),d1
        movep.w -32768(a2),d2
        movep.w -1(a3),d3
        movep.w 1357(a4),d4

        ; --- MOVEP.l (d16,An),Dn ---
        movep.l 0(a5),d5
        movep.l 32767(a6),d6
        movep.l -32768(a7),d7
        movep.l -1(a0),d0
        movep.l 999(a1),d1

; ----------------------------------------------------------------------
; SUBX - Subtract with extend (register and predecrement forms)
; ----------------------------------------------------------------------
; Notes:
; - Two forms exist:
; 1) Register-to-register: SUBX.<size> Dx,Dy
; 2) Memory predecrement: SUBX.<size> -(Ax),-(Ay)
; - Sizes: .b, .w, .l
; - Uses the X flag (not relevant here since code is not executed).
; - Memory form requires predecrement addressing mode only.

SUBX_SECTION:

; ======================================================================
; REGISTER-TO-REGISTER FORM
; ======================================================================

        ; --- SUBX.b Dx,Dy ---
        subx.b d0,d1
        subx.b d2,d3
        subx.b d4,d5
        subx.b d6,d7
        subx.b d7,d0 ; wrap-around pairing

        ; --- SUBX.w Dx,Dy ---
        subx.w d0,d1
        subx.w d2,d3
        subx.w d4,d5
        subx.w d6,d7
        subx.w d7,d0

        ; --- SUBX.l Dx,Dy ---
        subx.l d0,d1
        subx.l d2,d3
        subx.l d4,d5
        subx.l d6,d7
        subx.l d7,d0


; ======================================================================
; MEMORY PREDECREMENT FORM
; ======================================================================
; Only -(Ax),-(Ay) is allowed.

        ; --- SUBX.b -(Ax),-(Ay) ---
        subx.b -(a0),-(a1)
        subx.b -(a2),-(a3)
        subx.b -(a4),-(a5)
        subx.b -(a6),-(a7)
        subx.b -(a7),-(a0)

        ; --- SUBX.w -(Ax),-(Ay) ---
        subx.w -(a0),-(a1)
        subx.w -(a2),-(a3)
        subx.w -(a4),-(a5)
        subx.w -(a6),-(a7)
        subx.w -(a7),-(a0)

        ; --- SUBX.l -(Ax),-(Ay) ---
        subx.l -(a0),-(a1)
        subx.l -(a2),-(a3)
        subx.l -(a4),-(a5)
        subx.l -(a6),-(a7)
        subx.l -(a7),-(a0)

; ----------------------------------------------------------------------
; ADDX - Add with extend (register and predecrement forms)
; ----------------------------------------------------------------------
; Notes:
; - Two forms exist:
; 1) Register-to-register: ADDX.<size> Dx,Dy
; 2) Memory predecrement: ADDX.<size> -(Ax),-(Ay)
; - Sizes: .b, .w, .l
; - Uses the X flag (not relevant here since code is not executed).
; - Memory form requires predecrement addressing mode only.

ADDX_SECTION:

; ======================================================================
; REGISTER-TO-REGISTER FORM
; ======================================================================

        ; --- ADDX.b Dx,Dy ---
        addx.b d0,d1
        addx.b d2,d3
        addx.b d4,d5
        addx.b d6,d7
        addx.b d7,d0 ; wrap-around register pairing

        ; --- ADDX.w Dx,Dy ---
        addx.w d0,d1
        addx.w d2,d3
        addx.w d4,d5
        addx.w d6,d7
        addx.w d7,d0

        ; --- ADDX.l Dx,Dy ---
        addx.l d0,d1
        addx.l d2,d3
        addx.l d4,d5
        addx.l d6,d7
        addx.l d7,d0


; ======================================================================
; MEMORY PREDECREMENT FORM
; ======================================================================
; Only -(Ax),-(Ay) is allowed.
; No other addressing mode is legal for ADDX memory form.

        ; --- ADDX.b -(Ax),-(Ay) ---
        addx.b -(a0),-(a1)
        addx.b -(a2),-(a3)
        addx.b -(a4),-(a5)
        addx.b -(a6),-(a7)
        addx.b -(a7),-(a0) ; wrap-around pairing

        ; --- ADDX.w -(Ax),-(Ay) ---
        addx.w -(a0),-(a1)
        addx.w -(a2),-(a3)
        addx.w -(a4),-(a5)
        addx.w -(a6),-(a7)
        addx.w -(a7),-(a0)

        ; --- ADDX.l -(Ax),-(Ay) ---
        addx.l -(a0),-(a1)
        addx.l -(a2),-(a3)
        addx.l -(a4),-(a5)
        addx.l -(a6),-(a7)
        addx.l -(a7),-(a0)


; ----------------------------------------------------------------------
; SYSTEM INSTRUCTIONS - 68000
; ----------------------------------------------------------------------
; Notes:
; - Includes: NOP, RESET, STOP, RTE, RTS, TRAP, TRAPV, ILLEGAL
; - STOP uses a 16-bit immediate (SR mask)
; - TRAP uses a 4-bit vector (0–15)
; - TRAPV has no operand
; - RTE, RESET, STOP are privileged (ignored here, no execution)
; - ILLEGAL has no operand
; - Absolute short is SIGNED 16-bit

SYSTEM_SECTION:

; ======================================================================
; NOP - No Operation
; ======================================================================

        nop
        nop
        nop


; ======================================================================
; RESET - Reset external devices (privileged)
; ======================================================================

        reset
        reset


; ======================================================================
; STOP #<sr_mask> (16-bit immediate)
; ======================================================================

        stop #$0000
        stop #$2000
        stop #$2700
        stop #$FFFF
        stop #$1234


; ======================================================================
; RTE - Return from Exception (privileged)
; ======================================================================

        rte
        rte


; ======================================================================
; RTS - Return from Subroutine
; ======================================================================

        rts
        rts


; ======================================================================
; TRAP #<vector> (0–15)
; ======================================================================

        trap #0
        trap #1
        trap #2
        trap #3
        trap #4
        trap #5
        trap #6
        trap #7
        trap #8
        trap #9
        trap #10
        trap #11
        trap #12
        trap #13
        trap #14
        trap #15


; ======================================================================
; TRAPV - Trap on Overflow
; ======================================================================

        trapv
        trapv


; ======================================================================
; ILLEGAL - Illegal Instruction
; ======================================================================

        illegal
        illegal


; ======================================================================
; Misc small instructions
; ======================================================================

MISC_SMALL_SECTION:

        ; MOVE from SR/CCR already covered

END_OF_TEST:
        nop

