# ST4Ever — SRTD
## Software Requirements, Design and Tests Description

**Project:** ST4Ever — The Revival Engine for the Timeless ATARI ST
**Document:** SRTD.md — living document, updated at each validated Use Case
**Language:** English
**Companion:** CLAUDE.md (architecture decisions R1–R15, coding conventions, UC list)

---

## Revision History

| Rev | Date       | UC    | Author     | Changes                                                          |
|-----|------------|-------|------------|------------------------------------------------------------------|
| 1.0 | 2026-05-30 | UC1   | Claude/OMC | Initial baseline — UC1 validated, two-layer REQ model            |
| 1.1 | 2026-05-30 | UC2   | Claude/OMC | UC2 validated — trace on/off/toggle; UFR-CON-030..032 ✓          |
| 1.2 | 2026-05-30 | UC3.1 | Claude/OMC | UC3.1 validated — GUI msg_queue + Win32 window/thread/WndProc   |

---

## Document conventions

| Identifier             | Layer | Meaning                                                       |
|------------------------|-------|---------------------------------------------------------------|
| `UFR-[AREA]-NNN`       | 1     | **User/System Functional Requirement** — user-visible behaviour, implementation-agnostic |
| `REQ-[MOD]-NNN`        | 2     | **Software Requirement** — technical refinement of a UFR, API-level contract |
| `TC-[MOD]-NNN`         | —     | Test Case                                                     |
| `INT-[MOD]-NNN`        | —     | INTENT — unique ID for an `/* INTENT: */` block in a use case source file |
| `[N]` / `[R]` / `[S]` | —     | Nominal / Robustness / Skipped test type                      |
| `[ST4]`                | —     | ST4Ever internal function call                                |
| `[CRT]`                | —     | C runtime call (stdio.h, string.h, stdlib.h …)                |
| `[WIN]`                | —     | Win32 API call                                                |
| `[POX]`                | —     | POSIX API call                                                |
| `✓ UC1`                | —     | Implemented and all tests PASS in UC1                         |
| `ADAPTED(UCx)`         | —     | Stub assertion; will change when UCx implements real behaviour |
| `stub`                 | —     | Dispatched to `line_cmd_stub()` / LOG_TODO; not yet coded     |
| `TODO UCx`             | —     | Not yet started; planned for UCx                              |

**Traceability chain:**
```
UFR-CON-033  →  REQ-TRC-001  →  INT-TRC-001  →  TC-TRC-001
(user need)     (SW contract)   (test intent)   (executable assertion)
```

**Area codes** used in `UFR-*`: CON (console), TRC (trace), DIR (directory),
EDT (editor), MNT (mount/floppy), EXE (execution), FIL (file load), IMG (image).

**Module codes** used in `REQ-*`, `TC-*`, `INT-*`: TRC, CON, STM (ST machine),
CPU, DIS (disassembler). These follow the `.c` file granularity of Section 4.

---

## 1. System / User Functional Requirements

Requirements at this level describe ST4Ever behaviour **from the user's
perspective only**.  They are implementation-agnostic: no module names,
no API signatures, no internal data structures.  Each UFR will be verified
through one or more test cases in Section 5.

### 1.1 Console Interface — `CON`

| ID          | Requirement                                                                                       | Status   | UC   |
|-------------|---------------------------------------------------------------------------------------------------|----------|------|
| UFR-CON-001 | The application shall present an interactive console with a prompt (`ST4>`).                      | ✓ UC1    | UC1  |
| UFR-CON-002 | The console shall accept a text command line terminated by Enter and dispatch it immediately.      | ✓ UC1    | UC1  |
| UFR-CON-003 | The console shall remain active until the user issues the quit command or signals EOF (CTRL+Z).   | ✓ UC1    | UC1  |
| UFR-CON-004 | An unrecognised command shall display an informative error and keep the console active.           | ✓ UC1    | UC1  |
| UFR-CON-005 | Every command shall be invokable by its full name, its single-letter alias, or a CTRL shortcut.  | ✓ UC1    | UC1  |
| UFR-CON-006 | Arguments to unimplemented commands shall produce a "not yet implemented" message.                | ✓ UC1    | UC1  |
| UFR-CON-007 | Command history shall be navigable with the up/down arrow keys.                                   | TODO UC4 | UC4  |
| UFR-CON-008 | Home/End keys shall move the cursor to the start/end of the input line.                          | TODO UC4 | UC4  |
| UFR-CON-009 | Tab shall complete the current argument against file and directory names.                          | TODO UC4 | UC4  |

#### 1.1.1 `help` command (`h` | `help` | CTRL+H)

| ID          | Requirement                                                                                       | Status   | UC   |
|-------------|---------------------------------------------------------------------------------------------------|----------|------|
| UFR-CON-010 | `help` shall display the list of all available commands with a one-line summary for each.         | ✓ UC1    | UC1  |
| UFR-CON-011 | `help` shall ignore any arguments and warn the user if any are provided.                         | ✓ UC1    | UC1  |

#### 1.1.2 `quit` command (`q` | `quit` | CTRL+Q | CTRL+C)

| ID          | Requirement                                                                                       | Status   | UC   |
|-------------|---------------------------------------------------------------------------------------------------|----------|------|
| UFR-CON-020 | `quit` shall close all open views and exit the application cleanly.                               | ✓ UC1    | UC1  |
| UFR-CON-021 | `quit` shall ignore any arguments and warn the user if any are provided.                         | ✓ UC1    | UC1  |

#### 1.1.3 `trace` command (`t` | `trace` | CTRL+T)

| ID          | Requirement                                                                                       | Status   | UC   |
|-------------|---------------------------------------------------------------------------------------------------|----------|------|
| UFR-CON-030 | `trace on` shall open the trace console if not already visible.                                  | ✓ UC2    | UC2  |
| UFR-CON-031 | `trace off` shall close the trace console if currently visible.                                  | ✓ UC2    | UC2  |
| UFR-CON-032 | `trace` with no argument shall toggle the trace console visibility.                               | ✓ UC2    | UC2  |
| UFR-CON-033 | The `-t` application flag shall open the trace console immediately at startup.                   | ✓ UC1    | UC1  |

#### 1.1.4 Other commands (future UCs)

