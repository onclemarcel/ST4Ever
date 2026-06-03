        section text

        bra     start

putc:
        movem.l d0/a0,-(sp)   ; sauver D0 et A0 (GEMDOS les détruit)
        move.w  d0,-(sp)      ; paramètre : caractère
        move.w  #2,-(sp)      ; fonction : Cconout
        trap    #1
        addq.l  #4,sp         ; enlever les 2 mots poussés
        movem.l (sp)+,d0/a0   ; restaurer D0 et A0
        rts

print_str:
        clr.l   d0      
print_loop:
        move.b  (A0)+,D0    
        or.b    D0,D0
        beq     print_str_end   
        bsr     putc
        bra     print_loop      
print_str_end:
        rts

print_D1_nibble:
	move.b D1,D0
	and.b #$0F,D0
	cmp.b #$0A,D0
	blo value_number
	add.b #$37,D0	
	bra print_nibble
value_number:
	add.b #$30,D0
print_nibble:
	bsr putc
	rts

print_word:
	ror.w #8,D1
	bsr print_byte
	rol.w #8,D1
	bsr print_byte
	rts

print_byte:
	ror.b #4,D1
	bsr print_D1_nibble
	rol.b #4,D1
	bsr print_D1_nibble
	rts

print_hex:
	move.l #$10,D2	
	ror.l D2,D1		
	bsr print_word
	rol.l D2,D1		
	bsr print_word
	rts


start:
        lea     hello,a0    
        bsr     print_str           

again:	
		move.w #7,-(sp)		
		trap #1				
		addq.l #2,sp
		cmp.b #$20,D0		
		beq cont			
		bra again		
cont:
        clr.l   -(sp)
        pea     cmd
        pea     prog
        clr.w   -(sp)
        move.w  #$4B,-(SP)
        trap    #1
        add.w   #14,sp
		
		move.l  D0,D1
		bsr     print_hex
		addq.l  #4,sp
        
stop:	
		move.w #7,-(sp)		
		trap #1				
		addq.l #2,sp
		cmp.b #$20,D0		
		beq exit			
		bra stop		
exit:
        clr.w   -(sp)         ; Pterm0
        trap    #1

        section data

hello:
        dc.b "Hello!", 0
		
hexbuf:
	    ds.b 16

prog:
        dc.b "GEN.TTP",0
        even
        
cmd:
        dc.b 9," SOURCE.S",13
		even
        
        section bss
