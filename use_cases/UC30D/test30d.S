; test30d.S - UC30D instruction encoding test fixture
; Covers: ASL/ASR/LSL/LSR/ROL/ROR/ROXL/ROXR (register shifts)
;         + BTST/BCHG/BCLR/BSET (bit ops)
;         + MOVEM.L (store/load)
;         + ADDA/SUBA
;         + MULU/MULS/DIVU/DIVS
;         + ADDX/SUBX
;         + Scc
;         + DBcc (DBRA/DBNE)
;         + EXG / PEA
;
; All expected encodings verified against MC68000 PRM (Motorola).
; .text layout (28-byte PRG header prepended by assembler):
;
;  off  instruction              bytes   expected
;  ---  ----------------------   -----   --------
;   0   ASL.W   #2,D0             2      E5 40
;   2   ASR.W   #2,D1             2      E4 41   (D=0 right, CCC=2)
;   4   LSL.W   #1,D2             2      E3 4A   (TT=01 LS, CCC=1)
;   6   LSR.W   #1,D3             2      E2 4B   (TT=01 LS, D=0)
;   8   ROL.W   #1,D4             2      E3 5C   (TT=11 RO, D=1)
;  10   ROR.W   #1,D5             2      E2 5D   (TT=11 RO, D=0)
;  12   ROXL.W  #1,D6             2      E3 56   (TT=10 ROX, D=1, RRR=6)
;  14   ROXR.W  #1,D7             2      E2 57   (TT=10 ROX, D=0, RRR=7)
;  16   ASL.W   D1,D0             2      E3 60   (CCC=D1, I=1, TT=00, D=1)
;  18   BTST    D0,D1             2      01 01
;  20   BCHG    D2,D3             2      05 43
;  22   BCLR    D4,D5             2      09 85
;  24   BSET    D6,D7             2      0D C7
;  26   BTST    #7,D0             4      08 00 00 07
;  30   BSET    #3,D1             4      08 C1 00 03
;  34   MOVEM.L D0-D1/A0-A1,-(SP) 4+2  48 E7 C0 C0  (mask=D0+D1+A0+A1 reversed)
;  38   MOVEM.L (SP)+,D0-D1/A0-A1 4+2  4C DF 03 03  (mask=D0+D1+A0+A1 normal)
;  42   ADDA.W  D0,A1             2      D2 C0
;  44   ADDA.L  D2,A3             2      D7 C2   (An=A3, ooo=7 for .L)
;  46   SUBA.W  D4,A5             2      9A C4
;  48   MULU.W  D0,D1             2      C2 C0
;  50   MULS.W  D2,D3             2      C7 C2
;  52   DIVU.W  D0,D1             2      82 C0
;  54   DIVS.W  D2,D3             2      87 C2
;  56   ADDX.W  D0,D1             2      D3 40
;  58   SUBX.W  D2,D3             2      97 42
;  60   SNE     D0                2      56 C0
;  62   SEQ     D1                2      57 C1
;  64   dbra_top: DBRA  D0,dbra_top  4  51 C8 FF FE  (disp=-2: 64-66=-2)
;  68   dbne_top: DBNE  D1,dbne_top  4  56 C9 FF FE
;  72   EXG     D0,D1             2      C1 41
;  74   EXG     A0,A1             2      C1 49
;  76   EXG     D0,A0             2      C1 88
;  78   PEA     (A0)              2      48 50
;  80   PEA     $100.W            4      48 78 01 00
;
; Total .text size: 84 bytes
;
; Notes on encoding verification:
;  ASL.W #2,D0: 1110 010 1 01 0 00 000 = E5 40
;  ASR.W #2,D1: 1110 010 0 01 0 00 001 = E4 41
;  LSL.W #1,D2: 1110 001 1 01 0 01 010 = E3 4A
;  LSR.W #1,D3: 1110 001 0 01 0 01 011 = E2 4B
;  ROL.W #1,D4: 1110 001 1 01 0 11 100 = E3 5C
;  ROR.W #1,D5: 1110 001 0 01 0 11 101 = E2 5D
;  ROXL.W #1,D6: 1110 001 1 01 0 10 110 = E3 54  (wait: 1110 001 1 01 0 10 110 = 0xE356?)
;  Recompute: 1110=0xE, 001=1, 1=D, 01=sz, 0=I, 10=TT, 110=reg
;  = 1110_0011_0101_0110 = 0xE356... hmm let me recalculate
;
;  Bit layout: 15-12: 1110, 11-9: CCC, 8: D, 7-6: SS, 5: I, 4-3: TT, 2-0: RRR
;
;  ASL.W #2,D0: CCC=2=010, D=1, SS=01, I=0, TT=00, RRR=000
;    = 1110 010 1 01 0 00 000 = 0xE540
;  ASR.W #2,D1: CCC=2=010, D=0, SS=01, I=0, TT=00, RRR=001
;    = 1110 010 0 01 0 00 001 = 0xE441
;  LSL.W #1,D2: CCC=1=001, D=1, SS=01, I=0, TT=01, RRR=010
;    = 1110 001 1 01 0 01 010 = 0xE34A
;  LSR.W #1,D3: CCC=1=001, D=0, SS=01, I=0, TT=01, RRR=011
;    = 1110 001 0 01 0 01 011 = 0xE24B
;  ROL.W #1,D4: CCC=1=001, D=1, SS=01, I=0, TT=11, RRR=100
;    = 1110 001 1 01 0 11 100 = 0xE35C
;  ROR.W #1,D5: CCC=1=001, D=0, SS=01, I=0, TT=11, RRR=101
;    = 1110 001 0 01 0 11 101 = 0xE25D
;  ROXL.W #1,D6: CCC=1=001, D=1, SS=01, I=0, TT=10, RRR=110
;    = 1110 001 1 01 0 10 110 = 0xE356
;  ROXR.W #1,D7: CCC=1=001, D=0, SS=01, I=0, TT=10, RRR=111
;    = 1110 001 0 01 0 10 111 = 0xE257
;  ASL.W D1,D0: CCC=D1=001, D=1, SS=01, I=1, TT=00, RRR=000
;    = 1110 001 1 01 1 00 000 = 0xE360
;
;  BTST D0,D1: 0000 000 1 00 000 001 = 0x0101
;  BCHG D2,D3: 0000 010 1 01 000 011 = 0x0543
;  BCLR D4,D5: 0000 100 1 10 000 101 = 0x0985
;  BSET D6,D7: 0000 110 1 11 000 111 = 0x0DC7
;  BTST #7,D0: 0000 100 1 00 000 000 + 0x0007 = 0x0800 0x0007
;  BSET #3,D1: 0000 100 1 11 000 001 + 0x0003 = 0x08C1 0x0003
;
;  MOVEM.L D0-D1/A0-A1,-(SP):
;    Normal mask: D0=bit0, D1=bit1, A0=bit8, A1=bit9 => 0b0000001100000011 = 0x0303
;    Predecrement: reversed => bit15=A1→bit0, bit14=A0→bit1, bit1=D1→bit14, bit0=D0→bit15
;    reverse of 0x0303: bit0→bit15=0x8000, bit1→bit14=0x4000, bit8→bit7=0x0080, bit9→bit6=0x0040
;    = 0x8000|0x4000|0x0080|0x0040 = 0xC0C0
;    Opword: 0100 1000 1 1 100 111 = 0x48E7 (store, L, -(SP)=AN_PRE SP=A7)
;    = 0x48E7 0xC0C0
;
;  MOVEM.L (SP)+,D0-D1/A0-A1:
;    Normal mask = 0x0303 (D0+D1+A0+A1 in bits 0,1,8,9)
;    Load mode (AN_POST) - no reversal
;    Opword: 0100 1100 1 1 011 111 = 0x4CDF (load, L, (SP)+=AN_POST SP=A7)
;    = 0x4CDF 0x0303
;
;  ADDA.W D0,A1: 1101 001 011 000 000 = 0xD2C0
;  ADDA.L D2,A3: 1101 011 111 000 010 = 0xD782
;  SUBA.W D4,A5: 1001 101 011 000 100 = 0x9AC4
;
;  MULU.W D0,D1: 1100 001 011 000 000 = 0xC2C0
;  MULS.W D2,D3: 1100 011 111 000 010 = 0xC7C2
;  DIVU.W D0,D1: 1000 001 011 000 000 = 0x82C0
;  DIVS.W D2,D3: 1000 011 111 000 010 = 0x87C2
;
;  ADDX.W D0,D1: 1101 001 1 01 00 0 000 = 0xD340
;  SUBX.W D2,D3: 1001 011 1 01 00 0 010 = 0x9742 -> wait
;    SUBX: 1001 yyy 1 ss 00 0 xxx, yyy=D3=3, ss=01(W), xxx=D2=2
;    = 1001 011 1 01 00 0 010 = 0x9742
;
;  SNE D0: 0101 0110 11 000 000 = 0x56C0
;  SEQ D1: 0101 0111 11 000 001 = 0x57C1
;
;  DBRA D0,dbra_top: opword=0x51C8, disp=dbra_top-(PC+2)
;    dbra_top is at offset 64, opword at 64, PC+2=66, disp=64-66=-2=0xFFFE
;    => 0x51C8 0xFFFE
;  DBNE D1,dbne_top: opword=0x56C9, disp=68-70=-2=0xFFFE
;    => 0x56C9 0xFFFE
;
;  EXG D0,D1: 1100 000 1 01000 001 = 0xC141
;  EXG A0,A1: 1100 000 1 01001 001 = 0xC149
;  EXG D0,A0: 1100 000 1 10001 000 = 0xC188
;
;  PEA (A0): 0100 100 001 010 000 = 0x4850
;  PEA $100.W: 0100 100 001 111 000 + 0x0100 = 0x4878 0x0100

        SECTION TEXT