| ID          | Requirement                                                                                       | Status      | UC   |
|-------------|---------------------------------------------------------------------------------------------------|-------------|------|
| UFR-CON-040 | `dir [path]` shall open a file-tree view of the given (or current) directory.                    | TODO UC3.3  | UC3.3|
| UFR-CON-050 | `load <file>` shall load a file or binary into emulated ST memory.                               | TODO UC7    | UC7  |
| UFR-CON-060 | `edit <file>` shall open the appropriate editor view for the file type.                          | TODO UC8-10 | UC8  |
| UFR-CON-070 | `mount [path]` shall emulate an Atari ST floppy drive A:\ from the given directory.             | TODO UC18   | UC18 |
| UFR-CON-071 | `umount` shall eject the emulated floppy, offering to save a disk image if modified.             | TODO UC19   | UC19 |
| UFR-CON-080 | `where` shall display the current working directory and the currently selected file/directory.   | TODO UC5    | UC5  |
| UFR-CON-090 | `execute <file>` shall open the full execution engine with all linked views.                     | TODO UC25   | UC25 |
| UFR-CON-091 | `image` shall create a `.st` or `.msa` disk image from the mounted floppy content.              | TODO UC20   | UC20 |

### 1.2 Trace Console — `TRC`

| ID          | Requirement                                                                                                   | Status   | UC   |
|-------------|---------------------------------------------------------------------------------------------------------------|----------|------|
| UFR-TRC-001 | The trace console shall display log entries with colour coding by level (TRACE, INFO, ERROR, TODO).           | ✓ UC1    | UC1  |
| UFR-TRC-002 | Each entry shall include the emitting function name and source line number.                                   | ✓ UC1    | UC1  |
| UFR-TRC-003 | Repeated TRACE entries from the same function shall be compacted as a single `[xN]` counter entry.           | ✓ UC1    | UC1  |
| UFR-TRC-004 | All log entries shall be written to `st4ever_trace.log` regardless of console visibility.                    | ✓ UC1    | UC1  |
| UFR-TRC-005 | LOG_INFO, LOG_ERROR, and LOG_TODO shall always be emitted regardless of the TRACE filter.                    | ✓ UC1    | UC1  |
| UFR-TRC-006 | LOG_TRACE entries shall be suppressible independently from the other levels.                                  | ✓ UC1    | UC1  |
| UFR-TRC-007 | The trace console shall appear as a dedicated non-modal GUI window (Win32 / X11).                            | TODO UC3.3 | UC3.3|

### 1.3 GUI Framework — `GUI` (UC3.1 infra ✓, UC3.2 renderer, UC3.3 dir view)

| ID          | Requirement                                                                                                   | Status      | UC     |
|-------------|---------------------------------------------------------------------------------------------------------------|-------------|--------|
| UFR-GUI-001 | Each view command shall open a dedicated non-modal window running in its own thread.                          | ✓ UC3.1     | UC3.1  |
| UFR-GUI-002 | The console thread shall communicate with view threads via a bounded thread-safe message queue.               | ✓ UC3.1     | UC3.1  |
| UFR-GUI-003 | A view shall notify the console of user actions (file selected, close) via a registered event callback.       | ✓ UC3.1     | UC3.1  |
| UFR-GUI-004 | The application shall support up to 16 simultaneously open views.                                             | ✓ UC3.1     | UC3.1  |
| UFR-GUI-005 | All 2D rendering (text, rectangles, lines) shall use Direct2D (Windows) or X11/XRender (Linux).              | TODO UC3.2  | UC3.2  |

### 1.4 Directory View — `DIR` (TODO UC3.3)

| ID          | Requirement                                                                                                   | Status   | UC   |
|-------------|---------------------------------------------------------------------------------------------------------------|----------|------|
| UFR-DIR-001 | The directory view shall display the file tree of the target path, indented and expandable.                  | TODO UC3 | UC3  |
| UFR-DIR-002 | Left-clicking a file or pressing Space shall select it as the default argument for load, edit, image, where. | TODO UC3 | UC3  |
| UFR-DIR-003 | A `+` / `-` control shall expand or collapse a directory node.                                               | TODO UC3 | UC3  |
| UFR-DIR-004 | A `..` entry at the top shall navigate to the parent directory.                                               | TODO UC3 | UC3  |
| UFR-DIR-005 | Right-clicking a file shall display a context menu with `load` and `edit` options.                           | TODO UC3 | UC3  |
| UFR-DIR-006 | Right-clicking a directory shall display a context menu with `mount` and `image` options.                    | TODO UC3 | UC3  |

### 1.5 Editor Views — `EDT` (TODO UC8–10)

*Requirements will be detailed when UC8–UC10 are planned.*

### 1.6 Floppy Emulation — `MNT` (TODO UC16–19)

*Requirements will be detailed when UC16–UC19 are planned.*

### 1.7 Execution Engine — `EXE` (TODO UC21–27)

*Requirements will be detailed when UC21–UC27 are planned.*

---

## 2. Software Requirements

Software Requirements are technical refinements of Section 1 UFRs.  They are
formulated at API level: function contracts, return codes, data invariants.
Each REQ traces back to a parent UFR.  Where a component is not yet
user-visible (STM, CPU, DIS in UC1), the parent UFR is the execution-engine
requirement that will expose it (`UFR-EXE-*`, planned UC21–27).

### 2.1 Trace Subsystem — `trace.h` / `trace.c`

> Design ref: CLAUDE.md §1.1.10, §4.7, §6.1

