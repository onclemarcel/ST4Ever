# ST4Ever — Document 2 — SR : Software Requirements

**Project:** ST4Ever — The Revival Engine for the Timeless ATARI ST
**Document:** 2 - SR.md — living document, updated at each validated Use Case (Phase 2)
**Language:** English
**Companion documents:**
- `CLAUDE.md` — architecture decisions, coding conventions, UC list
- `3 - TC.md` — Test Cases
- `6 - UC.md` — Behavioral contracts per UC, inter-UC dependency table

> **Traceability chain:** `UFR-[AREA]-NNN → REQ-[MOD]-NNN → Impl. (function) → TC-[MOD]-NNN (in 3-TC.md)`
>
> **§4 (Detailed Component Architecture) has been removed.** Function-level design is in `.h` docstrings (R17).
> From UC22 onward, §2 REQ tables include an `Impl.` column with the implementing function(s).
> For UC1–UC21, the function↔REQ mapping is in the archived `SRTD.md §4`.

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
| UFR-CON-070 | `mount [path]` shall emulate an Atari ST floppy drive A:\ from the given directory.             | ✓ UC18.1    | UC18.1 |
| UFR-CON-071 | `umount` shall eject the emulated floppy, offering to save a disk image if modified.             | ✓ UC19      | UC19   |
| UFR-CON-080 | `where` shall display the current working directory and the currently selected file/directory.   | ✓ UC5       | UC5  |
| UFR-CON-081 | `info` shall display a status dashboard: cwd, selected file, trace state, colors state, history count, mounted disk, loaded binary. | ✓ UC5 | UC5 |
| UFR-CON-082 | `history [N]` shall display the last N command history entries numbered in order (default N=10). | ✓ UC5       | UC5  |
| UFR-CON-083 | The OS window/terminal title bar shall be updated automatically after each command to reflect the current cwd, selection, and trace state. (P8) | ✓ UC5 | UC5 |
| UFR-CON-084 | On Tab with multiple completion candidates, the longest common prefix shall be inserted into the input buffer before cycling ghost text candidates. (P23bis) | ✓ UC5 | UC5 |
| UFR-CON-085 | ANSI colour output shall be auto-enabled or auto-disabled based on `isatty(stdout)` at startup; `colors on/off` shall remain available to override. (P24) | ✓ UC5 | UC5 |
| UFR-CON-090 | `execute <file>` shall open the full execution engine with all linked views.                     | TODO UC25   | UC25 |
| UFR-CON-091 | `image` shall create a `.st` or `.msa` disk image from the mounted floppy content.              | TODO UC20   | UC20 |
| UFR-CON-092 | `st2msa [--dir p] [--all] [-r]` shall convert `.st` disk image(s) to `.msa` format; `--all` processes all `.st` files in a directory; `-r` recurses into subdirectories. (P42) | ✓ UC20A | UC20A |
| UFR-CON-093 | `msa2st [--dir p] [--all] [-r]` shall convert `.msa` disk image(s) to `.st` format; `--all` processes all `.msa` files in a directory; `-r` recurses into subdirectories. (P42) | ✓ UC20A | UC20A |

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
| UFR-DIR-013 | When a file is committed via ENTER or SPACE in the dir view, a dark green secondary background (`g_dir_clrLastSel`) shall be rendered on that row in `dir_render()`, visually distinct from the navigation-cursor highlight. The indicator persists when the cursor moves; a new commit updates it. (P11) | ✓ UC7 | UC7 |
| UFR-DIR-014 | ALT+LEFT shall navigate to the previous path in the directory view's navigation history; ALT+RIGHT shall navigate to the next path. The history shall be seeded with the initial path at `dir_open()` time and updated each time the root changes via `dir_navigate_up()` or explicit navigation. The history stack is non-cyclical (max `DIR_NAV_HIST_MAX` entries). (P10) | ✓ UC18.2 | UC18.2 |
| UFR-DIR-015 | CTRL+SPACE shall toggle multi-selection of the currently focused **file** entry (not directories). The multi-selected set is displayed with a purple background (`g_dir_clrMultiSel`), distinct from the green last-committed indicator (P11) and the blue cursor highlight. The set holds up to `DIR_MULTI_SEL_MAX` paths. (P14) | ✓ UC18.2 | UC18.2 |

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
| UFR-LOD-004 | For ATARI ST executables (`.prg`/`.ttp`/`.tos`), `load` shall validate the `0x601A` magic, load the `.text`+`.data` sections at `ST_LOAD_BASE`, apply the fixup relocation table (when abs_flag=0), and report the result including the fixup count. | ✓ UC15 | UC7/UC15 |
| UFR-LOD-005 | The `info` command shall display the currently loaded binary name, type, ST load address, and byte count; if nothing is loaded, it shall display `(none)`. | ✓ UC7 | UC7 |
| UFR-LOD-006 | For `.prg`/`.ttp`/`.tos` files, `load` shall zero the `.bss` section in ST RAM immediately after the `.data` section; the BSS area is not present in the file and must be initialised to zero regardless of prior RAM content. | ✓ UC15 | UC15 |
| UFR-LOD-007 | For relocatable PRG files (abs_flag=0), `load` shall apply the Atari ST fixup relocation table: each longword address in `.text`+`.data` pointed to by the fixup table shall be incremented by `ST_LOAD_BASE`; a fixup offset that points outside `.text`+`.data` shall cause `load` to return an error without modifying the previously loaded state. | ✓ UC15 | UC15 |
| UFR-LOD-008 | For absolute PRG files (abs_flag=1), `load` shall skip fixup processing entirely; the fixup table is absent from the file. | ✓ UC15 | UC15 |

### 1.7 Editor Views — `EDT` (UC8 text editor ✓, UC9 hex+ASCII ✓, UC10 hex+disasm ✓)

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
| UFR-HEX-005 | The hex editor shall display a disassembly panel to the right of the ASCII column showing 68000 instructions decoded from the file content. The panel shall be toggled with F2 (default on). TAB shall cycle three zones: HEX → ASCII → DISASM → HEX. Navigation in the DISASM zone (↑↓/PgUp/PgDn) shall move by instruction and keep the hex cursor synchronised; conversely, cursor movement in HEX/ASCII shall keep the DISASM highlight synchronised. The DISASM zone is read-only. Clicking in the DISASM panel shall switch to DISASM zone and position the cursor at the clicked instruction. Mouse scroll in the DISASM zone shall scroll the disasm panel independently of the hex panel. | ✓ UC10 | UC10 |

### 1.8 Disk Image — `DSK` (UC16 .st ✓; UC17 .msa ✓; UC20 create TODO)

| ID          | Requirement                                                                                                                                      | Status    | UC   |
|-------------|--------------------------------------------------------------------------------------------------------------------------------------------------|-----------|------|
| UFR-DSK-001 | The application shall load a standard Atari ST DD 3.5" `.st` image file (737,280 bytes) into memory and make its FAT12 content accessible.      | ✓ UC16    | UC16 |
| UFR-DSK-002 | The application shall enumerate root directory entries of a loaded `.st` image, providing name, size, and attributes for each file.              | ✓ UC16    | UC16 |
| UFR-DSK-003 | The application shall read, write, and delete files in the root directory of an in-memory `.st` image using standard FAT12 8.3 naming rules.     | ✓ UC16    | UC16 |
| UFR-DSK-004 | The application shall save a modified in-memory `.st` image back to a file, producing a valid 737,280-byte image readable by external emulators. | ✓ UC16    | UC16 |
| UFR-DSK-005 | The application shall load a `.msa` (Magic Shadow Archiver) disk image file, decompress its per-track RLE data, and expose the same `image_st_t` interface as a `.st` image. | ✓ UC17 | UC17 |
| UFR-DSK-006 | The application shall save an in-memory `image_st_t` to a `.msa` file, compressing each track with MSA RLE when the compressed form is shorter than the raw 4608-byte track. | ✓ UC17 | UC17 |
| UFR-DSK-007 | The application shall provide a safe accessor `image_st_get_disk()` that exposes the raw byte buffer of an `image_st_t` to codec modules without breaking the opaque struct encapsulation. | ✓ UC17 | UC17 |

