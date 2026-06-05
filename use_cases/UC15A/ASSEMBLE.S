;
; ASSEMBLE.S
;
; Source file for batch assembling all source files found in ./SRC folder
; and copy all produced *.PRG in ./PRG folder
;
        section text

        bra _init

; ----------------------------
; STRINGS MANAGEMENT FUNCTIONS
; ----------------------------
; void Cconout(char c) - GEMDOS Trap #1 - 0x02
;
; Desc: outputs one character via GEMDOS handle 1 (normally ‘con:’)
; In:   D3
; Out:  void
;
Cconout:
        move.w D3,-(sp) 		; Arg1  :   char to print
        move.w #2,-(sp) 	    ; Opcode: GEMDOS #2: Cconout
        trap #1					; gemdos(0x02, c);
        addq.l #4,sp 			; Stack cleanup (w + w = 4)
        rts						;

; void printf(char* s)
;
; Desc: Loop on A0 to output each character until detection of NULL (\0)
; In:   A3 
; Out:  void
;
printf:
        move.l D3,-(SP)			; Save D3 (used in this function)
		clr.l D3				; char c = 0;
printf_next:					; do {
        move.b (A3)+,D3			; 	c = *s++;
        or.b D3,D3			    ; 	ASSERT(c == 0)
        beq printf_end			; 	if (c == 0) { return; }
        bsr Cconout				; 	else { Cconout(c); }
        bra printf_next			; }			
printf_end:
		move.l (SP)+,D3			; Restore D3
        rts						;

; void print_nibble(int i)
;
; Desc: Print a single hex digit (4 bits) on screen
; In:   D4: digit is in bits 0-4
; Out:  void
;
print_nibble:
        move.l D3,-(SP)			; Save D3 (used in this function)
		move.b D4,D3			; char c = (char)i
        and.b #$0F,D3			; c = c & 0x0F;		/* Keep nibble only */
        cmp.b #$0A,D3			; if (c < 10)
        blo is_number			; 	{ goto is_number; } 
        add.b #$37,D3			; else { c = c + 0x37; } /* Start from 'A' */
        bra nibble_out
is_number:
        add.b #$30,D3			; is_number: c = c + 0x30; /* Start from '0' */
nibble_out:
        bsr Cconout
		move.l (SP)+,D3			; Restore D3
        rts						;

; void print_word(int i)
;
; Desc: Print a word on screen - process byte per byte
; In:   D4: word is in D4
; Out:  void
;
print_word:						
        ror.w #8,D4				; i = (i << 24) | (i >> 8); /* Rotate to the right */
        bsr print_byte			; print_byte(i);
        rol.w #8,D4				; i = (i >> 24) | (i << 8); /* Rotate to the left */
        bsr print_byte			; print_byte(t);
        rts

; void print_byte(int i)
;
; Desc: Print a byte on screen - process nibble by nibble
; In:   D4: byte is in D4
; Out:  void
;
print_byte:
        ror.b #4,D4				; i = (i << 28) | (i >> 4); /* Rotate to the right */
        bsr print_nibble		; print_nibble(i);
        rol.b #4,D4				; i = (i >> 28) | (i << 4); /* Rotate to the left */
        bsr print_nibble		; print_nibble(i);
        rts

; void print_hex(int i)
;
; Desc: Print a long (32-bits) on screen - process word by word
; In:   D4: long value is in D4
; Out:  void
;
print_hex:
        move.l D3,-(SP)			; Save D3 (used in this function)
        move.l #$10,D3			; int val = 16;
        ror.l D3,D4				; i = (i << 16) | (i >> 16); /* Rotate one word to the right */
        bsr print_word			; print_word(i);
        rol.l D3,D4				; i = (i << 16) | (i >> 16); /* Rotate one word to the left */
        bsr print_word			; print_word(i);
		move.l (SP)+,D3			; Restore D3
        rts

; void print_nline()
;
; Desc: Print a new line - equals printf('\n');
; In:   void
; Out:  void
;
print_nline:
		move.b #10,D3		; c = 0x0A /* Line Feed */
		bsr Cconout			; Cconout(c);
		move.b #13,D3		; c = 0x0D /* Carriage Return */
		bsr Cconout			; Cconout(c);
		rts