| ID          | Software Requirement                                                                                               | Parent UFR               | Status        | UC   |
|-------------|--------------------------------------------------------------------------------------------------------------------|--------------------------|---------------|------|
| REQ-TRC-001 | `trace_init(bOpen)` shall open the log file and optionally show the console. Returns `ST_NO_ERROR`.                | UFR-TRC-001, UFR-CON-033 | ✓ UC1         | UC1  |
| REQ-TRC-002 | `trace_init()` called when already initialised shall return `ST_NO_ERROR` (idempotent; warning to stderr only).    | UFR-TRC-001              | ✓ UC1         | UC1  |
| REQ-TRC-003 | `trace_is_open()` shall return `ST_TRUE` immediately after `trace_init(ST_TRUE)`.                                 | UFR-TRC-001              | ✓ UC1         | UC1  |
| REQ-TRC-004 | `LOG_TRACE`, `LOG_INFO`, `LOG_ERROR`, `LOG_TODO` shall each emit one entry without crashing.                       | UFR-TRC-001, UFR-TRC-005 | ✓ UC1         | UC1  |
| REQ-TRC-005 | Consecutive `LOG_TRACE` calls from the same function shall be compacted to a single `[xN]` entry.                  | UFR-TRC-003              | ✓ UC1         | UC1  |
| REQ-TRC-006 | `trace_close()` shall return `ST_NO_ERROR` and set `trace_is_open()` to `ST_FALSE`.                               | UFR-CON-031              | ✓ UC1         | UC1  |
| REQ-TRC-007 | `trace_close()` called when already closed shall return `ST_NO_ERROR` (idempotent).                               | UFR-CON-031              | ✓ UC1         | UC1  |
| REQ-TRC-008 | `trace_open()` shall return `ST_NO_ERROR` and set `trace_is_open()` to `ST_TRUE`.                                 | UFR-CON-030              | ✓ UC1         | UC1  |
| REQ-TRC-009 | `trace_set_trace_enabled(ST_FALSE)` shall suppress `LOG_TRACE` only; INFO/ERROR/TODO unaffected.                   | UFR-TRC-006              | ✓ UC1         | UC1  |
| REQ-TRC-010 | `trace_set_trace_enabled(ST_TRUE)` shall re-activate `LOG_TRACE` output immediately.                              | UFR-TRC-006              | ✓ UC1         | UC1  |
| REQ-TRC-011 | `trace_is_trace_enabled()` shall reflect the current filter state accurately.                                     | UFR-TRC-006              | ✓ UC1         | UC1  |
| REQ-TRC-012 | `trace_shutdown()` shall flush pending entries, close the log file, and free all resources.                       | UFR-TRC-004              | ✓ UC1         | UC1  |
| REQ-TRC-013 | Trace output shall be written to `st4ever_trace.log` (always) and to stderr with ANSI colours (when open).        | UFR-TRC-004, UFR-TRC-001 | ✓ UC1         | UC1  |
| REQ-TRC-014 | `trace_open/close` shall call `gui_open_window` / close for the dedicated trace GUI window.                       | UFR-TRC-007              | TODO UC3      | UC3  |

### 2.2 Console Line Reader — `line.h` / `line.c`

> Design ref: CLAUDE.md §1.1.1–1.1.12, §5 R5

| ID           | Software Requirement                                                                                    | Parent UFR               | Status   | UC   |
|--------------|---------------------------------------------------------------------------------------------------------|--------------------------|----------|------|
| REQ-CON-001  | `line_init(NULL)` shall return `ST_ERROR` without modifying any state.                                 | UFR-CON-001              | ✓ UC1    | UC1  |
| REQ-CON-002  | `line_init(ptCtx)` shall set `ptCtx->bRunning = ST_TRUE`.                                              | UFR-CON-001              | ✓ UC1    | UC1  |
| REQ-CON-003  | `line_init(ptCtx)` shall populate `ptCtx->szCwd` with the current working directory (non-empty).       | UFR-CON-001              | ✓ UC1    | UC1  |
| REQ-CON-004  | `line_shutdown(NULL)` shall return `ST_ERROR` without modifying any state.                             | UFR-CON-001              | ✓ UC1    | UC1  |
| REQ-CON-005  | `line_shutdown(ptCtx)` shall set `ptCtx->bRunning = ST_FALSE`.                                        | UFR-CON-001              | ✓ UC1    | UC1  |
| REQ-CON-006  | `line_run()` shall dispatch `help` / `h` / CTRL+H to the help handler.                                | UFR-CON-010              | ✓ UC1    | UC1  |
| REQ-CON-007  | `line_run()` shall dispatch `quit` / `q` / CTRL+Q / CTRL+C to the quit handler and exit the loop.    | UFR-CON-020              | ✓ UC1    | UC1  |
| REQ-CON-008  | `line_run()` shall dispatch `trace on/off/toggle` to the trace command handler.                       | UFR-CON-030..032         | ✓ UC2    | UC2  |
| REQ-CON-009  | All unimplemented commands shall dispatch to `line_cmd_stub()` and emit `LOG_TODO`.                    | UFR-CON-006              | ✓ UC1    | UC1  |
| REQ-CON-010  | `line_run()` shall support history (↑/↓), Home/End, and tab-completion via the rich line editor.      | UFR-CON-007..009         | TODO UC4 | UC4  |

### 2.3 ST Machine Emulator — `ST.h` / `ST.c`

> Design ref: CLAUDE.md §1 memory map, §5 R6, R9
> Parent UFR: none at UC1 level — will link to `UFR-EXE-*` when UC25 exposes execution.

| ID           | Software Requirement                                                                                    | Parent UFR    | Status        | UC    |
|--------------|---------------------------------------------------------------------------------------------------------|---------------|---------------|-------|
| REQ-STM-001  | `st_init(NULL, *)` shall return `ST_ERROR`.                                                             | — (see UC25)  | ✓ UC1         | UC1   |
| REQ-STM-002  | `st_init(ptMachine, *)` shall set `ptMachine->bPoweredOn = ST_TRUE`.                                   | — (see UC25)  | ✓ UC1         | UC1   |
| REQ-STM-003  | `st_init()` shall initialise the video resolution to `ST_RES_LOW`.                                     | — (see UC25)  | ✓ UC1         | UC1   |
| REQ-STM-004  | `st_write_byte(NULL,…)` and `st_read_byte(NULL,…)` shall return `ST_ERROR`.                            | — (see UC25)  | ✓ UC1         | UC1   |
| REQ-STM-005  | `st_read_byte(…, NULL)` shall return `ST_ERROR`.                                                       | — (see UC25)  | ✓ UC1         | UC1   |
| REQ-STM-006  | Byte read/write at a valid RAM address shall produce an exact round-trip.                               | — (see UC25)  | ✓ UC1         | UC1   |
| REQ-STM-007  | Word read/write shall preserve 68000 big-endian byte order.                                            | — (see UC25)  | ✓ UC1         | UC1   |
| REQ-STM-008  | Long read/write shall preserve 68000 big-endian byte order.                                            | — (see UC25)  | ✓ UC1         | UC1   |
| REQ-STM-009  | Word access to an odd address shall return `ST_ERROR` (68000 address error).                           | — (see UC25)  | ✓ UC1         | UC1   |
| REQ-STM-010  | Long access to an odd address shall return `ST_ERROR` (68000 address error).                           | — (see UC25)  | ✓ UC1         | UC1   |
| REQ-STM-011  | Access to address ≥ `ST_RAM_SIZE` shall not crash. Stub returns `ST_NO_ERROR + 0xFF`.                 | — (see UC24)  | ADAPTED(UC24) | UC24  |
| REQ-STM-012  | Access to unmapped address regions shall return `ST_ERROR` (bus error).                                | — (see UC24)  | TODO UC24     | UC24  |
| REQ-STM-013  | `st_shutdown(NULL)` shall return `ST_ERROR`.                                                           | — (see UC25)  | ✓ UC1         | UC1   |
| REQ-STM-014  | `st_shutdown(ptMachine)` shall set `ptMachine->bPoweredOn = ST_FALSE`.                                 | — (see UC25)  | ✓ UC1         | UC1   |

