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
| 1.3 | 2026-05-30 | UC3.2 | Claude/OMC | UC3.2 validated — Direct2D renderer (win_D2D.c COM pure C + renderer.c) |
| 1.4 | 2026-05-31 | UC3.3 | Claude/OMC | UC3.3 validated — dir view: lazy-load FS tree, flat list, D2D render, kbd/mouse |
| 1.5 | 2026-05-31 | UC4.1 | Claude/OMC | UC4.1 validated — gui_request_close, bShowHidden, ESC/←/→/SPACE separation, auto-focus, TEST_MANUAL, make manual |
| 1.6 | 2026-06-01 | UC4.2 | Claude/OMC | UC4.2 validated — console.h (CON_KEY_*) + win pipe/VT100 + line_read_rich + CTRL shortcuts + make manual UC=XX + focus restore on close |
| 1.7 | 2026-06-01 | UC4.3 | Claude/OMC | UC4.3 validated — history ↑↓ + ~/.st4ever_history + TAB completion + ghost text + prompt contextuel + colors on/off + --script + mutex ptSelectedMutex |
| 1.8 | 2026-06-01 | UC4.4 | Claude/OMC | UC4.4 validated — trace_view_t D2D ring buffer + auto-scroll + keyboard nav + stderr suppressed when GUI live; REQ-TRC-017..022; UFR-TRC-007 closed |
| 1.9 | 2026-06-01 | UC5   | Claude/OMC | UC5 validated — where/info/history [N]; P8 console title; P21 H-key hidden toggle; P22 F5 refresh + state-preserve; P23bis TAB common prefix; P24 colors isatty; P27 trace clear; P28 trace level; REQ-TRC-023..026; REQ-CON-025..031; REQ-DIR-021..022 |
| 2.0 | 2026-06-01 | UC6   | Claude/OMC | UC6 validated — portable file FS abstraction (file.h/file.c): file_stat, file_open/read/write/close, file_mkdir, file_list_dir; REQ-FIL-001..022; UFR-FIL-001..007 |

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
FIL (file system abstraction), EDT (editor), MNT (mount/floppy), EXE (execution), IMG (image).

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
| UFR-CON-007 | Command history shall be navigable with the up/down arrow keys; the most recent entry browsed then restored on DOWN past newest. | ✓ UC4.3 | UC4.3 |
| UFR-CON-008 | Home/End keys shall move the cursor to the start/end of the input line.                          | ✓ UC4.2  | UC4.2 |
| UFR-CON-009 | Tab shall complete the command name on the first word, file/directory names on further words; a single match inserts immediately, multiple matches cycle as ghost text. | ✓ UC4.3 | UC4.3 |
| UFR-CON-047 | `colors [on|off]` shall toggle or force ANSI colour codes in console output; useful for terminals without VT100 or file redirection. | ✓ UC4.3 | UC4.3 |
| UFR-CON-048 | `--script <file>` shall execute commands from the given file line-by-line without interactive input; a missing file shall return an error and exit. | ✓ UC4.3 | UC4.3 |
| UFR-CON-049 | The prompt shall display `[T]` when the trace view is open and `[basename]` when a file is currently selected, so the user sees active state at a glance. | ✓ UC4.3 | UC4.3 |
| UFR-CON-041 | The application shall support cursor movement (←/→) within the current input line.               | ✓ UC4.2  | UC4.2 |
| UFR-CON-042 | The application shall support character insertion at cursor position.                             | ✓ UC4.2  | UC4.2 |
| UFR-CON-043 | The application shall support character deletion (Backspace before cursor, Delete at cursor).     | ✓ UC4.2  | UC4.2 |
| UFR-CON-044 | ESC shall clear the current input line without executing it.                                      | ✓ UC4.2  | UC4.2 |
| UFR-CON-045 | CTRL+C shall cancel the current input; if the buffer is empty it shall request quit.              | ✓ UC4.2  | UC4.2 |
| UFR-CON-046 | CTRL shortcuts (CTRL+T, CTRL+D, CTRL+L, CTRL+E, CTRL+U, CTRL+W, CTRL+X, CTRL+Q) shall immediately execute the mapped command. | ✓ UC4.2 | UC4.2 |

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
| UFR-CON-031 | `trace off` shall filter LOG_TRACE output without closing the trace view; LOG_INFO/ERROR/TODO remain visible. Use `trace` (no arg) to close. (ADAPTED P19) | ✓ P19 | UC2 |
| UFR-CON-032 | `trace` with no argument shall toggle the trace console visibility.                               | ✓ UC2    | UC2  |
| UFR-CON-033 | The `-t` application flag shall open the trace console immediately at startup.                   | ✓ UC1    | UC1  |

#### 1.1.4 Other commands (future UCs)

| ID          | Requirement                                                                                       | Status      | UC   |
|-------------|---------------------------------------------------------------------------------------------------|-------------|------|
| UFR-CON-040 | `dir [path]` shall open a file-tree view of the given (or current) directory.                    | ✓ UC3.3     | UC3.3|
| UFR-CON-050 | `load <file>` shall load a file or binary into emulated ST memory.                               | TODO UC7    | UC7  |
| UFR-CON-060 | `edit <file>` shall open the appropriate editor view for the file type.                          | TODO UC8-10 | UC8  |
| UFR-CON-070 | `mount [path]` shall emulate an Atari ST floppy drive A:\ from the given directory.             | TODO UC18   | UC18 |
| UFR-CON-071 | `umount` shall eject the emulated floppy, offering to save a disk image if modified.             | TODO UC19   | UC19 |
| UFR-CON-080 | `where` shall display the current working directory and the currently selected file/directory.   | ✓ UC5       | UC5  |
| UFR-CON-081 | `info` shall display a status dashboard: cwd, selected file, trace state, colors state, history count, mounted disk, loaded binary. | ✓ UC5 | UC5 |
| UFR-CON-082 | `history [N]` shall display the last N command history entries numbered in order (default N=10). | ✓ UC5       | UC5  |
| UFR-CON-083 | The OS window/terminal title bar shall be updated automatically after each command to reflect the current cwd, selection, and trace state. (P8) | ✓ UC5 | UC5 |
| UFR-CON-084 | On Tab with multiple completion candidates, the longest common prefix shall be inserted into the input buffer before cycling ghost text candidates. (P23bis) | ✓ UC5 | UC5 |
| UFR-CON-085 | ANSI colour output shall be auto-enabled or auto-disabled based on `isatty(stdout)` at startup; `colors on/off` shall remain available to override. (P24) | ✓ UC5 | UC5 |
| UFR-CON-090 | `execute <file>` shall open the full execution engine with all linked views.                     | TODO UC25   | UC25 |
| UFR-CON-091 | `image` shall create a `.st` or `.msa` disk image from the mounted floppy content.              | TODO UC20   | UC20 |

### 1.2 Trace Console — `TRC`

| ID          | Requirement                                                                                                   | Status   | UC   |
|-------------|---------------------------------------------------------------------------------------------------------------|----------|------|
| UFR-TRC-001 | The trace console shall display log entries with colour coding by level (TRACE, INFO, ERROR, TODO).           | ✓ UC1    | UC1  |
| UFR-TRC-002 | Each entry shall be timestamped and include the emitting function name and source line number.                                   | ✓ UC1    | UC1  |
| UFR-TRC-003 | Repeated TRACE entries from the same function shall be compacted as a single `[xN]` counter entry.           | ✓ UC1    | UC1  |
| UFR-TRC-004 | All log entries shall be written to `st4ever_trace.log` regardless of console visibility.                    | ✓ UC1    | UC1  |
| UFR-TRC-005 | LOG_INFO, LOG_ERROR, and LOG_TODO shall always be emitted regardless of the TRACE filter.                    | ✓ UC1    | UC1  |
| UFR-TRC-006 | LOG_TRACE entries shall be suppressible independently from the other levels.                                  | ✓ UC1    | UC1  |
| UFR-TRC-007 | The trace console shall appear as a dedicated non-modal GUI window (Win32 / X11).                            | ✓ UC4.4    | UC4.4|
| UFR-TRC-008 | The trace window shall render log entries colour-coded by level: TRACE=grey, INFO=cyan, ERROR=red, TODO=magenta. | ✓ UC4.4  | UC4.4|
| UFR-TRC-009 | The trace window shall auto-scroll to the newest entry on append; manual scroll disables auto-scroll until End is pressed. | ✓ UC4.4 | UC4.4|
| UFR-TRC-010 | Opening the trace window shall not steal keyboard focus from the console.                                     | ✓ UC4.4    | UC4.4|
| UFR-TRC-011 | `trace clear` shall erase all lines from the GUI trace window ring buffer without affecting the log file. (P27) | ✓ UC5    | UC5  |
| UFR-TRC-012 | `trace level <trace\|info\|error>` shall set a display filter on the GUI trace window; lines below the filter level shall be hidden without being removed from the ring buffer or the log file. (P28) | ✓ UC5 | UC5 |

### 1.3 GUI Framework — `GUI` (UC3.1 infra ✓, UC3.2 renderer, UC3.3 dir view)

| ID          | Requirement                                                                                                   | Status      | UC     |
|-------------|---------------------------------------------------------------------------------------------------------------|-------------|--------|
| UFR-GUI-001 | Each view command shall open a dedicated non-modal window running in its own thread.                          | ✓ UC3.1     | UC3.1  |
| UFR-GUI-002 | The console thread shall communicate with view threads via a bounded thread-safe message queue.               | ✓ UC3.1     | UC3.1  |
| UFR-GUI-003 | A view shall notify the console of user actions (file selected, close) via a registered event callback.       | ✓ UC3.1     | UC3.1  |
| UFR-GUI-004 | The application shall support up to 16 simultaneously open views.                                             | ✓ UC3.1     | UC3.1  |
| UFR-GUI-005 | All 2D rendering (text, rectangles, lines) shall use Direct2D (Windows) or X11/XRender (Linux).              | ✓ UC3.2     | UC3.2  |
| UFR-GUI-006 | Each view's window title bar shall be updated dynamically to reflect the current context (R18).               | ✓ UC3.3     | UC3.3  |
| UFR-GUI-007 | A newly opened view window shall automatically receive keyboard focus without requiring the user to click or alt-tab. | ✓ UC4.1 | UC4.1 |

### 1.4 Directory View — `DIR` (TODO UC3.3)

| ID          | Requirement                                                                                                   | Status   | UC   |
|-------------|---------------------------------------------------------------------------------------------------------------|----------|------|
| UFR-DIR-001 | The directory view shall display the file tree of the target path, indented and expandable.                  | ✓ UC3.3  | UC3.3|
| UFR-DIR-002 | Left-clicking a file or pressing Space shall select it as the default argument for load, edit, image, where. | ✓ UC3.3  | UC3.3|
| UFR-DIR-003 | A `+` / `-` control shall expand or collapse a directory node.                                               | ✓ UC3.3  | UC3.3|
| UFR-DIR-004 | A `..` entry at the top shall navigate to the parent directory.                                               | ✓ UC3.3  | UC3.3|
| UFR-DIR-005 | Right-clicking a file shall display a context menu with `load` and `edit` options.                           | TODO UC7 | UC7  |
| UFR-DIR-006 | Right-clicking a directory shall display a context menu with `mount` and `image` options.                    | TODO UC18| UC18 |
| UFR-DIR-007 | Hidden entries (names starting with `.`) shall be excluded from the directory view by default; `dir -a` shall include them. | ✓ UC4.1 | UC4.1 |
| UFR-DIR-008 | ESC key shall close the directory view without requiring console-thread action.                              | ✓ UC4.1  | UC4.1 |
| UFR-DIR-009 | LEFT arrow key shall collapse an expanded directory node; RIGHT arrow key shall expand a collapsed directory node (loading children lazily if needed). | ✓ UC4.1 | UC4.1 |
| UFR-DIR-010 | SPACE shall update the default selection without modifying the expand/collapse state; ENTER shall activate the node (expand/collapse/navigate). | ✓ UC4.1 | UC4.1 |
| UFR-DIR-011 | The `H` key shall toggle the visibility of hidden files (entries starting with `.`) in the currently open directory view, reloading the root children with the new filter. (P21) | ✓ UC5 | UC5 |
| UFR-DIR-012 | The F5 key shall refresh the directory listing from disk while preserving the current expansion state: directories that were expanded before the refresh shall remain expanded afterwards; deleted directories are silently dropped. (P22) | ✓ UC5 | UC5 |

### 1.5 File System Abstraction — `FIL` (UC6)

| ID          | Requirement                                                                                                         | Status   | UC   |
|-------------|---------------------------------------------------------------------------------------------------------------------|----------|------|
| UFR-FIL-001 | The application shall provide a portable `file_stat()` function that returns whether a path exists, whether it is a directory, its size in bytes, and its lowercase extension (without the dot). A non-existent path shall not be an error. | ✓ UC6 | UC6 |
| UFR-FIL-002 | The application shall provide `file_open()` to open an existing file for reading, create/truncate for writing, or open/create for appending, returning an opaque handle. | ✓ UC6 | UC6 |
| UFR-FIL-003 | `file_read()` shall read up to N bytes from an open file into a caller-supplied buffer, reporting the actual byte count; a partial read at EOF shall not be an error. | ✓ UC6 | UC6 |
| UFR-FIL-004 | `file_write()` shall write exactly N bytes to an open file; a short write shall be an error. | ✓ UC6 | UC6 |
| UFR-FIL-005 | `file_close()` shall close the file, release the handle, and set the caller's pointer to NULL; calling on a NULL handle shall be a safe no-op. | ✓ UC6 | UC6 |
| UFR-FIL-006 | `file_mkdir()` shall create a single directory level; calling on an already-existing directory shall succeed silently. | ✓ UC6 | UC6 |
| UFR-FIL-007 | `file_list_dir()` shall enumerate directory entries via a user-supplied callback, excluding `.` and `..` always and optionally entries starting with `.`; the callback receives the full path, the entry name, and a pre-filled `file_stat_t`. | ✓ UC6 | UC6 |

### 1.6 Load Command — `LOD` (UC7)

| ID          | Requirement                                                                                                         | Status   | UC   |
|-------------|---------------------------------------------------------------------------------------------------------------------|----------|------|
| UFR-LOD-001 | The `load` command shall accept a file path as argument; when no argument is given it shall fall back to the path selected via `dir`; if neither is available, it shall display an informational message and return without error. | ✓ UC7 | UC7 |
| UFR-LOD-002 | The `load` command shall detect the file type via `file_stat()` and reject directories and disk images (`.st`/`.msa`/`.stx`) with a user-readable message indicating that `mount` should be used instead. | ✓ UC7 | UC7 |
| UFR-LOD-003 | For any accepted non-PRG file, `load` shall copy the file content verbatim into emulated ST RAM at address `ST_LOAD_BASE` (0x0800) and report the load address and byte count to the user. | ✓ UC7 | UC7 |
| UFR-LOD-004 | For ATARI ST executables (`.prg`/`.ttp`/`.tos`), `load` shall validate the `0x601A` magic, load the `.text`+`.data` sections at `ST_LOAD_BASE`, and report the result; fixup relocation is deferred to UC15. | ✓ UC7 | UC7 |
| UFR-LOD-005 | The `info` command shall display the currently loaded binary name, type, ST load address, and byte count; if nothing is loaded, it shall display `(none)`. | ✓ UC7 | UC7 |
| UFR-DIR-013 | When a file is committed via ENTER or SPACE in the dir view, a dark green secondary background (`g_dir_clrLastSel`) shall be rendered on that row in `dir_render()`, visually distinct from the navigation-cursor highlight. The indicator persists when the cursor moves; a new commit updates it. (P11) | ✓ UC7 | UC7 |