### 1.9 Floppy Mount / Emulation — `MNT`

> UC18.1 validated 2026-06-06. UC18.2 (drag-and-drop), UC19 (umount dialog), UC20 (image creation) planned.

| ID           | Requirement                                                                                                                                        | Status       | UC       |
|--------------|----------------------------------------------------------------------------------------------------------------------------------------------------|--------------|----------|
| UFR-MNT-001  | `mount [path]` shall open a D2D GUI view showing the FAT12 root directory of the given `.st` or `.msa` image, or of a PC directory converted to an in-memory disk. | ✓ UC18.1 | UC18.1 |
| UFR-MNT-002  | The mount view shall display a left panel listing FAT root entries (name, size) and a right panel showing disk properties (source type, total files, free bytes, modified flag). | ✓ UC18.1 | UC18.1 |
| UFR-MNT-003  | The mount view shall allow the user to add files from the PC file system via `mount_view_add_file()`, and delete selected entries via the DEL key; both actions shall update the in-memory image and refresh the view. | ✓ UC18.1 | UC18.1 |
| UFR-MNT-004  | `umount` shall close the mount view. If the in-memory image is dirty (modified since open), the user shall be notified (warning log). A full save dialog is planned for UC19. | ✓ UC18.1 | UC18.1 |
| UFR-MNT-005  | The mount view right panel shall display the BPB (BIOS Parameter Block) geometry fields read-only: heads, sectors/track, tracks, and the root directory entry capacity (RDE). The left panel header shall **not** include the `/RDE` suffix; that capacity is shown only in the right panel. (P34, P36) | ✓ UC18.2 | UC18.2 |
| UFR-MNT-006  | The mount view right panel shall display a "Bootable" indicator determined by the WD1772 checksum: the sum of the 256 LE16 words of the bootsector shall equal `0x1234 mod 0x10000`. `mount_is_bootable()` returns `ST_TRUE` if so, `ST_FALSE` otherwise (including `NULL` input). (P37) | ✓ UC18.2 | UC18.2 |
| UFR-MNT-007  | The `B` key in the mount view shall extract the 512-byte bootsector from `aDisk[0..511]` to a temporary file and open it via `edit_hex_open()`. The window title shall include a heuristic description (heads/sectors/tracks, bootable flag). Only one bootsector view shall be open at a time; pressing `B` again replaces the previous view. (P38) | ✓ UC18.2 | UC18.2 |
| UFR-MNT-008  | `umount` shall offer a dialog (or accept `--st`/`--msa`/`--dir [path]` flags) to save the in-memory disk image as `.st`, `.msa`, or an extracted directory when the image is dirty; a clean image shall close without any dialog. (P35) | ✓ UC19      | UC19     |
| UFR-MNT-009  | The mount view shall display a persistent status bar at the bottom of the window showing free space in KB, file count, and a `[*] unsaved` indicator when the image has been modified. The status bar occupies the last cell row; `iVisRows` is reduced by 2 (header + status bar). (P39) | ✓ UC19      | UC19     |
| UFR-MNT-010  | `image [--st|--msa] [--bootable] [path]` shall create a disk image from the currently mounted content or, when no mount view is open, from a specified directory; `--bootable` applies `mount_make_bootable()` before saving. | ✓ UC20      | UC20     |
| UFR-MNT-011  | ENTER on a selected file in the mount view shall extract the FAT entry to `MOUNT_FILE_TMP` and open it via `edit_hex_open()`; only one file hex view is open at a time; pressing ENTER on a new file closes the previous view first. (P41) | ✓ UC20      | UC20     |
| UFR-MNT-012  | The `F` key in the mount view shall patch bootsector word[0] so that the sum of the 256 LE16 words equals `0x1234 mod 0x10000`; the `Bootable` indicator shall update to `Yes` and the dirty flag shall be set. The operation is idempotent. (P37 write) | ✓ UC20      | UC20     |

### 1.10 Execution Engine — `EXE`

> Design ref: CLAUDE.md §1.1.7, §5 R2, §6.29; MC68000 Programmer's Reference Manual

| ID           | User/System Functional Requirement                                                                                                    | Status       | UC    |
|--------------|---------------------------------------------------------------------------------------------------------------------------------------|--------------|-------|
| UFR-EXE-001  | The CPU emulator shall fetch, decode, and execute MC68000 instructions from ST machine RAM, updating registers and SR flags per the Motorola specification. | ✓ UC22 (partial) | UC22 |
| UFR-EXE-002  | The CPU emulator shall implement the full 12-mode EA decoder: Dn, An, (An), (An)+, -(An), d16(An), d8(An,Xn), abs.W, abs.L, d16(PC), d8(PC,Xn), #imm. | ✓ UC21       | UC21  |
| UFR-EXE-003  | Byte and word writes to data registers shall preserve the unaffected upper bits; only MOVEQ and long-size operations replace all 32 bits. | ✓ UC21       | UC21  |
| UFR-EXE-004  | MOVEA shall not affect SR flags; MOVEA.W shall sign-extend the 16-bit source to 32 bits before writing An.                            | ✓ UC21       | UC21  |
| UFR-EXE-005  | The -(A7) and (A7)+ addressing modes with byte size shall adjust A7 by 2 (not 1) to maintain word alignment on the supervisor stack. | ✓ UC21       | UC21  |
| UFR-EXE-006  | The CPU emulator shall implement ADD/SUB/CMP/AND/OR/EOR/shifts with correct SR flags (N, Z, V, C, X).                                | ✓ UC22       | UC22  |
| UFR-EXE-007  | The CPU emulator shall implement BRA/Bcc/JSR/RTS/TRAP + exception vector dispatch.                                                   | ✓ UC23       | UC23  |
| UFR-EXE-008  | The execution monitor shall provide step, run, stop, and breakpoint controls with a register/memory display view.                     | TODO UC25    | UC25  |

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
| REQ-CON-032  | `line_cmd_st2msa()` and `line_cmd_msa2st()` shall dispatch to `line_cmd_convert()` with the appropriate source/destination extension pair. `line_cmd_convert()` shall: without `--all`, convert the single file given as argument or selected via `dir` (warn if no selection); with `--all`, scan the directory (from `--dir`, selection, or cwd) and convert every matching file. Files with the wrong extension shall produce a warning and `ST_NO_ERROR` (non-fatal). | UFR-CON-092, UFR-CON-093 | ✓ UC20A | UC20A |
| REQ-CON-033  | `line_conv_one(szSrc, szDst, szSrcExt, szDstExt)` shall verify the source extension via `file_stat().szExt`; `.st` → `image_st_load` + `image_msa_save`; `.msa` → `image_msa_load` + `image_st_save`; on failure it shall close any partially-loaded image and return `ST_ERROR` without leaving a corrupted destination file. | UFR-CON-092, UFR-CON-093 | ✓ UC20A | UC20A |
| REQ-CON-034  | `line_conv_rec_cb` shall have the `file_list_fn` signature (`void`, `const file_stat_t *`); it shall recurse into subdirectories if `bRecurse == ST_TRUE`; individual file failures shall increment `iFailed` without aborting the batch. | UFR-CON-092, UFR-CON-093 | ✓ UC20A | UC20A |

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
> Parent UFR: UFR-EXE-001..008 (§1.10)