### 2.4 CPU 68000 Emulator — `CPU.h` / `CPU.c`

> Design ref: CLAUDE.md §5 R2; MC68000 Programmer's Reference Manual
> Parent UFR: none at UC1 level — will link to `UFR-EXE-*` when UC21–25 expose execution.

| ID          | Software Requirement                                                                                   | Parent UFR    | Status        | UC    |
|-------------|--------------------------------------------------------------------------------------------------------|---------------|---------------|-------|
| REQ-CPU-001 | `cpu_init(NULL, *)` and `cpu_init(*, NULL)` shall return `ST_ERROR`.                                  | — (see UC25)  | ✓ UC1         | UC1   |
| REQ-CPU-002 | `cpu_init()` shall read the initial SSP from the reset vector at address `0x000000`.                   | — (see UC25)  | ✓ UC1         | UC1   |
| REQ-CPU-003 | `cpu_init()` shall read the initial PC from the reset vector at address `0x000004`.                    | — (see UC25)  | ✓ UC1         | UC1   |
| REQ-CPU-004 | `cpu_init()` shall set the SR supervisor bit (`CPU_SR_S`) and interrupt mask to 7 (`SR = 0x2700`).    | — (see UC25)  | ✓ UC1         | UC1   |
| REQ-CPU-005 | `cpu_init()` shall set `eState = CPU_STATE_RUNNING`.                                                   | — (see UC25)  | ✓ UC1         | UC1   |
| REQ-CPU-006 | `cpu_step(NULL, *, *)` and `cpu_step(*, NULL, *)` shall return `ST_ERROR`.                             | — (see UC25)  | ✓ UC1         | UC1   |
| REQ-CPU-007 | `cpu_step()` shall fetch the opcode word at the current PC and return it in `ptResult->uiOpcode`.     | — (see UC25)  | ✓ UC1         | UC1   |
| REQ-CPU-008 | Stub: `cpu_step()` shall advance PC by 2 per call. Full decode implemented in UC21.                   | — (see UC21)  | ADAPTED(UC21) | UC21  |
| REQ-CPU-009 | `cpu_step()` shall decode and execute MOVE / MOVEQ / LEA / CLR / SWAP.                                | — (see UC21)  | TODO UC21     | UC21  |
| REQ-CPU-010 | `cpu_step()` shall decode and execute ADD / SUB / CMP / AND / OR / EOR / shifts.                      | — (see UC22)  | TODO UC22     | UC22  |
| REQ-CPU-011 | `cpu_step()` shall decode and execute BRA / Bcc / JSR / RTS / TRAP + exception vectors.               | — (see UC23)  | TODO UC23     | UC23  |

### 2.5 GUI Framework — `gui.h` / `gui.c` / `win_gui.c`

> Design ref: CLAUDE.md §5 R4, §6.3; gui_backend.h (internal interface)

| ID           | Software Requirement                                                                                          | Parent UFR   | Status   | UC     |
|--------------|---------------------------------------------------------------------------------------------------------------|--------------|----------|--------|
| REQ-GUI-001  | `gui_init()` shall register the Win32 WNDCLASS "ST4EverView" (or open X11 display on Linux).                 | UFR-GUI-001  | ✓ UC3.1  | UC3.1  |
| REQ-GUI-002  | `gui_init()` called when already initialised shall return `ST_NO_ERROR` (idempotent).                         | UFR-GUI-001  | ✓ UC3.1  | UC3.1  |
| REQ-GUI-003  | `gui_open_window(NULL, *)` and `gui_open_window(*, NULL)` shall return `ST_ERROR`.                            | UFR-GUI-001  | ✓ UC3.1  | UC3.1  |
| REQ-GUI-004  | `gui_open_window()` shall spawn a dedicated thread and return only after the HWND is live.                    | UFR-GUI-001  | ✓ UC3.1  | UC3.1  |
| REQ-GUI-005  | `gui_close_window(NULL)` shall return `ST_ERROR`.                                                             | UFR-GUI-001  | ✓ UC3.1  | UC3.1  |
| REQ-GUI-006  | `gui_close_window()` shall post WM_CLOSE and join the window thread before returning.                         | UFR-GUI-001  | ✓ UC3.1  | UC3.1  |
| REQ-GUI-007  | `gui_shutdown()` without prior `gui_init()` shall return `ST_NO_ERROR` (safe no-op).                         | UFR-GUI-001  | ✓ UC3.1  | UC3.1  |
| REQ-GUI-008  | `gui_shutdown()` shall close all open windows before releasing platform resources.                            | UFR-GUI-001  | ✓ UC3.1  | UC3.1  |
| REQ-GUI-009  | `gui_msg_create(NULL, *)` shall return `ST_ERROR`.                                                            | UFR-GUI-002  | ✓ UC3.1  | UC3.1  |
| REQ-GUI-010  | `gui_msg_create(*, 0)` shall return `ST_ERROR` (zero capacity rejected).                                      | UFR-GUI-002  | ✓ UC3.1  | UC3.1  |
| REQ-GUI-011  | `gui_msg_post()` shall preserve events in strict FIFO order.                                                  | UFR-GUI-002  | ✓ UC3.1  | UC3.1  |
| REQ-GUI-012  | `gui_msg_post()` to a full queue shall return `ST_ERROR` without blocking.                                    | UFR-GUI-002  | ✓ UC3.1  | UC3.1  |
| REQ-GUI-013  | `gui_msg_get(bBlock=ST_FALSE)` on an empty queue shall return `ST_ERROR` immediately.                         | UFR-GUI-002  | ✓ UC3.1  | UC3.1  |
| REQ-GUI-014  | `gui_msg_destroy()` shall free all resources and set the handle to NULL.                                      | UFR-GUI-002  | ✓ UC3.1  | UC3.1  |
| REQ-GUI-015  | The WndProc shall translate WM_PAINT / WM_KEYDOWN / WM_CHAR / WM_MOUSE* / WM_MOUSEWHEEL / WM_SIZE → gui_event_t. | UFR-GUI-003 | ✓ UC3.1 | UC3.1 |
| REQ-GUI-016  | The renderer shall use Direct2D / DirectWrite (Windows) or X11/XRender (Linux).                              | UFR-GUI-005  | TODO UC3.2| UC3.2 |