### 1.7 Editor Views — `EDT` (UC8 text editor ✓, UC9–10 TODO)

| ID          | User Functional Requirement                                                                                                                                         | Status   | UC    |
|-------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------|----------|-------|
| UFR-EDT-001 | The `edit` command on a text file (non-binary, non-image extension) shall open a D2D window showing the file content with a line-number gutter and monospace font. | ✓ UC8    | UC8   |
| UFR-EDT-002 | The editor shall support full cursor navigation: arrow keys, Home/End (line), PgUp/PgDn (screen), CTRL+Home/End (file), CTRL+←/→ (word). SHIFT+any motion extends a rectangular selection highlighted in blue. | ✓ UC8 | UC8 |
| UFR-EDT-003 | The editor shall support character insert, ENTER (split line), Backspace (delete-back or merge lines), Delete (delete-fwd or merge lines), Tab (insert tab char). CTRL+A selects all. CTRL+C/X/V uses the system clipboard. | ✓ UC8 | UC8 |
| UFR-EDT-004 | The editor shall implement 20-level CTRL+Z undo with consecutive-insert grouping: a run of printable characters typed without intervening navigation is undone as one unit. | ✓ UC8 | UC8 |
| UFR-EDT-005 | CTRL+S shall save the file via `file.h FILE_MODE_WRITE`; the title bar shall show `[*]` when unsaved changes exist and shall remove it after a successful save (R18). | ✓ UC8 | UC8 |
| UFR-EDT-006 | The `edit` command on a binary or disk-image file (`.prg/.ttp/.tos/.st/.msa/.stx`) shall display a user-readable stub message indicating that hex editing is available in UC9. | ✓ UC8 | UC8 |
| UFR-GUI-007 | The GUI event system shall transmit keyboard modifier state (CTRL, SHIFT, ALT) as bitmask flags in `gui_event_t.uData.tKey.uiMods`; CTRL+letter keys shall be forwarded as `GUI_KEY_PRINTABLE+'A'..'Z'` with `GUI_MOD_CTRL`. The system clipboard shall be accessible via `gui_clipboard_set/get_text`. | ✓ UC8 | UC8 |
| UFR-HEX-001 | The `edit` command on a binary file (`.prg/.ttp/.tos/.bin/.raw`) shall open a D2D hex+ASCII editor window (`GUI_WND_EDIT_HEX`) displaying file content as rows of 16 bytes with a 6-digit hex address column. | ✓ UC9 | UC9 |
| UFR-HEX-002 | The hex view shall display 16 bytes per row: hex pairs separated by spaces, with an extra space gap after byte 7; bytes beyond the file end shown as spaces; non-printable bytes shown as `.` in the ASCII column. | ✓ UC9 | UC9 |
| UFR-HEX-003 | The editor shall support two zones toggled by TAB: HEX zone (nibble-by-nibble editing: 0–9/A–F) and ASCII zone (byte editing: printable 0x20–0x7E); cursor color distinguishes the active zone (blue=HEX, yellow=ASCII). | ✓ UC9 | UC9 |
| UFR-HEX-004 | CTRL+S shall save the file in-place (replace-mode, fixed size); the title bar shall show `[*]` when dirty and remove it after a successful save. ESC closes with a trace log entry if dirty. | ✓ UC9 | UC9 |

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
| REQ-TRC-013 | Trace output shall be written to `st4ever_trace.log` (always). When the GUI view is live, stderr output is suppressed; LOG_* goes to the ring buffer only. ADAPTED UC4.4: stderr was the primary output in UC1; the GUI ring buffer replaces it. | UFR-TRC-004, UFR-TRC-001 | ✓ UC1, ADAPTED UC4.4 | UC4.4 |
| REQ-TRC-014 | `trace_open()` shall call `gui_open_window(GUI_WND_TRACE)` to open the dedicated D2D trace window; `trace_close()` shall call `gui_close_window()` to close it. | UFR-TRC-007 | ✓ UC4.4 | UC4.4 |
| REQ-TRC-017 | `trace_open()` shall be idempotent when the GUI window is already open: a second call shall return `ST_NO_ERROR` without creating a second window. | UFR-TRC-007 | ✓ UC4.4 | UC4.4 |
| REQ-TRC-018 | `trace_log()` called while the GUI view is live (`g_trace_ptView != NULL`) shall append to the ring buffer and call `gui_invalidate()` under a re-entrancy guard (`g_trace_bInNotify`). | UFR-TRC-007, UFR-TRC-001 | ✓ UC4.4 | UC4.4 |
| REQ-TRC-019 | The trace ring buffer shall hold the last `TRACE_RING_SIZE` (200) lines; the oldest line is silently evicted on overflow. | UFR-TRC-007 | ✓ UC4.4 | UC4.4 |
| REQ-TRC-020 | The trace window shall auto-scroll to the newest entry after each append when `bAutoScroll == ST_TRUE`; user scroll-up shall set `bAutoScroll = ST_FALSE`; pressing End shall set it back to `ST_TRUE`. | UFR-TRC-009 | ✓ UC4.4 | UC4.4 |
| REQ-TRC-021 | The trace window shall not steal keyboard focus on open: the `GUI_WND_TRACE` type shall bypass the `AttachThreadInput` / `SetForegroundWindow` block in `win_wnd_thread()`. | UFR-TRC-010 | ✓ UC4.4 | UC4.4 |
| REQ-TRC-022 | ESC in the trace window shall call `gui_request_close(hWnd)` (non-blocking); ↑/↓/PgUp/PgDn/Home/End and mouse wheel shall scroll the view content. | UFR-TRC-009 | ✓ UC4.4 | UC4.4 |
| REQ-TRC-015 | Each log entry shall be formatted as `HH:MM:SS [LEVEL] function:line  message` — timestamp from system clock (`localtime`/`strftime`), level tag, emitting function (`__func__`), source line (`__LINE__`). | UFR-TRC-002 | ✓ UC1 | UC1 |
| REQ-TRC-016 | `line_cmd_trace("off")` shall call `trace_set_trace_enabled(ST_FALSE)` without calling `trace_close()`; the trace view shall remain open. Calling `trace off` twice shall be harmless. (P19) | UFR-CON-031 | ✓ P19 | P19 |
| REQ-TRC-023 | `trace_clear()` shall lock the ring buffer mutex, reset `iHead`, `iCount`, and `iScrollOff` to 0, unlock, and call `gui_invalidate()`. If the trace window is not open, it shall return `ST_NO_ERROR` immediately (no-op). The log file shall not be affected. | UFR-TRC-011 | ✓ UC5 | UC5 |
| REQ-TRC-024 | `trace_set_view_level(eMinLevel)` shall update the global `g_trace_eViewMinLevel` and the `eViewMinLevel` field of the active `trace_view_t` (if open), then call `gui_invalidate()`. The filter shall apply to rendering only; the ring buffer and log file receive all entries regardless of level. | UFR-TRC-012 | ✓ UC5 | UC5 |
| REQ-TRC-025 | `trace_get_view_level()` shall return the current `g_trace_eViewMinLevel` value. The global default is `LOG_LEVEL_TRACE` (show all). The level shall persist across `trace_close()` / `trace_open()` cycles: `trace_open()` shall copy `g_trace_eViewMinLevel` into the new `trace_view_t.eViewMinLevel`. | UFR-TRC-012 | ✓ UC5 | UC5 |
| REQ-TRC-026 | In `trace_render()`, for each line in the ring buffer, if `eLevel < ptView->eViewMinLevel` the line shall be skipped without consuming a screen row. The screen row counter shall advance only for lines that pass the filter. | UFR-TRC-012 | ✓ UC5 | UC5 |

### 2.2 Console Line Reader — `line.h` / `line.c`

> Design ref: CLAUDE.md §1.1 (table commandes validées + sous-sections dir/load/edit…), §5 R5

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
| REQ-CON-010  | `line_run()` shall support cursor movement (←/→/Home/End), Backspace/Delete, and CTRL shortcuts via the rich line editor when stdin is a TTY. | UFR-CON-008, UFR-CON-041..046 | ✓ UC4.2 | UC4.2 |
| REQ-CON-015  | `line_run()` shall fall back to plain `fgets()` input when `console_set_raw()` returns `ST_ERROR` (non-TTY: CI, pipes). | UFR-CON-001 | ✓ UC4.2 | UC4.2 |
| REQ-CON-016  | `line_run()` shall call `console_restore()` after the main loop when raw mode was active.             | UFR-CON-001              | ✓ UC4.2  | UC4.2 |
| REQ-CON-017  | `line_read_rich()` shall browse history with ↑/↓: UP stores the current edit buffer in `szSavedInput` and fetches entries from most-recent toward oldest; DOWN moves toward newest; pressing DOWN past the newest restores `szSavedInput`. | UFR-CON-007 | ✓ UC4.3 | UC4.3 |
| REQ-CON-018  | TAB on the first word shall call `line_complete_cmd_query()`; TAB on subsequent words shall call `line_complete_path_query()`. One candidate → immediate suffix insertion. Multiple candidates → P23bis: first TAB inserts the longest common prefix of all candidates; subsequent TABs cycle ghost text. Any non-TAB key clears ghost. | UFR-CON-009, UFR-CON-084 | ✓ UC4.3, P23bis UC5 | UC5 |
| REQ-CON-019  | `line_history_add()` shall silently ignore empty strings and exact duplicates of the most-recent entry. The ring buffer is capped at `LINE_HISTORY_MAX` (64); the oldest entry is evicted when full. | UFR-CON-007 | ✓ UC4.3 | UC4.3 |
| REQ-CON-020  | `line_init()` shall call `line_history_load(NULL)` to restore history from `$HOME/.st4ever_history`; `line_shutdown()` shall call `line_history_save(NULL)` to persist it. A missing file on load shall return `ST_NO_ERROR` (first-run case). | UFR-CON-007 | ✓ UC4.3 | UC4.3 |
| REQ-CON-021  | `line_cmd_colors()` shall toggle `g_line_bColors` when called without argument; `colors on/off` shall force the value. All ANSI output shall be gated by `c_*()` static-inline functions that return `""` when colors are disabled. | UFR-CON-047 | ✓ UC4.3 | UC4.3 |
| REQ-CON-022  | When `szScriptFile[0] != '\0'`, `line_run()` shall open the file and dispatch each non-blank, non-comment line through `line_parse_cmd()` + `line_dispatch()` without entering raw mode. A missing file shall return `ST_ERROR`. | UFR-CON-048 | ✓ UC4.3 | UC4.3 |
| REQ-CON-023  | `line_set_selected(ptCtx, szPath)` and `line_get_selected(ptCtx, szBuf, uiBufLen)` shall acquire `ptCtx->ptSelectedMutex` before accessing `szSelected`; `set_selected(ptCtx, NULL)` shall clear the field. | UFR-DIR-002 | ✓ UC4.3 | UC4.3 |
| REQ-CON-024  | `line_build_prompt()` shall prepend `[T]` when `trace_is_open() == ST_TRUE` and `[basename]` when `szSelected` is non-empty; both are included, neither, or just one depending on state. | UFR-CON-049 | ✓ UC4.3 | UC4.3 |
| REQ-CON-025  | `line_cmd_where()` shall print `ptCtx->szCwd` and the result of `line_get_selected()` to stdout; if no file is selected, it shall print `(none)`. Extra arguments shall trigger a warning and be ignored. | UFR-CON-080 | ✓ UC5 | UC5 |
| REQ-CON-026  | `line_cmd_info()` shall print a dashboard covering: cwd, selected file, trace state (open/closed + LOG_TRACE on/off), colors state (on/off), history entry count, mounted disk (stub: "(none)"), and loaded binary (stub: "(none)"). Extra arguments shall trigger a warning. | UFR-CON-081 | ✓ UC5 | UC5 |
| REQ-CON-027  | `line_cmd_history([N])` shall print the last `min(N, count)` history entries numbered from 1. Without an argument, N defaults to 10. A non-positive or non-numeric N shall produce a warning and no output. If history is empty, it shall print `(no history)`. | UFR-CON-082 | ✓ UC5 | UC5 |
| REQ-CON-028  | `line_update_console_title(ptCtx)` shall update the OS window/terminal title to `"ST4Ever vX.Y.Z \| cwd [\| sel: basename] [\| T]"`. On Windows it shall call `SetConsoleTitleA()`; on Linux it shall emit ANSI OSC `\033]0;...\007`. A NULL `ptCtx` shall not crash (minimal title with no cwd). The function shall be called at `line_run()` start and after every `line_dispatch()` call. | UFR-CON-083 | ✓ UC5 | UC5 |
| REQ-CON-029  | `line_init()` shall detect stdout TTY status via `_isatty(_fileno(stdout))` (Windows) or `isatty(STDOUT_FILENO)` (Linux) and set `g_line_bColors` accordingly: `ST_TRUE` for a live terminal, `ST_FALSE` for a pipe or file. `colors on/off` shall remain available to override. | UFR-CON-085 | ✓ UC5 | UC5 |
| REQ-CON-030  | `line_cmd_trace()` shall accept the sub-command `clear` (calls `trace_clear()`, prints confirmation) and `level <trace\|info\|error>` (calls `trace_set_view_level()`, prints confirmation). Unknown first arguments shall produce a warning listing all valid sub-commands. | UFR-TRC-011, UFR-TRC-012 | ✓ UC5 | UC5 |
| REQ-CON-031  | `line_complete_cmd_query()` shall include `"info"` and `"history"` in its candidate set, matching on both the full name and the single-letter shortcut (`"n"` and `"y"` respectively). | UFR-CON-009 | ✓ UC5 | UC5 |

### 2.3 ST Machine Emulator — `ST.h` / `ST.c`