| ID          | Software Requirement                                                                                                                          | Parent UFR    | Status        | UC    |
|-------------|-----------------------------------------------------------------------------------------------------------------------------------------------|---------------|---------------|-------|
| REQ-CPU-001 | `cpu_init(NULL, *)` and `cpu_init(*, NULL)` shall return `ST_ERROR`.                                                                         | UFR-EXE-001   | ✓ UC1         | UC1   |
| REQ-CPU-002 | `cpu_init()` shall read the initial SSP from the reset vector at address `0x000000`.                                                          | UFR-EXE-001   | ✓ UC1         | UC1   |
| REQ-CPU-003 | `cpu_init()` shall read the initial PC from the reset vector at address `0x000004`.                                                           | UFR-EXE-001   | ✓ UC1         | UC1   |
| REQ-CPU-004 | `cpu_init()` shall set the SR supervisor bit (`CPU_SR_S`) and interrupt mask to 7 (`SR = 0x2700`).                                           | UFR-EXE-001   | ✓ UC1         | UC1   |
| REQ-CPU-005 | `cpu_init()` shall set `eState = CPU_STATE_RUNNING`.                                                                                          | UFR-EXE-001   | ✓ UC1         | UC1   |
| REQ-CPU-006 | `cpu_step(NULL, *, *)` and `cpu_step(*, NULL, *)` shall return `ST_ERROR`.                                                                    | UFR-EXE-001   | ✓ UC1         | UC1   |
| REQ-CPU-007 | `cpu_step()` shall fetch the opcode word at the current PC and return it in `ptResult->uiOpcode`.                                            | UFR-EXE-001   | ✓ UC1         | UC1   |
| REQ-CPU-008 | Stub: `cpu_step()` advances PC by 2; ADAPTED UC21: `cpu_step()` advances PC by the full instruction length (opcode + extension words).       | UFR-EXE-001   | ADAPTED(UC21) | UC21  |
| REQ-CPU-009 | `cpu_step()` shall decode and execute MOVE.B/W/L, MOVEA.W/L, MOVEQ, LEA, CLR.B/W/L, SWAP.                                                   | UFR-EXE-001   | ✓ UC21        | UC21  |
| REQ-CPU-010 | `cpu_step()` shall decode and execute ADD/ADDA/ADDI/ADDQ/ADDX, SUB/SUBA/SUBI/SUBQ/SUBX, CMP/CMPA/CMPI/CMPM, AND/ANDI, OR/ORI, EOR/EORI, shifts/rotations, with correct SR flags N/Z/V/C/X. | UFR-EXE-006   | ✓ UC22        | UC22  |
| REQ-CPU-011 | `cpu_step()` shall decode and execute BRA/BSR/Bcc (short+long), JMP, JSR/RTS/RTR/RTE, TRAP, LINK/UNLK + exception vector dispatch.          | UFR-EXE-007   | ✓ UC23        | UC23  |
| REQ-CPU-012 | `cpu_ea_read()` shall implement all 12 EA modes; (An)+ shall increment An after the read; -(An) shall decrement An before.                   | UFR-EXE-002   | ✓ UC21        | UC21  |
| REQ-CPU-013 | For (A7)+ and -(A7) with byte size, A7 shall be adjusted by 2 (not 1) to preserve word alignment.                                           | UFR-EXE-005   | ✓ UC21        | UC21  |
| REQ-CPU-014 | `cpu_ea_write()` with Dn destination and byte size shall preserve bits 31–8; with word size shall preserve bits 31–16.                       | UFR-EXE-003   | ✓ UC21        | UC21  |
| REQ-CPU-015 | MOVE with An destination shall be decoded as MOVEA and shall not modify SR flags.                                                             | UFR-EXE-004   | ✓ UC21        | UC21  |
| REQ-CPU-016 | MOVEA.W shall sign-extend the 16-bit source value to 32 bits before writing the address register.                                            | UFR-EXE-004   | ✓ UC21        | UC21  |
| REQ-CPU-017 | MOVEQ shall sign-extend the 8-bit immediate in bits 7–0 of the opcode to 32 bits and write the full Dn; bit 8 of the opcode must be 0.       | UFR-EXE-001   | ✓ UC21        | UC21  |
| REQ-CPU-018 | CLR shall write 0 to the destination EA and unconditionally set N=0, Z=1, V=0, C=0.                                                          | UFR-EXE-001   | ✓ UC21        | UC21  |
| REQ-CPU-019 | LEA shall use control-mode EA only (modes 2/5/6/7.0/7.1/7.2/7.3) and write the computed address to An without modifying SR.                 | UFR-EXE-001   | ✓ UC21        | UC21  |
| REQ-CPU-020 | SWAP shall exchange bits 31–16 and bits 15–0 of Dn, then set N and Z from the 32-bit result and clear V and C.                              | UFR-EXE-001   | ✓ UC21        | UC21  |
| REQ-CPU-021 | MOVE/CLR shall set N from bit `MSB(size)` of the result, Z if the result equals 0, and clear V and C.                                        | UFR-EXE-001   | ✓ UC21        | UC21  |
| REQ-CPU-022 | `cpu_flags_add(src,dst,res,sz)`: C = unsigned carry (res < src \|\| res < dst); V = both operands same sign and result different sign; X = C; N/Z from masked result. | UFR-EXE-006   | ✓ UC22        | UC22  |
| REQ-CPU-023 | `cpu_flags_sub(src,dst,res,sz)` (dst−src=res): C = borrow (src > dst unsigned); V = operands different signs and result sign ≠ dst; X = C; N/Z from masked result. | UFR-EXE-006   | ✓ UC22        | UC22  |
| REQ-CPU-024 | ADDX/SUBX/NEGX shall use Z "not-cleared-if-zero" semantics: Z is only cleared when the result is non-zero, never set by these instructions. | UFR-EXE-006   | ✓ UC22        | UC22  |
| REQ-CPU-025 | NEG(0) shall produce result=0, C=0, Z=1 (no borrow when subtracting 0 from 0). | UFR-EXE-006   | ✓ UC22        | UC22  |
| REQ-CPU-026 | ADDA/SUBA shall not modify SR flags; they operate on the full 32-bit An. CMPA shall compute flags using the full 32-bit An as destination. | UFR-EXE-006   | ✓ UC22        | UC22  |
| REQ-CPU-027 | MULU.W shall perform unsigned 16×16→32 multiplication into Dn. MULS.W shall perform signed 16×16→32 multiplication. Both set N/Z, clear V/C. | UFR-EXE-006   | ✓ UC22        | UC22  |
| REQ-CPU-028 | DIVU/DIVS shall place the quotient in bits 15–0 of Dn and the remainder in bits 31–16. Division by zero shall be a non-fatal LOG_TODO (UC23 exception). | UFR-EXE-006   | ✓ UC22        | UC22  |
| REQ-CPU-029 | Shift count 0 in the immediate field of group-E instructions shall be interpreted as 8. Register-mode shift count shall use `Dn & 63`. | UFR-EXE-006   | ✓ UC22        | UC22  |
| REQ-CPU-030 | ASR shall preserve the MSB (sign extension) at each step. LSR shall insert 0. Both update C and X from the last bit shifted out. | UFR-EXE-006   | ✓ UC22        | UC22  |
| REQ-CPU-031 | RO(L/R) shall rotate without affecting X; ROX(L/R) shall rotate through X, updating both C and X from the transferred bit. | UFR-EXE-006   | ✓ UC22        | UC22  |
| REQ-CPU-032 | BRA (cc=0) shall branch unconditionally by the signed displacement; BSR (cc=1) shall push the return PC on the supervisor stack then jump; disp8=0 means fetch a word extension for the displacement. Impl: `cpu_exec_branch()` | UFR-EXE-007   | ✓ UC23        | UC23  |
| REQ-CPU-033 | Bcc (cc=2..15) shall branch if the 68000 condition is true (using N, Z, V, C flags per the MC68000 condition code table); not-taken Bcc shall advance PC past the displacement word only. Impl: `cpu_exec_branch()`, `cpu_eval_cc()` | UFR-EXE-007   | ✓ UC23        | UC23  |
| REQ-CPU-034 | JMP shall transfer control to the EA computed address without saving a return address; JSR shall push the PC of the instruction following the JSR onto the supervisor stack (as a long-word), then jump to the EA. Impl: `cpu_exec_misc4e()` | UFR-EXE-007   | ✓ UC23        | UC23  |
| REQ-CPU-035 | RTS shall pop a long-word from the supervisor stack into PC. RTR shall pop a word into the CCR (low byte of SR), then pop a long-word into PC, preserving the supervisor/interrupt bits of SR. Impl: `cpu_exec_misc4e()` | UFR-EXE-007   | ✓ UC23        | UC23  |
| REQ-CPU-036 | RTE shall pop a word into SR then pop a long-word into PC from the supervisor stack; the exception frame layout is [SP+0..+1]=SR, [SP+2..+5]=PC (big-endian). Impl: `cpu_exec_misc4e()` | UFR-EXE-007   | ✓ UC23        | UC23  |
| REQ-CPU-037 | NOP shall advance PC by 2 and leave all registers and flags unchanged. Impl: `cpu_exec_misc4e()` | UFR-EXE-007   | ✓ UC23        | UC23  |
| REQ-CPU-038 | STOP shall load the word extension into SR and set `eState = CPU_STATE_STOPPED`; subsequent `cpu_step()` calls on a non-RUNNING CPU shall return `ST_NO_ERROR` as a no-op. Impl: `cpu_exec_misc4e()` | UFR-EXE-007   | ✓ UC23        | UC23  |
| REQ-CPU-039 | TRAP #n shall invoke `cpu_raise_exception()` with vector address `CPU_VEC_TRAP(n)` (0x0080 + 4n). Impl: `cpu_exec_misc4e()` | UFR-EXE-007   | ✓ UC23        | UC23  |
| REQ-CPU-040 | `cpu_raise_exception()` shall: (1) save current SR; (2) if in user mode swap auAn[7]↔uiSSP and set CPU_SR_S; (3) push PC as long-word on supervisor stack; (4) push saved SR as word on supervisor stack; (5) read handler address from vector table and load into PC. On SSP underflow `eState = CPU_STATE_HALTED`. Impl: `cpu_raise_exception()` | UFR-EXE-007   | ✓ UC23        | UC23  |
| REQ-CPU-041 | LINK An,#disp shall push An as a long-word on the supervisor stack, copy SP to An, then add the signed word displacement to SP. Impl: `cpu_exec_misc4e()` | UFR-EXE-007   | ✓ UC23        | UC23  |
| REQ-CPU-042 | UNLK An shall copy An to SP, then pop the long-word at the new SP into An. Impl: `cpu_exec_misc4e()` | UFR-EXE-007   | ✓ UC23        | UC23  |
| REQ-CPU-043 | DBcc (group 0x5, size=3, mode=1) shall decrement Dn.W; if the condition is false AND Dn.W ≠ −1 then branch by the signed word displacement (base = address of extension word); fall-through when the condition is true or the counter reaches −1. Impl: `cpu_exec_group5()` | UFR-EXE-007   | ✓ UC23        | UC23  |
| REQ-CPU-044 | Scc (group 0x5, size=3, mode≠1) shall write 0xFF to the destination byte EA if the 68000 condition is true, 0x00 if false; SR flags shall not be modified. Impl: `cpu_exec_group5()` | UFR-EXE-007   | ✓ UC23        | UC23  |
| REQ-CPU-045 | MOVEM opcode space (0x4880/0x48C0/0x4C80/0x4CC0) shall be dispatched after the EXT check (same upper byte) to avoid false matches on mode=0 EA. Impl: `cpu_exec_misc4()`, `cpu_exec_movem()` | UFR-EXE-007   | ✓ UC23-bis    | UC23-bis |
| REQ-CPU-046 | MOVEM.L regs,-(An) shall use the reversed register mask (bit 0=A7…bit 7=A0, bit 8=D7…bit 15=D0), pre-decrement An for each set bit, and write the pre-snapshotted register value. Impl: `cpu_exec_movem()` | UFR-EXE-007   | ✓ UC23-bis    | UC23-bis |
| REQ-CPU-047 | MOVEM.L (An)+,regs shall use the standard register mask (bit 0=D0…bit 7=D7, bit 8=A0…bit 15=A7), post-increment An for each set bit. Impl: `cpu_exec_movem()` | UFR-EXE-007   | ✓ UC23-bis    | UC23-bis |
| REQ-CPU-048 | MOVEM to/from control EA (not pre-decrement or post-increment) shall compute the EA address once then advance sequentially by iStep per register. Impl: `cpu_exec_movem()` | UFR-EXE-007   | ✓ UC23-bis    | UC23-bis |
| REQ-CPU-049 | MOVEM.W loads shall sign-extend each 16-bit memory value to 32 bits before storing in the destination register. Impl: `cpu_exec_movem()` | UFR-EXE-007   | ✓ UC23-bis    | UC23-bis |
| REQ-CPU-050 | ADDA.W (iDir=0, iSzCode=3 in group D) shall read a 16-bit source EA, sign-extend it to 32 bits, and add to An without modifying SR. SUBA.W (iDir=0, iSzCode=3 in group 9) shall do the same but subtract. Impl: `cpu_exec_groupD()`, `cpu_exec_group9()` | UFR-EXE-007   | ✓ UC23-bis    | UC23-bis |

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
| REQ-DIR-024  | `dir_open()` shall seed `aszNavHistory[0]` with the initial root path; `iNavHistHead = 0`, `iNavHistCount = 1`. On each navigation that changes the root path (navigate-up or explicit navigate), `dir_nav_history_push()` shall append the new path before calling `dir_navigate_to()`. `dir_navigate_to()` shall not push by itself. | UFR-DIR-014 | ✓ UC18.2 | UC18.2 |
| REQ-DIR-025  | ALT+LEFT in `dir_handle_key()` (detected via `uiMods & GUI_MOD_ALT`): if `iNavHistHead > 0`, decrement `iNavHistHead` and call `dir_navigate_to(aszNavHistory[iNavHistHead])`. ALT+RIGHT: if `iNavHistHead < iNavHistCount - 1`, increment `iNavHistHead` and call `dir_navigate_to(...)`. At the boundary: no-op. | UFR-DIR-014 | ✓ UC18.2 | UC18.2 |
| REQ-DIR-026  | CTRL+SPACE in `dir_handle_key()` (detected via `uiMods & GUI_MOD_CTRL`): if the focused entry is a file (not a directory, not `..`), call `dir_toggle_multi_sel(ptView, szPath)` and redraw. `dir_toggle_multi_sel()` adds the path if not present and `iMultiSelCount < DIR_MULTI_SEL_MAX`; removes it with shift-down if already present. Directories are never added to the multi-sel set. | UFR-DIR-015 | ✓ UC18.2 | UC18.2 |
| REQ-DIR-027  | `dir_render()` shall draw a purple layer (`g_dir_clrMultiSel`) on each file row whose path is in `aszMultiSel[]`, between the green P11 layer and the blue cursor layer. `dir_open()` shall initialise `iMultiSelCount = 0`. | UFR-DIR-015 | ✓ UC18.2 | UC18.2 |
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
| REQ-LOD-015  | `load_do_prg()` shall read the 28-byte PRG header fields in big-endian order: `uiTextSize` at offset 2, `uiDataSize` at offset 6, `uiBssSize` at offset 10, `uiSymSize` at offset 14, `uiAbsFlag` at offset 26. | UFR-LOD-004 | ✓ UC15 | UC15 |
| REQ-LOD-016  | After loading `.text`+`.data`, `load_do_prg()` shall skip `uiSymSize` bytes via `load_skip_bytes()` to position the file cursor at the start of the fixup table. | UFR-LOD-004 | ✓ UC15 | UC15 |
| REQ-LOD-017  | When `uiAbsFlag == 0`, `load_do_prg()` shall call `load_apply_fixups()`; when `uiAbsFlag != 0`, fixup processing shall be skipped and `uiFixupCount` shall remain 0. | UFR-LOD-007, UFR-LOD-008 | ✓ UC15 | UC15 |
| REQ-LOD-018  | `load_do_prg()` shall zero `uiBssSize` bytes in ST RAM at `ST_LOAD_BASE + uiTextSize + uiDataSize` if `uiBssSize > 0`, regardless of prior RAM content. | UFR-LOD-006 | ✓ UC15 | UC15 |
| REQ-LOD-019  | `load_state_t` for a PRG load shall expose `uiTextSize`, `uiDataSize`, `uiBssSize` (values from the header, 0 for LOAD_TYPE_BINARY) and `uiFixupCount` (number of fixups applied, 0 for abs or no-fixup). `uiSize` shall equal `uiTextSize + uiDataSize + uiBssSize`. | UFR-LOD-004, UFR-LOD-006 | ✓ UC15 | UC15 |
| REQ-LOD-020  | `load_apply_fixups()` shall read the first 4-byte big-endian offset; a value of 0 shall mean "no fixups" and return immediately with ST_NO_ERROR. For each subsequent fixup: validate `uiOffset + 4 <= uiContentSize` (else ST_ERROR), read the longword from `aRam[ST_LOAD_BASE + uiOffset]`, add `ST_LOAD_BASE`, write back big-endian, increment `*puiCount`, read next byte: `0x00`=end, `0x01`=advance 254, else advance N. | UFR-LOD-007 | ✓ UC15 | UC15 |
| REQ-LOD-021  | A fixup offset that causes `uiOffset + 4 > uiContentSize` shall cause `load_apply_fixups()` to return `ST_ERROR` with `LOG_ERROR`; the previously-loaded state shall not be modified (state update in `load_do_prg()` happens only after successful fixup application). | UFR-LOD-007 | ✓ UC15 | UC15 |
| REQ-LOD-022  | EOF during fixup table traversal without an explicit `0x00` terminator shall be treated as end-of-list (break); `uiFixupCount` reflects the fixups actually applied before EOF. | UFR-LOD-007 | ✓ UC15 | UC15 |

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