### 2.6 Disassembler — `disassemble.h` / `disassemble.c`

> Design ref: CLAUDE.md §5 R7; DEVPAC3 syntax reference
> Parent UFR: none at UC1 level — will link to `UFR-EDT-*` (UC10) and `UFR-EXE-*` (UC25).

| ID          | Software Requirement                                                                                     | Parent UFR    | Status        | UC    |
|-------------|----------------------------------------------------------------------------------------------------------|---------------|---------------|-------|
| REQ-DIS-001 | `disasm_range(NULL buf, …)` shall return `ST_ERROR`.                                                    | — (see UC10)  | ✓ UC1         | UC1   |
| REQ-DIS-002 | `disasm_range(…, NULL ptResults, …)` shall return `ST_ERROR`.                                            | — (see UC10)  | ✓ UC1         | UC1   |
| REQ-DIS-003 | `disasm_range(…, NULL puiLines)` shall return `ST_ERROR`.                                               | — (see UC10)  | ✓ UC1         | UC1   |
| REQ-DIS-004 | `disasm_range()` with `uiBufLen = 0` shall return `ST_NO_ERROR` and write 0 lines.                     | — (see UC10)  | ✓ UC1         | UC1   |
| REQ-DIS-005 | Stub: any opcode shall produce a `DC.W $XXXX` fallback (`bValid = ST_FALSE`).                          | — (see UC11)  | ADAPTED(UC11) | UC11  |
| REQ-DIS-006 | Shall decode MOVE / MOVEQ / LEA / PEA / CLR / EXG / SWAP to DEVPAC3 format.                           | — (see UC11)  | TODO UC11     | UC11  |
| REQ-DIS-007 | Shall decode ADD / SUB / CMP / MULU / DIVS / AND / OR / EOR / NOT / NEG.                              | — (see UC12)  | TODO UC12     | UC12  |
| REQ-DIS-008 | Shall decode ASL/ASR/LSL/LSR/ROL/ROR/ROXL/ROXR / BTST/BSET/BCLR/BCHG.                                | — (see UC13)  | TODO UC13     | UC13  |
| REQ-DIS-009 | Shall decode BRA / BSR / Bcc / JMP / JSR / RTS / RTR / RTE / TRAP / ILLEGAL.                          | — (see UC14)  | TODO UC14     | UC14  |

---

## 3. Functional Architecture

### 3.1 System-level block diagram

```
 ┌─────────────────────────────────────────────────────────────────────┐
 │                        User (keyboard / display)                    │
 └────────────────────────────┬───────────────────────────────────────┘
                               │ commands / responses
 ┌─────────────────────────────▼───────────────────────────────────────┐
 │                   Console Interface                                 │
 │            (prompt, input loop, command dispatch)                   │
 └──────┬──────────────────┬──────────────────────────────────────────┘
        │ control           │ open view
        ▼                   ▼
 ┌──────────────┐   ┌───────────────────────────────────────────────┐
 │    Trace     │   │               GUI Framework                   │
 │  Subsystem   │   │  dir view │ text editor │ hex editor │ …     │
 │              │   │  mount view │ exec views │ trace window       │
 └──────────────┘   └─────────────────────┬─────────────────────────┘
                                          │ read/write
 ┌────────────────────────────────────────▼─────────────────────────┐
 │                    ST Machine Emulator                            │
 │         RAM (512 KB) │ ROM │ Hardware registers                   │
 │         (Shifter/video, YM2149, MFP, ACIA, Blitter)              │
 └───────────────────────┬───────────────────┬────────────────────┘
                         │ fetch/exec         │ disassemble / assemble
               ┌─────────▼──────────┐  ┌─────▼─────────────────────┐
               │   CPU 68000        │  │  Disassembler │ Assembler  │
               │  (fetch/decode/    │  │  (68k→DEVPAC3 │ DEVPAC3→   │
               │   execute/SR)      │  │   text)       │  binary)   │
               └────────────────────┘  └───────────────────────────┘

 ┌─────────────────────────────────────────────────────────────────┐
 │                  Platform Abstraction Layer                     │
 │  Windows: win_console.c  win_gui.c  win_D2D.c  win_platform.c  │
 │  Linux:   lx_console.c   lx_gui.c   lx_X11.c   lx_platform.c  │
 └─────────────────────────────────────────────────────────────────┘
```

### 3.2 Subsystem interface table

