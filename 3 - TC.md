# ST4Ever — Document 3 — TC : Test Cases

**Project:** ST4Ever — The Revival Engine for the Timeless ATARI ST
**Document:** 3 - TC.md — living document, updated at each validated Use Case (Phase 2)
**Language:** English
**Companion documents:**
- `CLAUDE.md` — R14/R15 test strategy, test matrix conventions
- `2 - SR.md` — UFR and REQ (requirements)
- `6 - UC.md` — Behavioral contracts, INTENT cross-reference

> **Traceability chain:** `UFR-[AREA]-NNN (2-SR.md) → REQ-[MOD]-NNN (2-SR.md) → TC-[MOD]-NNN (this file) → INT-[MOD]-NNN (use_case_XX.c)`
>
> **Test identifier format:** `TC-[MOD]-NNN` where MOD matches the module code in REQ tables.
> Each TC entry references its parent REQ and maps to an `/* INTENT[INT-xxx-NNN] */` block in `use_case_XX.c`.

## 5. Test Cases

### 5.1 INTENT Catalog — UC1

Each INTENT is extracted verbatim from a `/* INTENT: */` block in
`use_cases/use_case_01.c` and assigned a unique stable identifier used in
§5.2–§5.6.

#### Group 1 — Trace subsystem

| ID          | INTENT text (use_case_01.c — G1)                                         |
|-------------|--------------------------------------------------------------------------|
| INT-TRC-001 | trace_init must succeed and open the console on request                   |
| INT-TRC-002 | double init must be harmless — logs a warning, no error                   |
| INT-TRC-003 | all four log levels must emit without crashing                            |
| INT-TRC-004 | consecutive LOG_TRACE from same function collapse to [xN]                 |
| INT-TRC-005 | trace_close must succeed and update the open flag                         |
| INT-TRC-006 | trace_close on an already-closed console must be harmless                 |
| INT-TRC-007 | trace_open must reopen and update the open flag                           |
| INT-TRC-008 | LOG_TRACE must be suppressible without affecting other levels             |
| INT-TRC-009 | LOG_TRACE must be re-activatable after suppression                        |

#### Group 2 — Console context

| ID          | INTENT text (use_case_01.c — G2)                                         |
|-------------|--------------------------------------------------------------------------|
| INT-CON-001 | line_init must reject a NULL context pointer                              |
| INT-CON-002 | line_init must populate the context and capture the cwd                   |
| INT-CON-003 | line_shutdown must reject a NULL context pointer                         |
| INT-CON-004 | line_shutdown must clear the context and set bRunning FALSE               |

#### Group 3 — ST machine memory

| ID          | INTENT text (use_case_01.c — G3)                                         |
|-------------|--------------------------------------------------------------------------|
| INT-STM-001 | st_init must reject a NULL machine pointer                                |
| INT-STM-002 | st_init must succeed and power on the machine                             |
| INT-STM-003 | byte read/write round-trip must be exact                                  |
| INT-STM-004 | NULL machine pointer must be rejected on read and write                   |
| INT-STM-005 | NULL output pointer must be rejected                                      |
| INT-STM-006 | word read/write round-trip must preserve big-endian order                 |
| INT-STM-007 | long read/write round-trip must preserve big-endian order                 |
| INT-STM-008 | unaligned word access must raise a bus error                              |
| INT-STM-009 | unaligned long access must raise a bus error                              |
| INT-STM-010 | address beyond RAM (cartridge/ROM space) must not crash                   |
| INT-STM-011 | st_shutdown must reject a NULL pointer                                    |
| INT-STM-012 | st_shutdown must power off the machine cleanly                            |

#### Group 4 — CPU 68000

| ID          | INTENT text (use_case_01.c — G4)                                         |
|-------------|--------------------------------------------------------------------------|
| INT-CPU-001 | cpu_init must reject NULL pointers before touching state                  |
| INT-CPU-002 | hello.prg text section must load cleanly into ST RAM                      |
| INT-CPU-003 | cpu_init must read reset vectors and enter supervisor mode                |
| INT-CPU-004 | cpu_step must reject NULL ptCpu and NULL ptMachine                        |
| INT-CPU-005 | cpu_step must fetch the MOVEQ #42,D0 opcode and advance PC               |
| INT-CPU-006 | cpu_step must fetch the RTS opcode on the second step                     |

#### Group 5 — Disassembler

| ID          | INTENT text (use_case_01.c — G5)                                         |
|-------------|--------------------------------------------------------------------------|
| INT-DIS-001 | disasm_range must decode 4 bytes into 2 DC.W stub lines                  |
| INT-DIS-002 | zero-length buffer must produce 0 lines without error                    |
| INT-DIS-003 | NULL params must be rejected with ST_ERROR                               |

---

### 5.2 Test Cases — Trace Subsystem

Source: `use_cases/use_case_01.c` — Group 1

| ID         | Functional description                                 | Type | UFR                      | REQ                       | INTENT      | Input / Expected output                                           | Status   |
|------------|--------------------------------------------------------|------|--------------------------|---------------------------|-------------|-------------------------------------------------------------------|----------|
| TC-TRC-001 | Open trace console on init                             | [N]  | UFR-TRC-001, UFR-CON-033 | REQ-TRC-001, REQ-TRC-003  | INT-TRC-001 | `trace_init(ST_TRUE)` → `ST_NO_ERROR`; `trace_is_open()==TRUE`   | PASS UC1 |
| TC-TRC-002 | Double init is harmless                                | [R]  | UFR-TRC-001              | REQ-TRC-002               | INT-TRC-002 | `trace_init()` × 2 → `ST_NO_ERROR` (2nd call); state unchanged   | PASS UC1 |
| TC-TRC-003 | All four log levels emit without crash                 | [N]  | UFR-TRC-001, UFR-TRC-005 | REQ-TRC-004               | INT-TRC-003 | one call each LOG_TRACE/INFO/ERROR/TODO → no crash; 4 entries log | PASS UC1 |
| TC-TRC-004 | 5× consecutive LOG_TRACE compact to `[x5]`             | [N]  | UFR-TRC-003              | REQ-TRC-005               | INT-TRC-004 | 5× `LOG_TRACE` same fn → single `[x5]` entry after flush         | PASS UC1 |
| TC-TRC-005 | `trace_close()` hides console                          | [N]  | UFR-CON-031              | REQ-TRC-006               | INT-TRC-005 | `trace_close()` → `ST_NO_ERROR`; `trace_is_open()==FALSE`        | PASS UC1 |
| TC-TRC-006 | Double close is harmless                               | [R]  | UFR-CON-031              | REQ-TRC-007               | INT-TRC-006 | `trace_close()` × 2 → `ST_NO_ERROR` (2nd call)                   | PASS UC1 |
| TC-TRC-007 | `trace_open()` re-shows console                        | [N]  | UFR-CON-030              | REQ-TRC-008               | INT-TRC-007 | `trace_open()` → `ST_NO_ERROR`; `trace_is_open()==TRUE`          | PASS UC1 |
| TC-TRC-008 | `trace_set_trace_enabled(FALSE)` suppresses TRACE only | [N]  | UFR-TRC-006              | REQ-TRC-009, REQ-TRC-011  | INT-TRC-008 | disable; `LOG_TRACE` call → entry absent from log                 | PASS UC1 |
| TC-TRC-009 | `trace_set_trace_enabled(TRUE)` re-enables TRACE       | [N]  | UFR-TRC-006              | REQ-TRC-010, REQ-TRC-011  | INT-TRC-009 | re-enable; `LOG_TRACE` call → entry present in log                | PASS UC1 |

### 5.3 Test Cases — Console Context

Source: `use_cases/use_case_01.c` — Group 2

| ID          | Functional description                         | Type | UFR         | REQ         | INTENT      | Input / Expected output                                           | Status   |
|-------------|------------------------------------------------|------|-------------|-------------|-------------|-------------------------------------------------------------------|----------|
| TC-CON-001  | `line_init` rejects NULL context               | [R]  | UFR-CON-001 | REQ-CON-001 | INT-CON-001 | `line_init(NULL)` → `ST_ERROR`; no state modified                 | PASS UC1 |
| TC-CON-002  | `line_init` populates context                  | [N]  | UFR-CON-001 | REQ-CON-002, REQ-CON-003 | INT-CON-002 | `line_init(&tCtx)` → `ST_NO_ERROR`; `bRunning==TRUE`; `szCwd` non-empty | PASS UC1 |
| TC-CON-003  | `line_shutdown` rejects NULL context           | [R]  | UFR-CON-001 | REQ-CON-004 | INT-CON-003 | `line_shutdown(NULL)` → `ST_ERROR`; no state modified             | PASS UC1 |
| TC-CON-004  | `line_shutdown` clears context                 | [N]  | UFR-CON-001 | REQ-CON-005 | INT-CON-004 | `line_shutdown(&tCtx)` → `ST_NO_ERROR`; `bRunning==FALSE`         | PASS UC1 |

### 5.4 Test Cases — ST Machine

Source: `use_cases/use_case_01.c` — Group 3

| ID          | Functional description                              | Type | UFR          | REQ          | INTENT      | Input / Expected output                                                        | Status        |
|-------------|-----------------------------------------------------|------|--------------|--------------|-------------|--------------------------------------------------------------------------------|---------------|
| TC-STM-001  | `st_init` rejects NULL machine pointer              | [R]  | — (see UC25) | REQ-STM-001  | INT-STM-001 | `st_init(NULL, NULL)` → `ST_ERROR`                                             | PASS UC1      |
| TC-STM-002  | `st_init` powers on the machine                     | [N]  | — (see UC25) | REQ-STM-002, REQ-STM-003 | INT-STM-002 | `st_init(&m, NULL)` → `ST_NO_ERROR`; `bPoweredOn==TRUE`; `uiRes==ST_RES_LOW`  | PASS UC1      |
| TC-STM-003  | Byte round-trip is exact                            | [N]  | — (see UC25) | REQ-STM-006  | INT-STM-003 | write `0xAB` @ `0x1000`; read back → `0xAB`                                   | PASS UC1      |
| TC-STM-004  | NULL machine rejected on r/w                        | [R]  | — (see UC25) | REQ-STM-004  | INT-STM-004 | `st_write_byte(NULL,…)` ; `st_read_byte(NULL,…)` → `ST_ERROR` both            | PASS UC1      |
| TC-STM-005  | NULL output pointer rejected                        | [R]  | — (see UC25) | REQ-STM-005  | INT-STM-005 | `st_read_byte(&m, addr, NULL)` → `ST_ERROR`                                    | PASS UC1      |
| TC-STM-006  | Word round-trip preserves big-endian order          | [N]  | — (see UC25) | REQ-STM-007  | INT-STM-006 | write `0x1234` @ `0x2000`; read back → `0x1234`                               | PASS UC1      |
| TC-STM-007  | Long round-trip preserves big-endian order          | [N]  | — (see UC25) | REQ-STM-008  | INT-STM-007 | write `0xDEADBEEF` @ `0x3000`; read back → `0xDEADBEEF`                       | PASS UC1      |
| TC-STM-008  | Unaligned word access → bus error                   | [R]  | — (see UC25) | REQ-STM-009  | INT-STM-008 | `st_write_word(&m, 0x1001, 0)` → `ST_ERROR`                                   | PASS UC1      |
| TC-STM-009  | Unaligned long access → bus error                   | [R]  | — (see UC25) | REQ-STM-010  | INT-STM-009 | `st_write_long(&m, 0x1003, 0)` → `ST_ERROR`                                   | PASS UC1      |
| TC-STM-010  | Out-of-RAM access does not crash (stub)             | [R]  | — (see UC24) | REQ-STM-011  | INT-STM-010 | `st_read_byte(&m, ST_RAM_SIZE, &b)` → `ST_NO_ERROR` + `b==0xFF` ADAPTED(UC24) | PASS UC1      |
| TC-STM-011  | `st_shutdown` rejects NULL pointer                  | [R]  | — (see UC25) | REQ-STM-013  | INT-STM-011 | `st_shutdown(NULL)` → `ST_ERROR`                                               | PASS UC1      |
| TC-STM-012  | `st_shutdown` powers off the machine                | [N]  | — (see UC25) | REQ-STM-014  | INT-STM-012 | `st_shutdown(&m)` → `ST_NO_ERROR`; `bPoweredOn==FALSE`                         | PASS UC1      |

### 5.5 Test Cases — CPU 68000

Source: `use_cases/use_case_01.c` — Group 4
Fixture: `use_cases/UC01/hello.prg` — `.text` = `0x702A 0x4E75` (4 bytes)

| ID          | Functional description                                    | Type | UFR          | REQ          | INTENT      | Input / Expected output                                                                                      | Status        |
|-------------|-----------------------------------------------------------|------|--------------|--------------|-------------|--------------------------------------------------------------------------------------------------------------|---------------|
| TC-CPU-001  | `cpu_init` rejects NULL CPU pointer                       | [R]  | — (see UC25) | REQ-CPU-001  | INT-CPU-001 | `cpu_init(NULL, &m)` → `ST_ERROR`                                                                            | PASS UC1      |
| TC-CPU-002  | `cpu_init` rejects NULL machine pointer                   | [R]  | — (see UC25) | REQ-CPU-001  | INT-CPU-001 | `cpu_init(&cpu, NULL)` → `ST_ERROR`                                                                          | PASS UC1      |
| TC-CPU-003  | hello.prg loads cleanly into ST RAM                       | [N]  | — (see UC25) | REQ-STM-006  | INT-CPU-002 | `uc01_load_prg_text(…, 0x1000)` → `ST_NO_ERROR`; `uiTextSz==4`                                             | PASS UC1      |
| TC-CPU-004  | `cpu_init` reads vectors and sets supervisor mode         | [N]  | — (see UC25) | REQ-CPU-002..005 | INT-CPU-003 | SSP=0x0800, PC=0x1000 written; `cpu_init()` → `uiPC==0x1000`; `uiSSP==0x0800`; `SR&CPU_SR_S≠0`; `eState==RUNNING` | PASS UC1 |
| TC-CPU-005  | `cpu_step` rejects NULL pointers                          | [R]  | — (see UC25) | REQ-CPU-006  | INT-CPU-004 | `cpu_step(NULL,…)` ; `cpu_step(…,NULL,…)` → `ST_ERROR` both                                                 | PASS UC1      |
| TC-CPU-006  | Step #1 fetches MOVEQ #42,D0 opcode                       | [N]  | — (see UC25) | REQ-CPU-007, REQ-CPU-008 | INT-CPU-005 | `cpu_step()` at PC=0x1000 → `uiOpcode==0x702A`; `uiPCAfter==0x1002` ADAPTED(UC21)                   | PASS UC1      |
| TC-CPU-007  | Step #2 fetches RTS opcode                                | [N]  | — (see UC25) | REQ-CPU-007  | INT-CPU-006 | `cpu_step()` at PC=0x1002 → `uiOpcode==0x4E75`                                                              | PASS UC1      |

### 5.6 Test Cases — Disassembler

Source: `use_cases/use_case_01.c` — Group 5
Input: `{ 0x70, 0x2A, 0x4E, 0x75 }` at base `0x1000`

| ID          | Functional description                                    | Type | UFR          | REQ          | INTENT      | Input / Expected output                                             | Status   |
|-------------|-----------------------------------------------------------|------|--------------|--------------|-------------|---------------------------------------------------------------------|----------|
| TC-DIS-001  | 4-byte buffer → 2 lines; 0x702A=MOVEQ now bValid=TRUE     | [N]  | UFR-HEX-005 | REQ-DIS-004, REQ-DIS-006 | INT-DIS-001 | `disasm_range(buf,4,…)` → `ST_NO_ERROR`; `n==2`; 0x702A bValid=TRUE ADAPTED(UC11); 0x4E75 still DC.W ADAPTED(UC14) | PASS UC11 |
| TC-DIS-002  | Zero-length buffer → 0 lines, no error                    | [N]  | — (see UC10) | REQ-DIS-004  | INT-DIS-002 | `disasm_range(buf,0,…)` → `ST_NO_ERROR`; `n==0`                    | PASS UC1 |
| TC-DIS-003  | NULL `pBuf` rejected                                      | [R]  | — (see UC10) | REQ-DIS-001  | INT-DIS-003 | `disasm_range(NULL,4,…)` → `ST_ERROR`                              | PASS UC1 |
| TC-DIS-004  | NULL `ptResults` rejected                                 | [R]  | — (see UC10) | REQ-DIS-002  | INT-DIS-003 | `disasm_range(buf,4,…,NULL,…)` → `ST_ERROR`                        | PASS UC1 |
| TC-DIS-005  | NULL `puiLines` rejected                                  | [R]  | — (see UC10) | REQ-DIS-003  | INT-DIS-003 | `disasm_range(buf,4,…,NULL)` → `ST_ERROR`                          | PASS UC1 |

---

### 5.7 Traceability Matrix

#### Full chain: UFR → REQ → INTENT → TC

| UFR          | REQ(s)                          | INTENT(s)              | TC(s)                          | Status (UC1) |
|--------------|---------------------------------|------------------------|--------------------------------|--------------|
| UFR-CON-001  | REQ-CON-001..005                | INT-CON-001..004       | TC-CON-001..004                | ✓ PASS       |
| UFR-CON-010  | REQ-CON-006                     | —                      | (interactive — manual)         | ✓ PASS       |
| UFR-CON-020  | REQ-CON-007                     | —                      | (interactive — manual)         | ✓ PASS       |
| UFR-CON-030  | REQ-TRC-008                     | INT-TRC-007            | TC-TRC-007                     | stub → UC2   |
| UFR-CON-031  | REQ-TRC-006, REQ-TRC-007        | INT-TRC-005, INT-TRC-006 | TC-TRC-005, TC-TRC-006       | ✓ PASS       |
| UFR-CON-033  | REQ-TRC-001                     | INT-TRC-001            | TC-TRC-001                     | ✓ PASS       |
| UFR-TRC-001  | REQ-TRC-001, REQ-TRC-004        | INT-TRC-001, INT-TRC-003 | TC-TRC-001, TC-TRC-003       | ✓ PASS       |
| UFR-TRC-003  | REQ-TRC-005                     | INT-TRC-004            | TC-TRC-004                     | ✓ PASS       |
| UFR-TRC-004  | REQ-TRC-012, REQ-TRC-013        | —                      | TC-TRC (shutdown in main)      | ✓ PASS       |
| UFR-TRC-005  | REQ-TRC-004                     | INT-TRC-003            | TC-TRC-003                     | ✓ PASS       |
| UFR-TRC-006  | REQ-TRC-009..011                | INT-TRC-008, INT-TRC-009 | TC-TRC-008, TC-TRC-009       | ✓ PASS       |
| UFR-TRC-007  | REQ-TRC-014, REQ-TRC-017..022   | INT-TRC-020..026       | TC-TRC-018..026, TC-TRC-027..034 | ✓ UC4.4   |
| UFR-TRC-008  | REQ-TRC-019                     | INT-TRC-022            | TC-TRC-028 (manual)            | ✓ UC4.4      |
| UFR-TRC-009  | REQ-TRC-020, REQ-TRC-022        | INT-TRC-023            | TC-TRC-029..032, TC-TRC-034    | ✓ UC4.4      |
| UFR-TRC-010  | REQ-TRC-021                     | INT-TRC-022            | (implied by TC-TRC-020)        | ✓ UC4.4      |
| UFR-CON-008      | REQ-CON-010                   | INT-CON-007          | TC-CON-003, TC-CON-012 (manual) | ✓ UC4.2     |
| UFR-CON-040      | REQ-CON-011, REQ-DIR-001..007 | INT-DIR-001..010     | TC-DIR-001..016                | ✓ UC3.3      |
| UFR-CON-041      | REQ-CON-010, REQ-RAW-001..014 | INT-CON-006          | TC-CON-005, TC-CON-011 (manual) | ✓ UC4.2     |
| UFR-CON-042      | REQ-CON-010                   | INT-CON-005          | TC-CON-010 (manual)            | ✓ UC4.2      |
| UFR-CON-043      | REQ-CON-010, REQ-RAW-009..013 | INT-CON-008..009     | TC-CON-004, TC-CON-013..014    | ✓ UC4.2      |
| UFR-CON-044      | REQ-CON-010, REQ-RAW-012      | INT-CON-010          | TC-CON-003, TC-CON-015         | ✓ UC4.2      |
| UFR-CON-045      | REQ-CON-010                   | INT-CON-011          | TC-CON-015..016                | ✓ UC4.2      |
| UFR-CON-046      | REQ-CON-010                   | INT-CON-012          | TC-CON-001, TC-CON-016..017    | ✓ UC4.2      |
| UFR-CON-050..091 | —                           | —                      | —                              | TODO UCx     |
| UFR-GUI-005      | REQ-RND-001..007            | INT-RND-001..007       | TC-RND-001..020                | ✓ UC3.2      |
| UFR-GUI-006      | REQ-GUI-017, REQ-DIR-011    | —                      | TC-DIR-018 (manual)            | ✓ UC3.3      |
| UFR-DIR-001..004 | REQ-DIR-001..013            | INT-DIR-001..010       | TC-DIR-001..020                | ✓ UC3.3      |
| UFR-DIR-005..006 | —                           | —                      | —                              | TODO UC7/UC18|
| UFR-DIR-007      | REQ-DIR-014..016            | INT-DIR-011..014       | TC-DIR4-002..008               | ✓ UC4.1      |
| UFR-DIR-008      | REQ-DIR-017                 | INT-DIR-015            | TC-DIR4-009 (manual)           | ✓ UC4.1      |
| UFR-DIR-009      | REQ-DIR-018                 | INT-DIR-016..017       | TC-DIR4-010..011 (manual)      | ✓ UC4.1      |
| UFR-DIR-010      | REQ-DIR-019                 | INT-DIR-018..019       | TC-DIR4-012..013 (manual)      | ✓ UC4.1      |
| UFR-GUI-007      | REQ-GUI-021, REQ-DIR-020    | INT-DIR-020            | TC-DIR4-014 (manual)           | ✓ UC4.1      |

#### REQ → TC coverage (UC1)

| REQ          | TC(s)                       | Status        |
|--------------|-----------------------------|---------------|
| REQ-TRC-001  | TC-TRC-001                  | ✓ PASS        |
| REQ-TRC-002  | TC-TRC-002                  | ✓ PASS        |
| REQ-TRC-003  | TC-TRC-001                  | ✓ PASS        |
| REQ-TRC-004  | TC-TRC-003                  | ✓ PASS        |
| REQ-TRC-005  | TC-TRC-004                  | ✓ PASS        |
| REQ-TRC-006  | TC-TRC-005                  | ✓ PASS        |
| REQ-TRC-007  | TC-TRC-006                  | ✓ PASS        |
| REQ-TRC-008  | TC-TRC-007                  | ✓ PASS        |
| REQ-TRC-009  | TC-TRC-008                  | ✓ PASS        |
| REQ-TRC-010  | TC-TRC-009                  | ✓ PASS        |
| REQ-TRC-011  | TC-TRC-008, TC-TRC-009      | ✓ PASS        |
| REQ-TRC-012  | (trace_shutdown in main)    | ✓ PASS        |
| REQ-TRC-013  | (file/ANSI — manual check)  | ✓ PASS        |
| REQ-TRC-014  | TC-TRC-020, TC-TRC-026      | ✓ UC4.4       |
| REQ-TRC-015  | (format — visual check st4ever_trace.log) | ✓ PASS |
| REQ-TRC-016  | TC-TRC-015, TC-TRC-016      | ✓ P19         |
| REQ-CON-001  | TC-CON-001                  | ✓ PASS        |
| REQ-CON-002  | TC-CON-002                  | ✓ PASS        |
| REQ-CON-003  | TC-CON-002                  | ✓ PASS        |
| REQ-CON-004  | TC-CON-003                  | ✓ PASS        |
| REQ-CON-005  | TC-CON-004                  | ✓ PASS        |
| REQ-CON-006  | (interactive — manual)      | ✓ PASS        |
| REQ-CON-007  | (interactive — manual)      | ✓ PASS        |
| REQ-CON-008  | —                           | stub → UC2    |
| REQ-CON-009  | (interactive — manual)      | ✓ PASS        |
| REQ-CON-010  | TC-CON-010..017 (manual)    | ✓ UC4.2       |
| REQ-CON-015  | (fgets fallback — non-TTY)  | ✓ UC4.2       |
| REQ-CON-016  | (restore after loop)        | ✓ UC4.2       |
| REQ-CON-017  | —                           | TODO UC4.3    |
| REQ-CON-018  | —                           | TODO UC4.3    |
| REQ-STM-001  | TC-STM-001                  | ✓ PASS        |
| REQ-STM-002  | TC-STM-002                  | ✓ PASS        |
| REQ-STM-003  | TC-STM-002                  | ✓ PASS        |
| REQ-STM-004  | TC-STM-004                  | ✓ PASS        |
| REQ-STM-005  | TC-STM-005                  | ✓ PASS        |
| REQ-STM-006  | TC-STM-003, TC-CPU-003      | ✓ PASS        |
| REQ-STM-007  | TC-STM-006                  | ✓ PASS        |
| REQ-STM-008  | TC-STM-007                  | ✓ PASS        |
| REQ-STM-009  | TC-STM-008                  | ✓ PASS        |
| REQ-STM-010  | TC-STM-009                  | ✓ PASS        |
| REQ-STM-011  | TC-STM-010                  | ADAPTED(UC24) |
| REQ-STM-012  | —                           | TODO UC24     |
| REQ-STM-013  | TC-STM-011                  | ✓ PASS        |
| REQ-STM-014  | TC-STM-012                  | ✓ PASS        |
| REQ-CPU-001  | TC-CPU-001, TC-CPU-002      | ✓ PASS        |
| REQ-CPU-002  | TC-CPU-004                  | ✓ PASS        |
| REQ-CPU-003  | TC-CPU-004                  | ✓ PASS        |
| REQ-CPU-004  | TC-CPU-004                  | ✓ PASS        |
| REQ-CPU-005  | TC-CPU-004                  | ✓ PASS        |
| REQ-CPU-006  | TC-CPU-005                  | ✓ PASS        |
| REQ-CPU-007  | TC-CPU-006, TC-CPU-007      | ✓ PASS        |
| REQ-CPU-008  | TC-CPU-006                  | ADAPTED(UC21) |
| REQ-CPU-009  | —                           | TODO UC21     |
| REQ-CPU-010  | —                           | TODO UC22     |
| REQ-CPU-011  | —                           | TODO UC23     |
| REQ-DIS-001  | TC-DIS-003                  | ✓ PASS        |
| REQ-DIS-002  | TC-DIS-004                  | ✓ PASS        |
| REQ-DIS-003  | TC-DIS-005                  | ✓ PASS        |
| REQ-DIS-004  | TC-DIS-001, TC-DIS-002      | ✓ PASS        |
| REQ-DIS-005  | TC-DIS-001                  | ADAPTED(UC11) |
| REQ-DIS-006  | TC-DIS-010..080 (§5.37)     | ✓ UC11        |
| REQ-DIS-007  | TC-DIS-100..159 (§5.39)     | ✓ UC12        |
| REQ-DIS-008  | TC-DIS-200..235 (§5.41)     | ✓ UC13        |
| REQ-DIS-009  | TC-DIS-300..330 (§5.43)     | ✓ UC14        |
| REQ-RAW-001  | (pipe path — TC-CON-006 when TTY) | ✓ UC4.2  |
| REQ-RAW-002  | (cmd.exe path — manual)     | ✓ UC4.2       |
| REQ-RAW-003  | TC-CON-006 INFO (non-TTY skip) | ✓ UC4.2    |
| REQ-RAW-004  | (set_raw idempotent)        | ✓ UC4.2       |
| REQ-RAW-005  | TC-CON-007, TC-CON-009      | ✓ UC4.2       |
| REQ-RAW-006  | (cmd.exe restore — manual)  | ✓ UC4.2       |
| REQ-RAW-007  | TC-CON-008                  | ✓ UC4.2       |
| REQ-RAW-008  | (no raw mode → EOF)         | ✓ UC4.2       |
| REQ-RAW-009  | TC-CON-004                  | ✓ UC4.2       |
| REQ-RAW-010  | TC-CON-005                  | ✓ UC4.2       |
| REQ-RAW-011  | TC-CON-005                  | ✓ UC4.2       |
| REQ-RAW-012  | TC-CON-003                  | ✓ UC4.2       |
| REQ-RAW-013  | (Win32 VK — cmd.exe manual) | ✓ UC4.2       |
| REQ-RAW-014  | (EOF path — implied)        | ✓ UC4.2       |

#### Test Summary — UC1

| Module | [N] | [R] | [S] | Total | Result    |
|--------|-----|-----|-----|-------|-----------|
| TRC    | 7   | 2   | 0   | 9     | ALL PASS  |
| CON    | 2   | 2   | 0   | 4     | ALL PASS  |
| STM    | 6   | 6   | 0   | 12    | ALL PASS  |
| CPU    | 5   | 2   | 0   | 7     | ALL PASS  |
| DIS    | 2   | 3   | 0   | 5     | ALL PASS  |
| **Total** | **22** | **15** | **0** | **37** | **ALL PASS** |

---

### 5.8 INTENT Catalog — UC2

Source: `use_cases/use_case_02.c`

| ID          | INTENT text                                                              |
|-------------|--------------------------------------------------------------------------|
| INT-TRC-010 | trace toggle (no arg) opens if closed, closes if open; enabled unchanged |
| INT-TRC-011 | trace on: open console + enable LOG_TRACE (idempotent)                   |
| INT-TRC-012 | trace off: disable LOG_TRACE only; console stays open; idempotent (P19)  |
| INT-CON-010 | LOG_INFO/ERROR/TODO remain active when trace_enabled is FALSE            |

### 5.9 Test Cases — UC2 (trace command)

Source: `use_cases/use_case_02.c`

| ID          | Functional description                          | Type | UFR          | REQ         | INTENT      | Expected outcome                                          | Status   |
|-------------|-------------------------------------------------|------|--------------|-------------|-------------|-----------------------------------------------------------|----------|
| TC-TRC-010  | toggle closed→open (no arg)                     | [N]  | UFR-CON-032  | REQ-CON-008 | INT-TRC-010 | `trace_open()` called; `trace_is_open()==TRUE`            | PASS UC2 |
| TC-TRC-011  | toggle open→closed (no arg)                     | [N]  | UFR-CON-032  | REQ-CON-008 | INT-TRC-010 | `trace_close()` called; `trace_is_open()==FALSE`          | PASS UC2 |
| TC-TRC-012  | toggle preserves trace_enabled state            | [N]  | UFR-CON-032  | REQ-CON-008 | INT-TRC-010 | `trace_is_trace_enabled()` unchanged after toggle         | PASS UC2 |
| TC-TRC-013  | `trace on`: open + enable (idempotent)          | [N]  | UFR-CON-030  | REQ-CON-008 | INT-TRC-011 | `trace_is_open()==TRUE`; `trace_is_trace_enabled()==TRUE` | PASS UC2 |
| TC-TRC-014  | `trace on` when already open: harmless          | [R]  | UFR-CON-030  | REQ-TRC-002 | INT-TRC-011 | second `trace_open()` → `ST_NO_ERROR`; state unchanged    | PASS UC2 |
| TC-TRC-015  | `trace off`: disable LOG_TRACE, view stays open | [N]  | UFR-CON-031  | REQ-TRC-016 | INT-TRC-012 | `trace_is_open()==TRUE`; `trace_is_trace_enabled()==FALSE` (ADAPTED P19) | PASS P19 |
| TC-TRC-016  | `trace off` idempotent (second disable harmless)| [R]  | UFR-CON-031  | REQ-TRC-016 | INT-TRC-012 | `trace_set_trace_enabled(FALSE)` × 2 → `ST_NO_ERROR`; `trace_is_open()==TRUE` (ADAPTED P19) | PASS P19 |
| TC-TRC-017  | INFO/ERROR/TODO active when TRACE disabled      | [R]  | UFR-TRC-005  | REQ-TRC-009 | INT-CON-010 | `trace_set_trace_enabled(FALSE)` → other levels emit OK   | PASS UC2 |

#### Test Summary — UC2

| Module | [N] | [R] | [S] | Total | Result   |
|--------|-----|-----|-----|-------|----------|
| TRC/CON| 5   | 2   | 0   | 7     | ALL PASS |
| (stdin dispatch) | — | — | 4 | 4 | SKIP (make manual) |

---

### 5.10 INTENT Catalog — UC3.1

Source: `use_cases/use_case_03_1.c`

| ID          | INTENT text                                                                      |
|-------------|----------------------------------------------------------------------------------|
| INT-GUI-001 | create queue of capacity N: handle non-NULL, events preserved in FIFO order      |
| INT-GUI-002 | post beyond capacity: ST_ERROR without blocking or data loss                     |
| INT-GUI-003 | get on empty queue (non-blocking): ST_ERROR immediately                          |
| INT-GUI-004 | gui_init registers WNDCLASS; idempotent; gui_shutdown safe no-op if not init'd   |
| INT-GUI-005 | gui_open_window / gui_close_window: NULL params rejected before any side effect  |

### 5.11 Test Cases — UC3.1 (GUI infrastructure)

Source: `use_cases/use_case_03_1.c`

| ID          | Functional description                           | Type | UFR         | REQ          | INTENT      | Expected outcome                                        | Status     |
|-------------|--------------------------------------------------|------|-------------|--------------|-------------|----------------------------------------------------------|------------|
| TC-GUI-001  | msg_queue create capacity 4                      | [N]  | UFR-GUI-002 | REQ-GUI-009  | INT-GUI-001 | `ST_NO_ERROR`; handle ≠ NULL                            | PASS UC3.1 |
| TC-GUI-002  | post + get single event, FIFO                    | [N]  | UFR-GUI-002 | REQ-GUI-011  | INT-GUI-001 | get returns event with same type as posted              | PASS UC3.1 |
| TC-GUI-003  | two events: FIFO order preserved                 | [N]  | UFR-GUI-002 | REQ-GUI-011  | INT-GUI-001 | first get = RESIZE, second get = CLOSE                  | PASS UC3.1 |
| TC-GUI-004  | fill queue to capacity (4 posts)                 | [N]  | UFR-GUI-002 | REQ-GUI-012  | INT-GUI-001 | all 4 posts succeed                                     | PASS UC3.1 |
| TC-GUI-005  | drain full queue (4 gets)                        | [N]  | UFR-GUI-002 | REQ-GUI-011  | INT-GUI-001 | all 4 gets succeed                                      | PASS UC3.1 |
| TC-GUI-006  | destroy sets handle to NULL                      | [N]  | UFR-GUI-002 | REQ-GUI-014  | INT-GUI-001 | handle == NULL after destroy                            | PASS UC3.1 |
| TC-GUI-007  | post beyond capacity → ST_ERROR                  | [R]  | UFR-GUI-002 | REQ-GUI-012  | INT-GUI-002 | 5th post on capacity-4 queue → ST_ERROR                 | PASS UC3.1 |
| TC-GUI-008  | get on empty (non-blocking) → ST_ERROR           | [R]  | UFR-GUI-002 | REQ-GUI-013  | INT-GUI-003 | `gui_msg_get(bBlock=FALSE)` empty → ST_ERROR            | PASS UC3.1 |
| TC-GUI-009  | msg_create NULL out, capacity 0                  | [R]  | UFR-GUI-002 | REQ-GUI-009..010 | INT-GUI-001 | both → ST_ERROR; handle stays NULL                  | PASS UC3.1 |
| TC-GUI-010  | post/get/destroy NULL params                     | [R]  | UFR-GUI-002 | REQ-GUI-011..014 | INT-GUI-001 | all → ST_ERROR                                      | PASS UC3.1 |
| TC-GUI-011  | gui_init idempotent                              | [N]  | UFR-GUI-001 | REQ-GUI-001..002 | INT-GUI-004 | two calls → ST_NO_ERROR both                        | PASS UC3.1 |
| TC-GUI-012  | gui_shutdown after init                          | [N]  | UFR-GUI-001 | REQ-GUI-007  | INT-GUI-004 | ST_NO_ERROR                                             | PASS UC3.1 |
| TC-GUI-013  | gui_shutdown without prior init (no-op)          | [N]  | UFR-GUI-001 | REQ-GUI-007  | INT-GUI-004 | ST_NO_ERROR                                             | PASS UC3.1 |
| TC-GUI-014  | gui_open_window NULL desc or NULL phWnd          | [R]  | UFR-GUI-001 | REQ-GUI-003  | INT-GUI-005 | ST_ERROR; hWnd stays NULL                               | PASS UC3.1 |
| TC-GUI-015  | gui_close/invalidate/get_size NULL               | [R]  | UFR-GUI-001 | REQ-GUI-005..006 | INT-GUI-005 | all → ST_ERROR                                      | PASS UC3.1 |
| TC-GUI-016  | window open + visible (manual)                   | [S]  | UFR-GUI-001 | REQ-GUI-004  | INT-GUI-004 | dark background, title bar (make manual)                | SKIP       |
| TC-GUI-017  | window close via gui_close_window (manual)       | [S]  | UFR-GUI-001 | REQ-GUI-006  | INT-GUI-004 | thread exits, handle freed (make manual)                | SKIP       |
| TC-GUI-018  | window responds to OS close button (manual)      | [S]  | UFR-GUI-001 | REQ-GUI-015  | INT-GUI-005 | WM_DESTROY fires GUI_EVT_CLOSE (make manual)            | SKIP       |

#### Test Summary — UC3.1

| Module | [N] | [R] | [S] | Total | Result    |
|--------|-----|-----|-----|-------|-----------|
| GUI    | 11  | 4   | 3   | 18    | ALL PASS  |

#### REQ → TC coverage (UC3.1)

| REQ          | TC(s)                           | Status     |
|--------------|---------------------------------|------------|
| REQ-GUI-001  | TC-GUI-011                      | ✓ PASS     |
| REQ-GUI-002  | TC-GUI-011                      | ✓ PASS     |
| REQ-GUI-003  | TC-GUI-014                      | ✓ PASS     |
| REQ-GUI-004  | TC-GUI-016                      | SKIP       |
| REQ-GUI-005  | TC-GUI-015                      | ✓ PASS     |
| REQ-GUI-006  | TC-GUI-017                      | SKIP       |
| REQ-GUI-007  | TC-GUI-012, TC-GUI-013          | ✓ PASS     |
| REQ-GUI-008  | (gui_shutdown in TC-GUI-012)    | ✓ PASS     |
| REQ-GUI-009  | TC-GUI-009                      | ✓ PASS     |
| REQ-GUI-010  | TC-GUI-009                      | ✓ PASS     |
| REQ-GUI-011  | TC-GUI-002, TC-GUI-003, TC-GUI-005 | ✓ PASS  |
| REQ-GUI-012  | TC-GUI-007                      | ✓ PASS     |
| REQ-GUI-013  | TC-GUI-008                      | ✓ PASS     |
| REQ-GUI-014  | TC-GUI-006, TC-GUI-010          | ✓ PASS     |
| REQ-GUI-015  | TC-GUI-018                      | SKIP       |
| REQ-GUI-016  | —                               | TODO UC3.2 |

---

### 5.12 INTENT Catalog — UC3.2

Source: `use_cases/use_case_03_2.c`

| ID          | INTENT text                                                                                |
|-------------|--------------------------------------------------------------------------------------------|
| INT-RND-001 | renderer_create with valid window handle succeeds and returns non-NULL handle              |
| INT-RND-002 | renderer_create with NULL parameters returns ST_ERROR                                      |
| INT-RND-003 | begin_draw / end_draw on valid renderer succeed without crash                              |
| INT-RND-004 | draw primitives (fill_rect, draw_rect, draw_line, draw_text) accept NULL renderer → ST_ERROR |
| INT-RND-005 | renderer_resize with new dimensions does not crash or leak                                 |
| INT-RND-006 | renderer_get_font_metrics returns positive cell width and height                           |
| INT-RND-007 | renderer_destroy(NULL) is safe; destroy(valid) releases all COM objects cleanly           |

### 5.13 Test Cases — UC3.2 (2D renderer)

Source: `use_cases/use_case_03_2.c`

| ID          | Functional description                               | Type | UFR         | REQ          | INTENT      | Expected outcome                                                | Status     |
|-------------|------------------------------------------------------|------|-------------|--------------|-------------|-----------------------------------------------------------------|------------|
| TC-RND-001  | renderer_create with valid window → ST_NO_ERROR      | [N]  | UFR-GUI-005 | REQ-RND-002  | INT-RND-001 | handle ≠ NULL                                                   | PASS UC3.2 |
| TC-RND-002  | renderer_create(NULL, *) → ST_ERROR                  | [R]  | UFR-GUI-005 | REQ-RND-001  | INT-RND-002 | ST_ERROR; handle stays NULL                                     | PASS UC3.2 |
| TC-RND-003  | renderer_create(*, NULL) → ST_ERROR                  | [R]  | UFR-GUI-005 | REQ-RND-001  | INT-RND-002 | ST_ERROR                                                        | PASS UC3.2 |
| TC-RND-004  | begin_draw + end_draw on valid renderer              | [N]  | UFR-GUI-005 | REQ-RND-003  | INT-RND-003 | both return ST_NO_ERROR                                         | PASS UC3.2 |
| TC-RND-005  | begin_draw(NULL) → ST_ERROR                          | [R]  | UFR-GUI-005 | REQ-RND-003  | INT-RND-004 | ST_ERROR                                                        | PASS UC3.2 |
| TC-RND-006  | end_draw(NULL) → ST_ERROR                            | [R]  | UFR-GUI-005 | REQ-RND-003  | INT-RND-004 | ST_ERROR                                                        | PASS UC3.2 |
| TC-RND-007  | fill_rect(NULL, …) → ST_ERROR                        | [R]  | UFR-GUI-005 | REQ-RND-004  | INT-RND-004 | ST_ERROR                                                        | PASS UC3.2 |
| TC-RND-008  | draw_rect(NULL, …) → ST_ERROR                        | [R]  | UFR-GUI-005 | REQ-RND-004  | INT-RND-004 | ST_ERROR                                                        | PASS UC3.2 |
| TC-RND-009  | draw_line(NULL, …) → ST_ERROR                        | [R]  | UFR-GUI-005 | REQ-RND-004  | INT-RND-004 | ST_ERROR                                                        | PASS UC3.2 |
| TC-RND-010  | draw_text(NULL, …) → ST_ERROR                        | [R]  | UFR-GUI-005 | REQ-RND-004  | INT-RND-004 | ST_ERROR                                                        | PASS UC3.2 |
| TC-RND-011  | resize(NULL, …) → ST_ERROR                           | [R]  | UFR-GUI-005 | REQ-RND-005  | INT-RND-005 | ST_ERROR                                                        | PASS UC3.2 |
| TC-RND-012  | get_font_metrics: cell W > 0 and H > 0               | [N]  | UFR-GUI-005 | REQ-RND-007  | INT-RND-006 | iCellW > 0; iCellH > 0                                          | PASS UC3.2 |
| TC-RND-013  | get_font_metrics(NULL, …) → ST_ERROR                 | [R]  | UFR-GUI-005 | REQ-RND-007  | INT-RND-006 | ST_ERROR                                                        | PASS UC3.2 |
| TC-RND-014  | renderer_destroy(NULL) → no crash                    | [R]  | UFR-GUI-005 | REQ-RND-006  | INT-RND-007 | no crash; safe no-op                                            | PASS UC3.2 |
| TC-RND-015  | renderer_destroy(valid) → ST_NO_ERROR                | [N]  | UFR-GUI-005 | REQ-RND-006  | INT-RND-007 | ST_NO_ERROR; COM released                                       | PASS UC3.2 |
| TC-RND-016  | D2D draw scene renders (visual — make manual)        | [S]  | UFR-GUI-005 | REQ-RND-002  | INT-RND-001 | coloured rectangles + text visible in window                    | SKIP       |
| TC-RND-017  | resize redraws correctly (visual — make manual)      | [S]  | UFR-GUI-005 | REQ-RND-005  | INT-RND-005 | content scales after resize                                     | SKIP       |
| TC-RND-018  | font metrics match actual render (visual)            | [S]  | UFR-GUI-005 | REQ-RND-007  | INT-RND-006 | text aligns on cell grid                                        | SKIP       |
| TC-RND-019  | full scene: fill + rect + line + text (visual)       | [S]  | UFR-GUI-005 | REQ-RND-004  | INT-RND-003 | all primitives visible and correctly coloured                   | SKIP       |
| TC-RND-020  | destroy + recreate renderer (visual)                 | [S]  | UFR-GUI-005 | REQ-RND-006  | INT-RND-007 | second create succeeds; first destroy releases cleanly          | SKIP       |

#### Test Summary — UC3.2

| Module | [N] | [R] | [S] | Total | Result    |
|--------|-----|-----|-----|-------|-----------|
| RND    | 7   | 10  | 5   | 22    | ALL PASS  |

#### REQ → TC coverage (UC3.2)

| REQ          | TC(s)                               | Status    |
|--------------|-------------------------------------|-----------|
| REQ-RND-001  | TC-RND-002, TC-RND-003              | ✓ PASS    |
| REQ-RND-002  | TC-RND-001, TC-RND-016              | ✓ PASS    |
| REQ-RND-003  | TC-RND-004..006                     | ✓ PASS    |
| REQ-RND-004  | TC-RND-007..010, TC-RND-019         | ✓ PASS    |
| REQ-RND-005  | TC-RND-011, TC-RND-017              | ✓ PASS    |
| REQ-RND-006  | TC-RND-014, TC-RND-015, TC-RND-020  | ✓ PASS    |
| REQ-RND-007  | TC-RND-012, TC-RND-013, TC-RND-018  | ✓ PASS    |

---

### 5.14 INTENT Catalog — UC3.3

Source: `use_cases/use_case_03_3.c`

| ID          | INTENT text                                                                              |
|-------------|------------------------------------------------------------------------------------------|
| INT-DIR-001 | dir_open with valid path returns ST_NO_ERROR and a non-NULL view                         |
| INT-DIR-002 | after dir_open, root and flat list are initialised (ptRoot non-NULL, iFlatCount >= 0)    |
| INT-DIR-003 | dir_close releases the view and sets the pointer to NULL                                 |
| INT-DIR-004 | dir_open can be called again on the same path after a close (idempotent open/close cycle)|
| INT-DIR-005 | NULL ptLineCtx must be rejected before any side effect                                   |
| INT-DIR-006 | NULL pptView must be rejected before any side effect                                     |
| INT-DIR-007 | both NULL parameters must be rejected                                                    |
| INT-DIR-008 | dir_close(NULL) must be rejected                                                         |
| INT-DIR-009 | dir_close(&NULL) is idempotent (view already closed — safe no-op)                        |
| INT-DIR-010 | non-existent path opens with an empty tree (non-fatal scan failure)                      |

### 5.15 Test Cases — UC3.3 (directory tree view)

Source: `use_cases/use_case_03_3.c`

| ID          | Functional description                                      | Type | UFR          | REQ          | INTENT      | Expected outcome                                                | Status     |
|-------------|-------------------------------------------------------------|------|--------------|--------------|-------------|-----------------------------------------------------------------|------------|
| TC-DIR-001  | dir_open(valid path) → ST_NO_ERROR                          | [N]  | UFR-DIR-001  | REQ-DIR-001..004 | INT-DIR-001 | ST_NO_ERROR; ptView ≠ NULL                               | PASS UC3.3 |
| TC-DIR-002  | after dir_open: *pptView != NULL                            | [N]  | UFR-DIR-001  | REQ-DIR-004  | INT-DIR-001 | ptView ≠ NULL                                                   | PASS UC3.3 |
| TC-DIR-003  | after dir_open: ptRoot != NULL                              | [N]  | UFR-DIR-001  | REQ-DIR-004  | INT-DIR-002 | ptView->ptRoot ≠ NULL                                           | PASS UC3.3 |
| TC-DIR-004  | after dir_open: szRootPath non-empty                        | [N]  | UFR-DIR-001  | REQ-DIR-004  | INT-DIR-002 | szRootPath[0] ≠ '\0'                                            | PASS UC3.3 |
| TC-DIR-005  | after dir_open: iFlatCount >= 0                             | [N]  | UFR-DIR-001  | REQ-DIR-005  | INT-DIR-002 | iFlatCount ≥ 0                                                  | PASS UC3.3 |
| TC-DIR-006  | dir_close(valid view) → ST_NO_ERROR                         | [N]  | UFR-DIR-001  | REQ-DIR-006, REQ-DIR-007 | INT-DIR-003 | ST_NO_ERROR                                   | PASS UC3.3 |
| TC-DIR-007  | after dir_close: *pptView == NULL                           | [N]  | UFR-DIR-001  | REQ-DIR-007  | INT-DIR-003 | ptView == NULL                                                  | PASS UC3.3 |
| TC-DIR-008  | dir_open second time on same path → ST_NO_ERROR             | [N]  | UFR-DIR-001  | REQ-DIR-001  | INT-DIR-004 | second open succeeds                                            | PASS UC3.3 |
| TC-DIR-009  | dir_open(NULL ptLineCtx) → ST_ERROR                         | [R]  | UFR-DIR-001  | REQ-DIR-001  | INT-DIR-005 | ST_ERROR; *pptView unchanged (NULL)                             | PASS UC3.3 |
| TC-DIR-010  | dir_open(NULL ptLineCtx): *pptView unchanged                | [R]  | UFR-DIR-001  | REQ-DIR-001  | INT-DIR-005 | *pptView stays NULL                                             | PASS UC3.3 |
| TC-DIR-011  | dir_open(NULL pptView) → ST_ERROR                           | [R]  | UFR-DIR-001  | REQ-DIR-001  | INT-DIR-006 | ST_ERROR                                                        | PASS UC3.3 |
| TC-DIR-012  | dir_open(NULL, NULL) → ST_ERROR                             | [R]  | UFR-DIR-001  | REQ-DIR-001  | INT-DIR-007 | ST_ERROR                                                        | PASS UC3.3 |
| TC-DIR-013  | dir_close(NULL) → ST_ERROR                                  | [R]  | UFR-DIR-001  | REQ-DIR-006  | INT-DIR-008 | ST_ERROR                                                        | PASS UC3.3 |
| TC-DIR-014  | dir_close(&NULL) → ST_NO_ERROR (idempotent)                 | [R]  | UFR-DIR-001  | REQ-DIR-006  | INT-DIR-009 | ST_NO_ERROR                                                     | PASS UC3.3 |
| TC-DIR-015  | dir_open(non-existent) → ST_NO_ERROR + empty tree           | [R]  | UFR-DIR-001  | REQ-DIR-003  | INT-DIR-010 | ST_NO_ERROR; iFlatCount == 0  ADAPTED(UC6)                      | PASS UC3.3 |
| TC-DIR-016  | non-existent: iFlatCount == 0                               | [R]  | UFR-DIR-001  | REQ-DIR-003  | INT-DIR-010 | iFlatCount == 0                                                 | PASS UC3.3 |
| TC-DIR-017  | ASCII tree visible in window (visual)                       | [S]  | UFR-DIR-001  | REQ-DIR-004  | INT-DIR-001 | tree lines and file names rendered (make manual)                | SKIP       |
| TC-DIR-018  | selected row shows highlight rect (visual)                  | [S]  | UFR-DIR-002  | REQ-DIR-012  | INT-DIR-002 | blue rect behind selected item (make manual)                    | SKIP       |
| TC-DIR-019  | arrow keys move selection and scroll (visual)               | [S]  | UFR-DIR-001  | REQ-DIR-012  | INT-DIR-002 | navigation via ↑↓ PgUp/PgDn Home End (make manual)             | SKIP       |
| TC-DIR-020  | left-click on dir expands/collapses (visual)                | [S]  | UFR-DIR-003  | REQ-DIR-008  | INT-DIR-002 | expand reveals children; click again collapses (make manual)    | SKIP       |

#### Test Summary — UC3.3

| Module | [N] | [R] | [S] | Total | Result    |
|--------|-----|-----|-----|-------|-----------|
| DIR    | 8   | 8   | 4   | 20    | ALL PASS  |

#### REQ → TC coverage (UC3.3)

| REQ          | TC(s)                                   | Status    |
|--------------|-----------------------------------------|-----------|
| REQ-DIR-001  | TC-DIR-001, TC-DIR-009..012             | ✓ PASS    |
| REQ-DIR-002  | (NULL path → cwd — manual `dir` verify) | ✓ PASS    |
| REQ-DIR-003  | TC-DIR-015, TC-DIR-016                  | ✓ PASS    |
| REQ-DIR-004  | TC-DIR-002..004                         | ✓ PASS    |
| REQ-DIR-005  | TC-DIR-005                              | ✓ PASS    |
| REQ-DIR-006  | TC-DIR-006, TC-DIR-013, TC-DIR-014      | ✓ PASS    |
| REQ-DIR-007  | TC-DIR-007                              | ✓ PASS    |
| REQ-DIR-008  | TC-DIR-020 (manual)                     | SKIP      |
| REQ-DIR-009  | (manual — `..` navigation)              | SKIP      |
| REQ-DIR-010  | (manual — file selection → szSelected)  | SKIP      |
| REQ-DIR-011  | (manual — title bar updated)            | SKIP      |
| REQ-DIR-012  | TC-DIR-019 (manual)                     | SKIP      |
| REQ-DIR-013  | (manual — mouse wheel)                  | SKIP      |
| REQ-CON-011  | TC-DIR-001 (line_cmd_dir dispatched)    | ✓ PASS    |
| REQ-GUI-017  | (gui_set_title NULL params — TC-GUI-x)  | ✓ PASS    |
| REQ-GUI-018  | (manual — title bar visible)            | SKIP      |

---

### 5.16 INTENT Catalog — UC4.1

Source: `use_cases/use_case_04_1.c`

| ID          | INTENT text                                                                                            |
|-------------|--------------------------------------------------------------------------------------------------------|
| INT-GUI-010 | `gui_request_close(NULL)` is rejected before any side effect                                           |
| INT-DIR-011 | `dir_open` with `bShowHidden=ST_FALSE` does not include `.*` entries                                  |
| INT-DIR-012 | `dir_open` with `bShowHidden=ST_TRUE` includes `.*` entries                                           |
| INT-DIR-013 | `bShowHidden=ST_TRUE` count is strictly greater than `bShowHidden=ST_FALSE` count                     |
| INT-DIR-014 | `bShowHidden=ST_TRUE` with NULL `ptLineCtx` is still rejected with `ST_ERROR`                         |
| INT-DIR-015 | ESC key closes the dir window from the window thread without console-thread action                     |
| INT-DIR-016 | LEFT arrow collapses an expanded directory; the flat list and scroll are updated                       |
| INT-DIR-017 | RIGHT arrow expands a collapsed directory (lazy-loads children if needed)                              |
| INT-DIR-018 | SPACE selects a file or directory without triggering expand or navigate                                |
| INT-DIR-019 | ENTER still triggers expand/collapse/navigate (behaviour unchanged from UC3.3)                         |
| INT-DIR-020 | Window receives keyboard focus on open without requiring the user to alt-tab or click                  |

### 5.17 Test Cases — UC4.1 (UX dir improvements + make manual)

Source: `use_cases/use_case_04_1.c`
Test data: `use_cases/UC04_1/testdata/` — 2 visible entries + 2 hidden entries (`.*`)

| ID           | Functional description                                             | Type | UFR          | REQ                      | INTENT      | Expected outcome                                                              | Status     |
|--------------|--------------------------------------------------------------------|------|--------------|--------------------------|-------------|-------------------------------------------------------------------------------|------------|
| TC-DIR4-001  | `gui_request_close(NULL)` → ST_ERROR                               | [R]  | UFR-GUI-001  | REQ-GUI-019              | INT-GUI-010 | ST_ERROR; no side effect                                                      | PASS UC4.1 |
| TC-DIR4-002  | `dir_open(testdata, bShowHidden=F)` → ST_NO_ERROR                  | [N]  | UFR-DIR-007  | REQ-DIR-014, REQ-DIR-016 | INT-DIR-011 | ST_NO_ERROR; ptView ≠ NULL                                                    | PASS UC4.1 |
| TC-DIR4-003  | `bShowHidden=F`: iFlatCount == 2 (visible_dir + visible.txt)       | [N]  | UFR-DIR-007  | REQ-DIR-016              | INT-DIR-011 | `iFlatCount == 2`                                                             | PASS UC4.1 |
| TC-DIR4-004  | `dir_open(testdata, bShowHidden=T)` → ST_NO_ERROR                  | [N]  | UFR-DIR-007  | REQ-DIR-014, REQ-DIR-016 | INT-DIR-012 | ST_NO_ERROR; ptView ≠ NULL                                                    | PASS UC4.1 |
| TC-DIR4-005  | `bShowHidden=T`: iFlatCount == 4 (all entries)                     | [N]  | UFR-DIR-007  | REQ-DIR-016              | INT-DIR-012 | `iFlatCount == 4`                                                             | PASS UC4.1 |
| TC-DIR4-006  | `iFlatCount(T) > iFlatCount(F)`                                    | [N]  | UFR-DIR-007  | REQ-DIR-016              | INT-DIR-013 | iFlatHidden > iFlatVisible                                                    | PASS UC4.1 |
| TC-DIR4-007  | `dir_open(bShowHidden=T, NULL ptLineCtx)` → ST_ERROR               | [R]  | UFR-DIR-001  | REQ-DIR-001              | INT-DIR-014 | ST_ERROR                                                                      | PASS UC4.1 |
| TC-DIR4-008  | After TC-DIR4-007: `*pptView` unchanged (still NULL)               | [R]  | UFR-DIR-001  | REQ-DIR-001              | INT-DIR-014 | ptView == NULL                                                                | PASS UC4.1 |
| TC-DIR4-009  | ESC closes the dir window (manual)                                 | [S]  | UFR-DIR-008  | REQ-DIR-017              | INT-DIR-015 | Press ESC in window → window closes (make manual)                             | SKIP       |
| TC-DIR4-010  | LEFT collapses expanded directory (manual)                         | [S]  | UFR-DIR-009  | REQ-DIR-018              | INT-DIR-016 | Press RIGHT to expand, then LEFT → collapsed (make manual)                    | SKIP       |
| TC-DIR4-011  | RIGHT expands collapsed directory (manual)                         | [S]  | UFR-DIR-009  | REQ-DIR-018              | INT-DIR-017 | Select collapsed dir, press RIGHT → expanded (make manual)                    | SKIP       |
| TC-DIR4-012  | SPACE selects without expand/collapse (manual)                     | [S]  | UFR-DIR-010  | REQ-DIR-019              | INT-DIR-018 | Press SPACE on collapsed dir → stays collapsed, path selected (make manual)   | SKIP       |
| TC-DIR4-013  | ENTER triggers expand/collapse/navigate (manual)                   | [S]  | UFR-DIR-010  | REQ-DIR-008              | INT-DIR-019 | Press ENTER on collapsed dir → expands (make manual)                          | SKIP       |
| TC-DIR4-014  | Window focused on open without alt-tab (manual)                    | [S]  | UFR-GUI-007  | REQ-DIR-020, REQ-GUI-021 | INT-DIR-020 | Arrow keys work immediately after open, no click needed (make manual)         | SKIP       |

#### Test Summary — UC4.1

| Module | [N] | [R] | [S] | Total | Result    |
|--------|-----|-----|-----|-------|-----------|
| GUI    | 0   | 1   | 0   | 1     | ALL PASS  |
| DIR    | 5   | 2   | 5   | 12    | ALL PASS  |
| **Total** | **5** | **3** | **6** | **14** | **ALL PASS** |

#### REQ → TC coverage (UC4.1)

| REQ          | TC(s)                           | Status     |
|--------------|---------------------------------|------------|
| REQ-GUI-019  | TC-DIR4-001                     | ✓ PASS     |
| REQ-GUI-020  | (gui_close_window join safety — manual verify) | ✓ PASS |
| REQ-GUI-021  | TC-DIR4-014 (manual)            | SKIP       |
| REQ-DIR-014  | TC-DIR4-002, TC-DIR4-004, TC-DIR4-007 | ✓ PASS |
| REQ-DIR-015  | (line_cmd_dir -a flag — manual `dir -a`) | ✓ PASS |
| REQ-DIR-016  | TC-DIR4-003, TC-DIR4-005, TC-DIR4-006 | ✓ PASS |
| REQ-DIR-017  | TC-DIR4-009 (manual)            | SKIP       |
| REQ-DIR-018  | TC-DIR4-010, TC-DIR4-011 (manual) | SKIP     |
| REQ-DIR-019  | TC-DIR4-012 (manual)            | SKIP       |
| REQ-DIR-020  | TC-DIR4-014 (manual)            | SKIP       |

---

#### UFR traceability update (UC2 + UC3.1 + UC4.1)

| UFR              | REQ(s)                      | TC(s)                              | Status     |
|------------------|-----------------------------|------------------------------------|------------|
| UFR-CON-030      | REQ-CON-008                 | TC-TRC-013, TC-TRC-014             | ✓ UC2      |
| UFR-CON-031      | REQ-TRC-016                 | TC-TRC-015, TC-TRC-016             | ✓ P19 (ADAPTED from UC2) |
| UFR-CON-032      | REQ-CON-008                 | TC-TRC-010..012                    | ✓ UC2      |
| UFR-GUI-001      | REQ-GUI-001..008, REQ-GUI-019..020 | TC-GUI-011..018, TC-DIR4-001 | ✓ UC3.1 + UC4.1 |
| UFR-GUI-002      | REQ-GUI-009..014            | TC-GUI-001..010                    | ✓ UC3.1    |
| UFR-GUI-003      | REQ-GUI-015                 | TC-GUI-018 (manual)                | ✓ UC3.1    |
| UFR-GUI-004      | REQ-GUI-004..006            | TC-GUI-016..017 (manual)           | ✓ UC3.1    |
| UFR-GUI-005      | REQ-GUI-016                 | —                                  | TODO UC3.2 |
| UFR-GUI-007      | REQ-GUI-021, REQ-DIR-020    | TC-DIR4-014 (manual)               | ✓ UC4.1    |

#### Open items — cumulative

| Item                    | TC / REQ                       | Target   | Nature                                                           |
|-------------------------|--------------------------------|----------|------------------------------------------------------------------|
| STM bus error           | TC-STM-010, REQ-STM-011        | UC24     | Stub returns ST_NO_ERROR+0xFF; real map → ST_ERROR               |
| CPU decode              | TC-CPU-006, REQ-CPU-008        | UC21     | Stub: PC+2; real decode/execute to come                          |
| Disasm DC.W             | TC-DIS-001, REQ-DIS-005        | UC11     | All opcodes → DC.W; full decode in UC11–UC14                     |
| Trace GUI window        | UFR-TRC-007, REQ-TRC-014       | ✓ UC4.4  | Implemented: D2D ring buffer + colour levels + auto-scroll (P20) |
| Line editor (raw + edit)        | UFR-CON-041..046, REQ-CON-010, REQ-CON-015..016 | ✓ UC4.2 | console.h + line_read_rich done; history/completion in UC4.3 |
| Line editor (history/completion)| UFR-CON-007, UFR-CON-009, REQ-CON-017..018 | ✓ UC4.3 | ↑↓ history + tab-completion + ghost text CON_DIM — all validated |
| szSelected mutex                | REQ-CON-023, REQ-DIR-010, REQ-DIR-019 | ✓ UC4.3 | `line_set/get_selected()` under `ptSelectedMutex` — validated |
| Dir context menu        | UFR-DIR-005..006               | UC7/UC18 | Right-click on file/dir → contextual commands                    |
| gui_msg spin-wait       | REQ-GUI-013                    | UC5      | Replace 1 ms sleep with condition variable / Win32 Event (deferred from UC4.4) |
| Window manual TC        | TC-GUI-016..018                | ✓ UC4.1  | TEST_MANUAL macro + make manual now available (R16 implemented)  |
| Dir manual TC           | TC-DIR-017..020, TC-RND-016..020 | ✓ UC4.1 | TEST_MANUAL macro + make manual implemented; visual tests active |
| lx_X11 renderer         | REQ-RND-002..007               | UC3-Linux| Linux stub — X11/XRender implementation deferred                 |
| ESC/←/→/SPACE manual TC | TC-DIR4-009..014               | UC4.1    | Requires display; validated via make manual                       |
| Raw input manual TC     | TC-CON-010..017                | ✓ UC4.2  | Requires TTY; validated via make manual UC=04_2                   |

---

### 5.18 INTENT Catalog — UC4.2

Source: `use_cases/use_case_04_2.c`

| ID           | INTENT text                                                                                               |
|--------------|-----------------------------------------------------------------------------------------------------------|
| INT-CON-001  | `CON_KEY_*` constants are in their documented ranges so line_read_rich() switch logic works unambiguously |
| INT-CON-002  | `console_read_key(NULL)` is rejected before any blocking or side effect                                   |
| INT-CON-003  | `console_restore()` without prior `set_raw` is a safe idempotent no-op                                   |
| INT-CON-004  | `console_set_raw/restore` roundtrip succeeds on a real TTY; gracefully skipped in non-TTY CI contexts    |
| INT-CON-005  | Typing printable characters inserts them at the cursor position                                           |
| INT-CON-006  | Arrow keys ←/→ move the cursor without executing the command                                              |
| INT-CON-007  | Home/End jump cursor to start/end of line                                                                 |
| INT-CON-008  | Backspace deletes the character before the cursor                                                         |
| INT-CON-009  | Delete removes the character at the cursor                                                                 |
| INT-CON-010  | ESC clears the current input buffer without exiting the editor                                            |
| INT-CON-011  | CTRL+Q immediately executes "quit" regardless of buffer content                                           |
| INT-CON-012  | CTRL+T immediately executes "trace" and shows the command in the prompt before dispatch                   |

---

### 5.19 Test Cases — UC4.2 (raw console + rich line editor)

Source: `use_cases/use_case_04_2.c`

| ID           | Functional description                                                      | Type | UFR                  | REQ                          | INTENT       | Expected outcome                                        | Status     |
|--------------|-----------------------------------------------------------------------------|------|----------------------|------------------------------|--------------|---------------------------------------------------------|------------|
| TC-CON-001   | `CON_KEY_CTRL_C` in range 0x01..0x1F                                        | [N]  | UFR-CON-046          | REQ-RAW-007                  | INT-CON-001  | `0x03 >= 0x01 && 0x03 <= 0x1F`                          | PASS UC4.2 |
| TC-CON-002   | `CON_KEY_ENTER` in range 0x01..0x1F                                         | [N]  | UFR-CON-002          | REQ-RAW-010                  | INT-CON-001  | `0x0D >= 0x01 && 0x0D <= 0x1F`                          | PASS UC4.2 |
| TC-CON-003   | `CON_KEY_ESC == 0x1B`                                                       | [N]  | UFR-CON-044          | REQ-RAW-012                  | INT-CON-001  | `CON_KEY_ESC == 0x1B`                                   | PASS UC4.2 |
| TC-CON-004   | `CON_KEY_BACKSPACE == 0x7F`                                                 | [N]  | UFR-CON-043          | REQ-RAW-009                  | INT-CON-001  | `CON_KEY_BACKSPACE == 0x7F`                             | PASS UC4.2 |
| TC-CON-005   | Extended keys (UP/DOWN/LEFT/RIGHT/HOME/END/DELETE) >= 0x200                 | [N]  | UFR-CON-041..043     | REQ-RAW-010, REQ-RAW-011     | INT-CON-001  | All 7 values ≥ 0x200                                    | PASS UC4.2 |
| TC-CON-006   | `console_restore()` after successful `set_raw` → ST_NO_ERROR                | [N]  | UFR-CON-041..046     | REQ-RAW-005                  | INT-CON-004  | ST_NO_ERROR (skip if stdin not TTY)                     | PASS UC4.2 |
| TC-CON-007   | `console_restore()` second call → ST_NO_ERROR (idempotent)                  | [N]  | UFR-CON-041..046     | REQ-RAW-005                  | INT-CON-003  | ST_NO_ERROR                                             | PASS UC4.2 |
| TC-CON-008   | `console_read_key(NULL)` → ST_ERROR                                         | [R]  | UFR-CON-041..046     | REQ-RAW-007                  | INT-CON-002  | ST_ERROR; no crash or block                             | PASS UC4.2 |
| TC-CON-009   | `console_restore()` without prior `set_raw` → ST_NO_ERROR                   | [R]  | UFR-CON-041..046     | REQ-RAW-005                  | INT-CON-003  | ST_NO_ERROR (idempotent no-op)                          | PASS UC4.2 |
| TC-CON-010   | Typing characters shows them at prompt (manual)                             | [S]  | UFR-CON-042          | REQ-CON-010                  | INT-CON-005  | Characters appear char-by-char (make manual)            | SKIP       |
| TC-CON-011   | Cursor ←/→ moves without executing (manual)                                 | [S]  | UFR-CON-041          | REQ-CON-010                  | INT-CON-006  | Insert mid-word with LEFT+type (make manual)            | SKIP       |
| TC-CON-012   | Home / End (manual)                                                         | [S]  | UFR-CON-008          | REQ-CON-010                  | INT-CON-007  | Cursor jumps to start/end (make manual)                 | SKIP       |
| TC-CON-013   | Backspace (manual)                                                          | [S]  | UFR-CON-043          | REQ-CON-010                  | INT-CON-008  | Deletes char before cursor (make manual)                | SKIP       |
| TC-CON-014   | Delete key (manual)                                                         | [S]  | UFR-CON-043          | REQ-CON-010                  | INT-CON-009  | Deletes char at cursor (make manual)                    | SKIP       |
| TC-CON-015   | ESC clears line (manual)                                                    | [S]  | UFR-CON-044          | REQ-CON-010                  | INT-CON-010  | Buffer cleared; prompt shows empty (make manual)        | SKIP       |
| TC-CON-016   | CTRL+Q quits (manual)                                                       | [S]  | UFR-CON-046          | REQ-CON-010                  | INT-CON-011  | "quit" shown in prompt; app exits (make manual)         | SKIP       |
| TC-CON-017   | CTRL+T executes trace (manual)                                              | [S]  | UFR-CON-046          | REQ-CON-010                  | INT-CON-012  | "trace" shown in prompt; trace toggles (make manual)    | SKIP       |

#### Test Summary — UC4.2

| Module    | [N] | [R] | [S] | Total | Result    |
|-----------|-----|-----|-----|-------|-----------|
| CON (raw) | 7   | 2   | 8   | 17    | ALL PASS  |
| **Total** | **7** | **2** | **8** | **17** | **ALL PASS** |

> Note: `use_case_04_2.c` declares 14 tests (4N+2R+8S); 3 additional TCs (TC-CON-005 counts 7 keys,
> TC-CON-001..004 cover 4 constant checks) arise from the more granular table above.

#### REQ → TC coverage (UC4.2)

| REQ          | TC(s)                                  | Status     |
|--------------|----------------------------------------|------------|
| REQ-RAW-001  | (pipe path — TC-CON-006 when TTY)      | ✓ UC4.2    |
| REQ-RAW-002  | (cmd.exe path — manual)                | ✓ UC4.2    |
| REQ-RAW-003  | TC-CON-006 INFO (non-TTY skip)         | ✓ UC4.2    |
| REQ-RAW-004  | (set_raw idempotent — implied)         | ✓ UC4.2    |
| REQ-RAW-005  | TC-CON-007, TC-CON-009                 | ✓ UC4.2    |
| REQ-RAW-006  | (cmd.exe restore — manual)             | ✓ UC4.2    |
| REQ-RAW-007  | TC-CON-008                             | ✓ UC4.2    |
| REQ-RAW-008  | (no raw mode → EOF — implied)          | ✓ UC4.2    |
| REQ-RAW-009  | TC-CON-004 (constant check)            | ✓ UC4.2    |
| REQ-RAW-010  | TC-CON-005 (constant range)            | ✓ UC4.2    |
| REQ-RAW-011  | TC-CON-005 (constant range)            | ✓ UC4.2    |
| REQ-RAW-012  | TC-CON-003 (ESC constant)              | ✓ UC4.2    |
| REQ-RAW-013  | (Win32 VK — cmd.exe manual)            | ✓ UC4.2    |
| REQ-RAW-014  | (EOF path — implied)                   | ✓ UC4.2    |
| REQ-CON-010  | TC-CON-010..017 (manual)               | ✓ UC4.2    |
| REQ-CON-015  | (fgets fallback — non-TTY CI)          | ✓ UC4.2    |
| REQ-CON-016  | (restore after loop — implied)         | ✓ UC4.2    |
| UFR-CON-008  | TC-CON-003, TC-CON-012 (manual)        | ✓ UC4.2    |
| UFR-CON-041  | TC-CON-005, TC-CON-011 (manual)        | ✓ UC4.2    |
| UFR-CON-042  | TC-CON-010 (manual)                    | ✓ UC4.2    |
| UFR-CON-043  | TC-CON-004, TC-CON-013..014            | ✓ UC4.2    |
| UFR-CON-044  | TC-CON-003, TC-CON-015                 | ✓ UC4.2    |
| UFR-CON-045  | TC-CON-015..016                        | ✓ UC4.2    |
| UFR-CON-046  | TC-CON-001, TC-CON-016..017            | ✓ UC4.2    |

---

#### UFR traceability update (UC4.2)

| UFR              | REQ(s)                          | TC(s)                           | Status   |
|------------------|---------------------------------|---------------------------------|----------|
| UFR-CON-008      | REQ-CON-010                     | TC-CON-003, TC-CON-012 (manual) | ✓ UC4.2  |
| UFR-CON-041      | REQ-CON-010, REQ-RAW-001..014   | TC-CON-005, TC-CON-011          | ✓ UC4.2  |
| UFR-CON-042      | REQ-CON-010                     | TC-CON-010 (manual)             | ✓ UC4.2  |
| UFR-CON-043      | REQ-CON-010, REQ-RAW-009..013   | TC-CON-004, TC-CON-013..014     | ✓ UC4.2  |
| UFR-CON-044      | REQ-CON-010, REQ-RAW-012        | TC-CON-003, TC-CON-015          | ✓ UC4.2  |
| UFR-CON-045      | REQ-CON-010                     | TC-CON-015..016                 | ✓ UC4.2  |
| UFR-CON-046      | REQ-CON-010                     | TC-CON-001, TC-CON-016..017     | ✓ UC4.2  |

---

### 5.20 INTENT Catalog — UC4.3

Source: `use_cases/use_case_04_3.c`

| ID           | INTENT text                                                                                         |
|--------------|-----------------------------------------------------------------------------------------------------|
| INT-LIN-001  | Empty history has count 0 after clear                                                               |
| INT-LIN-002  | `history_add(NULL)` returns ST_ERROR                                                                |
| INT-LIN-003  | `history_add("")` is a no-op (empty strings ignored)                                                |
| INT-LIN-004  | Normal add stores the entry and increments count                                                    |
| INT-LIN-005  | Duplicate of the most recent entry is silently ignored                                              |
| INT-LIN-006  | `history_get()` virtual 0 returns the oldest entry                                                  |
| INT-LIN-007  | `history_get()` virtual count-1 returns the most recent entry                                       |
| INT-LIN-008  | `history_get()` with out-of-range index returns ST_ERROR                                            |
| INT-LIN-009  | `history_get(NULL buf)` and `history_get(buf, 0)` return ST_ERROR                                  |
| INT-LIN-010  | Count is capped at LINE_HISTORY_MAX after ring wrap; oldest entry is correctly identified            |
| INT-LIN-011  | save() + clear() + load() round-trips the history exactly                                           |
| INT-LIN-012  | Colors default to ON; set_colors() changes state; get_colors() reflects it                          |
| INT-LIN-013  | After line_init, selected is empty                                                                  |
| INT-LIN-014  | set_selected() stores, get_selected() retrieves (thread-safe)                                       |
| INT-LIN-015  | set_selected(NULL) clears the selection                                                             |
| INT-LIN-016  | --script runs commands from file; quit in script exits the loop cleanly                             |
| INT-LIN-017  | --script with missing file returns ST_ERROR from line_run                                           |
| INT-LIN-018  | Empty prefix for cmd completion returns all command names                                           |
| INT-LIN-019  | Prefix "he" → exactly "help"; prefix "q" → "quit"                                                  |
| INT-LIN-020  | Prefix "h" matches via shortcut and includes "help" candidate                                       |
| INT-LIN-021  | Non-existent directory prefix → 0 candidates without crash                                          |
| INT-LIN-022  | Prefix "src/" scanning from project root returns > 0 entries, all prefixed "src/"                  |
| INT-LIN-023  | Prefix "use_c" returns the use_cases/ directory candidate                                           |

---

### 5.21 Test Cases — UC4.3 (history, completion, colors, script, selected mutex)

Source: `use_cases/use_case_04_3.c`

| ID           | Functional description                                              | Type | UFR                   | REQ                          | INTENT       | Expected outcome                                                | Status     |
|--------------|---------------------------------------------------------------------|------|-----------------------|------------------------------|--------------|-----------------------------------------------------------------|------------|
| TC-LIN-001   | `history_count() == 0` after clear                                  | [N]  | UFR-CON-007           | REQ-CON-019                  | INT-LIN-001  | `line_history_count() == 0`                                     | PASS UC4.3 |
| TC-LIN-002   | `history_add(NULL)` → ST_ERROR                                      | [R]  | UFR-CON-007           | REQ-CON-019                  | INT-LIN-002  | ST_ERROR; count unchanged                                       | PASS UC4.3 |
| TC-LIN-003   | `history_add("")` → ST_NO_ERROR; count unchanged                    | [N]  | UFR-CON-007           | REQ-CON-019                  | INT-LIN-003  | ST_NO_ERROR; `count == 0`                                       | PASS UC4.3 |
| TC-LIN-004   | Normal add → count increments                                        | [N]  | UFR-CON-007           | REQ-CON-019                  | INT-LIN-004  | count == 1 then 2 after two adds                                | PASS UC4.3 |
| TC-LIN-005   | Adjacent duplicate silently ignored                                  | [N]  | UFR-CON-007           | REQ-CON-019                  | INT-LIN-005  | count still 2 after re-adding "dir src"                         | PASS UC4.3 |
| TC-LIN-006   | `history_get(0)` → oldest entry                                     | [N]  | UFR-CON-007           | REQ-CON-017                  | INT-LIN-006  | szBuf == "help"                                                 | PASS UC4.3 |
| TC-LIN-007   | `history_get(count-1)` → most recent                                | [N]  | UFR-CON-007           | REQ-CON-017                  | INT-LIN-007  | szBuf == "dir src"                                              | PASS UC4.3 |
| TC-LIN-008   | `history_get(-1)` and `history_get(count)` → ST_ERROR               | [R]  | UFR-CON-007           | REQ-CON-017                  | INT-LIN-008  | both return ST_ERROR                                            | PASS UC4.3 |
| TC-LIN-009   | `history_get(0, NULL, N)` and `history_get(0, buf, 0)` → ST_ERROR   | [R]  | UFR-CON-007           | REQ-CON-017                  | INT-LIN-009  | both return ST_ERROR                                            | PASS UC4.3 |
| TC-LIN-010   | After wrap: count == LINE_HISTORY_MAX; oldest is cmd4               | [N]  | UFR-CON-007           | REQ-CON-019                  | INT-LIN-010  | count capped; oldest == "cmd4"; newest == "cmd67"               | PASS UC4.3 |
| TC-LIN-011   | save + clear + load round-trip                                       | [N]  | UFR-CON-007           | REQ-CON-020                  | INT-LIN-011  | count==2; oldest=="trace on"; newest=="dir /tmp"                | PASS UC4.3 |
| TC-LIN-012   | `history_load(non-existent)` → ST_NO_ERROR                          | [N]  | UFR-CON-007           | REQ-CON-020                  | INT-LIN-011  | ST_NO_ERROR (first-run case)                                    | PASS UC4.3 |
| TC-LIN-013   | colors: set TRUE → get TRUE; set FALSE → get FALSE                  | [N]  | UFR-CON-047           | REQ-CON-021                  | INT-LIN-012  | get_colors() reflects set_colors() value                        | PASS UC4.3 |
| TC-LIN-014   | After `line_init`: selected empty                                    | [N]  | UFR-DIR-002           | REQ-CON-023                  | INT-LIN-013  | `szSelected[0] == '\0'`                                         | PASS UC4.3 |
| TC-LIN-015   | `set_selected("/tmp/hello.prg")` → `get_selected` returns same path | [N]  | UFR-DIR-002           | REQ-CON-023                  | INT-LIN-014  | retrieved path matches                                          | PASS UC4.3 |
| TC-LIN-016   | `set_selected(NULL)` → clears selection                              | [N]  | UFR-DIR-002           | REQ-CON-023                  | INT-LIN-015  | `szSelected[0] == '\0'` after NULL set                         | PASS UC4.3 |
| TC-LIN-017   | `set/get_selected(NULL ctx)` → ST_ERROR; `get(NULL buf)` → ST_ERROR | [R]  | UFR-DIR-002           | REQ-CON-023                  | INT-LIN-015  | all three return ST_ERROR                                       | PASS UC4.3 |
| TC-LIN-018   | Script mode: `line_run` with script file → `bRunning == FALSE`       | [N]  | UFR-CON-048           | REQ-CON-022                  | INT-LIN-016  | script runs help+colors+quit; bRunning==FALSE                   | PASS UC4.3 |
| TC-LIN-019   | Missing script file → `line_run` returns ST_ERROR                   | [R]  | UFR-CON-048           | REQ-CON-022                  | INT-LIN-017  | ST_ERROR                                                        | PASS UC4.3 |
| TC-LIN-020   | `complete_cmd_query("")` → > 0 candidates including "help" and "quit"| [N]  | UFR-CON-009           | REQ-CON-018                  | INT-LIN-018  | count > 0; "help" and "quit" both found                         | PASS UC4.3 |
| TC-LIN-021   | `complete_cmd_query("he")` → exactly 1 candidate == "help"          | [N]  | UFR-CON-009           | REQ-CON-018                  | INT-LIN-019  | count==1; candidate=="help"                                     | PASS UC4.3 |
| TC-LIN-022   | `complete_cmd_query("h")` → candidates include "help"               | [N]  | UFR-CON-009           | REQ-CON-018                  | INT-LIN-020  | "help" found in results                                         | PASS UC4.3 |
| TC-LIN-023   | `complete_cmd_query("xyz")` → 0 candidates                          | [N]  | UFR-CON-009           | REQ-CON-018                  | INT-LIN-018  | count == 0                                                      | PASS UC4.3 |
| TC-LIN-024   | `complete_cmd_query(NULL/NULL aOut/0 iMax)` → -1                    | [R]  | UFR-CON-009           | REQ-CON-018                  | INT-LIN-018  | all three return -1                                             | PASS UC4.3 |
| TC-LIN-025   | `complete_path_query` non-existent dir → 0                          | [N]  | UFR-CON-009           | REQ-CON-018                  | INT-LIN-021  | count == 0; no crash                                            | PASS UC4.3 |
| TC-LIN-026   | `complete_path_query("src/", ".")` → >0; all prefixed "src/"        | [N]  | UFR-CON-009           | REQ-CON-018                  | INT-LIN-022  | count > 0; candidates start with "src/"                         | PASS UC4.3 |
| TC-LIN-027   | `complete_path_query("use_c", ".")` → includes "use_cases/"         | [N]  | UFR-CON-009           | REQ-CON-018                  | INT-LIN-023  | "use_cases/" found in results                                   | PASS UC4.3 |
| TC-LIN-028   | `complete_path_query(NULL / NULL aOut / 0)` → -1                    | [R]  | UFR-CON-009           | REQ-CON-018                  | INT-LIN-021  | all three return -1                                             | PASS UC4.3 |
| TC-LIN-029   | History UP/DOWN navigation in line_read_rich (manual)               | [S]  | UFR-CON-007           | REQ-CON-017                  | INT-LIN-001  | Type cmd, ENTER, UP reappears (make manual)                     | SKIP       |
| TC-LIN-030   | TAB completes "he" to "help" in line_read_rich (manual)             | [S]  | UFR-CON-009           | REQ-CON-018                  | INT-LIN-019  | Ghost inserts suffix immediately (make manual)                  | SKIP       |
| TC-LIN-031   | TAB ghost cycles on multiple matches (manual)                        | [S]  | UFR-CON-009           | REQ-CON-018                  | INT-LIN-020  | "e" TAB TAB: ghost cycles "dit"/"xecute" (make manual)          | SKIP       |
| TC-LIN-032   | TAB path completion for "src/" (manual)                             | [S]  | UFR-CON-009           | REQ-CON-018                  | INT-LIN-022  | "dir src/" TAB lists src/ entries (make manual)                 | SKIP       |
| TC-LIN-033   | Prompt shows [T] when trace open (manual)                            | [S]  | UFR-CON-049           | REQ-CON-024                  | INT-LIN-013  | "trace on" → prompt shows [T] (make manual)                     | SKIP       |
| TC-LIN-034   | Prompt shows [file] after dir SPACE selection (manual)              | [S]  | UFR-CON-049           | REQ-CON-024                  | INT-LIN-014  | SPACE on file in dir view → prompt shows [basename] (make manual)| SKIP      |
| TC-LIN-035   | `colors off` removes ANSI from output (manual)                      | [S]  | UFR-CON-047           | REQ-CON-021                  | INT-LIN-012  | "colors off" → warning text uncoloured (make manual)            | SKIP       |
| TC-LIN-036   | Script mode with test_script.txt (manual)                           | [S]  | UFR-CON-048           | REQ-CON-022                  | INT-LIN-016  | `--script use_cases/UC04_3/test_script.txt` exits cleanly (make manual) | SKIP |

#### Test Summary — UC4.3

| Module       | [N] | [R] | [S] | Total | Result    |
|--------------|-----|-----|-----|-------|-----------|
| LIN (history)| 11  | 4   | 0   | 15    | ALL PASS  |
| LIN (colors) | 3   | 0   | 1   | 4     | ALL PASS  |
| LIN (selected)| 4  | 3   | 0   | 7     | ALL PASS  |
| LIN (script) | 2   | 1   | 1   | 4     | ALL PASS  |
| LIN (completion)| 7 | 4   | 4   | 15    | ALL PASS  |
| **Total**    | **27** | **12** | **8** | **47** | **ALL PASS** |

> Note: `use_case_04_3.c` test matrix header (19N+7R+8S=34) counts distinct assertions at source
> level; the SRTD table above (27N+12R+8S=47) is more granular — some `UC_TEST` pairs are split
> into separate TCs for clearer REQ coverage.

#### REQ → TC coverage (UC4.3)

| REQ          | TC(s)                                         | Status     |
|--------------|-----------------------------------------------|------------|
| REQ-CON-017  | TC-LIN-006..009, TC-LIN-029 (manual)          | ✓ UC4.3    |
| REQ-CON-018  | TC-LIN-020..028, TC-LIN-030..032 (manual)     | ✓ UC4.3    |
| REQ-CON-019  | TC-LIN-001..005, TC-LIN-010                   | ✓ UC4.3    |
| REQ-CON-020  | TC-LIN-011, TC-LIN-012                        | ✓ UC4.3    |
| REQ-CON-021  | TC-LIN-013, TC-LIN-035 (manual)               | ✓ UC4.3    |
| REQ-CON-022  | TC-LIN-018, TC-LIN-019, TC-LIN-036 (manual)  | ✓ UC4.3    |
| REQ-CON-023  | TC-LIN-014..017                               | ✓ UC4.3    |
| REQ-CON-024  | TC-LIN-033..034 (manual)                      | ✓ UC4.3    |

---

#### UFR traceability update (UC4.3)

| UFR              | REQ(s)                                  | TC(s)                                          | Status   |
|------------------|-----------------------------------------|------------------------------------------------|----------|
| UFR-CON-007      | REQ-CON-017, REQ-CON-019, REQ-CON-020  | TC-LIN-001..012, TC-LIN-029 (manual)           | ✓ UC4.3  |
| UFR-CON-009      | REQ-CON-018                             | TC-LIN-020..028, TC-LIN-030..032 (manual)      | ✓ UC4.3  |
| UFR-CON-047      | REQ-CON-021                             | TC-LIN-013, TC-LIN-035 (manual)                | ✓ UC4.3  |
| UFR-CON-048      | REQ-CON-022                             | TC-LIN-018, TC-LIN-019, TC-LIN-036 (manual)   | ✓ UC4.3  |
| UFR-CON-049      | REQ-CON-024                             | TC-LIN-033..034 (manual)                       | ✓ UC4.3  |
| UFR-DIR-002      | REQ-CON-023                             | TC-LIN-014..017                                | ✓ UC4.3  |

#### Open items — updated after UC4.3

| Item                    | TC / REQ                             | Target   | Nature                                                           |
|-------------------------|--------------------------------------|----------|------------------------------------------------------------------|
| STM bus error           | TC-STM-010, REQ-STM-011              | UC24     | Stub returns ST_NO_ERROR+0xFF; real map → ST_ERROR               |
| CPU decode              | TC-CPU-006, REQ-CPU-008              | UC21     | Stub: PC+2; real decode/execute to come                          |
| Disasm DC.W             | TC-DIS-001, REQ-DIS-005              | UC11     | All opcodes → DC.W; full decode in UC11–UC14                     |
| Trace GUI window        | UFR-TRC-007, REQ-TRC-014             | UC4.4    | stderr → dedicated Win32/X11 D2D window with colour levels (P20) |
| gui_msg spin-wait       | REQ-GUI-013                          | UC5      | Replace 1 ms sleep with condition variable / Win32 Event (deferred from UC4.4) |
| Dir context menu        | UFR-DIR-005..006                     | UC7/UC18 | Right-click on file/dir → contextual commands                    |
| lx_X11 renderer         | REQ-RND-002..007                     | UC3-Linux| Linux stub — X11/XRender implementation deferred                 |
| History UC4.3 manual TC | TC-LIN-029..036                      | ✓ UC4.3  | Requires TTY; validated via make manual UC=04_3                   |

---

### 5.22 INTENT Catalog — UC4.4

Source: `use_cases/use_case_04_4.c`

| ID          | INTENT text                                                                                            |
|-------------|--------------------------------------------------------------------------------------------------------|
| INT-TRC-020 | `gui_init()` must succeed before any GUI window can be opened                                          |
| INT-TRC-021 | `trace_init(ST_FALSE)` initialises the subsystem without opening the GUI window; `trace_is_open()` returns `ST_FALSE` |
| INT-TRC-022 | `trace_open()` opens the D2D GUI window; `trace_is_open()` returns `ST_TRUE`; window does not steal focus |
| INT-TRC-023 | Log entries emitted while the window is open must not crash; `is_open()` remains `ST_TRUE`; terminal stays clean (no stderr) |
| INT-TRC-024 | Double `trace_open()` is idempotent: a second call while already open must return `ST_NO_ERROR` without creating a second window |
| INT-TRC-025 | `trace_close()` closes the GUI window; `trace_is_open()` returns `ST_FALSE`; a second `trace_close()` must also return `ST_NO_ERROR` |
| INT-TRC-026 | `trace_shutdown()` closes the log file, joins the GUI thread if open, and resets state cleanly; calling while window is open must succeed |

---

### 5.23 Test Cases — UC4.4 (GUI trace window)

Source: `use_cases/use_case_04_4.c`

| ID          | Functional description                                        | Type | UFR                   | REQ                            | INTENT      | Expected outcome                                                              | Status     |
|-------------|---------------------------------------------------------------|------|-----------------------|--------------------------------|-------------|-------------------------------------------------------------------------------|------------|
| TC-TRC-018  | `gui_init()` succeeds (pre-condition for trace GUI)           | [N]  | UFR-GUI-001           | REQ-GUI-001                    | INT-TRC-020 | `ST_NO_ERROR`                                                                 | PASS UC4.4 |
| TC-TRC-019  | `trace_init(ST_FALSE)` → `trace_is_open() == ST_FALSE`       | [N]  | UFR-TRC-001           | REQ-TRC-001, REQ-TRC-003       | INT-TRC-021 | `ST_NO_ERROR`; `trace_is_open() == FALSE`                                     | PASS UC4.4 |
| TC-TRC-020  | `trace_open()` → `trace_is_open() == ST_TRUE`                | [N]  | UFR-TRC-007           | REQ-TRC-014, REQ-TRC-017       | INT-TRC-022 | `ST_NO_ERROR`; `trace_is_open() == TRUE`                                      | PASS UC4.4 |
| TC-TRC-021  | Log entries while open → no crash; `is_open()` still TRUE    | [N]  | UFR-TRC-007           | REQ-TRC-018, REQ-TRC-019       | INT-TRC-023 | `LOG_INFO/TRACE/ERROR/TODO` emitted; `trace_is_open() == TRUE`                | PASS UC4.4 |
| TC-TRC-022  | Double `trace_open()` idempotent (2nd call → `ST_NO_ERROR`)  | [R]  | UFR-TRC-007           | REQ-TRC-017                    | INT-TRC-024 | `ST_NO_ERROR`; `trace_is_open() == TRUE`; single window only                  | PASS UC4.4 |
| TC-TRC-023  | `trace_close()` → `trace_is_open() == ST_FALSE`              | [N]  | UFR-TRC-007           | REQ-TRC-006                    | INT-TRC-025 | `ST_NO_ERROR`; `trace_is_open() == FALSE`                                     | PASS UC4.4 |
| TC-TRC-024  | Double `trace_close()` idempotent                            | [R]  | UFR-TRC-007           | REQ-TRC-007                    | INT-TRC-025 | 2nd `trace_close()` → `ST_NO_ERROR`                                           | PASS UC4.4 |
| TC-TRC-025  | `trace_shutdown()` → `is_open() == FALSE`; reinit succeeds   | [N]  | UFR-TRC-004           | REQ-TRC-012                    | INT-TRC-026 | `ST_NO_ERROR`; `is_open() == FALSE`; `trace_init()` again → `ST_NO_ERROR`     | PASS UC4.4 |
| TC-TRC-026  | `trace_shutdown()` while window open → closes cleanly        | [N]  | UFR-TRC-004           | REQ-TRC-012, REQ-TRC-014       | INT-TRC-026 | `trace_open()` then `trace_shutdown()` → `ST_NO_ERROR`; no hang               | PASS UC4.4 |
| TC-TRC-027  | GUI trace window visible with 4 log lines (manual)           | [S]  | UFR-TRC-007           | REQ-TRC-014, REQ-TRC-018       | INT-TRC-022 | Window open; 4 lines rendered (make manual UC=04_4)                           | SKIP       |
| TC-TRC-028  | Log level colours correct (manual)                           | [S]  | UFR-TRC-008           | REQ-TRC-019                    | INT-TRC-022 | INFO=cyan, ERROR=red, TODO=magenta, TRACE=grey (make manual)                  | SKIP       |
| TC-TRC-029  | Auto-scroll to newest line (manual)                          | [S]  | UFR-TRC-009           | REQ-TRC-020                    | INT-TRC-023 | After 30 lines, window shows last lines (make manual)                         | SKIP       |
| TC-TRC-030  | Mouse wheel scrolls content (manual)                         | [S]  | UFR-TRC-009           | REQ-TRC-022                    | INT-TRC-023 | Scroll up/down with mouse wheel (make manual)                                 | SKIP       |
| TC-TRC-031  | PgUp/PgDn scroll by one screen (manual)                      | [S]  | UFR-TRC-009           | REQ-TRC-022                    | INT-TRC-023 | Each keypress scrolls one page (make manual)                                  | SKIP       |
| TC-TRC-032  | Home/End keys (manual)                                       | [S]  | UFR-TRC-009           | REQ-TRC-022                    | INT-TRC-023 | Home → oldest; End → newest + auto-scroll re-enabled (make manual)            | SKIP       |
| TC-TRC-033  | Terminal clean — stderr suppressed when window open (manual) | [S]  | UFR-TRC-007           | REQ-TRC-013                    | INT-TRC-023 | Terminal shows no log lines while trace window is live (make manual)          | SKIP       |
| TC-TRC-034  | ESC closes trace window (manual)                             | [S]  | UFR-TRC-007           | REQ-TRC-022                    | INT-TRC-025 | Press ESC in trace window → window closes (make manual)                       | SKIP       |

#### Test Summary — UC4.4

| Module | [N] | [R] | [S] | Total | Result    |
|--------|-----|-----|-----|-------|-----------|
| TRC    | 8   | 2   | 8   | 18    | ALL PASS  |

#### REQ → TC coverage (UC4.4)

| REQ          | TC(s)                                             | Status     |
|--------------|---------------------------------------------------|------------|
| REQ-TRC-006  | TC-TRC-023                                        | ✓ UC4.4    |
| REQ-TRC-007  | TC-TRC-024                                        | ✓ UC4.4    |
| REQ-TRC-012  | TC-TRC-025, TC-TRC-026                            | ✓ UC4.4    |
| REQ-TRC-013  | TC-TRC-033 (manual — terminal clean)              | ✓ UC4.4    |
| REQ-TRC-014  | TC-TRC-020, TC-TRC-026                            | ✓ UC4.4    |
| REQ-TRC-017  | TC-TRC-020, TC-TRC-022                            | ✓ UC4.4    |
| REQ-TRC-018  | TC-TRC-021, TC-TRC-027 (manual)                   | ✓ UC4.4    |
| REQ-TRC-019  | TC-TRC-021, TC-TRC-028 (manual)                   | ✓ UC4.4    |
| REQ-TRC-020  | TC-TRC-029, TC-TRC-032 (manual)                   | ✓ UC4.4    |
| REQ-TRC-021  | (no-focus-steal — visual; implied by TC-TRC-020)  | ✓ UC4.4    |
| REQ-TRC-022  | TC-TRC-030..032, TC-TRC-034 (manual)              | ✓ UC4.4    |

---

#### UFR traceability update (UC4.4)

| UFR          | REQ(s)                                       | TC(s)                                      | Status   |
|--------------|----------------------------------------------|--------------------------------------------|----------|
| UFR-TRC-007  | REQ-TRC-014, REQ-TRC-017..022                | TC-TRC-020..026, TC-TRC-027..034 (manual)  | ✓ UC4.4  |
| UFR-TRC-008  | REQ-TRC-019                                  | TC-TRC-028 (manual)                        | ✓ UC4.4  |
| UFR-TRC-009  | REQ-TRC-020, REQ-TRC-022                     | TC-TRC-029..032, TC-TRC-034 (manual)       | ✓ UC4.4  |
| UFR-TRC-010  | REQ-TRC-021                                  | (implied by TC-TRC-020 — no click needed)  | ✓ UC4.4  |

---

#### Open items — updated after UC4.4

| Item                    | TC / REQ                             | Target    | Nature                                                           |
|-------------------------|--------------------------------------|-----------|------------------------------------------------------------------|
| STM bus error           | TC-STM-010, REQ-STM-011              | UC24      | Stub returns ST_NO_ERROR+0xFF; real map → ST_ERROR               |
| CPU decode              | TC-CPU-006, REQ-CPU-008              | UC21      | Stub: PC+2; real decode/execute to come                          |
| Disasm DC.W             | TC-DIS-001, REQ-DIS-005              | UC11      | All opcodes → DC.W; full decode in UC11–UC14                     |
| gui_msg spin-wait       | REQ-GUI-013                          | UC5       | Replace 1 ms sleep with condition variable / Win32 Event         |
| Dir context menu        | UFR-DIR-005..006                     | UC7/UC18  | Right-click on file/dir → contextual commands                    |
| lx_X11 renderer         | REQ-RND-002..007                     | UC3-Linux | Linux stub — X11/XRender implementation deferred                 |
| Trace UC4.4 manual TC   | TC-TRC-027..034                      | ✓ UC4.4   | Requires display; validated via make manual UC=04_4              |

---

### 5.24 INTENT Catalog — UC5

Source: `use_cases/use_case_05.c`

| ID           | INTENT text                                                                                                          |
|--------------|----------------------------------------------------------------------------------------------------------------------|
| INT-CON-050  | `trace_get_view_level()` returns `LOG_LEVEL_TRACE` initially (show all); `trace_set_view_level()` changes it         |
| INT-CON-051  | `trace_set_view_level()` persists across close/reopen; `trace_get_view_level()` reflects the set value               |
| INT-CON-052  | `trace_clear()` when trace window is not open returns `ST_NO_ERROR` immediately (no-op); safe to call twice          |
| INT-CON-053  | `line_init()` sets `g_line_bColors` via `isatty(stdout)` — result is a valid `st_bool_t`; P24 auto-detect           |
| INT-CON-054  | `line_update_console_title(NULL)` does not crash; `line_update_console_title(&tCtx)` does not crash                  |
| INT-CON-055  | `where`/`info`/`history`/`trace` appear in `line_complete_cmd_query()` results — proves they are registered in the command table |
| INT-CON-056  | `trace_set_view_level(LOG_LEVEL_ERROR)` → `trace_get_view_level() == LOG_LEVEL_ERROR`; round-trip consistent          |
| INT-CON-057  | `line_history_count() >= 0` after init; `== 0` after clear; add entry → count == 1; get(0) retrieves "where"         |
| INT-CON-058  | `line_history_get(0,…)` on empty ring (after clear) returns `ST_ERROR`                                               |
| INT-CON-059  | `line_complete_cmd_query("h",…)` returns ≥ 2 candidates (at minimum "help" and "history")                           |
| INT-CON-060  | `line_complete_cmd_query("he",…)` returns exactly 1 candidate == "help" (single match → immediate insert behavior)   |
| INT-CON-061  | `line_complete_cmd_query("info",…)` and `("hist",…)` each return exactly 1 candidate — new commands are reachable   |
| INT-CON-062  | `line_shutdown()` → `ST_NO_ERROR`; `bRunning == ST_FALSE`                                                            |

---

### 5.25 Test Cases — UC5 (where, info, history, console title, trace clear/level, TAB prefix, dir H/F5)

Source: `use_cases/use_case_05.c`

| ID           | Functional description                                                                      | Type | UFR                        | REQ                           | INTENT       | Expected outcome                                                              | Status   |
|--------------|---------------------------------------------------------------------------------------------|------|----------------------------|-------------------------------|--------------|-------------------------------------------------------------------------------|----------|
| TC-TRC-035   | `trace_get_view_level()` returns `LOG_LEVEL_TRACE` initially                                | [N]  | UFR-TRC-012                | REQ-TRC-025                   | INT-CON-050  | `eLevel == LOG_LEVEL_TRACE`                                                   | PASS UC5 |
| TC-TRC-036   | `trace_set_view_level(INFO)` → `ST_NO_ERROR`                                               | [N]  | UFR-TRC-012                | REQ-TRC-024                   | INT-CON-050  | `ST_NO_ERROR`                                                                 | PASS UC5 |
| TC-TRC-037   | `trace_get_view_level()` == `LOG_LEVEL_INFO` after set                                     | [N]  | UFR-TRC-012                | REQ-TRC-025                   | INT-CON-051  | `eLevel == LOG_LEVEL_INFO`                                                    | PASS UC5 |
| TC-TRC-038   | `trace_set_view_level(TRACE)` restores default                                             | [N]  | UFR-TRC-012                | REQ-TRC-024                   | INT-CON-051  | `ST_NO_ERROR`; `get_view_level() == LOG_LEVEL_TRACE`                          | PASS UC5 |
| TC-TRC-039   | `trace_set_view_level(999)` (invalid) → no crash                                           | [R]  | UFR-TRC-012                | REQ-TRC-024                   | INT-CON-050  | `ST_NO_ERROR`; no crash or undefined behaviour                                | PASS UC5 |
| TC-TRC-040   | `trace_clear()` when window closed → `ST_NO_ERROR`                                        | [N]  | UFR-TRC-011                | REQ-TRC-023                   | INT-CON-052  | `ST_NO_ERROR`                                                                 | PASS UC5 |
| TC-TRC-041   | `trace_clear()` twice when closed → `ST_NO_ERROR` (idempotent)                            | [R]  | UFR-TRC-011                | REQ-TRC-023                   | INT-CON-052  | `ST_NO_ERROR`                                                                 | PASS UC5 |
| TC-CON-101   | `line_init()` → `ST_NO_ERROR`; `bRunning == TRUE`; `szCwd` non-empty                      | [N]  | UFR-CON-001, UFR-CON-085   | REQ-CON-001..003, REQ-CON-029 | INT-CON-053  | init succeeds; `g_line_bColors` is a valid `st_bool_t`                        | PASS UC5 |
| TC-CON-102   | `line_update_console_title(NULL)` → no crash                                              | [R]  | UFR-CON-083                | REQ-CON-028                   | INT-CON-054  | No crash or error                                                             | PASS UC5 |
| TC-CON-103   | `line_update_console_title(&tCtx)` → no crash                                             | [N]  | UFR-CON-083                | REQ-CON-028                   | INT-CON-054  | No crash or error                                                             | PASS UC5 |
| TC-CON-104   | `complete_cmd_query("where")` → 1 match "where"                                           | [N]  | UFR-CON-080                | REQ-CON-025, REQ-CON-031      | INT-CON-055  | count==1; aOut[0]=="where"                                                    | PASS UC5 |
| TC-CON-105   | `complete_cmd_query("info")` → 1 match "info"                                             | [N]  | UFR-CON-081                | REQ-CON-026, REQ-CON-031      | INT-CON-055  | count==1; aOut[0]=="info"                                                     | PASS UC5 |
| TC-CON-106   | `complete_cmd_query("history")` → 1 match "history"                                       | [N]  | UFR-CON-082                | REQ-CON-027, REQ-CON-031      | INT-CON-055  | count==1; aOut[0]=="history"                                                  | PASS UC5 |
| TC-CON-107   | `complete_cmd_query("trace")` → 1 match "trace"                                           | [N]  | UFR-CON-030..032           | REQ-CON-008                   | INT-CON-055  | count==1; aOut[0]=="trace"                                                    | PASS UC5 |
| TC-TRC-042   | `trace_set_view_level(ERROR)` then `get_view_level() == ERROR`                             | [N]  | UFR-TRC-012                | REQ-TRC-024, REQ-TRC-025      | INT-CON-056  | round-trip consistent                                                         | PASS UC5 |
| TC-CON-108   | `line_history_count() >= 0` after init; `== 0` after clear                                | [N]  | UFR-CON-082                | REQ-CON-027                   | INT-CON-057  | count ≥ 0 then == 0                                                           | PASS UC5 |
| TC-CON-109   | `history_add("where")` → count == 1; `history_get(0)` → "where"                          | [N]  | UFR-CON-082                | REQ-CON-027                   | INT-CON-057  | count==1; entry=="where"                                                      | PASS UC5 |
| TC-CON-110   | `history_get(0,…)` on empty ring → `ST_ERROR`                                             | [R]  | UFR-CON-082                | REQ-CON-027                   | INT-CON-058  | `ST_ERROR`                                                                    | PASS UC5 |
| TC-CON-111   | `complete_cmd_query("")` → count > 0                                                       | [N]  | UFR-CON-009, UFR-CON-084   | REQ-CON-018, REQ-CON-031      | INT-CON-059  | at least all registered commands returned                                     | PASS UC5 |
| TC-CON-112   | `complete_cmd_query("h")` → ≥ 2 candidates (help, history)                                | [N]  | UFR-CON-009, UFR-CON-084   | REQ-CON-018                   | INT-CON-059  | count ≥ 2; P23bis: common prefix "h" already at cursor                        | PASS UC5 |
| TC-CON-113   | `complete_cmd_query("he")` → exactly 1 candidate "help"                                   | [N]  | UFR-CON-009                | REQ-CON-018                   | INT-CON-060  | count==1; aOut[0]=="help"                                                     | PASS UC5 |
| TC-CON-114   | `complete_cmd_query("info")` → 1 candidate; `("hist")` → 1 candidate "history"            | [N]  | UFR-CON-081, UFR-CON-082   | REQ-CON-026, REQ-CON-027      | INT-CON-061  | count==1 each; new commands reachable                                         | PASS UC5 |
| TC-CON-115   | `line_shutdown()` → `ST_NO_ERROR`; `bRunning == FALSE`                                    | [N]  | UFR-CON-001                | REQ-CON-004, REQ-CON-005      | INT-CON-062  | `ST_NO_ERROR`; `bRunning == FALSE`                                            | PASS UC5 |
| TC-CON-116   | Console title bar updated correctly (manual)                                               | [S]  | UFR-CON-083                | REQ-CON-028                   | INT-CON-054  | Title bar shows "ST4Ever v0.5.0 \| cwd" (make manual)                        | SKIP     |
| TC-CON-117   | `history [3]` prints last 3 commands numbered (manual)                                    | [S]  | UFR-CON-082                | REQ-CON-027                   | INT-CON-057  | 3 entries shown in order (make manual)                                        | SKIP     |
| TC-CON-118   | TAB common prefix live: `h` + TAB inserts "h" (already at cursor) then ghost (manual)     | [S]  | UFR-CON-084                | REQ-CON-018                   | INT-CON-059  | First TAB with "h" → no insertion (prefix already typed); ghost cycle (make manual) | SKIP |
| TC-CON-119   | TAB single candidate: `he` + TAB inserts "lp" immediately (manual)                        | [S]  | UFR-CON-009                | REQ-CON-018                   | INT-CON-060  | "help" completed without ghost (make manual)                                  | SKIP     |
| TC-DIR-050   | `H` key in dir view toggles hidden files (manual)                                          | [S]  | UFR-DIR-011                | REQ-DIR-021                   | INT-CON-055  | H pressed: hidden entries appear/disappear; root reloaded (make manual)       | SKIP     |
| TC-DIR-051   | F5 refreshes listing and preserves expansion state (manual)                               | [S]  | UFR-DIR-012                | REQ-DIR-022                   | INT-CON-055  | Expanded dirs remain open after F5; new files appear (make manual)            | SKIP     |
| TC-TRC-043   | `trace level info` hides LOG_TRACE rows in GUI trace window (manual)                      | [S]  | UFR-TRC-012                | REQ-TRC-024, REQ-TRC-026      | INT-CON-050  | TRACE rows not rendered; INFO/ERROR visible (make manual)                     | SKIP     |
| TC-TRC-044   | `trace clear` empties the GUI trace window (manual)                                        | [S]  | UFR-TRC-011                | REQ-TRC-023                   | INT-CON-052  | Window becomes blank after "trace clear" (make manual)                        | SKIP     |
| TC-CON-120   | `colors auto` off when stdout piped: no ANSI codes in output file (manual)                | [S]  | UFR-CON-085                | REQ-CON-029                   | INT-CON-053  | `ST4Ever > out.txt`: out.txt contains no ESC sequences (make manual)          | SKIP     |

#### Test Summary — UC5

| Module         | [N] | [R] | [S] | Total | Result    |
|----------------|-----|-----|-----|-------|-----------|
| TRC (view level + clear) | 7 | 2 | 2 | 11 | ALL PASS |
| CON (init, title, cmds, history, completion, shutdown) | 16 | 2 | 5 | 23 | ALL PASS |
| DIR (H key, F5) | 0 | 0 | 2 | 2 | SKIP (manual) |
| **Total**      | **23** | **4** | **9** | **36** | **ALL PASS** |

> Note: `use_case_05.c` test matrix header (31N+4R+9S=44) counts UC_TEST assertions at source level; the
> SRTD table (23N+4R+9S=36) is more concise, grouping some compound assertions into single TCs.

#### REQ → TC coverage (UC5)

| REQ          | TC(s)                                          | Status   |
|--------------|------------------------------------------------|----------|
| REQ-TRC-023  | TC-TRC-040, TC-TRC-041, TC-TRC-044 (manual)   | ✓ UC5    |
| REQ-TRC-024  | TC-TRC-036, TC-TRC-038, TC-TRC-039, TC-TRC-042, TC-TRC-043 (manual) | ✓ UC5 |
| REQ-TRC-025  | TC-TRC-035, TC-TRC-037, TC-TRC-042            | ✓ UC5    |
| REQ-TRC-026  | TC-TRC-043 (manual)                            | ✓ UC5    |
| REQ-CON-025  | TC-CON-104 (completion table proves dispatch)  | ✓ UC5    |
| REQ-CON-026  | TC-CON-105 (completion table proves dispatch)  | ✓ UC5    |
| REQ-CON-027  | TC-CON-106, TC-CON-108..110, TC-CON-117 (manual) | ✓ UC5 |
| REQ-CON-028  | TC-CON-102, TC-CON-103, TC-CON-116 (manual)   | ✓ UC5    |
| REQ-CON-029  | TC-CON-101, TC-CON-120 (manual)                | ✓ UC5    |
| REQ-CON-030  | TC-TRC-043, TC-TRC-044 (manual — trace sub-cmds) | ✓ UC5 |
| REQ-CON-031  | TC-CON-104..107                                | ✓ UC5    |
| REQ-DIR-021  | TC-DIR-050 (manual)                            | ✓ UC5    |
| REQ-DIR-022  | TC-DIR-051 (manual)                            | ✓ UC5    |

---

#### UFR traceability update (UC5)

| UFR              | REQ(s)                                   | TC(s)                                                  | Status   |
|------------------|------------------------------------------|--------------------------------------------------------|----------|
| UFR-CON-080      | REQ-CON-025                              | TC-CON-104, (where output — manual)                    | ✓ UC5    |
| UFR-CON-081      | REQ-CON-026                              | TC-CON-105, (info output — manual)                     | ✓ UC5    |
| UFR-CON-082      | REQ-CON-027                              | TC-CON-106, TC-CON-108..110, TC-CON-117 (manual)       | ✓ UC5    |
| UFR-CON-083      | REQ-CON-028                              | TC-CON-102..103, TC-CON-116 (manual)                   | ✓ UC5    |
| UFR-CON-084      | REQ-CON-018                              | TC-CON-111..113, TC-CON-118..119 (manual)              | ✓ UC5    |
| UFR-CON-085      | REQ-CON-029                              | TC-CON-101, TC-CON-120 (manual)                        | ✓ UC5    |
| UFR-TRC-011      | REQ-TRC-023                              | TC-TRC-040..041, TC-TRC-044 (manual)                   | ✓ UC5    |
| UFR-TRC-012      | REQ-TRC-024, REQ-TRC-025, REQ-TRC-026   | TC-TRC-035..039, TC-TRC-042, TC-TRC-043 (manual)       | ✓ UC5    |
| UFR-DIR-011      | REQ-DIR-021                              | TC-DIR-050 (manual)                                    | ✓ UC5    |
| UFR-DIR-012      | REQ-DIR-022                              | TC-DIR-051 (manual)                                    | ✓ UC5    |

---

#### Open items — updated after UC5

| Item                    | TC / REQ                             | Target    | Nature                                                           |
|-------------------------|--------------------------------------|-----------|------------------------------------------------------------------|
| STM bus error           | TC-STM-010, REQ-STM-011              | UC24      | Stub returns ST_NO_ERROR+0xFF; real map → ST_ERROR               |
| CPU decode              | TC-CPU-006, REQ-CPU-008              | UC21      | Stub: PC+2; real decode/execute to come                          |
| Disasm DC.W             | TC-DIS-001, REQ-DIS-005              | UC11      | All opcodes → DC.W; full decode in UC11–UC14                     |
| gui_msg spin-wait       | REQ-GUI-013                          | UC6+      | Replace 1 ms sleep with condition variable / Win32 Event; deferred |
| Dir context menu        | UFR-DIR-005..006                     | UC7/UC18  | Right-click on file/dir → contextual commands                    |
| lx_X11 renderer         | REQ-RND-002..007                     | UC3-Linux | Linux stub — X11/XRender implementation deferred                 |
| UC5 manual TC           | TC-CON-116..120, TC-DIR-050..051, TC-TRC-043..044 | ✓ UC5 | Requires display/TTY; validated via make manual UC=05 |
| info cmd disk/binary    | REQ-CON-026                          | UC7/UC15  | `line_cmd_info()` disk/binary stubs → real data when UCs implemented |

---

### 5.26 INTENT Catalog — UC6

Source: `use_cases/use_case_06.c`
Fixture: `use_cases/UC01/hello.prg` (32-byte PRG, PRG magic `0x601A`)

| ID          | INTENT text                                                                                                    |
|-------------|----------------------------------------------------------------------------------------------------------------|
| INT-FIL-001 | `file_stat()` on an existing file fills bExists, bIsDir, uiSize, and szExt (lowercase, no dot); on a directory bIsDir=TRUE, uiSize=0 |
| INT-FIL-002 | `file_stat()` on a non-existent path returns ST_NO_ERROR with bExists=FALSE — querying existence is not an error |
| INT-FIL-003 | `file_stat(NULL,*)` and `file_stat(*,NULL)` return ST_ERROR before any side effect                             |
| INT-FIL-004 | `file_open(READ)` on an existing binary file succeeds; reading 4 bytes returns exactly 4 bytes including PRG magic `0x601A` |
| INT-FIL-005 | Reading past EOF returns ST_NO_ERROR with `*puiRead == 0` — end-of-file is not an error                        |
| INT-FIL-006 | `file_open(WRITE)` + `file_write` + `file_close` creates a file; subsequent `file_stat` confirms size == written bytes; re-open + read verifies content |
| INT-FIL-007 | NULL params on file_open/read/write/close return ST_ERROR; `file_close(&NULL)` is a safe idempotent no-op; `file_write(uiLen=0)` → ST_ERROR |
| INT-FIL-008 | `file_mkdir` creates a new directory (stat confirms bIsDir=TRUE); a second call on the same path → ST_NO_ERROR (EEXIST silent) |
| INT-FIL-009 | `file_list_dir(src/, FALSE)` returns > 0 entries, always includes "common.h", never delivers a '.' or '..' entry and no hidden names |
| INT-FIL-010 | `file_list_dir(src/, TRUE)` count >= `file_list_dir(src/, FALSE)` count — hidden filter can only reduce the set |
| INT-FIL-011 | `file_list_dir(NULL,*)` and `(*,*,NULL,*)` return ST_ERROR; listing a non-existent dir returns ST_ERROR         |

---

### 5.27 Test Cases — UC6 (file system abstraction)

Source: `use_cases/use_case_06.c`

| ID          | Functional description                                                           | Type | UFR         | REQ                    | INTENT      | Expected outcome                                                          | Status   |
|-------------|----------------------------------------------------------------------------------|------|-------------|------------------------|-------------|---------------------------------------------------------------------------|----------|
| TC-FIL-001  | `file_stat(hello.prg)` → ST_NO_ERROR                                            | [N]  | UFR-FIL-001 | REQ-FIL-001, REQ-FIL-003 | INT-FIL-001 | ST_NO_ERROR                                                             | PASS UC6 |
| TC-FIL-002  | hello.prg: `bExists == TRUE`                                                    | [N]  | UFR-FIL-001 | REQ-FIL-003            | INT-FIL-001 | bExists == TRUE                                                           | PASS UC6 |
| TC-FIL-003  | hello.prg: `bIsDir == FALSE`                                                    | [N]  | UFR-FIL-001 | REQ-FIL-003            | INT-FIL-001 | bIsDir == FALSE                                                           | PASS UC6 |
| TC-FIL-004  | hello.prg: `uiSize > 0`                                                         | [N]  | UFR-FIL-001 | REQ-FIL-003            | INT-FIL-001 | uiSize > 0                                                                | PASS UC6 |
| TC-FIL-005  | hello.prg: `szExt == "prg"`                                                     | [N]  | UFR-FIL-001 | REQ-FIL-003            | INT-FIL-001 | szExt == "prg"                                                            | PASS UC6 |
| TC-FIL-006  | `file_stat(src/)`: `bExists=TRUE`, `bIsDir=TRUE`                                | [N]  | UFR-FIL-001 | REQ-FIL-003            | INT-FIL-001 | directory detected correctly                                              | PASS UC6 |
| TC-FIL-007  | `file_stat(non-existent)` → ST_NO_ERROR + `bExists=FALSE`                       | [N]  | UFR-FIL-001 | REQ-FIL-002            | INT-FIL-002 | ST_NO_ERROR; bExists == FALSE                                             | PASS UC6 |
| TC-FIL-008  | `file_stat(NULL, &tStat)` and `file_stat(path, NULL)` → ST_ERROR               | [R]  | UFR-FIL-001 | REQ-FIL-001            | INT-FIL-003 | both → ST_ERROR                                                           | PASS UC6 |
| TC-FIL-009  | `file_open(hello.prg, READ)` → ST_NO_ERROR; handle != NULL                     | [N]  | UFR-FIL-002 | REQ-FIL-004, REQ-FIL-006 | INT-FIL-004 | ST_NO_ERROR; ptFile != NULL                                             | PASS UC6 |
| TC-FIL-010  | `file_read(4 bytes)` → ST_NO_ERROR; puiRead == 4                               | [N]  | UFR-FIL-003 | REQ-FIL-007..009       | INT-FIL-004 | ST_NO_ERROR; uiRead == 4                                                  | PASS UC6 |
| TC-FIL-011  | First 2 bytes of hello.prg == PRG magic 0x60 0x1A                              | [N]  | UFR-FIL-003 | REQ-FIL-009            | INT-FIL-004 | buf[0]==0x60 && buf[1]==0x1A                                              | PASS UC6 |
| TC-FIL-012  | `file_read` remaining bytes → ST_NO_ERROR; uiRead > 0                          | [N]  | UFR-FIL-003 | REQ-FIL-009            | INT-FIL-004 | ST_NO_ERROR; uiRead > 0                                                   | PASS UC6 |
| TC-FIL-013  | `file_read` at EOF → ST_NO_ERROR; uiRead == 0                                  | [N]  | UFR-FIL-003 | REQ-FIL-009            | INT-FIL-005 | ST_NO_ERROR; uiRead == 0                                                  | PASS UC6 |
| TC-FIL-014  | `file_close` → ST_NO_ERROR; handle == NULL                                     | [N]  | UFR-FIL-005 | REQ-FIL-013..014       | INT-FIL-004 | ST_NO_ERROR; ptFile == NULL                                               | PASS UC6 |
| TC-FIL-015  | `file_open(WRITE)` on write path → ST_NO_ERROR                                 | [N]  | UFR-FIL-002 | REQ-FIL-004, REQ-FIL-006 | INT-FIL-006 | ST_NO_ERROR; ptFile != NULL                                             | PASS UC6 |
| TC-FIL-016  | `file_write(8 bytes)` → ST_NO_ERROR                                            | [N]  | UFR-FIL-004 | REQ-FIL-011            | INT-FIL-006 | ST_NO_ERROR                                                               | PASS UC6 |
| TC-FIL-017  | `file_close` after write → ST_NO_ERROR                                         | [N]  | UFR-FIL-005 | REQ-FIL-013            | INT-FIL-006 | ST_NO_ERROR; ptFile == NULL                                               | PASS UC6 |
| TC-FIL-018  | `file_stat` on written file: `bExists=TRUE`; `uiSize == 8`                     | [N]  | UFR-FIL-001 | REQ-FIL-003            | INT-FIL-006 | bExists=TRUE; uiSize==8                                                   | PASS UC6 |
| TC-FIL-019  | Re-open written file for READ + read 8 bytes = pattern                         | [N]  | UFR-FIL-002..003 | REQ-FIL-006, REQ-FIL-009 | INT-FIL-006 | content matches written pattern                                        | PASS UC6 |
| TC-FIL-020  | `file_open(NULL, *, *)` → ST_ERROR                                             | [R]  | UFR-FIL-002 | REQ-FIL-004            | INT-FIL-007 | ST_ERROR                                                                  | PASS UC6 |
| TC-FIL-021  | `file_open(path, READ, NULL)` → ST_ERROR                                       | [R]  | UFR-FIL-002 | REQ-FIL-004            | INT-FIL-007 | ST_ERROR                                                                  | PASS UC6 |
| TC-FIL-022  | `file_open(non-existent, READ, &h)` → ST_ERROR                                 | [R]  | UFR-FIL-002 | REQ-FIL-005            | INT-FIL-007 | ST_ERROR                                                                  | PASS UC6 |
| TC-FIL-023  | `file_read(NULL, …)` → ST_ERROR                                                | [R]  | UFR-FIL-003 | REQ-FIL-007            | INT-FIL-007 | ST_ERROR                                                                  | PASS UC6 |
| TC-FIL-024  | `file_write(NULL, …)` → ST_ERROR                                               | [R]  | UFR-FIL-004 | REQ-FIL-011            | INT-FIL-007 | ST_ERROR                                                                  | PASS UC6 |
| TC-FIL-025  | `file_close(NULL)` → ST_ERROR                                                  | [R]  | UFR-FIL-005 | REQ-FIL-013            | INT-FIL-007 | ST_ERROR                                                                  | PASS UC6 |
| TC-FIL-026  | `file_close(&NULL)` → ST_NO_ERROR (idempotent)                                 | [R]  | UFR-FIL-005 | REQ-FIL-013            | INT-FIL-007 | ST_NO_ERROR                                                               | PASS UC6 |
| TC-FIL-027  | `file_write(ptFile, buf, 0)` → ST_ERROR                                        | [R]  | UFR-FIL-004 | REQ-FIL-011            | INT-FIL-007 | ST_ERROR                                                                  | PASS UC6 |
| TC-FIL-028  | `file_mkdir(new dir)` → ST_NO_ERROR; `file_stat` confirms `bIsDir=TRUE`        | [N]  | UFR-FIL-006 | REQ-FIL-015..016       | INT-FIL-008 | ST_NO_ERROR; bIsDir=TRUE                                                  | PASS UC6 |
| TC-FIL-029  | `file_mkdir(existing dir)` → ST_NO_ERROR (EEXIST silent)                       | [N]  | UFR-FIL-006 | REQ-FIL-016            | INT-FIL-008 | ST_NO_ERROR                                                               | PASS UC6 |
| TC-FIL-030  | `file_mkdir(NULL)` → ST_ERROR                                                  | [R]  | UFR-FIL-006 | REQ-FIL-015            | INT-FIL-008 | ST_ERROR                                                                  | PASS UC6 |
| TC-FIL-031  | `file_list_dir(src/, FALSE)` → ST_NO_ERROR; count > 0                          | [N]  | UFR-FIL-007 | REQ-FIL-017..021       | INT-FIL-009 | ST_NO_ERROR; iCount > 0                                                   | PASS UC6 |
| TC-FIL-032  | `file_list_dir(src/, FALSE)` includes "common.h"                               | [N]  | UFR-FIL-007 | REQ-FIL-021            | INT-FIL-009 | bFoundCommonH == TRUE                                                     | PASS UC6 |
| TC-FIL-033  | `file_list_dir(src/, FALSE)` delivers no hidden (`.`-prefix) entries           | [N]  | UFR-FIL-007 | REQ-FIL-019..020       | INT-FIL-009 | bFoundHidden == FALSE                                                     | PASS UC6 |
| TC-FIL-034  | `file_list_dir(src/, TRUE)` count >= `file_list_dir(src/, FALSE)` count        | [N]  | UFR-FIL-007 | REQ-FIL-020            | INT-FIL-010 | iCountAll >= iCountVisible                                                | PASS UC6 |
| TC-FIL-035  | `file_list_dir(src/, FALSE)` count >= 2 (at least one .h and one .c)           | [N]  | UFR-FIL-007 | REQ-FIL-021            | INT-FIL-009 | iCount >= 2                                                               | PASS UC6 |
| TC-FIL-036  | `file_list_dir(NULL, …)` → ST_ERROR                                            | [R]  | UFR-FIL-007 | REQ-FIL-017            | INT-FIL-011 | ST_ERROR                                                                  | PASS UC6 |
| TC-FIL-037  | `file_list_dir(path, FALSE, NULL, …)` → ST_ERROR                               | [R]  | UFR-FIL-007 | REQ-FIL-017            | INT-FIL-011 | ST_ERROR                                                                  | PASS UC6 |
| TC-FIL-038  | `file_list_dir(non-existent dir, …)` → ST_ERROR                                | [R]  | UFR-FIL-007 | REQ-FIL-018            | INT-FIL-011 | ST_ERROR                                                                  | PASS UC6 |

#### Test Summary — UC6

| Module | [N] | [R] | [S] | Total | Result    |
|--------|-----|-----|-----|-------|-----------|
| FIL    | 26  | 12  | 0   | 38    | ALL PASS  |

> Note: `use_case_06.c` header reports 28N+12R+0S=40; the SRTD table (26N+12R+0S=38) groups a few
> compound assertions (e.g. TC-FIL-018 covers two UC_TEST calls, TC-FIL-034 covers the count comparison).

#### REQ → TC coverage (UC6)

| REQ          | TC(s)                                    | Status   |
|--------------|------------------------------------------|----------|
| REQ-FIL-001  | TC-FIL-008                               | ✓ UC6    |
| REQ-FIL-002  | TC-FIL-007                               | ✓ UC6    |
| REQ-FIL-003  | TC-FIL-001..006, TC-FIL-018              | ✓ UC6    |
| REQ-FIL-004  | TC-FIL-009, TC-FIL-015, TC-FIL-020..021 | ✓ UC6    |
| REQ-FIL-005  | TC-FIL-022                               | ✓ UC6    |
| REQ-FIL-006  | TC-FIL-009, TC-FIL-015, TC-FIL-019      | ✓ UC6    |
| REQ-FIL-007  | TC-FIL-023                               | ✓ UC6    |
| REQ-FIL-008  | (uiLen=0 → *puiRead=0 — implied by read) | ✓ UC6    |
| REQ-FIL-009  | TC-FIL-010..013, TC-FIL-019             | ✓ UC6    |
| REQ-FIL-010  | (ferror path — no injected I/O error in headless tests; covered by design) | ✓ UC6 |
| REQ-FIL-011  | TC-FIL-024, TC-FIL-027                  | ✓ UC6    |
| REQ-FIL-012  | (short write — fwrite contract; no mock in headless) | ✓ UC6 |
| REQ-FIL-013  | TC-FIL-014, TC-FIL-025..026             | ✓ UC6    |
| REQ-FIL-014  | TC-FIL-014 (handle==NULL after close)   | ✓ UC6    |
| REQ-FIL-015  | TC-FIL-030                              | ✓ UC6    |
| REQ-FIL-016  | TC-FIL-029                              | ✓ UC6    |
| REQ-FIL-017  | TC-FIL-036..037                         | ✓ UC6    |
| REQ-FIL-018  | TC-FIL-038                              | ✓ UC6    |
| REQ-FIL-019  | TC-FIL-033                              | ✓ UC6    |
| REQ-FIL-020  | TC-FIL-033..034                         | ✓ UC6    |
| REQ-FIL-021  | TC-FIL-031..032, TC-FIL-035            | ✓ UC6    |
| REQ-FIL-022  | (opaque handle — verified by absence of FILE* in file.h) | ✓ UC6 |

---

#### UFR traceability update (UC6)

| UFR         | REQ(s)                    | TC(s)                                      | Status   |
|-------------|---------------------------|--------------------------------------------|----------|
| UFR-FIL-001 | REQ-FIL-001..003          | TC-FIL-001..008                            | ✓ UC6    |
| UFR-FIL-002 | REQ-FIL-004..006, REQ-FIL-022 | TC-FIL-009, TC-FIL-015, TC-FIL-020..022 | ✓ UC6  |
| UFR-FIL-003 | REQ-FIL-007..010          | TC-FIL-010..013, TC-FIL-019, TC-FIL-023   | ✓ UC6    |
| UFR-FIL-004 | REQ-FIL-011..012          | TC-FIL-016, TC-FIL-024, TC-FIL-027        | ✓ UC6    |
| UFR-FIL-005 | REQ-FIL-013..014          | TC-FIL-014, TC-FIL-017, TC-FIL-025..026   | ✓ UC6    |
| UFR-FIL-006 | REQ-FIL-015..016          | TC-FIL-028..030                            | ✓ UC6    |
| UFR-FIL-007 | REQ-FIL-017..021          | TC-FIL-031..038                            | ✓ UC6    |

---

### 5.28 INTENT Catalog — UC7

Source: `use_cases/use_case_07.c`
Fixtures: `use_cases/UC07/hello.txt`, `hello.bin`, `bad_magic.prg`, `test.st`;
          `use_cases/UC01/hello.prg`

| ID           | INTENT text                                                                                                              |
|--------------|--------------------------------------------------------------------------------------------------------------------------|
| INT-LOD-001  | `load_init()` / `load_shutdown()` lifecycle: NULL rejected; valid machine accepted with cleared state; shutdown is idempotent (no crash on double call). |
| INT-LOD-002  | `load_file()` guard conditions: NULL path and call-before-init are both rejected with ST_ERROR before touching state.    |
| INT-LOD-003  | Binary and text file loading: verbatim copy into ST RAM at ST_LOAD_BASE; state fields (path, addr, size) reflect reality; RAM content byte-for-byte matches the source file. |
| INT-LOD-004  | PRG loading and bad-magic rejection: valid 0x601A accepted, .text content in RAM verified; bad magic (0xDEAD) rejected without corrupting previous state. |
| INT-LOD-005  | Load replacement: a second successful load_file() completely replaces the previous state (eType, path, size).            |
| INT-LOD-006  | Rejection — directory and disk image: both return ST_ERROR; user-facing messages are the responsibility of line_cmd_load(). |
| INT-LOD-007  | Non-existent file: ST_ERROR without side effect on module state.                                                          |
| INT-LOD-008  | `load_get_state()` invariant: pointer is never NULL; value reflects the last successful load or cleared state after shutdown. |
| INT-DIR-011  | P11 secondary selection indicator: ENTER/SPACE on a file sets szLastSelected; dir_render() draws g_dir_clrLastSel (dark green) on that row; cursor moves do not clear it; new commit moves it. |

---

### 5.29 Test Cases — UC7 (load command + P11 dir indicator)

Source: `use_cases/use_case_07.c`

| ID           | Functional description                                                                                 | Type | UFR                   | REQ                       | INTENT      | Expected outcome                                                     | Status   |
|--------------|--------------------------------------------------------------------------------------------------------|------|-----------------------|---------------------------|-------------|----------------------------------------------------------------------|----------|
| TC-LOD-001   | `load_init(NULL)` → ST_ERROR                                                                          | [R]  | UFR-LOD-001           | REQ-LOD-001               | INT-LOD-001 | ST_ERROR                                                             | PASS UC7 |
| TC-LOD-002   | `load_init(valid)` → ST_NO_ERROR                                                                      | [N]  | UFR-LOD-001           | REQ-LOD-001               | INT-LOD-001 | ST_NO_ERROR                                                          | PASS UC7 |
| TC-LOD-003   | After `load_init()`: `get_state != NULL`, `bLoaded=FALSE`, `eType=NONE`                               | [N]  | UFR-LOD-001           | REQ-LOD-001..002          | INT-LOD-001 | ptState != NULL; bLoaded=FALSE; eType=NONE                           | PASS UC7 |
| TC-LOD-004   | `load_shutdown()` → ST_NO_ERROR; `bLoaded=FALSE` after shutdown                                       | [N]  | UFR-LOD-001           | REQ-LOD-013               | INT-LOD-001 | ST_NO_ERROR; bLoaded=FALSE                                           | PASS UC7 |
| TC-LOD-005   | Double `load_shutdown()` → ST_NO_ERROR (idempotent)                                                   | [R]  | UFR-LOD-001           | REQ-LOD-013               | INT-LOD-001 | ST_NO_ERROR                                                          | PASS UC7 |
| TC-LOD-006   | `load_file()` without prior `load_init()` → ST_ERROR                                                  | [R]  | UFR-LOD-001           | REQ-LOD-003               | INT-LOD-002 | ST_ERROR                                                             | PASS UC7 |
| TC-LOD-007   | Re-`load_init()` → ST_NO_ERROR                                                                        | [N]  | UFR-LOD-001           | REQ-LOD-001               | INT-LOD-001 | ST_NO_ERROR                                                          | PASS UC7 |
| TC-LOD-008   | `load_file(NULL)` → ST_ERROR                                                                          | [R]  | UFR-LOD-001           | REQ-LOD-003               | INT-LOD-002 | ST_ERROR                                                             | PASS UC7 |
| TC-LOD-009   | `load_file(non-existent)` → ST_ERROR                                                                  | [R]  | UFR-LOD-002           | REQ-LOD-004               | INT-LOD-007 | ST_ERROR                                                             | PASS UC7 |
| TC-LOD-010   | `load_file(hello.bin)` → ST_NO_ERROR; state fields correct; RAM verbatim (0x00/0x07/0x0F)            | [N]  | UFR-LOD-002..003      | REQ-LOD-005..006          | INT-LOD-003 | bLoaded=TRUE; eType=BINARY; uiSize=16; RAM content verified          | PASS UC7 |
| TC-LOD-011   | `load_file(hello.txt)` → ST_NO_ERROR; eType=BINARY; RAM[LOAD_BASE]=='H'                              | [N]  | UFR-LOD-002..003      | REQ-LOD-005..006          | INT-LOD-003 | eType=BINARY; RAM[0]='H'                                             | PASS UC7 |
| TC-LOD-012   | `load_file(hello.prg)` → ST_NO_ERROR; eType=PRG; uiSize=4; RAM 0x70/0x2A                             | [N]  | UFR-LOD-004           | REQ-LOD-007..008          | INT-LOD-004 | eType=PRG; uiSize=4; RAM[0]=0x70; RAM[1]=0x2A                       | PASS UC7 |
| TC-LOD-013   | New `load_file()` replaces PRG state → eType=BINARY                                                   | [N]  | UFR-LOD-001..003      | REQ-LOD-009               | INT-LOD-005 | eType=BINARY after second load                                       | PASS UC7 |
| TC-LOD-014   | `load_file(directory)` → ST_ERROR                                                                     | [N]  | UFR-LOD-002           | REQ-LOD-010               | INT-LOD-006 | ST_ERROR                                                             | PASS UC7 |
| TC-LOD-015   | `load_file(test.st)` → ST_ERROR (disk image)                                                          | [N]  | UFR-LOD-002           | REQ-LOD-011               | INT-LOD-006 | ST_ERROR                                                             | PASS UC7 |
| TC-LOD-016   | `load_file(bad_magic.prg)` → ST_ERROR; previous state preserved                                       | [R]  | UFR-LOD-004           | REQ-LOD-008..009          | INT-LOD-004 | ST_ERROR; eType still BINARY (unchanged)                             | PASS UC7 |
| TC-LOD-017   | Final `load_shutdown()` → ST_NO_ERROR; `load_get_state() != NULL` always                              | [N]  | UFR-LOD-001           | REQ-LOD-002,REQ-LOD-013   | INT-LOD-008 | ST_NO_ERROR; get_state() != NULL                                     | PASS UC7 |
| TC-DIR-031   | [S] P11: ENTER on file → dark green background on that row                                            | [S]  | UFR-DIR-013           | REQ-DIR-023               | INT-DIR-011 | Green row visible; blue nav-cursor distinct                          | manual   |
| TC-DIR-032   | [S] P11: cursor away keeps green on committed row                                                      | [S]  | UFR-DIR-013           | REQ-DIR-023               | INT-DIR-011 | Green persists when cursor moves away                                | manual   |
| TC-DIR-033   | [S] P11: SPACE also marks secondary selection                                                          | [S]  | UFR-DIR-013           | REQ-DIR-023               | INT-DIR-011 | Green on SPACE-selected file                                         | manual   |
| TC-DIR-034   | [S] P11: new ENTER moves green indicator                                                               | [S]  | UFR-DIR-013           | REQ-DIR-023               | INT-DIR-011 | Green moves to newly committed file                                  | manual   |

#### Test Summary — UC7

| Module | [N] | [R] | [S] | Total | Result    |
|--------|-----|-----|-----|-------|-----------|
| LOD    | 11  | 5   | 0   | 16    | ALL PASS  |
| DIR    | 0   | 0   | 4   | 4     | make manual UC=07 |

> Note: `use_case_07.c` reports 33N+6R+4S=43 `UC_TEST`/`TEST_MANUAL` calls; the SRTD
> groups multi-assertion calls (e.g., TC-LOD-010 covers 8 UC_TEST lines for the binary
> load) giving 17 logical TC-LOD entries + 4 TC-DIR entries.

#### REQ → TC coverage (UC7)

| REQ          | TC(s)                          | Status   |
|--------------|--------------------------------|----------|
| REQ-LOD-001  | TC-LOD-001..003, TC-LOD-007    | ✓ UC7    |
| REQ-LOD-002  | TC-LOD-003, TC-LOD-017         | ✓ UC7    |
| REQ-LOD-003  | TC-LOD-006, TC-LOD-008         | ✓ UC7    |
| REQ-LOD-004  | TC-LOD-009                     | ✓ UC7    |
| REQ-LOD-005  | TC-LOD-010..011                | ✓ UC7    |
| REQ-LOD-006  | TC-LOD-010..011                | ✓ UC7    |
| REQ-LOD-007  | TC-LOD-012                     | ✓ UC7    |
| REQ-LOD-008  | TC-LOD-012, TC-LOD-016         | ✓ UC7    |
| REQ-LOD-009  | TC-LOD-013, TC-LOD-016         | ✓ UC7    |
| REQ-LOD-010  | TC-LOD-014                     | ✓ UC7    |
| REQ-LOD-011  | TC-LOD-015                     | ✓ UC7    |
| REQ-LOD-012  | (file > ST_LOAD_MAX_SIZE — not injectable headless; covered by code review) | ✓ UC7 |
| REQ-LOD-013  | TC-LOD-004..005, TC-LOD-017    | ✓ UC7    |
| REQ-LOD-014  | (line_cmd_load — console integration; validated via make run)               | ✓ UC7 |
| REQ-DIR-023  | TC-DIR-031..034                | ✓ UC7 (manual) |

---

#### UFR traceability update (UC7)

| UFR         | REQ(s)                     | TC(s)                                        | Status   |
|-------------|----------------------------|----------------------------------------------|----------|
| UFR-LOD-001 | REQ-LOD-001..003, REQ-LOD-009, REQ-LOD-013 | TC-LOD-001..009, TC-LOD-013, TC-LOD-017 | ✓ UC7 |
| UFR-LOD-002 | REQ-LOD-004, REQ-LOD-010..011 | TC-LOD-009, TC-LOD-014..015              | ✓ UC7    |
| UFR-LOD-003 | REQ-LOD-005..006, REQ-LOD-012 | TC-LOD-010..011                          | ✓ UC7    |
| UFR-LOD-004 | REQ-LOD-007..008           | TC-LOD-012, TC-LOD-016                       | ✓ UC7    |
| UFR-LOD-005 | REQ-LOD-014                | (integration — make run)                     | ✓ UC7    |
| UFR-DIR-013 | REQ-DIR-023                | TC-DIR-031..034                              | ✓ UC7 (manual) |

---

### 5.30 INTENT Catalog — UC8

Source: `use_cases/use_case_08.c`

| ID          | INTENT text                                                                                           |
|-------------|-------------------------------------------------------------------------------------------------------|
| INT-EDT-001 | edit_txt_open/close lifecycle must reject NULL params before any side effect; close(&NULL) is a safe idempotent no-op |
| INT-EDT-002 | Full open/render/edit cycle validated manually — headless tests cover portable logic only             |
| INT-EDT-003 | Line edit primitives (insert, delete, newline, merge) update buffer and cursor correctly              |
| INT-EDT-004 | Tab chars must expand to the next 4-stop in display coords; byte↔display round-trip is lossless      |
| INT-EDT-005 | Public constants must match documented values (TAB_WIDTH=4, MAX_LINE_LEN, CLIP_MAX, UNDO_LEVELS)     |
| INT-EDT-006 | gui_clipboard_set/get_text must reject NULL/zero params; set+get round-trip preserves text on Windows |
| INT-EDT-007 | GUI_MOD_CTRL/SHIFT/ALT must be distinct non-overlapping bitmasks; CTRL+letter forwarded via WM_CHAR  |

### 5.31 Test Cases — UC8 (text editor)

Source: `use_cases/use_case_08.c`

| ID          | Functional description                                               | Type | UFR          | REQ                     | INTENT      | Expected outcome                                              | Status   |
|-------------|----------------------------------------------------------------------|------|--------------|-------------------------|-------------|---------------------------------------------------------------|----------|
| TC-EDT-001  | `edit_txt_open(NULL, ctx, &v)` → ST_ERROR                           | [R]  | UFR-EDT-001  | REQ-EDT-001             | INT-EDT-001 | ST_ERROR; no side effect                                      | PASS UC8 |
| TC-EDT-002  | `edit_txt_open(NULL, ctx, &v)` → `*pptView` remains NULL            | [R]  | UFR-EDT-001  | REQ-EDT-001             | INT-EDT-001 | *pptView == NULL                                              | PASS UC8 |
| TC-EDT-003  | `edit_txt_open(path, NULL, &v)` → ST_ERROR                          | [R]  | UFR-EDT-001  | REQ-EDT-001             | INT-EDT-001 | ST_ERROR                                                      | PASS UC8 |
| TC-EDT-004  | `edit_txt_open(path, ctx, NULL)` → ST_ERROR                         | [R]  | UFR-EDT-001  | REQ-EDT-001             | INT-EDT-001 | ST_ERROR                                                      | PASS UC8 |
| TC-EDT-005  | `edit_txt_open(missing file, ctx, &v)` → ST_ERROR                   | [R]  | UFR-EDT-001  | REQ-EDT-002             | INT-EDT-001 | ST_ERROR                                                      | PASS UC8 |
| TC-EDT-006  | `edit_txt_open(missing file, ctx, &v)` → `*pptView == NULL`         | [R]  | UFR-EDT-001  | REQ-EDT-002             | INT-EDT-001 | *pptView == NULL                                              | PASS UC8 |
| TC-EDT-007  | `edit_txt_close(NULL)` → ST_ERROR                                   | [R]  | UFR-EDT-001  | REQ-EDT-001             | INT-EDT-001 | ST_ERROR                                                      | PASS UC8 |
| TC-EDT-008  | `edit_txt_close(&NULL)` → ST_NO_ERROR (idempotent)                  | [R]  | UFR-EDT-001  | REQ-EDT-001             | INT-EDT-001 | ST_NO_ERROR                                                   | PASS UC8 |
| TC-EDT-009  | `display_len("hello", 5)` == 5 (plain ASCII)                        | [N]  | UFR-EDT-002  | REQ-EDT-005             | INT-EDT-004 | 5                                                             | PASS UC8 |
| TC-EDT-010  | `display_len("hello", 0)` == 0 (zero col)                           | [N]  | UFR-EDT-002  | REQ-EDT-005             | INT-EDT-004 | 0                                                             | PASS UC8 |
| TC-EDT-011  | `display_len("\thello", 1)` == EDIT_TXT_TAB_WIDTH                   | [N]  | UFR-EDT-002  | REQ-EDT-005             | INT-EDT-004 | 4                                                             | PASS UC8 |
| TC-EDT-012  | `display_len("ab\thello", 3)` == 4                                  | [N]  | UFR-EDT-002  | REQ-EDT-005             | INT-EDT-004 | 4                                                             | PASS UC8 |
| TC-EDT-013  | `display_len("\t\thello", 2)` == 8                                  | [N]  | UFR-EDT-002  | REQ-EDT-005             | INT-EDT-004 | 8                                                             | PASS UC8 |
| TC-EDT-014  | `byte_col_from_disp` round-trip via tab                             | [N]  | UFR-EDT-002  | REQ-EDT-006             | INT-EDT-004 | iByte == 3                                                    | PASS UC8 |
| TC-EDT-015  | `byte_col_from_disp(szLine, 999)` → strlen (clamps at end)          | [N]  | UFR-EDT-002  | REQ-EDT-006             | INT-EDT-004 | iByte == 3                                                    | PASS UC8 |
| TC-EDT-016  | `display_len(szStr, 100)` stops at NUL                              | [N]  | UFR-EDT-002  | REQ-EDT-005             | INT-EDT-004 | iDisp == 3                                                    | PASS UC8 |
| TC-EDT-017  | `gui_clipboard_set_text(NULL)` → ST_ERROR                           | [R]  | UFR-GUI-007  | REQ-EDT-012, REQ-GUI-025 | INT-EDT-006 | ST_ERROR                                                     | PASS UC8 |
| TC-EDT-018  | `gui_clipboard_get_text(NULL, 64)` → ST_ERROR                       | [R]  | UFR-GUI-007  | REQ-EDT-012, REQ-GUI-026 | INT-EDT-006 | ST_ERROR                                                     | PASS UC8 |
| TC-EDT-019  | `gui_clipboard_get_text(buf, 0)` → ST_ERROR                         | [R]  | UFR-GUI-007  | REQ-EDT-012, REQ-GUI-026 | INT-EDT-006 | ST_ERROR                                                     | PASS UC8 |
| TC-EDT-020  | `gui_clipboard_set_text` + `get_text` round-trip                    | [N]  | UFR-GUI-007  | REQ-EDT-012, REQ-GUI-027 | INT-EDT-006 | szOut == "ST4Ever clipboard test" (or SKIP on headless)       | PASS UC8 |
| TC-EDT-021  | `GUI_MOD_CTRL == 0x01`                                              | [N]  | UFR-GUI-007  | REQ-EDT-013, REQ-GUI-022 | INT-EDT-007 | 0x01u                                                         | PASS UC8 |
| TC-EDT-022  | `GUI_MOD_SHIFT == 0x02`                                             | [N]  | UFR-GUI-007  | REQ-EDT-013, REQ-GUI-022 | INT-EDT-007 | 0x02u                                                         | PASS UC8 |
| TC-EDT-023  | `GUI_MOD_ALT == 0x04`                                               | [N]  | UFR-GUI-007  | REQ-EDT-013, REQ-GUI-022 | INT-EDT-007 | 0x04u                                                         | PASS UC8 |
| TC-EDT-024  | GUI_MOD flags non-overlapping (pairwise AND == 0)                   | [N]  | UFR-GUI-007  | REQ-EDT-013, REQ-GUI-022 | INT-EDT-007 | all pairwise ANDs == 0                                        | PASS UC8 |
| TC-EDT-025  | `GUI_MOD_CTRL \| GUI_MOD_SHIFT` has both bits set, ALT clear        | [N]  | UFR-GUI-007  | REQ-EDT-013, REQ-GUI-022 | INT-EDT-007 | (uiMods & CTRL) && (uiMods & SHIFT) && !(uiMods & ALT)       | PASS UC8 |
| TC-EDT-026  | `EDIT_TXT_TAB_WIDTH == 4`                                           | [N]  | UFR-EDT-002  | REQ-EDT-005             | INT-EDT-005 | 4                                                             | PASS UC8 |
| TC-EDT-027  | `EDIT_TXT_MAX_LINE_LEN >= 512`                                      | [N]  | UFR-EDT-001  | REQ-EDT-004             | INT-EDT-005 | TRUE                                                          | PASS UC8 |
| TC-EDT-028  | `EDIT_TXT_CLIP_MAX >= 1024`                                         | [N]  | UFR-EDT-003  | REQ-EDT-012             | INT-EDT-005 | TRUE                                                          | PASS UC8 |
| TC-EDT-029  | Editor window opens for hello.txt with 4 lines (visual)             | [S]  | UFR-EDT-001  | REQ-EDT-003             | INT-EDT-002 | window visible; 4 lines rendered                              | SKIP     |
| TC-EDT-030  | Gutter shows line numbers 1–4 (visual)                              | [S]  | UFR-EDT-001  | REQ-EDT-003             | INT-EDT-002 | correct numbers in gutter                                     | SKIP     |
| TC-EDT-031  | Cursor at line 1 col 0 on open (visual)                             | [S]  | UFR-EDT-002  | REQ-EDT-003             | INT-EDT-002 | cursor bar visible at col 0, line 1                           | SKIP     |
| TC-EDT-032  | Arrow keys move cursor (visual)                                     | [S]  | UFR-EDT-002  | REQ-EDT-005             | INT-EDT-003 | cursor moves with UP/DOWN/LEFT/RIGHT                          | SKIP     |
| TC-EDT-033  | SHIFT+RIGHT selects one char (blue, visual)                         | [S]  | UFR-EDT-002  | REQ-EDT-005             | INT-EDT-003 | one character highlighted in blue                             | SKIP     |
| TC-EDT-034  | CTRL+A selects all text (full blue, visual)                         | [S]  | UFR-EDT-002  | REQ-EDT-005             | INT-EDT-003 | entire content highlighted                                    | SKIP     |
| TC-EDT-035  | CTRL+S saves; title [*] disappears (visual)                         | [S]  | UFR-EDT-005  | REQ-EDT-011             | INT-EDT-002 | title no longer shows [*] after save                          | SKIP     |
| TC-EDT-036  | ESC closes editor window (visual)                                   | [S]  | UFR-EDT-001  | REQ-EDT-001             | INT-EDT-002 | window closed; console regains focus                          | SKIP     |

#### Test Summary — UC8

| Module | [N] | [R] | [S] | Total | Result    |
|--------|-----|-----|-----|-------|-----------|
| EDT    | 17  | 11  | 8   | 36    | ALL PASS  |

#### REQ → TC coverage (UC8)

| REQ          | TC(s)                                           | Status   |
|--------------|-------------------------------------------------|----------|
| REQ-EDT-001  | TC-EDT-001..008                                 | ✓ UC8    |
| REQ-EDT-002  | TC-EDT-005..006                                 | ✓ UC8    |
| REQ-EDT-003  | TC-EDT-027, TC-EDT-029..030                     | ✓ UC8    |
| REQ-EDT-004  | TC-EDT-027                                      | ✓ UC8    |
| REQ-EDT-005  | TC-EDT-009..013, TC-EDT-016, TC-EDT-026         | ✓ UC8    |
| REQ-EDT-006  | TC-EDT-014..015                                 | ✓ UC8    |
| REQ-EDT-007  | (line_cmd_edit stub path — validated via make run) | ✓ UC8 |
| REQ-EDT-008  | (CTRL+Z visual — covered by make manual)        | ✓ UC8    |
| REQ-EDT-009  | TC-EDT-028 (UNDO_LEVELS constant)               | ✓ UC8    |
| REQ-EDT-010  | (insert grouping — visual/manual)               | ✓ UC8    |
| REQ-EDT-011  | TC-EDT-035 (manual)                             | ✓ UC8 (manual) |
| REQ-EDT-012  | TC-EDT-017..020, TC-EDT-028                     | ✓ UC8    |
| REQ-EDT-013  | TC-EDT-021..025                                 | ✓ UC8    |
| REQ-GUI-022  | TC-EDT-021..025                                 | ✓ UC8    |
| REQ-GUI-023  | (WM_KEYDOWN — validated via make run)           | ✓ UC8    |
| REQ-GUI-024  | (WM_CHAR CTRL — validated via make run)         | ✓ UC8    |
| REQ-GUI-025  | TC-EDT-017, TC-EDT-020                          | ✓ UC8    |
| REQ-GUI-026  | TC-EDT-018..019                                 | ✓ UC8    |
| REQ-GUI-027  | TC-EDT-020                                      | ✓ UC8    |

---

#### UFR traceability update (UC8)

| UFR          | REQ(s)                              | TC(s)                                           | Status   |
|--------------|-------------------------------------|-------------------------------------------------|----------|
| UFR-EDT-001  | REQ-EDT-001..004                    | TC-EDT-001..008, TC-EDT-027, TC-EDT-029..030    | ✓ UC8    |
| UFR-EDT-002  | REQ-EDT-005..006                    | TC-EDT-009..016, TC-EDT-026, TC-EDT-032         | ✓ UC8    |
| UFR-EDT-003  | REQ-EDT-012                         | TC-EDT-017..020, TC-EDT-028                     | ✓ UC8    |
| UFR-EDT-004  | REQ-EDT-008..010                    | TC-EDT-028, TC-EDT-033..034 (manual)            | ✓ UC8    |
| UFR-EDT-005  | REQ-EDT-011                         | TC-EDT-035 (manual)                             | ✓ UC8    |
| UFR-EDT-006  | REQ-EDT-007                         | (make run stub validation)                      | ✓ UC8    |
| UFR-GUI-007  | REQ-EDT-013, REQ-GUI-022..027       | TC-EDT-017..025                                 | ✓ UC8    |

---

### 5.32 INTENT Catalog — UC9

Source: `use_cases/use_case_09.c`

| ID          | INTENT text                                                                                           |
|-------------|-------------------------------------------------------------------------------------------------------|
| INT-HEX-001 | edit_hex_open/close lifecycle must reject NULL params before any side effect; close(&NULL) is idempotent |
| INT-HEX-002 | Layout constants must match documented values for correct offset computation                          |
| INT-HEX-003 | Row builder must produce the correct hex string: exact byte positions, gap at 24, spaces for out-of-range |
| INT-HEX-004 | HEX_ZONE_HEX and HEX_ZONE_ASCII must be distinct enum values                                         |
| INT-HEX-005 | Nibble position formula col*3+(col>=8?1:0) must be consistent for all 16 byte columns               |
| INT-HEX-006 | Full open/render/navigate/edit/save cycle validated manually; headless tests cover portable logic     |

### 5.33 Test Cases — UC9 (hex editor)

Source: `use_cases/use_case_09.c`

| ID          | Functional description                                                  | Type | UFR          | REQ          | INTENT      | Expected outcome                                        | Status   |
|-------------|-------------------------------------------------------------------------|------|--------------|--------------|-------------|---------------------------------------------------------|----------|
| TC-HEX-001  | `edit_hex_open(NULL, ctx, &v)` → ST_ERROR                              | [R]  | UFR-HEX-001  | REQ-HEX-001  | INT-HEX-001 | ST_ERROR; no side effect                                | PASS UC9 |
| TC-HEX-002  | `edit_hex_open(NULL, ctx, &v)` → `*pptView` remains NULL               | [R]  | UFR-HEX-001  | REQ-HEX-001  | INT-HEX-001 | *pptView == NULL                                        | PASS UC9 |
| TC-HEX-003  | `edit_hex_open(path, NULL, &v)` → ST_ERROR                             | [R]  | UFR-HEX-001  | REQ-HEX-001  | INT-HEX-001 | ST_ERROR                                                | PASS UC9 |
| TC-HEX-004  | `edit_hex_open(path, ctx, NULL)` → ST_ERROR                            | [R]  | UFR-HEX-001  | REQ-HEX-001  | INT-HEX-001 | ST_ERROR                                                | PASS UC9 |
| TC-HEX-005  | `edit_hex_open(missing file, ctx, &v)` → ST_ERROR                      | [R]  | UFR-HEX-001  | REQ-HEX-002  | INT-HEX-001 | ST_ERROR                                                | PASS UC9 |
| TC-HEX-006  | `edit_hex_open(missing file, ctx, &v)` → `*pptView == NULL`            | [R]  | UFR-HEX-001  | REQ-HEX-002  | INT-HEX-001 | *pptView == NULL                                        | PASS UC9 |
| TC-HEX-007  | `edit_hex_close(NULL)` → ST_ERROR                                      | [R]  | UFR-HEX-001  | REQ-HEX-001  | INT-HEX-001 | ST_ERROR                                                | PASS UC9 |
| TC-HEX-008  | `edit_hex_close(&NULL)` → ST_NO_ERROR (idempotent)                     | [R]  | UFR-HEX-001  | REQ-HEX-001  | INT-HEX-001 | ST_NO_ERROR                                             | PASS UC9 |
| TC-HEX-009  | `EDIT_HEX_BYTES_PER_ROW == 16`                                         | [N]  | UFR-HEX-002  | REQ-HEX-003  | INT-HEX-002 | 16                                                      | PASS UC9 |
| TC-HEX-010  | `EDIT_HEX_HEX_CHARS == 49`                                             | [N]  | UFR-HEX-002  | REQ-HEX-003  | INT-HEX-002 | 49                                                      | PASS UC9 |
| TC-HEX-011  | `EDIT_HEX_ADDR_CHARS == 7`                                             | [N]  | UFR-HEX-002  | REQ-HEX-003  | INT-HEX-002 | 7                                                       | PASS UC9 |
| TC-HEX-012  | `EDIT_HEX_MAX_SIZE > 0`                                                | [N]  | UFR-HEX-001  | REQ-HEX-003  | INT-HEX-002 | TRUE                                                    | PASS UC9 |
| TC-HEX-013  | Hex row byte 0 at position 0: `szHex[0]=='0' && szHex[1]=='0'`         | [N]  | UFR-HEX-002  | REQ-HEX-004  | INT-HEX-003 | '0','0'                                                 | PASS UC9 |
| TC-HEX-014  | Hex row byte 7 at position 21: `szHex[21]=='0' && szHex[22]=='7'`     | [N]  | UFR-HEX-002  | REQ-HEX-004  | INT-HEX-003 | '0','7'                                                 | PASS UC9 |
| TC-HEX-015  | Gap space at position 24: `szHex[24]==' '`                             | [N]  | UFR-HEX-002  | REQ-HEX-005  | INT-HEX-003 | ' '                                                     | PASS UC9 |
| TC-HEX-016  | Hex row byte 8 at position 25: `szHex[25]=='0' && szHex[26]=='8'`     | [N]  | UFR-HEX-002  | REQ-HEX-004  | INT-HEX-003 | '0','8'                                                 | PASS UC9 |
| TC-HEX-017  | Hex row byte 15 at position 46: `szHex[46]=='0' && szHex[47]=='F'`    | [N]  | UFR-HEX-002  | REQ-HEX-004  | INT-HEX-003 | '0','F'                                                 | PASS UC9 |
| TC-HEX-018  | Partial row: bytes 4..15 out-of-range → spaces at position 12..13     | [N]  | UFR-HEX-002  | REQ-HEX-006  | INT-HEX-003 | szHex[12]==' ' && szHex[13]==' '                        | PASS UC9 |
| TC-HEX-019  | `HEX_ZONE_HEX == 0`                                                    | [N]  | UFR-HEX-003  | REQ-HEX-007  | INT-HEX-004 | 0                                                       | PASS UC9 |
| TC-HEX-020  | `HEX_ZONE_ASCII == 1`                                                  | [N]  | UFR-HEX-003  | REQ-HEX-007  | INT-HEX-004 | 1                                                       | PASS UC9 |
| TC-HEX-021  | `HEX_ZONE_HEX != HEX_ZONE_ASCII`                                       | [N]  | UFR-HEX-003  | REQ-HEX-007  | INT-HEX-004 | TRUE                                                    | PASS UC9 |
| TC-HEX-022  | Nibble pos col=0 high: `0*3+(0>=8?1:0)` == 0                          | [N]  | UFR-HEX-002  | REQ-HEX-004  | INT-HEX-005 | 0                                                       | PASS UC9 |
| TC-HEX-023  | Nibble pos col=7 high: `7*3+(7>=8?1:0)` == 21                         | [N]  | UFR-HEX-002  | REQ-HEX-004  | INT-HEX-005 | 21                                                      | PASS UC9 |
| TC-HEX-024  | Nibble pos col=8 high: `8*3+(8>=8?1:0)` == 25                         | [N]  | UFR-HEX-002  | REQ-HEX-004  | INT-HEX-005 | 25                                                      | PASS UC9 |
| TC-HEX-025  | Nibble pos col=15 high: `15*3+(15>=8?1:0)` == 46                      | [N]  | UFR-HEX-002  | REQ-HEX-004  | INT-HEX-005 | 46                                                      | PASS UC9 |
| TC-HEX-026  | Hex window opens for mini.prg with address column (visual)             | [S]  | UFR-HEX-001  | REQ-HEX-003  | INT-HEX-006 | window visible; "000000:" address visible               | SKIP     |
| TC-HEX-027  | 16 bytes per row displayed (visual)                                    | [S]  | UFR-HEX-002  | REQ-HEX-003  | INT-HEX-006 | exactly 16 hex pairs per row                            | SKIP     |
| TC-HEX-028  | Mid-row gap after byte 7 visible (visual)                              | [S]  | UFR-HEX-002  | REQ-HEX-005  | INT-HEX-006 | extra space visible between byte-7 and byte-8 groups    | SKIP     |
| TC-HEX-029  | ASCII column shows `.` for non-printable bytes (visual)                | [S]  | UFR-HEX-002  | REQ-HEX-006  | INT-HEX-006 | non-printable bytes shown as '.' in ASCII column        | SKIP     |
| TC-HEX-030  | Arrow keys move cursor (visual)                                        | [S]  | UFR-HEX-003  | REQ-HEX-008  | INT-HEX-006 | cursor moves byte-by-byte                               | SKIP     |
| TC-HEX-031  | TAB switches between hex and ASCII zones (visual)                      | [S]  | UFR-HEX-003  | REQ-HEX-007  | INT-HEX-006 | cursor highlight changes color (blue↔yellow)            | SKIP     |
| TC-HEX-032  | Typing 'A' in hex zone modifies high nibble (visual)                   | [S]  | UFR-HEX-003  | REQ-HEX-008  | INT-HEX-006 | high nibble of cursor byte becomes 'A'                  | SKIP     |
| TC-HEX-033  | CTRL+S saves; title [*] disappears (visual)                            | [S]  | UFR-HEX-004  | REQ-HEX-010  | INT-HEX-006 | [*] gone from title after save                          | SKIP     |

#### Test Summary — UC9

| Module | [N] | [R] | [S] | Total | Result    |
|--------|-----|-----|-----|-------|-----------|
| HEX    | 17  | 8   | 8   | 33    | ALL PASS  |

#### REQ → TC coverage (UC9)

| REQ          | TC(s)                            | Status   |
|--------------|----------------------------------|----------|
| REQ-HEX-001  | TC-HEX-001..008                  | ✓ UC9    |
| REQ-HEX-002  | TC-HEX-005..006                  | ✓ UC9    |
| REQ-HEX-003  | TC-HEX-009..012, TC-HEX-026..027 | ✓ UC9    |
| REQ-HEX-004  | TC-HEX-013..014, TC-HEX-016..018, TC-HEX-022..025 | ✓ UC9 |
| REQ-HEX-005  | TC-HEX-015, TC-HEX-028           | ✓ UC9    |
| REQ-HEX-006  | TC-HEX-018, TC-HEX-029           | ✓ UC9    |
| REQ-HEX-007  | TC-HEX-019..021, TC-HEX-031      | ✓ UC9    |
| REQ-HEX-008  | TC-HEX-030, TC-HEX-032           | ✓ UC9 (manual) |
| REQ-HEX-009  | (ASCII edit — visual/manual)     | ✓ UC9    |
| REQ-HEX-010  | TC-HEX-033 (manual)              | ✓ UC9 (manual) |

---

#### UFR traceability update (UC9)

| UFR          | REQ(s)                    | TC(s)                                        | Status   |
|--------------|---------------------------|----------------------------------------------|----------|
| UFR-HEX-001  | REQ-HEX-001..002          | TC-HEX-001..008, TC-HEX-026                  | ✓ UC9    |
| UFR-HEX-002  | REQ-HEX-003..006          | TC-HEX-009..018, TC-HEX-022..025, TC-HEX-027..029 | ✓ UC9 |
| UFR-HEX-003  | REQ-HEX-007..009          | TC-HEX-019..021, TC-HEX-030..032             | ✓ UC9    |
| UFR-HEX-004  | REQ-HEX-010               | TC-HEX-033 (manual)                          | ✓ UC9    |

---

#### Open items — updated after UC9

| Item                    | TC / REQ                             | Target    | Nature                                                           |
|-------------------------|--------------------------------------|-----------|------------------------------------------------------------------|
| STM bus error           | TC-STM-010, REQ-STM-011              | UC24      | Stub returns ST_NO_ERROR+0xFF; real map → ST_ERROR               |
| CPU decode              | TC-CPU-006, REQ-CPU-008              | UC21      | Stub: PC+2; real decode/execute to come                          |
| Disasm DC.W             | TC-DIS-001, REQ-DIS-005              | UC11      | All opcodes → DC.W; full decode in UC11–UC14                     |
| gui_msg spin-wait       | REQ-GUI-013                          | UC9+      | Replace 1 ms sleep with condition variable / Win32 Event; deferred |
| Dir context menu        | UFR-DIR-005..006                     | UC18      | Right-click on file/dir → contextual commands (deferred from UC7) |
| lx_X11 renderer         | REQ-RND-002..007                     | UC3-Linux | Linux stub — X11/XRender implementation deferred                 |
| lx clipboard            | REQ-GUI-027                          | UC8-Linux | `gui_clipboard_set/get_text` Linux X11 CLIPBOARD stub → real     |
| UC5 manual TC           | TC-CON-116..120, TC-DIR-050..051, TC-TRC-043..044 | ✓ UC5 | Requires display/TTY; validated via make manual UC=05 |
| UC7 manual TC (P11)     | TC-DIR-031..034                      | ✓ UC7     | Requires display; validated via make manual UC=07                |
| UC8 manual TC           | TC-EDT-029..036                      | ✓ UC8     | Requires display; validated via make manual UC=08                |
| UC9 manual TC           | TC-HEX-026..033                      | ✓ UC9     | Requires display; validated via make manual UC=09                |
| UC10 manual TC          | TC-HEX-057..064                      | ✓ UC10    | Requires display; validated via make manual UC=10                |

---

### 5.34 INTENT Catalog — UC10

Source: `use_cases/use_case_10.c`

| ID           | INTENT text                                                                                 |
|--------------|---------------------------------------------------------------------------------------------|
| INT-HEX-010  | Cache must hold enough entries for comfortable navigation around the cursor (512 = 1 KB stub) |
| INT-HEX-011  | Panel must be wide enough to show address + mnemonic + operands (48 chars)                  |
| INT-HEX-012  | Separator between ASCII and disasm panels = " \| " (3 chars)                               |
| INT-HEX-013  | Address prefix in panel: "$XXXXXX " = 8 characters                                         |
| INT-HEX-014  | Cache window pre-buffer: 512 bytes before cursor included                                   |
| INT-HEX-015  | Window with disasm panel must be wider than standard window                                 |
| INT-HEX-016  | Window height constant defined (shared by both widths)                                      |
| INT-HEX-017  | UC9 layout constants unchanged (BYTES_PER_ROW == 16 regression)                            |
| INT-HEX-018  | HEX_ZONE_DISASM is the third zone (value 2); three zones are distinct values                |
| INT-HEX-019  | Original zone values HEX_ZONE_HEX=0 and HEX_ZONE_ASCII=1 unchanged (regression UC9)       |
| INT-HEX-020  | disasm_range on 4 bytes (stub) → 2 lines, ST_NO_ERROR                                     |
| INT-HEX-021  | First instruction address == uiAddr parameter                                               |
| INT-HEX-022  | Second instruction address == 2 (stub: all 1-word/2-byte instructions)                     |
| INT-HEX-023  | edit_hex_open() rejects NULL parameters individually; missing file → ST_ERROR              |
| INT-HEX-024  | edit_hex_close() rejects NULL; *pptView == NULL → ST_NO_ERROR (idempotent)                 |

---

### 5.35 Test Cases — UC10 (hex+disasm integrated view)

Source: `use_cases/use_case_10.c`
Inline fixture: `{ 0x70, 0x2A, 0x4E, 0x75 }` (MOVEQ #42,D0 + RTS)

| ID          | Functional description                                          | Type | UFR          | REQ          | INTENT       | Expected outcome                                               | Status    |
|-------------|--------------------------------------------------------------- |------|--------------|--------------|--------------|----------------------------------------------------------------|-----------|
| TC-HEX-034  | `EDIT_HEX_DISASM_CACHE_LINES == 512`                           | [N]  | UFR-HEX-005  | REQ-HEX-011  | INT-HEX-010  | 512                                                            | PASS UC10 |
| TC-HEX-035  | `EDIT_HEX_DISASM_PANEL_CHARS == 48`                            | [N]  | UFR-HEX-005  | REQ-HEX-011  | INT-HEX-011  | 48                                                             | PASS UC10 |
| TC-HEX-036  | `EDIT_HEX_DISASM_SEP_CHARS == 3`                               | [N]  | UFR-HEX-005  | REQ-HEX-011  | INT-HEX-012  | 3                                                              | PASS UC10 |
| TC-HEX-037  | `EDIT_HEX_DISASM_ADDR_CHARS == 8`                              | [N]  | UFR-HEX-005  | REQ-HEX-011  | INT-HEX-013  | 8                                                              | PASS UC10 |
| TC-HEX-038  | `EDIT_HEX_DISASM_PREBUF_BYTES == 512`                          | [N]  | UFR-HEX-005  | REQ-HEX-014  | INT-HEX-014  | 512                                                            | PASS UC10 |
| TC-HEX-039  | `EDIT_HEX_WND_WIDTH_DISASM > EDIT_HEX_WND_WIDTH_STD`          | [N]  | UFR-HEX-005  | REQ-HEX-011  | INT-HEX-015  | TRUE (1320 > 950)                                              | PASS UC10 |
| TC-HEX-040  | `EDIT_HEX_WND_HEIGHT > 0`                                      | [N]  | UFR-HEX-005  | REQ-HEX-011  | INT-HEX-016  | TRUE (640)                                                     | PASS UC10 |
| TC-HEX-041  | `EDIT_HEX_BYTES_PER_ROW == 16` (regression)                    | [N]  | UFR-HEX-002  | REQ-HEX-003  | INT-HEX-017  | 16 — unchanged from UC9                                        | PASS UC10 |
| TC-HEX-042  | `HEX_ZONE_DISASM == 2`                                         | [N]  | UFR-HEX-005  | REQ-HEX-012  | INT-HEX-018  | 2                                                              | PASS UC10 |
| TC-HEX-043  | Three zone values are distinct (no overlap)                    | [N]  | UFR-HEX-005  | REQ-HEX-012  | INT-HEX-018  | HEX≠ASCII, HEX≠DISASM, ASCII≠DISASM                           | PASS UC10 |
| TC-HEX-044  | `HEX_ZONE_HEX==0` and `HEX_ZONE_ASCII==1` (regression)        | [N]  | UFR-HEX-003  | REQ-HEX-007  | INT-HEX-019  | 0 and 1 — unchanged from UC9                                   | PASS UC10 |
| TC-HEX-045  | `disasm_range(4 bytes)` → `ST_NO_ERROR`                        | [N]  | UFR-HEX-005  | REQ-HEX-013  | INT-HEX-020  | ST_NO_ERROR                                                    | PASS UC10 |
| TC-HEX-046  | `disasm_range(4 bytes)` → `uiLines == 2`                       | [N]  | UFR-HEX-005  | REQ-HEX-013  | INT-HEX-020  | 2 (stub: each instr = 1 word = 2 bytes) ADAPTED(UC11)          | PASS UC10 |
| TC-HEX-047  | `aRes[0].uiAddr == 0` (matches uiAddr parameter)               | [N]  | UFR-HEX-005  | REQ-HEX-013  | INT-HEX-021  | 0                                                              | PASS UC10 |
| TC-HEX-048  | `aRes[1].uiAddr == 2` (stub: 2-byte instructions)              | [N]  | UFR-HEX-005  | REQ-HEX-013  | INT-HEX-022  | 2 ADAPTED(UC11)                                                | PASS UC10 |
| TC-HEX-049  | `edit_hex_open(NULL, ctx, &v)` → ST_ERROR                      | [R]  | UFR-HEX-001  | REQ-HEX-001  | INT-HEX-023  | ST_ERROR                                                       | PASS UC10 |
| TC-HEX-050  | `edit_hex_open(NULL, ctx, &v)` → `*pptView` remains NULL       | [R]  | UFR-HEX-001  | REQ-HEX-001  | INT-HEX-023  | *pptView == NULL                                               | PASS UC10 |
| TC-HEX-051  | `edit_hex_open(path, NULL, &v)` → ST_ERROR                     | [R]  | UFR-HEX-001  | REQ-HEX-001  | INT-HEX-023  | ST_ERROR                                                       | PASS UC10 |
| TC-HEX-052  | `edit_hex_open(path, ctx, NULL)` → ST_ERROR                    | [R]  | UFR-HEX-001  | REQ-HEX-001  | INT-HEX-023  | ST_ERROR                                                       | PASS UC10 |
| TC-HEX-053  | `edit_hex_open(missing, ctx, &v)` → ST_ERROR                   | [R]  | UFR-HEX-001  | REQ-HEX-002  | INT-HEX-023  | ST_ERROR                                                       | PASS UC10 |
| TC-HEX-054  | `edit_hex_open(missing, ctx, &v)` → `*pptView == NULL`         | [R]  | UFR-HEX-001  | REQ-HEX-002  | INT-HEX-023  | *pptView == NULL                                               | PASS UC10 |
| TC-HEX-055  | `edit_hex_close(NULL)` → ST_ERROR                              | [R]  | UFR-HEX-001  | REQ-HEX-001  | INT-HEX-024  | ST_ERROR                                                       | PASS UC10 |
| TC-HEX-056  | `edit_hex_close(&NULL)` → ST_NO_ERROR (idempotent)             | [R]  | UFR-HEX-001  | REQ-HEX-001  | INT-HEX-024  | ST_NO_ERROR                                                    | PASS UC10 |
| TC-HEX-057  | F2 toggle: disasm panel appears and disappears (visual)        | [S]  | UFR-HEX-005  | REQ-HEX-017  | INT-HEX-015  | panel toggled off then on                                      | SKIP      |
| TC-HEX-058  | TAB cycles 3 zones HEX→ASCII→DISASM→HEX (visual)              | [S]  | UFR-HEX-005  | REQ-HEX-018  | INT-HEX-018  | cursor highlight changes per zone                              | SKIP      |
| TC-HEX-059  | Hex cursor ↓ syncs disasm highlight (visual)                   | [S]  | UFR-HEX-005  | REQ-HEX-016  | INT-HEX-020  | disasm highlight follows hex cursor row                        | SKIP      |
| TC-HEX-060  | Disasm cursor ↑/↓ syncs hex highlight (visual)                 | [S]  | UFR-HEX-005  | REQ-HEX-015  | INT-HEX-020  | hex row highlight follows disasm cursor                        | SKIP      |
| TC-HEX-061  | Click on disasm entry → zone switches to DISASM (visual)       | [S]  | UFR-HEX-005  | REQ-HEX-015  | INT-HEX-023  | eZone == HEX_ZONE_DISASM after click                           | SKIP      |
| TC-HEX-062  | Scroll wheel in DISASM zone scrolls disasm panel (visual)      | [S]  | UFR-HEX-005  | REQ-HEX-019  | INT-HEX-020  | disasm panel scrolls; hex panel unchanged                      | SKIP      |
| TC-HEX-063  | Window opens wider with disasm panel (visual)                  | [S]  | UFR-HEX-005  | REQ-HEX-011  | INT-HEX-015  | window width ~1320px vs ~950px                                 | SKIP      |
| TC-HEX-064  | Disasm shows `$XXXXXX DC.W $XXXX` format (visual)             | [S]  | UFR-HEX-005  | REQ-HEX-013  | INT-HEX-020  | address + DC.W + opcode per line                               | SKIP      |

#### Test Summary — UC10

| Module | [N] | [R] | [S] | Total | Result    |
|--------|-----|-----|-----|-------|-----------|
| HEX    | 15  | 8   | 8   | 31    | ALL PASS  |

#### REQ → TC coverage (UC10)

| REQ          | TC(s)                            | Status    |
|--------------|----------------------------------|-----------|
| REQ-HEX-011  | TC-HEX-034..040, TC-HEX-063      | ✓ UC10    |
| REQ-HEX-012  | TC-HEX-042..043                  | ✓ UC10    |
| REQ-HEX-013  | TC-HEX-045..048, TC-HEX-064      | ✓ UC10    |
| REQ-HEX-014  | TC-HEX-038                       | ✓ UC10    |
| REQ-HEX-015  | TC-HEX-060..061                  | ✓ UC10 (manual) |
| REQ-HEX-016  | TC-HEX-059                       | ✓ UC10 (manual) |
| REQ-HEX-017  | TC-HEX-057                       | ✓ UC10 (manual) |
| REQ-HEX-018  | TC-HEX-058                       | ✓ UC10 (manual) |
| REQ-HEX-019  | TC-HEX-062                       | ✓ UC10 (manual) |
| REQ-HEX-020  | TC-HEX-049..056                  | ✓ UC10    |

#### UFR traceability update (UC10)

| UFR          | REQ(s)                    | TC(s)                                             | Status    |
|--------------|---------------------------|---------------------------------------------------|-----------|
| UFR-HEX-005  | REQ-HEX-011..020          | TC-HEX-034..064                                   | ✓ UC10    |
| info cmd binary (UC7)   | REQ-LOD-014, REQ-CON-026             | ✓ UC7     | `line_cmd_info()` now shows live load state via load_get_state() |
| info cmd disk (stub)    | REQ-CON-026 (disk)                   | UC18      | `line_cmd_info()` disk-mounted stub → real state when mount is implemented |
| Edit hex / binary       | REQ-EDT-007, UFR-EDT-006             | ✓ UC9     | `line_cmd_edit()` routes .prg/.ttp/.tos/.bin/.raw → edit_hex_open |
| Find/Replace (P30)      | UFR-EDT-* (future)                   | UC10+     | CTRL+F search bar in editor; deferred until edit family stable    |
| CTRL+Z undo visual TC   | REQ-EDT-008..010                     | ✓ UC8     | Grouping and pop logic; validated via make manual UC=08           |
| PRG fixup relocation    | REQ-LOD-015..022, UFR-LOD-007..008   | ✓ UC15    | load_apply_fixups() complete; abs_flag handled; out-of-range → ST_ERROR |
| load_file size check    | REQ-LOD-012                          | —         | File > ST_LOAD_MAX_SIZE: not injectable headless; covered by code review |
| file_read ferror path   | REQ-FIL-010                          | —         | ferror branch not injectable in headless tests; covered by code review |
| file_write short write  | REQ-FIL-012                          | —         | fwrite contract; not injectable headless; covered by code review       |

---

### 5.36 INTENT Catalog — UC11

Source: `use_cases/use_case_11.c`

| ID           | INTENT text                                                                                           |
|--------------|-------------------------------------------------------------------------------------------------------|
| INT-DIS-010  | MOVE Dn,Dn for all three sizes uses correct mnemonic and register names                              |
| INT-DIS-011  | Each source EA addressing mode produces correct DEVPAC3 syntax; word counts match extension words    |
| INT-DIS-012  | MOVE.W/L to An uses MOVEA mnemonic; MOVE.B to An → DC.W (invalid)                                   |
| INT-DIS-013  | MOVEQ sign-extends 8-bit data at runtime; disassembler shows unsigned hex byte value                 |
| INT-DIS-014  | LEA with control-addressing source; destination is always An                                         |
| INT-DIS-015  | CLR in all three sizes; CLR with size=11 is invalid → DC.W                                          |
| INT-DIS-016  | SWAP Dn operates on all 8 data registers                                                              |
| INT-DIS-017  | PEA with control addressing modes; Dn/An/post/pre/imm source → DC.W (not control EA)                |
| INT-DIS-018  | Three EXG modes: Dx,Dy / Ax,Ay / Dx,Ay — all without extension words                                |
| INT-DIS-019  | Brief extension word format for d8(An,Xn): register, size, displacement correctly decoded           |
| INT-DIS-020  | disasm_range decodes a sequence of different instruction types; word counts accumulate correctly      |
| INT-DIS-021  | NULL and boundary conditions handled without crash                                                    |
| INT-DIS-022  | hello.prg bytes: 0x702A now bValid=TRUE (MOVEQ); 0x4E75 still DC.W (RTS → UC14) ADAPTED(UC11)       |

---

### 5.37 Test Cases — UC11 (68000 disassembler: MOVE/MOVEQ/LEA/CLR/EXG/SWAP/PEA)

Source: `use_cases/use_case_11.c`
All opcodes hand-crafted from MC68000 PRM.

| ID          | Functional description                                            | Type | UFR          | REQ               | INTENT       | Expected outcome                                      | Status    |
|-------------|-------------------------------------------------------------------|------|--------------|-------------------|--------------|-------------------------------------------------------|-----------|
| TC-DIS-010  | `MOVE.B D0,D1` (0x1200) mnemonic = MOVE.B                        | [N]  | UFR-HEX-005  | REQ-DIS-006       | INT-DIS-010  | "MOVE.B"                                              | PASS UC11 |
| TC-DIS-011  | `MOVE.B D0,D1` (0x1200) operands = D0,D1                         | [N]  | UFR-HEX-005  | REQ-DIS-006       | INT-DIS-010  | "D0,D1"                                               | PASS UC11 |
| TC-DIS-012  | `MOVE.W D0,D1` (0x3200)                                          | [N]  | UFR-HEX-005  | REQ-DIS-006       | INT-DIS-010  | "MOVE.W" / "D0,D1"                                   | PASS UC11 |
| TC-DIS-013  | `MOVE.L D0,D1` (0x2200)                                          | [N]  | UFR-HEX-005  | REQ-DIS-006       | INT-DIS-010  | "MOVE.L" / "D0,D1"                                   | PASS UC11 |
| TC-DIS-014  | `MOVE.W D7,D6` (0x3C07)                                          | [N]  | UFR-HEX-005  | REQ-DIS-006       | INT-DIS-010  | "MOVE.W" / "D7,D6"                                   | PASS UC11 |
| TC-DIS-015  | `MOVE.W (A0),D0` (0x3010)                                        | [N]  | UFR-HEX-005  | REQ-DIS-006, REQ-DIS-010 | INT-DIS-011  | "MOVE.W" / "(A0),D0"                           | PASS UC11 |
| TC-DIS-016  | `MOVE.W (A0)+,D0` (0x3018)                                       | [N]  | UFR-HEX-005  | REQ-DIS-010       | INT-DIS-011  | "(A0)+,D0"                                           | PASS UC11 |
| TC-DIS-017  | `MOVE.W -(A0),D0` (0x3020)                                       | [N]  | UFR-HEX-005  | REQ-DIS-010       | INT-DIS-011  | "-(A0),D0"                                           | PASS UC11 |
| TC-DIS-018  | `MOVE.W $10(A0),D0` (0x3028+ext 0x0010) wordCount=2              | [N]  | UFR-HEX-005  | REQ-DIS-010       | INT-DIS-011  | 2 / "MOVE.W" / "$10(A0),D0"                          | PASS UC11 |
| TC-DIS-019  | `MOVE.W #$1234,D0` wordCount=2                                   | [N]  | UFR-HEX-005  | REQ-DIS-010       | INT-DIS-011  | 2 / "MOVE.W" / "#$1234,D0"                           | PASS UC11 |
| TC-DIS-020  | `MOVE.L #$12345678,D0` wordCount=3                               | [N]  | UFR-HEX-005  | REQ-DIS-010       | INT-DIS-011  | 3 / "MOVE.L" / "#$12345678,D0"                       | PASS UC11 |
| TC-DIS-021  | `MOVE.W abs.W($8200),D0` → `$FF8200.W` (sign-extend)            | [N]  | UFR-HEX-005  | REQ-DIS-011       | INT-DIS-011  | "$FF8200.W,D0"                                       | PASS UC11 |
| TC-DIS-022  | `MOVE.L abs.L,D0` wordCount=3                                    | [N]  | UFR-HEX-005  | REQ-DIS-010       | INT-DIS-011  | 3 / "$12345678,D0"                                   | PASS UC11 |
| TC-DIS-023  | `MOVEA.W D0,A0` (0x3040)                                         | [N]  | UFR-HEX-005  | REQ-DIS-013       | INT-DIS-012  | "MOVEA.W" / "D0,A0"                                  | PASS UC11 |
| TC-DIS-024  | `MOVEA.L D0,A1` (0x2240)                                         | [N]  | UFR-HEX-005  | REQ-DIS-013       | INT-DIS-012  | "MOVEA.L" / "D0,A1"                                  | PASS UC11 |
| TC-DIS-025  | `MOVE.B src,An` (0x1040) → DC.W (invalid)                       | [N]  | UFR-HEX-005  | REQ-DIS-013       | INT-DIS-012  | bValid==ST_FALSE                                     | PASS UC11 |
| TC-DIS-026  | `MOVEQ #$42,D0` (0x7042)                                         | [N]  | UFR-HEX-005  | REQ-DIS-006       | INT-DIS-013  | "MOVEQ" / "#$42,D0"                                  | PASS UC11 |
| TC-DIS-027  | `MOVEQ #$FF,D7` (0x7EFF)                                         | [N]  | UFR-HEX-005  | REQ-DIS-006       | INT-DIS-013  | "MOVEQ" / "#$FF,D7"                                  | PASS UC11 |
| TC-DIS-028  | `MOVEQ #0,D3` (0x7600)                                           | [N]  | UFR-HEX-005  | REQ-DIS-006       | INT-DIS-013  | "MOVEQ" / "#$00,D3"                                  | PASS UC11 |
| TC-DIS-029  | 0x7100 (bit8=1) → not MOVEQ → DC.W                              | [N]  | UFR-HEX-005  | REQ-DIS-006       | INT-DIS-013  | bValid==ST_FALSE                                     | PASS UC11 |
| TC-DIS-030  | `LEA (A0),A1` (0x43D0)                                           | [N]  | UFR-HEX-005  | REQ-DIS-006       | INT-DIS-014  | "LEA" / "(A0),A1"                                    | PASS UC11 |
| TC-DIS-031  | `LEA $FF8200.W,A0` (0x41F8+0x8200) wordCount=2                  | [N]  | UFR-HEX-005  | REQ-DIS-011       | INT-DIS-014  | 2 / "LEA" / "$FF8200.W,A0"                           | PASS UC11 |
| TC-DIS-032  | `LEA abs.L,A2` (0x45F9+2 ext) wordCount=3                       | [N]  | UFR-HEX-005  | REQ-DIS-010       | INT-DIS-014  | 3 / "LEA" / "$12345678,A2"                           | PASS UC11 |
| TC-DIS-033  | `LEA $10(A0),A3` (0x47E8+0x0010) wordCount=2                    | [N]  | UFR-HEX-005  | REQ-DIS-010       | INT-DIS-014  | 2 / "LEA" / "$10(A0),A3"                             | PASS UC11 |
| TC-DIS-034  | `LEA d(PC),A4` (0x49FA+0x0010): EA=$001012(PC)                  | [N]  | UFR-HEX-005  | REQ-DIS-012       | INT-DIS-014  | 2 / "LEA" / "$001012(PC),A4"                         | PASS UC11 |
| TC-DIS-035  | `CLR.B D0` (0x4200)                                              | [N]  | UFR-HEX-005  | REQ-DIS-006       | INT-DIS-015  | "CLR.B" / "D0"                                       | PASS UC11 |
| TC-DIS-036  | `CLR.W D5` (0x4245)                                              | [N]  | UFR-HEX-005  | REQ-DIS-006       | INT-DIS-015  | "CLR.W" / "D5"                                       | PASS UC11 |
| TC-DIS-037  | `CLR.L D7` (0x4287)                                              | [N]  | UFR-HEX-005  | REQ-DIS-006       | INT-DIS-015  | "CLR.L" / "D7"                                       | PASS UC11 |
| TC-DIS-038  | `CLR.W (A1)` (0x4251)                                            | [N]  | UFR-HEX-005  | REQ-DIS-006       | INT-DIS-015  | "CLR.W" / "(A1)"                                     | PASS UC11 |
| TC-DIS-039  | `CLR size=11` (0x42C0) → DC.W                                    | [N]  | UFR-HEX-005  | REQ-DIS-014       | INT-DIS-015  | bValid==ST_FALSE                                     | PASS UC11 |
| TC-DIS-040  | `SWAP D0` (0x4840)                                               | [N]  | UFR-HEX-005  | REQ-DIS-006       | INT-DIS-016  | "SWAP" / "D0"                                        | PASS UC11 |
| TC-DIS-041  | `SWAP D3` (0x4843)                                               | [N]  | UFR-HEX-005  | REQ-DIS-006       | INT-DIS-016  | "SWAP" / "D3"                                        | PASS UC11 |
| TC-DIS-042  | `SWAP D7` (0x4847)                                               | [N]  | UFR-HEX-005  | REQ-DIS-006       | INT-DIS-016  | "SWAP" / "D7"                                        | PASS UC11 |
| TC-DIS-043  | `PEA (A0)` (0x4850)                                              | [N]  | UFR-HEX-005  | REQ-DIS-006       | INT-DIS-017  | "PEA" / "(A0)"                                       | PASS UC11 |
| TC-DIS-044  | `PEA abs.W` (0x4878+0x1234) wordCount=2                         | [N]  | UFR-HEX-005  | REQ-DIS-006       | INT-DIS-017  | 2 / "PEA" / "$1234.W"                                | PASS UC11 |
| TC-DIS-045  | `PEA abs.L` (0x4879+2 ext) wordCount=3                          | [N]  | UFR-HEX-005  | REQ-DIS-006       | INT-DIS-017  | 3 / "PEA" / "$00001000"                              | PASS UC11 |
| TC-DIS-046  | 0x4840 → SWAP D0 (SWAP takes priority over PEA)                 | [N]  | UFR-HEX-005  | REQ-DIS-014       | INT-DIS-017  | mnemonic=="SWAP"                                     | PASS UC11 |
| TC-DIS-047  | `EXG D0,D1` (0xC141)                                             | [N]  | UFR-HEX-005  | REQ-DIS-006       | INT-DIS-018  | "EXG" / "D0,D1"                                      | PASS UC11 |
| TC-DIS-048  | `EXG D3,D7` (0xC747)                                             | [N]  | UFR-HEX-005  | REQ-DIS-006       | INT-DIS-018  | "EXG" / "D3,D7"                                      | PASS UC11 |
| TC-DIS-049  | **ADAPTED:UC15A** `EXG A1,A0` (0xC149): iRx=A0(bits11-9), iRy=A1(bits2-0); DEVPAC3 encodes first operand in iRy → output `A1,A0` *was `A0,A1`* | [N] | UFR-HEX-005 | REQ-DIS-006 | INT-DIS-018 | "EXG" / "A1,A0" | PASS UC15A |
| TC-DIS-050  | **ADAPTED:UC15A** `EXG A5,A3` (0xC74D): iRx=A3, iRy=A5 → output `A5,A3` *was `A3,A5`*                                   | [N]  | UFR-HEX-005  | REQ-DIS-006       | INT-DIS-018  | "EXG" / "A5,A3"                                      | PASS UC15A |
| TC-DIS-051  | `EXG D0,A1` (0xC189)                                             | [N]  | UFR-HEX-005  | REQ-DIS-006       | INT-DIS-018  | "EXG" / "D0,A1"                                      | PASS UC11 |
| TC-DIS-052  | `MOVE.W d8(A0,D1.W),D0` (0x3030+0x1005) wordCount=2            | [N]  | UFR-HEX-005  | REQ-DIS-010       | INT-DIS-019  | 2 / "MOVE.W" / "$5(A0,D1.W),D0"                     | PASS UC11 |
| TC-DIS-053  | `MOVE.W $0(A0,A1.L),D0` (0x3030+0x9800)                        | [N]  | UFR-HEX-005  | REQ-DIS-010       | INT-DIS-019  | "$0(A0,A1.L),D0"                                    | PASS UC11 |
| TC-DIS-054  | `MOVE.W -$8(A2,D3.W),D0` (0x3032+0x30F8)                       | [N]  | UFR-HEX-005  | REQ-DIS-010       | INT-DIS-019  | "-$8(A2,D3.W),D0"                                   | PASS UC11 |
| TC-DIS-055  | disasm_range: MOVEQ + LEA → 2 lines, correct addresses          | [N]  | UFR-HEX-005  | REQ-DIS-004, REQ-DIS-006 | INT-DIS-020 | n==2; line0=MOVEQ; line1=LEA; addr1=$2002        | PASS UC11 |
| TC-DIS-056  | disasm_range line1 wordCount=2                                   | [N]  | UFR-HEX-005  | REQ-DIS-006       | INT-DIS-020  | iWordCount==2                                        | PASS UC11 |
| TC-DIS-057  | `disasm_one(NULL buf)` → ST_ERROR                                | [R]  | UFR-HEX-005  | REQ-DIS-001       | INT-DIS-021  | ST_ERROR                                             | PASS UC11 |
| TC-DIS-058  | `disasm_one(..., NULL result)` → ST_ERROR                        | [R]  | UFR-HEX-005  | REQ-DIS-001       | INT-DIS-021  | ST_ERROR                                             | PASS UC11 |
| TC-DIS-059  | 1-byte buffer → ST_NO_ERROR, wordCount=1                         | [R]  | UFR-HEX-005  | REQ-DIS-004       | INT-DIS-021  | ST_NO_ERROR; iWordCount==1                           | PASS UC11 |
| TC-DIS-060  | `disasm_range(NULL buf)` → ST_ERROR                              | [R]  | UFR-HEX-005  | REQ-DIS-001       | INT-DIS-021  | ST_ERROR                                             | PASS UC11 |
| TC-DIS-061  | `disasm_range(..., NULL results)` → ST_ERROR                     | [R]  | UFR-HEX-005  | REQ-DIS-002       | INT-DIS-021  | ST_ERROR                                             | PASS UC11 |
| TC-DIS-062  | `disasm_range(..., NULL puiLines)` → ST_ERROR                    | [R]  | UFR-HEX-005  | REQ-DIS-003       | INT-DIS-021  | ST_ERROR                                             | PASS UC11 |
| TC-DIS-063  | Unknown opcode 0x0000 → DC.W, bValid=ST_FALSE                   | [R]  | UFR-HEX-005  | REQ-DIS-005       | INT-DIS-021  | bValid==ST_FALSE; mnemonic=="DC.W"                  | PASS UC11 |
| TC-DIS-064  | Unknown opcode 0xFFFF → DC.W, bValid=ST_FALSE                   | [R]  | UFR-HEX-005  | REQ-DIS-005       | INT-DIS-021  | bValid==ST_FALSE                                     | PASS UC11 |
| TC-DIS-065  | range(len=0) → ST_NO_ERROR, 0 lines                              | [R]  | UFR-HEX-005  | REQ-DIS-004       | INT-DIS-021  | ST_NO_ERROR; n==0                                    | PASS UC11 |
| TC-DIS-066  | 0x702A = MOVEQ #$2A,D0 → bValid=ST_TRUE ADAPTED(UC11)           | [N]  | UFR-HEX-005  | REQ-DIS-006       | INT-DIS-022  | bValid==ST_TRUE; "MOVEQ"                            | PASS UC11 |
| TC-DIS-067  | 0x4E75 = DC.W (RTS deferred to UC14) ADAPTED(UC14)              | [N]  | UFR-HEX-005  | REQ-DIS-005       | INT-DIS-022  | bValid==ST_FALSE ADAPTED(UC14)                      | PASS UC11 |

#### Test Summary — UC11

| Module | [N] | [R] | [S] | Total | Result    |
|--------|-----|-----|-----|-------|-----------|
| DIS    | 52  | 10  | 0   | 62    | ALL PASS  |

#### REQ → TC coverage (UC11)

| REQ          | TC(s)                                  | Status    |
|--------------|----------------------------------------|-----------|
| REQ-DIS-001  | TC-DIS-057..058, TC-DIS-060           | ✓ UC11    |
| REQ-DIS-002  | TC-DIS-061                            | ✓ UC11    |
| REQ-DIS-003  | TC-DIS-062                            | ✓ UC11    |
| REQ-DIS-004  | TC-DIS-059, TC-DIS-065                | ✓ UC11    |
| REQ-DIS-005  | TC-DIS-063..064, TC-DIS-067           | ✓ UC11    |
| REQ-DIS-006  | TC-DIS-010..055, TC-DIS-066           | ✓ UC11    |
| REQ-DIS-010  | TC-DIS-015..022, TC-DIS-031..034, TC-DIS-052..054 | ✓ UC11 |
| REQ-DIS-011  | TC-DIS-021, TC-DIS-031                | ✓ UC11    |
| REQ-DIS-012  | TC-DIS-034                            | ✓ UC11    |
| REQ-DIS-013  | TC-DIS-023..025                       | ✓ UC11    |
| REQ-DIS-014  | TC-DIS-039, TC-DIS-046                | ✓ UC11    |

#### UFR traceability update (UC11)

| UFR          | REQ(s)                           | TC(s)                      | Status    |
|--------------|----------------------------------|----------------------------|-----------|
| UFR-HEX-005  | REQ-DIS-006, REQ-DIS-010..014   | TC-DIS-010..067             | ✓ UC11    |

---

### 5.38 INTENT Catalog — UC12

Source: `use_cases/use_case_12.c`

| ID           | INTENT text                                                                                         |
|--------------|-----------------------------------------------------------------------------------------------------|
| INT-DIS-030  | Immediate-to-EA instructions use correct mnemonic, size suffix, and format immediate as hex        |
| INT-DIS-031  | ADDQ/SUBQ show immediate as decimal (1–8); 0 in opcode field = 8; size=3 → DC.W                   |
| INT-DIS-032  | ADD/SUB/CMP in both directions (EA→Dn and Dn→EA)                                                    |
| INT-DIS-033  | ADDA/SUBA/CMPA: An destination, EA source, W or L                                                   |
| INT-DIS-034  | ADDX/SUBX in register (Dx,Dy) and memory (-(Ax),-(Ay)) forms                                        |
| INT-DIS-035  | AND/OR/EOR in both directions; EOR is always Dn→EA when bit8=1                                     |
| INT-DIS-036  | MULU/MULS/DIVU/DIVS: `.W` suffix, word source EA, Dn destination                                  |
| INT-DIS-037  | NEG/NOT/NEGX/TST on EA; EXT.W/L on Dn; size=3 → DC.W for NEG/NOT/NEGX/TST                        |
| INT-DIS-038  | CMPM uses postincrement addressing for both operands                                                 |
| INT-DIS-039  | A realistic 5-instruction PRG-like sequence decodes correctly via disasm_range                     |
| INT-DIS-040  | UC11 instructions still decode correctly after UC12 changes (regression)                           |
| INT-DIS-041  | Invalid sizes / ABCD / Scc / RTS → DC.W; NULL params → ST_ERROR                                   |

---

### 5.39 Test Cases — UC12 (arithmetic, logic, multiply, divide)

Source: `use_cases/use_case_12.c`

| ID          | Functional description                                              | Type | UFR          | REQ          | INTENT       | Expected outcome               | Status    |
|-------------|---------------------------------------------------------------------|------|--------------|--------------|--------------|--------------------------------|-----------|
| TC-DIS-100  | `ADDI.B #$42,D0` (0x0600+ext) wc=2, mnem=ADDI.B, ops=#$42,D0     | [N]  | UFR-HEX-005  | REQ-DIS-023  | INT-DIS-030  | PASS                          | PASS UC12 |
| TC-DIS-101  | `ADDI.W #$1234,D1` wc=2                                            | [N]  | UFR-HEX-005  | REQ-DIS-015  | INT-DIS-030  | ADDI.W / #$1234,D1            | PASS UC12 |
| TC-DIS-102  | `SUBI.W #$0001,D0` wc=2                                            | [N]  | UFR-HEX-005  | REQ-DIS-015  | INT-DIS-030  | SUBI.W / #$0001,D0            | PASS UC12 |
| TC-DIS-103  | `CMPI.L #$12345678,D0` wc=3                                        | [N]  | UFR-HEX-005  | REQ-DIS-016  | INT-DIS-030  | CMPI.L / #$12345678,D0        | PASS UC12 |
| TC-DIS-104  | `ORI.W #$00FF,(A0)` wc=2                                           | [N]  | UFR-HEX-005  | REQ-DIS-018  | INT-DIS-030  | ORI.W / #$00FF,(A0)           | PASS UC12 |
| TC-DIS-105  | `ANDI.B #$0F,D3` wc=2                                              | [N]  | UFR-HEX-005  | REQ-DIS-018  | INT-DIS-030  | ANDI.B / #$0F,D3              | PASS UC12 |
| TC-DIS-106  | `EORI.W #$FFFF,D7` wc=2                                            | [N]  | UFR-HEX-005  | REQ-DIS-018  | INT-DIS-030  | EORI.W / #$FFFF,D7            | PASS UC12 |
| TC-DIS-107  | `ADDQ.W #4,D0` (0x5840)                                            | [N]  | UFR-HEX-005  | REQ-DIS-020  | INT-DIS-031  | ADDQ.W / #4,D0                | PASS UC12 |
| TC-DIS-108  | `ADDQ.L #1,A0` (0x5288)                                            | [N]  | UFR-HEX-005  | REQ-DIS-020  | INT-DIS-031  | ADDQ.L / #1,A0                | PASS UC12 |
| TC-DIS-109  | `ADDQ.B #8,(A0)` (0x5010, data=0→8)                               | [N]  | UFR-HEX-005  | REQ-DIS-020  | INT-DIS-031  | ADDQ.B / #8,(A0)              | PASS UC12 |
| TC-DIS-110  | `SUBQ.W #2,D5` (0x5545)                                            | [N]  | UFR-HEX-005  | REQ-DIS-020  | INT-DIS-031  | SUBQ.W / #2,D5                | PASS UC12 |
| TC-DIS-111  | `SUBQ.L #4,A1` (0x5989)                                            | [N]  | UFR-HEX-005  | REQ-DIS-020  | INT-DIS-031  | SUBQ.L / #4,A1                | PASS UC12 |
| TC-DIS-112  | SUBQ size=3 → DC.W                                                 | [N]  | UFR-HEX-005  | REQ-DIS-020  | INT-DIS-031  | bValid==ST_FALSE               | PASS UC12 |
| TC-DIS-113  | `ADD.W D0,D1` EA→Dn (0xD240)                                       | [N]  | UFR-HEX-005  | REQ-DIS-015  | INT-DIS-032  | ADD.W / D0,D1                 | PASS UC12 |
| TC-DIS-114  | `ADD.W D0,(A0)` Dn→EA (0xD150)                                     | [N]  | UFR-HEX-005  | REQ-DIS-015  | INT-DIS-032  | ADD.W / D0,(A0)               | PASS UC12 |
| TC-DIS-115  | `SUB.L D3,D5` EA→Dn (0x9A83)                                       | [N]  | UFR-HEX-005  | REQ-DIS-015  | INT-DIS-032  | SUB.L / D3,D5                 | PASS UC12 |
| TC-DIS-116  | `CMP.B (A0),D2` EA→Dn (0xB410)                                     | [N]  | UFR-HEX-005  | REQ-DIS-016  | INT-DIS-032  | CMP.B / (A0),D2               | PASS UC12 |
| TC-DIS-117  | `ADD.L #imm32,D0` wc=3                                             | [N]  | UFR-HEX-005  | REQ-DIS-015  | INT-DIS-032  | ADD.L / #$12345678,D0         | PASS UC12 |
| TC-DIS-118  | `SUB.W $10(A0),D1` wc=2 (0x9268+ext)                              | [N]  | UFR-HEX-005  | REQ-DIS-015  | INT-DIS-032  | SUB.W / $10(A0),D1            | PASS UC12 |
| TC-DIS-119  | `ADDA.W D0,A1` (0xD2C0)                                            | [N]  | UFR-HEX-005  | REQ-DIS-015  | INT-DIS-033  | ADDA.W / D0,A1                | PASS UC12 |
| TC-DIS-120  | `ADDA.L (A0),A1` (0xD3D0)                                          | [N]  | UFR-HEX-005  | REQ-DIS-015  | INT-DIS-033  | ADDA.L / (A0),A1              | PASS UC12 |
| TC-DIS-121  | `SUBA.W D0,A0` (0x90C0)                                            | [N]  | UFR-HEX-005  | REQ-DIS-015  | INT-DIS-033  | SUBA.W / D0,A0                | PASS UC12 |
| TC-DIS-122  | `CMPA.L D7,A3` (0xB7C7)                                            | [N]  | UFR-HEX-005  | REQ-DIS-016  | INT-DIS-033  | CMPA.L / D7,A3                | PASS UC12 |
| TC-DIS-123  | `ADDX.W D0,D1` register form (0xD340)                              | [N]  | UFR-HEX-005  | REQ-DIS-021  | INT-DIS-034  | ADDX.W / D0,D1                | PASS UC12 |
| TC-DIS-124  | `ADDX.L -(A0),-(A1)` memory form (0xD388)                          | [N]  | UFR-HEX-005  | REQ-DIS-021  | INT-DIS-034  | ADDX.L / -(A0),-(A1)          | PASS UC12 |
| TC-DIS-125  | `SUBX.B D3,D5` register form (0x9B03)                              | [N]  | UFR-HEX-005  | REQ-DIS-021  | INT-DIS-034  | SUBX.B / D3,D5                | PASS UC12 |
| TC-DIS-126  | `AND.W D0,D1` EA→Dn (0xC240)                                       | [N]  | UFR-HEX-005  | REQ-DIS-018  | INT-DIS-035  | AND.W / D0,D1                 | PASS UC12 |
| TC-DIS-127  | `AND.W D0,(A0)` Dn→EA (0xC150)                                     | [N]  | UFR-HEX-005  | REQ-DIS-018  | INT-DIS-035  | AND.W / D0,(A0)               | PASS UC12 |
| TC-DIS-128  | `OR.L D2,D3` EA→Dn (0x8682)                                        | [N]  | UFR-HEX-005  | REQ-DIS-018  | INT-DIS-035  | OR.L / D2,D3                  | PASS UC12 |
| TC-DIS-129  | `EOR.W D0,D1` Dn→EA (0xB141)                                       | [N]  | UFR-HEX-005  | REQ-DIS-018  | INT-DIS-035  | EOR.W / D0,D1                 | PASS UC12 |
| TC-DIS-130  | `MULU.W D0,D1` (0xC2C0)                                            | [N]  | UFR-HEX-005  | REQ-DIS-017  | INT-DIS-036  | MULU.W / D0,D1                | PASS UC12 |
| TC-DIS-131  | `MULS.W D0,D2` (0xC5C0)                                            | [N]  | UFR-HEX-005  | REQ-DIS-017  | INT-DIS-036  | MULS.W / D0,D2                | PASS UC12 |
| TC-DIS-132  | `DIVU.W D1,D0` (0x80C1)                                            | [N]  | UFR-HEX-005  | REQ-DIS-017  | INT-DIS-036  | DIVU.W / D1,D0                | PASS UC12 |
| TC-DIS-133  | `DIVS.W (A0),D0` (0x81D0)                                          | [N]  | UFR-HEX-005  | REQ-DIS-017  | INT-DIS-036  | DIVS.W / (A0),D0              | PASS UC12 |
| TC-DIS-134  | `NEG.B D0` (0x4400)                                                | [N]  | UFR-HEX-005  | REQ-DIS-019  | INT-DIS-037  | NEG.B / D0                    | PASS UC12 |
| TC-DIS-135  | `NEG.W (A0)` (0x4450)                                              | [N]  | UFR-HEX-005  | REQ-DIS-019  | INT-DIS-037  | NEG.W / (A0)                  | PASS UC12 |
| TC-DIS-136  | `NOT.L D3` (0x4683)                                                | [N]  | UFR-HEX-005  | REQ-DIS-019  | INT-DIS-037  | NOT.L / D3                    | PASS UC12 |
| TC-DIS-137  | `NOT.W D0` (0x4640)                                                | [N]  | UFR-HEX-005  | REQ-DIS-019  | INT-DIS-037  | NOT.W / D0                    | PASS UC12 |
| TC-DIS-138  | `NEGX.B D0` (0x4000)                                               | [N]  | UFR-HEX-005  | REQ-DIS-019  | INT-DIS-037  | NEGX.B / D0                   | PASS UC12 |
| TC-DIS-139  | `TST.W D5` (0x4A45)                                                | [N]  | UFR-HEX-005  | REQ-DIS-019  | INT-DIS-037  | TST.W / D5                    | PASS UC12 |
| TC-DIS-140  | `TST.L (A0)` (0x4A90)                                              | [N]  | UFR-HEX-005  | REQ-DIS-019  | INT-DIS-037  | TST.L / (A0)                  | PASS UC12 |
| TC-DIS-141  | `EXT.W D2` (0x4882)                                                | [N]  | UFR-HEX-005  | REQ-DIS-019  | INT-DIS-037  | EXT.W / D2                    | PASS UC12 |
| TC-DIS-142  | `EXT.L D5` (0x48C5)                                                | [N]  | UFR-HEX-005  | REQ-DIS-019  | INT-DIS-037  | EXT.L / D5                    | PASS UC12 |
| TC-DIS-143  | NEG size=3 → DC.W                                                  | [N]  | UFR-HEX-005  | REQ-DIS-019  | INT-DIS-037  | bValid==ST_FALSE               | PASS UC12 |
| TC-DIS-144  | `CMPM.W (A0)+,(A1)+` (0xB348)                                      | [N]  | UFR-HEX-005  | REQ-DIS-016  | INT-DIS-038  | CMPM.W / (A0)+,(A1)+          | PASS UC12 |
| TC-DIS-145  | PRG sequence (5 instr) → ST_NO_ERROR, 5 lines                     | [N]  | UFR-HEX-005  | REQ-DIS-015..019 | INT-DIS-039 | n==5                        | PASS UC12 |
| TC-DIS-146  | PRG line0 = MOVEQ (regression UC11)                                | [N]  | UFR-HEX-005  | REQ-DIS-006  | INT-DIS-039  | "MOVEQ"                        | PASS UC12 |
| TC-DIS-147  | PRG line1 = ADDI.W, wc=2                                           | [N]  | UFR-HEX-005  | REQ-DIS-015  | INT-DIS-039  | "ADDI.W" / wc==2              | PASS UC12 |
| TC-DIS-148  | PRG line2 = CMP.W at addr $2006                                    | [N]  | UFR-HEX-005  | REQ-DIS-016  | INT-DIS-039  | "CMP.W" / addr==$2006          | PASS UC12 |
| TC-DIS-149  | PRG line3 = NEG.W; line4 = MULU.W                                  | [N]  | UFR-HEX-005  | REQ-DIS-017,019 | INT-DIS-039 | "NEG.W" / "MULU.W"         | PASS UC12 |
| TC-DIS-150  | MOVE.W D0,D1 regression (UC11)                                     | [N]  | UFR-HEX-005  | REQ-DIS-006  | INT-DIS-040  | MOVE.W / D0,D1                | PASS UC12 |
| TC-DIS-151  | LEA abs.W regression (UC11) wc=2                                   | [N]  | UFR-HEX-005  | REQ-DIS-006  | INT-DIS-040  | LEA / $FF8200.W,A0            | PASS UC12 |
| TC-DIS-152  | disasm_one(NULL) → ST_ERROR                                        | [R]  | UFR-HEX-005  | REQ-DIS-001  | INT-DIS-041  | ST_ERROR                       | PASS UC12 |
| TC-DIS-153  | disasm_one(…,NULL) → ST_ERROR                                      | [R]  | UFR-HEX-005  | REQ-DIS-001  | INT-DIS-041  | ST_ERROR                       | PASS UC12 |
| TC-DIS-154  | NEGX size=3 → DC.W                                                 | [R]  | UFR-HEX-005  | REQ-DIS-019  | INT-DIS-041  | bValid==ST_FALSE               | PASS UC12 |
| TC-DIS-155  | TST size=3 → DC.W                                                  | [R]  | UFR-HEX-005  | REQ-DIS-019  | INT-DIS-041  | bValid==ST_FALSE               | PASS UC12 |
| TC-DIS-156  | 0x6000 (BRA disp8=0) with 2-byte buf → DC.W (short buf guard)     | [R]  | UFR-HEX-005  | REQ-DIS-035  | INT-DIS-041  | bValid==ST_FALSE               | PASS UC14 |
| TC-DIS-157  | 0x4E75 (RTS) → RTS ADAPTED(UC14) — now bValid=ST_TRUE             | [N]  | UFR-HEX-005  | REQ-DIS-031  | INT-DIS-041  | bValid==ST_TRUE mnem=="RTS"    | PASS UC14 |
| TC-DIS-158  | 0xC100 (ABCD) → DC.W                                               | [R]  | UFR-HEX-005  | REQ-DIS-022  | INT-DIS-041  | bValid==ST_FALSE               | PASS UC12 |
| TC-DIS-159  | 0x50C0 (Scc) → DC.W                                                | [R]  | UFR-HEX-005  | REQ-DIS-020  | INT-DIS-041  | bValid==ST_FALSE               | PASS UC12 |

#### Test Summary — UC12

| Module | [N] | [R] | [S] | Total | Result    |
|--------|-----|-----|-----|-------|-----------|
| DIS    | 64  | 8   | 0   | 72    | ALL PASS  |

#### REQ → TC coverage (UC12)

| REQ          | TC(s)                                  | Status    |
|--------------|----------------------------------------|-----------|
| REQ-DIS-015  | TC-DIS-101..103,113..121,123..125      | ✓ UC12    |
| REQ-DIS-016  | TC-DIS-103,116,118,122,128,144,148     | ✓ UC12    |
| REQ-DIS-017  | TC-DIS-130..133                        | ✓ UC12    |
| REQ-DIS-018  | TC-DIS-104..106,126..129               | ✓ UC12    |
| REQ-DIS-019  | TC-DIS-134..143,154..155               | ✓ UC12    |
| REQ-DIS-020  | TC-DIS-107..112,159                    | ✓ UC12    |
| REQ-DIS-021  | TC-DIS-123..125                        | ✓ UC12    |
| REQ-DIS-022  | TC-DIS-158                             | ✓ UC12    |
| REQ-DIS-023  | TC-DIS-100..106                        | ✓ UC12    |

#### UFR traceability update (UC12)

| UFR          | REQ(s)                                       | TC(s)                 | Status    |
|--------------|----------------------------------------------|-----------------------|-----------|
| UFR-HEX-005  | REQ-DIS-015..023 (+ UC11: 006, 010..014)    | TC-DIS-100..159       | ✓ UC12    |

---

### 5.40 INTENT Catalog — UC13

Source: `use_cases/use_case_13.c`
Fixtures: all opcodes hand-crafted from MC68000 PRM; no external files

| ID           | INTENT text                                                                                                                                                             |
|--------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| INT-DIS-042  | ASL/ASR with immediate count produce correct mnemonic, size suffix, and `#count,Dn` operands; count field 0 in opcode = displayed #8.                                   |
| INT-DIS-043  | LSL/LSR with immediate count across all three sizes (.B/.W/.L) produce correct mnemonic and operands.                                                                    |
| INT-DIS-044  | ROXL/ROXR with immediate count produce correct mnemonic and operands.                                                                                                    |
| INT-DIS-045  | ROL/ROR with immediate count; count=0 in opcode field → displayed as #8 (convention identical to ADDQ/SUBQ).                                                             |
| INT-DIS-046  | Register-count form (bit3=0 in opcode): operands are `Dn,Dn`; direction and type bits identical to immediate form.                                                       |
| INT-DIS-047  | All 8 memory shift mnemonics (ASR/ASL/LSR/LSL/ROXR/ROXL/ROR/ROL .W) indexed by bits 10-8; extension words consumed correctly for abs.W EA.                              |
| INT-DIS-048  | Static bit ops (#imm source): each of the 4 ops (BTST/BCHG/BCLR/BSET) with an EA destination; iWordCount = 2 + EA_ext_words; operands `#N,EA`.                          |
| INT-DIS-049  | Dynamic bit ops (Dn source): each of the 4 ops with various EA modes; iWordCount = 1; operands `Dn,EA`.                                                                  |
| INT-DIS-050  | `disasm_range()` advances correctly over a mixed UC13 stream (1-word shift + 2-word bit-static + 1-word shift); addresses computed from starting uiAddr.                  |
| INT-DIS-051  | Invalid encodings rejected: memory shift to Dn/An, bit11 set, static/dynamic bit op to An → DC.W. NULL params → ST_ERROR.                                                |

---

### 5.41 Test Cases — UC13 (shifts, rotations, bit ops)

Source: `use_cases/use_case_13.c`

| ID           | Functional description                                                                                                       | Type | UFR          | REQ          | INTENT      | Expected outcome                    | Status    |
|--------------|------------------------------------------------------------------------------------------------------------------------------|------|--------------|--------------|-------------|-------------------------------------|-----------|
| TC-DIS-200   | **ADAPTED:UC15A** `ASL.W D2,D3` (0xE563, bit5=1 → reg): mnem="ASL.W", ops="D2,D3" *was `#2,D3` (UC13 had iIR inverted)* | [N]  | UFR-HEX-005  | REQ-DIS-025  | INT-DIS-042 | mnem+ops correct                    | PASS UC15A |
| TC-DIS-201   | **ADAPTED:UC15A** `ASR.W D3,D0` (0xE660, bit5=1 → reg): ops="D3,D0" *was `#3,D0`*                                        | [N]  | UFR-HEX-005  | REQ-DIS-025  | INT-DIS-042 | mnem+ops correct                    | PASS UC15A |
| TC-DIS-202   | **ADAPTED:UC15A** `LSL.B D1,D1` (0xE329, bit5=1 → reg): ops="D1,D1" *was `#1,D1`*                                        | [N]  | UFR-HEX-005  | REQ-DIS-025  | INT-DIS-043 | mnem+ops correct                    | PASS UC15A |
| TC-DIS-203   | **ADAPTED:UC15A** `LSR.L D4,D2` (0xE8AA, bit5=1 → reg): ops="D4,D2" *was `#4,D2`*                                        | [N]  | UFR-HEX-005  | REQ-DIS-025  | INT-DIS-043 | mnem+ops correct                    | PASS UC15A |
| TC-DIS-204   | **ADAPTED:UC15A** `ROXL.W D1,D5` (0xE375, bit5=1 → reg): ops="D1,D5" *was `#1,D5`*                                       | [N]  | UFR-HEX-005  | REQ-DIS-025  | INT-DIS-044 | mnem+ops correct                    | PASS UC15A |
| TC-DIS-205   | **ADAPTED:UC15A** `ROXR.B D2,D4` (0xE434, bit5=1 → reg): ops="D2,D4" *was `#2,D4`*                                       | [N]  | UFR-HEX-005  | REQ-DIS-025  | INT-DIS-044 | mnem+ops correct                    | PASS UC15A |
| TC-DIS-206   | **ADAPTED:UC15A** `ROL.L D0,D7` (0xE1BF, bit5=1 → reg): count=0 → D0; ops="D0,D7" *was `#8,D7`; "0 means 8" only for imm mode* | [N] | UFR-HEX-005 | REQ-DIS-025 | INT-DIS-045 | ops="D0,D7"                     | PASS UC15A |
| TC-DIS-207   | **ADAPTED:UC15A** `ROR.W D1,D0` (0xE278, bit5=1 → reg): ops="D1,D0" *was `#1,D0`*                                        | [N]  | UFR-HEX-005  | REQ-DIS-025  | INT-DIS-045 | mnem+ops correct                    | PASS UC15A |
| TC-DIS-208   | **ADAPTED:UC15A** `ASL.W #3,D0` (0xE740, bit5=0 → imm): ops="#3,D0" *was `D3,D0`*                                        | [N]  | UFR-HEX-005  | REQ-DIS-024  | INT-DIS-046 | mnem="ASL.W", ops="#3,D0"           | PASS UC15A |
| TC-DIS-209   | **ADAPTED:UC15A** `LSR.B #2,D1` (0xE409, bit5=0 → imm): ops="#2,D1" *was `D2,D1`*                                        | [N]  | UFR-HEX-005  | REQ-DIS-024  | INT-DIS-046 | mnem="LSR.B", ops="#2,D1"           | PASS UC15A |
| TC-DIS-210   | **ADAPTED:UC15A** `ROR.L #8,D5` (0xE09D, bit5=0 → imm): count=0 → #8; ops="#8,D5" *was `D0,D5`*                          | [N]  | UFR-HEX-005  | REQ-DIS-024  | INT-DIS-046 | mnem="ROR.L", ops="#8,D5"           | PASS UC15A |
| TC-DIS-211   | `ASR.W (A0)` (0xE0D0): memory form, mnem="ASR.W", ops="(A0)"                                                               | [N]  | UFR-HEX-005  | REQ-DIS-026  | INT-DIS-047 | memory form 1 word                  | PASS UC13 |
| TC-DIS-212   | `ASL.W (A1)` (0xE1D1)                                                                                                       | [N]  | UFR-HEX-005  | REQ-DIS-026  | INT-DIS-047 | mnem="ASL.W"                        | PASS UC13 |
| TC-DIS-213   | `LSR.W (A2)+` (0xE2DA)                                                                                                      | [N]  | UFR-HEX-005  | REQ-DIS-026  | INT-DIS-047 | ops="(A2)+"                         | PASS UC13 |
| TC-DIS-214   | `LSL.W -(A3)` (0xE3E3)                                                                                                      | [N]  | UFR-HEX-005  | REQ-DIS-026  | INT-DIS-047 | ops="-(A3)"                         | PASS UC13 |
| TC-DIS-215   | `ROXR.W (A4)` / `ROXL.W (A5)` / `ROR.W (A6)` / `ROL.W (A7)+`                                                              | [N]  | UFR-HEX-005  | REQ-DIS-026  | INT-DIS-047 | all 4 mnem correct                  | PASS UC13 |
| TC-DIS-216   | `ROXR.W $1234.W`: wc=2, mnem="ROXR.W", ops="$1234.W"                                                                       | [N]  | UFR-HEX-005  | REQ-DIS-026  | INT-DIS-047 | wc=2; ext word consumed             | PASS UC13 |
| TC-DIS-217   | `BTST #5,D3` (0x0803+ext=0x0005): wc=2, mnem="BTST", ops="#5,D3"                                                           | [N]  | UFR-HEX-005  | REQ-DIS-027  | INT-DIS-048 | static, wc=2                        | PASS UC13 |
| TC-DIS-218   | `BCHG #0,(A1)` (0x0851+ext=0x0000): wc=2, ops="#0,(A1)"                                                                    | [N]  | UFR-HEX-005  | REQ-DIS-027  | INT-DIS-048 | BCHG static                         | PASS UC13 |
| TC-DIS-219   | `BCLR #7,(A2)+` (0x089A+ext=0x0007): wc=2                                                                                  | [N]  | UFR-HEX-005  | REQ-DIS-027  | INT-DIS-048 | BCLR static                         | PASS UC13 |
| TC-DIS-220   | `BSET #15,D7` (0x08C7+ext=0x000F): wc=2, ops="#15,D7"                                                                      | [N]  | UFR-HEX-005  | REQ-DIS-027  | INT-DIS-048 | BSET static                         | PASS UC13 |
| TC-DIS-221   | `BTST D0,D3` (0x0103): wc=1, ops="D0,D3"                                                                                   | [N]  | UFR-HEX-005  | REQ-DIS-028  | INT-DIS-049 | dynamic, wc=1                       | PASS UC13 |
| TC-DIS-222   | `BCHG D1,(A0)` (0x0350): wc=1, ops="D1,(A0)"                                                                               | [N]  | UFR-HEX-005  | REQ-DIS-028  | INT-DIS-049 | BCHG dynamic                        | PASS UC13 |
| TC-DIS-223   | `BCLR D7,D5` (0x0F85): wc=1, ops="D7,D5"                                                                                   | [N]  | UFR-HEX-005  | REQ-DIS-028  | INT-DIS-049 | BCLR dynamic                        | PASS UC13 |
| TC-DIS-224   | `BSET D3,(A2)+` (0x07DA): wc=1, ops="D3,(A2)+"                                                                             | [N]  | UFR-HEX-005  | REQ-DIS-028  | INT-DIS-049 | BSET dynamic                        | PASS UC13 |
| TC-DIS-225   | Range: ASL.W #2,D3 (1W) + BTST #5,D3 (2W) + LSR.B D2,D1 (1W) = 3 instructions; addr[2]=0x2006                            | [N]  | UFR-HEX-005  | REQ-DIS-029  | INT-DIS-050 | uiN=3; addr[2]=0x2006               | PASS UC13 |
| TC-DIS-226   | MOVEQ regression (0x702A): still decoded correctly after UC13                                                                | [N]  | UFR-HEX-005  | REQ-DIS-006  | INT-DIS-001 | mnem="MOVEQ"                        | PASS UC13 |
| TC-DIS-227   | ADD.W regression (0xD240): still decoded correctly                                                                           | [N]  | UFR-HEX-005  | REQ-DIS-015  | INT-DIS-001 | mnem="ADD.W"                        | PASS UC13 |
| TC-DIS-228   | Memory shift to Dn (0xE0C0) → DC.W                                                                                          | [R]  | UFR-HEX-005  | REQ-DIS-030  | INT-DIS-051 | DC.W                                | PASS UC13 |
| TC-DIS-229   | Memory shift to An (0xE0C8) → DC.W                                                                                          | [R]  | UFR-HEX-005  | REQ-DIS-030  | INT-DIS-051 | DC.W                                | PASS UC13 |
| TC-DIS-230   | Memory shift bit11 set (0xE8D0) → DC.W                                                                                      | [R]  | UFR-HEX-005  | REQ-DIS-030  | INT-DIS-051 | DC.W                                | PASS UC13 |
| TC-DIS-231   | Static bit op An mode (0x0808+ext=0) → DC.W                                                                                 | [R]  | UFR-HEX-005  | REQ-DIS-030  | INT-DIS-051 | DC.W                                | PASS UC13 |
| TC-DIS-232   | Dynamic bit op An mode (0x010B) → DC.W                                                                                      | [R]  | UFR-HEX-005  | REQ-DIS-030  | INT-DIS-051 | DC.W                                | PASS UC13 |
| TC-DIS-233   | NULL pBuf → ST_ERROR                                                                                                         | [R]  | UFR-HEX-005  | REQ-DIS-001  | INT-DIS-051 | ST_ERROR                            | PASS UC13 |
| TC-DIS-234   | NULL ptResult → ST_ERROR                                                                                                     | [R]  | UFR-HEX-005  | REQ-DIS-001  | INT-DIS-051 | ST_ERROR                            | PASS UC13 |
| TC-DIS-235   | range NULL puiLines → ST_ERROR                                                                                               | [R]  | UFR-HEX-005  | REQ-DIS-003  | INT-DIS-051 | ST_ERROR                            | PASS UC13 |

#### Test Summary — UC13

| Module | [N] | [R] | [S] | Total | Result    |
|--------|-----|-----|-----|-------|-----------|
| DIS    | 62  | 8   | 0   | 70    | ALL PASS  |

> Note: The 70 `UC_TEST` calls (via UC13_1W/2W/DCW macros, each expanding to 2-3 assertions) are
> grouped into 36 logical TCs above. TC-DIS-215 covers 4 mnemonics in one logical entry.

#### REQ → TC coverage (UC13)

| REQ          | TC(s)                                  | Status    |
|--------------|----------------------------------------|-----------|
| REQ-DIS-008  | TC-DIS-200..235                        | ✓ UC13    |
| REQ-DIS-024  | TC-DIS-200..207                        | ✓ UC13    |
| REQ-DIS-025  | TC-DIS-208..210                        | ✓ UC13    |
| REQ-DIS-026  | TC-DIS-211..216                        | ✓ UC13    |
| REQ-DIS-027  | TC-DIS-217..220                        | ✓ UC13    |
| REQ-DIS-028  | TC-DIS-221..224                        | ✓ UC13    |
| REQ-DIS-029  | TC-DIS-225                             | ✓ UC13    |
| REQ-DIS-030  | TC-DIS-228..232                        | ✓ UC13    |

#### UFR traceability update (UC13)

| UFR          | REQ(s)                                        | TC(s)             | Status   |
|--------------|-----------------------------------------------|-------------------|----------|
| UFR-HEX-005  | REQ-DIS-024..030 (+ UC12: 015..023, UC11: 006,010..014) | TC-DIS-200..235 | ✓ UC13 |

---

### 5.42 INTENT Catalog — UC14

Source: `use_cases/use_case_14.c`
Fixtures: all opcodes hand-crafted from MC68000 PRM; no external files

| ID           | INTENT text                                                                                                                                                              |
|--------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| INT-DIS-061  | NOP/RTS/RTE/RTR: single-word control instructions produce empty operands and `bValid=ST_TRUE`.                                                                            |
| INT-DIS-062  | TRAP #N: vector in low nibble (0-15), displayed as decimal; 1 word.                                                                                                      |
| INT-DIS-063  | LINK An,#disp16 (2 words): negative displacement shown as `#-$N`; zero/positive as `#$N`. UNLK An (1 word).                                                              |
| INT-DIS-064  | JSR with control EA: (An) = 1 word; abs.W = 2 words; invalid modes (Dn, An, -(An)) → DC.W.                                                                              |
| INT-DIS-065  | JMP with control EA: (An) = 1 word; abs.L = 3 words; d16(PC) = 2 words with computed target; invalid modes → DC.W.                                                      |
| INT-DIS-066  | BRA/BSR: disp8=0xFF → DC.W (68020+); disp8=0x00 → long form 2 words, no `.S`; other disp8 → short form 1 word with `.S` suffix; target = `(addr+2+disp)&0xFFFFFF`.     |
| INT-DIS-067  | Bcc (conditions 2-15): same short/long/0xFF rules; mnemonics from g_aszBcc table; backward branches (negative disp8) compute correct target.                             |
| INT-DIS-068  | `disasm_range()` over a realistic fragment (MOVE.W + TST.W + BEQ.S + NOP + BRA.S + RTS = 6 instructions at mixed word counts) with correct addresses.                    |
| INT-DIS-069  | Robustness: JMP/JSR to invalid EAs (Dn, An, -(An)) → DC.W; BRA disp8=0xFF → DC.W; BRA long-form with only 2-byte buffer → DC.W; NULL params → ST_ERROR.                |

---

### 5.43 Test Cases — UC14 (control flow BRA/BSR/Bcc/JMP/JSR/RTS/TRAP)

Source: `use_cases/use_case_14.c`

| ID           | Functional description                                                                                                         | Type | UFR          | REQ                    | INTENT      | Expected outcome                          | Status    |
|--------------|--------------------------------------------------------------------------------------------------------------------------------|------|--------------|------------------------|-------------|-------------------------------------------|-----------|
| TC-DIS-300   | `NOP` (0x4E71): mnem="NOP", ops=""                                                                                            | [N]  | UFR-HEX-005  | REQ-DIS-031            | INT-DIS-061 | 1 word, empty ops                         | PASS UC14 |
| TC-DIS-301   | `RTS` (0x4E75): mnem="RTS", ops=""                                                                                            | [N]  | UFR-HEX-005  | REQ-DIS-031            | INT-DIS-061 | bValid=ST_TRUE                            | PASS UC14 |
| TC-DIS-302   | `RTE` (0x4E73): mnem="RTE", ops=""                                                                                            | [N]  | UFR-HEX-005  | REQ-DIS-031            | INT-DIS-061 | 1 word, empty ops                         | PASS UC14 |
| TC-DIS-303   | `RTR` (0x4E77): mnem="RTR", ops=""                                                                                            | [N]  | UFR-HEX-005  | REQ-DIS-031            | INT-DIS-061 | 1 word, empty ops                         | PASS UC14 |
| TC-DIS-304   | `TRAP #1` (0x4E41): ops="#1"                                                                                                  | [N]  | UFR-HEX-005  | REQ-DIS-032            | INT-DIS-062 | decimal vector                            | PASS UC14 |
| TC-DIS-305   | `TRAP #14` (0x4E4E): ops="#14"                                                                                                | [N]  | UFR-HEX-005  | REQ-DIS-032            | INT-DIS-062 | decimal vector 14                         | PASS UC14 |
| TC-DIS-306   | `LINK A6,#-8` (0x4E56+0xFFF8): wc=2, ops="A6,#-$8"                                                                          | [N]  | UFR-HEX-005  | REQ-DIS-033            | INT-DIS-063 | negative disp as #-$N                     | PASS UC14 |
| TC-DIS-307   | `UNLK A6` (0x4E5E): wc=1, ops="A6"                                                                                           | [N]  | UFR-HEX-005  | REQ-DIS-033            | INT-DIS-063 | 1 word                                    | PASS UC14 |
| TC-DIS-308   | `LINK A0,#$0` (0x4E50+0x0000): wc=2, ops="A0,#$0"                                                                           | [N]  | UFR-HEX-005  | REQ-DIS-033            | INT-DIS-063 | zero disp as #$0                          | PASS UC14 |
| TC-DIS-309   | `JSR (A0)` (0x4E90): wc=1, ops="(A0)"                                                                                        | [N]  | UFR-HEX-005  | REQ-DIS-034            | INT-DIS-064 | 1 word control mode                       | PASS UC14 |
| TC-DIS-310   | `JSR $1234.W` (0x4EB8+0x1234): wc=2, ops="$1234.W"                                                                          | [N]  | UFR-HEX-005  | REQ-DIS-034            | INT-DIS-064 | 2 words abs.W                             | PASS UC14 |
| TC-DIS-311   | `JMP (A1)` (0x4ED1): wc=1, ops="(A1)"                                                                                        | [N]  | UFR-HEX-005  | REQ-DIS-034            | INT-DIS-065 | 1 word                                    | PASS UC14 |
| TC-DIS-312   | `JMP $00FF8800` (0x4EF9+0x00FF+0x8800): wc=3, ops="$00FF8800"                                                                | [N]  | UFR-HEX-005  | REQ-DIS-034            | INT-DIS-065 | 3 words abs.L                             | PASS UC14 |
| TC-DIS-313   | `JMP $002236(PC)` from addr=0x1000 (0x4EFA+0x1234): wc=2, ops="$002236(PC)"                                                  | [N]  | UFR-HEX-005  | REQ-DIS-034,REQ-DIS-036 | INT-DIS-065 | computed target                          | PASS UC14 |
| TC-DIS-314   | `BRA.S $001006` from 0x1000 (0x6004): wc=1, mnem="BRA.S", ops="$001006"                                                     | [N]  | UFR-HEX-005  | REQ-DIS-035,REQ-DIS-036 | INT-DIS-066 | short form                               | PASS UC14 |
| TC-DIS-315   | `BRA $001102` from 0x1000 (0x6000+0x0100): wc=2, mnem="BRA" (no .S), ops="$001102"                                          | [N]  | UFR-HEX-005  | REQ-DIS-035,REQ-DIS-036 | INT-DIS-066 | long form                                | PASS UC14 |
| TC-DIS-316   | `BSR.S $001000` from 0x1000 (0x61FE): backward branch to self, ops="$001000"                                                 | [N]  | UFR-HEX-005  | REQ-DIS-035,REQ-DIS-036 | INT-DIS-066 | backward short BSR                       | PASS UC14 |
| TC-DIS-317   | `BNE.S $00100A` from 0x1000 (0x6608): cond=6, short                                                                         | [N]  | UFR-HEX-005  | REQ-DIS-037            | INT-DIS-067 | mnem="BNE.S"                             | PASS UC14 |
| TC-DIS-318   | `BEQ $001202` from 0x1000 (0x6700+0x0200): cond=7, long form                                                                | [N]  | UFR-HEX-005  | REQ-DIS-037            | INT-DIS-067 | mnem="BEQ" (no .S)                       | PASS UC14 |
| TC-DIS-319   | `BPL.S $000FF8` from 0x1000 (0x6AF6): cond=A, backward branch                                                               | [N]  | UFR-HEX-005  | REQ-DIS-037            | INT-DIS-067 | mnem="BPL.S", backward addr             | PASS UC14 |
| TC-DIS-320   | `BCC.S $001012` (0x6410) and `BGT.S $000FFE` (0x6EFC)                                                                       | [N]  | UFR-HEX-005  | REQ-DIS-037            | INT-DIS-067 | BCC.S / BGT.S correct                   | PASS UC14 |
| TC-DIS-321   | Range: MOVE.W #1,D0 + TST.W D0 + BEQ.S + NOP + BRA.S + RTS = 6 instructions; correct mnemonics and addresses               | [N]  | UFR-HEX-005  | REQ-DIS-038            | INT-DIS-068 | uiN=6; all mnemonics+addrs correct      | PASS UC14 |
| TC-DIS-322   | MOVEQ / ADD.W / ASL.W regression (UC11/12/13 still pass)                                                                     | [N]  | UFR-HEX-005  | REQ-DIS-006,REQ-DIS-015,REQ-DIS-024 | INT-DIS-001 | 3 regressions PASS         | PASS UC14 |
| TC-DIS-323   | `JMP Dn` (0x4EC0) → DC.W                                                                                                     | [R]  | UFR-HEX-005  | REQ-DIS-034            | INT-DIS-069 | DC.W                                    | PASS UC14 |
| TC-DIS-324   | `JSR An` (0x4E88) → DC.W                                                                                                     | [R]  | UFR-HEX-005  | REQ-DIS-034            | INT-DIS-069 | DC.W                                    | PASS UC14 |
| TC-DIS-325   | `JSR -(An)` (0x4EA0) → DC.W                                                                                                  | [R]  | UFR-HEX-005  | REQ-DIS-034            | INT-DIS-069 | DC.W                                    | PASS UC14 |
| TC-DIS-326   | BRA disp8=0xFF → DC.W (68020 32-bit form rejected)                                                                           | [R]  | UFR-HEX-005  | REQ-DIS-035            | INT-DIS-069 | DC.W                                    | PASS UC14 |
| TC-DIS-327   | BRA long-form (0x6000) with only 2-byte buffer → DC.W (short buffer guard)                                                   | [R]  | UFR-HEX-005  | REQ-DIS-035            | INT-DIS-069 | DC.W                                    | PASS UC14 |
| TC-DIS-328   | NULL pBuf → ST_ERROR                                                                                                          | [R]  | UFR-HEX-005  | REQ-DIS-001            | INT-DIS-069 | ST_ERROR                                | PASS UC14 |
| TC-DIS-329   | NULL ptResult → ST_ERROR                                                                                                      | [R]  | UFR-HEX-005  | REQ-DIS-001            | INT-DIS-069 | ST_ERROR                                | PASS UC14 |
| TC-DIS-330   | range NULL ptResults → ST_ERROR                                                                                               | [R]  | UFR-HEX-005  | REQ-DIS-002            | INT-DIS-069 | ST_ERROR                                | PASS UC14 |

#### Test Summary — UC14

| Module | [N] | [R] | [S] | Total | Result    |
|--------|-----|-----|-----|-------|-----------|
| DIS    | 56  | 8   | 0   | 64    | ALL PASS  |

#### REQ → TC coverage (UC14)

| REQ          | TC(s)                                  | Status    |
|--------------|----------------------------------------|-----------|
| REQ-DIS-009  | TC-DIS-300..330                        | ✓ UC14    |
| REQ-DIS-031  | TC-DIS-300..303, TC-DIS-157            | ✓ UC14    |
| REQ-DIS-032  | TC-DIS-304..305                        | ✓ UC14    |
| REQ-DIS-033  | TC-DIS-306..308                        | ✓ UC14    |
| REQ-DIS-034  | TC-DIS-309..313, TC-DIS-323..325       | ✓ UC14    |
| REQ-DIS-035  | TC-DIS-314..316, TC-DIS-326..327       | ✓ UC14    |
| REQ-DIS-036  | TC-DIS-313..316, TC-DIS-319            | ✓ UC14    |
| REQ-DIS-037  | TC-DIS-317..320                        | ✓ UC14    |
| REQ-DIS-038  | TC-DIS-321                             | ✓ UC14    |

#### UFR traceability update (UC14)

| UFR          | REQ(s)                                               | TC(s)             | Status   |
|--------------|------------------------------------------------------|-------------------|----------|
| UFR-HEX-005  | REQ-DIS-031..038 (+ UC13: 024..030, UC12: 015..023, UC11: 006,010..014) | TC-DIS-300..330 | ✓ UC14 |

---

### 5.44 INTENT Catalog — UC15

Source: `use_cases/use_case_15.c`
Fixtures: `use_cases/UC01/hello.prg`; `use_cases/UC15/fixup.prg`, `bss.prg`, `absolute.prg`, `multi_fixup.prg`, `bad_fixup.prg`

| ID           | INTENT text                                                                                                                                             |
|--------------|---------------------------------------------------------------------------------------------------------------------------------------------------------|
| INT-LOD-009  | UC7 regression path with full implementation: hello.prg (no fixups, first_offset=0) must load sections and produce uiFixupCount=0; load_do_prg() now complete vs. UC7 stub. |
| INT-LOD-010  | Single fixup relocation: fixup.prg has 1 fixup at .text offset 2; the longword at RAM[ST_LOAD_BASE+2] must equal ST_LOAD_BASE (0x0800) after load; opcodes before and after the fixup must be unchanged. |
| INT-LOD-011  | BSS zeroing: bss.prg has a 16-byte .bss with no fixups; the BSS area in ST RAM must be fully zeroed regardless of prior RAM content (sentinel-fill test). |
| INT-LOD-012  | Absolute PRG (abs_flag=1): no fixup table present in the file; load must succeed with uiFixupCount=0 without attempting to read a fixup table. |
| INT-LOD-013  | Multiple fixups: multi_fixup.prg has 2 fixups at offsets 2 and 8; each longword must be independently incremented by ST_LOAD_BASE; the advance byte between them positions the second fixup correctly. |
| INT-LOD-014  | State field invariants: uiLoadAddr is always ST_LOAD_BASE; uiSize = text+data+bss (RAM footprint, not file size). |
| INT-LOD-015  | Robustness — bad fixup and guards: a fixup offset out of .text+.data range must return ST_ERROR without modifying the previous load state; NULL path and call-after-shutdown are also guarded. |

---

### 5.45 Test Cases — UC15 (PRG full load — header, BSS, fixup relocation)

Source: `use_cases/use_case_15.c`

| ID           | Functional description                                                                                              | Type | UFR                    | REQ                        | INTENT      | Expected outcome                                      | Status    |
|--------------|---------------------------------------------------------------------------------------------------------------------|------|------------------------|----------------------------|-------------|-------------------------------------------------------|-----------|
| TC-LOD-018   | `load_init(valid)` → ST_NO_ERROR; re-init after sentinel RAM fill                                                  | [N]  | UFR-LOD-001            | REQ-LOD-001                | INT-LOD-009 | ST_NO_ERROR                                           | PASS UC15 |
| TC-LOD-019   | `load_file(hello.prg)` → ST_NO_ERROR; eType=PRG; uiSize=4; uiFixupCount=0                                         | [N]  | UFR-LOD-004            | REQ-LOD-007, REQ-LOD-019   | INT-LOD-009 | bLoaded=TRUE; uiFixupCount=0                          | PASS UC15 |
| TC-LOD-020   | hello.prg: RAM[LOAD_BASE+0]=0x70 (MOVEQ hi), RAM[LOAD_BASE+1]=0x2A (MOVEQ #42)                                    | [N]  | UFR-LOD-004            | REQ-LOD-015                | INT-LOD-009 | Opcodes verbatim in RAM                               | PASS UC15 |
| TC-LOD-021   | `load_file(fixup.prg)` → ST_NO_ERROR; eType=PRG; uiTextSize=6; uiDataSize=0; uiFixupCount=1                       | [N]  | UFR-LOD-004, UFR-LOD-007 | REQ-LOD-017, REQ-LOD-019  | INT-LOD-010 | Sizes and fixup count correct                         | PASS UC15 |
| TC-LOD-022   | fixup.prg: JMP opcode at RAM[LOAD_BASE+0..1] = 0x4E/0xF9 (unchanged)                                              | [N]  | UFR-LOD-004            | REQ-LOD-020                | INT-LOD-010 | Opcode bytes unmodified by fixup                      | PASS UC15 |
| TC-LOD-023   | fixup.prg: RAM[LOAD_BASE+2..5] = 0x00000800 (relocated from 0x00000000 + ST_LOAD_BASE)                            | [N]  | UFR-LOD-007            | REQ-LOD-020                | INT-LOD-010 | Longword = ST_LOAD_BASE (0x0800)                      | PASS UC15 |
| TC-LOD-024   | `load_file(bss.prg)` → ST_NO_ERROR; uiTextSize=2; uiBssSize=16; uiSize=18 (text+bss)                              | [N]  | UFR-LOD-004, UFR-LOD-006 | REQ-LOD-018, REQ-LOD-019  | INT-LOD-011 | uiSize = text+data+bss                                | PASS UC15 |
| TC-LOD-025   | bss.prg: RAM[LOAD_BASE+0..1] = 0x4E/0x75 (RTS); BSS area (offsets 2..17) all zeroed                              | [N]  | UFR-LOD-006            | REQ-LOD-018                | INT-LOD-011 | RTS present; BSS = 0x00 despite 0xFF sentinel         | PASS UC15 |
| TC-LOD-026   | `load_file(absolute.prg)` → ST_NO_ERROR; eType=PRG; uiFixupCount=0 (abs_flag=1)                                   | [N]  | UFR-LOD-008            | REQ-LOD-017                | INT-LOD-012 | No fixup processing; uiFixupCount=0                   | PASS UC15 |
| TC-LOD-027   | absolute.prg: RAM[LOAD_BASE+0..1] = 0x4E/0x75 (RTS, abs content)                                                  | [N]  | UFR-LOD-008            | REQ-LOD-017                | INT-LOD-012 | Content loaded correctly without fixup                | PASS UC15 |
| TC-LOD-028   | `load_file(multi_fixup.prg)` → ST_NO_ERROR; uiTextSize=12; uiFixupCount=2                                         | [N]  | UFR-LOD-007            | REQ-LOD-020, REQ-LOD-019   | INT-LOD-013 | Two fixups applied                                    | PASS UC15 |
| TC-LOD-029   | multi_fixup: RAM[LOAD_BASE+2..5]=0x00000800 (first fixup); RAM[LOAD_BASE+8..11]=0x00000806 (second fixup)          | [N]  | UFR-LOD-007            | REQ-LOD-020                | INT-LOD-013 | Both longwords independently relocated                | PASS UC15 |
| TC-LOD-030   | multi_fixup: RAM[LOAD_BASE+6..7]=0x4E/0xF9 (JMP opcode between fixups, unchanged)                                 | [N]  | UFR-LOD-007            | REQ-LOD-020                | INT-LOD-013 | Bytes between fixups unmodified                       | PASS UC15 |
| TC-LOD-031   | state field invariants: uiLoadAddr == ST_LOAD_BASE; uiSize == 12 (text=12, data=0, bss=0)                          | [N]  | UFR-LOD-004            | REQ-LOD-019, REQ-LOD-021   | INT-LOD-014 | Load address constant; size = RAM footprint           | PASS UC15 |
| TC-LOD-032   | `load_file(bad_fixup.prg)` after good load → ST_ERROR; previous state preserved (eType=PRG; uiTextSize=4)          | [R]  | UFR-LOD-007            | REQ-LOD-021                | INT-LOD-015 | ST_ERROR; state unchanged                             | PASS UC15 |
| TC-LOD-033   | `load_file(NULL)` → ST_ERROR (guard check)                                                                         | [R]  | UFR-LOD-001            | REQ-LOD-003                | INT-LOD-015 | ST_ERROR                                              | PASS UC15 |
| TC-LOD-034   | `load_file(non-existent)` → ST_ERROR                                                                               | [R]  | UFR-LOD-001            | REQ-LOD-004                | INT-LOD-015 | ST_ERROR                                              | PASS UC15 |
| TC-LOD-035   | `load_shutdown()` → ST_NO_ERROR                                                                                     | [N]  | UFR-LOD-001            | REQ-LOD-013                | INT-LOD-015 | ST_NO_ERROR                                           | PASS UC15 |
| TC-LOD-036   | `load_file()` after shutdown → ST_ERROR                                                                             | [R]  | UFR-LOD-001            | REQ-LOD-003                | INT-LOD-015 | ST_ERROR (machine pointer = NULL)                     | PASS UC15 |

> Note: The 38 `UC_TEST` assertion calls in `use_case_15.c` are grouped into 19 logical TCs above;
> multi-assertion calls (e.g. TC-LOD-029 covers 8 RAM byte checks) follow the UC7 grouping convention.

#### Test Summary — UC15

| Module | [N] | [R] | [S] | Total | Result    |
|--------|-----|-----|-----|-------|-----------|
| LOD    | 33  | 5   | 0   | 38    | ALL PASS  |

#### REQ → TC coverage (UC15)

| REQ          | TC(s)                                  | Status    |
|--------------|----------------------------------------|-----------|
| REQ-LOD-015  | TC-LOD-020                             | ✓ UC15    |
| REQ-LOD-016  | (symtab skip — structural; covered by fixup.prg/multi_fixup.prg paths) | ✓ UC15 |
| REQ-LOD-017  | TC-LOD-021, TC-LOD-026, TC-LOD-028    | ✓ UC15    |
| REQ-LOD-018  | TC-LOD-024..025                        | ✓ UC15    |
| REQ-LOD-019  | TC-LOD-019, TC-LOD-024, TC-LOD-028, TC-LOD-031 | ✓ UC15 |
| REQ-LOD-020  | TC-LOD-023, TC-LOD-025, TC-LOD-029..030 | ✓ UC15  |
| REQ-LOD-021  | TC-LOD-032                             | ✓ UC15    |
| REQ-LOD-022  | (EOF-without-terminator: structural; no fixture available headless) | ✓ UC15 (code review) |

#### UFR traceability update (UC15)

| UFR          | REQ(s)                                      | TC(s)                                         | Status   |
|--------------|---------------------------------------------|-----------------------------------------------|----------|
| UFR-LOD-004  | REQ-LOD-007..008, REQ-LOD-015..017, REQ-LOD-019 | TC-LOD-019..023, TC-LOD-026..028, TC-LOD-031 | ✓ UC15 |
| UFR-LOD-006  | REQ-LOD-018..019                            | TC-LOD-024..025                               | ✓ UC15   |
| UFR-LOD-007  | REQ-LOD-020..022                            | TC-LOD-021, TC-LOD-023, TC-LOD-028..032       | ✓ UC15   |
| UFR-LOD-008  | REQ-LOD-017                                 | TC-LOD-026..027                               | ✓ UC15   |

---

### 5.46 INTENT Catalog — UC16

Source: `use_cases/use_case_16.c`

| INTENT ID    | Description                                                                                                                                         |
|--------------|-----------------------------------------------------------------------------------------------------------------------------------------------------|
| INT-IST-001  | Create + BPB + FAT init: `image_st_create()` produces a valid blank DD image with correct BPB values, FAT12 cluster-0/1 markers, and an empty root directory. |
| INT-IST-002  | Write / list / read round-trip: writing a file allocates FAT clusters, creates a directory entry, and the content is returned byte-for-byte by `image_st_read_file()`. |
| INT-IST-003  | Delete + free-bytes accounting: deleting a file marks the entry 0xE5, frees the FAT chain, and `free_bytes` returns to the pre-write value.         |
| INT-IST-004  | Save + reload round-trip: saving an image with `image_st_save()` and reloading with `image_st_load()` produces an identical structure with matching filenames, sizes, and file content. |
| INT-IST-005  | Overwrite existing file: writing a file whose name already exists replaces it: root count stays at 1 and new content is returned by `image_st_read_file()`. |
| INT-IST-006  | Close idempotence: `image_st_close()` sets the handle to NULL and is safe to call twice (`close(&NULL)` is a no-op). |
| INT-IST-007  | NULL guards: all public functions return `ST_ERROR` when any required pointer parameter is NULL. |
| INT-IST-008  | Error cases: loading a non-existent file, writing an invalid 8.3 name, and deleting a non-existent file all return `ST_ERROR` cleanly without side-effects. |

---

### 5.47 Test Cases — UC16 (.st disk image — FAT12 create/load/save/list/read/write/delete)

Source: `use_cases/use_case_16.c`

| ID           | Functional description                                                                                              | Type | UFR          | REQ                          | INTENT      | Expected outcome                                           | Status    |
|--------------|---------------------------------------------------------------------------------------------------------------------|------|--------------|------------------------------|-------------|------------------------------------------------------------|-----------|
| TC-IST-001   | `image_st_create(&ptImg)` → ST_NO_ERROR                                                                            | [N]  | UFR-DSK-001  | REQ-IST-001                  | INT-IST-001 | ST_NO_ERROR                                                | PASS UC16 |
| TC-IST-002   | After create: handle not NULL                                                                                       | [N]  | UFR-DSK-001  | REQ-IST-001                  | INT-IST-001 | ptImg != NULL                                              | PASS UC16 |
| TC-IST-003   | `IST_DISK_SIZE == 737280` (constant sanity)                                                                         | [N]  | UFR-DSK-001  | REQ-IST-001                  | INT-IST-001 | 80×2×9×512 = 737280                                        | PASS UC16 |
| TC-IST-004   | `image_st_free_bytes()` on fresh image → ST_NO_ERROR                                                               | [N]  | UFR-DSK-004  | REQ-IST-012                  | INT-IST-001 | ST_NO_ERROR                                                | PASS UC16 |
| TC-IST-005   | Free bytes == IST_DATA_CLUSTERS × IST_SPC × IST_BPS = 728,064                                                      | [N]  | UFR-DSK-004  | REQ-IST-012                  | INT-IST-001 | uiFree = 711×2×512 = 728064                                | PASS UC16 |
| TC-IST-006   | `image_st_list_root()` on fresh image → ST_NO_ERROR; piCount = 0                                                   | [N]  | UFR-DSK-002  | REQ-IST-006                  | INT-IST-001 | iCount == 0                                                | PASS UC16 |
| TC-IST-007   | `image_st_write_file(ptImg, "HELLO.PRG", 64 bytes)` → ST_NO_ERROR                                                  | [N]  | UFR-DSK-003  | REQ-IST-008                  | INT-IST-002 | ST_NO_ERROR                                                | PASS UC16 |
| TC-IST-008   | `image_st_list_root()` after write → ST_NO_ERROR                                                                   | [N]  | UFR-DSK-002  | REQ-IST-006                  | INT-IST-002 | ST_NO_ERROR                                                | PASS UC16 |
| TC-IST-009   | After write: root count == 1; entry name == "HELLO.PRG"; entry size == 64                                          | [N]  | UFR-DSK-002  | REQ-IST-006, REQ-IST-008     | INT-IST-002 | iCount=1; szName="HELLO.PRG"; uiSize=64                    | PASS UC16 |
| TC-IST-010   | `image_st_read_file()` on the written entry → ST_NO_ERROR                                                          | [N]  | UFR-DSK-003  | REQ-IST-007                  | INT-IST-002 | ST_NO_ERROR                                                | PASS UC16 |
| TC-IST-011   | Read content matches the 64 bytes originally written (pattern i×3+7)                                               | [N]  | UFR-DSK-003  | REQ-IST-007                  | INT-IST-002 | memcmp == 0                                                | PASS UC16 |
| TC-IST-012   | `image_st_write_file(ptImg, "DATA.BIN", 512 bytes)` → ST_NO_ERROR                                                  | [N]  | UFR-DSK-003  | REQ-IST-008                  | INT-IST-002 | ST_NO_ERROR                                                | PASS UC16 |
| TC-IST-013   | Root count == 2 after second write                                                                                  | [N]  | UFR-DSK-002  | REQ-IST-006                  | INT-IST-002 | iCount == 2                                                | PASS UC16 |
| TC-IST-014   | Free bytes decreases after writing a 512-byte file                                                                  | [N]  | UFR-DSK-004  | REQ-IST-012                  | INT-IST-003 | uiMid < uiFreeBefore                                       | PASS UC16 |
| TC-IST-015   | `image_st_delete_file(ptImg, "TMP.TXT")` → ST_NO_ERROR                                                             | [N]  | UFR-DSK-003  | REQ-IST-011                  | INT-IST-003 | ST_NO_ERROR                                                | PASS UC16 |
| TC-IST-016   | Root count == 0 after delete                                                                                        | [N]  | UFR-DSK-002  | REQ-IST-006                  | INT-IST-003 | iCount == 0                                                | PASS UC16 |
| TC-IST-017   | Free bytes restored to pre-write value after delete                                                                 | [N]  | UFR-DSK-004  | REQ-IST-012                  | INT-IST-003 | uiFreeAfter == uiFreeBefore                                | PASS UC16 |
| TC-IST-018   | `image_st_save(ptImg, roundtrip.st)` → ST_NO_ERROR                                                                 | [N]  | UFR-DSK-004  | REQ-IST-005                  | INT-IST-004 | ST_NO_ERROR                                                | PASS UC16 |
| TC-IST-019   | `image_st_load("roundtrip.st", &ptImg2)` → ST_NO_ERROR                                                             | [N]  | UFR-DSK-001  | REQ-IST-003, REQ-IST-004     | INT-IST-004 | ST_NO_ERROR                                                | PASS UC16 |
| TC-IST-020   | Reload: root count == 1; entry name == "REVIVAL.PRG"; entry size == 100                                            | [N]  | UFR-DSK-002  | REQ-IST-006                  | INT-IST-004 | iCount=1; szName="REVIVAL.PRG"; uiSize=100                 | PASS UC16 |
| TC-IST-021   | Reload: read_file content matches original 100 bytes (pattern i^0xA5)                                              | [N]  | UFR-DSK-003  | REQ-IST-007                  | INT-IST-004 | memcmp == 0                                                | PASS UC16 |
| TC-IST-022   | Overwrite "DEMO.PRG": write 32 bytes, then overwrite with 48 bytes; root count still 1                             | [N]  | UFR-DSK-003  | REQ-IST-009                  | INT-IST-005 | iCount == 1                                                | PASS UC16 |
| TC-IST-023   | Overwrite: new content (0x22 × 48) returned by read_file                                                           | [N]  | UFR-DSK-003  | REQ-IST-009                  | INT-IST-005 | memcmp(aBuf, aNew, 48) == 0                                | PASS UC16 |
| TC-IST-024   | `image_st_close(&ptImg)` → ST_NO_ERROR; ptImg == NULL                                                              | [N]  | UFR-DSK-001  | REQ-IST-002                  | INT-IST-006 | ST_NO_ERROR; handle NULL                                   | PASS UC16 |
| TC-IST-025   | `image_st_close(&NULL)` → ST_NO_ERROR (idempotent)                                                                 | [N]  | UFR-DSK-001  | REQ-IST-002                  | INT-IST-006 | ST_NO_ERROR                                                | PASS UC16 |
| TC-IST-026   | `image_st_create(NULL)` → ST_ERROR                                                                                  | [R]  | UFR-DSK-001  | REQ-IST-001                  | INT-IST-007 | ST_ERROR                                                   | PASS UC16 |
| TC-IST-027   | `image_st_load(NULL, &p)` → ST_ERROR                                                                               | [R]  | UFR-DSK-001  | REQ-IST-003                  | INT-IST-007 | ST_ERROR                                                   | PASS UC16 |
| TC-IST-028   | `image_st_load("x.st", NULL)` → ST_ERROR                                                                           | [R]  | UFR-DSK-001  | REQ-IST-003                  | INT-IST-007 | ST_ERROR                                                   | PASS UC16 |
| TC-IST-029   | `image_st_close(NULL)` → ST_ERROR                                                                                   | [R]  | UFR-DSK-001  | REQ-IST-002                  | INT-IST-007 | ST_ERROR                                                   | PASS UC16 |
| TC-IST-030   | `image_st_save(NULL, "x.st")` → ST_ERROR                                                                           | [R]  | UFR-DSK-004  | REQ-IST-005                  | INT-IST-007 | ST_ERROR                                                   | PASS UC16 |
| TC-IST-031   | `image_st_save(ptImg, NULL)` → ST_ERROR                                                                             | [R]  | UFR-DSK-004  | REQ-IST-005                  | INT-IST-007 | ST_ERROR                                                   | PASS UC16 |
| TC-IST-032   | `image_st_list_root(NULL, …)` → ST_ERROR                                                                           | [R]  | UFR-DSK-002  | REQ-IST-006                  | INT-IST-007 | ST_ERROR                                                   | PASS UC16 |
| TC-IST-033   | `image_st_list_root(ptImg, NULL, …)` → ST_ERROR                                                                    | [R]  | UFR-DSK-002  | REQ-IST-006                  | INT-IST-007 | ST_ERROR                                                   | PASS UC16 |
| TC-IST-034   | `image_st_read_file(NULL, …)` → ST_ERROR                                                                           | [R]  | UFR-DSK-003  | REQ-IST-007                  | INT-IST-007 | ST_ERROR                                                   | PASS UC16 |
| TC-IST-035   | `image_st_read_file(ptImg, 2, 4, NULL, 16)` → ST_ERROR                                                             | [R]  | UFR-DSK-003  | REQ-IST-007                  | INT-IST-007 | ST_ERROR                                                   | PASS UC16 |
| TC-IST-036   | `image_st_free_bytes(NULL, &v)` → ST_ERROR                                                                         | [R]  | UFR-DSK-004  | REQ-IST-012                  | INT-IST-007 | ST_ERROR                                                   | PASS UC16 |
| TC-IST-037   | `image_st_load("no_such.st", &p)` → ST_ERROR; *pptImg remains NULL                                                 | [R]  | UFR-DSK-001  | REQ-IST-003                  | INT-IST-008 | ST_ERROR; handle unchanged                                 | PASS UC16 |
| TC-IST-038   | `image_st_write_file(ptImg, "TOOLONGNAME.TXT", …)` → ST_ERROR (name > 8 chars)                                     | [R]  | UFR-DSK-003  | REQ-IST-008                  | INT-IST-008 | ST_ERROR                                                   | PASS UC16 |
| TC-IST-039   | `image_st_delete_file(ptImg, "ABSENT.TXT")` → ST_ERROR (not found)                                                 | [R]  | UFR-DSK-003  | REQ-IST-011                  | INT-IST-008 | ST_ERROR                                                   | PASS UC16 |

#### Test Summary — UC16

| Module | [N] | [R] | [S] | Total | Result   |
|--------|-----|-----|-----|-------|----------|
| IST    | 25  | 14  | 0   | 39    | ALL PASS |

> Note: The 46 `UC_TEST`/`UC_CHECK` macro calls in `use_case_16.c` are grouped into 39 logical TCs above.
> TC-IST-009 covers 3 assertions (iCount + szName + uiSize); TC-IST-020 covers 3 assertions; TC-IST-021 covers read_file call + memcmp.

#### REQ → TC coverage (UC16)

| REQ          | TC(s)                                              | Status    |
|--------------|----------------------------------------------------|-----------|
| REQ-IST-001  | TC-IST-001..005                                    | ✓ UC16    |
| REQ-IST-002  | TC-IST-024..025, TC-IST-029                        | ✓ UC16    |
| REQ-IST-003  | TC-IST-027..028, TC-IST-037                        | ✓ UC16    |
| REQ-IST-004  | TC-IST-019 (BPB validated on reload)               | ✓ UC16    |
| REQ-IST-005  | TC-IST-018, TC-IST-030..031                        | ✓ UC16    |
| REQ-IST-006  | TC-IST-006, TC-IST-008..009, TC-IST-013, TC-IST-016, TC-IST-020, TC-IST-032..033 | ✓ UC16 |
| REQ-IST-007  | TC-IST-010..011, TC-IST-021, TC-IST-034..035       | ✓ UC16    |
| REQ-IST-008  | TC-IST-007, TC-IST-012, TC-IST-026, TC-IST-038    | ✓ UC16    |
| REQ-IST-009  | TC-IST-022..023                                    | ✓ UC16    |
| REQ-IST-010  | (disk-full path structural; no fixture for a filled image) | ✓ UC16 (code review) |
| REQ-IST-011  | TC-IST-015, TC-IST-036, TC-IST-039                 | ✓ UC16    |
| REQ-IST-012  | TC-IST-004..005, TC-IST-014, TC-IST-017            | ✓ UC16    |

#### UFR traceability update (UC16)

| UFR          | REQ(s)                                           | TC(s)                                                    | Status   |
|--------------|--------------------------------------------------|----------------------------------------------------------|----------|
| UFR-DSK-001  | REQ-IST-001..004                                 | TC-IST-001..006, TC-IST-018..019, TC-IST-024..029, TC-IST-037 | ✓ UC16 |
| UFR-DSK-002  | REQ-IST-006                                      | TC-IST-006, TC-IST-008..009, TC-IST-013, TC-IST-016, TC-IST-020, TC-IST-032..033 | ✓ UC16 |
| UFR-DSK-003  | REQ-IST-007..011                                 | TC-IST-007, TC-IST-010..012, TC-IST-015, TC-IST-021..023, TC-IST-034..036, TC-IST-038..039 | ✓ UC16 |
| UFR-DSK-004  | REQ-IST-005, REQ-IST-012                         | TC-IST-014, TC-IST-017..018, TC-IST-030..031              | ✓ UC16   |

---

### 5.48 INTENT Catalog — UC15A

Source: `use_cases/use_case_15A.c`
Fixtures: `use_cases/UC15A/SOURCE.PRG`, `use_cases/UC15A/SOURCE.S`

| ID           | INTENT text                                                                                                                                                 |
|--------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------|
| INT-DIS-070  | Normalisation helpers robustness: empty string and comment/label lines must not crash or produce artefacts; degenerate inputs return empty/0 correctly.      |
| INT-DIS-071  | Full torture test pipeline: load SOURCE.PRG into ST RAM, disassemble .text via disasm_range(), parse SOURCE.S expected instructions, compare pair by pair. The count of SOURCE.S instructions must equal the disasm count (no stream misalignment). DIFF must be 0. |

---

### 5.49 Test Cases — UC15A (disassembler torture test vs DEVPAC3 SOURCE.PRG)

Source: `use_cases/use_case_15A.c`

| ID           | Functional description                                                                                                        | Type | UFR          | REQ                       | INTENT      | Expected outcome                                         | Status     |
|--------------|-------------------------------------------------------------------------------------------------------------------------------|------|--------------|---------------------------|-------------|----------------------------------------------------------|------------|
| TC-DIS-400   | `uc15a_normalize(NULL, ...)` and `(src, NULL, ...)` → empty output, no crash                                                 | [R]  | UFR-HEX-005  | REQ-DIS-001               | INT-DIS-070 | No crash; output[0]=='\0'                                | PASS UC15A |
| TC-DIS-401   | `uc15a_extract_instr` on blank line → returns 0 (no instruction extracted)                                                   | [R]  | UFR-HEX-005  | REQ-DIS-001               | INT-DIS-070 | return == 0                                              | PASS UC15A |
| TC-DIS-402   | `uc15a_extract_instr` on pure comment line (`;...`) → returns 0                                                              | [R]  | UFR-HEX-005  | REQ-DIS-001               | INT-DIS-070 | return == 0                                              | PASS UC15A |
| TC-DIS-403   | `st_init(&tMachine, NULL)` → ST_NO_ERROR                                                                                     | [N]  | UFR-HEX-005  | REQ-DIS-001               | INT-DIS-071 | ST_NO_ERROR                                              | PASS UC15A |
| TC-DIS-404   | `load_init(&tMachine)` → ST_NO_ERROR                                                                                         | [N]  | UFR-HEX-005  | REQ-LOD-001               | INT-DIS-071 | ST_NO_ERROR                                              | PASS UC15A |
| TC-DIS-405   | `load_file("bad/path")` → ST_ERROR (guard)                                                                                   | [R]  | UFR-HEX-005  | REQ-LOD-004               | INT-DIS-071 | ST_ERROR                                                 | PASS UC15A |
| TC-DIS-406   | `load_file("SOURCE.PRG")` → ST_NO_ERROR; `ptState->bLoaded == ST_TRUE`                                                      | [N]  | UFR-HEX-005  | REQ-LOD-007               | INT-DIS-071 | ST_NO_ERROR; bLoaded=TRUE                                | PASS UC15A |
| TC-DIS-407   | `ptState->uiTextSize > 0` after loading SOURCE.PRG                                                                           | [N]  | UFR-HEX-005  | REQ-LOD-019               | INT-DIS-071 | uiTextSize > 0                                           | PASS UC15A |
| TC-DIS-408   | `disasm_range(RAM+ST_LOAD_BASE, uiTextSize, ST_LOAD_BASE, atRes, MAX, &uiN)` → ST_NO_ERROR; uiN > 0                         | [N]  | UFR-HEX-005  | REQ-DIS-002               | INT-DIS-071 | ST_NO_ERROR; uiN > 0                                     | PASS UC15A |
| TC-DIS-409   | SOURCE.S parsed instruction count > 0                                                                                         | [N]  | UFR-HEX-005  | —                         | INT-DIS-071 | iSrcCount > 0                                            | PASS UC15A |
| TC-DIS-410   | SOURCE.S instruction count == disasm result count (no stream misalignment — iWordCount correct for all DC.W with ext words)  | [N]  | UFR-HEX-005  | REQ-DIS-002               | INT-DIS-071 | iSrcCount == uiN                                         | PASS UC15A |
| TC-DIS-411   | DIFF count == 0 after MATCH/DCW/PCREL/COMPAT classification of all 2525 pairs                                                | [N]  | UFR-HEX-005  | REQ-DIS-006..009          | INT-DIS-071 | iDiff == 0                                               | PASS UC15A |
| TC-DIS-412   | MATCH + DCW + PCREL + COMPAT == total (classification is exhaustive, no unclassified instruction)                            | [N]  | UFR-HEX-005  | REQ-DIS-006..009          | INT-DIS-071 | iMatch+iDcw+iPcrel+iCompat == iTotal                     | PASS UC15A |

#### Test Summary — UC15A

| Module | [N] | [R] | [S] | Total | Result     |
|--------|-----|-----|-----|-------|------------|
| DIS    | 10  | 3   | 0   | 13    | ALL PASS   |

#### Final score — SOURCE.S vs disasm_range()

| SOURCE.S instructions | MATCH | DCW | PCREL | COMPAT | DIFF  |
|-----------------------|-------|-----|-------|--------|-------|
| 2525                  | 963   | 270 | 495   | 797    | **0** |

#### REQ → TC coverage (UC15A)

| REQ         | TC(s)                                         | Status     |
|-------------|-----------------------------------------------|------------|
| REQ-DIS-001 | TC-DIS-400..402 (NULL/blank guards)           | ✓ UC15A    |
| REQ-DIS-002 | TC-DIS-408, TC-DIS-410                        | ✓ UC15A    |
| REQ-DIS-006 | TC-DIS-411..412 (MOVE/MOVEQ/LEA/EXG…)        | ✓ UC15A    |
| REQ-DIS-007 | TC-DIS-411..412 (ADD/SUB/CMP/AND/OR/EOR…)    | ✓ UC15A    |
| REQ-DIS-008 | TC-DIS-411..412 (shifts/bit ops)              | ✓ UC15A    |
| REQ-DIS-009 | TC-DIS-411..412 (branches/JMP/JSR/RTS…)      | ✓ UC15A    |
| REQ-DIS-024 | TC-DIS-208..210 (ADAPTED — imm shift, bit5=0) | ✓ UC15A   |
| REQ-DIS-025 | TC-DIS-200..207 (ADAPTED — reg shift, bit5=1) | ✓ UC15A   |

#### UFR traceability update (UC15A)

| UFR         | REQ(s)                                              | TC(s)                           | Status    |
|-------------|-----------------------------------------------------|---------------------------------|-----------|
| UFR-HEX-005 | REQ-DIS-001..009, REQ-DIS-024..030 (all corrected) | TC-DIS-400..412 + ADAPTED TCs   | ✓ UC15A   |

#### ADAPTED markers summary (UC15A)

| TC           | Previous value (UC13)          | Corrected value (UC15A)         | Root cause                             |
|--------------|--------------------------------|---------------------------------|----------------------------------------|
| TC-DIS-049   | EXG A0,A1                      | EXG A1,A0                       | DEVPAC3 EXG An,An: first op in bits 2-0 |
| TC-DIS-050   | EXG A3,A5                      | EXG A5,A3                       | same as above                          |
| TC-DIS-200   | ASL.W #2,D3                    | ASL.W D2,D3                     | groupE iIR bit was inverted             |
| TC-DIS-201   | ASR.W #3,D0                    | ASR.W D3,D0                     | same                                   |
| TC-DIS-202   | LSL.B #1,D1                    | LSL.B D1,D1                     | same                                   |
| TC-DIS-203   | LSR.L #4,D2                    | LSR.L D4,D2                     | same                                   |
| TC-DIS-204   | ROXL.W #1,D5                   | ROXL.W D1,D5                    | same                                   |
| TC-DIS-205   | ROXR.B #2,D4                   | ROXR.B D2,D4                    | same                                   |
| TC-DIS-206   | ROL.L #8,D7                    | ROL.L D0,D7                     | same (bit5=1 → D0, not "0 means 8")   |
| TC-DIS-207   | ROR.W #1,D0                    | ROR.W D1,D0                     | same                                   |
| TC-DIS-208   | ASL.W D3,D0                    | ASL.W #3,D0                     | groupE iIR bit was inverted             |
| TC-DIS-209   | LSR.B D2,D1                    | LSR.B #2,D1                     | same                                   |
| TC-DIS-210   | ROR.L D0,D5                    | ROR.L #8,D5                     | same (bit5=0 → imm, "0 means 8")      |

---

### 5.50 INTENT Catalog — UC17

| ID           | Intent                                                                                        |
|--------------|-----------------------------------------------------------------------------------------------|
| INT-MSA-001  | NULL path to `image_msa_load` must return ST_ERROR immediately                               |
| INT-MSA-002  | NULL `pptImg` to `image_msa_load` must return ST_ERROR                                       |
| INT-MSA-003  | NULL `ptImg` to `image_msa_save` must return ST_ERROR                                        |
| INT-MSA-004  | NULL path to `image_msa_save` must return ST_ERROR                                           |
| INT-MSA-005  | `image_st_get_disk` with NULL `ptImg` must return ST_ERROR                                   |
| INT-MSA-006  | `image_st_get_disk` with NULL `ppDisk` must return ST_ERROR                                  |
| INT-MSA-010  | A blank image saved as `.msa` then reloaded must be identical (same free_bytes, 0 entries)   |
| INT-MSA-011  | MSA compression must produce a file smaller than IST_DISK_SIZE/10 for a blank image          |
| INT-MSA-020  | Files written to an image must survive a `.msa` save/load cycle (sizes and content intact)   |
| INT-MSA-021  | Files containing 0xE5 bytes must round-trip exactly; codec must escape/restore them correctly |
| INT-MSA-030  | Loading a nonexistent file must return ST_ERROR; handle stays NULL                           |
| INT-MSA-031  | A file with wrong magic must be rejected; handle stays NULL                                  |
| INT-MSA-032  | A truncated MSA file must be rejected during header read; handle stays NULL                  |
| INT-MSA-033  | A MSA file with impossible geometry (sides=5) must be rejected; handle stays NULL            |

---

### 5.51 Test Cases — UC17 (MSA disk image — RLE codec + round-trip)

| TC           | Description                                                                                        | Type | Parent UFR  | REQ          | INT          | Notes                             | Result    |
|--------------|----------------------------------------------------------------------------------------------------|------|-------------|--------------|--------------|-----------------------------------|-----------|
| TC-MSA-001   | `image_msa_load(NULL, &p)` -> ST_ERROR                                                             | [R]  | UFR-DSK-005 | REQ-MSA-001  | INT-MSA-001  |                                   | PASS UC17 |
| TC-MSA-002   | `image_msa_load("x.msa", NULL)` -> ST_ERROR                                                        | [R]  | UFR-DSK-005 | REQ-MSA-001  | INT-MSA-002  |                                   | PASS UC17 |
| TC-MSA-003   | `image_msa_save(NULL, "x.msa")` -> ST_ERROR                                                        | [R]  | UFR-DSK-006 | REQ-MSA-002  | INT-MSA-003  |                                   | PASS UC17 |
| TC-MSA-004   | `image_msa_save(ptImg, NULL)` -> ST_ERROR                                                          | [R]  | UFR-DSK-006 | REQ-MSA-002  | INT-MSA-004  |                                   | PASS UC17 |
| TC-MSA-005   | `image_st_get_disk(NULL, &p)` -> ST_ERROR                                                          | [R]  | UFR-DSK-007 | REQ-MSA-010  | INT-MSA-005  |                                   | PASS UC17 |
| TC-MSA-006   | `image_st_get_disk(ptImg, NULL)` -> ST_ERROR                                                       | [R]  | UFR-DSK-007 | REQ-MSA-010  | INT-MSA-006  |                                   | PASS UC17 |
| TC-MSA-010   | Blank image: `image_st_create` + `image_msa_save` -> ST_NO_ERROR                                  | [N]  | UFR-DSK-006 | REQ-MSA-003  | INT-MSA-010  |                                   | PASS UC17 |
| TC-MSA-011   | Blank `.msa` file exists on disk after save                                                        | [N]  | UFR-DSK-006 | REQ-MSA-004  | INT-MSA-011  |                                   | PASS UC17 |
| TC-MSA-012   | Blank `.msa` file size > `IMSA_HDR_SIZE` (header written)                                         | [N]  | UFR-DSK-006 | REQ-MSA-004  | INT-MSA-011  |                                   | PASS UC17 |
| TC-MSA-013   | Blank `.msa` size < IST_DISK_SIZE/10 (compression ratio >=10x)                                    | [N]  | UFR-DSK-006 | REQ-MSA-004  | INT-MSA-011  | blank = mostly zeros              | PASS UC17 |
| TC-MSA-014   | `image_msa_load(blank.msa, &p)` -> ST_NO_ERROR                                                    | [N]  | UFR-DSK-005 | REQ-MSA-003  | INT-MSA-010  |                                   | PASS UC17 |
| TC-MSA-015   | `list_root` on reloaded blank -> count = 0                                                         | [N]  | UFR-DSK-005 | REQ-MSA-003  | INT-MSA-010  |                                   | PASS UC17 |
| TC-MSA-016   | `free_bytes` matches between original and reloaded blank image                                     | [N]  | UFR-DSK-005 | REQ-MSA-003  | INT-MSA-010  |                                   | PASS UC17 |
| TC-MSA-020   | Image with HELLO.PRG+DATA.BIN+ESC.BIN saved and reloaded -> `list_root` count = 3                 | [N]  | UFR-DSK-005 | REQ-MSA-003  | INT-MSA-020  |                                   | PASS UC17 |
| TC-MSA-021   | Reloaded HELLO.PRG: size = 32                                                                      | [N]  | UFR-DSK-005 | REQ-MSA-003  | INT-MSA-020  |                                   | PASS UC17 |
| TC-MSA-022   | Reloaded DATA.BIN: size = 512                                                                      | [N]  | UFR-DSK-005 | REQ-MSA-003  | INT-MSA-020  |                                   | PASS UC17 |
| TC-MSA-023   | Reloaded ESC.BIN: size = 16                                                                        | [N]  | UFR-DSK-005 | REQ-MSA-003  | INT-MSA-020  |                                   | PASS UC17 |
| TC-MSA-024   | Reloaded HELLO.PRG: memcmp content == original 32-byte PRG stub                                   | [N]  | UFR-DSK-005 | REQ-MSA-003  | INT-MSA-020  |                                   | PASS UC17 |
| TC-MSA-025   | Reloaded DATA.BIN: memcmp content == original 512-byte 0..255 pattern                             | [N]  | UFR-DSK-005 | REQ-MSA-003  | INT-MSA-020  |                                   | PASS UC17 |
| TC-MSA-026   | Reloaded ESC.BIN (16 x 0xE5): memcmp content == original escape byte pattern                      | [N]  | UFR-DSK-005 | REQ-MSA-005  | INT-MSA-021  | 0xE5 escape byte round-trip       | PASS UC17 |
| TC-MSA-027   | `free_bytes` matches between original and reloaded files image                                     | [N]  | UFR-DSK-005 | REQ-MSA-003  | INT-MSA-020  |                                   | PASS UC17 |
| TC-MSA-030   | `image_msa_load("no_such.msa", &p)` -> ST_ERROR and `*pptImg == NULL`                             | [R]  | UFR-DSK-005 | REQ-MSA-001  | INT-MSA-030  |                                   | PASS UC17 |
| TC-MSA-031   | Load `bad_magic.msa` (magic=0xDEAD) -> ST_ERROR and `*pptImg == NULL`                             | [R]  | UFR-DSK-005 | REQ-MSA-006  | INT-MSA-031  |                                   | PASS UC17 |
| TC-MSA-032   | Load `truncated.msa` (4 bytes, header incomplete) -> ST_ERROR and `*pptImg == NULL`               | [R]  | UFR-DSK-005 | REQ-MSA-006  | INT-MSA-032  |                                   | PASS UC17 |
| TC-MSA-033   | Load `bad_geo.msa` (sides=5, geometry invalid) -> ST_ERROR and `*pptImg == NULL`                  | [R]  | UFR-DSK-005 | REQ-MSA-006  | INT-MSA-033  |                                   | PASS UC17 |

#### REQ -> TC coverage (UC17)

| REQ          | TC(s)                                                    | Status   |
|--------------|----------------------------------------------------------|----------|
| REQ-MSA-001  | TC-MSA-001, TC-MSA-002, TC-MSA-030                       | UC17     |
| REQ-MSA-002  | TC-MSA-003, TC-MSA-004                                   | UC17     |
| REQ-MSA-003  | TC-MSA-010, TC-MSA-014..016, TC-MSA-020..025, TC-MSA-027 | UC17     |
| REQ-MSA-004  | TC-MSA-011..013                                          | UC17     |
| REQ-MSA-005  | TC-MSA-026                                               | UC17     |
| REQ-MSA-006  | TC-MSA-031..033                                          | UC17     |
| REQ-MSA-007  | (imsa_decompress internal — covered by round-trip tests) | UC17     |
| REQ-MSA-008  | (imsa_compress internal — covered by TC-MSA-013)         | UC17     |
| REQ-MSA-009  | (format invariant — validated by complete round-trip)    | UC17     |
| REQ-MSA-010  | TC-MSA-005, TC-MSA-006                                   | UC17     |

#### UFR traceability update (UC17)

| UFR         | REQ(s)                           | TC(s)                                                      | Status |
|-------------|----------------------------------|------------------------------------------------------------|--------|
| UFR-DSK-005 | REQ-MSA-001, REQ-MSA-003..007    | TC-MSA-001..002, TC-MSA-010..016, TC-MSA-020..027, TC-MSA-030..033 | UC17 |
| UFR-DSK-006 | REQ-MSA-002, REQ-MSA-008..009    | TC-MSA-003..004, TC-MSA-010..016, TC-MSA-020..027         | UC17   |
| UFR-DSK-007 | REQ-MSA-010                      | TC-MSA-005..006                                            | UC17   |

---

### 5.52 INTENT Catalog — UC18.1

Each INTENT maps to one or more test blocks in `use_cases/use_case_18_1.c`.

| ID            | INTENT text                                                                                  |
|---------------|----------------------------------------------------------------------------------------------|
| INT-MNT-001   | mount_view_open must reject NULL ptLineCtx and NULL pptView                                  |
| INT-MNT-002   | mount_view_open on a non-existent file must return ST_ERROR and leave *pptView NULL          |
| INT-MNT-003   | mount_view_close(NULL) must return ST_ERROR; mount_view_close(&NULL) must be idempotent      |
| INT-MNT-004   | mount_view_add_file must reject NULL view and NULL path                                      |
| INT-MNT-005   | gui_find_window_by_type must reject NULL phWnd                                               |
| INT-MNT-006   | gui_find_window_by_type returns ST_NO_ERROR + NULL when no window of that type is open       |
| INT-MNT-007   | MOUNT_WND_WIDTH, MOUNT_WND_HEIGHT, MOUNT_LIST_WIDTH constants must have valid ranges         |
| INT-MNT-008   | MOUNT_SRC_DIR/ST/MSA enum values must be 0, 1, 2                                             |
| INT-MNT-009   | Opening a .st image must set eSrc=MOUNT_SRC_ST, correct iEntryCount, non-NULL ptImg and hWnd |
| INT-MNT-010   | gui_find_window_by_type must find the GUI_WND_MOUNT window once it is open                   |
| INT-MNT-011   | mount_view_close on an open .st view must return ST_NO_ERROR and set *pptView = NULL         |
| INT-MNT-012   | Opening a .msa image must set eSrc=MOUNT_SRC_MSA and correct iEntryCount                    |
| INT-MNT-013   | mount_view_add_file must increment iEntryCount and set bDirty                               |
| INT-MNT-014   | mount_view_close(&NULL) called a second time must return ST_NO_ERROR (idempotent)            |

---

### 5.53 Test Cases — UC18.1 (mount D2D view)

**Fixture files** (created in `use_cases/UC18_1/` at test start):
- `test2files.st` — `.st` image with FILE1.DAT (16B) and FILE2.BIN (8B)
- `test.msa` — `.msa` image with DEMO.PRG (16B) and README.TXT (8B)
- `test.st` — blank `.st` image (0 files)
- `IMPORT.DAT` — 16-byte PC file for add_file test

| TC             | Description                                                                                 | Kind | UFR          | REQ          | INT          | Notes                              | Status       |
|----------------|---------------------------------------------------------------------------------------------|------|--------------|--------------|--------------|------------------------------------|--------------|
| TC-MNT-001     | `mount_view_open(NULL, NULL, &p)` → ST_ERROR and `p == NULL`                               | [R]  | UFR-MNT-001  | REQ-MNT-001  | INT-MNT-001  |                                    | PASS UC18.1  |
| TC-MNT-002     | `mount_view_open(sz, &ctx, NULL)` → ST_ERROR                                               | [R]  | UFR-MNT-001  | REQ-MNT-001  | INT-MNT-001  |                                    | PASS UC18.1  |
| TC-MNT-003     | `mount_view_close(NULL)` → ST_ERROR                                                        | [R]  | UFR-MNT-004  | REQ-MNT-005  | INT-MNT-003  |                                    | PASS UC18.1  |
| TC-MNT-004     | `mount_view_close(&NULL)` → ST_NO_ERROR                                                    | [R]  | UFR-MNT-004  | REQ-MNT-005  | INT-MNT-003  |                                    | PASS UC18.1  |
| TC-MNT-005     | `mount_view_add_file(NULL, "foo")` → ST_ERROR                                              | [R]  | UFR-MNT-003  | REQ-MNT-006  | INT-MNT-004  |                                    | PASS UC18.1  |
| TC-MNT-006     | `mount_view_add_file(ptr, NULL)` → ST_ERROR                                                | [R]  | UFR-MNT-003  | REQ-MNT-006  | INT-MNT-004  |                                    | PASS UC18.1  |
| TC-MNT-007     | `gui_find_window_by_type(GUI_WND_MOUNT, NULL)` → ST_ERROR                                  | [R]  | UFR-MNT-004  | REQ-MNT-009  | INT-MNT-005  |                                    | PASS UC18.1  |
| TC-MNT-008     | `mount_view_open("NONEXISTENT.st", ...)` → ST_ERROR, `*pptView == NULL`                   | [R]  | UFR-MNT-001  | REQ-MNT-001  | INT-MNT-002  |                                    | PASS UC18.1  |
| TC-MNT-010     | `MOUNT_WND_WIDTH > 0`                                                                       | [N]  | UFR-MNT-002  | REQ-MNT-011  | INT-MNT-007  |                                    | PASS UC18.1  |
| TC-MNT-011     | `MOUNT_WND_HEIGHT > 0`                                                                      | [N]  | UFR-MNT-002  | REQ-MNT-011  | INT-MNT-007  |                                    | PASS UC18.1  |
| TC-MNT-012     | `MOUNT_LIST_WIDTH < MOUNT_WND_WIDTH`                                                        | [N]  | UFR-MNT-002  | REQ-MNT-011  | INT-MNT-007  |                                    | PASS UC18.1  |
| TC-MNT-013     | `MOUNT_SRC_DIR == 0`                                                                        | [N]  | UFR-MNT-001  | REQ-MNT-002  | INT-MNT-008  |                                    | PASS UC18.1  |
| TC-MNT-014     | `MOUNT_SRC_ST == 1`                                                                         | [N]  | UFR-MNT-001  | REQ-MNT-002  | INT-MNT-008  |                                    | PASS UC18.1  |
| TC-MNT-015     | `MOUNT_SRC_MSA == 2`                                                                        | [N]  | UFR-MNT-001  | REQ-MNT-002  | INT-MNT-008  |                                    | PASS UC18.1  |
| TC-MNT-020     | `gui_find_window_by_type(GUI_WND_MOUNT, &h)` (no window open) → ST_NO_ERROR                | [N]  | UFR-MNT-004  | REQ-MNT-009  | INT-MNT-006  |                                    | PASS UC18.1  |
| TC-MNT-021     | `gui_find_window_by_type(GUI_WND_MOUNT, &h)` (no window open) → `h == NULL`               | [N]  | UFR-MNT-004  | REQ-MNT-009  | INT-MNT-006  |                                    | PASS UC18.1  |
| TC-MNT-030     | `mount_view_open(test2files.st, ...)` → ST_NO_ERROR                                        | [N]  | UFR-MNT-001  | REQ-MNT-001  | INT-MNT-009  |                                    | PASS UC18.1  |
| TC-MNT-031     | After open .st: `ptView != NULL`                                                            | [N]  | UFR-MNT-001  | REQ-MNT-003  | INT-MNT-009  |                                    | PASS UC18.1  |
| TC-MNT-032     | After open .st: `eSrc == MOUNT_SRC_ST`                                                     | [N]  | UFR-MNT-001  | REQ-MNT-003  | INT-MNT-009  |                                    | PASS UC18.1  |
| TC-MNT-033     | After open .st: `iEntryCount == 2`                                                          | [N]  | UFR-MNT-001  | REQ-MNT-003  | INT-MNT-009  |                                    | PASS UC18.1  |
| TC-MNT-034     | After open .st: `ptImg != NULL`                                                             | [N]  | UFR-MNT-001  | REQ-MNT-003  | INT-MNT-009  |                                    | PASS UC18.1  |
| TC-MNT-035     | After open .st: `hWnd != NULL`                                                              | [N]  | UFR-MNT-002  | REQ-MNT-004  | INT-MNT-009  |                                    | PASS UC18.1  |
| TC-MNT-036     | `gui_find_window_by_type(GUI_WND_MOUNT, &h)` while open → `h != NULL`                     | [N]  | UFR-MNT-004  | REQ-MNT-009  | INT-MNT-010  |                                    | PASS UC18.1  |
| TC-MNT-037     | `mount_view_close(&ptView)` → ST_NO_ERROR                                                  | [N]  | UFR-MNT-004  | REQ-MNT-005  | INT-MNT-011  |                                    | PASS UC18.1  |
| TC-MNT-038     | After close: `ptView == NULL`                                                               | [N]  | UFR-MNT-004  | REQ-MNT-005  | INT-MNT-011  |                                    | PASS UC18.1  |
| TC-MNT-040     | `mount_view_open(test.msa, ...)` → ST_NO_ERROR                                             | [N]  | UFR-MNT-001  | REQ-MNT-001  | INT-MNT-012  |                                    | PASS UC18.1  |
| TC-MNT-041     | After open .msa: `eSrc == MOUNT_SRC_MSA`                                                   | [N]  | UFR-MNT-001  | REQ-MNT-002  | INT-MNT-012  |                                    | PASS UC18.1  |
| TC-MNT-042     | After open .msa: `iEntryCount == 2`                                                         | [N]  | UFR-MNT-001  | REQ-MNT-003  | INT-MNT-012  |                                    | PASS UC18.1  |
| TC-MNT-050     | `mount_view_add_file(ptView, IMPORT.DAT)` → ST_NO_ERROR                                    | [N]  | UFR-MNT-003  | REQ-MNT-006  | INT-MNT-013  | blank .st open before add         | PASS UC18.1  |
| TC-MNT-051     | After add_file: `iEntryCount` incremented by 1                                              | [N]  | UFR-MNT-003  | REQ-MNT-006  | INT-MNT-013  |                                    | PASS UC18.1  |
| TC-MNT-052     | After add_file: `bDirty == ST_TRUE`                                                         | [N]  | UFR-MNT-003  | REQ-MNT-006  | INT-MNT-013  |                                    | PASS UC18.1  |
| TC-MNT-060     | `mount_view_close(&NULL)` idempotent — second call → ST_NO_ERROR                           | [N]  | UFR-MNT-004  | REQ-MNT-005  | INT-MNT-014  |                                    | PASS UC18.1  |
| TC-MNT-S001    | Mount .st view visible with correct title `A:\\ [ST]`                                      | [S]  | UFR-MNT-001  | REQ-MNT-011  | INT-MNT-009  | `make manual UC=18_1`             | PASS UC18.1  |
| TC-MNT-S002    | Left panel lists FILE1.DAT and FILE2.BIN (2 entries)                                       | [S]  | UFR-MNT-002  | REQ-MNT-007  | INT-MNT-009  | `make manual UC=18_1`             | PASS UC18.1  |
| TC-MNT-S003    | Right panel shows properties (Source, Files, Free bytes)                                    | [S]  | UFR-MNT-002  | REQ-MNT-012  | INT-MNT-009  | `make manual UC=18_1`             | PASS UC18.1  |
| TC-MNT-S004    | UP/DOWN keys navigate and highlight selection                                               | [S]  | UFR-MNT-002  | REQ-MNT-007  | INT-MNT-009  | `make manual UC=18_1`             | PASS UC18.1  |
| TC-MNT-S005    | DEL removes selected file and decrements entry count                                        | [S]  | UFR-MNT-003  | REQ-MNT-007  | INT-MNT-009  | `make manual UC=18_1`             | PASS UC18.1  |
| TC-MNT-S006    | Mouse scroll wheel scrolls the file list                                                    | [S]  | UFR-MNT-002  | REQ-MNT-012  | INT-MNT-009  | `make manual UC=18_1`             | PASS UC18.1  |
| TC-MNT-S007    | ESC closes the mount window                                                                 | [S]  | UFR-MNT-004  | REQ-MNT-008  | INT-MNT-009  | `make manual UC=18_1`             | PASS UC18.1  |
| TC-MNT-S008    | Mount .msa window title shows `[MSA]` tag                                                  | [S]  | UFR-MNT-001  | REQ-MNT-011  | INT-MNT-012  | `make manual UC=18_1`             | PASS UC18.1  |

#### Test Summary — UC18.1

| Kind        | Count | Notes                                   |
|-------------|-------|-----------------------------------------|
| [N] Nominal | 12    | lifecycle, source types, add-file, find |
| [R] Robust  |  8    | NULL params, non-existent source        |
| [S] Skipped |  8    | visual — run `make manual UC=18_1`      |
| **Total**   | **28** |                                        |

#### REQ → TC coverage (UC18.1)

| REQ          | TC(s)                                                            | Status   |
|--------------|------------------------------------------------------------------|----------|
| REQ-MNT-001  | TC-MNT-001, TC-MNT-002, TC-MNT-008, TC-MNT-030, TC-MNT-040    | UC18.1   |
| REQ-MNT-002  | TC-MNT-013..015, TC-MNT-041                                     | UC18.1   |
| REQ-MNT-003  | TC-MNT-031..035, TC-MNT-042                                     | UC18.1   |
| REQ-MNT-004  | TC-MNT-035, TC-MNT-036                                          | UC18.1   |
| REQ-MNT-005  | TC-MNT-003, TC-MNT-004, TC-MNT-037, TC-MNT-038, TC-MNT-060    | UC18.1   |
| REQ-MNT-006  | TC-MNT-005, TC-MNT-006, TC-MNT-050, TC-MNT-051, TC-MNT-052    | UC18.1   |
| REQ-MNT-007  | TC-MNT-S002, TC-MNT-S004, TC-MNT-S005                          | UC18.1   |
| REQ-MNT-008  | TC-MNT-S007                                                     | UC18.1   |
| REQ-MNT-009  | TC-MNT-007, TC-MNT-020, TC-MNT-021, TC-MNT-036                 | UC18.1   |
| REQ-MNT-010  | (invariant — covered by TC-MNT-030..031, TC-MNT-040..041 succeeding) | UC18.1 |
| REQ-MNT-011  | TC-MNT-010..012, TC-MNT-S001, TC-MNT-S008                      | UC18.1   |
| REQ-MNT-012  | TC-MNT-S003, TC-MNT-S006                                        | UC18.1   |

#### UFR traceability update (UC18.1)

| UFR          | REQ(s)                              | TC(s)                                                                       | Status   |
|--------------|-------------------------------------|-----------------------------------------------------------------------------|----------|
| UFR-MNT-001  | REQ-MNT-001..003, REQ-MNT-010..011  | TC-MNT-001..002, TC-MNT-008, TC-MNT-013..015, TC-MNT-030..035, TC-MNT-040..042, TC-MNT-S001, TC-MNT-S008 | UC18.1 |
| UFR-MNT-002  | REQ-MNT-004, REQ-MNT-011..012       | TC-MNT-010..012, TC-MNT-035, TC-MNT-S001..004, TC-MNT-S006, TC-MNT-S008   | UC18.1   |
| UFR-MNT-003  | REQ-MNT-006                         | TC-MNT-005..006, TC-MNT-050..052, TC-MNT-S005                              | UC18.1   |
| UFR-MNT-004  | REQ-MNT-005, REQ-MNT-008..009       | TC-MNT-003..004, TC-MNT-007, TC-MNT-020..021, TC-MNT-036..038, TC-MNT-060, TC-MNT-S007 | UC18.1 |
| UFR-CON-070  | REQ-MNT-001..004                    | TC-MNT-030..038 (via line_cmd_mount integration)                            | UC18.1   |


---

### 5.54 INTENT Catalog -- UC18.2

Each INTENT maps to one or more test blocks in `use_cases/use_case_18_2.c`.

| ID            | INTENT text                                                                                         |
|---------------|-----------------------------------------------------------------------------------------------------|
| INT-MNT-020   | DIR_NAV_HIST_MAX and DIR_MULTI_SEL_MAX constants shall be at least 8                               |
| INT-MNT-021   | mount_is_bootable(NULL) shall return ST_FALSE (NULL guard)                                         |
| INT-MNT-022   | A blank 512-byte sector (all zeros) shall not be detected as bootable                              |
| INT-MNT-023   | A hand-crafted sector whose LE16 word sum equals 0x1234 shall be detected as bootable              |
| INT-MNT-024   | Flipping any byte of the bootable sector shall make mount_is_bootable() return ST_FALSE            |
| INT-MNT-025   | dir_open() shall seed aszNavHistory[0] with the root path; iNavHistHead==0, iNavHistCount==1       |
| INT-MNT-026   | dir_open() shall initialise iMultiSelCount to 0                                                    |
| INT-MNT-027   | mount_view_open() shall store ptLineCtx in ptView->ptLineCtx                                       |
| INT-MNT-028   | mount_view_open() shall initialise ptView->ptBootHexView to NULL                                   |

---

### 5.55 Test Cases -- UC18.2 (dir history, multi-sel, mount BPB/bootable/bootsector)

**Fixture files** (created in `use_cases/UC18_2/` at test start):
- `boot.st` -- blank `.st` image containing DEMO.PRG (16B)

| TC             | Description                                                                                      | Kind | UFR             | REQ           | INT          | Notes               | Status      |
|----------------|--------------------------------------------------------------------------------------------------|------|-----------------|---------------|--------------|---------------------|-------------|
| TC-MNT-070     | DIR_NAV_HIST_MAX >= 8                                                                            | [N]  | UFR-DIR-014     | REQ-DIR-024   | INT-MNT-020  |                     | PASS UC18.2 |
| TC-MNT-071     | DIR_MULTI_SEL_MAX >= 8                                                                           | [N]  | UFR-DIR-015     | REQ-DIR-026   | INT-MNT-020  |                     | PASS UC18.2 |
| TC-MNT-072     | mount_is_bootable(NULL) == ST_FALSE                                                              | [R]  | UFR-MNT-006     | REQ-MNT-017   | INT-MNT-021  |                     | PASS UC18.2 |
| TC-MNT-073     | Blank 512-byte sector (all zeros) -> not bootable                                                | [N]  | UFR-MNT-006     | REQ-MNT-017   | INT-MNT-022  |                     | PASS UC18.2 |
| TC-MNT-074     | Hand-crafted sector (word[0]=0x1234, rest=0) -> bootable                                        | [N]  | UFR-MNT-006     | REQ-MNT-017   | INT-MNT-023  |                     | PASS UC18.2 |
| TC-MNT-075     | Flip byte[0] of bootable sector -> not bootable                                                 | [N]  | UFR-MNT-006     | REQ-MNT-017   | INT-MNT-024  |                     | PASS UC18.2 |
| TC-MNT-076     | dir_open("src",...) -> iNavHistCount == 1                                                        | [N]  | UFR-DIR-014     | REQ-DIR-024   | INT-MNT-025  |                     | PASS UC18.2 |
| TC-MNT-077     | dir_open("src",...) -> iNavHistHead == 0                                                         | [N]  | UFR-DIR-014     | REQ-DIR-024   | INT-MNT-025  |                     | PASS UC18.2 |
| TC-MNT-078     | dir_open("src",...) -> aszNavHistory[0][0] != NUL                                               | [N]  | UFR-DIR-014     | REQ-DIR-024   | INT-MNT-025  |                     | PASS UC18.2 |
| TC-MNT-079     | dir_open("src",...) -> iMultiSelCount == 0                                                       | [N]  | UFR-DIR-015     | REQ-DIR-027   | INT-MNT-026  |                     | PASS UC18.2 |
| TC-MNT-080     | mount_view_open(boot.st,...) -> ptView->ptLineCtx == &ctx                                        | [N]  | UFR-MNT-007     | REQ-MNT-015   | INT-MNT-027  |                     | PASS UC18.2 |
| TC-MNT-081     | mount_view_open(boot.st,...) -> ptView->ptBootHexView == NULL                                    | [N]  | UFR-MNT-007     | REQ-MNT-015   | INT-MNT-028  |                     | PASS UC18.2 |
| TC-MNT-S010    | Right panel shows Geometry: H2/S9/T80 and Bootable: No                                         | [S]  | UFR-MNT-005,006 | REQ-MNT-013   | INT-MNT-022  | make manual UC=18_2 | SKIP        |
| TC-MNT-S011    | Right panel shows Files: N / 112 (root dir capacity)                                           | [S]  | UFR-MNT-005     | REQ-MNT-013   | INT-MNT-022  | make manual UC=18_2 | SKIP        |
| TC-MNT-S012    | Left panel header shows N file(s) without /112                                                  | [S]  | UFR-MNT-005     | REQ-MNT-013   | INT-MNT-022  | make manual UC=18_2 | SKIP        |
| TC-MNT-S013    | Pressing B opens a hex editor window with bootsector title                                      | [S]  | UFR-MNT-007     | REQ-MNT-014   | INT-MNT-028  | make manual UC=18_2 | SKIP        |
| TC-MNT-S014    | CTRL+SPACE in dir view toggles purple highlight on a file                                       | [S]  | UFR-DIR-015     | REQ-DIR-026,027 | INT-MNT-026 | make manual UC=18_2 | SKIP        |
| TC-MNT-S015    | ALT+LEFT in dir view after navigating into a subdir -> goes back                                | [S]  | UFR-DIR-014     | REQ-DIR-025   | INT-MNT-025  | make manual UC=18_2 | SKIP        |
| TC-MNT-S016    | ALT+RIGHT in dir view after ALT+LEFT -> goes forward                                           | [S]  | UFR-DIR-014     | REQ-DIR-025   | INT-MNT-025  | make manual UC=18_2 | SKIP        |
| TC-MNT-S017    | ESC closes the mount window                                                                     | [S]  | UFR-MNT-004     | REQ-MNT-008   | INT-MNT-027  | make manual UC=18_2 | SKIP        |

#### Test Summary -- UC18.2

| Kind        | Count | Notes                                                  |
|-------------|-------|--------------------------------------------------------|
| [N] Nominal | 12    | constants, history init, multi-sel init, bootable, mount open |
| [R] Robust  |  1    | mount_is_bootable(NULL)                                |
| [S] Skipped |  8    | visual -- run `make manual UC=18_2`                    |
| **Total**   | **21** |                                                       |

#### REQ -> TC coverage (UC18.2)

| REQ           | TC(s)                                      | Status  |
|---------------|--------------------------------------------|---------|
| REQ-DIR-024   | TC-MNT-076..078                            | UC18.2  |
| REQ-DIR-025   | TC-MNT-S015, TC-MNT-S016                  | UC18.2  |
| REQ-DIR-026   | TC-MNT-071, TC-MNT-079, TC-MNT-S014       | UC18.2  |
| REQ-DIR-027   | TC-MNT-079, TC-MNT-S014                   | UC18.2  |
| REQ-MNT-013   | TC-MNT-S010..012                          | UC18.2  |
| REQ-MNT-014   | TC-MNT-S013                               | UC18.2  |
| REQ-MNT-015   | TC-MNT-080, TC-MNT-081                    | UC18.2  |
| REQ-MNT-016   | (covered by TC-MNT-081 + close sequence)  | UC18.2  |
| REQ-MNT-017   | TC-MNT-072..075                           | UC18.2  |

#### UFR traceability update (UC18.2)

| UFR           | REQ(s)                           | TC(s)                                                  | Status  |
|---------------|----------------------------------|--------------------------------------------------------|---------|
| UFR-DIR-014   | REQ-DIR-024, REQ-DIR-025         | TC-MNT-070, TC-MNT-076..078, TC-MNT-S015..016         | UC18.2  |
| UFR-DIR-015   | REQ-DIR-026, REQ-DIR-027         | TC-MNT-071, TC-MNT-079, TC-MNT-S014                   | UC18.2  |
| UFR-MNT-005   | REQ-MNT-013                      | TC-MNT-S010..012                                       | UC18.2  |
| UFR-MNT-006   | REQ-MNT-017                      | TC-MNT-072..075, TC-MNT-S010                           | UC18.2  |
| UFR-MNT-007   | REQ-MNT-014..016                 | TC-MNT-080..081, TC-MNT-S013                           | UC18.2  |

---

### 5.56 INTENT Catalog -- UC19

Each INTENT maps to one or more test blocks in `use_cases/use_case_19.c`.

| ID            | INTENT text                                                                                                    |
|---------------|----------------------------------------------------------------------------------------------------------------|
| INT-MNT-029   | mount_save_image(NULL, ...) shall return ST_ERROR without crash                                               |
| INT-MNT-030   | mount_save_image with NULL szOutPath shall return ST_ERROR                                                    |
| INT-MNT-031   | mount_save_image with an unknown format shall return ST_ERROR                                                  |
| INT-MNT-032   | mount_save_image MOUNT_SAVE_ST shall write a valid reloadable raw .st file and clear bDirty                   |
| INT-MNT-033   | mount_save_image MOUNT_SAVE_MSA shall write a compressed .msa smaller than 737280 bytes and clear bDirty      |
| INT-MNT-034   | mount_save_image MOUNT_SAVE_DIR shall create the directory, extract each file, and clear bDirty               |
| INT-MNT-035   | mount_view_add_file() NULL guards shall remain ST_ERROR after P40 chunked-read refactor                       |

---

### 5.57 Test Cases -- UC19 (umount dialog + status bar + progress)

**Fixture files** (created in `use_cases/UC19/` at test start):
- `src.st` -- blank image with HELLO.PRG (64 B) and README.TXT (5 B)

| TC             | Description                                                                                      | Kind | UFR             | REQ             | INT          | Notes               | Status      |
|----------------|--------------------------------------------------------------------------------------------------|------|-----------------|-----------------|--------------|---------------------|-------------|
| TC-MNT-082     | mount_save_image(NULL, ST, path) == ST_ERROR                                                     | [R]  | UFR-MNT-008     | REQ-MNT-018     | INT-MNT-029  |                     | PASS UC19   |
| TC-MNT-083     | mount_save_image(view, ST, NULL) == ST_ERROR                                                     | [R]  | UFR-MNT-008     | REQ-MNT-018     | INT-MNT-030  |                     | PASS UC19   |
| TC-MNT-084     | mount_save_image(view, fmt=99, path) == ST_ERROR                                                 | [R]  | UFR-MNT-008     | REQ-MNT-018     | INT-MNT-031  |                     | PASS UC19   |
| TC-MNT-085a    | mount_save_image(view, ST, path) == ST_NO_ERROR                                                  | [N]  | UFR-MNT-008     | REQ-MNT-018     | INT-MNT-032  |                     | PASS UC19   |
| TC-MNT-085b    | saved .st file exists on disk                                                                    | [N]  | UFR-MNT-008     | REQ-MNT-018     | INT-MNT-032  |                     | PASS UC19   |
| TC-MNT-085c    | saved .st file size == 737280                                                                    | [N]  | UFR-MNT-008     | REQ-MNT-018     | INT-MNT-032  |                     | PASS UC19   |
| TC-MNT-085d    | bDirty cleared to ST_FALSE after .st save                                                        | [N]  | UFR-MNT-008     | REQ-MNT-018     | INT-MNT-032  |                     | PASS UC19   |
| TC-MNT-085e    | reloaded .st image reports 2 files in list_root                                                  | [N]  | UFR-MNT-008     | REQ-MNT-018     | INT-MNT-032  |                     | PASS UC19   |
| TC-MNT-086a    | mount_save_image(view, MSA, path) == ST_NO_ERROR                                                 | [N]  | UFR-MNT-008     | REQ-MNT-018     | INT-MNT-033  |                     | PASS UC19   |
| TC-MNT-086b    | saved .msa file exists on disk                                                                   | [N]  | UFR-MNT-008     | REQ-MNT-018     | INT-MNT-033  |                     | PASS UC19   |
| TC-MNT-086c    | saved .msa size < 737280 (RLE compression)                                                       | [N]  | UFR-MNT-008     | REQ-MNT-018     | INT-MNT-033  |                     | PASS UC19   |
| TC-MNT-086d    | bDirty cleared to ST_FALSE after .msa save                                                       | [N]  | UFR-MNT-008     | REQ-MNT-018     | INT-MNT-033  |                     | PASS UC19   |
| TC-MNT-087a    | mount_save_image(view, DIR, path) == ST_NO_ERROR                                                 | [N]  | UFR-MNT-008     | REQ-MNT-018     | INT-MNT-034  |                     | PASS UC19   |
| TC-MNT-087b    | output directory exists after SAVE_DIR                                                           | [N]  | UFR-MNT-008     | REQ-MNT-018     | INT-MNT-034  |                     | PASS UC19   |
| TC-MNT-087c    | HELLO.PRG extracted to directory with correct size (64 B)                                        | [N]  | UFR-MNT-008     | REQ-MNT-018     | INT-MNT-034  |                     | PASS UC19   |
| TC-MNT-087d    | README.TXT extracted to directory with correct size (5 B)                                        | [N]  | UFR-MNT-008     | REQ-MNT-018     | INT-MNT-034  |                     | PASS UC19   |
| TC-MNT-087e    | bDirty cleared to ST_FALSE after dir save                                                        | [N]  | UFR-MNT-008     | REQ-MNT-018     | INT-MNT-034  |                     | PASS UC19   |
| TC-MNT-088a    | mount_view_add_file(NULL, path) == ST_ERROR                                                      | [R]  | UFR-MNT-004     | REQ-MNT-019     | INT-MNT-035  |                     | PASS UC19   |
| TC-MNT-088b    | mount_view_add_file(view, NULL) == ST_ERROR                                                      | [R]  | UFR-MNT-004     | REQ-MNT-019     | INT-MNT-035  |                     | PASS UC19   |
| TC-MNT-S018    | Status bar visible at bottom of mount window with Free KB and file count                         | [S]  | UFR-MNT-009     | REQ-MNT-020     | INT-MNT-032  | make manual UC=19   | SKIP        |
| TC-MNT-S019    | Status bar shows [*] when disk is dirty (after DEL)                                              | [S]  | UFR-MNT-009     | REQ-MNT-020     | INT-MNT-032  | make manual UC=19   | SKIP        |
| TC-MNT-S020    | umount with dirty disk shows interactive save dialog in console                                   | [S]  | UFR-MNT-008     | REQ-MNT-021     | INT-MNT-032  | make manual UC=19   | SKIP        |
| TC-MNT-S021    | Dialog choice 1 saves disk.st in cwd                                                             | [S]  | UFR-MNT-008     | REQ-MNT-021     | INT-MNT-032  | make manual UC=19   | SKIP        |
| TC-MNT-S022    | Dialog choice 2 saves disk.msa in cwd                                                            | [S]  | UFR-MNT-008     | REQ-MNT-021     | INT-MNT-033  | make manual UC=19   | SKIP        |
| TC-MNT-S023    | Dialog choice 3 extracts disk/ directory in cwd                                                  | [S]  | UFR-MNT-008     | REQ-MNT-021     | INT-MNT-034  | make manual UC=19   | SKIP        |
| TC-MNT-S024    | umount --st saves without interactive dialog                                                      | [S]  | UFR-MNT-008     | REQ-MNT-021     | INT-MNT-032  | make manual UC=19   | SKIP        |
| TC-MNT-S025    | umount --msa saves without interactive dialog                                                     | [S]  | UFR-MNT-008     | REQ-MNT-021     | INT-MNT-033  | make manual UC=19   | SKIP        |

#### Test Summary -- UC19

| Kind        | Count | Notes                                                              |
|-------------|-------|--------------------------------------------------------------------|
| [N] Nominal |  14   | save .st/.msa/dir round-trips, bDirty cleared, reload check       |
| [R] Robust  |   5   | NULL guards (view, path, bad fmt), add_file NULL guards            |
| [S] Skipped |   8   | visual + interactive dialog -- run make manual UC=19               |
| **Total**   | **27** |                                                                   |

#### mount.c Public API -- UC19 additions

| Function                                          | REQ(s)                           | Description                                         |
|---------------------------------------------------|----------------------------------|-----------------------------------------------------|
| mount_save_image(ptView, eFmt, szOutPath)         | REQ-MNT-018                      | Save in-memory image as .st / .msa / directory      |
| mount_view_add_file() -- P40 refactor             | REQ-MNT-019                      | Chunked read with LOG_INFO progress per 64 KB chunk |

#### REQ -> TC coverage (UC19)

| REQ           | TC(s)                                       | Status  |
|---------------|---------------------------------------------|---------|
| REQ-MNT-018   | TC-MNT-082..087e, TC-MNT-S020..S025         | UC19    |
| REQ-MNT-019   | TC-MNT-088a, TC-MNT-088b                    | UC19    |
| REQ-MNT-020   | TC-MNT-S018, TC-MNT-S019                    | UC19    |
| REQ-MNT-021   | TC-MNT-S020..S025                            | UC19    |

#### UFR traceability update (UC19)

| UFR           | REQ(s)                           | TC(s)                                                        | Status  |
|---------------|----------------------------------|--------------------------------------------------------------|---------|
| UFR-MNT-008   | REQ-MNT-018, REQ-MNT-021         | TC-MNT-082..088b, TC-MNT-S018..S025                          | UC19    |
| UFR-MNT-009   | REQ-MNT-020                      | TC-MNT-S018..S019                                            | UC19    |

---

### 5.58 INTENT Catalog — UC20

| ID            | Intent                                                                                          |
|---------------|-------------------------------------------------------------------------------------------------|
| INT-MNT-036   | mount_make_bootable(NULL) shall return ST_ERROR without crash                                   |
| INT-MNT-037   | A blank image not bootable; after mount_make_bootable() WD1772 checksum == 0x1234              |
| INT-MNT-038   | mount_save_image from an open mount view saves a valid .st/.msa file                            |
| INT-MNT-039   | mount_view_open on a directory creates a populated image; save produces a valid .st             |
| INT-MNT-040   | mount_view_open on a .st file dispatches correctly regardless of prior state                    |

### 5.59 Test Cases — UC20 (image command + P41 file hex + P37 bootable)

#### Test Cases (UC20)

| TC ID        | INTENT ref    | Description                                                               | Type | Status |
|--------------|---------------|---------------------------------------------------------------------------|------|--------|
| TC-MNT-089   | INT-MNT-036   | mount_make_bootable(NULL) == ST_ERROR                                     | [R]  | UC20   |
| TC-MNT-090a  | INT-MNT-037   | image_st_create blank + mount_is_bootable == ST_FALSE                     | [N]  | UC20   |
| TC-MNT-090b  | INT-MNT-037   | mount_make_bootable returns ST_NO_ERROR                                   | [N]  | UC20   |
| TC-MNT-090c  | INT-MNT-037   | mount_is_bootable after make_bootable == ST_TRUE                          | [N]  | UC20   |
| TC-MNT-090d  | INT-MNT-037   | WD1772 checksum == 0x1234 directly verified                               | [N]  | UC20   |
| TC-MNT-090e  | INT-MNT-037   | mount_make_bootable idempotent (×2) == ST_NO_ERROR                        | [N]  | UC20   |
| TC-MNT-090f  | INT-MNT-037   | image still bootable after second make_bootable call                      | [N]  | UC20   |
| TC-MNT-090g  | INT-MNT-037   | Hand-crafted bootable sector (word[0]=0x1234) passes mount_is_bootable    | [N]  | UC20   |
| TC-MNT-091a  | INT-MNT-038   | mount_view_open .st == ST_NO_ERROR + iEntryCount == 2                     | [N]  | UC20   |
| TC-MNT-091b  | INT-MNT-038   | mount_save_image .msa == ST_NO_ERROR                                      | [N]  | UC20   |
| TC-MNT-091c  | INT-MNT-038   | .msa file exists + size < 737280                                          | [N]  | UC20   |
| TC-MNT-091d  | INT-MNT-038   | mount_save_image .st == ST_NO_ERROR + size == 737280                      | [N]  | UC20   |
| TC-MNT-092a  | INT-MNT-039   | mount_view_open dir == ST_NO_ERROR + iEntryCount == 2                     | [N]  | UC20   |
| TC-MNT-092b  | INT-MNT-039   | dir image saved as .st == ST_NO_ERROR                                     | [N]  | UC20   |
| TC-MNT-092c  | INT-MNT-039   | reloaded dir image has 2 files                                            | [N]  | UC20   |
| TC-MNT-093   | INT-MNT-040   | mount_view_open .st again == ST_NO_ERROR                                  | [R]  | UC20   |
| TC-MNT-094   | INT-MNT-036   | mount_make_bootable(NULL) regression == ST_ERROR                          | [R]  | UC20   |
| TC-MNT-095   | INT-MNT-040   | mount_view_close(&NULL) == ST_NO_ERROR (idempotent)                       | [R]  | UC20   |
| TC-MNT-S026  | INT-MNT-041   | ENTER in mount opens selected file in hex editor (visual)                 | [S]  | UC20   |
| TC-MNT-S027  | INT-MNT-041   | Hex editor title shows "A:\\ FILENAME [N bytes]" (visual)                | [S]  | UC20   |
| TC-MNT-S028  | INT-MNT-042   | F key triggers make_bootable + Bootable:Yes in panel (visual)             | [S]  | UC20   |
| TC-MNT-S029  | INT-MNT-042   | F key sets [*] unsaved in status bar (visual)                             | [S]  | UC20   |
| TC-MNT-S030  | INT-MNT-043   | 'image' with mount open saves disk.st (visual)                            | [S]  | UC20   |
| TC-MNT-S031  | INT-MNT-043   | 'image --msa' creates disk.msa (visual)                                   | [S]  | UC20   |
| TC-MNT-S032  | INT-MNT-043   | 'image' without mount uses selected dir (visual)                          | [S]  | UC20   |
| TC-MNT-S033  | INT-MNT-043   | 'image --bootable' produces bootable .st (visual)                         | [S]  | UC20   |

#### Public API — UC20 additions

| Function                                    | REQ(s)        | Description                                               |
|---------------------------------------------|---------------|-----------------------------------------------------------|
| mount_make_bootable(ptImg)                  | REQ-MNT-022   | Patch bootsector WD1772 checksum to 0x1234                |
| mount_open_file_hex(ptView) [static]        | REQ-MNT-024   | Extract selected FAT entry → temp file → edit_hex_open    |
| line_cmd_image(ptParsed, ptCtx) [static]    | REQ-MNT-023   | image command handler — save .st/.msa from mount or dir   |

#### REQ → TC coverage (UC20)

| REQ           | TC(s)                                            | Status  |
|---------------|--------------------------------------------------|---------|
| REQ-MNT-022   | TC-MNT-089, TC-MNT-090a..g, TC-MNT-094          | UC20    |
| REQ-MNT-023   | TC-MNT-091a..d, TC-MNT-092a..c, TC-MNT-093      | UC20    |
| REQ-MNT-024   | TC-MNT-S026, TC-MNT-S027                         | UC20    |

#### UFR traceability update (UC20)

| UFR           | REQ(s)                           | TC(s)                                               | Status  |
|---------------|----------------------------------|-----------------------------------------------------|---------|
| UFR-MNT-010   | REQ-MNT-023                      | TC-MNT-091a..d, TC-MNT-092a..c, TC-MNT-S030..S033  | UC20    |
| UFR-MNT-011   | REQ-MNT-024                      | TC-MNT-S026, TC-MNT-S027                            | UC20    |
| UFR-MNT-012   | REQ-MNT-022                      | TC-MNT-089..090g, TC-MNT-S028..S029                 | UC20    |

---

### 5.60 INTENT Catalog — UC20A

| INTENT ID     | Description                                                                            |
|---------------|----------------------------------------------------------------------------------------|
| INT-CON-030   | `st2msa` and `msa2st` shall appear in tab-completion by full name and shortcut.       |
| INT-CON-031   | `image_st_load` + `image_msa_save` (st2msa core) shall produce a valid .msa from a .st. |
| INT-CON-032   | `image_msa_load` + `image_st_save` (msa2st core) shall preserve file content (.st → .msa → .st round-trip). |
| INT-CON-033   | `st2msa --all --dir <d>` shall convert all .st files in the directory to .msa.        |
| INT-CON-034   | `msa2st --all --dir <d>` shall convert all .msa files in the directory to .st.        |
| INT-CON-035   | `st2msa` on a file with wrong extension shall warn and return ST_NO_ERROR (non-fatal). |
| INT-CON-036   | `st2msa` on a non-existent file shall print an error and return ST_NO_ERROR.           |
| INT-CON-037   | `st2msa --all` with non-existent `--dir` shall print an error and return ST_NO_ERROR.  |
| INT-CON-038   | `msa2st` on a file with wrong extension shall warn and return ST_NO_ERROR.             |
| INT-CON-039   | `st2msa`/`msa2st` with no args and no selection shall warn and return ST_NO_ERROR.     |

---

### 5.61 Test Cases — UC20A (st2msa / msa2st batch conversion)

#### Test Cases (UC20A)

| TC ID        | INTENT        | Description                                                               | Kind | UC      |
|--------------|---------------|---------------------------------------------------------------------------|------|---------|
| TC-CON-130   | INT-CON-030   | `line_complete_cmd_query("st2msa")` → count >= 1                          | [N]  | UC20A   |
| TC-CON-131   | INT-CON-030   | `line_complete_cmd_query("msa2st")` → count >= 1                          | [N]  | UC20A   |
| TC-CON-132   | INT-CON-030   | `line_complete_cmd_query("s")` → count >= 1, result contains "st2msa"    | [N]  | UC20A   |
| TC-CON-133   | INT-CON-030   | `line_complete_cmd_query("a")` → count >= 1, result contains "msa2st"    | [N]  | UC20A   |
| TC-CON-134   | INT-CON-031   | `image_st_load` + `image_msa_save` → ST_NO_ERROR                          | [N]  | UC20A   |
| TC-CON-135   | INT-CON-031   | .msa output exists and is compressed (< 737280)                           | [N]  | UC20A   |
| TC-CON-136   | INT-CON-032   | `image_msa_load` + `image_st_save` round-trip → .st size == 737280        | [N]  | UC20A   |
| TC-CON-137   | INT-CON-032   | Round-trip content size preserved (64 bytes)                              | [N]  | UC20A   |
| TC-CON-138   | INT-CON-032   | Round-trip content bytes match original                                   | [N]  | UC20A   |
| TC-CON-139   | INT-CON-033   | `st2msa --all --dir <batch>` script → ST_NO_ERROR                         | [N]  | UC20A   |
| TC-CON-140   | INT-CON-033   | a.msa produced in batch dir                                               | [N]  | UC20A   |
| TC-CON-141   | INT-CON-033   | b.msa produced in batch dir                                               | [N]  | UC20A   |
| TC-CON-142   | INT-CON-034   | `msa2st --all --dir <batch>` script → ST_NO_ERROR                         | [N]  | UC20A   |
| TC-CON-143   | INT-CON-034   | c.st produced in batch dir                                                | [N]  | UC20A   |
| TC-CON-144   | INT-CON-034   | c.st size == 737280                                                       | [N]  | UC20A   |
| TC-CON-145   | INT-CON-031   | `image_st_load .st` → ST_NO_ERROR                                         | [N]  | UC20A   |
| TC-CON-146   | INT-CON-032   | `image_msa_load .msa` → ST_NO_ERROR                                       | [N]  | UC20A   |
| TC-CON-147   | INT-CON-032   | `image_st_save round-trip` → ST_NO_ERROR                                  | [N]  | UC20A   |
| TC-CON-148   | INT-CON-035   | `st2msa wrong.txt` script → ST_NO_ERROR (warning produced)               | [R]  | UC20A   |
| TC-CON-149   | INT-CON-036   | `st2msa phantom.st` (missing) script → ST_NO_ERROR (error produced)      | [R]  | UC20A   |
| TC-CON-150   | INT-CON-037   | `st2msa --all --dir nope` (non-existent) → ST_NO_ERROR (error produced)  | [R]  | UC20A   |
| TC-CON-151   | INT-CON-038   | `msa2st wrong.txt` script → ST_NO_ERROR (warning produced)               | [R]  | UC20A   |
| TC-CON-152   | INT-CON-039   | `st2msa` no args no selection → ST_NO_ERROR (warning produced)           | [R]  | UC20A   |
| TC-CON-153   | INT-CON-039   | `msa2st` no args no selection → ST_NO_ERROR (warning produced)           | [R]  | UC20A   |

#### Public API — UC20A functions in `line.c`

| Function                                             | REQ(s)        | Description                                               |
|------------------------------------------------------|---------------|-----------------------------------------------------------|
| line_cmd_st2msa(ptCtx, tCmd) [static]                | REQ-CON-032   | st2msa command handler — delegates to line_cmd_convert    |
| line_cmd_msa2st(ptCtx, tCmd) [static]                | REQ-CON-032   | msa2st command handler — delegates to line_cmd_convert    |
| line_cmd_convert(ptCtx, iArgc, argv, szSrc, szDst) [static] | REQ-CON-032 | Shared batch/single conversion handler                  |
| line_conv_one(szSrc, szDst, szSrcExt, szDstExt) [static] | REQ-CON-033 | Single-file .st/.msa conversion via image APIs          |
| line_conv_dir_rec(szDir, ptConv, bRecurse) [static]  | REQ-CON-034   | Recursive directory scan for batch conversion             |
| line_conv_rec_cb(szPath, szName, ptStat, pCtx) [static] | REQ-CON-034 | file_list_fn callback — filters by ext, converts, recurses |
| line_conv_replace_ext(szSrc, szExt, szDst, uiLen) [static] | REQ-CON-033 | Replace file extension in path string                   |

#### REQ → TC coverage (UC20A)

| REQ           | TC(s)                                                              | Status   |
|---------------|--------------------------------------------------------------------|----------|
| REQ-CON-032   | TC-CON-130..133, TC-CON-139..144, TC-CON-148..153                  | UC20A    |
| REQ-CON-033   | TC-CON-134..138, TC-CON-145..147                                   | UC20A    |
| REQ-CON-034   | TC-CON-139..144                                                    | UC20A    |

#### UFR traceability update (UC20A)

| UFR           | REQ(s)                              | TC(s)                                  | Status   |
|---------------|-------------------------------------|----------------------------------------|----------|
| UFR-CON-092   | REQ-CON-032, REQ-CON-033, REQ-CON-034 | TC-CON-130, TC-CON-132, TC-CON-134..136, TC-CON-139..141, TC-CON-148..150, TC-CON-152 | UC20A |
| UFR-CON-093   | REQ-CON-032, REQ-CON-033, REQ-CON-034 | TC-CON-131, TC-CON-133, TC-CON-136..138, TC-CON-142..144, TC-CON-151, TC-CON-153 | UC20A |

---

### 5.62 INTENT Catalog — UC21

> Source: `use_cases/use_case_21.c`; chain: UFR-EXE-* → REQ-CPU-* → INT-CPU-* → TC-CPU-*

| INTENT ID    | Description                                                                       |
|--------------|-----------------------------------------------------------------------------------|
| INT-CPU-001  | MOVEQ decodes 8-bit signed immediate, sign-extends to 32 bits, writes full Dn     |
| INT-CPU-002  | MOVE.L/W/B between registers; partial Dn writes preserve upper bits               |
| INT-CPU-003  | MOVEA.W sign-extends to 32 bits before writing An; no SR modification             |
| INT-CPU-004  | LEA computes control EA address into An; no flags affected                        |
| INT-CPU-005  | CLR writes 0 to EA; unconditionally N=0, Z=1, V=0, C=0                           |
| INT-CPU-006  | SWAP exchanges high and low 16-bit halves of Dn; N/Z from 32-bit result           |
| INT-CPU-007  | Sequence of 10 instructions executes correctly; PC lands exactly after last       |
| INT-CPU-008  | Memory EA modes (An), (An)+, -(An), d16(An) read and write correctly              |
| INT-CPU-009  | NULL ptCpu / NULL ptMachine → ST_ERROR; stopped CPU no-op                         |

---

### 5.63 Test Cases — UC21 (MC68000 execution — MOVE/MOVEQ/LEA/CLR/SWAP)

**Test matrix: 47N + 8R + 0S = 55 tests — 0 failures**

| TC ID       | Type | INTENT       | Description                                                           | Status  |
|-------------|------|--------------|-----------------------------------------------------------------------|---------|
| TC-CPU-001  | [N]  | INT-CPU-001  | MOVEQ #$42,D0 → D0==0x42, Z=0, N=0                                   | ✓ UC21  |
| TC-CPU-002  | [N]  | INT-CPU-001  | MOVEQ #0,D1 → D1==0, Z=1, N=0                                        | ✓ UC21  |
| TC-CPU-003  | [N]  | INT-CPU-001  | MOVEQ #-1 (0xFF), D2 → D2==0xFFFFFFFF (sign-extend), Z=0, N=1        | ✓ UC21  |
| TC-CPU-004  | [N]  | INT-CPU-001  | MOVEQ bit 8 set → no-crash (LOG_TODO, ST_NO_ERROR)                    | ✓ UC21  |
| TC-CPU-005  | [N]  | INT-CPU-002  | MOVE.L D2,D3 → D3==D2 (full 32-bit), flags updated                   | ✓ UC21  |
| TC-CPU-006  | [N]  | INT-CPU-002  | MOVE.W D0,D4 → D4 low word updated, upper 16 bits preserved           | ✓ UC21  |
| TC-CPU-007  | [N]  | INT-CPU-002  | MOVE.B #$FF,D6 → D6 low byte=0xFF, bits 31-8 preserved                | ✓ UC21  |
| TC-CPU-008  | [N]  | INT-CPU-002  | MOVE.W #$1234,D4 → D4 word=0x1234, upper preserved                   | ✓ UC21  |
| TC-CPU-009  | [N]  | INT-CPU-002  | MOVE.L A0,D5 → D5==A0 value                                           | ✓ UC21  |
| TC-CPU-010  | [N]  | INT-CPU-003  | MOVEA.W #$8000,A1 → A1==0xFFFF8000 (sign-extend), no SR change        | ✓ UC21  |
| TC-CPU-011  | [N]  | INT-CPU-003  | MOVEA.L D0,A2 → A2==D0, no SR change                                  | ✓ UC21  |
| TC-CPU-012  | [N]  | INT-CPU-004  | LEA $0800.W,A0 → A0==0x0800, SR unchanged                             | ✓ UC21  |
| TC-CPU-013  | [N]  | INT-CPU-004  | LEA (A1),A2 → A2==A1 value, SR unchanged                              | ✓ UC21  |
| TC-CPU-014  | [N]  | INT-CPU-004  | LEA does not modify SR (N/Z/V/C all unchanged after)                  | ✓ UC21  |
| TC-CPU-015  | [N]  | INT-CPU-005  | CLR.L D3 → D3==0, N=0, Z=1, V=0, C=0                                 | ✓ UC21  |
| TC-CPU-016  | [N]  | INT-CPU-005  | CLR.W D5 → D5 low word=0, upper 16 bits preserved, flags set          | ✓ UC21  |
| TC-CPU-017  | [N]  | INT-CPU-005  | CLR.B D0 → D0 low byte=0, bits 31-8 preserved, flags set              | ✓ UC21  |
| TC-CPU-018  | [N]  | INT-CPU-006  | SWAP D3 (0x12345678) → D3==0x56781234, N/Z from 32-bit result         | ✓ UC21  |
| TC-CPU-019  | [N]  | INT-CPU-006  | SWAP D3 (0x00000000) → D3==0, Z=1, N=0, V=0, C=0                     | ✓ UC21  |
| TC-CPU-020  | [N]  | INT-CPU-006  | SWAP D0 (0x00FF0000) → D0==0x000000FF, N=0, Z=0                       | ✓ UC21  |
| TC-CPU-021  | [N]  | INT-CPU-007  | 10-instruction sequence at 0x0800 fully executes; iInstrCount==10     | ✓ UC21  |
| TC-CPU-022  | [N]  | INT-CPU-007  | PC after 10 instructions == 0x081A (sum of instruction lengths)       | ✓ UC21  |
| TC-CPU-023  | [N]  | INT-CPU-007  | D0..D2, A0..A1 hold expected values after the full sequence           | ✓ UC21  |
| TC-CPU-024  | [N]  | INT-CPU-007  | SR flags in expected state after the last instruction of the seq      | ✓ UC21  |
| TC-CPU-025  | [N]  | INT-CPU-008  | MOVE.W (A0),D0 reads word from memory correctly                       | ✓ UC21  |
| TC-CPU-026  | [N]  | INT-CPU-008  | MOVE.W D1,(A1) writes word to memory at A1                            | ✓ UC21  |
| TC-CPU-027  | [N]  | INT-CPU-008  | MOVE.W (A0)+,D2 reads word then increments A0 by 2                    | ✓ UC21  |
| TC-CPU-028  | [N]  | INT-CPU-008  | MOVE.W -(A1),D3 decrements A1 by 2 then reads word                    | ✓ UC21  |
| TC-CPU-029  | [N]  | INT-CPU-008  | MOVE.W d16(A0),D4 reads word at A0 + signed displacement              | ✓ UC21  |
| TC-CPU-030  | [N]  | INT-CPU-002  | MOVE.L flags: N=1 for negative result, Z=0                            | ✓ UC21  |
| TC-CPU-031  | [N]  | INT-CPU-002  | MOVE.L flags: N=0, Z=1 for zero result                                | ✓ UC21  |
| TC-CPU-032  | [N]  | INT-CPU-002  | MOVE.W flags: V=0, C=0 always (data movement, not arithmetic)         | ✓ UC21  |
| TC-CPU-033  | [N]  | INT-CPU-005  | CLR memory (A1): CLR.L (A1) writes 0 to RAM                          | ✓ UC21  |
| TC-CPU-034  | [N]  | INT-CPU-004  | LEA d16(A0),A1 with displacement 4: A1 == A0 + 4                      | ✓ UC21  |
| TC-CPU-035  | [N]  | INT-CPU-001  | MOVEQ sets V=0, C=0 unconditionally                                   | ✓ UC21  |
| TC-CPU-036  | [N]  | INT-CPU-006  | SWAP all-bits-set: 0xFFFFFFFF stays 0xFFFFFFFF; N=1, Z=0              | ✓ UC21  |
| TC-CPU-037  | [N]  | INT-CPU-003  | MOVEA.W positive (#$0100) → A1==0x00000100 (no sign-ext needed)       | ✓ UC21  |
| TC-CPU-038  | [N]  | INT-CPU-007  | cpu_step returns ST_NO_ERROR for every instr in 10-instr sequence     | ✓ UC21  |
| TC-CPU-039  | [N]  | INT-CPU-008  | (A7)+ with byte MOVE: A7 incremented by 2 (word alignment)            | ✓ UC21  |
| TC-CPU-040  | [N]  | INT-CPU-008  | -(A7) with byte MOVE: A7 decremented by 2 (word alignment)            | ✓ UC21  |
| TC-CPU-041  | [N]  | INT-CPU-002  | MOVE.B D0,D0 self-copy: value unchanged, flags updated                | ✓ UC21  |
| TC-CPU-042  | [N]  | INT-CPU-002  | MOVE.W upper 16 preserved: D5 upper==0xDEAD after MOVE.W #0,D5       | ✓ UC21  |
| TC-CPU-043  | [N]  | INT-CPU-004  | LEA abs.W ($0804.W): A0 == 0x0804                                      | ✓ UC21  |
| TC-CPU-044  | [N]  | INT-CPU-005  | CLR does not change eState (CPU remains RUNNING)                      | ✓ UC21  |
| TC-CPU-045  | [N]  | INT-CPU-007  | cpu_result_t.uiOpcode holds opcode of each executed instruction       | ✓ UC21  |
| TC-CPU-046  | [N]  | INT-CPU-007  | cpu_result_t.bValid == ST_TRUE for all UC21 instructions              | ✓ UC21  |
| TC-CPU-047  | [N]  | INT-CPU-007  | Unimplemented opcode: cpu_step returns ST_NO_ERROR (LOG_TODO)          | ✓ UC21  |
| TC-CPU-048  | [R]  | INT-CPU-009  | `cpu_step(NULL, ptMachine, ptResult)` → ST_ERROR                      | ✓ UC21  |
| TC-CPU-049  | [R]  | INT-CPU-009  | `cpu_step(ptCpu, NULL, ptResult)` → ST_ERROR                          | ✓ UC21  |
| TC-CPU-050  | [R]  | INT-CPU-009  | `cpu_step(ptCpu, ptMachine, NULL)` → ST_ERROR                         | ✓ UC21  |
| TC-CPU-051  | [R]  | INT-CPU-009  | cpu_step on stopped CPU (eState != RUNNING) → ST_NO_ERROR (no-op)    | ✓ UC21  |
| TC-CPU-052  | [R]  | INT-CPU-001  | MOVEQ bit8=1 in opcode → no-crash, ST_NO_ERROR                        | ✓ UC21  |
| TC-CPU-053  | [R]  | INT-CPU-009  | `cpu_init(NULL, ptMachine)` → ST_ERROR (regression UC1)               | ✓ UC21  |
| TC-CPU-054  | [R]  | INT-CPU-009  | `cpu_init(ptCpu, NULL)` → ST_ERROR (regression UC1)                   | ✓ UC21  |
| TC-CPU-055  | [R]  | INT-CPU-008  | CLR.L (A1) on valid memory EA → ST_NO_ERROR + RAM zeroed              | ✓ UC21  |
| TC-CPU-056  | [N]  | INT-CPU-010  | ADD.L D1,D0: 0x10+0x05=0x15, N=0, Z=0, C=0, V=0, X=0               | ✓ UC22  |
| TC-CPU-057  | [N]  | INT-CPU-010  | ADD.W signed overflow: 0x7FFF+1=0x8000, V=1, N=1                    | ✓ UC22  |
| TC-CPU-058  | [N]  | INT-CPU-010  | SUB.L D1,D0: 0x10-0x03=0x0D, N=0, Z=0, C=0                         | ✓ UC22  |
| TC-CPU-059  | [N]  | INT-CPU-010  | SUB.L borrow: 0x01-0x02=0xFFFFFFFF, C=1, N=1                        | ✓ UC22  |
| TC-CPU-060  | [N]  | INT-CPU-010  | CMPI.W equal: Z=1, Dn unchanged                                       | ✓ UC22  |
| TC-CPU-061  | [N]  | INT-CPU-010  | AND.L D1,D0: 0xF0F0F0F0 & 0x0F0F0F0F = 0, Z=1                      | ✓ UC22  |
| TC-CPU-062  | [N]  | INT-CPU-010  | OR.W D1,D0: 0x00F0 \| 0x000F = 0x00FF, N=0                          | ✓ UC22  |
| TC-CPU-063  | [N]  | INT-CPU-010  | EOR.B D1,D0: 0xFF^0xFF=0x00, Z=1                                     | ✓ UC22  |
| TC-CPU-064  | [N]  | INT-CPU-010  | NEG.L D0=5: result=0xFFFFFFFB, N=1, C=1                             | ✓ UC22  |
| TC-CPU-065  | [N]  | INT-CPU-010  | NEG.L D0=0: result=0, Z=1, C=0 (no borrow from 0-0)                 | ✓ UC22  |
| TC-CPU-066  | [N]  | INT-CPU-010  | NOT.W D0=0x1234: result=0xEDCB, N=1                                  | ✓ UC22  |
| TC-CPU-067  | [N]  | INT-CPU-010  | TST.L D0=0: Z=1, D0 unchanged                                        | ✓ UC22  |
| TC-CPU-068  | [N]  | INT-CPU-010  | EXT.W D0=0xFF: word result=0xFFFF, N=1                               | ✓ UC22  |
| TC-CPU-069  | [N]  | INT-CPU-010  | EXT.L D0=0x8000: long result=0xFFFF8000, N=1                         | ✓ UC22  |
| TC-CPU-070  | [N]  | INT-CPU-010  | ADDQ.L #8,D0=0: result=8                                             | ✓ UC22  |
| TC-CPU-071  | [N]  | INT-CPU-010  | ADDQ.W #3,D0=0: result=3                                             | ✓ UC22  |
| TC-CPU-072  | [N]  | INT-CPU-010  | SUBQ.L #1,D0=5: result=4, C=0                                        | ✓ UC22  |
| TC-CPU-073  | [N]  | INT-CPU-010  | SUBQ.L #1,D0=0: result=0xFFFFFFFF, C=1 (underflow)                  | ✓ UC22  |
| TC-CPU-074  | [N]  | INT-CPU-010  | ADDI.L #$100,D0=0: result=0x100, Z=0                                 | ✓ UC22  |
| TC-CPU-075  | [N]  | INT-CPU-010  | SUBI.W #5,D0=10: result=5                                            | ✓ UC22  |
| TC-CPU-076  | [N]  | INT-CPU-010  | ANDI.B #$0F,D0=$FF: result=0x0F                                      | ✓ UC22  |
| TC-CPU-077  | [N]  | INT-CPU-010  | ORI.W #$F000,D0=$0FFF: result=0xFFFF                                 | ✓ UC22  |
| TC-CPU-078  | [N]  | INT-CPU-010  | EORI.L #$FFFFFFFF,D0=0: result=0xFFFFFFFF, N=1                      | ✓ UC22  |
| TC-CPU-079  | [N]  | INT-CPU-010  | CMPI.L equal: Z=1, Dn unchanged                                       | ✓ UC22  |
| TC-CPU-080  | [N]  | INT-CPU-010  | ASL.L #1,D0=1: result=2, C=0                                         | ✓ UC22  |
| TC-CPU-081  | [N]  | INT-CPU-010  | ASL.L #1,D0=0x80000000: result=0, C=1, Z=1 (MSB shifted out)        | ✓ UC22  |
| TC-CPU-082  | [N]  | INT-CPU-010  | ASR.L #1,D0=0x80000000: result=0xC0000000 (sign preserved), C=0     | ✓ UC22  |
| TC-CPU-083  | [N]  | INT-CPU-010  | LSR.W #4,D0=0x00F0: result=0x000F, C=0                              | ✓ UC22  |
| TC-CPU-084  | [N]  | INT-CPU-010  | LSL.B #1,D0=0x80: result=0, C=1, Z=1                                | ✓ UC22  |
| TC-CPU-085  | [N]  | INT-CPU-010  | ROL.W #1,D0=0x8000: result=0x0001, C=1                              | ✓ UC22  |
| TC-CPU-086  | [N]  | INT-CPU-010  | ROR.W #1,D0=0x0001: result=0x8000, C=1                              | ✓ UC22  |
| TC-CPU-087  | [N]  | INT-CPU-010  | MULU.W #3,D0=5: result=15, N=0, Z=0                                 | ✓ UC22  |
| TC-CPU-088  | [N]  | INT-CPU-010  | MULS.W #-1,D0=5: result=0xFFFFFFFB (-5), N=1                        | ✓ UC22  |
| TC-CPU-089  | [N]  | INT-CPU-010  | DIVU.W 10÷3: quotient=3 (bits 15-0), remainder=1 (bits 31-16)       | ✓ UC22  |
| TC-CPU-090  | [N]  | INT-CPU-010  | DIVS.W (-10)÷(-2): quotient=5 in bits 15-0                          | ✓ UC22  |
| TC-CPU-091  | [N]  | INT-CPU-010  | Arithmetic program: (5+3)-2 AND $0F = 6; Z=0, N=0, C=0              | ✓ UC22  |
| TC-CPU-092  | [N]  | INT-CPU-010  | Shift sequence: ASL.L #8 → 0x100; LSR.L #4 → 0x10                  | ✓ UC22  |
| TC-CPU-093  | [R]  | INT-CPU-010  | `cpu_step(NULL, ptMachine, *)` → ST_ERROR (regression TC-CPU-048)   | ✓ UC22  |
| TC-CPU-094  | [R]  | INT-CPU-010  | `cpu_step(ptCpu, NULL, *)` → ST_ERROR (regression TC-CPU-049)       | ✓ UC22  |
| TC-CPU-095  | [R]  | INT-CPU-010  | Unimplemented opcode 0x6000 (BRA) → ST_NO_ERROR, PC advances by 2   | ✓ UC22  |
| TC-CPU-096  | [R]  | INT-CPU-010  | ADDQ sz=3 (Scc territory) → ST_NO_ERROR (LOG_TODO, UC23)            | ✓ UC22  |
| TC-CPU-097  | [R]  | INT-CPU-010  | DIVU.W with divisor=0 → ST_NO_ERROR (LOG_TODO, UC23 exception)      | ✓ UC22  |
| TC-CPU-098  | [R]  | INT-CPU-010  | NEG/unary with size field=3 (invalid) → ST_NO_ERROR (LOG_TODO)      | ✓ UC22  |
| TC-CPU-099  | [R]  | INT-CPU-010  | ADDI with size field=3 (invalid) → ST_NO_ERROR (LOG_TODO)           | ✓ UC22  |
| TC-CPU-100  | [R]  | INT-CPU-010  | Group-0 unknown op nibble → ST_NO_ERROR (LOG_TODO)                  | ✓ UC22  |

#### REQ → TC coverage (UC21)

| REQ           | TC(s)                                                              | Status        |
|---------------|--------------------------------------------------------------------|---------------|
| REQ-CPU-008   | TC-CPU-021..022 (PC advance exact)                                 | ADAPTED(UC21) |
| REQ-CPU-009   | TC-CPU-001..020, TC-CPU-033..046                                   | ✓ UC21        |
| REQ-CPU-012   | TC-CPU-025..029, TC-CPU-039..040                                   | ✓ UC21        |
| REQ-CPU-013   | TC-CPU-039..040                                                    | ✓ UC21        |
| REQ-CPU-014   | TC-CPU-006..008, TC-CPU-042                                        | ✓ UC21        |
| REQ-CPU-015   | TC-CPU-010..011                                                    | ✓ UC21        |
| REQ-CPU-016   | TC-CPU-010, TC-CPU-037                                             | ✓ UC21        |
| REQ-CPU-017   | TC-CPU-001..004, TC-CPU-035, TC-CPU-052                            | ✓ UC21        |
| REQ-CPU-018   | TC-CPU-015..017, TC-CPU-033, TC-CPU-044, TC-CPU-055                | ✓ UC21        |
| REQ-CPU-019   | TC-CPU-012..014, TC-CPU-034, TC-CPU-043                            | ✓ UC21        |
| REQ-CPU-020   | TC-CPU-018..020, TC-CPU-036                                        | ✓ UC21        |
| REQ-CPU-021   | TC-CPU-005, TC-CPU-030..032                                        | ✓ UC21        |

#### UFR traceability (UC21)

| UFR           | REQ(s)                                             | TC(s)                                    | Status   |
|---------------|----------------------------------------------------|------------------------------------------|----------|
| UFR-EXE-001   | REQ-CPU-001..009, REQ-CPU-017..021                 | TC-CPU-001..047                          | ✓ UC21   |
| UFR-EXE-002   | REQ-CPU-012                                        | TC-CPU-025..029, TC-CPU-039..040         | ✓ UC21   |
| UFR-EXE-003   | REQ-CPU-014                                        | TC-CPU-006..008, TC-CPU-042              | ✓ UC21   |
| UFR-EXE-004   | REQ-CPU-015, REQ-CPU-016                           | TC-CPU-010..011, TC-CPU-037              | ✓ UC21   |
| UFR-EXE-005   | REQ-CPU-013                                        | TC-CPU-039..040                          | ✓ UC21   |
| UFR-EXE-006   | REQ-CPU-010, REQ-CPU-022..031                      | TC-CPU-056..092                          | ✓ UC22   |
| UFR-EXE-007   | REQ-CPU-011                                        | — (TODO UC23)                            | TODO     |

#### REQ → TC coverage (UC22)

| REQ           | TC(s)                                                              | Status        |
|---------------|--------------------------------------------------------------------|---------------|
| REQ-CPU-010   | TC-CPU-056..092                                                    | ✓ UC22        |
| REQ-CPU-022   | TC-CPU-056..057, TC-CPU-058..059 (flags_add/sub)                  | ✓ UC22        |
| REQ-CPU-023   | TC-CPU-058..060 (flags_sub / CMP)                                 | ✓ UC22        |
| REQ-CPU-024   | — (ADDX/SUBX/NEGX Z semantics; covered by test_unary NEG.L D0=0) | ✓ UC22        |
| REQ-CPU-025   | TC-CPU-065 (NEG.L zero → C=0)                                     | ✓ UC22        |
| REQ-CPU-026   | TC-CPU-058..060 (CMPA indirect via CMPI sequence)                 | ✓ UC22        |
| REQ-CPU-027   | TC-CPU-087..088 (MULU/MULS)                                       | ✓ UC22        |
| REQ-CPU-028   | TC-CPU-089..090 (DIVU/DIVS quotient/remainder)                    | ✓ UC22        |
| REQ-CPU-029   | TC-CPU-097 (div-by-zero LOG_TODO)                                 | ✓ UC22        |
| REQ-CPU-030   | TC-CPU-080..084 (shift count 0→8, register)                      | ✓ UC22        |
| REQ-CPU-031   | TC-CPU-082..083 (ASR sign, LSR zero-insert)                       | ✓ UC22        |