> Design ref: CLAUDE.md §1.1.5, §6.14, §6.15; depends on `file.h` (UC6), `gui.h` (UC3.1), `renderer.h` (UC3.2), `disassemble.h` (UC1 stub → UC11–14 real)

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
| REQ-HEX-011  | Disasm panel constants shall be: `EDIT_HEX_DISASM_CACHE_LINES=512`, `EDIT_HEX_DISASM_PANEL_CHARS=48`, `EDIT_HEX_DISASM_SEP_CHARS=3`, `EDIT_HEX_DISASM_ADDR_CHARS=8`, `EDIT_HEX_DISASM_PREBUF_BYTES=512`. | UFR-HEX-005 | ✓ UC10 | UC10 |
| REQ-HEX-012  | `HEX_ZONE_DISASM` shall be a third distinct zone value (2) in `edit_hex_zone_t`; existing values `HEX_ZONE_HEX=0` and `HEX_ZONE_ASCII=1` shall be unchanged. | UFR-HEX-005 | ✓ UC10 | UC10 |
| REQ-HEX-013  | `disasm_range()` called on the file buffer with `uiAddr = uiStartByte` shall return instruction results whose `.uiAddr` fields form a monotonically increasing sequence matching the byte offsets consumed. | UFR-HEX-005 | ✓ UC10 | UC10 |
| REQ-HEX-014  | The disasm cache shall be rebuilt (via `ehex_disasm_cache_update()`) after any cursor movement in HEX or ASCII zone and after any byte edit. The cache window shall start at `max(0, uiCursor − PREBUF_BYTES) & ~1`. | UFR-HEX-005 | ✓ UC10 | UC10 |
| REQ-HEX-015  | Moving the cursor in the DISASM zone shall update `uiCursor` to the start byte of the selected instruction and call `ehex_scroll_to_cursor()` to keep the hex panel in sync. | UFR-HEX-005 | ✓ UC10 | UC10 |
| REQ-HEX-016  | Moving the cursor in HEX or ASCII zone shall call `ehex_disasm_find_cursor()` to update `iDisasmCursorIdx` and `ehex_disasm_scroll_to_cursor()` to keep the disasm panel in sync. | UFR-HEX-005 | ✓ UC10 | UC10 |
| REQ-HEX-017  | F2 shall toggle `bShowDisasm`; if the active zone is `HEX_ZONE_DISASM` when F2 is pressed, the zone shall revert to `HEX_ZONE_HEX`. | UFR-HEX-005 | ✓ UC10 | UC10 |
| REQ-HEX-018  | TAB shall cycle zones HEX → ASCII → DISASM (if `bShowDisasm`) → HEX; when `bShowDisasm` is FALSE TAB shall toggle between HEX and ASCII only. | UFR-HEX-005 | ✓ UC10 | UC10 |
| REQ-HEX-019  | Mouse scroll in `HEX_ZONE_DISASM` shall update `iDisasmScrollRow`; in any other zone it shall update `iScrollRow` (hex panel). Both shall be clamped to valid range. | UFR-HEX-005 | ✓ UC10 | UC10 |
| REQ-HEX-020  | `edit_hex_open()` shall allocate `atDisasmCache` heap and set `bShowDisasm=TRUE`; `edit_hex_close()` shall free it. If the allocation fails, `bShowDisasm` shall be `ST_FALSE` and the window shall still open. | UFR-HEX-005 | ✓ UC10 | UC10 |