| From subsystem       | To subsystem          | Interface / data exchanged                            | Status   |
|----------------------|-----------------------|-------------------------------------------------------|----------|
| Console Interface    | Trace Subsystem       | `trace_open/close()`, `LOG_*` macros                  | ✓ UC1    |
| Console Interface    | GUI Framework         | `gui_open_window(desc)` per view command              | TODO UC3.3|
| GUI Framework        | ST Machine Emulator   | `st_read/write_byte/word/long()`                      | TODO UC25 |
| GUI Framework        | Disassembler          | `disasm_range()` for hex+asm view sync                | TODO UC10 |
| ST Machine Emulator  | CPU 68000             | `cpu_init()`, `cpu_step()`, shared `st_machine_t*`    | TODO UC21 |
| Console Interface    | Platform Layer        | `win_console_init()` at startup                       | ✓ UC1     |
| GUI Framework        | Platform Layer        | `gui_open_window()` → `gui_platform_window_create()`  | ✓ UC3.1   |
| GUI Framework        | Renderer              | `renderer_begin/end_draw()`, draw primitives          | TODO UC3.2|

### 3.3 Threading model (CLAUDE.md R4)

```
 Main thread (console)           View thread (one per open window)
      │                                    │
  line_run()              gui_open_window() → platform_thread_create()
      │                                    │
      │    ┌──────────────────────┐        │
      ├───►│   gui_msg_queue_t    │───────►│  event loop: WndProc / XNextEvent
      │    │  (mutex-protected,   │        │
      │    │   bounded circular)  │        │
      │◄───│                      │◄───────┤  gui_event_fn callback
      │    └──────────────────────┘        │  (file selected, breakpoint …)
```

---

## 4. Detailed Component Architecture

Each `.c` file is documented as an atomic component.  Notation:

| Tag     | Meaning                                     |
|---------|---------------------------------------------|
| `[ST4]` | Call to another ST4Ever function            |
| `[CRT]` | C runtime call (stdio, string, stdlib …)    |
| `[WIN]` | Win32 API call                              |
| `[POX]` | POSIX API call                              |

---

### 4.1 Application Entry — `main.c`

**Role:** parse CLI options, initialise subsystems in order, run the console
loop, shut down in reverse order.

**Initialisation sequence:**

```
main(argc, argv)
  ├─ parse -t / -h flags                           [CRT]
  ├─ win_console_init()         [ST4]  ANSI + UTF-8 on Win32
  ├─ trace_init(bOpen)          [ST4]  open log; optionally show console
  ├─ gui_init()                 [ST4]  register Win32 class (stub UC3)
  ├─ line_init(&tCtx)           [ST4]  capture cwd, bRunning=TRUE
  ├─ line_run(&tCtx)            [ST4]  blocking loop
  ├─ line_shutdown(&tCtx)       [ST4]
  ├─ gui_shutdown()             [ST4]  (stub UC3)
  └─ trace_shutdown()           [ST4]  flush + close log
```

| External call      | Tag   | Purpose          |
|--------------------|-------|------------------|
| `fprintf(stderr,…)`| [CRT] | `-h` usage text  |

---

### 4.2 Trace Subsystem — `trace.c`

**Role:** logging to `st4ever_trace.log` and optionally stderr with ANSI
colour. Four levels: TRACE (compacted), INFO, ERROR, TODO.

**Public API:**

| Function                             | Description                                    |
|--------------------------------------|------------------------------------------------|
| `trace_init(bOpen)`                  | Init log file; optional console open           |
| `trace_open()`                       | Show trace console (stderr for UC1)            |
| `trace_close()`                      | Hide trace console (idempotent)                |
| `trace_set_trace_enabled(b)`         | Suppress / re-enable LOG_TRACE only            |
| `trace_is_trace_enabled()`           | Query LOG_TRACE filter state                   |
| `trace_is_open()`                    | Query console visibility                       |
| `trace_log(level, func, line, fmt,…)`| Emit one entry (use LOG_* macros instead)      |
| `trace_shutdown()`                   | Flush, close file, free state                  |

**Key internal functions:**

| Function            | Description                                                          |
|---------------------|----------------------------------------------------------------------|
| `flush_compaction()`| Writes the pending `[xN]` entry before any non-TRACE line           |
| `format_entry(…)`   | Builds the timestamped, level-tagged text line                       |
| `write_to_file(…)`  | `fprintf` to the log file handle `[CRT]`                            |
| `write_to_stderr(…)`| ANSI-coloured write to stderr `[CRT]` (when console open, UC1 only) |

**External dependencies:**

| Call                     | Tag   | Purpose                              |
|--------------------------|-------|--------------------------------------|
| `fopen / fclose / fprintf`| [CRT]| Log file I/O                         |
| `vsnprintf`              | [CRT] | Format variadic args                 |
| `fprintf(stderr,…)`      | [CRT] | ANSI console output (UC1)            |
| `localtime / strftime`   | [CRT] | Timestamp in log entries             |

**TODO(UC3):** `trace_open/close` will call `gui_open_window` / close for the
dedicated trace GUI window.

**Compaction sequence:**

```
LOG_TRACE("msg A")  ← from function foo()
  └─ same function as last?  YES → g_trace_uiCount++
                             NO  → flush_compaction(); emit new entry

LOG_INFO("msg B")   ← from function bar()
  └─ flush_compaction() → write "[x3] foo …"
     then emit INFO "msg B"
```

---

### 4.3 Console Line Reader — `line.c`

**Role:** display prompt, read a line, tokenise it, dispatch to a command
handler.  Owns `line_context_t` (cwd, current selection, running flag).

**Public API:**

| Function                    | Description                                      |
|-----------------------------|--------------------------------------------------|
| `line_init(ptCtx)`          | Zero context, capture cwd, set bRunning=TRUE     |
| `line_run(ptCtx)`           | Blocking loop: prompt → read → dispatch          |
| `line_shutdown(ptCtx)`      | Clear context, set bRunning=FALSE                |
| `line_print_msg(fmt,…)`     | Normal response to stdout                        |
| `line_print_warning(fmt,…)` | Yellow warning to stdout                         |
| `line_print_error(fmt,…)`   | Red error to stdout                              |

**Key internal functions:**

| Function            | Description                                              |
|---------------------|----------------------------------------------------------|
| `parse_line()`      | Tokenise raw input into `parsed_cmd_t`                   |
| `match_command()`   | Map first token to `cmd_id_t` via `g_line_aCmds[]`      |
| `line_cmd_help()`   | Print command summary                                    |
| `line_cmd_quit()`   | Set `bRunning = FALSE`                                   |
| `line_cmd_trace()`  | Call `trace_open/close/toggle` (full impl UC2)           |
| `line_cmd_stub()`   | LOG_TODO + "not yet implemented" for all other commands  |

