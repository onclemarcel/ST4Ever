; test30c.S - UC30C instruction encoding test fixture
; Covers: ADD/SUB/AND/OR/CMP/EOR + Imm ALU + ADDQ/SUBQ
;         + NEG/NOT/TST/EXT + NOP/RTS/RTR/RTE/STOP/TRAP
;         + JMP/JSR/LINK/UNLK + BRA/BSR/Bcc(14)
;
; All expected encodings verified against MC68000 PRM (Motorola).
; .text layout (28-byte PRG header added by assembler):
;
;  off  instruction               bytes   expected
;  ---  -----------------------   -----   --------
;   0   ADD.W   D0,D1              2      D2 40
;   2   SUB.L   D2,D3              2      96 82
;   4   AND.W   D4,D5              2      CA 44
;   6   OR.W    D0,D1              2      82 40
;   8   CMP.W   D0,D1              2      B2 40
;  10   ADD.W   D0,(A1)            2      D1 51    Dn->EA direction
;  12   EOR.W   D0,D1              2      B1 41
;  14   ADDI.W  #$1234,D0          4      06 40 12 34
;  18   SUBI.W  #$10,D1            4      04 41 00 10
;  22   CMPI.L  #$12345678,D0      6      0C 80 12 34 56 78
;  28   ANDI.B  #$FF,D0            4      02 00 00 FF
;  32   ORI.W   #$80,D0            4      00 40 00 80
;  36   EORI.W  #$FFFF,D0          4      0A 40 FF FF
;  40   ADDQ.W  #4,D0              2      58 40
;  42   ADDQ.W  #8,D0              2      50 40    data=0 encodes 8
;  44   SUBQ.W  #2,D0              2      55 40
;  46   NEG.W   D0                 2      44 40
;  48   NOT.W   D0                 2      46 40
;  50   TST.W   D0                 2      4A 40
;  52   EXT.W   D0                 2      48 80
;  54   EXT.L   D0                 2      48 C0
;  56   NOP                        2      4E 71
;  58   RTS                        2      4E 75
;  60   RTR                        2      4E 77
;  62   RTE                        2      4E 73
;  64   STOP    #$2000             4      4E 72 20 00
;  68   TRAP    #1                 2      4E 41
;  70   TRAP    #14                2      4E 4E
;  72   JMP     (A0)               2      4E D0
;  74   JSR     (A0)               2      4E 90
;  76   LINK    A1,#-8             4      4E 51 FF F8
;  80   UNLK    A1                 2      4E 59
;  82   BRA     bra_tgt (long)     4      60 00 00 02  (disp=86-84=2)
;  86   bra_tgt: NOP               2      4E 71
;  88   BSR     bsr_tgt (long)     4      61 00 00 02  (disp=92-90=2)
;  92   bsr_tgt: NOP               2      4E 71
;  94   BEQ     beq_tgt (long)     4      67 00 00 02
;  98   beq_tgt: NOP               2      4E 71
; 100   BNE     bne_tgt (long)     4      66 00 00 02
; 104   bne_tgt: NOP               2      4E 71
; 106   bwd_tgt: NOP               2      4E 71
; 108   BNE     bwd_tgt (long)     4      66 00 FF FC  (disp=-4=0xFFFC)
;
; Total .text size: 112 bytes

        SECTION TEXT

; --- ALU binary: EA,Dn direction ---
        ADD.W   D0,D1
        SUB.L   D2,D3
        AND.W   D4,D5
        OR.W    D0,D1
        CMP.W   D0,D1

; --- ALU binary: Dn,EA direction ---
        ADD.W   D0,(A1)

; --- EOR (always Dn->EA) ---
        EOR.W   D0,D1

; --- Immediate ALU ---
        ADDI.W  #$1234,D0
        SUBI.W  #$10,D1
        CMPI.L  #$12345678,D0
        ANDI.B  #$FF,D0
        ORI.W   #$80,D0
        EORI.W  #$FFFF,D0

; --- Quick ---
        ADDQ.W  #4,D0
        ADDQ.W  #8,D0
        SUBQ.W  #2,D0

; --- Unary ---
        NEG.W   D0
        NOT.W   D0
        TST.W   D0
        EXT.W   D0
        EXT.L   D0

; --- Implied (single-word) ---
        NOP
        RTS
        RTR
        RTE

; --- STOP / TRAP ---
        STOP    #$2000
        TRAP    #1
        TRAP    #14

; --- JMP / JSR ---
        JMP     (A0)
        JSR     (A0)

; --- LINK / UNLK ---
        LINK    A1,#-8
        UNLK    A1

; --- Forward branches (long form) ---
bra_inst:
        BRA     bra_tgt
bra_tgt:
        NOP

bsr_inst:
        BSR     bsr_tgt
bsr_tgt:
        NOP

beq_inst:
        BEQ     beq_tgt
beq_tgt:
        NOP

bne_inst:
        BNE     bne_tgt
bne_tgt:
        NOP

; --- Backward branch (negative displacement) ---
bwd_tgt:
        NOP
        BNE     bwd_tgt

        END