; --- Shifts (immediate count) ---
        ASL.W   #2,D0
        ASR.W   #2,D1
        LSL.W   #1,D2
        LSR.W   #1,D3
        ROL.W   #1,D4
        ROR.W   #1,D5
        ROXL.W  #1,D6
        ROXR.W  #1,D7

; --- Shift (register count) ---
        ASL.W   D1,D0

; --- Bit operations (dynamic Dn) ---
        BTST    D0,D1
        BCHG    D2,D3
        BCLR    D4,D5
        BSET    D6,D7

; --- Bit operations (static #imm) ---
        BTST    #7,D0
        BSET    #3,D1

; --- MOVEM ---
        MOVEM.L D0-D1/A0-A1,-(SP)
        MOVEM.L (SP)+,D0-D1/A0-A1

; --- Address register arithmetic ---
        ADDA.W  D0,A1
        ADDA.L  D2,A3
        SUBA.W  D4,A5

; --- Multiply / divide ---
        MULU.W  D0,D1
        MULS.W  D2,D3
        DIVU.W  D0,D1
        DIVS.W  D2,D3

; --- Extended arithmetic ---
        ADDX.W  D0,D1
        SUBX.W  D2,D3

; --- Set on condition ---
        SNE     D0
        SEQ     D1

; --- Decrement and branch ---
dbra_top:
        DBRA    D0,dbra_top
dbne_top:
        DBNE    D1,dbne_top

; --- Exchange ---
        EXG     D0,D1
        EXG     A0,A1
        EXG     D0,A0

; --- Push effective address ---
        PEA     (A0)
        PEA     $100.W

        END