---

### 2.14 Disk Image — `image_st.h` / `image_st.c`

> Design ref: CLAUDE.md §1.1.6, §6.21; depends on `file.h` (UC6)

| ID           | Software Requirement                                                                                                                                                                                                                          | Parent UFR  | Status   | UC   |
|--------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|-------------|----------|------|
| REQ-IST-001  | `image_st_create(NULL)` shall return `ST_ERROR`. `image_st_create(&p)` shall allocate, zero, and format a blank 737,280-byte image: BPB at boot sector offset +11 (BPS=512, SPC=2, RES=1, NFATS=2, RDE=112, TS=1440, MEDIA=0xF9, SPF=5, SPT=9, HEADS=2); FAT1 and FAT2 cluster 0 = IST_FAT_MEDIA_BYTE (0xFF9), cluster 1 = 0xFFF; root directory zeroed. | UFR-DSK-001 | ✓ UC16 | UC16 |
| REQ-IST-002  | `image_st_close(NULL)` shall return `ST_ERROR`. `image_st_close(&NULL)` shall return `ST_NO_ERROR` (idempotent no-op). After a successful close, `*pptImg` shall be NULL and the heap block freed. | UFR-DSK-001 | ✓ UC16 | UC16 |
| REQ-IST-003  | `image_st_load(NULL,*)` or `image_st_load(*,NULL)` shall return `ST_ERROR`. Loading a non-existent or unreadable file shall return `ST_ERROR`. If `file_read()` delivers fewer than IST_DISK_SIZE bytes, `image_st_load()` shall return `ST_ERROR`. | UFR-DSK-001 | ✓ UC16 | UC16 |
| REQ-IST-004  | After successful `file_read()`, `image_st_load()` shall validate the BPB: the bytes-per-sector field (offset +0x0B, LE16) must equal 512 and the total-sectors field (offset +0x13, LE16) must equal 1440; any divergence shall return `ST_ERROR`. | UFR-DSK-001 | ✓ UC16 | UC16 |
| REQ-IST-005  | `image_st_save(NULL,*)` or `image_st_save(*,NULL)` shall return `ST_ERROR`. On success the output file shall contain exactly IST_DISK_SIZE bytes. | UFR-DSK-004 | ✓ UC16 | UC16 |
| REQ-IST-006  | `image_st_list_root()` shall return `ST_ERROR` if any pointer parameter is NULL or `iMaxEntries` ≤ 0. It shall skip deleted entries (first byte = 0xE5) and volume-label entries (`IST_ATTR_VOLNAME` set). Enumeration shall stop at the first unused entry (first byte = 0x00). | UFR-DSK-002 | ✓ UC16 | UC16 |
| REQ-IST-007  | `image_st_read_file()` shall return `ST_ERROR` if `ptImg` or `pBuf` is NULL, or if `uiSize > uiBufSize`. It shall return `ST_ERROR` if any cluster in the FAT12 chain is out of range (< IST_CLUSTER_FIRST or ≥ IST_CLUSTER_FIRST + IST_DATA_CLUSTERS) or if the chain ends (≥ IST_FAT_EOC) before `uiSize` bytes have been delivered. | UFR-DSK-003 | ✓ UC16 | UC16 |
| REQ-IST-008  | `image_st_write_file()` shall return `ST_ERROR` if `ptImg` or `szName` is NULL, or if `pData` is NULL and `uiSize > 0`. The name shall be validated as 8.3 (base 1–8 chars, extension 0–3 chars) and normalised to uppercase; an invalid name shall return `ST_ERROR`. | UFR-DSK-003 | ✓ UC16 | UC16 |
| REQ-IST-009  | If a file with the same 8.3 name (case-insensitive) already exists, `image_st_write_file()` shall free its FAT chain and mark its directory entry deleted (0xE5) before allocating the replacement; the root entry count visible through `image_st_list_root()` shall remain unchanged. | UFR-DSK-003 | ✓ UC16 | UC16 |
| REQ-IST-010  | `image_st_write_file()` shall return `ST_ERROR` if the disk is full (no free cluster) or the root directory is full (no free or deleted slot). On disk-full, all partially-allocated clusters shall be freed before returning `ST_ERROR`. | UFR-DSK-003 | ✓ UC16 | UC16 |
| REQ-IST-011  | `image_st_delete_file()` shall return `ST_ERROR` if `ptImg` or `szName` is NULL or if the file is not found. On success the full FAT12 chain shall be freed (all entries set to IST_FAT_FREE) and the directory entry first byte set to 0xE5. | UFR-DSK-003 | ✓ UC16 | UC16 |
| REQ-IST-012  | `image_st_free_bytes()` shall return `ST_ERROR` if any pointer is NULL. On success `*puiFreeBytes` shall equal the count of IST_FAT_FREE (0x000) entries in FAT1 for clusters IST_CLUSTER_FIRST through IST_CLUSTER_FIRST+IST_DATA_CLUSTERS−1, multiplied by IST_SPC × IST_BPS. | UFR-DSK-004 | ✓ UC16 | UC16 |