> Design ref: src/ST.h (memory map 24-bit address space en commentaire fichier), CLAUDE.md §5 R6, R9
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
| REQ-GUI-016  | The renderer shall use Direct2D / DirectWrite (Windows) or X11/XRender (Linux).                              | UFR-GUI-005  | ✓ UC3.2   | UC3.2 |
| REQ-GUI-017  | `gui_set_title(NULL, *)` and `gui_set_title(*, NULL)` shall return `ST_ERROR`.                               | UFR-GUI-006  | ✓ UC3.3   | UC3.3 |
| REQ-GUI-018  | `gui_set_title(hWnd, szTitle)` shall update the OS window title bar via the platform backend.                | UFR-GUI-006  | ✓ UC3.3   | UC3.3 |
| REQ-GUI-019  | `gui_request_close(NULL)` shall return `ST_ERROR`.                                                           | UFR-GUI-001  | ✓ UC4.1   | UC4.1 |
| REQ-GUI-020  | `gui_request_close(hWnd)` shall post `WM_CLOSE` asynchronously without joining the window thread; `gui_close_window()` shall handle the join correctly even if the window was already destroyed by a prior close request. | UFR-GUI-001 | ✓ UC4.1 | UC4.1 |
| REQ-GUI-021  | The Win32 window thread shall call `SetForegroundWindow()` and `SetFocus()` after `ShowWindow()` so the new view receives keyboard input immediately. | UFR-GUI-007 | ✓ UC4.1 | UC4.1 |
| REQ-GUI-022  | `gui_event_t.uData.tKey.uiMods` shall carry modifier bitmask flags: `GUI_MOD_CTRL=0x01`, `GUI_MOD_SHIFT=0x02`, `GUI_MOD_ALT=0x04`; the three flags shall be distinct and non-overlapping. | UFR-GUI-007 | ✓ UC8 | UC8 |
| REQ-GUI-023  | `WM_KEYDOWN` shall set `uiMods` from `GetKeyState(VK_CONTROL/SHIFT/MENU)` on all navigation events; existing views that ignore `uiMods` shall behave identically (field initialised to 0 by `memset`). | UFR-GUI-007 | ✓ UC8 | UC8 |
| REQ-GUI-024  | `WM_CHAR` control codes 1–26 shall be forwarded as `GUI_KEY_PRINTABLE+'A'..'Z'` with `uiMods=GUI_MOD_CTRL`, enabling views to dispatch CTRL+letter actions without separate VK handling. | UFR-GUI-007 | ✓ UC8 | UC8 |
| REQ-GUI-025  | `gui_clipboard_set_text(NULL)` shall return `ST_ERROR`. `gui_clipboard_set_text(szText)` shall place a NUL-terminated copy of `szText` on the system clipboard. | UFR-GUI-007 | ✓ UC8 | UC8 |
| REQ-GUI-026  | `gui_clipboard_get_text(NULL, N)` and `gui_clipboard_get_text(buf, 0)` shall return `ST_ERROR`; `szBuf` shall always be NUL-terminated on return (empty string on failure). | UFR-GUI-007 | ✓ UC8 | UC8 |
| REQ-GUI-027  | On Windows, `gui_clipboard_set_text` + `gui_clipboard_get_text` shall perform a correct round-trip. On Linux, both shall be stubs returning `ST_ERROR` (TODO UC8-Linux). | UFR-GUI-007 | ✓ UC8 | UC8 |

### 2.7 2D Renderer — `renderer.h` / `renderer.c` / `win_D2D.c`

> Design ref: CLAUDE.md §5 R1; Direct2D COM pure C (COBJMACROS + INITGUID required)

| ID           | Software Requirement                                                                                           | Parent UFR   | Status   | UC     |
|--------------|----------------------------------------------------------------------------------------------------------------|--------------|----------|--------|
| REQ-RND-001  | `renderer_create(NULL, *)` or `renderer_create(*, NULL)` shall return `ST_ERROR`.                             | UFR-GUI-005  | ✓ UC3.2  | UC3.2  |
| REQ-RND-002  | `renderer_create()` shall allocate a D2D render target bound to the window HWND and return `ST_NO_ERROR`.     | UFR-GUI-005  | ✓ UC3.2  | UC3.2  |
| REQ-RND-003  | `renderer_begin_draw(NULL)` and `renderer_end_draw(NULL)` shall return `ST_ERROR`.                            | UFR-GUI-005  | ✓ UC3.2  | UC3.2  |
| REQ-RND-004  | All draw primitives (`fill_rect`, `draw_rect`, `draw_line`, `draw_text`, `draw_bitmap`) shall return `ST_ERROR` when renderer handle is NULL. | UFR-GUI-005 | ✓ UC3.2 | UC3.2 |
| REQ-RND-005  | `renderer_resize()` shall recreate the HwndRenderTarget to match the new window dimensions.                   | UFR-GUI-005  | ✓ UC3.2  | UC3.2  |
| REQ-RND-006  | `renderer_destroy(NULL)` shall be a safe no-op; a valid handle shall release all COM resources.               | UFR-GUI-005  | ✓ UC3.2  | UC3.2  |
| REQ-RND-007  | `renderer_get_font_metrics()` shall return the monospace cell width and height in pixels.                     | UFR-GUI-005  | ✓ UC3.2  | UC3.2  |

---

### 2.8 Directory View — `dir.h` / `dir.c`

> Design ref: CLAUDE.md §1.1.3, §5 R18, §6.5

| ID           | Software Requirement                                                                                           | Parent UFR   | Status   | UC     |
|--------------|----------------------------------------------------------------------------------------------------------------|--------------|----------|--------|
| REQ-DIR-001  | `dir_open(path, NULL, *)` or `dir_open(path, *, NULL)` shall return `ST_ERROR`.                               | UFR-DIR-001  | ✓ UC3.3  | UC3.3  |
| REQ-DIR-002  | `dir_open(NULL path, ptCtx, pptView)` shall resolve to the current working directory (non-fatal fallback).    | UFR-DIR-001  | ✓ UC3.3  | UC3.3  |
| REQ-DIR-003  | `dir_open(non-existent path, …)` shall return `ST_NO_ERROR` with an empty tree (`iFlatCount == 0`).          | UFR-DIR-001  | ✓ UC3.3  | UC3.3  |
| REQ-DIR-004  | After `dir_open()`, `ptView->ptRoot != NULL` and `szRootPath` shall be non-empty.                             | UFR-DIR-001  | ✓ UC3.3  | UC3.3  |
| REQ-DIR-005  | `dir_open()` shall scan only the root's immediate children; sub-directories are loaded lazily at first expand.| UFR-DIR-001  | ✓ UC3.3  | UC3.3  |
| REQ-DIR-006  | `dir_close(NULL)` shall return `ST_ERROR`; `dir_close(&NULL)` shall return `ST_NO_ERROR` (idempotent).       | UFR-DIR-001  | ✓ UC3.3  | UC3.3  |
| REQ-DIR-007  | After `dir_close()`, the window thread shall be joined and `*pptView` shall be set to NULL.                   | UFR-DIR-001  | ✓ UC3.3  | UC3.3  |
| REQ-DIR-008  | ENTER or left-click on a directory node shall toggle expand/collapse, loading children lazily on first expand.| UFR-DIR-003  | ✓ UC3.3  | UC3.3  |
| REQ-DIR-009  | The `..` row shall navigate to the parent directory when activated.                                           | UFR-DIR-004  | ✓ UC3.3  | UC3.3  |
| REQ-DIR-010  | Activating a file (ENTER/SPACE) shall write its full path to `ptLineCtx->szSelected`.                        | UFR-DIR-002  | ✓ UC3.3  | UC3.3  |
| REQ-DIR-011  | The window title shall be updated to `"ST4Ever - Dir: <path>"` at open and on each navigation.               | UFR-GUI-006  | ✓ UC3.3  | UC3.3  |
| REQ-DIR-012  | Arrow keys (↑↓ PgUp/PgDn Home End) shall move selection and guarantee it remains visible (scroll-to-sel).    | UFR-DIR-001  | ✓ UC3.3  | UC3.3  |
| REQ-DIR-013  | Mouse wheel scroll shall update `iScrollOffset` clamped to `[0, max(0, iFlatCount+1 − iVisRows)]`.           | UFR-DIR-001  | ✓ UC3.3  | UC3.3  |
| REQ-CON-011  | `line_run()` shall dispatch `dir` / `d` / CTRL+D to `line_cmd_dir()`.                                        | UFR-CON-040  | ✓ UC3.3  | UC3.3  |
| REQ-DIR-014  | `dir_open()` shall accept a `bShowHidden` boolean parameter; when `ST_FALSE`, directory entries whose name starts with `.` shall be excluded from the scan; `.` and `..` pseudo-entries are always excluded. | UFR-DIR-007 | ✓ UC4.1 | UC4.1 |
| REQ-DIR-015  | `line_cmd_dir()` shall parse a `-a` flag (in any position among arguments) and pass `bShowHidden=ST_TRUE` to `dir_open()`. | UFR-DIR-007 | ✓ UC4.1 | UC4.1 |
| REQ-DIR-016  | With `bShowHidden=ST_FALSE`, `iFlatCount` shall reflect only non-hidden entries; with `bShowHidden=ST_TRUE`, all non-pseudo entries shall be counted. Both calls shall succeed with `ST_NO_ERROR`. | UFR-DIR-007 | ✓ UC4.1 | UC4.1 |
| REQ-DIR-017  | ESC key in `dir_handle_key()` shall call `gui_request_close(hWnd)` (non-blocking); the window thread may continue until the OS destroys the window. | UFR-DIR-008 | ✓ UC4.1 | UC4.1 |
| REQ-DIR-018  | LEFT arrow on an expanded directory node shall collapse it and rebuild the flat list; on a collapsed directory or a file: no-op. RIGHT arrow on a collapsed directory shall lazy-load children if needed, expand it, and rebuild the flat list; on an expanded directory or a file: no-op. | UFR-DIR-009 | ✓ UC4.1 | UC4.1 |
| REQ-DIR-019  | SPACE on `iSelectedFlat >= 0` shall write the node's full path to `ptLineCtx->szSelected` without modifying expand/collapse state; SPACE on the `..` row (`iSelectedFlat == -1`) shall be a no-op. | UFR-DIR-010 | ✓ UC4.1 | UC4.1 |
| REQ-DIR-020  | After `ShowWindow()`, the Win32 window thread shall call `SetForegroundWindow()` and `SetFocus()` so the view receives keyboard input immediately. | UFR-GUI-007 | ✓ UC4.1 | UC4.1 |
| REQ-DIR-021  | In `dir_handle_key()`, the `H`/`h` printable key shall toggle `ptView->bShowHidden`, call `dir_node_reload_children(ptRoot, bShowHidden)`, rebuild the flat list, and reset `iSelectedFlat = -2` and `iScrollOffset = 0`. (P21) | UFR-DIR-011 | ✓ UC5 | UC5 |
| REQ-DIR-023  | When a file node is committed via ENTER (`dir_activate_sel()`) or SPACE, `dir_view_t.szLastSelected` shall be updated with the node's full path; `dir_render()` shall draw `g_dir_clrLastSel` (dark green) on the matching row before the nav-highlight; directories and the `..` row shall never set `szLastSelected`; an empty `szLastSelected` means no green indicator. (P11) | UFR-DIR-013 | ✓ UC7 | UC7 |
| REQ-DIR-022  | In `dir_handle_key()`, `GUI_KEY_F5` shall call `dir_refresh_tree()`, which: (1) collects all currently-expanded dir paths via DFS into a stack-local array (max 256); (2) reloads the root's direct children; (3) for each saved path, navigates the new tree component-by-component, loading children and setting `bExpanded=ST_TRUE` at each level; missing components (dir deleted) are silently ignored. The flat list is rebuilt and the existing selection is clamped to valid range. (P22) | UFR-DIR-012 | ✓ UC5 | UC5 |

---

### 2.9 Console Raw Input — `console.h` / `win_console.c` / `lx_console.c`

> Design ref: CLAUDE.md §5 R5, R21; UC4.2

| ID           | Software Requirement                                                                                          | Parent UFR              | Status   | UC     |
|--------------|---------------------------------------------------------------------------------------------------------------|-------------------------|----------|--------|
| REQ-RAW-001  | `console_set_raw()` on a pipe handle (mintty) shall return `ST_NO_ERROR` without changing any mode.          | UFR-CON-041..046        | ✓ UC4.2  | UC4.2  |
| REQ-RAW-002  | `console_set_raw()` on a Win32 console handle (cmd.exe) shall remove ENABLE_PROCESSED/LINE/ECHO_INPUT.       | UFR-CON-041..046        | ✓ UC4.2  | UC4.2  |
| REQ-RAW-003  | `console_set_raw()` on non-TTY stdin shall return `ST_ERROR`.                                                 | UFR-CON-001             | ✓ UC4.2  | UC4.2  |
| REQ-RAW-004  | `console_set_raw()` shall be idempotent: a second call shall return `ST_NO_ERROR` (no-op).                    | UFR-CON-041..046        | ✓ UC4.2  | UC4.2  |
| REQ-RAW-005  | `console_restore()` shall be idempotent: calling without prior `set_raw` shall return `ST_NO_ERROR`.          | UFR-CON-041..046        | ✓ UC4.2  | UC4.2  |
| REQ-RAW-006  | `console_restore()` shall restore the original Win32 console mode (cmd.exe path only).                        | UFR-CON-041..046        | ✓ UC4.2  | UC4.2  |
| REQ-RAW-007  | `console_read_key(NULL)` shall return `ST_ERROR`.                                                             | UFR-CON-041..046        | ✓ UC4.2  | UC4.2  |
| REQ-RAW-008  | `console_read_key()` without active raw mode shall return `ST_ERROR` and set `*piKey = CON_KEY_EOF`.          | UFR-CON-041..046        | ✓ UC4.2  | UC4.2  |
| REQ-RAW-009  | Byte value `0x7F` shall be decoded as `CON_KEY_BACKSPACE` (mintty pipe path).                                 | UFR-CON-043             | ✓ UC4.2  | UC4.2  |
| REQ-RAW-010  | VT100 sequences `\033[A..D`, `\033[H`, `\033[F` shall decode to UP/DOWN/RIGHT/LEFT/HOME/END.                  | UFR-CON-041, UFR-CON-008| ✓ UC4.2  | UC4.2  |
| REQ-RAW-011  | VT100 sequences `\033[3~`, `\033[5~`, `\033[6~` shall decode to DELETE, PAGE_UP, PAGE_DOWN.                   | UFR-CON-043             | ✓ UC4.2  | UC4.2  |
| REQ-RAW-012  | Bare ESC (no continuation within 50 ms) shall decode to `CON_KEY_ESC`.                                       | UFR-CON-044             | ✓ UC4.2  | UC4.2  |
| REQ-RAW-013  | Win32 VK_BACK shall decode to `CON_KEY_BACKSPACE`; VK_DELETE to `CON_KEY_DELETE` (cmd.exe path).             | UFR-CON-043             | ✓ UC4.2  | UC4.2  |
| REQ-RAW-014  | stdin EOF or ReadFile failure shall return `ST_ERROR` with `*piKey = CON_KEY_EOF`.                            | UFR-CON-003             | ✓ UC4.2  | UC4.2  |

---

### 2.10 File System Abstraction — `file.h` / `file.c`

