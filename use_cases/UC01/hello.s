;--------------------------------------------------------------------
; hello.s - Minimal Atari ST DEVPAC3 test program
;
; This is the reference test binary for ST4Ever UC01 and onwards.
; It loads the value 42 into D0 and returns to TOS via RTS.
;
; Assemble with DEVPAC3 on Atari ST:
;   GEN.TTP hello.s hello.prg
;
; Expected binary (.text section = 4 bytes):
;   70 2A   MOVEQ  #42,D0
;   4E 75   RTS
;
; Expected PRG header (28 bytes):
;   60 1A          magic
;   00 00 00 04    text_size  = 4
;   00 00 00 00    data_size  = 0
;   00 00 00 00    bss_size   = 0
;   00 00 00 00    sym_size   = 0
;   00 00 00 00    reserved
;   00 00 00 00    flags
;   00 00          abs_flag   = 0 (relocatable)
;--------------------------------------------------------------------

        SECTION TEXT

start:
        MOVEQ   #42,D0          ; Load return code 42 into D0
        RTS                     ; Return to TOS / caller

        SECTION DATA

        SECTION BSS

        END