---

### 2.15 MSA Codec — `image_msa.h` / `image_msa.c`

> Design ref: CLAUDE.md §6.23; depends on `image_st.h` (UC16) + `file.h` (UC6)

| ID           | Software Requirement                                                                                                                                                                                                                           | Parent UFR  | Status   | UC   |
|--------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|-------------|----------|------|
| REQ-MSA-001  | `image_msa_load(NULL,*)` or `image_msa_load(*,NULL)` shall return `ST_ERROR`. A nonexistent or unreadable `.msa` file shall return `ST_ERROR`. | UFR-DSK-005 | ✓ UC17 | UC17 |
| REQ-MSA-002  | `image_msa_save(NULL,*)` or `image_msa_save(*,NULL)` shall return `ST_ERROR`. | UFR-DSK-006 | ✓ UC17 | UC17 |
| REQ-MSA-003  | A round-trip (save then load) shall produce an `image_st_t` with identical FAT12 content: same root directory entries, same file sizes, same byte-for-byte file content. | UFR-DSK-005 | ✓ UC17 | UC17 |
| REQ-MSA-004  | A blank `image_st_t` saved to `.msa` shall produce a file smaller than IST_DISK_SIZE / 10 (compression ratio ≥ 10× for a zeroed disk). | UFR-DSK-006 | ✓ UC17 | UC17 |
| REQ-MSA-005  | Files containing the escape byte (0xE5) shall round-trip exactly; `imsa_compress()` shall always encode 0xE5 as an escape sequence (even count=1), and `imsa_decompress()` shall reconstruct them correctly. | UFR-DSK-005 | ✓ UC17 | UC17 |
| REQ-MSA-006  | `image_msa_load()` shall reject files with: wrong magic (≠ 0x0E0F), SPT == 0, sides > 1, first > last, or last ≥ IST_TRACKS. All shall return `ST_ERROR`; `*pptImg` shall remain NULL. | UFR-DSK-005 | ✓ UC17 | UC17 |
| REQ-MSA-007  | `imsa_decompress()` shall return `ST_ERROR` if: a run overflow occurs (count would exceed output buffer), a truncated escape sequence is detected (< 3 bytes remaining), or the total output is not exactly `uiOutLen` bytes. | UFR-DSK-005 | ✓ UC17 | UC17 |
| REQ-MSA-008  | `imsa_compress()` shall return 0 if the compressed output would equal or exceed `uiOutMax` bytes; the caller shall then write the raw track at `data_length == IMSA_TRACK_BYTES`. | UFR-DSK-006 | ✓ UC17 | UC17 |
| REQ-MSA-009  | When saving, each track shall be written as 2 bytes `data_length` (big-endian) followed by `data_length` bytes of data; if `data_length == IMSA_TRACK_BYTES` the data is raw; otherwise it is MSA-RLE encoded. | UFR-DSK-006 | ✓ UC17 | UC17 |
| REQ-MSA-010  | `image_st_get_disk(NULL,*)` or `image_st_get_disk(*,NULL)` shall return `ST_ERROR`. On success, `*ppDisk` shall point to the internal `aDisk[]` array of the image and remain valid until `image_st_close()`. | UFR-DSK-007 | ✓ UC17 | UC17 |

---

### 2.16 Disassembler — `disassemble.h` / `disassemble.c`

> Design ref: CLAUDE.md §5 R7; DEVPAC3 syntax reference
> Parent UFR: `UFR-HEX-005` (UC10 disasm panel) and `UFR-EXE-*` (UC25 execution engine).