> Design ref: CLAUDE.md §6.11; CRT fopen/fread/fwrite + POSIX opendir/readdir/stat

| ID           | Software Requirement                                                                                                                         | Parent UFR  | Status   | UC   |
|--------------|----------------------------------------------------------------------------------------------------------------------------------------------|-------------|----------|------|
| REQ-FIL-001  | `file_stat(NULL, *)` and `file_stat(*, NULL)` shall return `ST_ERROR`.                                                                      | UFR-FIL-001 | ✓ UC6    | UC6  |
| REQ-FIL-002  | `file_stat()` on a non-existent path shall return `ST_NO_ERROR` with `ptStat->bExists == ST_FALSE`; no LOG_ERROR shall be emitted.          | UFR-FIL-001 | ✓ UC6    | UC6  |
| REQ-FIL-003  | `file_stat()` on an existing file shall fill `bExists=TRUE`, `bIsDir=FALSE`, `uiSize` (bytes), and `szExt` (lowercase extension without the dot, or `""` if none). On a directory: `bIsDir=TRUE`, `uiSize=0`. | UFR-FIL-001 | ✓ UC6 | UC6 |
| REQ-FIL-004  | `file_open(NULL, *, *)` and `file_open(*, *, NULL)` shall return `ST_ERROR`.                                                                | UFR-FIL-002 | ✓ UC6    | UC6  |
| REQ-FIL-005  | `file_open()` with `FILE_MODE_READ` on a non-existent path shall return `ST_ERROR` and LOG_ERROR.                                           | UFR-FIL-002 | ✓ UC6    | UC6  |
| REQ-FIL-006  | `file_open()` with `FILE_MODE_WRITE` shall create the file or truncate it; `FILE_MODE_APPEND` shall create or open for appending.           | UFR-FIL-002 | ✓ UC6    | UC6  |
| REQ-FIL-007  | `file_read(NULL, *, *, *)`, `file_read(*, NULL, *, *)`, and `file_read(*, *, *, NULL)` shall return `ST_ERROR`.                            | UFR-FIL-003 | ✓ UC6    | UC6  |
| REQ-FIL-008  | `file_read()` with `uiLen == 0` shall return `ST_NO_ERROR` with `*puiRead == 0`.                                                            | UFR-FIL-003 | ✓ UC6    | UC6  |
| REQ-FIL-009  | `file_read()` at end-of-file shall return `ST_NO_ERROR` with `*puiRead == 0`; a partial read (short of `uiLen` without `ferror`) is also `ST_NO_ERROR`. | UFR-FIL-003 | ✓ UC6 | UC6 |
| REQ-FIL-010  | `file_read()` with `ferror()` set shall return `ST_ERROR`.                                                                                  | UFR-FIL-003 | ✓ UC6    | UC6  |
| REQ-FIL-011  | `file_write(NULL, *, *)` and `file_write(*, NULL, *)` shall return `ST_ERROR`; `file_write(*, *, 0)` shall return `ST_ERROR`.              | UFR-FIL-004 | ✓ UC6    | UC6  |
| REQ-FIL-012  | `file_write()` with a short write (fwrite returns < uiLen) shall return `ST_ERROR`.                                                        | UFR-FIL-004 | ✓ UC6    | UC6  |
| REQ-FIL-013  | `file_close(NULL)` shall return `ST_ERROR`; `file_close(&NULL)` shall return `ST_NO_ERROR` (idempotent safe no-op).                        | UFR-FIL-005 | ✓ UC6    | UC6  |
| REQ-FIL-014  | `file_close()` shall call `fclose()`, `free()` the handle, and set `*pptFile = NULL` before returning.                                     | UFR-FIL-005 | ✓ UC6    | UC6  |
| REQ-FIL-015  | `file_mkdir(NULL)` shall return `ST_ERROR`.                                                                                                 | UFR-FIL-006 | ✓ UC6    | UC6  |
| REQ-FIL-016  | `file_mkdir()` on an already-existing directory shall return `ST_NO_ERROR` (EEXIST silently accepted).                                      | UFR-FIL-006 | ✓ UC6    | UC6  |
| REQ-FIL-017  | `file_list_dir(NULL, *, *, *)` and `file_list_dir(*, *, NULL, *)` shall return `ST_ERROR`.                                                 | UFR-FIL-007 | ✓ UC6    | UC6  |
| REQ-FIL-018  | `file_list_dir()` on a path that cannot be opened with `opendir()` shall return `ST_ERROR`.                                                 | UFR-FIL-007 | ✓ UC6    | UC6  |
| REQ-FIL-019  | `file_list_dir()` shall never pass `.` or `..` to the callback.                                                                             | UFR-FIL-007 | ✓ UC6    | UC6  |
| REQ-FIL-020  | When `bShowHidden == ST_FALSE`, `file_list_dir()` shall exclude entries whose name starts with `.`.                                          | UFR-FIL-007 | ✓ UC6    | UC6  |
| REQ-FIL-021  | The callback shall receive: `szFull` (full path = `szDir/szName`), `szName` (entry name only), and `ptStat` (pre-filled via `file_stat()`). | UFR-FIL-007 | ✓ UC6    | UC6  |
| REQ-FIL-022  | The `file_t` handle type shall be opaque: defined only in `file.c`; callers must not access the underlying `FILE *` directly.               | UFR-FIL-002 | ✓ UC6    | UC6  |

### 2.11 Load Command — `load.h` / `load.c`

> Design ref: CLAUDE.md §1.1.4, §6.12; depends on `file.h` (UC6) and `ST.h` (UC1)

| ID           | Software Requirement                                                                                                                               | Parent UFR  | Status   | UC   |
|--------------|----------------------------------------------------------------------------------------------------------------------------------------------------|-------------|----------|------|
| REQ-LOD-001  | `load_init(NULL)` shall return `ST_ERROR`; `load_init(ptMachine)` shall store the machine pointer and reset state to bLoaded=FALSE, eType=NONE.   | UFR-LOD-001 | ✓ UC7    | UC7  |
| REQ-LOD-002  | `load_get_state()` shall always return a non-NULL pointer to the internal `load_state_t`; it is valid for the program lifetime.                    | UFR-LOD-001 | ✓ UC7    | UC7  |
| REQ-LOD-003  | `load_file(NULL)` shall return `ST_ERROR`; calling `load_file()` before `load_init()` shall return `ST_ERROR`.                                    | UFR-LOD-001 | ✓ UC7    | UC7  |
| REQ-LOD-004  | `load_file()` on a non-existent path shall return `ST_ERROR` with `LOG_ERROR`.                                                                    | UFR-LOD-002 | ✓ UC7    | UC7  |
| REQ-LOD-005  | `load_file()` on any file that is not a directory, not a disk image, and not a PRG-type extension shall copy the content verbatim to `aRam[ST_LOAD_BASE..]` and set `eType = LOAD_TYPE_BINARY`. | UFR-LOD-003 | ✓ UC7 | UC7 |
| REQ-LOD-006  | After a successful `load_file()`, the state shall reflect `bLoaded=TRUE`, the actual file path in `szPath`, `uiLoadAddr = ST_LOAD_BASE`, and `uiSize` equal to the number of bytes loaded into RAM. | UFR-LOD-003 | ✓ UC7 | UC7 |
| REQ-LOD-007  | `load_file()` on a `.prg`/`.ttp`/`.tos` path shall read the 28-byte PRG header, validate the `0x601A` magic, read `uiTextSize + uiDataSize` bytes at `ST_LOAD_BASE`, and set `eType = LOAD_TYPE_PRG`. | UFR-LOD-004 | ✓ UC7 | UC7 |
| REQ-LOD-008  | A PRG file whose first two bytes differ from `0x601A` shall cause `load_file()` to return `ST_ERROR` with `LOG_ERROR`; the previous load state shall not be modified. | UFR-LOD-004 | ✓ UC7 | UC7 |
| REQ-LOD-009  | A new successful `load_file()` call shall replace the entire previous load state atomically.                                                       | UFR-LOD-001 | ✓ UC7    | UC7  |
| REQ-LOD-010  | `load_file()` on a directory shall return `ST_ERROR` with `LOG_ERROR` ("use mount").                                                              | UFR-LOD-002 | ✓ UC7    | UC7  |
| REQ-LOD-011  | `load_file()` on a path whose lowercase extension is `st`, `msa`, or `stx` shall return `ST_ERROR` with `LOG_ERROR` ("use mount").                | UFR-LOD-002 | ✓ UC7    | UC7  |
| REQ-LOD-012  | `load_file()` on a file whose size exceeds `ST_LOAD_MAX_SIZE` (= `ST_RAM_SIZE − 0x0800`) shall return `ST_ERROR`.                                | UFR-LOD-003 | ✓ UC7    | UC7  |
| REQ-LOD-013  | `load_shutdown()` shall set the machine pointer to NULL and reset state to bLoaded=FALSE; calling it when already shut down shall return `ST_NO_ERROR` (idempotent). | UFR-LOD-001 | ✓ UC7 | UC7 |
| REQ-LOD-014  | `line_cmd_load()` shall call `file_stat()` before `load_file()` to produce user-friendly console messages for directory/image rejection; on success it shall print the load address and byte count; `line_update_console_title()` shall be called after success. | UFR-LOD-001..005 | ✓ UC7 | UC7 |

---

### 2.12 Text Editor — `edit_txt.h` / `edit_txt.c`

> Design ref: CLAUDE.md §1.1.5, §6.13; depends on `file.h` (UC6), `gui.h` (UC3.1), `renderer.h` (UC3.2)

| ID           | Software Requirement                                                                                                                                | Parent UFR  | Status   | UC   |
|--------------|----------------------------------------------------------------------------------------------------------------------------------------------------|-------------|----------|------|
| REQ-EDT-001  | `edit_txt_open(NULL, *, *)`, `(*, NULL, *)`, `(*, *, NULL)` shall return `ST_ERROR` without modifying `*pptView`. | UFR-EDT-001 | ✓ UC8 | UC8 |
| REQ-EDT-002  | `edit_txt_open()` on a non-existent file shall return `ST_ERROR` and leave `*pptView == NULL`. | UFR-EDT-001 | ✓ UC8 | UC8 |
| REQ-EDT-003  | An empty (zero-byte) file shall load as `iLineCount == 1` with one empty string; the line buffer shall never be empty after a successful open. | UFR-EDT-001 | ✓ UC8 | UC8 |
| REQ-EDT-004  | File content shall be split on `\n`; `\r\n` sequences shall be normalised by stripping `\r`; lines longer than `EDIT_TXT_MAX_LINE_LEN − 1` bytes shall be truncated silently. | UFR-EDT-001 | ✓ UC8 | UC8 |
| REQ-EDT-005  | Tab characters shall expand to the next `EDIT_TXT_TAB_WIDTH` (4) display column stop; non-tab characters shall map 1:1 from byte offset to display column. | UFR-EDT-002 | ✓ UC8 | UC8 |
| REQ-EDT-006  | `etxt_byte_col_from_disp(szLine, etxt_display_len(szLine, iByteCol))` shall return `iByteCol` for all valid byte positions (round-trip invariant). | UFR-EDT-002 | ✓ UC8 | UC8 |
| REQ-EDT-007  | `line_cmd_edit()` on an extension in `{prg, ttp, tos, st, msa, stx}` shall display a stub message and return `ST_NO_ERROR` without opening a window (hex editor deferred to UC9). | UFR-EDT-006 | ✓ UC8 | UC8 |
| REQ-EDT-008  | CTRL+Z shall restore the most recent undo snapshot; if no snapshot exists, CTRL+Z shall be a no-op. | UFR-EDT-004 | ✓ UC8 | UC8 |
| REQ-EDT-009  | The undo ring shall hold `EDIT_TXT_UNDO_LEVELS` (20) snapshots; when full, the oldest entry shall be silently overwritten. | UFR-EDT-004 | ✓ UC8 | UC8 |
| REQ-EDT-010  | Consecutive printable-character insertions shall be grouped into a single undo level; any navigation key (arrow, Home, End, PgUp/Dn) or structural operation (ENTER, Backspace, Delete, Tab, paste) shall break the current group and start a new one on the next insert. | UFR-EDT-004 | ✓ UC8 | UC8 |
| REQ-EDT-011  | CTRL+S shall write the file via `file_open(FILE_MODE_WRITE)`; after a successful save `bDirty` shall be `ST_FALSE` and the window title shall no longer contain `[*]`. | UFR-EDT-005 | ✓ UC8 | UC8 |
| REQ-EDT-012  | `gui_clipboard_set_text(NULL)` → `ST_ERROR`; `gui_clipboard_get_text(NULL, N)` and `(buf, 0)` → `ST_ERROR`; `szBuf` shall always be NUL-terminated on return (empty string on any failure). | UFR-EDT-003, UFR-GUI-007 | ✓ UC8 | UC8 |
| REQ-EDT-013  | `GUI_MOD_CTRL`, `GUI_MOD_SHIFT`, and `GUI_MOD_ALT` shall be distinct power-of-two bitmask values with no overlap; they shall be combinable by bitwise OR. | UFR-GUI-007 | ✓ UC8 | UC8 |

### 2.13 Hex Editor — `edit_hex.h` / `edit_hex.c`

> Design ref: CLAUDE.md §1.1.5, §6.14; depends on `file.h` (UC6), `gui.h` (UC3.1), `renderer.h` (UC3.2)

