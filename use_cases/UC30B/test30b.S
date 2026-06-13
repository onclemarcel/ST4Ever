; test30b.S - UC30B DEVPAC3 instruction encoding test fixture
; Covers: MOVEQ / MOVE.B/W/L / MOVEA / CLR / SWAP / LEA
;
; Expected encodings (verified against MC68000 PRM):
;   MOVEQ  #0,D0          0x7000              offset  0
;   MOVEQ  #42,D3         0x762A              offset  2
;   MOVEQ  #-1,D7         0x7EFF              offset  4
;   MOVE.W D0,D1          0x3200              offset  6
;   MOVE.W D7,D6          0x3C07              offset  8
;   MOVE.W A2,D1          0x320A              offset 10
;   MOVE.L D2,D3          0x2602              offset 12
;   MOVE.L A1,D3          0x2609              offset 14
;   MOVE.B D4,D5          0x1A04              offset 16
;   MOVE.W #$1234,D0      0x303C 0x1234       offset 18
;   MOVE.W (A0),D1        0x3210              offset 22
;   MOVE.W D2,(A1)+       0x32C2              offset 24
;   MOVE.W D0,$1000.W     0x31C0 0x1000       offset 26
;   MOVE.W $2000.W,D1     0x3238 0x2000       offset 30
;   MOVEA.W D0,A1         0x3240              offset 34
;   MOVEA.L D0,A2         0x2440              offset 36
;   CLR.W  D0             0x4240              offset 38
;   CLR.L  D1             0x4281              offset 40
;   CLR.B  D2             0x4202              offset 42
;   SWAP   D0             0x4840              offset 44
;   SWAP   D3             0x4843              offset 46
;   LEA    $1234.W,A0     0x41F8 0x1234       offset 48
;   LEA    $12345678.L,A1 0x43F9 0x1234 0x5678 offset 52
;
; Total .text size: 58 bytes, no .data, no .bss

        SECTION TEXT

start:
        MOVEQ   #0,D0
        MOVEQ   #42,D3
        MOVEQ   #-1,D7

        MOVE.W  D0,D1
        MOVE.W  D7,D6
        MOVE.W  A2,D1
        MOVE.L  D2,D3
        MOVE.L  A1,D3
        MOVE.B  D4,D5

        MOVE.W  #$1234,D0

        MOVE.W  (A0),D1
        MOVE.W  D2,(A1)+

        MOVE.W  D0,$1000.W
        MOVE.W  $2000.W,D1

        MOVEA.W D0,A1
        MOVEA.L D0,A2

        CLR.W   D0
        CLR.L   D1
        CLR.B   D2

        SWAP    D0
        SWAP    D3

        LEA     $1234.W,A0
        LEA     $12345678.L,A1

        END