| ID          | Software Requirement                                                                                     | Parent UFR    | Status        | UC    |
|-------------|----------------------------------------------------------------------------------------------------------|---------------|---------------|-------|
| REQ-DIS-001 | `disasm_range(NULL buf, …)` shall return `ST_ERROR`.                                                    | — (see UC10)  | ✓ UC1         | UC1   |
| REQ-DIS-002 | `disasm_range(…, NULL ptResults, …)` shall return `ST_ERROR`.                                            | — (see UC10)  | ✓ UC1         | UC1   |
| REQ-DIS-003 | `disasm_range(…, NULL puiLines)` shall return `ST_ERROR`.                                               | — (see UC10)  | ✓ UC1         | UC1   |
| REQ-DIS-004 | `disasm_range()` with `uiBufLen = 0` shall return `ST_NO_ERROR` and write 0 lines.                     | — (see UC10)  | ✓ UC1         | UC1   |
| REQ-DIS-005 | Stub: any opcode shall produce a `DC.W $XXXX` fallback (`bValid = ST_FALSE`). ADAPTED(UC11): instructions decoded in UC11 now set `bValid = ST_TRUE`; unrecognised opcodes still fall through to DC.W. | — | ADAPTED(UC11) | UC11 |
| REQ-DIS-006 | Shall decode MOVE.B/W/L, MOVEA.W/L, MOVEQ, LEA, PEA, CLR.B/W/L, SWAP, EXG (Dx/Dy, Ax/Ay, Dx/Ay) to DEVPAC3 format with correct mnemonic, operands, and `iWordCount`. | UFR-HEX-005 | ✓ UC11 | UC11 |
| REQ-DIS-010 | `disasm_fmt_ea()` shall support all 12 EA modes: Dn, An, (An), (An)+, -(An), d16(An), d8(An,Xn), abs.W (sign-extended to 24-bit display), abs.L, d16(PC) (computed EA), d8(PC,Xn), #imm (.B/.W/.L). | UFR-HEX-005 | ✓ UC11 | UC11 |
| REQ-DIS-011 | For abs.W mode (7.0): the 16-bit extension word shall be sign-extended to 32 bits and masked to 24 bits for display; negative values shall appear as `$FFxxxx.W`, positive as `$xxxx.W`. | UFR-HEX-005 | ✓ UC11 | UC11 |
| REQ-DIS-012 | For d16(PC) and d8(PC,Xn): the effective address displayed shall be calculated as `(address_of_extension_word + displacement) & 0xFFFFFF`. | UFR-HEX-005 | ✓ UC11 | UC11 |
| REQ-DIS-013 | MOVE.B to An (destination mode=001, size=B) shall produce a DC.W fallback (`bValid=ST_FALSE`). MOVEA.W/L (destination mode=001, size=W/L) shall use mnemonic `MOVEA.W`/`MOVEA.L`. | UFR-HEX-005 | ✓ UC11 | UC11 |
| REQ-DIS-014 | CLR with size field=11 (bits 7-6 = 11) shall produce a DC.W fallback. PEA with non-control EA mode (Dn, An, (An)+, -(An), #imm) shall produce a DC.W fallback. | UFR-HEX-005 | ✓ UC11 | UC11 |
| REQ-DIS-015 | Shall decode ADD/ADDA/ADDI/ADDQ/ADDX and SUB/SUBA/SUBI/SUBQ/SUBX with correct size suffix, operands, and word count. | UFR-HEX-005 | ✓ UC12 | UC12 |
| REQ-DIS-016 | Shall decode CMP/CMPA/CMPI/CMPM; CMPM uses (An)+,(An)+ syntax. | UFR-HEX-005 | ✓ UC12 | UC12 |
| REQ-DIS-017 | Shall decode MULU.W/MULS.W/DIVU.W/DIVS.W with `.W` size suffix; source is always word-sized EA; destination is Dn. | UFR-HEX-005 | ✓ UC12 | UC12 |
| REQ-DIS-018 | Shall decode AND/ANDI/OR/ORI/EOR/EORI; immediate forms use `#$imm` source then EA destination; CCR/SR detected and named. | UFR-HEX-005 | ✓ UC12 | UC12 |
| REQ-DIS-019 | Shall decode NOT/NEG/NEGX/TST with correct size suffix; size=3 → DC.W. EXT.W and EXT.L on Dn. | UFR-HEX-005 | ✓ UC12 | UC12 |
| REQ-DIS-020 | ADDQ/SUBQ: 3-bit immediate field (0 means 8) displayed as decimal; size=3 → DC.W (Scc/DBcc). | UFR-HEX-005 | ✓ UC12 | UC12 |
| REQ-DIS-021 | ADDX/SUBX: register form `Dx,Dy` and memory form `-(Ax),-(Ay)` distinguished by bit3 of opcode. | UFR-HEX-005 | ✓ UC12 | UC12 |
| REQ-DIS-022 | For group 0xC (AND/MULU/MULS + EXG) and group 0x8 (OR/DIVU/DIVS): ABCD/SBCD patterns (bit8=1, sz=B, mode≤1) → DC.W. | UFR-HEX-005 | ✓ UC12 | UC12 |
| REQ-DIS-023 | IMMI instructions (ADDI/SUBI/CMPI/ANDI/ORI/EORI): immediate source words precede destination EA extension words in the instruction stream. | UFR-HEX-005 | ✓ UC12 | UC12 |
| REQ-DIS-007 | Shall decode ADD / SUB / CMP / MULU / DIVS / AND / OR / EOR / NOT / NEG.                              | UFR-HEX-005   | ✓ UC12        | UC12  |
| REQ-DIS-008 | Shall decode ASL/ASR/LSL/LSR/ROL/ROR/ROXL/ROXR (register immediate, register Dn, and memory .W forms) and BTST/BCHG/BCLR/BSET (static #imm and dynamic Dn forms). | UFR-HEX-005 | ✓ UC13 | UC13 |
| REQ-DIS-009 | Shall decode BRA/BSR/Bcc (short and long displacement), JMP/JSR (all control EA modes), NOP/RTS/RTR/RTE, TRAP #N, LINK An/UNLK An. | UFR-HEX-005 | ✓ UC14 | UC14 |
| REQ-DIS-024 | **ADAPTED:UC15A** — Register-form shift/rotate (immediate count): format `1110 cccc d ss 0 tt rrr` (**bit5=0** = immediate); count field=0 → displayed as #8; direction bit: 1=left, 0=right; size field: 00=.B, 01=.W, 10=.L; type: 00=AS, 01=LS, 10=ROX, 11=RO; operands `#N,Dn`. *UC13 tests used inverted opcodes (bit5=1); corrected in UC15A.* | UFR-HEX-005 | ✓ UC15A | UC15A |
| REQ-DIS-025 | **ADAPTED:UC15A** — Register-form shift/rotate (register count): format `1110 cccc d ss 1 tt rrr` (**bit5=1** = register); count field = Dn number (no "0 means 8" rule); operands `Dn,Dn`. *UC13 tests used inverted opcodes (bit5=0); corrected in UC15A.* | UFR-HEX-005 | ✓ UC15A | UC15A |
| REQ-DIS-026 | Memory-form shift/rotate: format `1110 xxx 11 EA` (bits 7-6 = 11 selects memory form); always `.W`; mnemonic indexed by bits 10-8 (0=ASR..7=ROL); bit 11 set → DC.W; EA mode 0 (Dn) or mode 1 (An) → DC.W; extension words consumed for abs.W addressing. | UFR-HEX-005 | ✓ UC13 | UC13 |
| REQ-DIS-027 | Static bit ops (BTST/BCHG/BCLR/BSET #imm): opcode `0000 1000 op EA` + extension word #bit; op field indexes BTST/BCHG/BCLR/BSET; An mode (mode=1) → DC.W; `iWordCount = 2 + EA_ext_words`; operands `#N,EA`. | UFR-HEX-005 | ✓ UC13 | UC13 |
| REQ-DIS-028 | Dynamic bit ops (BTST/BCHG/BCLR/BSET Dn): opcode `0000 Dn 1 op EA`; An mode → DC.W; `iWordCount = 1 + EA_ext_words`; operands `Dn,EA`. | UFR-HEX-005 | ✓ UC13 | UC13 |
| REQ-DIS-029 | `disasm_range()` shall advance by `iWordCount*2` bytes per instruction across UC13 opcodes (shift+bit ops of varying word counts) and decode all instructions in a mixed stream correctly. | UFR-HEX-005 | ✓ UC13 | UC13 |
| REQ-DIS-030 | Invalid shift/bit encodings shall produce DC.W: memory shift to Dn/An destination; memory shift with bit11 set; static/dynamic bit op to An destination. | UFR-HEX-005 | ✓ UC13 | UC13 |
| REQ-DIS-031 | NOP (0x4E71), RTE (0x4E73), RTS (0x4E75), RTR (0x4E77): 1-word instructions, empty operands, `bValid=ST_TRUE`. | UFR-HEX-005 | ✓ UC14 | UC14 |
| REQ-DIS-032 | TRAP #N (0x4E40-0x4E4F): vector N in low nibble, displayed as decimal; 1 word, operands `#N`. | UFR-HEX-005 | ✓ UC14 | UC14 |
| REQ-DIS-033 | LINK An,#disp16 (0x4E50-0x4E57 + ext): 2 words; displacement signed, displayed as `#$N` (positive/zero) or `#-$N` (negative). UNLK An (0x4E58-0x4E5F): 1 word. | UFR-HEX-005 | ✓ UC14 | UC14 |
| REQ-DIS-034 | JSR (0x4E80-0x4EBF) and JMP (0x4EC0-0x4EFF): EA must be a control mode; Dn (mode=0), An (mode=1), (An)+ (mode=3), -(An) (mode=4), #imm (mode=7,reg=4) → DC.W. | UFR-HEX-005 | ✓ UC14 | UC14 |
| REQ-DIS-035 | BRA (cond=0x0) and BSR (cond=0x1): disp8=0xFF → DC.W (68020+); disp8=0x00 → long form (2 words, mnemonic without `.S`); disp8 other → short form (1 word, mnemonic with `.S`). | UFR-HEX-005 | ✓ UC14 | UC14 |
| REQ-DIS-036 | Branch target address = `(instruction_addr + 2 + displacement) & 0x00FFFFFF`; displayed as `$XXXXXX` (6 hex digits). | UFR-HEX-005 | ✓ UC14 | UC14 |
| REQ-DIS-037 | Bcc (conditions 2-15): same short/long/0xFF rules as BRA/BSR; mnemonics from `g_aszBcc[16]` table (BHI, BLS, BCC, BCS, BNE, BEQ, BVC, BVS, BPL, BMI, BGE, BLT, BGT, BLE). | UFR-HEX-005 | ✓ UC14 | UC14 |
| REQ-DIS-038 | `disasm_range()` shall walk a realistic mixed control-flow stream (MOVE + TST + BEQ.S + NOP + BRA.S + RTS) correctly, with addresses advancing per `iWordCount`. | UFR-HEX-005 | ✓ UC14 | UC14 |

---

### 2.17 Mount View — `mount.h` / `mount.c`

> Design ref: CLAUDE.md §6.24; depends on `image_st.h` (UC16), `image_msa.h` (UC17), `gui.h` (UC3.1), `renderer.h` (UC3.2), `file.h` (UC6).

| ID           | Software Requirement                                                                                                                                                                                                                        | Parent UFR   | Status    | UC      |
|--------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|--------------|-----------|---------|
| REQ-MNT-001  | `mount_view_open(szPath, NULL, pptView)` or `mount_view_open(szPath, ptCtx, NULL)` shall return `ST_ERROR`. `mount_view_open("noexist.st", ...)` shall return `ST_ERROR` and leave `*pptView == NULL`. | UFR-MNT-001 | ✓ UC18.1 | UC18.1 |
| REQ-MNT-002  | When `szPath` is NULL or empty, `mount_view_open()` shall resolve to `ptCtx->szCwd`. A PC directory shall open with `MOUNT_SRC_DIR`; a `.st` file with `MOUNT_SRC_ST`; a `.msa` file with `MOUNT_SRC_MSA`; any other extension shall return `ST_ERROR`. | UFR-MNT-001 | ✓ UC18.1 | UC18.1 |
| REQ-MNT-003  | After a successful `mount_view_open()`, `ptView->eSrc` shall match the detected source type, `ptView->iEntryCount` shall equal the number of FAT root entries, and `ptView->ptImg` shall be non-NULL. | UFR-MNT-001 | ✓ UC18.1 | UC18.1 |
| REQ-MNT-004  | `mount_view_open()` shall open a D2D GUI window (`GUI_WND_MOUNT`) visible to `gui_find_window_by_type(GUI_WND_MOUNT, &hWnd)` as long as the view is open. | UFR-MNT-002 | ✓ UC18.1 | UC18.1 |
| REQ-MNT-005  | `mount_view_close(NULL)` shall return `ST_ERROR`. `mount_view_close(&NULL)` shall return `ST_NO_ERROR` (idempotent). After a successful close, `*pptView` shall be NULL, the window thread joined, the renderer destroyed, and `ptImg` freed. | UFR-MNT-004 | ✓ UC18.1 | UC18.1 |
| REQ-MNT-006  | `mount_view_add_file(NULL, szPath)` or `mount_view_add_file(ptView, NULL)` shall return `ST_ERROR`. Adding a valid PC file shall call `image_st_write_file()`, set `ptView->bDirty = ST_TRUE`, increment `ptView->iEntryCount`, and invalidate the view. | UFR-MNT-003 | ✓ UC18.1 | UC18.1 |
| REQ-MNT-007  | The left panel shall list FAT root entries; keyboard UP/DOWN/PgUp/PgDn/Home/End shall navigate the selection with scroll clamping; DEL shall call `image_st_delete_file()`, refresh the entry list, and clamp the selection to the new count. | UFR-MNT-002 | ✓ UC18.1 | UC18.1 |
| REQ-MNT-008  | ESC shall call `gui_request_close()` from the window thread, without blocking. `line_cmd_umount()` shall call `mount_view_close()`. | UFR-MNT-004 | ✓ UC18.1 | UC18.1 |
| REQ-MNT-009  | `gui_find_window_by_type(eType, NULL)` shall return `ST_ERROR`. With a valid `phWnd`, if no window of `eType` is open, it shall return `ST_NO_ERROR` and set `*phWnd = NULL`; if one is open, `*phWnd != NULL`. | UFR-MNT-004 | ✓ UC18.1 | UC18.1 |
| REQ-MNT-010  | `file_stat_t.szExt` stores the extension without the leading dot (e.g., `"st"` not `".st"`). All comparisons in `mount_view_open()` and `line_cmd_mount()` shall compare against the dot-free form. | UFR-MNT-001 | ✓ UC18.1 | UC18.1 |
| REQ-MNT-011  | The window title shall follow R18 format: `"ST4Ever - Mount: A:\\ [SRC]"` where `[SRC]` is `DIR`, `ST`, or `MSA` according to `mount_src_t`. | UFR-MNT-002 | ✓ UC18.1 | UC18.1 |
| REQ-MNT-012  | The right properties panel shall display: Source type, source path, file count, free bytes, total bytes, and a "modified" indicator (`bDirty`). Mouse scroll wheel shall scroll the file list when focused on it. | UFR-MNT-002 | ✓ UC18.1 | UC18.1 |
| REQ-MNT-013  | The properties panel shall display BPB geometry fields in read-only mode: heads (`@0x1A` LE16), sectors/track (`@0x18` LE16), tracks (`@0x13` LE16), root dir capacity RDE (`@0x11` LE16). The `/RDE` count shall be removed from the left panel header (kept in right panel as `Files: N / RDE`). The "Bootable: Yes/No" indicator uses `mount_check_bootable()` (WD1772 checksum). (P34, P36, P37) | UFR-MNT-005, UFR-MNT-006 | ✓ UC18.2 | UC18.2 |
| REQ-MNT-014  | Keyboard shortcut `B`/`b` in `mount_handle_key()` shall call `mount_open_bootsector()`: extract 512 bytes from `aDisk` via `image_st_get_disk()`, write to `MOUNT_BOOT_TMP`, call `edit_hex_open()` with a heuristic title `"bootsector [H%u/S%u/T%u %uKo — bootable/not bootable]"`. If a previous bootsector view is already open, it shall be closed first. If `edit_hex_open()` fails, `ptBootHexView = NULL` and LOG_ERROR (non-fatal). (P38) | UFR-MNT-007 | ✓ UC18.2 | UC18.2 |
| REQ-MNT-015  | `mount_view_open()` shall store `ptLineCtx` in `ptView->ptLineCtx` for use by `mount_open_bootsector()`. `ptView->ptBootHexView` shall be initialised to NULL at open time. | UFR-MNT-007 | ✓ UC18.2 | UC18.2 |
| REQ-MNT-016  | `mount_view_close()` shall close any open bootsector hex view (`ptBootHexView != NULL` → `edit_hex_close()`) before calling `gui_close_window()`. | UFR-MNT-007 | ✓ UC18.2 | UC18.2 |
| REQ-MNT-017  | `mount_is_bootable(pBootSect)` shall return `ST_FALSE` if `pBootSect == NULL`. Otherwise it shall sum the 256 LE16 words of the 512-byte buffer and return `ST_TRUE` iff the result `& 0xFFFF == 0x1234`, `ST_FALSE` otherwise. | UFR-MNT-006 | ✓ UC18.2 | UC18.2 |
| REQ-MNT-018  | `mount_save_image(ptView, eFmt, szOutPath)` shall return `ST_ERROR` on NULL ptView, NULL path, or unknown format; on success it shall save the image in `.st`, `.msa`, or directory format and set `ptView->bDirty = ST_FALSE`. | UFR-MNT-008 | ✓ UC19 | UC19 |
| REQ-MNT-019  | `mount_view_add_file()` NULL guards (NULL ptView or NULL szPath) shall return `ST_ERROR`; the P40 chunked read (64 KB per chunk) shall produce identical FAT content to a monolithic read. | UFR-MNT-004 | ✓ UC19 | UC19 |
| REQ-MNT-020  | `mount_render()` shall reserve the last row as a status bar (`iVisRows = (iWndH/iCellH) - 2`) showing Free KB, file count, and `[*] unsaved` when `bDirty == ST_TRUE`. | UFR-MNT-009 | ✓ UC19 | UC19 |
| REQ-MNT-021  | `line_cmd_umount()` shall show an interactive save dialog (keys 1/2/3/n/ESC) when `bDirty == ST_TRUE` and no bypass flag is present; `--st`, `--msa`, and `--dir [path]` shall bypass the dialog. | UFR-MNT-008 | ✓ UC19 | UC19 |
| REQ-MNT-022  | `mount_make_bootable(ptImg)` shall return `ST_ERROR` if `ptImg == NULL`; otherwise it shall compute the WD1772 checksum (sum of 256 LE16 words) and patch `pDisk[0..1]` so the sum equals `0x1234 mod 0x10000`; the operation shall be idempotent. | UFR-MNT-012 | ✓ UC20 | UC20 |
| REQ-MNT-023  | `line_cmd_image()` shall save the image from an already-open mount view (`g_line_ptMountView != NULL`) or create a transient view from the selected directory; `--bootable` shall call `mount_make_bootable()` before saving; `--st`/`--msa` flags select the output format. | UFR-MNT-010 | ✓ UC20 | UC20 |
| REQ-MNT-024  | `mount_open_file_hex()` shall be a no-op if no entry is selected or the entry is empty; otherwise it shall extract the file via `image_st_read_file()`, write to `MOUNT_FILE_TMP`, close any previous file hex view, and open in `edit_hex_open()`. | UFR-MNT-011 | ✓ UC20 | UC20 |

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

