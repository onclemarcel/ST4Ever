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
        move.w  #$4B,-(sp)
        trap    #1
        add.w   #16,sp
        
        clr.w   -(sp)         ; Pterm0
        trap    #1

        section data

hello:
        dc.b "Hello!", 0

prog:
        dc.b "C:\GEN.TTP",0
        even
        
cmd:
        dc.b 11,"C:\SOURCE.S",13
		even
        
        section bss
