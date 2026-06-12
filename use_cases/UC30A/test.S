; test.S - DEVPAC3 data-only source for UC30A assembler validation
; Exercises: SECTION, DC.B/W/L, DS.B/W/L, EVEN, EQU, labels.

; ---- Symbolic constants ----
MAGIC   EQU     $601A
COUNT   EQU     8

        SECTION TEXT

; Label at offset 0 in .text
text_start:
        DC.W    MAGIC           ; 2 bytes: $60,$1A
        DC.W    $0000           ; 2 bytes: $00,$00
; Four bytes, total text so far: 4

        DC.L    $DEADBEEF       ; 4 bytes (absolute, no fixup)
; total: 8

        DC.B    'H','e','l','l' ; 4 bytes characters
        DC.B    'o',0           ; 2 bytes: 'o' + NUL
        EVEN                    ; pad to word boundary (total must be even)
; 8+4+2+2(pad) = 16? let's check: 8+6=14, EVEN makes 14->14 (already even)
; Actually 8+4+2=14, which is even, so EVEN is a no-op here
; But let us add a B to make it odd:
        DC.B    $FF             ; 1 byte -> total 15
        EVEN                    ; pad -> total 16

        DS.W    COUNT           ; reserve 8 words = 16 bytes -> total 32
; text_end - text_start = 32

text_end:

        SECTION DATA

data_start:
        DC.W    $1234,$5678,$9ABC       ; 6 bytes
        DC.L    $00112233               ; 4 bytes absolute (no fixup)
        DS.B    4                       ; 4 bytes zeros
; total data: 14

        SECTION BSS

bss_buf:
        DS.B    64              ; 64 bytes BSS (not emitted)

        END
