; test30c_bcc.S - UC30C: all 14 Bcc conditions + BHS/BLO aliases
; Each block: label at offset N, NOP at N, Bcc at N+2 -> backward
; displacement = N-(N+2+2) = -4 = 0xFFFC for all.
; Only the condition byte differs.
;
;  off  mnemonic  opword   disp
;   0   bhi_t NOP           4E 71
;   2   BHI bhi_t           62 00 FF FC   cc=0x02
;   6   bls_t NOP           4E 71
;   8   BLS bls_t           63 00 FF FC   cc=0x03
;  12   bcc_t NOP           4E 71
;  14   BCC bcc_t           64 00 FF FC   cc=0x04
;  18   bhs_t NOP           4E 71
;  20   BHS bhs_t           64 00 FF FC   cc=0x04 (BHS=BCC)
;  24   bcs_t NOP           4E 71
;  26   BCS bcs_t           65 00 FF FC   cc=0x05
;  30   blo_t NOP           4E 71
;  32   BLO blo_t           65 00 FF FC   cc=0x05 (BLO=BCS)
;  36   bvc_t NOP           4E 71
;  38   BVC bvc_t           68 00 FF FC   cc=0x08
;  42   bvs_t NOP           4E 71
;  44   BVS bvs_t           69 00 FF FC   cc=0x09
;  48   bpl_t NOP           4E 71
;  50   BPL bpl_t           6A 00 FF FC   cc=0x0A
;  54   bmi_t NOP           4E 71
;  56   BMI bmi_t           6B 00 FF FC   cc=0x0B
;  60   bge_t NOP           4E 71
;  62   BGE bge_t           6C 00 FF FC   cc=0x0C
;  66   blt_t NOP           4E 71
;  68   BLT blt_t           6D 00 FF FC   cc=0x0D
;  72   bgt_t NOP           4E 71
;  74   BGT bgt_t           6E 00 FF FC   cc=0x0E
;  78   ble_t NOP           4E 71
;  80   BLE ble_t           6F 00 FF FC   cc=0x0F
; Total: 84 bytes

        SECTION TEXT

bhi_t:
        NOP
        BHI     bhi_t

bls_t:
        NOP
        BLS     bls_t

bcc_t:
        NOP
        BCC     bcc_t

bhs_t:
        NOP
        BHS     bhs_t

bcs_t:
        NOP
        BCS     bcs_t

blo_t:
        NOP
        BLO     blo_t

bvc_t:
        NOP
        BVC     bvc_t

bvs_t:
        NOP
        BVS     bvs_t

bpl_t:
        NOP
        BPL     bpl_t

bmi_t:
        NOP
        BMI     bmi_t

bge_t:
        NOP
        BGE     bge_t

blt_t:
        NOP
        BLT     blt_t

bgt_t:
        NOP
        BGT     bgt_t

ble_t:
        NOP
        BLE     ble_t

        END