; ----------------------------
; FILE RELATED FUNCTION
; ----------------------------
; void fopen(int mode, char* filename) - GEMDOS Trap #1 - 0x3D
;
; Desc: open a file and return a handle to the file
; In:   D3 (mode), A3 (filename)
; Out:  D0
;
fopen:
		move.l D5,-(sp)		; int ret;
		move.l D4,-(sp)		; int err;
		
		move.w D3,-(sp) 	; Arg2  :   mode
        move.l A3,-(sp)		; Arg1  :   filename 
        move.w #$3D,-(sp) 	; Opcode: GEMDOS #61: Fopen
        trap #1				; gemdos(0x3D, filename, mode);
        addq.l #8,sp 		; Stack clean-up  (w + w + l = 8)

		move.l D0,D5		; ret = fopen(filename,mode); 
        cmp.l #0,D5			; if (ret >= 0)
        bge fopen_OK   		; 	{ goto OK; }
		lea fopen_err,A3    ; else /* Get GEMDOS message */
		bsr printf			; { printf(s);
		move.l D5,D4		;   err = ret;
		bsr print_hex		;   print_hex(err); 
		bsr print_nline		;   print("\n");
		move.l D5,D0		; Set return value;

fopen_OK:
		move.l (sp)+,D4		; Restore D4
		move.l (sp)+,D5		; Restore D5
		rts

; void fclose(int handle) - GEMDOS Trap #1 - 0x3E
;
; Desc: Close a file handle
; In:   D3 (handle)
; Out:  void
;
fclose:
		move.w D3,-(sp) 	; Arg1: file handle
        move.w #$3E,-(sp)   ; Opcode: GEMDOS #62: Fclose
        trap #1				; gemdos(0x3E, handle);
        addq.l #4,sp 		; Stack clean-up  (w + w = 4)
		rts

; ----------------------------
; INIT & MAIN FUNCTIONS
; ----------------------------
; _init()
;
; Desc: Initialize the application stack, Shrink Memory to TPA size
; In:   none
; Out:  none
;
_init:
		; Calculate application needed Memory (TPA size)
		; TPA_size = (BSS_Addr + BSS_size + Appl_Stack) - Basepage_Address
		move.l 4(sp),A0 		; Obtain pointer to basepage
		move.l A0,basepage 		; Save a copy in bss
		move.l $18(A0),A1 		; BSS Base address
		adda.l $1C(A0),A1 		; Add BSS size
		adda.l #$1000,A1 		; Add stack size (4K)
		move.l A1,sp 			; SP = new stack pointer
		suba.l basepage,A1 		; A1 = TPA_size
		
		; Give back the rest of the memory to the system
		; call GEMDOS Mshrink(start, size);
		move.l a1,-(sp)			; Arg2: TPA_Size needed only
		move.l basepage,-(sp)	; Arg1: Basepage address
		clr.w -(sp)				; reserved
		move.w #$4a,-(sp) 		; Opcode: GEMDOS #74: Mshrink
		trap #1					; gemdos(0x4A, Basepage, TPA_size); 
		lea 12(sp),sp 			; Stack clean-up  (l + L + w + w = 12)

; int main()
;
; Desc: main program flow - Check if GEN.TTP and SOURCE.S exists
; In:   none
; Out:  'ret' provided to Pterm() function
;
main:
        ; clear the screen using VT-52 emulator
		lea esc_clear,A3	; char* s = esc_clear;	/* VT-52 emulator: ESC-E */
        bsr printf 			; printf(s);
        lea intro,A3		; char* s = intro;		/* Welcome message */
        bsr printf 			; printf(s);

        ; Show check message for GEN.TTP
        lea checkmsg,A3		; char* s = checkmsg;	/* Checking */
        bsr printf			; printf(s);
        lea prog,A3			; char* s = prog;		/* Program to launch */
        bsr printf			; printf(s);
        lea newline,A3		; printf("\n");
        bsr printf			;

		; Try opening "GEN.TTP" (read-only)
		move #0,D3			; int mode = 0;     /* read-only */
		lea prog,A3			; char* s = prog;   /* program to launch*/
		bsr fopen
		
        ; D0 holds handle (>=0) or GEMDOS error (<0)
		move.l D0,D3		; int ret = fopen(s, mode);
        cmp.l #0,D3			; if (ret >= 0)
        bge GEN_OK   		; 	{ goto OK; }
		lea abort,A3		; else  /* Abort */
		bsr printf			; { printf(s) }
		bra wait_space		; goto wait_space & exit

GEN_OK:
		; File is found close its handle
		lea file_found,A3
		bsr printf		
        bsr fclose
		
        ; Show check message for SOURCE.S
        lea newline,A3		; printf("\n");
        bsr printf			;
		lea checkmsg,A3		; char* s = checkmsg;	/* Checking */
        bsr printf			; printf(s);
        lea source,A3		; char* s = source;		/* Source to assemble */
        bsr printf			; printf(s);
        lea newline,A3		; printf("\n");
        bsr printf			;

		; Try opening "SOURCE.S" (read-only)
		move #0,D3			; int mode = 0;     /* read-only */
		lea source,A3		; char* s = source; /* source to assemble */
		bsr fopen
		
        ; D0 holds handle (>=0) or GEMDOS error (<0)
		move.l D0,D3		; int ret = fopen(s, mode);
        cmp.l #0,D3			; if (ret >= 0)
        bge SOURCE_OK   	; 	{ goto OK; }
		lea abort,A3		; else  /* Abort */
		bsr printf			; { printf(s) }
		bra wait_space		; goto wait_space & exit

SOURCE_OK:
		; File is found close its handle
		lea file_found,A3
		bsr printf		
        bsr fclose
		        
GEN_EXE:
		; Calling Pexec(0, "GEN.TTP", "SOURCE.S", NULL)
		lea newline,A3		; printf("\n");
        bsr printf			;
		lea callingmsg,A3   ; char* s = callingmsg; /* Call message */
		bsr printf			; printf(s);

        clr.l -(sp) 		; Arg4: env = NULL (long)
        pea cmd				; Arg3: cmd line = 8,SOURCE.S,0
        pea prog			; Arg2: Program name = GEN.TTP
        clr.w -(sp) 		; Arg1: mode = 0 (PE_LOADGO)
        move.w #$4B,-(sp) 	; Opcode: GEMDOS #75: Pexec
        trap #1				; gemdos(0x4B, mode, prog, cmd, env); 
        add.l #16,sp 		; Stack clean-up  (l + l + L + w + w = 16)

		move.l D0,D3		; int ret = Pexec(0, "GEN.TTP", "SOURCE.S", NULL)
        lea retmsg,A3		; char* s = retmsg;
        bsr printf			; printf(s);
        move.l D3,D4		; code = ret;
        bsr print_hex		; print_hex(code)
        bsr print_nline		; printf("\n")

;        bra bye

;file_not_found:
;        lea notfoundmsg,a0
;        bsr printf
;        lea newline,a0
;        bsr printf

;bye:
;        lea bye_msg,a0
;        bsr printf

        ; attente d'appui sur espace pour quitter (optionnel)
wait_space:
        move.w #7,-(sp)
        trap #1
        addq.l #2,sp
        cmp.b #$20,D0
        beq exit
        bra wait_space

exit:
        clr.w -(sp) ; Pterm0
        trap #1

; -----------------------
; Données
; -----------------------
        section data

esc_clear:
		; ESC+E
        dc.b  27, 69, 0
		even
intro:
        dc.b "ASSEMBLE.PRG - Lancement GEN.TTP", 10, 13, 10, 0
		even
newline:
        dc.b 10, 13, 0
		even
checkmsg:
        dc.b "Checking ",0
		even
foundmsg:
        dc.b "Found: ",0
		even
callingmsg:
        dc.b "Calling Pexec()", 10, 13, 0
		even
retmsg:
        dc.b "Return code (D0) = 0x",0
		even
bye_msg:
        dc.b "Bye.",10, 13, 0
		even

hello:
        dc.b "Hello!", 0
		even
hexbuf:
        ds.b 16
		even
prog:
        dc.b "GEN.TTP", 0
        even

source:
		dc.b "SOURCE.S", 0
		even
		
cmd:
        dc.b 8, "SOURCE.S", 0
        even

fopen_err:
        dc.b "fopen() error code : 0x", 0
        even

file_found:
		dc.b "OK - file found", 10, 13, 0
		even
		
abort:
		dc.b "Aborting...", 10, 13, 0
		even
		
        section bss

basepage: 
		ds.l 1