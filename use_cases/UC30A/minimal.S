; minimal.S - simplest possible DEVPAC3 source for UC30A
; Contains only DC.W directives (no instructions).
        SECTION TEXT
start:
        DC.W    $4E71           ; NOP opcode as data
        DC.W    $4E75           ; RTS opcode as data
        END