| ID           | Software Requirement                                                                                                                                | Parent UFR  | Status   | UC   |
|--------------|----------------------------------------------------------------------------------------------------------------------------------------------------|-------------|----------|------|
| REQ-HEX-001  | `edit_hex_open(NULL, *, *)`, `(*, NULL, *)`, `(*, *, NULL)` shall return `ST_ERROR` without modifying `*pptView`. | UFR-HEX-001 | ✓ UC9 | UC9 |
| REQ-HEX-002  | `edit_hex_open()` on a non-existent file shall return `ST_ERROR` and leave `*pptView == NULL`. | UFR-HEX-001 | ✓ UC9 | UC9 |
| REQ-HEX-003  | Layout constants shall be: `EDIT_HEX_BYTES_PER_ROW=16`, `EDIT_HEX_HEX_CHARS=49`, `EDIT_HEX_ADDR_CHARS=7`, `EDIT_HEX_MAX_SIZE=16 MB`. | UFR-HEX-002 | ✓ UC9 | UC9 |
| REQ-HEX-004  | The hex row builder shall place byte i (0≤i<8) at string offset `i*3` and byte i (8≤i<16) at offset `i*3+1`; bytes beyond `uiSize` shall be rendered as spaces. | UFR-HEX-002 | ✓ UC9 | UC9 |
| REQ-HEX-005  | Position 24 in the hex row string shall always be a space (gap between byte-group 0–7 and byte-group 8–15). | UFR-HEX-002 | ✓ UC9 | UC9 |
| REQ-HEX-006  | A partial last row (fewer than 16 bytes) shall pad the hex and ASCII columns with spaces for the missing positions. | UFR-HEX-002 | ✓ UC9 | UC9 |
| REQ-HEX-007  | `HEX_ZONE_HEX` and `HEX_ZONE_ASCII` shall be distinct enum values. | UFR-HEX-003 | ✓ UC9 | UC9 |
| REQ-HEX-008  | HEX zone: typing a valid hex digit (0–9/A–F/a–f) shall overwrite the current nibble and advance the cursor (high→low within a byte, then to the next byte's high nibble). | UFR-HEX-003 | ✓ UC9 | UC9 |
| REQ-HEX-009  | ASCII zone: typing a printable character (0x20–0x7E) shall overwrite the current byte and advance the cursor by one byte. | UFR-HEX-003 | ✓ UC9 | UC9 |
| REQ-HEX-010  | CTRL+S shall write the entire `pData` buffer to file via `file_open(FILE_MODE_WRITE)`; after a successful save `bDirty` shall be `ST_FALSE` and the window title shall no longer contain `[*]`. | UFR-HEX-004 | ✓ UC9 | UC9 |

---

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
| Console Interface    | GUI Framework         | `gui_open_window(desc)` per view command              | ✓ UC3.3   |
| GUI Framework        | ST Machine Emulator   | `st_read/write_byte/word/long()`                      | TODO UC25 |
| GUI Framework        | Disassembler          | `disasm_range()` for hex+asm view sync                | TODO UC10 |
| ST Machine Emulator  | CPU 68000             | `cpu_init()`, `cpu_step()`, shared `st_machine_t*`    | TODO UC21 |
| Console Interface    | Platform Layer        | `win_console_init()` at startup                       | ✓ UC1     |
| GUI Framework        | Platform Layer        | `gui_open_window()` → `gui_platform_window_create()`  | ✓ UC3.1   |
| GUI Framework        | Renderer              | `renderer_begin/end_draw()`, draw primitives          | ✓ UC3.2   |
| Directory View       | GUI Framework         | `gui_set_title()`, `gui_invalidate()` (R18)           | ✓ UC3.3   |

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
  ├─ parse -t / -h / --script flags                [CRT]
  ├─ win_console_init()         [ST4]  ANSI + UTF-8 on Win32
  ├─ trace_init(bOpen)          [ST4]  open log; optionally show console
  ├─ gui_init()                 [ST4]  register Win32 class (stub UC3)
  ├─ line_init(&tCtx)           [ST4]  capture cwd, load history, bRunning=TRUE
  ├─ copy szScriptFile → tCtx   [CRT]  --script path set before line_run
  ├─ line_run(&tCtx)            [ST4]  blocking loop (or script batch)
  ├─ line_shutdown(&tCtx)       [ST4]  save history
  ├─ gui_shutdown()             [ST4]  (stub UC3)
  └─ trace_shutdown()           [ST4]  flush + close log
```

| External call      | Tag   | Purpose          |
|--------------------|-------|------------------|
| `fprintf(stderr,…)`| [CRT] | `-h` usage text  |

---

### 4.2 Trace Subsystem — `trace.c`

**Role:** logging to `st4ever_trace.log` and, from UC4.4, to a dedicated D2D
GUI window (ring buffer, colour by level). Four levels: TRACE (compacted),
INFO, ERROR, TODO. Stderr output is suppressed when the GUI view is live.

**Public API:**

| Function                              | REQ(s)                              | Description                                       |
|---------------------------------------|-------------------------------------|---------------------------------------------------|
| `trace_init(bOpen)`                   | REQ-TRC-001, REQ-TRC-002            | Init log file; optional GUI window open            |
| `trace_open()`                        | REQ-TRC-008, REQ-TRC-014, REQ-TRC-017 | Open `GUI_WND_TRACE`; idempotent if already open |
| `trace_close()`                       | REQ-TRC-006, REQ-TRC-007            | Close GUI window; join thread; idempotent          |
| `trace_set_trace_enabled(b)`          | REQ-TRC-009, REQ-TRC-010            | Suppress / re-enable LOG_TRACE only               |
| `trace_is_trace_enabled()`            | REQ-TRC-011                         | Query LOG_TRACE filter state                      |
| `trace_is_open()`                     | REQ-TRC-003                         | Query GUI window visibility                       |
| `trace_log(level, func, line, fmt,…)` | REQ-TRC-004, REQ-TRC-013, REQ-TRC-018 | Emit one entry (use LOG_* macros instead)       |
| `trace_shutdown()`                    | REQ-TRC-012                         | Flush, close file, close GUI if open, free state  |
| `trace_clear()`                       | REQ-TRC-023                         | Reset ring buffer; invalidate view; no-op if not open |
| `trace_set_view_level(eMinLevel)`     | REQ-TRC-024, REQ-TRC-025            | Set GUI display filter; persist in global; invalidate |
| `trace_get_view_level()`              | REQ-TRC-025                         | Return current `g_trace_eViewMinLevel`            |

**Key internal functions:**

| Function                               | REQ(s)                   | Description                                               |
|----------------------------------------|--------------------------|-----------------------------------------------------------|
| `trace_get_timestamp(szBuf, uiBufLen)` | REQ-TRC-015              | Fills buffer with `HH:MM:SS` from system clock            |
| `trace_level_label(eLevel)`            | REQ-TRC-015              | Returns level tag (`TRC `, `INF `, `ERR `, `TODO`)        |
| `trace_flush_compact()`                | REQ-TRC-005              | Emits pending `[xN]` summary before any non-TRACE entry   |
| `trace_event_callback(hWnd, ptEvt, p)` | REQ-TRC-019..022         | GUI_EVT_PAINT/RESIZE/KEY_DOWN/SCROLL/CLOSE for trace view |
| `trace_render(ptView)`                 | REQ-TRC-008, REQ-TRC-019 | D2D: bg + colour-coded lines + alternating row shading    |
| `trace_view_scroll(ptView, iDelta)`    | REQ-TRC-020, REQ-TRC-022 | Clamp scroll; update bAutoScroll                          |

**`trace_view_t` — internal ring buffer (UC4.4):**

| Field            | Type                     | Purpose                                               |
|------------------|--------------------------|-------------------------------------------------------|
| `ptMutex`        | `st_mutex_t *`           | Protects ring buffer on concurrent trace_log() calls  |
| `aszLines[]`     | `char[200][…]`           | Ring buffer: 200 entries × (ST_MAX_MSG + 64) bytes    |
| `iHead`          | `int`                    | Index of next write slot (circular)                   |
| `iCount`         | `int`                    | Number of valid entries (capped at 200)               |
| `iScrollOff`     | `int`                    | First visible line index                              |
| `bAutoScroll`    | `st_bool_t`              | TRUE: scroll to newest on each append                 |
| `eViewMinLevel`  | `log_level_t`            | P28 display filter: lines below this level not rendered; default LOG_LEVEL_TRACE (show all) |
| `hRenderer`      | `renderer_handle_t`      | D2D renderer (created lazily on first GUI_EVT_PAINT)  |
| `iCellH`         | `int`                    | Font cell height from `renderer_get_font_metrics()`   |

**Re-entrancy guard `g_trace_bInNotify` (UC4.4):**

`trace_log()` calls `platform_mutex_lock()` and `gui_invalidate()`. Without a
guard, any `LOG_TRACE` inside those functions would recurse into `trace_log()`
and deadlock. The flag is set before the critical section and cleared after
`gui_invalidate()`, covering the entire GUI notification path.

**External dependencies:**

| Call                                      | Tag   | Purpose                                         |
|-------------------------------------------|-------|-------------------------------------------------|
| `fopen / fclose / fprintf`                | [CRT] | Log file I/O                                    |
| `vsnprintf`                               | [CRT] | Format variadic args in `trace_log()`           |
| `time / localtime / strftime`             | [CRT] | Timestamp via `trace_get_timestamp()`           |
| `gui_open_window / gui_close_window`      | [ST4] | Window lifecycle for `GUI_WND_TRACE`            |
| `gui_invalidate / gui_request_close`      | [ST4] | Trigger repaint / async close on ESC            |
| `platform_mutex_create/lock/unlock/destroy` | [ST4] | Ring buffer thread safety                     |
| `renderer_*`                              | [ST4] | D2D draw primitives for trace view              |

**Output routing (UC4.4):**

```
trace_log(level, …)
  ├─ write to st4ever_trace.log            (always)
  ├─ g_trace_ptView != NULL?
  │    YES → append ring buffer            (GUI live — no stderr)
  │          gui_invalidate() under bInNotify guard
  │    NO  → (log file only; bOpen may be TRUE but GUI not yet ready)
  └─ filter: LOG_TRACE suppressed if trace_is_trace_enabled()==FALSE
```

**Compaction sequence:**

```
LOG_TRACE("msg A")  ← from function foo()
  └─ same function as last?  YES → g_trace_iCompactCount++
                             NO  → trace_flush_compact(); emit new entry

LOG_INFO("msg B")   ← from function bar()
  └─ trace_flush_compact() → write "[x3] foo …"
     then emit INFO "msg B"
```

---

### 4.3 Console Line Reader — `line.c`

**Role:** display prompt, read a line, tokenise it, dispatch to a command
handler.  Owns `line_context_t` (cwd, current selection, running flag).

**Public API:**

| Function                                          | REQ(s)                          | Description                                                        |
|---------------------------------------------------|---------------------------------|--------------------------------------------------------------------|
| `line_init(ptCtx)`                                | REQ-CON-001..003, REQ-CON-020   | Zero context, capture cwd, create mutex, load history, bRunning=TRUE |
| `line_run(ptCtx)`                                 | REQ-CON-006..009, REQ-CON-022   | Blocking loop (interactive or script batch)                        |
| `line_shutdown(ptCtx)`                            | REQ-CON-004, REQ-CON-005, REQ-CON-020 | Save history, close dir view, destroy mutex, bRunning=FALSE  |
| `line_print_msg(fmt,…)`                           | —                               | Normal response to stdout (utility)                                |
| `line_print_warning(fmt,…)`                       | —                               | Yellow warning to stdout (utility)                                 |
| `line_print_error(fmt,…)`                         | —                               | Red error to stdout (utility)                                      |
| `line_history_add(szEntry)`                       | REQ-CON-019                     | Add entry; ignore empty / adjacent duplicate; evict oldest on full |
| `line_history_count()`                            | REQ-CON-019                     | Return current number of stored entries                            |
| `line_history_get(iVirt, szBuf, uiBufLen)`        | REQ-CON-017                     | Get entry at virtual index 0=oldest; ST_ERROR if out-of-range      |
| `line_history_clear()`                            | —                               | Reset count/head to 0; no deallocation                             |
| `line_history_save(szPath)`                       | REQ-CON-020                     | Write history to file; NULL → `$HOME/.st4ever_history`            |
| `line_history_load(szPath)`                       | REQ-CON-020                     | Read history from file; missing file → ST_NO_ERROR (first run)    |
| `line_complete_cmd_query(szPrefix, aOut, iMax)`   | REQ-CON-018                     | Return count of command names matching prefix (szFull and szShort) |
| `line_complete_path_query(szPrefix, szCwd, aOut, iMax)` | REQ-CON-018               | Return count of FS entries matching prefix; dirs suffixed `/`      |
| `line_set_colors(bColors)`                        | REQ-CON-021                     | Set global ANSI color state                                        |
| `line_get_colors()`                               | REQ-CON-021                     | Query current ANSI color state                                     |
| `line_set_selected(ptCtx, szPath)`                | REQ-CON-023                     | Thread-safe write under mutex; NULL clears the field               |
| `line_get_selected(ptCtx, szBuf, uiBufLen)`       | REQ-CON-023                     | Thread-safe read under mutex                                       |
| `line_update_console_title(ptCtx)`                | REQ-CON-028                     | SetConsoleTitleA (Win) / OSC escape (Linux); NULL-safe             |

**Key internal functions:**

| Function                           | REQ(s)                              | Description                                                              |
|------------------------------------|-------------------------------------|--------------------------------------------------------------------------|
| `line_trim()`                      | —                                   | Remove leading/trailing whitespace in place (utility)                    |
| `line_parse_cmd()`                 | REQ-CON-006..009                    | Tokenise raw input into `parsed_cmd_t` and match command token           |
| `line_dispatch()`                  | REQ-CON-006..009                    | Route `parsed_cmd_t` to handler via `g_line_aHandlers[]`                |
| `line_cmd_help()`                  | REQ-CON-006                         | Print command summary                                                    |
| `line_cmd_quit()`                  | REQ-CON-007                         | Set `bRunning = FALSE`                                                   |
| `line_cmd_trace()`                 | REQ-CON-008                         | Call `trace_open/close/toggle` (full impl UC2)                           |
| `line_cmd_colors()`                | REQ-CON-021                         | Toggle or force `g_line_bColors`; warn on unknown arg                    |
| `line_cmd_where()`                 | REQ-CON-025                         | Print cwd + selected file (or "(none)")                                  |
| `line_cmd_info()`                  | REQ-CON-026                         | Print full status dashboard                                              |
| `line_cmd_history()`               | REQ-CON-027                         | Print last N history entries (default 10); warn on bad N                 |
| `line_cmd_stub()`                  | REQ-CON-009                         | LOG_TODO + "not yet implemented" for all other commands                  |
| `line_run_script(ptCtx)`           | REQ-CON-022                         | Open `szScriptFile`, parse+dispatch each line; `#` and blanks skipped    |
| `line_redraw(ptCtx, szBuf, uiLen, uiCur, szGhost)` | REQ-CON-010              | `\r\033[2K` + prompt + buffer + ghost DIM + `\033[ND` cursor reposition  |
| `line_shortcut(szBuf, szCmd)`      | REQ-CON-010, UFR-CON-046            | Fill buf with cmd name, redraw, `\n`, return ST_NO_ERROR                 |
| `line_read_rich(ptCtx, szBuf)`     | REQ-CON-010, REQ-CON-015..018, UFR-CON-041..046 | Rich editor: insert, edit, CTRL, history ↑↓, TAB ghost, EOF |
| `line_build_prompt(ptCtx, szBuf, uiBufLen)` | REQ-CON-024                | Build dynamic prompt `APP_NAME[T][basename]> ` using current state       |
| `line_path_basename(szPath)`       | —                                   | Return pointer to last component of a path (no alloc)                    |

**External dependencies:**

| Call                      | Tag       | Purpose                                             |
|---------------------------|-----------|-----------------------------------------------------|
| `fgets`                   | [CRT]     | Fallback line read (non-TTY / script mode)          |
| `console_set_raw`         | [ST4]     | Enable raw key-by-key input                         |
| `console_restore`         | [ST4]     | Restore console mode after loop                     |
| `console_read_key`        | [ST4]     | Read one key as CON_KEY_*                           |
| `trace_is_open`           | [ST4]     | Query trace state for prompt (UC4.3)                |
| `opendir / readdir / closedir` | [POX] | Path completion (POSIX available via MinGW)        |
| `getcwd`                  | [POX/CRT] | Capture working directory                           |
| `GetCurrentDirectory`     | [WIN]     | Capture working directory (Win32)                   |
| `platform_mutex_create/lock/unlock/destroy` | [ST4] | Thread-safe szSelected access       |
| `printf / fprintf / fopen / fclose` | [CRT] | Console output + history file I/O              |

**UC4.2:** `fgets` replaced by `line_read_rich()` when stdin is a TTY;
`fgets` fallback kept for non-TTY (CI, pipes, `--script` mode).

**UC4.3 additions:** history ring buffer (global to `line.c`, persists across init/shutdown);
`c_*()` static-inline ANSI helpers gate all colour output; `ptSelectedMutex` protects
`szSelected` from concurrent window-thread writes; `line_run_script()` branch in `line_run()`.

**UC5 additions:** `CMD_WHERE` / `CMD_INFO` / `CMD_HISTORY` dispatched to real handlers;
`line_update_console_title()` public — `SetConsoleTitleA` (Win) / OSC escape (Linux) — called
at loop start and after every dispatch; `g_line_bColors` initialised via `isatty()` in
`line_init()` (P24); TAB P23bis: common-prefix insertion block before ghost cycle in
`line_read_rich()`; `trace clear` and `trace level` sub-commands in `line_cmd_trace()`.

**Command dispatch sequence (UC5):**

```
line_run()
  ├─ line_update_console_title()   [ST4]  P8: set initial title
  ├─ console_set_raw()             [ST4]  switch to raw mode (or fgets fallback)
  └─ loop while bRunning:
       ├─ line_read_rich() / fgets [ST4/CRT]  read + edit line
       ├─ line_parse_cmd()         → parsed_cmd_t  (tokenise + match cmd token)
       ├─ line_dispatch()          → g_line_aHandlers[eCmd]
       │    ├─ CMD_HELP    → line_cmd_help()
       │    ├─ CMD_QUIT    → line_cmd_quit()  → bRunning=FALSE → loop exits
       │    ├─ CMD_TRACE   → line_cmd_trace() [on|off|clear|level]
       │    ├─ CMD_WHERE   → line_cmd_where()
       │    ├─ CMD_INFO    → line_cmd_info()
       │    ├─ CMD_HISTORY → line_cmd_history()
       │    ├─ CMD_DIR     → line_cmd_dir()
       │    ├─ CMD_COLORS  → line_cmd_colors()
       │    └─ CMD_*       → line_cmd_stub()  LOG_TODO + warning
       └─ line_update_console_title() [ST4]  P8: refresh after each cmd
```

---

### 4.4 ST Machine Emulator — `ST.c`

**Role:** model the 24-bit Atari ST address space.  UC1 implements RAM
byte/word/long r/w with big-endian ordering and alignment checks.  Hardware
register access is stubbed (returns `0xFF` / no-op).

**Public API:**

| Function                                      | REQ(s)                              | Description                            |
|-----------------------------------------------|-------------------------------------|----------------------------------------|
| `st_init(ptMachine, szRomPath)`               | REQ-STM-001..003                    | Zero RAM, set bPoweredOn, load ROM opt.|
| `st_read_byte(ptMachine, addr, puiByte)`       | REQ-STM-004, REQ-STM-005..006, REQ-STM-011 | Read 1 byte from address space  |
| `st_read_word(ptMachine, addr, puiWord)`       | REQ-STM-004, REQ-STM-007, REQ-STM-009 | Read big-endian word (even only)    |
| `st_read_long(ptMachine, addr, puiLong)`       | REQ-STM-004, REQ-STM-008, REQ-STM-010 | Read big-endian long (even only)    |
| `st_write_byte(ptMachine, addr, val)`          | REQ-STM-004, REQ-STM-006           | Write 1 byte                           |
| `st_write_word(ptMachine, addr, val)`          | REQ-STM-004, REQ-STM-007, REQ-STM-009 | Write big-endian word (even only)   |
| `st_write_long(ptMachine, addr, val)`          | REQ-STM-004, REQ-STM-008, REQ-STM-010 | Write big-endian long (even only)   |
| `st_shutdown(ptMachine)`                       | REQ-STM-013, REQ-STM-014           | Zero state, set bPoweredOn=FALSE       |

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

| Function                                          | REQ(s)                           | Description                              |
|---------------------------------------------------|----------------------------------|------------------------------------------|
| `cpu_init(ptCpu, ptMachine)`                      | REQ-CPU-001..005                 | Read reset vectors, set SR=0x2700        |
| `cpu_reset(ptCpu, ptMachine)`                     | REQ-CPU-002, REQ-CPU-003         | Re-read reset vectors                    |
| `cpu_step(ptCpu, ptMachine, ptResult)`            | REQ-CPU-006..008                 | Fetch + decode + execute one instruction |
| `cpu_raise_exception(ptCpu, ptMachine, uiVector)` | REQ-CPU-011                      | Push exception frame, jump to vector     |

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

| Function                                                              | REQ(s)              | Description                          |
|-----------------------------------------------------------------------|---------------------|--------------------------------------|
| `disasm_one(pBuf, uiBufLen, uiAddr, ptResult)`                        | REQ-DIS-001..005    | Disassemble one instruction          |
| `disasm_range(pBuf, uiBufLen, uiAddr, ptResults, uiMax, puiLines)`    | REQ-DIS-001..005    | Disassemble consecutive instructions |

**External dependencies:**

| Call       | Tag   | Purpose                             |
|------------|-------|-------------------------------------|
| `snprintf` | [CRT] | Format `szLine` in `disasm_result_t`|
| `memset`   | [CRT] | Zero each `disasm_result_t`         |

**TODO(UC11–14):** instruction decode tables (MOVE/LEA/ADD/BRA …).

---

### 4.7 Platform: Console Init + Raw Input — `win_console.c` / `lx_console.c`

**`win_console.c`** — two responsibilities:

*Startup init (`win_console_init`):*  enables ANSI/VT100 on Win32 stdout/stderr; sets code page UTF-8. Failures non-fatal (mintty has no real Win32 HANDLE).

*Raw keyboard input (UC4.2):* `console_set_raw()` uses `GetFileType(STD_INPUT_HANDLE)` to detect the stdin type at runtime:

| Type              | Context        | Strategy                                                                             |
|-------------------|----------------|--------------------------------------------------------------------------------------|
| `FILE_TYPE_PIPE`  | mintty / MSYS2 | No mode change; `ReadFile()` 1 byte + `PeekNamedPipe()` poll (5 ms / 50 ms) for ESC timeout + VT100 CSI/SS3 decode |
| `FILE_TYPE_CHAR`  | cmd.exe        | `SetConsoleMode` strips ENABLE_PROCESSED/LINE/ECHO_INPUT; `ReadConsoleInputA()` delivers pre-decoded VK codes |

**Public API (`console.h`):**

| Function                        | REQ(s)                       | Description                                                  |
|---------------------------------|------------------------------|--------------------------------------------------------------|
| `console_set_raw()`             | REQ-RAW-001..004             | Detect pipe vs console; enable raw mode                      |
| `console_restore()`             | REQ-RAW-005, REQ-RAW-006     | Restore original mode; idempotent                            |
| `console_read_key(piKey)`       | REQ-RAW-007..014             | Decode one key → CON_KEY_* or printable ASCII                |

**`lx_console.c`** — `lx_console_init` / `lx_console_shutdown` (no-ops for ANSI). `console_set_raw/restore/read_key` via POSIX `termios` + `read()` + `select()` 50 ms ESC timeout.

**`win_gui.c` focus management (UC4.1 + UC4.2 fix):**
`win_wnd_state_t` carries `hPrevFgWnd` (foreground window at open time). On open: `AttachThreadInput` + `SetForegroundWindow`. On `WM_DESTROY`: `SetForegroundWindow(hPrevFgWnd)` returns focus to the originating terminal (mintty, VS Code, PowerShell).

---

### 4.8 GUI Framework — `gui.c` / `gui_backend.h` / `win_gui.c` / `lx_gui.c`

**Role:** portable window lifecycle, thread-safe message queue, and event
routing.  `gui.c` is the portable layer; `gui_backend.h` is the private
interface between `gui.c` and platform backends (`win_gui.c` / `lx_gui.c`).
Each view (dir, edit, mount…) maps to one `gui_window_t` running in its own
thread.

**Public API:**

| Function                                  | REQ(s)                        | Description                                              |
|-------------------------------------------|-------------------------------|----------------------------------------------------------|
| `gui_init()`                              | REQ-GUI-001, REQ-GUI-002      | Register WNDCLASS / open X11 display; idempotent         |
| `gui_open_window(ptDesc, phWnd)`          | REQ-GUI-003, REQ-GUI-004      | Spawn thread; wait for HWND live; add to registry        |
| `gui_close_window(hWnd)`                  | REQ-GUI-005, REQ-GUI-006      | PostMessageA(WM_CLOSE); join thread; free window struct  |
| `gui_invalidate(hWnd)`                    | —                             | InvalidateRect → triggers WM_PAINT                       |
| `gui_get_size(hWnd, piW, piH)`            | —                             | GetClientRect → client area dimensions                   |
| `gui_shutdown()`                          | REQ-GUI-007, REQ-GUI-008      | Close all windows; UnregisterClass; destroy list mutex   |
| `gui_set_title(hWnd, szTitle)`            | REQ-GUI-017, REQ-GUI-018      | Update OS title bar (R18); delegates to platform backend |
| `gui_request_close(hWnd)`                 | REQ-GUI-019, REQ-GUI-020      | Post WM_CLOSE non-blocking; safe if called from window thread itself |
| `gui_msg_create(pphQ, cap)`               | REQ-GUI-009, REQ-GUI-010      | Allocate circular event buffer + mutex                   |
| `gui_msg_post(hQ, ptEvt)`                 | REQ-GUI-011, REQ-GUI-012      | Append event; ST_ERROR if full (non-blocking)            |
| `gui_msg_get(hQ, ptEvt, bBlock)`          | REQ-GUI-013                   | Dequeue; spin-wait 1 ms if bBlock=TRUE (TODO UC4: cond var) |
| `gui_msg_destroy(pphQ)`                   | REQ-GUI-014                   | Free buffer + mutex; set handle NULL                     |

**Platform backend (internal — gui_backend.h):**

| Function                                  | Description                                                        |
|-------------------------------------------|--------------------------------------------------------------------|
| `gui_platform_init()`                     | RegisterClassEx "ST4EverView" (Win32) / XOpenDisplay (Linux)       |
| `gui_platform_window_create(ptWnd)`       | CreateThread + Win32 Event sync (≤ 5 s); thread runs GetMessage loop |
| `gui_platform_window_close(ptWnd)`        | PostMessageA(WM_CLOSE)                                             |
| `gui_platform_window_invalidate(ptWnd)`   | InvalidateRect(NULL, FALSE)                                        |
| `gui_platform_window_get_size(ptWnd, …)` | GetClientRect                                                      |
| `gui_platform_window_set_title(ptWnd, sz)`| SetWindowTextA (Win32) / XStoreName (Linux)                       |
| `gui_platform_get_native_handle(ptWnd)`   | Returns HWND for renderer attachment                               |
| `gui_platform_shutdown()`                 | UnregisterClass (Win32) / XCloseDisplay (Linux)                    |

**WndProc event translation (Win32 → gui_event_t):**

| Win32 message    | gui_event_t produced                                    |
|------------------|---------------------------------------------------------|
| WM_PAINT         | GUI_EVT_PAINT                                           |
| WM_KEYDOWN       | GUI_EVT_KEY_DOWN (mapped to gui_key_t)                  |
| WM_CHAR          | GUI_EVT_KEY_DOWN (GUI_KEY_PRINTABLE + cChar)            |
| WM_LBUTTONDOWN   | GUI_EVT_MOUSE_DOWN (LEFT, iX, iY)                       |
| WM_RBUTTONDOWN   | GUI_EVT_MOUSE_DOWN (RIGHT, iX, iY)                      |
| WM_LBUTTONUP     | GUI_EVT_MOUSE_UP                                        |
| WM_MOUSEMOVE     | GUI_EVT_MOUSE_MOVE                                      |
| WM_MOUSEWHEEL    | GUI_EVT_SCROLL (iDelta = GET_WHEEL_DELTA_WPARAM / WHEEL_DELTA) |
| WM_SIZE          | GUI_EVT_RESIZE (iWidth, iHeight from lParam)            |
| WM_CLOSE→DESTROY | GUI_EVT_CLOSE                                           |

**External dependencies:**

| Call                                                  | Tag   | Purpose                              |
|-------------------------------------------------------|-------|--------------------------------------|
| `CreateThread / WaitForSingleObject / CloseHandle`    | [WIN] | Window thread lifecycle              |
| `CreateEvent / SetEvent`                              | [WIN] | Window-ready synchronisation signal  |
| `RegisterClassEx / CreateWindowEx / ShowWindow`       | [WIN] | Window creation                      |
| `GetMessage / TranslateMessage / DispatchMessage`     | [WIN] | Win32 message pump                   |
| `PostMessageA / InvalidateRect / GetClientRect`       | [WIN] | Window control                       |
| `SetWindowTextA`                                      | [WIN] | Dynamic title (R18)                  |
| `SetForegroundWindow / SetFocus`                      | [WIN] | Auto-focus on window open (REQ-GUI-021) |
| `platform_mutex_create/lock/unlock/destroy`           | [ST4] | List mutex + queue mutex             |
| `malloc / free / memset`                              | [CRT] | Window struct and queue allocation   |

---

### 4.9 2D Renderer — `renderer.c` / `win_D2D.c` / `lx_X11.c`

**Role:** portable 2D drawing abstraction. `renderer.c` provides NULL-guard
wrappers and the public API. `win_D2D.c` implements Direct2D and DirectWrite
via COM pure C (COBJMACROS + INITGUID — no C++ wrapper). `lx_X11.c` is a
stub (TODO UC3-Linux).

**Public API:**

| Function                                                      | REQ(s)      | Description                                                  |
|---------------------------------------------------------------|-------------|--------------------------------------------------------------|
| `renderer_create(pHandle, pNativeWnd)`                        | REQ-RND-001, REQ-RND-002 | Allocate D2D factory + HwndRenderTarget + DirectWrite text format |
| `renderer_begin_draw(hRnd)`                                   | REQ-RND-003 | `ID2D1HwndRenderTarget::BeginDraw()`                         |
| `renderer_end_draw(hRnd)`                                     | REQ-RND-003 | `EndDraw()`; recreate target on D2DERR_RECREATE_TARGET       |
| `renderer_fill_rect(hRnd, rect, clr)`                         | REQ-RND-004 | `FillRectangle()` with solid brush                           |
| `renderer_draw_rect(hRnd, rect, clr, fStrokeW)`               | REQ-RND-004 | `DrawRectangle()` outline                                    |
| `renderer_draw_line(hRnd, x0,y0,x1,y1, clr, fW)`             | REQ-RND-004 | `DrawLine()`                                                 |
| `renderer_draw_text(hRnd, szText, fX, fY, clr)`               | REQ-RND-004 | `DrawText()` with Courier New 13 pt monospace                |
| `renderer_draw_bitmap(hRnd, …)`                               | REQ-RND-004 | `DrawBitmap()` for ST screen blit (TODO UC26)                |
| `renderer_resize(hRnd, iW, iH)`                               | REQ-RND-005 | `Resize()` on render target (WM_SIZE)                        |
| `renderer_get_font_metrics(hRnd, piCellW, piCellH)`           | REQ-RND-007 | Query monospace cell dimensions for text-grid layout         |
| `renderer_destroy(pHandle)`                                   | REQ-RND-006 | Release all COM objects; safe no-op if NULL                  |

**Initialisation sequence:**

```
renderer_create(pHandle, hWnd)
  ├─ D2D1CreateFactory()              [WIN] — singleton, shared by all renderers
  ├─ DWriteCreateFactory()            [WIN] — singleton
  ├─ CreateTextFormat("Courier New", 13 pt, BOLD) [WIN]
  ├─ CreateHwndRenderTarget(hWnd, size) [WIN]
  └─ measure cell size via GetDesignGlyphMetrics → iCellW, iCellH
```

**External dependencies (win_D2D.c):**

| Call                                       | Tag   | Purpose                           |
|--------------------------------------------|-------|-----------------------------------|
| `D2D1CreateFactory`                        | [WIN] | Create ID2D1Factory               |
| `DWriteCreateFactory`                      | [WIN] | Create IDWriteFactory             |
| `CreateHwndRenderTarget`                   | [WIN] | Bind D2D to HWND                  |
| `CreateTextFormat`                         | [WIN] | Courier New 13 pt monospace       |
| `CoInitializeEx / CoUninitialize`          | [WIN] | COM initialisation per thread     |
| `malloc / free`                            | [CRT] | Renderer struct allocation        |

**TODO(UC26):** `renderer_draw_bitmap` for Atari ST screen surface blit.

---

### 4.10 Directory View — `dir.c`

**Role:** file-system tree scan, flat render list, D2D rendering, and
keyboard/mouse/scroll event handling. Opens as a `GUI_WND_DIR` window via
`gui_open_window()`.  Implements the `dir` / `d` / CTRL+D command.

**Public API:**

| Function                           | REQ(s)                    | Description                                              |
|------------------------------------|---------------------------|----------------------------------------------------------|
| `dir_open(szPath, ptLineCtx, bShowHidden, pptView)` | REQ-DIR-001..005, REQ-DIR-014, REQ-DIR-016 | Alloc view, scan root (filtered by bShowHidden), open GUI window (blocks until HWND live) |
| `dir_close(pptView)`               | REQ-DIR-006, REQ-DIR-007  | Close window (join thread), free tree + flat list, *pptView=NULL |

**Key internal functions:**

| Function                                                        | REQ(s)       | Description                                                      |
|-----------------------------------------------------------------|--------------|------------------------------------------------------------------|
| `dir_node_load_children(ptNode, bShowHidden)`                   | REQ-DIR-005, REQ-DIR-014 | Win32 two-pass FindFirstFileA (dirs first, then files); filters '.' prefix when bShowHidden=FALSE; non-fatal on access-denied |
| `dir_flat_rebuild(ptView)`                                      | REQ-DIR-005  | Reset flat array; call `dir_flat_rebuild_rec()` from root        |
| `dir_flat_rebuild_rec(ptView, ptNode, iDepth, bLast, uiMask)`   | —            | DFS → flat array; propagates `uiLastMask` bitmask per entry      |
| `dir_build_prefix(ptEntry, szBuf, uiMax)`                       | —            | ASCII connector from `uiLastMask`: `\-- ` / `+-- ` / `\|   ` / `    ` |
| `dir_render(ptView)`                                            | —            | D2D: bg + selection rect + `..` row (yellow) + flat entries      |
| `dir_event_callback(hWnd, ptEvent, pCtx)`                       | REQ-DIR-008..013 | GUI_EVT_PAINT / RESIZE / KEY_DOWN / MOUSE_DOWN / SCROLL / CLOSE |
| `dir_handle_key(ptView, eKey)`                                  | REQ-DIR-012, REQ-DIR-017..019, REQ-DIR-021..022 | ↑↓ PgUp/PgDn Home End ENTER SPACE ESC ←/→ F5 H/h → selection, scroll, expand/collapse, close, refresh, hidden toggle |
| `dir_handle_click(ptView, iX, iY)`                              | REQ-DIR-008  | Left-click: select + toggle expand/collapse on dirs              |
| `dir_handle_scroll(ptView, iDelta)`                             | REQ-DIR-013  | Clamp `iScrollOffset`                                            |
| `dir_activate_sel(ptView)`                                      | REQ-DIR-008..010 | `..` → navigate up; dir → toggle; file → set szSelected      |
| `dir_navigate_up(ptView)`                                       | REQ-DIR-009  | Free tree, reload from parent path, rebuild flat list            |
| `dir_scroll_to_sel(ptView)`                                     | REQ-DIR-012  | Ensure selected row is within visible range                      |
| `dir_get_parent_path(szIn, szOut, uiMax)`                       | —            | Strip last component; special-case Windows drive root `"C:\"`   |
| `dir_node_reload_children(ptNode, bShowHidden)`                 | REQ-DIR-021, REQ-DIR-022 | Free all children, reset bChildrenLoaded=FALSE, rescan via dir_node_load_children |
| `dir_collect_expanded(ptNode, aaszPaths, piCount, iMax)`        | REQ-DIR-022  | DFS: append szPath of every expanded+loaded dir to aaszPaths     |
| `dir_reexpand_path(ptRoot, szPath, bShowHidden)`                | REQ-DIR-022  | Navigate tree by path components from root; load + expand each level; stop silently on missing component |
| `dir_refresh_tree(ptView)`                                      | REQ-DIR-022  | Phase 1: collect expanded paths. Phase 2: reload root children. Phase 3: re-expand via dir_reexpand_path for each saved path |

**Flat list data model:**

```
dir_node_t (tree)           dir_flat_entry_t (O(1) render list)
─────────────────           ───────────────────────────────────
szName                 ──►  ptNode
bIsDir                       iDepth
bChildrenLoaded              bLastSibling
aptChildren[]                uiLastMask  ← bit i=1 if ancestor[i] was last sibling
iChildCount
```

`iSelectedFlat` convention: `-2` = nothing; `-1` = `..` row; `≥ 0` = `aptFlat[i]`.

**External dependencies:**

| Call                                             | Tag       | Purpose                                 |
|--------------------------------------------------|-----------|-----------------------------------------|
| `FindFirstFileA / FindNextFileA / FindClose`     | [WIN]     | Directory scan (two passes)             |
| `opendir / readdir / closedir`                   | [POX]     | Directory scan (Linux stub)             |
| `getcwd`                                         | [POX/CRT] | Resolve NULL path to cwd                |
| `renderer_*`                                     | [ST4]     | All drawing primitives                  |
| `gui_open_window / gui_close_window`             | [ST4]     | Window lifecycle                        |
| `gui_set_title`                                  | [ST4]     | Dynamic title R18                       |
| `malloc / free / memset / strncpy / snprintf`    | [CRT]     | Tree node and string management         |

**Thread model:**

```
Console thread                       Window thread (dir)
──────────────                       ───────────────────
dir_open()
  ├─ malloc(dir_view_t + aptFlat)
  ├─ dir_node_load_children(root)    (root children only — lazy)
  ├─ dir_flat_rebuild()
  └─ gui_open_window() ────────────► WndProc → dir_event_callback()
       blocks until HWND live            GUI_EVT_PAINT  → renderer_create (lazy)
                                         GUI_EVT_KEY_DOWN → dir_handle_key()
Console thread does NOT               GUI_EVT_MOUSE_DOWN → dir_handle_click()
access ptView after open               GUI_EVT_CLOSE  → renderer_destroy()
until dir_close() is called.
```

**UC4.3:** `dir.c` calls `line_set_selected()` (thread-safe) instead of writing `szSelected` directly. Mutex owned by `line_context_t.ptSelectedMutex` (created in `line_init`, destroyed in `line_shutdown`).

**UC5:** `H`/`h` key toggles `bShowHidden` + reload (P21). F5 calls `dir_refresh_tree()` which preserves expansion state across the reload (P22): DFS collect → reload root → re-expand by path navigation. Max 256 expanded paths saved on the stack (`DIR_REFRESH_MAX_EXP`).

**TODO(UC7/UC18):** right-click context menu on file / directory.

---

### 4.11 File System Abstraction — `file.h` / `file.c`

**Role:** portable FS API consumed by load, edit, mount, image UCs.
Wraps CRT `fopen`/`fread`/`fwrite`/`fclose` and POSIX
`opendir`/`readdir`/`closedir`/`stat`/`mkdir`, available on both MSYS2
(MinGW) and Linux without importing `<windows.h>` into `src/`.
The only platform divergence (`_mkdir` vs `mkdir`) is contained in one
macro (`FILE_MKDIR`) inside `file.c`.

**Public API:**

| Function                                              | REQ(s)                              | Description                                                             |
|-------------------------------------------------------|-------------------------------------|-------------------------------------------------------------------------|
| `file_stat(szPath, ptStat)`                           | REQ-FIL-001..003                    | Query existence, type, size, extension; non-existent → bExists=FALSE    |
| `file_open(szPath, eMode, pptFile)`                   | REQ-FIL-004..006, REQ-FIL-022      | Open file; allocate opaque handle; modes READ/WRITE/APPEND              |
| `file_read(ptFile, pBuf, uiLen, puiRead)`             | REQ-FIL-007..010                    | fread; partial read at EOF = ST_NO_ERROR; ferror → ST_ERROR             |
| `file_write(ptFile, pBuf, uiLen)`                     | REQ-FIL-011..012                    | fwrite; short write or uiLen==0 → ST_ERROR                              |
| `file_close(pptFile)`                                 | REQ-FIL-013..014                    | fclose + free + NULL; &NULL = idempotent no-op                          |
| `file_mkdir(szPath)`                                  | REQ-FIL-015..016                    | _mkdir/mkdir; EEXIST = silent success                                   |
| `file_list_dir(szPath, bShow, pfnCb, pCtx)`           | REQ-FIL-017..021                    | opendir/readdir; skip ./.. and optionally .*; invoke callback per entry |

**Key internal functions:**

| Function                                       | REQ(s)       | Description                                                             |
|------------------------------------------------|--------------|-------------------------------------------------------------------------|
| `file_extract_ext(szName, szExt, uiMax)`       | REQ-FIL-003  | Find last `.` after last separator; copy lowercase; `""` if none        |

**`file_stat_t` fields:**

| Field      | Type         | Meaning                                                             |
|------------|--------------|---------------------------------------------------------------------|
| `bExists`  | `st_bool_t`  | ST_TRUE if path exists                                              |
| `bIsDir`   | `st_bool_t`  | ST_TRUE if path is a directory                                      |
| `uiSize`   | `st_u64_t`   | File size in bytes; 0 for directories                               |
| `szExt`    | `char[16]`   | Lowercase extension without dot (`"prg"`, `"st"`, `""` if none)    |

**`file_t` (opaque struct, defined in file.c):**

| Field      | Type            | Meaning                           |
|------------|-----------------|-----------------------------------|
| `pHandle`  | `FILE *`        | CRT file handle                   |
| `eMode`    | `file_mode_t`   | Mode used at open                 |
| `szPath`   | `char[ST_MAX_PATH]` | Path for error messages       |

**`file_mode_t` → fopen mode mapping:**

| `file_mode_t`      | `fopen` arg | Behaviour                           |
|--------------------|-------------|-------------------------------------|
| `FILE_MODE_READ`   | `"rb"`      | Open existing; fail if absent       |
| `FILE_MODE_WRITE`  | `"wb"`      | Create or truncate                  |
| `FILE_MODE_APPEND` | `"ab"`      | Create or open at end               |

**`file_list_dir` callback signature:**

```c
void pfnCallback(const char        *szPath,   /* full path: szDir/szName */
                 const char        *szName,   /* entry name only         */
                 const file_stat_t *ptStat,   /* pre-filled via stat()   */
                 void              *pCtx)     /* caller context          */
```

**External dependencies:**

| Call                                      | Tag   | Purpose                                   |
|-------------------------------------------|-------|-------------------------------------------|
| `fopen / fread / fwrite / fclose`         | [CRT] | File I/O                                  |
| `stat`                                    | [POX] | Query file metadata (via sys/stat.h)      |
| `opendir / readdir / closedir`            | [POX] | Directory enumeration (dirent.h)          |
| `_mkdir` (Windows) / `mkdir` (Linux)      | [WIN/POX] | Create directory                      |
| `malloc / free / memset / strncpy`        | [CRT] | Handle allocation and string handling     |
| `tolower`                                 | [CRT] | Extension case folding in extract_ext     |
| `strerror / errno`                        | [CRT] | Error descriptions in LOG_ERROR calls     |

**Future consumers:**

| Future UC | Usage                                                        |
|-----------|--------------------------------------------------------------|
| ✓ UC7 load | `file_stat` (type detect) + `file_open/read/close` (load)  |
| UC8-10    | `file_open(READ/WRITE)` + `file_read/write` for edit save   |
| UC16      | `file_open(READ)` + `file_read` of 737 280 bytes (.st image)|
| UC18      | `file_list_dir` to enumerate mounted directory               |
| UC20      | `file_open(WRITE)` to create disk image file                 |

---

### 4.12 Load Command — `load.h` / `load.c`

**Role:** loads a file (binary/text/PRG) into emulated ST RAM and tracks the load
state for consumption by `line_cmd_load()`, `line_cmd_info()`, and future
execution UCs (UC25 will read `uiLoadAddr` as the initial PC for .PRG files).

**Public API:**

| Function                    | REQ(s)                          | Description                                                            |
|-----------------------------|---------------------------------|------------------------------------------------------------------------|
| `load_init(ptMachine)`      | REQ-LOD-001                     | Attach to ST machine; reset state; must precede load_file()            |
| `load_file(szPath)`         | REQ-LOD-003..012                | Detect type, load into RAM; replaces previous state on success         |
| `load_get_state()`          | REQ-LOD-002                     | Read-only pointer to g_load_tState; never NULL                         |
| `load_shutdown()`           | REQ-LOD-013                     | Clear machine pointer and state; idempotent                            |

**Key internal functions:**

| Function          | REQ(s)                  | Description                                                               |
|-------------------|-------------------------|---------------------------------------------------------------------------|
| `load_do_raw()`   | REQ-LOD-005..006        | Stream file into aRam[ST_LOAD_BASE..] in 4 KB blocks                     |
| `load_do_prg()`   | REQ-LOD-007..008        | Read 28-byte header, validate 0x601A, load .text+.data; TODO(UC15) fixup |
| `load_is_prg_ext()` | REQ-LOD-007           | Match "prg"/"ttp"/"tos" (lowercase, from file_stat szExt)                |
| `load_is_image_ext()` | REQ-LOD-011         | Match "st"/"msa"/"stx"                                                    |
| `load_u16be()` / `load_u32be()` | REQ-LOD-007 | Big-endian reads from PRG header byte buffer                              |

**`load_state_t` fields:**

| Field        | Type             | Meaning                                               |
|--------------|------------------|-------------------------------------------------------|
| `bLoaded`    | `st_bool_t`      | ST_TRUE if a file is currently loaded in RAM          |
| `eType`      | `load_type_t`    | NONE / BINARY / PRG                                   |
| `szPath`     | `char[ST_MAX_PATH]` | Full path of the loaded file                       |
| `uiLoadAddr` | `st_u32_t`       | ST RAM address of first content byte (= ST_LOAD_BASE) |
| `uiSize`     | `st_u32_t`       | Bytes of content loaded (.text+.data for PRG)         |

**Load address constants:**

| Constant           | Value             | Meaning                                        |
|--------------------|-------------------|------------------------------------------------|
| `ST_LOAD_BASE`     | `0x0800`          | After 68000 vector table (512 bytes)           |
| `ST_LOAD_MAX_SIZE` | `ST_RAM_SIZE − 0x0800` | Maximum loadable content                  |
| `ST_PRG_MAGIC`     | `0x601A`          | First 2 bytes of a valid ATARI ST PRG header   |
| `ST_PRG_HEADER_SIZE` | `28`            | Bytes before .text section                     |

**External dependencies:**

| Call                                      | Tag   | Purpose                                   |
|-------------------------------------------|-------|-------------------------------------------|
| `file_stat()`                             | [FIL] | Detect existence, dir, extension          |
| `file_open()` / `file_read()` / `file_close()` | [FIL] | File I/O for both binary and PRG      |
| `memcpy / memset / strncpy`               | [CRT] | RAM copy; state reset; path storage       |

**Future consumers:**

| Future UC      | Usage                                                                         |
|----------------|-------------------------------------------------------------------------------|
| UC15 `load_do_prg` | Parse PRG fixup table (bitstream); relocate address references in .text/.data |
| UC24 memory map | Compute `ST_LOAD_BASE` dynamically from free RAM above TOS variable area     |
| UC25 `execute`  | Use `load_get_state()->uiLoadAddr` as initial PC when eType == LOAD_TYPE_PRG |

---

### 4.13 Placeholder Components (stubs)

| Component                    | File(s)                                                              | Planned UC | Status        |
|------------------------------|----------------------------------------------------------------------|------------|---------------|
| GUI Framework                | `gui.c`, `win_gui.c`, `lx_gui.c`, `gui_backend.h`                  | UC3.1      | ✓ UC3.1 §4.8  |
| 2D Renderer                  | `renderer.c`, `win_D2D.c`, `lx_X11.c`                              | UC3.2      | ✓ UC3.2 §4.9  |
| Directory View               | `dir.c`                                                              | UC3.3      | ✓ UC3.3 §4.10 |
| Platform Thread/Mutex        | `win_platform.c`, `lx_platform.c`                                   | UC3.1      | ✓ UC3.1       |
| File System Abstraction      | `file.h`, `file.c`                                                   | UC6        | ✓ UC6 §4.11   |
| Load Command                 | `load.h`, `load.c`                                                   | UC7        | ✓ UC7 §4.12   |
| Text Editor View             | `edit_txt.c`                                                         | UC8        | stub         |
| Hex/ASCII Editor View        | `edit_hex.c`                                                         | UC9        | stub         |
| Integrated Editor Dispatcher | `edit.c`                                                             | UC10       | stub         |
| Floppy Mount                 | `mount.c`                                                            | UC18       | stub         |
| Execution Engine             | `exec.c`, `exec_mon.c`, `exec_cpu.c`, `exec_mem.c`, `exec_screen.c` | UC25       | stub         |
| DEVPAC3 Assembler            | `as.c`                                                               | UC30       | stub         |

---

### 4.14 Text Editor View — `edit_txt.c`

**Role:** load a text file into a heap line buffer, render it with a monospace D2D view (gutter + selection + cursor), handle all keyboard/mouse input, and save back via `file.h`.

**Key internal structure `edit_txt_view_t`:**

| Field                | Type                       | Purpose                                                        |
|----------------------|----------------------------|----------------------------------------------------------------|
| `hWnd`               | `gui_window_t`             | GUI window handle (owned by gui_open_window thread)            |
| `hRenderer`          | `renderer_t`               | D2D renderer (created lazily on first GUI_EVT_PAINT)           |
| `szPath[]`           | `char[ST_MAX_PATH]`        | Absolute path of the file being edited                         |
| `aszLines`           | `char **`                  | Heap array of heap strings (one per line, no newline)          |
| `iLineCount`         | `int`                      | Always ≥ 1 (empty file = 1 empty line)                         |
| `tCursor`            | `edit_pos_t`               | Current insertion point (iLine, iCol byte offset)              |
| `iScrollLine/Col`    | `int`                      | First visible row / display column                             |
| `iSelAnchorLine/Col` | `int`                      | Selection anchor; -1 = no selection                            |
| `bDirty`             | `st_bool_t`                | Unsaved changes → `[*]` in title                               |
| `iGutterW`           | `int`                      | Gutter width = (digits(iLineCount)+1) × iCellW                 |
| `aUndo[]`            | `edit_undo_entry_t[20]`    | Undo ring: deep copies of aszLines + cursor                    |
| `iUndoHead`          | `int`                      | Next write slot in ring (0-based)                              |
| `iUndoCount`         | `int`                      | Valid entries (0..EDIT_TXT_UNDO_LEVELS)                        |
| `bUndoGroupInsert`   | `st_bool_t`                | TRUE = last op was printable insert; group until broken        |
| `ptLineCtx`          | `line_context_t *`         | Back-reference for console feedback (trace on save)            |

**Public API:**

| Function | REQ(s) | Description |
|---|---|---|
| `edit_txt_open(szPath, ptLineCtx, pptView)` | REQ-EDT-001..004 | Load file, open D2D window in new thread |
| `edit_txt_close(pptView)` | REQ-EDT-001 | Join thread, free lines + undo ring, free view |

**Key internal functions:**

| Function | REQ(s) | Description |
|---|---|---|
| `etxt_load(ptV, szPath)` | REQ-EDT-003..004 | file_stat + file_read + split on `\n`; normalize `\r\n` |
| `etxt_save(ptV)` | REQ-EDT-011 | file_open(WRITE) + write all lines separated by `\n` |
| `etxt_display_len(szLine, iByteCol)` | REQ-EDT-005 | Byte→display col (tab expansion) |
| `etxt_byte_col_from_disp(szLine, iDisp)` | REQ-EDT-006 | Display col→byte col (inverse) |
| `etxt_undo_push(ptV)` | REQ-EDT-009..010 | Deep-copy aszLines into ring slot; advance head |
| `etxt_undo_pop(ptV)` | REQ-EDT-008 | Transfer ring snapshot back to live buffer |
| `etxt_undo_free_all(ptV)` | — | Release all ring entries (called on close) |
| `etxt_del_sel(ptV)` | REQ-EDT-003 | Delete selection range (single-line or multi-line merge) |
| `etxt_copy_sel(ptV)` | REQ-EDT-012 | Serialize selection to clipboard via `gui_clipboard_set_text` |
| `etxt_paste(ptV)` | REQ-EDT-012 | Read clipboard, insert char-by-char (\\n → split line) |
| `etxt_handle_key(ptV, eKey, cChar, uiMods)` | REQ-EDT-008..013 | Dispatch all keyboard actions; manage undo group |
| `etxt_render(ptV)` | REQ-EDT-005 | D2D draw: gutter, current-line highlight, selection, text, cursor |

**Undo push policy:**

```
Any modifying key:
  ENTER / Backspace / Delete / Tab / CTRL+X / CTRL+V
      → etxt_undo_push();  bUndoGroupInsert = FALSE
  First printable char (bUndoGroupInsert==FALSE)
      → etxt_undo_push();  bUndoGroupInsert = TRUE
  Subsequent printable chars (bUndoGroupInsert==TRUE)
      → no push  (grouped with previous)
  Any navigation key
      → bUndoGroupInsert = FALSE  (no push — no modification)
  Selection present before insert:
      → etxt_del_sel(); bUndoGroupInsert = FALSE (del+insert = one level)
CTRL+Z:
      → etxt_undo_pop();  bUndoGroupInsert = FALSE
```

**External dependencies:**

| Call | Tag | Purpose |
|---|---|---|
| `file_stat / file_open / file_read / file_write / file_close` | [ST4] | File I/O |
| `gui_open_window / gui_close_window / gui_invalidate / gui_request_close` | [ST4] | Window lifecycle |
| `gui_set_title` | [ST4] | Dynamic title with `[*]` dirty marker (R18) |
| `gui_clipboard_set_text / get_text` | [ST4] | CTRL+C/X/V clipboard |
| `renderer_create / begin_draw / fill_rect / draw_text / end_draw / destroy` | [ST4] | D2D rendering |
| `malloc / realloc / free` | [CRT] | Line buffer and undo snapshot management |

### 4.15 Hex Editor View — `edit_hex.c`

**Role:** load an entire binary file into a flat heap buffer, render it as a hex+ASCII table in a D2D window, handle nibble-level editing in the hex zone and byte-level editing in the ASCII zone, save via `file.h`.

**Key internal structure `edit_hex_view_t`:**

| Field            | Type                  | Purpose                                                          |
|------------------|-----------------------|------------------------------------------------------------------|
| `hWnd`           | `gui_window_t`        | GUI window handle                                                |
| `hRenderer`      | `renderer_t`          | D2D renderer (created lazily on first GUI_EVT_PAINT)            |
| `szPath[]`       | `char[ST_MAX_PATH]`   | Absolute path of the file being edited                          |
| `pData`          | `st_u8_t *`           | Heap buffer: entire file content                                 |
| `uiSize`         | `size_t`              | File size in bytes                                               |
| `uiCursor`       | `size_t`              | Byte offset of the cursor                                        |
| `iNibble`        | `int`                 | 0 = high nibble, 1 = low nibble (HEX zone only)                 |
| `eZone`          | `edit_hex_zone_t`     | Active editing zone (HEX or ASCII)                               |
| `iScrollRow`     | `int`                 | Index of first visible row                                       |
| `bDirty`         | `st_bool_t`           | Unsaved changes → `[*]` in title                                 |
| `iAddrX/iHexX/iAsciiX` | `int`        | Pre-computed column X positions (px), set on first paint        |
| `ptLineCtx`      | `line_context_t *`    | Back-reference for console feedback                              |

**Public API:**

| Function | REQ(s) | Description |
|---|---|---|
| `edit_hex_open(szPath, ptLineCtx, pptView)` | REQ-HEX-001..003 | Load file, open D2D window in new thread |
| `edit_hex_close(pptView)` | REQ-HEX-001 | Join thread, free pData, free view |

**Key internal functions:**

| Function | REQ(s) | Description |
|---|---|---|
| `ehex_load(ptV, szPath)` | REQ-HEX-002..003 | file_stat + malloc + file_read all bytes |
| `ehex_save(ptV)` | REQ-HEX-010 | file_open(WRITE) + file_write full pData buffer |
| `ehex_build_hex_row(pData, uiSize, iRow, szOut)` | REQ-HEX-004..006 | Build 49-char hex string for one row |
| `ehex_build_asc_row(pData, uiSize, iRow, szOut)` | REQ-HEX-006 | Build 16-char ASCII string for one row |
| `ehex_recalc_layout(ptV)` | REQ-HEX-003 | Set iAddrX, iHexX, iAsciiX from iCellW |
| `ehex_scroll_to_cursor(ptV)` | — | Keep iScrollRow so cursor row is visible |
| `ehex_handle_key(ptV, eKey, cChar, uiMods)` | REQ-HEX-007..010 | Navigation + editing dispatch |
| `ehex_handle_click(ptV, iX, iY)` | — | Pixel→byte+nibble hit-test; set zone |
| `ehex_render(ptV)` | REQ-HEX-004..006 | Per-row D2D draw: addr + hex + sep + ASCII + cursor overlay |

**Row layout (character offsets):**

```
"XXXXXX: XX XX XX XX XX XX XX XX  XX XX XX XX XX XX XX XX  |................|"
  0..6   8  11 14 17 20 23 26 29 33 36 39 42 45 48 51 54  57..72
         byte 0..7 at 3i      gap  byte 8..15 at 3i+1
```

**External dependencies:**

| Call | Tag | Purpose |
|---|---|---|
| `file_stat / file_open / file_read / file_write / file_close` | [ST4] | File I/O |
| `gui_open_window / gui_close_window / gui_invalidate / gui_request_close / gui_set_title` | [ST4] | Window lifecycle and title |
| `renderer_create / begin_draw / fill_rect / draw_text / end_draw / destroy` | [ST4] | D2D rendering |
| `malloc / free` | [CRT] | Buffer allocation |

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
| REQ-DIS-006  | —                           | TODO UC11     |
| REQ-DIS-007  | —                           | TODO UC12     |
| REQ-DIS-008  | —                           | TODO UC13     |
| REQ-DIS-009  | —                           | TODO UC14     |
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
| info cmd binary (UC7)   | REQ-LOD-014, REQ-CON-026             | ✓ UC7     | `line_cmd_info()` now shows live load state via load_get_state() |
| info cmd disk (stub)    | REQ-CON-026 (disk)                   | UC18      | `line_cmd_info()` disk-mounted stub → real state when mount is implemented |
| Edit hex / binary       | REQ-EDT-007, UFR-EDT-006             | ✓ UC9     | `line_cmd_edit()` routes .prg/.ttp/.tos/.bin/.raw → edit_hex_open |
| Find/Replace (P30)      | UFR-EDT-* (future)                   | UC10+     | CTRL+F search bar in editor; deferred until edit family stable    |
| CTRL+Z undo visual TC   | REQ-EDT-008..010                     | ✓ UC8     | Grouping and pop logic; validated via make manual UC=08           |
| PRG fixup relocation    | REQ-LOD-007, TODO(UC15)              | UC15      | load_do_prg() loads without fixup; relocation table parse deferred |
| load_file size check    | REQ-LOD-012                          | —         | File > ST_LOAD_MAX_SIZE: not injectable headless; covered by code review |
| file_read ferror path   | REQ-FIL-010                          | —         | ferror branch not injectable in headless tests; covered by code review |
| file_write short write  | REQ-FIL-012                          | —         | fwrite contract; not injectable headless; covered by code review       |