**External dependencies:**

| Call                    | Tag       | Purpose                                |
|-------------------------|-----------|----------------------------------------|
| `fgets`                 | [CRT]     | UC1 raw line read (replaced in UC4)    |
| `getcwd`                | [POX/CRT] | Capture working directory (Linux/MSYS2)|
| `GetCurrentDirectory`   | [WIN]     | Capture working directory (Win32)      |
| `printf / fprintf`      | [CRT]     | Console output                         |

**TODO(UC4):** `fgets` replaced by the rich line editor using
`ReadConsoleInput` (Windows) / `termios` raw mode (Linux).

**Command dispatch sequence:**

```
line_run()
  ├─ printf("ST4> ")         [CRT]
  ├─ fgets(szBuf)            [CRT]
  ├─ parse_line()     → parsed_cmd_t
  ├─ match_command()  → cmd_id_t
  ├─ CMD_HELP    → line_cmd_help()
  ├─ CMD_QUIT    → line_cmd_quit() → bRunning=FALSE → loop exits
  ├─ CMD_TRACE   → line_cmd_trace()
  └─ CMD_*       → line_cmd_stub()  LOG_TODO + warning
```

---

### 4.4 ST Machine Emulator — `ST.c`

**Role:** model the 24-bit Atari ST address space.  UC1 implements RAM
byte/word/long r/w with big-endian ordering and alignment checks.  Hardware
register access is stubbed (returns `0xFF` / no-op).

**Public API:**

| Function                                      | Description                                |
|-----------------------------------------------|--------------------------------------------|
| `st_init(ptMachine, szRomPath)`               | Zero RAM, set bPoweredOn, load ROM opt.    |
| `st_read_byte(ptMachine, addr, puiByte)`       | Read 1 byte from address space             |
| `st_read_word(ptMachine, addr, puiWord)`       | Read big-endian word (even addr only)      |
| `st_read_long(ptMachine, addr, puiLong)`       | Read big-endian long (even addr only)      |
| `st_write_byte(ptMachine, addr, val)`          | Write 1 byte                               |
| `st_write_word(ptMachine, addr, val)`          | Write big-endian word (even addr only)     |
| `st_write_long(ptMachine, addr, val)`          | Write big-endian long (even addr only)     |
| `st_shutdown(ptMachine)`                       | Zero state, set bPoweredOn=FALSE           |

**External dependencies:**

| Call                    | Tag   | Purpose                                   |
|-------------------------|-------|-------------------------------------------|
| `memset`                | [CRT] | Zero RAM on init                          |
| `fopen / fread / fclose`| [CRT] | ROM load (when szRomPath != NULL)         |
| `LOG_ERROR`             | [ST4] | Bus/alignment error reporting             |

**TODO(UC15):** PRG loading and fixup relocation.
**TODO(UC24):** hardware register r/w handlers (Shifter, MFP, YM2149).

**Address dispatch (design intent, UC24):**

```
st_read_byte(addr)
  ├─ addr < ST_RAM_SIZE          → aRam[addr]
  ├─ addr in ROM range           → aRom[addr - ROM_BASE]     (TODO UC24)
  ├─ addr in HW register range   → hw_register_handler()     (TODO UC24)
  └─ else                        → 0xFF, no bus error (stub)
```

---

### 4.5 CPU 68000 Emulator — `CPU.c`

**Role:** model the MC68000 register file.  UC1 stub: reads reset vectors,
sets supervisor mode SR=0x2700, and on each `cpu_step()` fetches the opcode
word and advances PC+2 without decoding.

**Public API:**

| Function                                          | Description                               |
|---------------------------------------------------|-------------------------------------------|
| `cpu_init(ptCpu, ptMachine)`                      | Read reset vectors, set SR=0x2700         |
| `cpu_reset(ptCpu, ptMachine)`                     | Re-read reset vectors                     |
| `cpu_step(ptCpu, ptMachine, ptResult)`            | Fetch + decode + execute one instruction  |
| `cpu_raise_exception(ptCpu, ptMachine, uiVector)` | Push exception frame, jump to vector      |

**External dependencies:**

| Call           | Tag   | Purpose                                      |
|----------------|-------|----------------------------------------------|
| `st_read_long` | [ST4] | Read SSP/PC reset vectors from ST machine    |
| `st_read_word` | [ST4] | Fetch opcode word at PC                      |
| `memset`       | [CRT] | Zero register file on init                   |

**TODO(UC21–23):** full opcode decode tables and execution handlers.

---

### 4.6 Disassembler — `disassemble.c`

**Role:** convert a byte buffer of 68000 opcodes to DEVPAC3-format text.
UC1 stub: every 2-byte word produces `DC.W $XXXX` (`bValid = ST_FALSE`).

**Public API:**

| Function                                                              | Description                            |
|-----------------------------------------------------------------------|----------------------------------------|
| `disasm_one(pBuf, uiBufLen, uiAddr, ptResult)`                        | Disassemble one instruction            |
| `disasm_range(pBuf, uiBufLen, uiAddr, ptResults, uiMax, puiLines)`    | Disassemble consecutive instructions   |

**External dependencies:**

| Call       | Tag   | Purpose                             |
|------------|-------|-------------------------------------|
| `snprintf` | [CRT] | Format `szLine` in `disasm_result_t`|
| `memset`   | [CRT] | Zero each `disasm_result_t`         |

**TODO(UC11–14):** instruction decode tables (MOVE/LEA/ADD/BRA …).

---

### 4.7 Platform: Console Init — `win_console.c` / `lx_console.c`

**`win_console.c`** — enables ANSI/VT100 on Win32 stdout/stderr; sets code
page UTF-8.  Failures are non-fatal (mintty has no real Win32 HANDLE).

| Call                         | Tag   | Purpose                                       |
|------------------------------|-------|-----------------------------------------------|
| `GetStdHandle`               | [WIN] | Obtain stdout / stderr HANDLE                 |
| `GetConsoleMode`             | [WIN] | Read current console mode flags               |
| `SetConsoleMode`             | [WIN] | Add `ENABLE_VIRTUAL_TERMINAL_PROCESSING`      |
| `SetConsoleOutputCP(65001)`  | [WIN] | UTF-8 output                                  |

**`lx_console.c`** — no-op for UC1; Linux terminals handle ANSI natively.
**TODO(UC4):** `termios` raw mode for the rich line editor.

---

### 4.8 Placeholder Components (stubs)

| Component                    | File(s)                                                             | Planned UC | Status   |
|------------------------------|---------------------------------------------------------------------|------------|----------|
| GUI Framework                | `gui.c`, `win_gui.c`, `lx_gui.c`, `gui_backend.h`                 | UC3.1      | ✓ UC3.1  |
| 2D Renderer                  | `renderer.c`, `win_D2D.c`, `lx_X11.c`                             | UC3.2      | stub     |
| Directory View               | `dir.c`                                                             | UC3.3      | stub     |
| Text Editor View             | `edit_txt.c`                                                        | UC8        |
| Hex/ASCII Editor View        | `edit_hex.c`                                                        | UC9        |
| Integrated Editor Dispatcher | `edit.c`                                                            | UC10       |
| Floppy Mount                 | `mount.c`                                                           | UC18       |
| Execution Engine             | `exec.c`, `exec_mon.c`, `exec_cpu.c`, `exec_mem.c`, `exec_screen.c`| UC25       |
| DEVPAC3 Assembler            | `as.c`                                                              | UC30       |
| Platform Thread/Mutex        | `win_platform.c`, `lx_platform.c`                                  | UC3.1      | ✓ UC3.1  |

---

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
| TC-DIS-001  | 4-byte buffer → 2 DC.W stub lines                         | [N]  | — (see UC10) | REQ-DIS-004, REQ-DIS-005 | INT-DIS-001 | `disasm_range(buf,4,…)` → `ST_NO_ERROR`; `n==2`; DC.W fallback ADAPTED(UC11) | PASS UC1 |
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
| UFR-TRC-007  | REQ-TRC-014                     | —                      | —                              | TODO UC3     |
| UFR-CON-040..091 | —                           | —                      | —                              | TODO UCx     |
| UFR-DIR-001..006 | —                           | —                      | —                              | TODO UC3     |

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
| REQ-TRC-014  | —                           | TODO UC3      |
| REQ-CON-001  | TC-CON-001                  | ✓ PASS        |
| REQ-CON-002  | TC-CON-002                  | ✓ PASS        |
| REQ-CON-003  | TC-CON-002                  | ✓ PASS        |
| REQ-CON-004  | TC-CON-003                  | ✓ PASS        |
| REQ-CON-005  | TC-CON-004                  | ✓ PASS        |
| REQ-CON-006  | (interactive — manual)      | ✓ PASS        |
| REQ-CON-007  | (interactive — manual)      | ✓ PASS        |
| REQ-CON-008  | —                           | stub → UC2    |
| REQ-CON-009  | (interactive — manual)      | ✓ PASS        |
| REQ-CON-010  | —                           | TODO UC4      |
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
| REQ-DIS-006  | —                           | TODO UC11     |
| REQ-DIS-007  | —                           | TODO UC12     |
| REQ-DIS-008  | —                           | TODO UC13     |
| REQ-DIS-009  | —                           | TODO UC14     |

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
| INT-TRC-012 | trace off: disable LOG_TRACE + close console (idempotent)                |
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
| TC-TRC-015  | `trace off`: disable + close (idempotent)       | [N]  | UFR-CON-031  | REQ-CON-008 | INT-TRC-012 | `trace_is_open()==FALSE`; `trace_is_trace_enabled()==FALSE`| PASS UC2|
| TC-TRC-016  | `trace off` when already closed: harmless       | [R]  | UFR-CON-031  | REQ-TRC-007 | INT-TRC-012 | second `trace_close()` → `ST_NO_ERROR`; state unchanged   | PASS UC2 |
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

#### UFR traceability update (UC2 + UC3.1)

| UFR              | REQ(s)           | TC(s)                              | Status     |
|------------------|------------------|------------------------------------|------------|
| UFR-CON-030      | REQ-CON-008      | TC-TRC-013, TC-TRC-014             | ✓ UC2      |
| UFR-CON-031      | REQ-CON-008      | TC-TRC-015, TC-TRC-016             | ✓ UC2      |
| UFR-CON-032      | REQ-CON-008      | TC-TRC-010..012                    | ✓ UC2      |
| UFR-GUI-001      | REQ-GUI-001..008 | TC-GUI-011..018                    | ✓ UC3.1    |
| UFR-GUI-002      | REQ-GUI-009..014 | TC-GUI-001..010                    | ✓ UC3.1    |
| UFR-GUI-003      | REQ-GUI-015      | TC-GUI-018 (manual)                | ✓ UC3.1    |
| UFR-GUI-004      | REQ-GUI-004..006 | TC-GUI-016..017 (manual)           | ✓ UC3.1    |
| UFR-GUI-005      | REQ-GUI-016      | —                                  | TODO UC3.2 |

#### Open items — cumulative

| Item              | TC / REQ                       | Target  | Nature                                              |
|-------------------|--------------------------------|---------|-----------------------------------------------------|
| STM bus error     | TC-STM-010, REQ-STM-011        | UC24    | Stub returns ST_NO_ERROR+0xFF; real map → ST_ERROR  |
| CPU decode        | TC-CPU-006, REQ-CPU-008        | UC21    | Stub: PC+2; real decode/execute to come             |
| Disasm DC.W       | TC-DIS-001, REQ-DIS-005        | UC11    | All opcodes → DC.W; full decode in UC11–UC14        |
| Trace GUI window  | UFR-TRC-007, REQ-TRC-014       | UC3.3   | stderr → dedicated Win32/X11 window                 |
| Line editor       | UFR-CON-007..009, REQ-CON-010  | UC4     | History, tab-completion, arrow navigation            |
| D2D renderer      | UFR-GUI-005, REQ-GUI-016       | UC3.2   | win_D2D.c (COM pure C) + renderer.c portable layer  |
| dir view          | UFR-DIR-001..004, UFR-CON-040  | UC3.3   | Tree scan + D2D rendering + navigation + selection   |
| Window manual TC  | TC-GUI-016..018                | UC4     | Will become TEST_MANUAL when make manual is ready   |
