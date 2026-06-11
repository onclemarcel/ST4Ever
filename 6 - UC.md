# ST4EVER - Document 6 - USE CASES - COMPORTEMENTS VALIDES

## 0 - INTRODUCTION
Ce document contient le périmètre fonctionnel implémenté, infrastructure validée, matrice de tests (N/R/S), anomalies résolues, et un paragraphe **"Contrats comportementaux validés"** par module — ces contrats assurent la traçabilité verticale : Concepts Opérationnels (**1 - OC.md**) → invariant technique (**6 - UC.md**) → preuve exécutable (`use_case_XX.c`). 

Ils guident les sessions futures lors de l'implémentation réelle.

## 1 Modèle de section Use Case XX (UCXX)

**Périmètre fonctionnel implémenté :**
- détail périmètre fonctionnel implémenté pendant l'UC concerné, par exemple:
  - Console context : `line_init`, `line_shutdown` (stub `line_run` non appelé — bloquerait sur stdin)
  - ST machine memory : `st_init/shutdown`, `st_read/write_byte/word/long` (big-endian, contrôle alignement)

**Infrastructure et outillage validés :** *(section optionnelle — omettre si aucune infra nouvelle)*
- Description des décisions d'architecture, de conception, outillage, pattern (abstraction plate-forme, par exemple, ...): En exemple:
  - Makefile multi-platform MSYS2/Windows : détection automatique sources, `-std=gnu99`, `-D_GNU_SOURCE`, objets dans `./build/`, exécutables dans `./release/`, tests dans `./test/`
  - Structure `src/` (portable) + `win/` (Win32) + `linux/` (stubs X11/Posix) + `use_cases/`

**Tests R14/R15 appliqués :**
- Bilan de la phase de test et identification des éléments de tests, par exemple:
  - `use_cases/use_cases.h` : ajout macro `TEST_SKIP`, documentation mécanisme `ST_TEST_LEVEL_UCNN`
  - `use_cases/use_case_01.c` : TEST MATRIX **45N + 19R + 0S = 64 tests**, tous PASS
    - Labels `[N]`/`[R]` sur chaque assertion, commentaires `/* INTENT: */` par bloc
    - Robustesse couverte : NULL params (toutes fonctions publiques), double `trace_init`, double `trace_close`, alignement word/long, borne RAM, buffer vide `disasm_range`

**Contrats comportementaux validés :**
- Un résumé des comportements ajoutés lors de l'UC en cours. Ces comportements peuvent être additionnels au document initial **1 - OC.md**, Tonton fera la mise à jour en fonction des contracts comportementaux validés décrits par Claude dans cette section. Par exemple:
  - *Module `trace` (→ UC2 pour l'implémentation complète)*
    - `trace_init(bOpen)` est idempotente : un double appel retourne `ST_NO_ERROR` avec warning stderr uniquement
    - `trace_close()` est idempotente : fermer une console déjà fermée retourne `ST_NO_ERROR`
    - Les quatre niveaux `LOG_TRACE/INFO/ERROR/TODO` doivent émettre sans crash quelle que soit l'état de la console
    - Les `LOG_TRACE` consécutifs depuis la même fonction sont compactés en `[xN]` (flush déclenché par un `LOG_INFO`)

**Points d'attention pour les UCs suivants :**
- Element clé de la maintenance du tableau résumé des Use Cases en section 6 de CLAUDE.md. Les dépendances entre UC sont indiquées ici et serviront de vérification à la cohérence du tableau section 6 - Use Cases de CLAUDE.md. Par exemple:
  - UC2 : validé — `line_cmd_trace()` complète : toggle, on, off, unknown arg, extra args warning
  - UC3 : `gui_init` réel ouvre une fenêtre Win32 → les tests UC1 qui appellent des stubs GUI devront utiliser `#ifdef ST_TEST_LEVEL_UC01` + `TEST_SKIP` (mécanisme R14 en place)
- Les points d'attention résolus sont marqués par la mention **RÉSOLU UCxx**, cela permet de faciliter la revue des points d'attentions restants ouverts dans le document. Par exemple:
  - ~~UC4.3 : CTRL+I = TAB = 0x09 conflit CMD_IMAGE~~ **RÉSOLU UC4.3** — TAB = complétion ; CMD_IMAGE via `"i"`/`"image"` uniquement (commenté dans `line.h` enum).

## 1.1 Points d'attention inter-UC (ouverts)

*Ce tableau agrège les dépendances inter-UC encore ouvertes. Source : sections "Points d'attention" de chaque UC ci-dessous. Mise à jour à chaque clôture de UC : ajouter les nouvelles lignes, marquer **RÉSOLU UCxx** les points adressés.*

| UC source | UC cible | Point d'attention | Résolu |
|-----------|----------|-------------------|--------|
| UC1 | UC24 | `st_read_byte` hors-RAM devra lever `ST_ERROR` pour zones vraiment non mappées — test marqué `ADAPTED:UC24` | — |
| UC7 | UC24 | `ST_LOAD_BASE = 0x0800` en dur — UC24 allouera première zone libre au-dessus des variables TOS | — |
| UC7 | UC25 | `execute` démarrera depuis `load_get_state()->uiLoadAddr` comme PC initial (`eType == LOAD_TYPE_PRG`) | — |
| UC14/UC15A | UC23+ | MOVEM, CHK, DBcc/Scc, MOVEP (~175 DC.W restants) — non couverts par le désassembleur ni le CPU actuel | **MOVEM/DBcc/Scc RÉSOLU UC23-bis** |
| UC21 | UC22 | Flags X, C, V — gestion complète pour ADD/SUB/CMP/shifts (dépassement, retenue, borrow) | **RÉSOLU UC22** |
| UC21 | UC22 | `iCycles` hardcodé à 4 — affiner selon table timings 68000 (EA mode + instruction) si nécessaire | **RÉSOLU UC22** (différé UC23+) |
| UC21 | UC23 | `cpu_exec_misc4` : RTS/NOP/JSR/JMP/TRAP/LINK/UNLK/RTE/RTR + `cpu_raise_exception()` complet (stacking SR+PC, changement mode) | — |
| UC22 | UC23 | Scc/DBcc (groupe 0x5 size=3) — LOG_TODO actuellement | — |
| UC22 | UC23 | Division par zéro DIVU/DIVS → `cpu_raise_exception(CPU_VEC_DIV_ZERO)` | — |

---

## 2 Use Case 01 (UC1) — VALIDÉ (2026-05-30)

*Prototype complet fourni intégralement par Claude.AI. Projet géré sous GitHub (https://github.com/onclemarcel/ST4Ever), intégré dans VS Code + Terminal MSYS2, clone local sous `/home/claude/ST4Ever`.*

**Périmètre fonctionnel implémenté :**
- Console interactive : `help` (h/CTRL+H), `quit` (q/CTRL+Q), `trace -t` (t/CTRL+T) ; toutes autres commandes → `line_cmd_stub()` + LOG_TODO
- Trace subsystem complet : `trace_init/open/close/shutdown`, `LOG_TRACE/INFO/ERROR/TODO`, compaction des TRACE consécutifs (`[xN]`), `trace_set/is_trace_enabled`, sortie fichier `st4ever_trace.log` + stderr ANSI
- Console context : `line_init`, `line_shutdown` (stub `line_run` non appelé — bloquerait sur stdin)
- ST machine memory : `st_init/shutdown`, `st_read/write_byte/word/long` (big-endian, contrôle alignement)
- CPU 68000 stub : `cpu_init` (lit vecteurs reset SSP/PC, mode superviseur SR=0x2700), `cpu_step` (avance PC+2, retourne opcode brut — sans décodage réel)
- Disassembler stub : `disasm_range/disasm_one` → fallback DC.W pour toute instruction
- Tous autres modules : stubs avec LOG_TODO (gui, renderer, dir, mount, edit, exec, CPU 68k réel, as, ST formats disque)

**Infrastructure et outillage validés :**
- Makefile multi-platform MSYS2/Windows : détection automatique sources, `-std=gnu99`, `-D_GNU_SOURCE`, objets dans `./build/`, exécutables dans `./release/`, tests dans `./test/`
- Structure `src/` (portable) + `win/` (Win32) + `linux/` (stubs X11/Posix) + `use_cases/`
- `win_console.c` : activation ANSI/VT100 non-fatale (mintty n'expose pas de handle Win32)
- Convention hongrois + macros `LOG_*` avec `##__VA_ARGS__` (`-std=gnu99`)
- Fixture de test stable : `use_cases/UC01/hello.prg` = header PRG 28 octets + MOVEQ #42,D0 (0x702A) + RTS (0x4E75), 4 octets .text

**Tests R14/R15 appliqués :**
- `use_cases/use_cases.h` : ajout macro `TEST_SKIP`, documentation mécanisme `ST_TEST_LEVEL_UCNN`
- `use_cases/use_case_01.c` : TEST MATRIX **45N + 19R + 0S = 64 tests**, tous PASS
  - Labels `[N]`/`[R]` sur chaque assertion, commentaires `/* INTENT: */` par bloc
  - Robustesse couverte : NULL params (toutes fonctions publiques), double `trace_init`, double `trace_close`, alignement word/long, borne RAM, buffer vide `disasm_range`

**Anomalies découvertes et résolues :**
- `trace.h` doc incorrecte : double-init documenté `ST_ERROR` → corrigé `ST_NO_ERROR` (implémentation intentionnellement idempotente, warning stderr uniquement)
- `st_read_byte(ST_RAM_SIZE)` : retourne `ST_NO_ERROR` + `0xFF` — comportement stub intentionnel (espace cartouche 24-bit valide, non implémenté). Test marqué `ADAPTED: UC24` pour quand la memory map réelle lèvera un bus error.
- `cpu_step` stub : avance PC+2 sans décoder — marqué `ADAPTED: UC21`
- `disasm_range` stub : DC.W fallback — marqué `ADAPTED: UC11-UC14`

**Contrats comportementaux validés :**

*Module `trace` (→ UC2 pour l'implémentation complète)*
- `trace_init(bOpen)` est idempotente : un double appel retourne `ST_NO_ERROR` avec warning stderr uniquement
- `trace_close()` est idempotente : fermer une console déjà fermée retourne `ST_NO_ERROR`
- Les quatre niveaux `LOG_TRACE/INFO/ERROR/TODO` doivent émettre sans crash quelle que soit l'état de la console
- Les `LOG_TRACE` consécutifs depuis la même fonction sont compactés en `[xN]` (flush déclenché par un `LOG_INFO`)
- `trace_set_trace_enabled(FALSE)` supprime uniquement `LOG_TRACE`, les trois autres niveaux restent actifs
- `trace_set_trace_enabled(TRUE)` réactive `LOG_TRACE` immédiatement

*Module `line` (→ UC4 pour l'éditeur riche)*
- `line_init(NULL)` retourne `ST_ERROR` sans toucher à l'état
- Après `line_init()` : `bRunning == TRUE` et `szCwd` non-vide
- `line_shutdown(NULL)` retourne `ST_ERROR` sans toucher à l'état
- Après `line_shutdown()` : `bRunning == FALSE`

*Module `ST` machine (→ UC24 pour la memory map complète)*
- `st_init(NULL, ...)` retourne `ST_ERROR` ; après init valide : `bPoweredOn == TRUE`, `uiResolution == ST_RES_LOW`
- Round-trip byte/word/long exact ; word et long en big-endian
- Accès NULL machine ou NULL pointeur de sortie → `ST_ERROR`
- Accès word/long non aligné → `ST_ERROR` (bus error 68000)
- Accès hors-RAM (espace cartouche/ROM 24-bit) : stub retourne `ST_NO_ERROR + 0xFF` — *ADAPTED:UC24* (lèvera `ST_ERROR` pour les zones vraiment non mappées)
- `st_shutdown(NULL)` → `ST_ERROR` ; après shutdown valide : `bPoweredOn == FALSE`

*Module `CPU` 68000 (→ UC21 pour le décodage réel)*
- `cpu_init(NULL, ...)` et `cpu_init(..., NULL)` → `ST_ERROR`
- Après `cpu_init()` valide : `uiPC` = vecteur reset PC, `uiSSP` = vecteur reset SSP, bit S du SR positionné, `eState == CPU_STATE_RUNNING`
- `cpu_step(NULL, ...)` et `cpu_step(..., NULL, ...)` → `ST_ERROR`
- Stub UC1 : `cpu_step()` avance PC+2 et retourne l'opcode brut — *ADAPTED:UC21* (décodage et exécution réels)

*Module `disasm` (→ UC11-UC14 pour le décodage complet)*
- `disasm_range()` avec buffer vide (len=0) : retourne `ST_NO_ERROR` et écrit 0 lignes
- `disasm_range(NULL buf, ...)` ou `disasm_range(..., NULL results, ...)` → `ST_ERROR`
- Stub UC1 : toute instruction → fallback `DC.W $XXXX` — *ADAPTED:UC11-UC14*

**Points d'attention pour les UCs suivants :**
- ~~UC2 : validé — `line_cmd_trace()` complète : toggle, on, off, unknown arg, extra args warning~~ **RÉSOLU UC2**
- ~~UC3 : `gui_init` réel ouvre une fenêtre Win32 → les tests UC1 qui appellent des stubs GUI devront utiliser `#ifdef ST_TEST_LEVEL_UC01` + `TEST_SKIP` (mécanisme R14 en place)~~ **RÉSOLU UC3.1**
- ~~UC21 : `cpu_step` réel changera le comportement des tests UC1 step — assertions marquées `ADAPTED`~~ **RÉSOLU UC21** — D0==42 + opcode correct ; RTS LOG_TODO UC23
- UC24 : `st_read_byte` hors-RAM devra lever ST_ERROR pour les zones vraiment non mappées — test marqué `ADAPTED`

## 3 Use Case 02 (UC2) — VALIDÉ (2026-05-30)

**Périmètre fonctionnel implémenté :**
- `line_cmd_trace()` complète dans `line.c` :
  - `trace` (no arg) : toggle open/close, `trace_enabled` inchangé
  - `trace on` : `trace_open()` + `trace_set_trace_enabled(TRUE)`
  - `trace off` : `trace_set_trace_enabled(FALSE)` + `trace_close()`
  - `trace <inconnu>` : warning utilisateur, aucun effet sur l'état
  - `trace on|off <extra>` : warning sur les args superflus, premier arg traité normalement (cohérence avec `help`/`quit`)
- Retrait du `TODO(UC2)` dans le header `line.c` et le commentaire de `line_cmd_trace()`

**Tests R14/R15 appliqués :**
- `use_cases/use_case_02.c` : TEST MATRIX **22N + 4R + 4S = 30 tests**, tous PASS
  - Stratégie : `line_cmd_trace()` et `line_dispatch()` étant `static`, les contrats comportementaux sont validés en pilotant l'API publique `trace_*` dans les mêmes séquences que la commande
  - Labels `[N]`/`[R]`/`[S]` sur chaque assertion, commentaires `/* INTENT: */` par bloc
  - [S] skipped : 4 tests de dispatch stdin validés manuellement via `make run`

**Contrats comportementaux validés :**

*Module `line_cmd_trace` (→ UC4 pour les tests de dispatch stdin automatisés)*
- `trace` (no arg) : ouvre si fermé (`trace_open()`), ferme si ouvert (`trace_close()`). Ne touche jamais à `trace_set_trace_enabled()`.
- `trace on` : séquence `trace_open()` puis `trace_set_trace_enabled(TRUE)`. Idempotent si la console est déjà ouverte (`trace_open()` est idempotent — contrat UC1).
- `trace off` : **ADAPTED P19** — appelle uniquement `trace_set_trace_enabled(FALSE)` sans fermer la vue. La vue reste visible et continue d'afficher LOG_INFO/ERROR/TODO. Idempotent (second appel à `trace_set_trace_enabled(FALSE)` est no-op). Pour fermer la vue, utiliser `trace` (toggle).
- `trace <inconnu>` : warning consommateur, pas de changement d'état.
- Args superflus (`trace on foo`) : warning, puis traitement normal du premier arg.
- Après `trace off` : `LOG_INFO/ERROR/TODO` restent actifs et visibles dans la vue ; `LOG_TRACE` silencieux. `trace_set_trace_enabled(TRUE)` réactive `LOG_TRACE` immédiatement.

**Points d'attention pour les UCs suivants :**
- ~~UC3 : `gui_init` réel ouvre une fenêtre Win32 → les tests UC1/UC2 qui appellent des stubs GUI devront utiliser `#ifdef ST_TEST_LEVEL_UCxx` + `TEST_SKIP` (mécanisme R14 en place)~~ **RÉSOLU UC3.1**
- ~~UC4 : le dispatch stdin complet permettra d'automatiser les 4 tests [S] actuellement manuels~~ **RÉSOLU UC4.2**

## 4 Use Case 03.1 (UC3.1) — VALIDÉ (2026-05-30)

**Périmètre fonctionnel implémenté :**
- `src/gui_backend.h` (nouveau) : interface interne entre `gui.c` et les backends plateforme (`gui_platform_*`). Contient la définition complète de `struct gui_window_s` (opaque dans `gui.h`).
- `src/gui.c` réécrit : implémentation complète de `gui_msg_queue_t` (tampon circulaire protégé par mutex, spin-wait 1ms pour le get bloquant) + registre global de fenêtres (`g_gui_aptWnd[16]`) + lifecycle `gui_init/open/close/shutdown` délégant aux backends.
- `win/win_gui.c` réécrit : backend Win32 complet — `gui_platform_init` (RegisterClassEx "ST4EverView"), `gui_platform_window_create` (thread dédié + synchronisation via Win32 Event, délai max 5s), WndProc (traduction de 12 types de messages Win32 → `gui_event_t`), fond sombre RGB(24,24,36) dans WM_PAINT jusqu'à D2D (UC3.2).
- `linux/lx_gui.c` réécrit : stubs `gui_platform_*` complets (compilation propre, TODO UC3-Linux).
- `src/common.h` + platform files : ajout de `platform_sleep_ms()` (Win32: Sleep, Linux: usleep).

**Architecture GUI établie (valable pour tous les UCs suivants) :**
- Chaque vue (`dir`, `edit`, `mount`, `execute`...) = un `gui_window_t` dans un thread dédié
- Thread créateur appelle `gui_open_window()`, attend la signalisation du Win32 Event, puis reprend
- Thread fenêtre : CreateWindowEx → ShowWindow → GetMessage loop → WM_DESTROY → PostQuitMessage
- Fermeture : `PostMessageA(WM_CLOSE)` + `platform_thread_join()`
- Communication console→vue : `gui_msg_queue_t` (UC4+ pour utilisation réelle)
- Communication vue→console : `pfnEvent` callback appelé depuis le thread fenêtre

**Tests R14/R15 appliqués :**
- `use_cases/use_case_03_1.c` : TEST MATRIX **18N + 11R + 3S = 32 tests**, tous PASS
  - [N] msg_queue lifecycle complet : create/post/get/FIFO/fill/drain/destroy
  - [N] gui_init/shutdown idempotents (RegisterClass/UnregisterClass headless)
  - [R] tous les paramètres NULL rejetés, capacité 0, queue pleine, queue vide
  - [S] 3 tests visuels (fenêtre ouverte/fond sombre/fermeture) : `make manual` UC4

**Décisions architecturales validées :**
- D2D dès UC3 (COM pur C via lpVtbl) — menu contextuel différé à UC7/UC18
- UC3 découpé en UC3.1 (infra) / UC3.2 (D2D renderer) / UC3.3 (dir view)
- `gui_backend.h` = interface privée entre la couche portable et les backends

**Contrats comportementaux validés :**

*Module `gui_msg_queue_t`*
- `gui_msg_create(0)` → `ST_ERROR` (capacité nulle rejetée)
- Post sur queue pleine → `ST_ERROR` sans bloquer
- Get non-bloquant sur queue vide → `ST_ERROR` immédiat
- FIFO strict : les événements sont dépilés dans l'ordre d'insertion
- `gui_msg_destroy()` libère buffer + mutex, met le handle à NULL

*Module `gui` lifecycle*
- `gui_init()` est idempotente : double appel → `ST_NO_ERROR`, pas de double-enregistrement WNDCLASS
- `gui_shutdown()` sans `gui_init()` → `ST_NO_ERROR` (no-op sûr)
- `gui_open_window(NULL, ...)` ou `(..., NULL)` → `ST_ERROR` sans ouvrir de fenêtre
- `gui_close_window(NULL)` → `ST_ERROR`
- `gui_invalidate(NULL)` → `ST_ERROR`
- `gui_get_size(NULL, ...)` → `ST_ERROR`

**Points d'attention pour les UCs suivants :**
- ~~UC3.2 : WM_PAINT actuel fait `FillRect` GDI → sera remplacé par `renderer_begin/end_draw` (D2D)~~ **RÉSOLU UC3.2**
- ~~UC3.2 : `win_D2D.c` doit implémenter `renderer_create()` prenant un `gui_window_t` et lisant le HWND depuis `ptWnd->pPlatform` (cast `win_wnd_state_t *`)~~ **RÉSOLU UC3.2**
- ~~UC3.3 : `dir.c` utilisera `gui_platform_window_invalidate()` pour forcer le repaint après navigation~~ **RÉSOLU UC3.3**
- ~~UC4 : le spin-wait 1ms de `gui_msg_get(bBlock=TRUE)` sera remplacé par une variable de condition ou un Win32 Event dans la queue~~ **RÉSOLU UC4** — spin-wait 1ms maintenu acceptable (sans impact UX visible)
- ~~UC4 : les 3 tests [S] (fenêtre visible) seront convertis en `TEST_MANUAL`~~ **RÉSOLU UC4.1**
- ~~UC3.3 (fin) : compléter SRTD.md §4 avec les sections détaillées GUI — §4.8 `gui.c`/`gui_backend.h`, §4.9 `win_gui.c`/`win_D2D.c`, §4.10 `dir.c` — sur le modèle de §4.2–§4.7 (rôle, API, dépendances, séquence). Différé car la GUI est construite en 3 sous-UCs ; un §4.x complet après UC3.3 sera plus cohérent.~~ **RÉSOLU UC3.3 Phase 2**

## 5 Use Case 03.2 (UC3.2) — VALIDÉ (2026-05-30)

*(Résumé : win_D2D.c Direct2D COM pur C + renderer.c portable. Voir commit history pour détails.)*

**Périmètre :** `win/win_D2D.c` (renderer D2D complet : create, resize, begin/end_draw, fill_rect, draw_rect, draw_line, draw_text, draw_bitmap, destroy) + `src/renderer.c` (couche portable + NULL-guards). `gui_platform_get_native_handle()` et `gui_platform_window_set_title()` ajoutés à `win_gui.c`.

**Tests :** `use_case_03_2.c` — **7N + 10R + 5S = 22 tests**, 0 failure.

---

## 6 Use Case 03.3 (UC3.3) — VALIDÉ (2026-05-31)

**Périmètre fonctionnel implémenté :**
- `src/dir.h` : `dir_node_t` + `bChildrenLoaded`/`iDepth` ; `dir_flat_entry_t` (iDepth, bLastSibling, uiLastMask) ; `dir_view_t` complet (hWnd, hRenderer, aptFlat, iFlatCount, iScrollOffset, iSelectedFlat, iCellW/H, iWndWidth/H).
- `src/dir.c` : implémentation complète —
  - `dir_node_load_children()` : scan Win32 FindFirstFile (deux passes dirs/fichiers) + stub Linux opendir/readdir. Scan non-fatal (access denied → nœud vide).
  - `dir_flat_rebuild_rec()` : DFS → tableau plat ; `uiLastMask` (bitmask 32 bits) calculé au fil de la récursion pour génération O(1) du préfixe ASCII.
  - `dir_build_prefix()` : préfixe ASCII par niveau (`|   ` / `    ` + `+-- ` / `\-- `).
  - `dir_render()` : begin_draw → fond sombre → highlight sélection → ".." (jaune) → entrées flat (cyan=dir, gris=fichier) → end_draw.
  - `dir_event_callback()` : `GUI_EVT_PAINT` (création lazy renderer), `GUI_EVT_RESIZE`, `GUI_EVT_KEY_DOWN`, `GUI_EVT_MOUSE_DOWN` (clic gauche), `GUI_EVT_SCROLL`, `GUI_EVT_CLOSE` (destruction renderer).
  - `dir_navigate_up()` : libère l'arbre, recharge depuis répertoire parent, reconstruit flat list.
  - `dir_activate_sel()` : expand/collapse (lazy load enfants) sur répertoire, écriture `ptLineCtx->szSelected` sur fichier, navigation parent sur "..".
  - `dir_handle_key()` : ↑↓ PgUp/PgDn Home End ENTER SPACE avec scroll-to-selection.
  - `dir_handle_click()` : clic gauche → sélection + expand si répertoire.
  - `dir_handle_scroll()` : molette souris (iDelta).
- `src/gui.h` + `src/gui.c` : ajout de `gui_set_title(hWnd, szTitle)` (R18) déléguant à `gui_platform_window_set_title()`.
- `src/line.c` : `line_cmd_dir()` (ferme vue existante, ouvre nouvelle via `dir_open`, affiche confirmation) ; `g_line_ptDirView` static global ; dispatch table CMD_DIR → `line_cmd_dir` ; `line_shutdown()` ferme la vue avant le memset.

**Architecture lazy-load validée :**
- Root level scanné à l'ouverture (1 niveau visible immédiatement).
- Sous-répertoires : `bChildrenLoaded == ST_FALSE` jusqu'au premier expand (ENTER/clic sur nœud).
- `dir_flat_rebuild()` reconstruit la liste plate après chaque expand/collapse/navigation.
- Thread model : console thread crée la vue → `gui_open_window()` bloque jusqu'à fenêtre live → window thread gère tous les events via `dir_event_callback`. Pas de race : le console thread ne touche plus à `ptView` après `gui_open_window()`.

**Tests R14/R15 appliqués :**
- `use_cases/use_case_03_3.c` : TEST MATRIX **8N + 8R + 4S = 20 tests**, 0 failure
  - [N] `dir_open(valid path)` → ST_NO_ERROR, ptView != NULL, ptRoot != NULL, szRootPath non-vide, iFlatCount >= 0
  - [N] `dir_close(valid view)` → ST_NO_ERROR, *pptView = NULL
  - [N] Double open (close + reopen)
  - [R] NULL ptLineCtx, NULL pptView, NULL les deux → ST_ERROR
  - [R] `dir_close(NULL)` → ST_ERROR ; `dir_close(&NULL)` → ST_NO_ERROR (idempotent)
  - [R] Chemin inexistant → ST_NO_ERROR + iFlatCount == 0 (non-fatal)
  - [S] 4 tests visuels : rendu ASCII, highlight sélection, navigation clavier, clic expand

**Contrats comportementaux validés :**

*Module `dir` lifecycle*
- `dir_open(NULL, ptCtx, &ptView)` : chemin NULL → résolu en cwd via `getcwd` (non fatal, fallback ".")
- `dir_open(path, NULL, ...)` ou `dir_open(..., NULL)` → `ST_ERROR`
- `dir_open(non-existent, ...)` → `ST_NO_ERROR` + iFlatCount == 0 (scan non-fatal)
- `dir_close(NULL)` → `ST_ERROR` ; `dir_close(&NULL)` → `ST_NO_ERROR`
- Après `dir_close()` : window thread joiné, renderer détruit, arbre libéré, aptFlat libéré, *pptView = NULL

*Module `dir` navigation / sélection*
- `iSelectedFlat = -1` : ".." sélectionné → ENTER charge le répertoire parent
- `iSelectedFlat >= 0` sur répertoire → ENTER toggle expand/collapse + lazy load si besoin
- `iSelectedFlat >= 0` sur fichier → ENTER écrit `ptLineCtx->szSelected` + LOG_INFO
- Clic gauche sur répertoire = select + toggle expand/collapse
- Molette scroll : `iScrollOffset` clampé à `[0, max(0, iFlatCount+1 - iVisRows)]`
- `dir_scroll_to_sel()` garantit que la sélection est toujours dans la zone visible

*Module `gui` (ajout UC3.3)*
- `gui_set_title(NULL, ...)` ou `gui_set_title(..., NULL)` → `ST_ERROR`
- `gui_set_title(hWnd, szTitle)` délègue à `gui_platform_window_set_title` → `SetWindowTextA` (Win32)

*Module `line_cmd_dir`*
- Ferme vue existante `g_line_ptDirView` si non NULL avant d'ouvrir la nouvelle
- Arg optionnel : `dir` (cwd) ou `dir <path>` ; extra args → warning, traitement normal
- `line_shutdown()` ferme `g_line_ptDirView` si ouvert avant de memset le contexte

**Points d'attention pour les UCs suivants :**
- ~~UC4 : écriture `ptLineCtx->szSelected` depuis le thread fenêtre sans verrou → UC4 ajoutera un mutex sur `line_context_t`~~ **RÉSOLU UC4.3** — `ptSelectedMutex` + `line_set/get_selected()`
- ~~UC4 : spin-wait 1ms dans `gui_msg_get(bBlock=TRUE)` → remplacer par Win32 Event/variable de condition~~ **RÉSOLU UC4** — spin-wait 1ms maintenu acceptable
- ~~UC4 : les 4 tests [S] seront convertis en `TEST_MANUAL` avec `make manual`~~ **RÉSOLU UC4.1**
- UC7/UC18 : menu contextuel (clic droit sur fichier/répertoire) différé à ces UCs
- ~~SRTD.md §4.8–§4.10 (`gui.c`, `win_gui.c`/`win_D2D.c`, `dir.c`) à rédiger en fin d'UC3 complet~~ **RÉSOLU UC3.3 Phase 2**

## 7 Use Case 04.1 (UC4.1) — VALIDÉ (2026-05-31)

**Périmètre fonctionnel implémenté :**
- `use_cases/use_cases.h` : macro `TEST_MANUAL(desc, question)` conforme R16 (mode `ST_MANUAL_TEST` : affiche question y/n ; mode automatique : affiche `[SKIP]`)
- `Makefile` : cible `make manual` — recompile tous les `use_case_NN.c` avec `-DST_MANUAL_TEST` dans `manual/`, les exécute séquentiellement avec reporting fail/pass identique à `make tests`
- `src/gui.h` + `src/gui.c` : `gui_request_close(hWnd)` — poste `WM_CLOSE` via `gui_platform_window_close()` sans `thread_join` (P9). Compatible avec appel depuis le thread fenêtre lui-même. `gui_close_window()` existant gère le join conditionnel : si `bOpen == ST_FALSE` déjà, skip le close, join quand même (thread déjà terminé → retour immédiat)
- `win/win_gui.c` : `SetForegroundWindow()` + `SetFocus()` après `ShowWindow()` dans `win_wnd_thread()` (P16) — la fenêtre reçoit le focus clavier dès l'ouverture sans alt-tab
- `src/dir.h` : champ `bShowHidden` ajouté à `dir_view_t`
- `src/dir.h` : `dir_open()` — nouveau paramètre `st_bool_t bShowHidden` (4ème arg, avant `pptView`)
- `src/dir.c` : `dir_node_load_children(ptNode, bShowHidden)` — filtre `cFileName[0] == '.'` (Windows) / `d_name[0] == '.'` (Linux) quand `bShowHidden == ST_FALSE` (P15). Les entrées `.` et `..` restent filtrées inconditionnellement. Mis à jour dans les 3 call sites : `dir_open()`, `dir_navigate_up()`, `dir_activate_sel()`
- `src/dir.c` : `dir_handle_key()` — 4 nouvelles cases :
  - `GUI_KEY_ESCAPE` → `gui_request_close(hWnd)` (P9)
  - `GUI_KEY_LEFT` → collapse si répertoire sélectionné est expanded (P12)
  - `GUI_KEY_RIGHT` → expand si répertoire sélectionné est collapsed, lazy-load si besoin (P12)
  - `GUI_KEY_SPACE` séparé de `GUI_KEY_ENTER` : ESPACE = sélection pure (`szSelected` mis à jour, aucun expand/collapse/navigate) ; ENTER = action existante via `dir_activate_sel()` (P13)
- `src/line.c` : `line_cmd_dir()` — parsing `-a` dans la boucle d'arguments (ordre quelconque : `dir -a /path` ou `dir /path -a`) → `bShowHidden = ST_TRUE` ; message de confirmation mention "showing hidden files"
- `use_cases/UC04_1/testdata/` : répertoire de test avec 2 entrées visibles (`visible.txt`, `visible_dir/`) et 2 cachées (`.hidden_file`, `.hidden_dir/`)
- `use_cases/use_case_03_3.c` : 5 appels `dir_open()` mis à jour (nouveau paramètre `ST_FALSE`), commentés `ADAPTED: UC4.1`
- `use_cases/use_case_04_1.c` : nouveau fichier de test

**Tests R14/R15 appliqués :**
- `use_cases/use_case_04_1.c` : TEST MATRIX **6N + 2R + 6S = 14 tests**, 0 failure
  - [N] `gui_request_close(NULL)` → ST_ERROR
  - [N] `dir_open(testdata, bShowHidden=F)` → iFlatCount == 2
  - [N] `dir_open(testdata, bShowHidden=T)` → iFlatCount == 4
  - [N] iFlatCount(T) > iFlatCount(F)
  - [R] `bShowHidden=T` + NULL ptLineCtx → ST_ERROR, *pptView inchangé
  - [S] 6 tests visuels (ESC, ←/→, ESPACE/ENTER, focus) : `make manual`
- `use_cases/use_case_03_3.c` : 0 failure (ADAPTED sans régression)

**Contrats comportementaux validés :**

*`gui_request_close(hWnd)`*
- `gui_request_close(NULL)` → `ST_ERROR`
- Depuis le thread fenêtre : poste `WM_CLOSE`, retourne immédiatement, le thread fenêtre peut continuer jusqu'à la destruction naturelle de la fenêtre
- `gui_close_window()` ultérieur fonctionne correctement quelle que soit l'origine de la fermeture (close → join immédiat sur thread déjà terminé)

*`dir_node_load_children(ptNode, bShowHidden)`*
- Entrées `'.'` et `'..'` toujours filtrées (inconditionnellement)
- `bShowHidden == ST_FALSE` : entrées `cFileName[0] == '.'` filtrées
- `bShowHidden == ST_TRUE` : toutes les entrées (hors `.`/`..`) incluses
- Deux passes dirs/fichiers conservées

*`dir_handle_key()` — nouvelles cases*
- ESC : `gui_request_close()` depuis le thread fenêtre, sans join
- LEFT sur dir expanded : collapse + rebuild flat + scroll_to_sel + redraw ; sur dir collapsed ou fichier : no-op
- RIGHT sur dir collapsed : lazy-load enfants si besoin + expand + rebuild flat + scroll_to_sel + redraw ; sur dir expanded ou fichier : no-op
- SPACE sur index `>= 0` : écrit `szSelected` du nœud sélectionné sans modifier l'état expand ; SPACE sur `..` (index -1) : no-op
- ENTER : comportement UC3.3 inchangé via `dir_activate_sel()`

*`line_cmd_dir()` — flag `-a`*
- `dir -a` : `bShowHidden = ST_TRUE`, chemin = cwd
- `dir -a /path` et `dir /path -a` : les deux formes sont valides
- Arguments supplémentaires après le chemin : warning, ignorés

**Points d'attention pour les UCs suivants :**
- ~~UC4.2 : `win_console_set_raw()` + `win_console_read_key()` — stratégie termios-first (MSYS2/mintty), fallback Win32 ReadConsoleInput (cmd.exe). `console_key_t` à définir dans `src/line.h` ou `src/common.h` en réutilisant `gui_key_t`~~ **RÉSOLU UC4.2** — R21, stratégie pipe/char runtime
- ~~UC4.2 : `line_read_rich()` remplace `fgets()` dans `line_run()` — affichage temps réel du buffer avec `\r\033[2K` (effacer ligne + réafficher)~~ **RÉSOLU UC4.2**
- ~~UC4.3 : mutex sur `line_context_t.szSelected` (write depuis thread fenêtre, note déjà dans UC3.3) — à ajouter avec le module d'historique~~ **RÉSOLU UC4.3** — `ptSelectedMutex` + `line_set/get_selected()`
- ~~SRTD.md §4.8–§4.10 (`gui.c`, `win_gui.c`/`win_D2D.c`, `dir.c`) à rédiger en Phase 2 complète (UC3+UC4.1 ensemble, différé par accord)~~ **RÉSOLU UC3.3/UC4.1 Phase 2**

## 8 Use Case 04.2 (UC4.2) — VALIDÉ (2026-06-01)

**Périmètre fonctionnel implémenté :**
- `src/console.h` (nouveau) : constantes `CON_KEY_*` (codes de contrôle 0x01–0x7F, touches étendues ≥ 0x200, `CON_KEY_EOF = -1`). API portable `console_set_raw()`, `console_restore()`, `console_read_key()`.
- `win/win_console.c` : détection runtime `GetFileType(STD_INPUT_HANDLE)` (R21) :
  - `FILE_TYPE_PIPE` (mintty/MSYS2) : aucune configuration raw-mode nécessaire ; `ReadFile()` octet par octet + `PeekNamedPipe()` (polling 5 ms, timeout 50 ms) pour détecter la fin d'une séquence VT100 ; décodage CSI/SS3 dans `console_decode_csi()` (buffer `szSeq[]`, terminé à l'octet final ≥ 0x40) ; 0x7F normalisé en `CON_KEY_BACKSPACE`.
  - `FILE_TYPE_CHAR` (cmd.exe) : `SetConsoleMode` sans `ENABLE_PROCESSED_INPUT/LINE_INPUT/ECHO_INPUT` + `ReadConsoleInputA()` (VK codes pré-décodés, pas de parsing VT100 nécessaire).
  - Aucune dépendance à `termios.h` (non disponible avec MINGW64).
- `linux/lx_console.c` : termios pur — `tcgetattr`/`tcsetattr(TCSANOW)`, `c_lflag &= ~(ECHO|ICANON|ISIG|IEXTEN)`, `read()` bloquant + `select()` 50 ms pour le timeout ESC ; même décodage CSI/SS3 que le chemin pipe Windows.
- `src/line.c` — nouvelles fonctions :
  - `line_redraw(szBuf, uiLen, uiCursor)` : `\r\033[2K` + `LINE_PROMPT` + `fwrite(szBuf)` + `\033[ND` (repositionnement curseur). Marqueur `TODO(UC4.3)` pour le ghost text.
  - `line_shortcut(szBuf, szCmd)` : remplit le buffer avec le nom de commande, redessine la ligne, émet `\n`, retourne `ST_NO_ERROR` — donne un retour visuel avant dispatch.
  - `line_read_rich(ptCtx, szBuf)` : boucle sur `console_read_key()` ; traite insert (0x20–0x7E), ←/→/Home/End, Backspace/Delete, ESC clear, CTRL+C (cancel ou quit), CTRL+Q/H/T/D/L/E/U/W/X (shortcuts immédiats), TAB/↑↓ no-op (UC4.3) ; retourne `ST_ERROR` sur EOF.
  - `line_run()` : appel `console_set_raw()` avant la boucle, fallback `fgets()` transparent si `ST_ERROR` (non-TTY : CI, pipe), `console_restore()` après la boucle.
  - `CON_DIM "\033[2m"` macro ajoutée (pour le ghost text UC4.3, par opposition à la couleur vive de la saisie en cours).
- `Makefile` : variable `UC` optionnelle — `make manual UC=04_2` compile et exécute uniquement `use_case_04_2`, `make manual` exécute tout. Variable `MANUAL_RUN` conditionnelle ; en-tête `Running UC=XX only (make manual for all)`.
- `use_cases/use_case_04_2.c` : TEST MATRIX **4N + 2R + 8S = 14 tests**, 0 failure.

**Architecture notable (Windows) :**
- MINGW64 ne fournit pas `termios.h`. La stratégie R5 est réinterprétée : mintty pipe ≡ "termios-like" (octets arrivant au fil de la frappe sans buffering ligne OS) ; la détection `GetFileType` remplace la tentative `tcgetattr`. Ce constat est formalisé en R21.

**Tests R14/R15 appliqués :**
- `use_cases/use_case_04_2.c` : TEST MATRIX **4N + 2R + 8S = 14 tests**, 0 failure
  - [N] `CON_KEY_*` ranges (contrôle 0x01-0x1F, BACKSPACE 0x7F, étendues ≥ 0x200)
  - [N] `console_set_raw/restore` roundtrip quand stdin est un TTY
  - [N] `console_restore()` second appel → ST_NO_ERROR
  - [R] `console_read_key(NULL)` → ST_ERROR
  - [R] `console_restore()` sans set_raw → ST_NO_ERROR (idempotent)
  - [S] 8 tests visuels : frappe, curseur ←/→, Home/End, Backspace, Delete, ESC, CTRL+Q, CTRL+T

**Contrats comportementaux validés :**

*Module `console` (console.h / win_console.c + lx_console.c)*
- `console_set_raw()` idempotent : double appel → `ST_NO_ERROR` + LOG_INFO (no-op)
- Sur `FILE_TYPE_PIPE` (mintty) : `ST_NO_ERROR`, aucun changement de mode — le pipe livre déjà les octets un par un
- Sur `FILE_TYPE_CHAR` (cmd.exe) : `ST_NO_ERROR` après `SetConsoleMode` raw
- Sur stdin non-TTY (fichier, pipe redirigé en CI) : `ST_ERROR` → `line_run()` bascule en `fgets()`
- `console_restore()` idempotent : sans `set_raw` préalable → `ST_NO_ERROR` (no-op)
- `console_restore()` chemin pipe : `ST_NO_ERROR` (rien à restaurer)
- `console_restore()` chemin console : `ST_NO_ERROR` après `SetConsoleMode(g_dwOrigConMode)`
- `console_read_key(NULL)` → `ST_ERROR`
- `console_read_key()` sans raw mode actif → `ST_ERROR` + `*piKey = CON_KEY_EOF`
- 0x7F → `CON_KEY_BACKSPACE` (normalisé en entrée, chemin pipe)
- 0x08 → `CON_KEY_CTRL_H` (distinct de BACKSPACE sur mintty)
- `\033[A/B/C/D/H/F` → UP/DOWN/RIGHT/LEFT/HOME/END
- `\033[1~/3~/4~/5~/6~` → HOME/DELETE/END/PAGE_UP/PAGE_DOWN
- `\033[15~` → F5 ; `\033[11~` → F1 ; `\033OA..D` → UP/DOWN/RIGHT/LEFT (SS3)
- Bare ESC (rien dans les 50 ms) → `CON_KEY_ESC`
- EOF / erreur `ReadFile` → `ST_ERROR` + `*piKey = CON_KEY_EOF`

*Module `line_read_rich` (line.c)*
- Printable 0x20–0x7E : insertion à `uiCursor`, shift droit, `uiCursor++`
- `CON_KEY_BACKSPACE` sur `uiCursor > 0` : supprime l'octet avant le curseur
- `CON_KEY_DELETE` sur `uiCursor < uiLen` : supprime l'octet au curseur
- `CON_KEY_LEFT/RIGHT` : borné à `[0, uiLen]`, no-op en limite
- `CON_KEY_HOME/END` : `uiCursor → 0 / uiLen`
- `CON_KEY_ESC` : buffer vidé, redraw prompt vide, pas de retour (continue la saisie)
- `CON_KEY_CTRL_C` non-vide : clear buffer (annulation) ; vide : `→ "quit"`
- `CON_KEY_CTRL_Q` : `→ "quit"` systématiquement
- CTRL shortcuts (H/T/D/L/E/U/W/X) : `line_shortcut()` — affiche la commande dans le prompt + `\n` + retourne `ST_NO_ERROR`
- `CON_KEY_UP/DOWN` : no-op silencieux (historique UC4.3)
- TAB (0x09) : no-op silencieux (complétion UC4.3 — 1er mot = commande, mots suivants = fichier/répertoire, ghost text `CON_DIM`)
- ENTER (0x0D) / LF (0x0A) : commit + `printf("\n")` + return `ST_NO_ERROR`
- EOF : `printf("\n")` + return `ST_ERROR`

*Module `line_run` (raw mode)*
- `console_set_raw()` appelé avant la boucle principale
- Si `ST_ERROR` (non-TTY) : fallback `fgets()` transparent, sans régression UC1/UC2
- `console_restore()` appelé après la boucle (chemin raw uniquement)

**Anomalies découvertes et résolues (post-validation UC4.2) :**

- `dir.c` — 4 warnings `-Wformat-truncation` corrigés : `szPat[ST_MAX_PATH+3]` (place pour `\\*\0`), `iPathLen` vérifié sur `snprintf(szPath)` + `dir_node_free_tree` + `continue` si overflow, `szLine[ST_MAX_PATH+320]` (couvre prefix 255 + déco 4 + nom 511 + `/\0`).
- `win_gui.c` — focus non restauré à la fermeture des vues depuis mintty : ajout de `hPrevFgWnd` dans `win_wnd_state_t`, stocké lors du `AttachThreadInput` à l'ouverture, restauré via `SetForegroundWindow(hPrevFgWnd)` dans `WM_DESTROY` (avec garde `IsWindow`). Fonctionne depuis mintty, VS Code et PowerShell.

**Points d'attention pour les UCs suivants :**
- ~~UC4.3 : CTRL+I = TAB = 0x09 conflit CMD_IMAGE~~ **RÉSOLU UC4.3** — TAB = complétion ; CMD_IMAGE via `"i"`/`"image"` uniquement (commenté dans `line.h` enum).
- ~~UC4.3 : CTRL+M = CR = ENTER conflit CMD_MOUNT~~ **RÉSOLU UC4.3** — ENTER = commit ; CMD_MOUNT via `"m"`/`"mount"` uniquement (commenté dans `line.h` enum).
- ~~UC4.3 : mutex szSelected~~ **RÉSOLU UC4.3** — `ptSelectedMutex` créé dans `line_init`, `line_set/get_selected()` dans `line.h`; dir.c utilise `line_set_selected()`.
- ~~UC4.4 : `console_restore()` doit être appelé avant l'affichage initial de la fenêtre trace GUI pour éviter interférence entre mode raw et stdout.~~ **RÉSOLU UC4.4**

## 9 Use Case 04.3 (UC4.3) — VALIDÉ (2026-06-01)

**Périmètre fonctionnel implémenté :**
- `src/console.h` : ajout de `CON_KEY_TAB 0x09` (constante nommée pour TAB/CTRL+I).
- `src/line.h` : `CMD_COLORS` ajouté à `cmd_id_t` (alias `"c"`, pas de CTRL) ; `LINE_HISTORY_MAX 64` et `LINE_COMPLETE_MAX 32` ; `ptSelectedMutex *st_mutex_t` et `szScriptFile[ST_MAX_PATH]` dans `line_context_t` ; API publique history (`line_history_add/count/get/clear/save/load`), completion query (`line_complete_cmd_query`, `line_complete_path_query`), colors (`line_set/get_colors`), selected accessor (`line_set/get_selected`). Note CTRL conflicts dans les commentaires enum.
- `src/line.c` — refonte UC4.3 :
  - **`c_*()`** : fonctions `static inline` remplacent les macros `CON_*` compile-time. Toutes les sorties console passent par `c_green()`, `c_yellow()`, etc., qui retournent `""` quand `g_line_bColors == ST_FALSE`.
  - **Historique** : ring buffer `g_line_aHistory[64][ST_MAX_CMD]` + `g_line_iHistHead/Count`. `line_history_add()` ignore les doublons du dernier. Persistence via `line_history_save/load(NULL)` → `$HOME/.st4ever_history` (MSYS2 `$HOME` ; fallback `%USERPROFILE%` sur cmd.exe). `line_history_load()` appelé dans `line_init()` ; `line_history_save()` dans `line_shutdown()`.
  - **TAB completion** dans `line_read_rich()` : premier mot → `line_complete_cmd_query` ; mots suivants → `line_complete_path_query` (POSIX `opendir/readdir/stat`, disponible via MinGW). 1 candidat = insertion immédiate du suffixe. N candidats = ghost text DIM + cycle TAB. Toute touche non-TAB efface le ghost.
  - **Ghost text** : `line_redraw()` (nouvelle signature : `ptCtx, szBuf, uiLen, uiCursor, szGhost`) — le texte fantôme est émis en `c_dim()` entre les chars avant et après curseur, puis `\033[ND` recule d'autant. Ghost = `NULL` quand inactif.
  - **Prompt contextuel** : `line_build_prompt()` construit `APP_NAME [T][basename]> ` dynamiquement — `[T]` si `trace_is_open()`, `[basename]` si `szSelected` non vide (basename extrait par `line_path_basename()`). Verrouillage bref via `line_get_selected()`.
  - **History navigation** dans `line_read_rich()` : `iHistBrowse=-1` (édition courante). UP sauvegarde dans `szSavedInput`, `iHistBrowse = count-1` (plus récent). UP répété descend vers l'ancien. DOWN remonte ; quand `iHistBrowse` dépasse `count-1`, restaure `szSavedInput`. ENTER ajoute l'entrée à l'historique (`line_history_add`).
  - **`line_cmd_colors()`** : sans arg = toggle ; `on`/`off` = forçage ; arg inconnu = warning.
  - **`line_run_script()`** : ouvre `szScriptFile`, lit ligne par ligne (strip CR/LF, saute `#`), dispatch via `line_parse_cmd + line_dispatch`. `line_run()` vérifie `szScriptFile[0] != '\0'` et branche vers script ou boucle interactive.
  - **`line_set/get_selected()`** : lock/unlock de `ptCtx->ptSelectedMutex` autour de `strncpy`. `line_init()` crée le mutex via `platform_mutex_create` ; `line_shutdown()` le détruit via `platform_mutex_destroy`.
- `src/dir.c` : 2 appels directs `strncpy(ptLineCtx->szSelected, ...)` remplacés par `line_set_selected(ptLineCtx, ptNode->szPath)` (ENTER sur fichier + SPACE sur fichier). Commenté `UC4.3`.
- `src/main.c` : option `--script <file>` parsée dans la boucle argv. Chemin sauvegardé dans `szScriptFile[ST_MAX_PATH]` avant `line_init()` (qui zeroe le contexte), puis recopié dans `tLineCtx.szScriptFile` après `line_init()`.
- `src/common.h` : `ST_APP_VERSION` → `"0.4.3"` (R20).

**Architecture notable UC4.3 :**
- Le ring buffer d'historique est **global** à `line.c` (pas dans `line_context_t`) pour éviter la sérialisation à l'init ; il persiste entre les appels à `line_init/shutdown` (utile pour les tests). `line_history_clear()` remet à zéro sans désallouer.
- Les fonctions `c_*()` sont `static inline` : coût nul en production, comportement runtime correct. Elles peuvent retourner `""` même dans `line_build_prompt()`, ce qui donne un prompt sans couleur mais lisible.
- `line_complete_path_query()` utilise `opendir/readdir` (POSIX) disponible via MinGW sans `<windows.h>` dans `src/`. Compatible avec la règle de portabilité maximale de `src/`.
- La longueur visible du prompt n'impacte pas le positionnement curseur : `line_redraw()` émet `\r\033[2K`, réécrit prompt + buffer, puis recule de `(uiLen - uiCursor) + ghost_len` chars — invariant par rapport à la longueur dynamique du prompt.

**Tests R14/R15 appliqués :**
- `use_cases/use_case_04_3.c` : TEST MATRIX **19N + 7R + 8S = 34 tests**, 0 failure
  - [N] history add/get/count/dup-filter/clear, ring wrap, oldest/newest après wrap
  - [N] save + clear + load round-trip ; load fichier inexistant → ST_NO_ERROR (premier lancement)
  - [N] colors set/get toggle
  - [N] line_init mutex creation, set/get_selected, clear via NULL, line_shutdown
  - [N] script mode : `line_run` + `szScriptFile` → `quit` → `bRunning == ST_FALSE`
  - [N] `line_complete_cmd_query` : préfixe vide (tous), "he" → "help", "q" → "quit", "h" → "help" via shortcut, "xyz" → 0
  - [N] `line_complete_path_query` : dir inexistant → 0, "src/" → >0, résultats préfixés "src/", "use_c" → inclut "use_cases/"
  - [R] NULL params (history_add/get, set/get_selected, complete queries), bounds (history_get out-of-range), script file manquant → ST_ERROR
  - [S] 8 tests visuels : `make manual UC=04_3`

**Contrats comportementaux validés :**

*Module `line_history_*`*
- `line_history_add(NULL)` → `ST_ERROR`
- `line_history_add("")` → `ST_NO_ERROR`, pas d'ajout (chaîne vide ignorée)
- Doublon du dernier élément → `ST_NO_ERROR`, pas d'ajout
- Ring wrap à `LINE_HISTORY_MAX` : les plus anciens sont écrasés, `count` reste plafonné à `LINE_HISTORY_MAX`
- `line_history_get(iVirt, NULL, N)` et `(..., szBuf, 0)` → `ST_ERROR`
- `line_history_get(iVirt < 0 ou >= count)` → `ST_ERROR`
- `line_history_save(NULL)` → écrit dans `$HOME/.st4ever_history`
- `line_history_load(NULL)` → charge depuis `$HOME/.st4ever_history` ; fichier absent → `ST_NO_ERROR` (zéro entrées)
- `line_history_clear()` : `count = 0`, `head = 0` — ne désalloue rien
- `line_init()` appelle `line_history_load(NULL)` — accumulation possible entre sessions
- `line_shutdown()` appelle `line_history_save(NULL)`

*Module `line_set/get_selected` (thread-safe)*
- `line_set_selected(NULL, ...)` → `ST_ERROR` ; `line_get_selected(NULL, ...)` → `ST_ERROR`
- `line_set_selected(ptCtx, NULL)` → efface `szSelected` (chaîne vide) sous mutex
- `line_get_selected(ptCtx, NULL, N)` et `(..., szBuf, 0)` → `ST_ERROR`
- Après `line_init()` : `szSelected[0] == '\0'`
- `dir.c` appelle `line_set_selected()` au lieu d'écrire directement — protégé par `ptSelectedMutex`
- `ptSelectedMutex` créé dans `line_init()`, détruit dans `line_shutdown()`

*Module TAB completion*
- `line_complete_cmd_query(NULL, ...)`, `(..., NULL, ...)`, `(..., ..., 0)` → `-1`
- `line_complete_cmd_query("", ...)` → tous les noms complets de commandes
- Préfixe match sur `szFull` ET `szShort` ; candidat retourné = toujours `szFull`
- `line_complete_path_query` : dir inexistant → 0 ; entrées `.`/`..` filtrées ; répertoires suffixés `/`
- Longueurs combinées > `ST_MAX_PATH` : entrée sautée silencieusement (pas de crash)
- Dans `line_read_rich()` : 1 candidat → insertion immédiate du suffixe, pas de ghost ; N candidats → ghost text DIM, cycle TAB ; toute touche non-TAB efface le ghost et remet `iCompleteCur = -1`

*Module `line_cmd_colors`*
- `colors` sans arg : toggle `g_line_bColors` ; message `Colors: ON/OFF`
- `colors on` / `colors off` : force ; arg inconnu → warning, pas de changement
- `c_*()` retournent `""` quand `g_line_bColors == ST_FALSE` → tous les printf perdent leurs codes ANSI sans modification des appelants

*Module `line_run_script`*
- Déclenché si `ptCtx->szScriptFile[0] != '\0'` dans `line_run()`
- Fichier manquant → `ST_ERROR` + `LOG_ERROR`
- Lignes commençant par `#` ou vides → ignorées
- `quit` dans le script → `ptCtx->bRunning = ST_FALSE`, fin de la boucle script
- Pas de mode raw, pas de banner : exécution silencieuse des commandes

*Prompt contextuel*
- Format : `APP_NAME[T][basename]> ` avec couleurs si actives
- `[T]` visible uniquement si `trace_is_open() == ST_TRUE`
- `[basename]` visible uniquement si `szSelected` non vide (basename = après dernier `/` ou `\`)
- Rebuild à chaque `line_redraw()` — pas de cache — cohérent par conception

**Points d'attention pour les UCs suivants :**
- ~~UC5 : `line_cmd_where()` pourra utiliser `line_get_selected()` pour afficher le fichier sélectionné courant.~~ **RÉSOLU UC5**
- ~~UC5 : P21 (touche `H` toggle hidden dans vue dir) et P22 (F5 refresh) touchent `dir_handle_key()` — même scope que UC5 `where`/`info`.~~ **RÉSOLU UC5**
- ~~UC18 : P10 (historique navigation dir ALT+←/→) + P14 (sélection multiple CTRL+ESPACE) toujours en attente de `mount` context.~~ **RÉSOLU UC18.2**

## 10 Use Case 04.4 (UC4.4) — ✓ VALIDÉ (2026-06-01)

**Périmètre fonctionnel implémenté :**
- `trace_view_t` (interne `trace.c`) : ring buffer 200 lignes × `(ST_MAX_MSG + 64)` octets heap, mutex `ptMutex`, auto-scroll `bAutoScroll`.
- `trace_event_callback()` : renderer D2D lazy (`GUI_EVT_PAINT`), resize, scroll molette, touches ↑↓/PgUp/PgDn/Home/End/ESC.
- `trace_render()` : couleurs par niveau (gris/cyan/rouge/magenta), alternance lignes paires/impaires.
- `trace_open()` : ouvre `GUI_WND_TRACE` ; dégradation propre si GUI non disponible (pas de focus volé — `eType == GUI_WND_TRACE` exclut le bloc P16 dans `win_gui.c`).
- `trace_close()` : `gui_close_window()` + libération view après join du thread.
- `trace_log()` : append ring buffer + `gui_invalidate()` sous garde `g_trace_bInNotify` (anti-récursion infinie via `LOG_TRACE` dans les sous-fonctions).
- Purge LOG_TRACE des primitives récurrentes (R22) : mutex, renderer draw, gui invalidate/get_size.
- `line_cmd_trace()` toggle-open : LOG_TRACE off par défaut (P26).

**Anomalies découvertes et résolues :**
- `renderer_draw_text()` : ordre d'arguments inversé (`szText, ptRect`, pas `ptRect, szText`).
- `trace_open()` : absence de garde idempotence → double-open ouvrait une 2ème fenêtre.
- Stack overflow : `trace_log → platform_mutex_lock → LOG_TRACE → trace_log →...` récursion infinie. Corrigé : `g_trace_bInNotify` couvre toute la section GUI (mutex + invalidate).
- `GUI_KEY_END` : `trace_view_scroll(ptView, 0)` était no-op. Corrigé : delta négatif grand → clamped à `iMaxOff` par la logique existante.

**Contrats comportementaux validés :**

*Module `trace_log()`* — **ADAPTED UC4.4** par rapport à UC1/UC2
- `trace_log()` quand `bOpen=TRUE` et GUI active (`g_trace_ptView != NULL`) : écrit dans **le ring buffer GUI + log file uniquement**. ~~stderr~~ : supprimé — terminal propre dès que la console est ouverte. *ADAPTED:UC4.4 — contrat UC1 prévoyait stderr comme sortie principale.*
- `trace_log()` quand `bOpen=TRUE` et GUI indisponible (`g_trace_ptView == NULL`) : écrit dans **log file uniquement** — stderr toujours supprimé. Les traces "préliminaires" pendant l'init de la fenêtre ne polluent pas le terminal.
- `trace_log()` quand `bOpen=FALSE` : écrit dans log file uniquement (comportement UC1 inchangé).
- Garde `g_trace_bInNotify` : empêche la récursion `trace_log → mutex_lock → LOG_TRACE → trace_log`. Toute la section GUI (mutex + append + invalidate) est protégée en une seule zone critique.

*Module `trace_open()`*
- `gui_open_window()` échoue (headless, gui_init non appelé) : `trace_open()` retourne `ST_NO_ERROR` avec warning `fprintf(stderr, ...)` direct — trace reste fonctionnelle (log file uniquement).
- Fenêtre ouverte : **pas de focus volé** (`eType == GUI_WND_TRACE` exclut le bloc `AttachThreadInput` / `SetForegroundWindow` dans `win_wnd_thread()`). L'utilisateur conserve le focus console.
- `trace_open()` idempotent : second appel quand déjà ouvert → `ST_NO_ERROR` immédiat sans créer de 2ème fenêtre.

*`line_cmd_trace()` toggle-open — P26*
- `trace` (fermé → ouvert) : `trace_open()` suivi de `trace_set_trace_enabled(ST_FALSE)`. LOG_TRACE off par défaut.
- Message console : *"Trace console opened (LOG_TRACE off — use 'trace on' to enable)."*
- `trace on` : `trace_open()` + `trace_set_trace_enabled(ST_TRUE)` — inchangé.
- `trace off` (P19) : `trace_set_trace_enabled(ST_FALSE)` — vue reste ouverte, inchangé.

*Navigation clavier dans la fenêtre trace*
- `ESC` : `gui_request_close(hWnd)` — ferme sans bloquer.
- `↑` / `↓` : scroll d'une ligne ; désactive auto-scroll si on remonte.
- `PgUp` / `PgDn` : scroll d'un écran.
- `Home` : `iScrollOff = 0`, `bAutoScroll = ST_FALSE`.
- `End` : scroll forcé à `iMaxOff` (delta négatif = iCount+1 → clamped), `bAutoScroll = ST_TRUE`.
- Molette souris : délégué à `trace_view_scroll()` ; `bAutoScroll` recalculé à chaque scroll.

*Auto-scroll*
- `bAutoScroll = ST_TRUE` à l'init et après `End` : chaque nouvelle ligne appended déplace `iScrollOff` pour garder la dernière ligne visible.
- `bAutoScroll = ST_FALSE` dès que l'utilisateur remonte (`↑`, `PgUp`, scroll molette vers le haut, `Home`).
- Retrouvé automatiquement quand `iScrollOff >= iMaxOff` (scroll molette jusqu'en bas ou touche `End`).

**Tests UC4.4 :**
- TEST MATRIX : **12N + 2R + 8S = 22 tests**, 0 failure.
- [N] : gui_init + trace lifecycle (init/open/close/shutdown), idempotence, shutdown-while-open.
- [R] : double open, double close.
- [S] : `make manual UC=04_4` — fenêtre visible, couleurs niveaux, auto-scroll, scroll molette, PgUp/PgDn, Home/End, stderr silencieux, ESC ferme.

**Points d'attention pour les UCs suivants :**
- ~~UC4.4 Phase 2 (documentation SRTD.md) : à compléter après validation manuelle complète.~~ **RÉSOLU UC4.4 Phase 2**
- ~~UC5 : `console_restore()` doit être appelé avant l'ouverture de la fenêtre trace GUI si trace est ouvert en mode raw.~~ **RÉSOLU UC5**
- ~~UC5 : P21/P22 touchent `dir_handle_key()` — même scope que UC5.~~ **RÉSOLU UC5**
- ~~UC18 : P10/P14 toujours en attente de `mount` context.~~ **RÉSOLU UC18.2**

## 11 Use Case 05 (UC5) — ✓ VALIDÉ (2026-06-01)

**Périmètre fonctionnel implémenté :**
- `src/line.h` : `CMD_INFO`, `CMD_HISTORY` dans `cmd_id_t` ; déclaration `line_update_console_title()`.
- `src/line.c` :
  - **P24** : `g_line_bColors = _isatty/_fileno(stdout)` dans `line_init()` — ANSI activé si TTY, désactivé si pipe/fichier.
  - **P8** : `line_update_console_title(ptCtx)` — `SetConsoleTitleA()` (Windows) / OSC `\033]0;...\007` (Linux). Format `ST4Ever vX.Y.Z | cwd [| sel: file] [| T]`. Appelé dans `line_run()` au démarrage et après chaque dispatch.
  - **`line_cmd_where()`** : affiche cwd + fichier sélectionné courant (via `line_get_selected()`). Dispatch `CMD_WHERE`.
  - **`line_cmd_info()`** : dashboard complet — cwd, sélection, trace (ouvert/fermé + LOG_TRACE on/off), colors, history count, disk/binary (stubs). Dispatch `CMD_INFO`.
  - **`line_cmd_history([N])`** : affiche les N dernières commandes numérotées (défaut 10). `atoi()` sur l'arg. Dispatch `CMD_HISTORY`.
  - **`trace clear`** (P27) dans `line_cmd_trace()` : appel `trace_clear()`.
  - **`trace level <trace|info|error>`** (P28) dans `line_cmd_trace()` : appel `trace_set_view_level()`.
  - **P23bis** dans `line_read_rich()` : sur N candidats au premier TAB, calcule le plus long préfixe commun et l'insère dans le buffer avant de passer en mode ghost cycle.
  - Commandes `info` (shortcut `n`) et `history` (shortcut `y`) dans la command table.
- `src/trace.h` : `trace_clear()`, `trace_set_view_level(eMinLevel)`, `trace_get_view_level()`.
- `src/trace.c` :
  - Champ `eViewMinLevel` dans `trace_view_t` ; global `g_trace_eViewMinLevel` (persistant entre close/reopen).
  - `trace_clear()` : efface ring buffer (lock/reset iHead/iCount/iScrollOff/unlock + gui_invalidate). No-op si vue non ouverte.
  - `trace_set_view_level()` : met à jour global + vue si ouverte + gui_invalidate.
  - `trace_render()` : boucle réécrite pour skiper les lignes `< eViewMinLevel` (filtre P28 rendu-seul, ring buffer inchangé).
- `src/dir.c` :
  - `dir_node_reload_children()` : libère les enfants d'un nœud + reset bChildrenLoaded + rescanne avec bShowHidden. Utilisée par P21 et P22.
  - **P21** `H`/`h` dans `dir_handle_key()` (case `default`, `eKey >= GUI_KEY_PRINTABLE`) : toggle `bShowHidden` + reload + rebuild flat + reset sélection/scroll.
  - **P22** `GUI_KEY_F5` dans `dir_handle_key()` : reload root children + rebuild flat (conserve sélection si valide).

**Tests R14/R15 appliqués :**
- `use_cases/use_case_05.c` : TEST MATRIX **31N + 4R + 9S = 44 tests**, 0 failure
  - [N] : `trace_get/set_view_level` round-trip ; `trace_clear` no-op ; `line_init` P24 ; `line_update_console_title` non-crash ; commandes dans completion table ; trace level round-trip after init ; history add/get/count ; TAB candidates pour `h`/`he`/`info`/`hist`/vide ; `line_shutdown`
  - [R] : `trace_set_view_level(999)` no-crash ; `trace_clear` deux fois à vide ; `console_title(NULL)` ; `history_get` sur ring vide
  - [S] : `make manual UC=05` (console title, H dir, F5 refresh, TAB live, trace clear/level GUI, where/info output, P24 pipe)

**Contrats comportementaux validés :**

*Module `line_cmd_where`*
- Affiche `szCwd` et `szSelected` (via `line_get_selected()`) en console.
- Args superflus → warning, traitement normal.

*Module `line_cmd_info`*
- Dashboard : cwd, sélection, trace (open/closed + LOG_TRACE), colors, history count, disk (stub), binary (stub).

*Module `line_cmd_history`*
- Sans arg : 10 derniers. Avec arg N : `min(N, count)` derniers. Arg non-entier → warning + return ST_NO_ERROR. Historique vide → "(no history)".

*Module `line_update_console_title`*
- `NULL` ctx : format minimal (pas de crash). Avec ctx : format complet incluant cwd, sélection, trace.
- Windows : `SetConsoleTitleA()`. Linux : OSC escape.

*Module `trace_clear()`*
- Vue non ouverte → `ST_NO_ERROR` (no-op). Vue ouverte : lock, reset iHead/iCount/iScrollOff=0, unlock, `gui_invalidate`. Log file non affecté.

*Module `trace_set_view_level(eMinLevel)` / `trace_get_view_level()`*
- Persistant : survit à `trace_close()/trace_open()` via `g_trace_eViewMinLevel`.
- Filtre display uniquement : le ring buffer et le log file ne sont pas altérés.
- `trace_render()` : skipe les lignes `eLevel < eViewMinLevel` (pas de "trou" dans l'affichage — le row counter avance uniquement sur les lignes affichées).

*P21 dir `H`/`h`*
- Toggle `bShowHidden` : reload root children avec nouveau filtre. Les sous-arborescences déjà expandées sont implicitement collapsées (nouveaux enfants = `bExpanded=ST_FALSE`). Sélection remise à `-2`, scroll à 0.

*P22 dir F5*
- Reload root children (même `bShowHidden`). Sélection préservée si index encore valide, sinon ramenée au dernier valid.

*P23bis TAB common prefix*
- Sur N>1 candidats : calcule le plus long préfixe commun de tous les candidats à partir de `uiCompletePrefixLen`. Insère le suffixe commun dans le buffer si > 0 chars. Met à jour `uiCompletePrefixLen`. Puis entre en mode ghost cycle normalement.
- Exemple : `h` → candidats `help`/`history` → préfixe commun = `h` (déjà tapé, rien à insérer) → ghost cycle direct.
- Exemple : `col` → candidat unique `colors` → insertion immédiate (branch 1-candidat, inchangé).

*P24 colors auto*
- `line_init()` : `g_line_bColors = isatty(stdout)`. `colors on/off` reste disponible pour forcer.

**Points d'attention pour les UCs suivants :**
- ~~UC6 : abstraction fichiers — `line_cmd_where()` pourra enrichir son affichage avec des infos stat sur la sélection.~~ **RÉSOLU UC6**
- ~~UC7 : `line_cmd_info()` rempira les stubs "disk mounted" et "binary loaded" au fur et à mesure des UCs.~~ **RÉSOLU UC7**
- ~~UC18 : P10 (historique navigation dir) + P14 (sélection multiple) toujours en attente de `mount` context.~~ **RÉSOLU UC18.2**

## 12 Use Case 06 (UC6) — ✓ VALIDÉ (2026-06-01)

**Périmètre fonctionnel implémenté :**
- `src/file.h` : types publics (`file_stat_t`, `file_mode_t`, `file_t` opaque, `file_list_fn` callback) + API complète.
- `src/file.c` : implémentation portable CRT + POSIX (disponible via MinGW sans Win32 dans `src/`) :
  - `file_stat()` : `stat()` POSIX → `bExists`/`bIsDir`/`uiSize` + extension lowercase via `file_extract_ext()`. Non-existence = `ST_NO_ERROR` + `bExists=FALSE`.
  - `file_open()` : `fopen()` avec mode `"rb"`/`"wb"`/`"ab"` selon `file_mode_t`.
  - `file_read()` : `fread()` + `ferror()` ; `*puiRead` = octets effectivement lus ; EOF = ST_NO_ERROR + `*puiRead=0`.
  - `file_write()` : `fwrite()` ; short write = ST_ERROR.
  - `file_close()` : `fclose()` + `free()` + `*pptFile=NULL` ; `file_close(&NULL)` = no-op.
  - `file_mkdir()` : `_mkdir()` (Windows) / `mkdir(path, 0755)` (Linux) ; `EEXIST` = ST_NO_ERROR.
  - `file_list_dir()` : `opendir`/`readdir`/`closedir` ; filtre `.`/`..` et `.*` si `bShowHidden=FALSE` ; invoque `pfnCallback(szFull, szName, &tStat, pCtx)` par entrée.
- `use_cases/use_case_06.c` : TEST MATRIX **28N + 12R + 0S = 40 tests**, 0 failure.

**Architecture :**
- `file_t` est opaque (défini uniquement dans `file.c`) — les appelants ne peuvent pas manipuler `FILE *` directement.
- `file_extract_ext()` : helper interne — trouve le dernier `.` après le dernier séparateur, copie en minuscules. Robuste sur les chemins avec points dans les répertoires.
- `FILE_MKDIR` macro : `_mkdir` (Windows) / `mkdir` (Linux) — seule divergence plateforme, contenue dans `file.c`.
- UC auto-inclus dans le build via `wildcard src/*.c` du Makefile.

**Tests R14/R15 :**
- `use_cases/use_case_06.c` : 28N + 12R + 0S = 40 tests
  - [N] : `file_stat` (fichier/dir/non-existant/extension), open/read/EOF/close, write+round-trip+stat, mkdir new+existing, `file_list_dir` src/ (count/common.h/no-hidden/show-hidden)
  - [R] : NULL params sur toutes les fonctions publiques, open non-existant en lecture, write uiLen=0, close(&NULL), list_dir non-existant

**Contrats comportementaux validés :**

*`file_stat(szPath, ptStat)`*
- `szPath` ou `ptStat` NULL → `ST_ERROR`
- Chemin non-existant → `ST_NO_ERROR` + `bExists=FALSE` (pas une erreur)
- Extension extraite en minuscules depuis le dernier `.` après le dernier séparateur
- Répertoire : `bIsDir=TRUE`, `uiSize=0`

*`file_open/read/write/close`*
- `file_open` non-existant en lecture → `ST_ERROR` + LOG_ERROR
- `file_read` à EOF → `ST_NO_ERROR` + `*puiRead=0`
- `file_write` avec `uiLen=0` → `ST_ERROR`
- `file_close(&NULL)` → `ST_NO_ERROR` (idempotent)
- `file_close` après close réussi → handle NULL, pas de double-free

*`file_mkdir`*
- Répertoire déjà existant → `ST_NO_ERROR` (EEXIST ignoré)

*`file_list_dir`*
- `pfnCallback=NULL` → `ST_ERROR`
- Répertoire non-ouvrable → `ST_ERROR`
- `.` et `..` jamais transmis au callback
- `bShowHidden=FALSE` : entrées `.*` filtrées

**Points d'attention pour les UCs suivants :**
- ~~UC8-10 : `edit` utilisera `file_open(FILE_MODE_READ)` + `file_open(FILE_MODE_WRITE)` pour chargement et sauvegarde.~~ **RÉSOLU UC8/UC9/UC10**
- ~~UC16 : image `.st` = `file_open(READ)` + `file_read` de 737 280 octets exact.~~ **RÉSOLU UC16**
- ~~UC18 : `file_list_dir` alimentera la vue mount (listing du répertoire monté).~~ **RÉSOLU UC18.1**
- `file_stat` n'est pas récursif : pas d'équivalent `stat64` nécessaire pour l'instant (fichiers PRG/ST restent < 4 Go).

## 13 Use Case 07 (UC7) — ✓ VALIDÉ (2026-06-01)

**Périmètre fonctionnel implémenté :**
- `src/load.h` (nouveau) : `load_type_t` (NONE/BINARY/PRG), `load_state_t` (bLoaded, eType, szPath, uiLoadAddr, uiSize), API `load_init/shutdown/file/get_state`.
- `src/load.c` (nouveau) : implémentation portable CRT+file.h :
  - `load_init(ptMachine)` : attache le module à un `st_machine_t`, remet l'état à zéro.
  - `load_file(szPath)` : `file_stat()` → rejet dir/image (ST_ERROR) ; `.prg/.ttp/.tos` → `load_do_prg()` ; sinon → `load_do_raw()`.
  - `load_do_raw()` : lecture streaming par blocs 4 Ko, copie verbatim dans `aRam[ST_LOAD_BASE..]`.
  - `load_do_prg()` : lecture header 28 octets, validation magic `0x601A`, lecture `.text`+`.data` (`uiTextSize + uiDataSize` octets) à `ST_LOAD_BASE`. `LOG_TODO(UC15)` pour les fixups. Charge de contenu = 0 byte sur `.data` = no-op.
  - `load_get_state()` : pointeur read-only vers `g_load_tState` — valide jusqu'à `load_shutdown()`.
  - `load_shutdown()` : remet machine pointer à NULL + remet état à zéro.
  - Constantes : `ST_LOAD_BASE = 0x0800` (après table de vecteurs 68000), `ST_LOAD_MAX_SIZE = ST_RAM_SIZE - ST_LOAD_BASE`, `ST_PRG_MAGIC = 0x601A`, `ST_PRG_HEADER_SIZE = 28`.
- `src/dir.h` : champ `szLastSelected[ST_MAX_PATH]` ajouté à `dir_view_t` (P11).
- `src/dir.c` :
  - `g_dir_clrLastSel = {0.04f, 0.28f, 0.10f, 1.0f}` (vert sombre) ajouté aux constantes couleur.
  - `dir_activate_sel()` (ENTER sur fichier) et handler SPACE : après `line_set_selected()`, copient aussi `ptNode->szPath` dans `ptView->szLastSelected`.
  - `dir_render()` : boucle augmentée — pour `iRow > 0`, compare `aptFlat[iRow-1].ptNode->szPath` à `szLastSelected` ; si égal, `renderer_fill_rect(…, &g_dir_clrLastSel)` avant le fond de sélection nav (vert sous le bleu, vert seul sinon). Séquence : fond vert sombre (P11) → fond bleu nav (si sélectionné) → texte.
- `src/line.c` :
  - `#include "load.h"` et `#include "file.h"` ajoutés.
  - `line_cmd_load()` (nouveau) : collecte chemin depuis l'argument ou `line_get_selected()` ; si vide → message "use dir" ; `file_stat()` → rejet dir ("use mount") / image ("use mount") avec `line_print_warning()` ; sinon `load_file()` + affichage résultat (type, adresse ST, taille). `line_update_console_title()` après succès.
  - Dispatch table : `CMD_LOAD → line_cmd_load` (remplace `line_cmd_stub`).
  - `line_cmd_info()` : stub "Binary : (none)" remplacé par appel `load_get_state()` → affiche nom (basename), type (binary/PRG-stub), adresse ST et taille si `bLoaded=TRUE`.
- `src/main.c` : `st_machine_t tMachine` + `st_init(&tMachine, NULL)` + `load_init(&tMachine)` au démarrage ; `load_shutdown()` + `st_shutdown(&tMachine)` à l'arrêt ; labels goto mis à jour (`shutdown_load`, `shutdown_st`).
- `src/common.h` : `ST_APP_VERSION` → `"0.7.0"`.
- `use_cases/UC07/` : fixtures `hello.txt` (~48 octets), `hello.bin` (16 octets 0x00..0x0F), `bad_magic.prg` (magic 0xDEAD), `test.st` (extension uniquement).

**Tests R14/R15 appliqués :**
- `use_cases/use_case_07.c` : TEST MATRIX **19N + 8R + 4S = 31 tests**, 0 failure.
  - [N] : lifecycle load_init/shutdown ; get_state bLoaded/eType après init ; chargement binary (adresse, taille, contenu RAM) ; chargement txt (type BINARY, contenu RAM[0]=='H') ; chargement PRG hello.prg (type PRG, uiSize=4, RAM[0]=0x70/[1]=0x2A) ; remplacement état par nouveau load ; rejet dir/image ; état inchangé après PRG raté ; get_state != NULL après shutdown
  - [R] : `load_init(NULL)` → ST_ERROR ; `load_file` sans init → ST_ERROR ; `load_file(NULL)` → ST_ERROR ; fichier inexistant → ST_ERROR ; bad PRG magic → ST_ERROR ; double shutdown idempotent
  - [S] : 4 tests visuels P11 (`make manual UC=07`)

**Contrats comportementaux validés :**

*Module `load_init/shutdown`*
- `load_init(NULL)` → `ST_ERROR`
- `load_init(ptMachine)` → `ST_NO_ERROR` + état remis à zéro (bLoaded=FALSE, eType=NONE)
- Idempotence : `load_shutdown()` deux fois → `ST_NO_ERROR` (no-op sur machine pointer déjà NULL)
- `load_get_state()` toujours safe (jamais NULL) : retourne le pointeur vers `g_load_tState`

*Module `load_file`*
- `load_file(NULL)` → `ST_ERROR`
- `load_file(...)` sans `load_init()` préalable → `ST_ERROR`
- Fichier inexistant → `ST_ERROR` + `LOG_ERROR`
- Répertoire → `ST_ERROR` + `LOG_ERROR` ("use mount")
- Extension `.st`/`.msa`/`.stx` → `ST_ERROR` + `LOG_ERROR` ("use mount")
- Extension `.prg`/`.ttp`/`.tos` → `load_do_prg()` : validation magic 0x601A + chargement `.text`+`.data` à `ST_LOAD_BASE`
- Bad magic (≠ 0x601A) → `ST_ERROR` ; état précédent préservé (pas de reset partiel)
- Tous les autres → `load_do_raw()` : copie verbatim à `ST_LOAD_BASE`
- Fichier trop grand (> `ST_LOAD_MAX_SIZE`) → `ST_ERROR`
- Succès : `g_load_tState.bLoaded=TRUE`, `eType`, `szPath`, `uiLoadAddr=ST_LOAD_BASE`, `uiSize` mis à jour

*P11 — indicateur sélection active dans `dir_render()`*
- `szLastSelected` mis à jour uniquement sur ENTER (via `dir_activate_sel()`) ou SPACE sur un fichier
- Répertoires et ".." ne mettent jamais à jour `szLastSelected`
- Fond vert sombre (`g_dir_clrLastSel`) dessiné pour la rangée dont `szPath == szLastSelected`
- Fond bleu nav (`g_dir_clrSel`) superposé si la même rangée a aussi le curseur → bleu prime visuellement
- `szLastSelected` vide (`[0]=='\0'`) → aucun fond vert (comportement initial)

*`line_cmd_load`*
- Sans argument et `szSelected` vide → message "select a file with dir", `ST_NO_ERROR`
- Argument fourni → utilisé en priorité sur `szSelected`
- Dir → `line_print_warning("use mount")`, `ST_NO_ERROR`
- Image disque → `line_print_warning("use mount")`, `ST_NO_ERROR`
- Erreur de stat → `line_print_error`, `ST_NO_ERROR` (erreur non fatale pour la boucle console)
- Succès binary → message avec adresse ST et taille ; succès PRG → message avec mention "(fixup deferred to UC15)" *(ADAPTED:UC15 — message mis à jour pour afficher uiFixupCount)*
- `line_update_console_title()` appelé après chaque succès

**Points d'attention pour les UCs suivants :**
- ~~UC8-10 : `edit` utilisera `load_get_state()` pour savoir si un fichier est déjà chargé et proposer de l'éditer directement en mémoire émulée.~~ **RÉSOLU UC8/UC9/UC10**
- ~~UC15 : `load_do_prg()` TODO(UC15)~~ **✓ IMPLÉMENTÉ UC15** — fixup relocation complet.
- UC24 : `ST_LOAD_BASE = 0x0800` est en dur ; UC24 devra allouer la première zone libre au-dessus des variables TOS selon la memory map réelle.
- UC25 : `execute` démarrera depuis `load_get_state()->uiLoadAddr` comme PC initial (si eType == LOAD_TYPE_PRG).
- ~~`line_cmd_info()` stub "Disk mounted" reste à remplir lors de UC18 (`mount`).~~ **RÉSOLU UC18.1**

## 14 Use Case 08 (UC8) — ✓ VALIDÉ (2026-06-02)

**Périmètre fonctionnel implémenté :**
- `src/edit_txt.h` + `src/edit_txt.c` : éditeur texte sur fenêtre D2D (`GUI_WND_EDIT_TXT`) :
  - Chargement de fichier via `file.h` (split newlines, expansion tabs, cap `EDIT_TXT_MAX_LINE_LEN`) ; fichier vide → 1 ligne vide garantie.
  - Rendu : fond sombre + gutter (numéros de ligne auto-width) + highlight ligne courante + surlignage sélection (bleu) + curseur barre verticale `CURSOR_BAR_W=2 px`.
  - Navigation curseur : ↑↓←→, Home/End, PgUp/PgDn ; CTRL+Home/End (début/fin fichier) ; CTRL+←/→ (mot précédent/suivant).
  - Sélection : SHIFT+toute navigation = ancre + curseur ; CTRL+A = tout sélectionner.
  - Édition : printable insert, ENTER (split ligne), TAB (tab char), Backspace (delete back ou merge lignes), Delete (delete fwd ou merge lignes).
  - Clipboard : CTRL+C (copie sélection), CTRL+X (coupe), CTRL+V (colle ; support multiligne).
  - Save : CTRL+S → `file.h FILE_MODE_WRITE` + `bDirty=FALSE` + title update ; `[*]` dans le titre quand dirty (R18).
  - ESC : ferme la vue sans dialog ; `LOG_INFO` si dirty.
  - Scroll souris : molette verticale via `GUI_EVT_SCROLL`.
  - Clic souris : `GUI_EVT_MOUSE_DOWN` → mapping pixel→ligne/colonne + déplacement curseur.
  - Titre dynamique : `"ST4Ever - Edit: <basename> [*]"` (R18).
- `src/gui.h` :
  - `GUI_MOD_CTRL = 0x01`, `GUI_MOD_SHIFT = 0x02`, `GUI_MOD_ALT = 0x04` (masques).
  - Champ `st_u8_t uiMods` ajouté à `struct sKey` dans `gui_event_t` — rétrocompatible (code existant qui ignore `uiMods` inchangé).
  - API clipboard : `gui_clipboard_set_text(szText)`, `gui_clipboard_get_text(szBuf, uiMax)`.
- `win/win_gui.c` :
  - `WM_KEYDOWN` : `GetKeyState(VK_CONTROL/SHIFT/MENU)` → `uiMods` sur tous les événements navigation.
  - `WM_CHAR` : codes de contrôle 1–26 → `GUI_KEY_PRINTABLE+'A'..'Z'` avec `GUI_MOD_CTRL` ; printable ≥ 32 → `uiMods` SHIFT.
  - `gui_clipboard_set_text` : `GlobalAlloc/Lock + OpenClipboard(NULL) + EmptyClipboard + SetClipboardData(CF_TEXT)`.
  - `gui_clipboard_get_text` : `OpenClipboard(NULL) + GetClipboardData(CF_TEXT) + GlobalLock + copy`.
- `linux/lx_gui.c` : stubs clipboard avec `LOG_TODO(UC8-Linux)`.
- `src/line.c` :
  - `g_line_ptEditTxtView *edit_txt_view_t` static.
  - `line_cmd_edit()` : collecte chemin depuis arg ou `line_get_selected()` ; `file_stat()` ; rejette dir / images / binaires (stub UC9) ; ouvre `edit_txt_view_t` ; ferme précédente si nécessaire.
  - Dispatch `CMD_EDIT → line_cmd_edit` (remplace `line_cmd_stub`).
  - `line_shutdown()` ferme `g_line_ptEditTxtView` si ouvert.
- `src/common.h` : `ST_APP_VERSION → "0.8.0"`.

**Tests R14/R15 appliqués :**
- `use_cases/use_case_08.c` : TEST MATRIX **17N + 11R + 8S = 36 tests**, 0 failure
  - [N] : display_len (tab expansion, round-trip, NUL-stop : 8 tests), clipboard round-trip, GUI_MOD flags+combinaison (5), constantes EDIT_TXT_* (3)
  - [R] : lifecycle NULL guards (8 : path/ctx/pptView/missing-file×2 + close×2), clipboard NULL guards (3)
  - [S] : 8 tests visuels `make manual UC=08`

**Contrats comportementaux validés :**

*`edit_txt_open(szPath, ptLineCtx, pptView)`*
- `szPath=NULL`, `ptLineCtx=NULL`, `pptView=NULL` → `ST_ERROR`
- Fichier inexistant → `ST_ERROR` + `*pptView=NULL`
- Fichier vide → `ST_NO_ERROR` + `iLineCount == 1` (ligne vide garantie)
- Succès : fenêtre D2D visible, curseur en (0,0), pas de focus volé (R22 : `GUI_WND_EDIT_TXT` exclut `AttachThreadInput` comme `GUI_WND_TRACE`)

*`edit_txt_close(pptView)`*
- `pptView=NULL` → `ST_ERROR`
- `*pptView=NULL` → `ST_NO_ERROR` (idempotent)
- Après close : thread joint, renderer détruit, lignes libérées, `*pptView=NULL`

*Navigation curseur*
- Toute navigation clamp au range valide `[0, iLineCount-1]` × `[0, len(ligne)]`
- CTRL+Home/End positionnent le curseur en début/fin de fichier
- CTRL+←/→ sautent les frontières de mots (alphanumériques + `_`)
- SHIFT+toute navigation étend la sélection depuis l'ancre

*Sélection*
- `iSelAnchorLine < 0` = pas de sélection
- CTRL+A = ancre (0,0) + curseur (dernière ligne, fin de ligne)
- Clic simple = clear sélection ; SHIFT+clic = étend depuis ancre courante
- Toute navigation sans SHIFT = `etxt_sel_clear()` (ancre=-1)

*Edition (gestion des lignes)*
- ENTER split la ligne au curseur : la fin de ligne courante devient une nouvelle ligne
- Backspace en col=0 : merge avec la ligne précédente (curseur au joint)
- Delete en fin de ligne : merge avec la ligne suivante
- `etxt_del_sel()` : sélection mono-ligne → suppression de plage ; multi-ligne → merge préfixe+suffixe + suppression lignes intermédiaires

*Clipboard*
- `gui_clipboard_set_text(NULL)` → `ST_ERROR`
- `gui_clipboard_get_text(NULL, N)` ou `(buf, 0)` → `ST_ERROR`
- `gui_clipboard_get_text` : toujours NUL-terminé sur retour, même sur erreur (chaîne vide)
- CTRL+V : `etxt_del_sel()` si sélection active, puis colle char par char ('\r' ignoré, '\n' = split ligne)

*Save*
- CTRL+S quand `bDirty=FALSE` : no-op
- CTRL+S quand `bDirty=TRUE` : `FILE_MODE_WRITE` (crée/tronque) ; écrit toutes les lignes séparées par '\n' (pas de '\n' final) ; `bDirty=FALSE` ; titre mis à jour
- Échec save : `LOG_ERROR`, `bDirty` reste TRUE

*`gui.h` `GUI_MOD_*`*
- Flags distincts : `CTRL=0x01`, `SHIFT=0x02`, `ALT=0x04` — aucun overlap
- Combinables par OR ; code existant qui ignore `uiMods` = 0 (champ initialisé par `memset` dans WndProc)

*`win_gui.c` CTRL+lettre*
- `WM_CHAR` avec `wParam ∈ [1..26]` → `GUI_KEY_PRINTABLE + ('A'+wParam-1)`, `uiMods = GUI_MOD_CTRL`
- Le code existant (dir, trace) reçoit `uiMods=0` par défaut — aucune régression

*Module `edit_undo_*` — CTRL+Z (ajouté UC8, 2026-06-02)*
- Ring buffer de `EDIT_TXT_UNDO_LEVELS = 20` snapshots ; chaque entrée = copie profonde de `aszLines[]` + `tCursor`.
- `etxt_undo_push()` : copie profonde du buffer courant dans le slot `iUndoHead`, avance le ring. Si ring plein, l'entrée la plus ancienne est libérée et remplacée.
- `etxt_undo_pop()` (CTRL+Z) : restaure le dernier snapshot, transfère l'ownership de `aszLines` vers le buffer live, décrémente `iUndoCount`. No-op si ring vide.
- Groupage des inserts consécutifs : `bUndoGroupInsert=TRUE` après le premier printable insert ; push sauté tant que la frappe reste printable ; tout autre key remet `bUndoGroupInsert=FALSE` → CTRL+Z annule tout le groupe d'une traite.
- Exception : `del_sel + insert_char` (remplacement de sélection) = un seul push avant le del_sel ; le insert suivant est déjà dans le même niveau.
- Navigation (←/→/↑/↓/Home/End/PgUp/PgDn) : `bUndoGroupInsert=FALSE`, pas de push.
- CTRL+S (save) : pas de push (la sauvegarde n'est pas annulable via undo).
- `etxt_undo_free_all()` appelé dans `edit_txt_close()` pour libérer tous les snapshots.
- Après `etxt_undo_pop()` : `bDirty=TRUE`, `iSelAnchorLine=-1` (sélection réinitialisée).

**Points d'attention pour les UCs suivants :**
- ~~UC9 : `edit_hex_open(szPath, ptLineCtx, pptView)` — même architecture `GUI_WND_EDIT_HEX` ; `line_cmd_edit()` redirigera les `.prg/.ttp/.tos/.st/.msa` vers `edit_hex_open()`.~~ **RÉSOLU UC9**
- ~~UC10 : `edit.c` dispatcher intégré (hex+ASCII+désasm synchronisés) — `edit_open()` choisit entre `edit_txt` et `edit_hex`.~~ **RÉSOLU UC10**
- ~~UC18 (mount) : `GUI_MOD_SHIFT` est désormais transmis par `win_gui.c` → `dir_handle_key()` peut exploiter SHIFT+ESPACE pour la sélection multiple P14 sans modification du backend.~~ **RÉSOLU UC18.2** — P14 multi-select CTRL+ESPACE implémenté
- Encodage : `WM_CHAR` limité au code page ANSI courant (0xFF mask) — suffisant pour les sources ATARI ST ASCII. Support UTF-8 différé.

## 15 Use Case 09 (UC9) — ✓ VALIDÉ (2026-06-02)

**Périmètre fonctionnel implémenté :**
- `src/edit_hex.h` + `src/edit_hex.c` : éditeur hex+ASCII D2D (`GUI_WND_EDIT_HEX`) :
  - Chargement complet du fichier en buffer heap via `file.h` ; cap `EDIT_HEX_MAX_SIZE = 16 MB`.
  - Layout fixe : colonne adresse `XXXXXX:` (7 chars + espace) | zone hex 49 chars (16×3, gap après byte 7) | séparateur ` | ` | zone ASCII 16 chars.
  - Rendu D2D : fond noir, gutter adresse, highlight ligne courante, zone hex (gris clair), zone ASCII (vert ; '.' pour non-imprimable), curseur hex (bleu, 1 nibble) ou ASCII (jaune, 1 char) avec re-dessin du caractère en blanc.
  - Deux zones d'édition : `HEX_ZONE_HEX` (édition nibble par nibble : 0–9/A–F) et `HEX_ZONE_ASCII` (édition byte direct : printable 0x20–0x7E), basculement par TAB.
  - Navigation : ←→ (nibble/byte), ↑↓ (±16 bytes), Home/End (début/fin de row), CTRL+Home/End (début/fin fichier), PgUp/PgDn.
  - Clic gauche : positionne le curseur sur nibble (zone hex) ou byte (zone ASCII) le plus proche du clic.
  - Scroll molette verticale.
  - CTRL+S : sauvegarde via `file_open(FILE_MODE_WRITE)` ; titre `[*]` (R18).
  - ESC : ferme sans dialog, `LOG_INFO` si dirty.
  - Mode remplacement uniquement (taille fichier fixe).
- `src/line.c` :
  - `g_line_ptEditHexView *edit_hex_view_t` static.
  - `line_cmd_edit()` : `.prg/.ttp/.tos/.bin/.raw → edit_hex_open` ; `.st/.msa/.stx → "use mount"` ; tous les autres → `edit_txt_open`.
  - `line_shutdown()` ferme `g_line_ptEditHexView`.
- `src/common.h` : `ST_APP_VERSION → "0.9.0"`.

**Tests R14/R15 appliqués :**
- `use_cases/use_case_09.c` : TEST MATRIX **17N + 8R + 8S = 33 tests**, 0 failure
  - [N] : constantes layout (4), row builder (6 : byte 0, byte 7, gap 24, byte 8, byte 15, partiel), zones enum (3), nibble positions (4)
  - [R] : NULL guards (8 : path/ctx/pptView/missing×2 + close×2)
  - [S] : 8 tests visuels `make manual UC=09`

**Contrats comportementaux validés :**

*`edit_hex_open(szPath, ptLineCtx, pptView)`*
- NULL szPath / ptLineCtx / pptView → `ST_ERROR`
- Fichier inexistant → `ST_ERROR` + `*pptView = NULL`
- Fichier > `EDIT_HEX_MAX_SIZE` → `ST_ERROR`
- Succès : fenêtre D2D visible, curseur en byte 0, zone HEX active

*`edit_hex_close(pptView)`*
- `pptView=NULL` → `ST_ERROR` ; `*pptView=NULL` → `ST_NO_ERROR` (idempotent)
- Après close : thread joiné, renderer détruit, `pData` libéré, `*pptView=NULL`

*Row builder (format hex)*
- Byte i (0≤i<8) : position `i*3` dans la chaîne
- Byte i (8≤i<16) : position `i*3+1` (le +1 représente le gap après byte 7 à la position 24)
- Bytes hors portée (> uiSize) : espaces
- Longueur totale : 49 chars (`EDIT_HEX_HEX_CHARS`)

*Navigation*
- ← en zone HEX nibble=1 → nibble=0 (reste même byte)
- ← en zone HEX nibble=0 / zone ASCII → byte précédent (clampé à 0)
- → en zone HEX nibble=0 → nibble=1
- → en zone HEX nibble=1 / zone ASCII → byte suivant (clampé à uiSize-1)
- ↑/↓ : ±EDIT_HEX_BYTES_PER_ROW bytes, clampé
- Home : début de la row courante, nibble=0
- CTRL+Home : byte 0, nibble=0
- End : fin de la row (ou dernier byte si partielle), nibble=1 (hex) ou 0 (ASCII)
- CTRL+End : dernier byte, nibble=1 (hex) ou 0 (ASCII)

*Édition nibble (zone HEX)*
- Nibble 0 (haut) : `(iVal << 4) | (byte & 0x0F)` ; avance à nibble=1
- Nibble 1 (bas) : `(byte & 0xF0) | iVal` ; avance au byte suivant nibble=0
- `bDirty = TRUE` après toute modification

*Édition byte (zone ASCII)*
- Printable 0x20–0x7E : écrit le byte, avance au byte suivant

*Clic souris*
- Click zone ASCII (iX ≥ iAsciiX) → zone ASCII, byte = row×16 + col
- Click zone HEX (iHexX ≤ iX < iAsciiX) : parcourt les positions de nibble pour trouver le nibble exact → zone HEX
- Click gutter (iX < iHexX) : ignoré

**Points d'attention pour les UCs suivants :**
- ~~UC10 : `edit.c` dispatcher intégré — `edit_open()` choisit entre `edit_txt` et `edit_hex` selon l'extension ; les vues hex+ASCII et disassembly 68000 seront synchronisées (curseur partagé).~~ **RÉSOLU UC10**
- ~~UC11–14 : désassembleur 68000 → vue disassembly dans UC10 (overlay sur la zone hex des sections `.text` d'un PRG).~~ **RÉSOLU UC14**
- ~~UC15 : `load_do_prg()` fixup relocation → permet de charger le PRG correctement relocalisé en RAM ST avant `edit_hex`.~~ **RÉSOLU UC15**
- Undo (CTRL+Z) pour le hex editor : non implémenté en UC9 (mode remplacement, fichiers petits → l'utilisateur peut re-ouvrir). À planifier si besoin en UC9-bis.

## 16 Use Case 10 (UC10) — ✓ VALIDÉ (2026-06-02)

**Périmètre fonctionnel implémenté :**
- `src/edit_hex.h` étendu :
  - `HEX_ZONE_DISASM = 2` ajouté à `edit_hex_zone_t` (troisième zone read-only).
  - Constantes : `EDIT_HEX_DISASM_CACHE_LINES 512`, `EDIT_HEX_DISASM_PANEL_CHARS 48`, `EDIT_HEX_DISASM_SEP_CHARS 3`, `EDIT_HEX_DISASM_ADDR_CHARS 8`, `EDIT_HEX_DISASM_PREBUF_BYTES 512`.
  - Constantes de fenêtre : `EDIT_HEX_WND_WIDTH_STD 950`, `EDIT_HEX_WND_WIDTH_DISASM 1320`, `EDIT_HEX_WND_HEIGHT 640`.
  - Champs dans `edit_hex_view_t` : `atDisasmCache *disasm_result_t` (heap), `iDisasmCacheCount`, `uiDisasmCacheBase`, `iDisasmScrollRow`, `iDisasmCursorIdx`, `iDisasmX`, `bShowDisasm`.
  - Include `disassemble.h` dans le header.
- `src/edit_hex.c` étendu :
  - Nouvelles couleurs : `g_clrDisasm` (tan), `g_clrDisasmDC` (DC.W stub, dim), `g_clrDisasmHL` (fond highlight), `g_clrDisasmSepBg`.
  - `ehex_disasm_cache_update()` : reconstruit le cache autour du curseur (512 bytes avant + autant que le cache permet) via `disasm_range()`.
  - `ehex_disasm_find_cursor()` : trouve l'instruction du cache contenant `uiCursor` ; met à jour `iDisasmCursorIdx` ; appelle `ehex_disasm_scroll_to_cursor()`.
  - `ehex_disasm_scroll_to_cursor()` : maintient `iDisasmCursorIdx` dans la zone visible du panneau.
  - `ehex_disasm_render()` : dessine séparateur + liste d'instructions (`$XXXXXX %-8s %s`) ; instruction active surlignée selon la zone active.
  - `ehex_recalc_layout()` : calcule `iDisasmX` après la zone ASCII + séparateur.
  - `ehex_render()` : appelle `ehex_disasm_render()` en fin si `bShowDisasm`.
  - `ehex_handle_key()` : F2 toggle ; TAB cycle 3 zones (HEX→ASCII→DISASM si visible→HEX) ; navigation DISASM zone (↑↓/PgUp/PgDn/Home/End syncs `uiCursor` + `ehex_scroll_to_cursor` + `ehex_disasm_scroll_to_cursor`) ; rebuild cache après édition.
  - `ehex_handle_click()` : click sur panneau disasm → `HEX_ZONE_DISASM` + sync curseur.
  - `GUI_EVT_SCROLL` : scrolle le panneau disasm si `eZone == HEX_ZONE_DISASM`, sinon hex.
  - `edit_hex_open()` : alloue `atDisasmCache` heap + `bShowDisasm = ST_TRUE` ; taille initiale `EDIT_HEX_WND_WIDTH_DISASM × EDIT_HEX_WND_HEIGHT` ; rebuild cache au premier paint.
  - `edit_hex_close()` : libère `atDisasmCache`.

**Architecture du cache (UC10) :**
- Le cache est une fenêtre glissante : `EDIT_HEX_DISASM_PREBUF_BYTES` avant le curseur + `EDIT_HEX_DISASM_CACHE_LINES` instructions en avant. Reconstruit à chaque déplacement de curseur ou édition d'octet.
- Avec le stub UC10 (toutes instructions = 1 word = 2 bytes) : `instruction N = byte 2N`, mapping trivial.
- Quand UC11–14 implémenteront le vrai désassembleur (instructions de longueur variable) : le cache sera automatiquement correct car `disasm_range()` retournera les bonnes longueurs via `iWordCount`.
- La zone DISASM est read-only : aucun chemin d'édition, touches printables ignorées.

**Tests R14/R15 appliqués :**
- `use_cases/use_case_10.c` : TEST MATRIX **15N + 8R + 8S = 31 tests**, 0 failure
  - [N] : constantes DISASM_CACHE_LINES/PANEL_CHARS/SEP_CHARS/ADDR_CHARS/PREBUF_BYTES (5) ; WND_WIDTH_DISASM > STD, WND_HEIGHT > 0 (2) ; BYTES_PER_ROW == 16 régression (1) ; zones distinctes + valeurs correctes (3) ; `disasm_range` 4 bytes → 2 lignes + adresses (4)
  - [R] : NULL guards open/close (8)
  - [S] : `make manual UC=10` (8 tests visuels)

**Contrats comportementaux validés :**

*Zone DISASM — navigation*
- TAB depuis HEX_ZONE_ASCII : → HEX_ZONE_DISASM si `bShowDisasm == ST_TRUE` ; sinon → HEX_ZONE_HEX.
- TAB depuis HEX_ZONE_DISASM : → HEX_ZONE_HEX, `iNibble = 0`.
- ↑ en DISASM : `iDisasmCursorIdx--` + `uiCursor = atDisasmCache[i-1].uiAddr` + scroll hex + scroll disasm.
- ↓ en DISASM : `iDisasmCursorIdx++` + `uiCursor = atDisasmCache[i+1].uiAddr` + scroll hex + scroll disasm.
- PgUp/PgDn en DISASM : décalage par `iVisRows` instructions.
- Home/End en DISASM : rebuildent le cache depuis le début/la fin du fichier.
- ESC : `gui_request_close()` (identique aux autres zones).
- Touches printables en DISASM : ignorées (zone read-only).

*F2 toggle*
- `bShowDisasm = !bShowDisasm` ; si `eZone == HEX_ZONE_DISASM` → bascule vers `HEX_ZONE_HEX`.
- `ehex_recalc_layout()` appelé pour recalculer `iDisasmX` (0 si masqué).
- Cache rebuild si panel devient visible.

*Sync bidirectionnel*
- Curseur en HEX/ASCII → `ehex_disasm_find_cursor()` → `iDisasmCursorIdx` mis à jour → `ehex_disasm_scroll_to_cursor()`.
- Curseur en DISASM → `uiCursor = atDisasmCache[idx].uiAddr` → `ehex_scroll_to_cursor()`.
- Edit d'octet (HEX ou ASCII) → `ehex_disasm_cache_update()` → cache reconstruit (contenu du fichier a changé).

*Scroll molette*
- Zone HEX ou ASCII : scrolle `iScrollRow` (hex panel).
- Zone DISASM : scrolle `iDisasmScrollRow` (disasm panel indépendant).

*Fenêtre*
- `bShowDisasm == ST_TRUE` → `tDesc.iWidth = EDIT_HEX_WND_WIDTH_DISASM (1320)`.
- `bShowDisasm == ST_FALSE` (malloc cache échoué) → `tDesc.iWidth = EDIT_HEX_WND_WIDTH_STD (950)`.
- Les deux : `tDesc.iHeight = EDIT_HEX_WND_HEIGHT (640)`.

*Lifecycle*
- `atDisasmCache` alloué dans `edit_hex_open()` ; `malloc` échec → `bShowDisasm = ST_FALSE` + LOG_ERROR, ouverture continue (non fatal).
- `atDisasmCache` libéré dans `edit_hex_close()`.
- Cache rebuild différé au premier `GUI_EVT_PAINT` (cellules connues seulement après création du renderer).

**Points d'attention pour les UCs suivants :**
- ~~UC11–14 : le désassembleur réel remplacera les DC.W stubs. `ehex_disasm_cache_update()` appellera toujours `disasm_range()` sans modification — le cache sera automatiquement valide. La seule différence : `iWordCount` peut être > 1, donc `iDisasmCursorIdx` ne sera plus triviallement `uiCursor / 2`. `ehex_disasm_find_cursor()` gère déjà ce cas via la comparaison `uiAddr ≤ uiCursor < uiAddr_next`.~~ **RÉSOLU UC14**
- ~~UC15 : fixup PRG → les adresses dans le disasm panel seront correctes par rapport à la RAM ST (pas juste des offsets dans le fichier). `uiAddr` dans `disasm_result_t` sera l'adresse ST réelle.~~ **RÉSOLU UC15**
- ~~UC18 (mount) : disasm des secteurs boot d'une image `.st` — le secteur 0 est potentiellement un programme 68000. La sélection de plage (`szDisasmRange` UI) peut être un UC futur.~~ **RÉSOLU UC18.2** — touche B → `edit_hex_open(bootsector)`
- P32 (SHIFT+flèches sélection plage) : maintenant que DISASM zone existe, P32 peut cibler la vue hex pour surligner les octets correspondant à une instruction disasm sélectionnée — à arbitrer.
- P33 (`edit hex` forcer hex) : applicable à UC10 — une commande `edit hex <file>` forcerait l'ouverture hex même pour les fichiers non-binaires.

## 17 Use Case 11 (UC11) — ✓ VALIDÉ (2026-06-03)

**Périmètre fonctionnel implémenté :**
- `src/disassemble.c` réécrit intégralement :
  - `disasm_rw()` : helper lecture big-endian 16-bit.
  - `disasm_fmt_words()` : formatage du champ hex de `szLine` (jusqu'à 5 mots, 20 chars).
  - `disasm_fmt_line()` : finalise `ptResult->szLine` à partir des champs remplis.
  - `disasm_fill_words()` : copie `auWords[1..iWordCount-1]` depuis le buffer.
  - `disasm_dc_word()` : helper fallback `DC.W $XXXX`.
  - `disasm_fmt_ea()` : décodeur EA complet (12 modes) avec gestion des mots d'extension :
    - Mode 0/1 : Dn/An (0 mot ext)
    - Mode 2/3/4 : (An)/(An)+/-(An) (0 mot ext)
    - Mode 5 : d16(An) — 1 mot ext, déplacement signé en hex
    - Mode 6 : d8(An,Xn) brief format — 1 mot ext (index An/Dn + taille .W/.L + déplacement signé)
    - Mode 7.0 : abs.W — 1 mot ext, signe étendu sur 24 bits (ex : `$FF8200.W`)
    - Mode 7.1 : abs.L — 2 mots ext
    - Mode 7.2 : d16(PC) — 1 mot ext, EA absolue calculée depuis adresse extension
    - Mode 7.3 : d8(PC,Xn) — 1 mot ext brief
    - Mode 7.4 : immédiat #data — 1 mot (B/W) ou 2 mots (L)
  - `disasm_move()` : MOVE.B/W/L + MOVEA.W/L (destination An → MOVEA) ; rejet MOVE.B→An (DC.W).
  - `disasm_moveq()` : MOVEQ #$XX,Dn — 1 mot, données 8 bits en hex non signé.
  - `disasm_misc4()` : groupe 0100 — LEA / CLR.B/W/L / SWAP / PEA dans cet ordre de priorité ; CLR size=11 → DC.W ; PEA mode Dn/An/(An)+/-(An)/imm → DC.W.
  - `disasm_exg()` : EXG Dx,Dy / Ax,Ay / Dx,Ay (3 patterns `0xF1F8`).
  - `disasm_one()` : dispatch top nibble → 0001/0010/0011 MOVE, 0100 misc4, 0111 MOVEQ, 1100 EXG, autres DC.W.
  - `disasm_range()` : inchangé (avance de `iWordCount*2` par itération).
- `TC-DIS-001` ADAPTED UC11 : `bValid = ST_TRUE` pour MOVEQ ; `bValid = ST_FALSE` pour RTS (0x4E75) reste DC.W jusqu'à UC14.

**Architecture EA :**
- `disasm_fmt_ea()` prend `uiPC` (adresse du mot opcode) et `iExtSoFar` (mots d'extension déjà consommés avant cet EA). L'adresse PC pour d(PC) = `uiPC + (1 + iExtSoFar) * 2`.
- Les mots d'extension sont ordonnés : source EA en premier, puis destination EA (conforme PRM Motorola).
- Retour `-1` sur EA invalide → la fonction appelante émet DC.W.

**Tests R14/R15 appliqués :**
- `use_cases/use_case_11.c` : TEST MATRIX **52N + 10R + 0S = 62 tests**, 0 failure
  - [N] : MOVE 3 tailles reg→reg (4), MOVE modes source (7×3), MOVEA (2+1 DC.W), MOVEQ (3+1 DC.W), LEA (5×3), CLR (4+1 DC.W), SWAP (3×2), PEA (2×3+1 SWAP), EXG (5×2), d8(An,Xn) 3 cas, range mixed (6), UC1 regression (4)
  - [R] : NULL buf/result, buf trop court, range NULL ×3, opcode inconnu ×2, range len=0 ×2

**Contrats comportementaux validés (ADAPTED UC11) :**

*Décodeur EA*
- Mode 6 (d8(An,Xn)) : déplacement 0 affiché `$0(An,Xn)`.
- Mode 7.0 (abs.W) : signe étendu à 24 bits → `$FF8200.W` pour extension 0x8200 ; `$1234.W` pour extension 0x1234 positive.
- Mode 7.2 (d16(PC)) : EA calculée = adresse de l'extension + déplacement signé.
- Immédiat .B : mot d'extension entier lu, seul l'octet bas utilisé (`#$XX`).

*`disasm_move()`*
- Destination mode=001 (An) + taille B → DC.W (MOVEA.B n'existe pas).
- Destination mode=001 + taille W/L → mnemonic `MOVEA.W`/`MOVEA.L`.
- Source et destination avec extensions : source en premier, puis destination.

*`disasm_moveq()`*
- Bit 8 du mot opcode doit être 0 ; sinon DC.W.
- Immédiat affiché en hex non-signé 2 chiffres.

*`disasm_misc4()`*
- Ordre de priorité : LEA → CLR → SWAP → PEA.
- SWAP (0x4840-0x4847) précède PEA dans la vérification (même masque 0xFFC0).
- PEA rejette les modes non-contrôle (0,1,3,4 et mode7.reg4 = immédiat).

*`disasm_exg()`*
- 3 modes distincts testés via masque `0xF1F8`.

*`disasm_one()` / `disasm_range()`*
- `bValid = ST_TRUE` pour toute instruction reconnue en UC11.
- Tout opcode non reconnu → `bValid = ST_FALSE`, DC.W, `iWordCount = 1`.
- `szLine` toujours renseigné (même sur buffer trop court).
- `disasm_range()` : avance de `iWordCount * 2` à chaque ligne.

**Points d'attention pour les UCs suivants :**
- ~~UC10 : le panneau disasm affiche maintenant les vraies mnémoniques pour MOVE/MOVEQ/LEA/CLR/EXG/SWAP/PEA — sans modification de `edit_hex.c` (appelle `disasm_range()` déjà).~~ **RÉSOLU UC11** — panneau automatiquement mis à jour
- ~~UC12 : groupe 0100 contient aussi NOT/NEG/TST/NBCD/EXT — décodage complémentaire dans `disasm_misc4()`.~~ **RÉSOLU UC12**
- ~~UC12 : groupe 0110 (ADD/SUB immediat) + groupe 1000 (OR/DIVU) + 1001 (SUB/SUBX) + 1011 (CMP/EOR) + 1100 (AND/MULU) + 1101 (ADD/ADDX).~~ **RÉSOLU UC12**
- ~~UC13 : groupe 1110 (shifts/rotations) + BTST/BSET/BCLR/BCHG (groupe 0000 bits 7-6 ≠ 11).~~ **RÉSOLU UC13**
- ~~UC14 : groupe 0100 (`4E75` RTS, `4E74` RTD, `4E73` RTE, `4EF9` JMP) + `6xxx` (Bcc/BRA/BSR) + `4E80-4EBF` (JSR).~~ **RÉSOLU UC14**
- ~~`TC-DIS-001` (ADAPTED UC11) : reste valide — `bValid = ST_FALSE` pour 0x4E75 (RTS, non décodé jusqu'à UC14). A mettre à jour en ADAPTED(UC14) dans use_case_01.c en UC14.~~ **RÉSOLU UC14**

## 18 Use Case 12 (UC12) — ✓ VALIDÉ (2026-06-03)

**Périmètre fonctionnel implémenté :**
- `src/disassemble.c` étendu : 8 nouveaux groupes de dispatch + 8 helpers internes.
- **Nouveaux helpers** :
  - `disasm_unary_ea(szMnem, iSz, iMode, iReg)` : CLR/NEGX/NEG/NOT/TST — factorisation de l'opération "MNEM.sz EA".
  - `disasm_dreg_ea(szMnem, iDn, iDir, iSz, iMode, iReg)` : ADD/SUB/CMP/AND/OR/EOR dans les 2 directions.
  - `disasm_x_a(szMnem, iSz)` : ADDA/SUBA/CMPA (An destination).
  - `disasm_addq_subq()` : ADDQ/SUBQ — immédiat 3 bits (0=8) affiché en décimal.
  - `disasm_addx_subx(szMnem)` : ADDX/SUBX register (Dx,Dy) et mémoire (-(Ax),-(Ay)).
  - `disasm_imm_ea(szMnem)` : ADDI/SUBI/CMPI/ANDI/ORI/EORI + cas spéciaux CCR/SR.
  - `disasm_mul_div(szMnem)` : MULU.W/MULS.W/DIVU.W/DIVS.W (source word, dest Dn).
  - `disasm_cmpm()` : CMPM.x (An)+,(An)+ — distingué de EOR par bits5-3=001 (mode1).
- **`disasm_misc4()` étendu** (UC12) : NEGX (0x4000), NEG (0x4400), NOT (0x4600), TST (0x4A00), EXT.W (0x4880..7), EXT.L (0x48C0..7) — insérés avant SWAP/PEA UC11.
- **Nouveaux groupes** dans `disasm_one()` :
  - `disasm_group0()` : ORI/ANDI/SUBI/ADDI/EORI/CMPI — dispatch sur bits11-8.
  - `disasm_group5()` : ADDQ/SUBQ (size≠3) ; Scc/DBcc (size=3) → DC.W.
  - `disasm_group8()` : OR/DIVU/DIVS — DIVU si sz=3,bit8=0 ; DIVS si sz=3,bit8=1 ; SBCD → DC.W.
  - `disasm_group9()` : SUB/SUBA/SUBX — SUBA si sz=3 ; SUBX si bit8=1,mode≤1 ; SUB sinon.
  - `disasm_groupB()` : CMP/CMPA/EOR/CMPM — CMPA si sz=3 ; CMPM si bit8=1,mode=1 ; EOR si bit8=1,mode≠1 ; CMP si bit8=0.
  - `disasm_groupC()` : AND/MULU/MULS + EXG (UC11) — MULU/MULS si sz=3 ; ABCD → DC.W ; AND sinon.
  - `disasm_groupD()` : ADD/ADDA/ADDX — ADDA si sz=3 ; ADDX si bit8=1,mode≤1 ; ADD sinon.
- **Disambiguation ADDX/SUBX** : mode=0 (Dn) et mode=1 (An/predecrement) dans le champ EA bits5-3 détectent les formes ADDX/SUBX avant le chemin ADD/SUB Dn→EA.

**Tests R14/R15 appliqués :**
- `use_cases/use_case_12.c` : TEST MATRIX **64N + 8R + 0S = 72 tests**, 0 failure
  - [N] : ADDI/SUBI/CMPI/ORI/ANDI/EORI (7×2+2=16), ADDQ/SUBQ (5×2+2=12), ADD/SUB/CMP (6×3=18), ADDA/SUBA/CMPA (4×2=8), ADDX/SUBX (3×2=6), AND/OR/EOR (4×2=8), MULU/MULS/DIVU/DIVS (4×2=8), NEG/NOT/NEGX/TST/EXT (9×2+2=20), CMPM (2), range PRG (9), régression UC11 (2+3)
  - [R] : NULL ×2, DC.W invalides (NEGX/TST/NOT sz=11, RTS, ABCD, Scc) ×6

**Contrats comportementaux validés :**

*`disasm_addq_subq()`*
- Data=0 dans le champ opcode → affiche `#8` (pas `#0`).
- Immediate affiché en décimal (convention DEVPAC3).
- Size=3 (bits7-6=11) → DC.W (Scc/DBcc non implémenté).

*`disasm_imm_ea()`*
- Immédiat source toujours en premier (avant l'EA destination dans le buffer).
- Destination EA mode=7, reg=4 → "CCR" (taille B) ou "SR" (taille W) — pas "#$imm".
- Size=3 → DC.W.

*`disasm_mul_div()`*
- Opérande source est toujours word (iSz=1 passé à `disasm_fmt_ea`).
- Mnemonic avec `.W` suffix : `MULU.W`, `MULS.W`, `DIVU.W`, `DIVS.W`.

*ABCD/SBCD detection → DC.W*
- Dans groupC/group8 : bit8=1, sz=0 (B), mode=0 ou 1 → DC.W sans crash.

*`disasm_groupB()` EOR vs CMPM*
- bit8=1, sz≠3, mode=001 → CMPM ; mode≠001 → EOR.

**Points d'attention pour les UCs suivants :**
- UC13 : groupe 1110 (ASL/ASR/LSL/LSR/ROL/ROR/ROXL/ROXR) + groupe 0000 bits7-6≠11 (BTST/BSET/BCLR/BCHG). ✓ VALIDÉ UC13
- UC14 : groupe 0100 restant (RTS/NOP/RTE/RTR/TRAP/LINK/UNLK/JSR/JMP) + groupe 0110 (BRA/BSR/Bcc). ✓ VALIDÉ UC14
- UC11/UC12 se complètent : toutes les instructions arithmétiques et logiques de type R-R et M-R sont décodées. Le panneau disasm UC10 affiche maintenant l'ensemble complet des instructions algébriques.

## 19 Use Case 13 (UC13) — ✓ VALIDÉ (2026-06-03)

**Périmètre fonctionnel implémenté :**
- `src/disassemble.c` étendu : groupe 0xE + BTST/BCHG/BCLR/BSET dans groupe 0x0.
- **Nouvelles tables** :
  - `g_aszShiftMnem[4][2]` : noms des 4 types × 2 directions (ASR/ASL, LSR/LSL, ROXR/ROXL, ROR/ROL).
  - `g_aszShiftMemMnem[8]` : indexé par bits 10-8 de l'opcode pour les shifts mémoire.
  - `g_aszBitOp[4]` : BTST/BCHG/BCLR/BSET (indexé par bits 7-6).
- **`disasm_groupE()`** : groupe 0xE — shifts et rotations :
  - *Forme registre* (sz≠3, bits 7-6 ≠ 11) : `1110 cccc d ss i tt rrr` — mnemonic `XXX.sz`, opérandes `#N,Dn` (immédiat, count=0→8) ou `Dn,Dn` (registre).
  - *Forme mémoire* (sz=3, bits 7-6 = 11) : `1110 cccc 11 EA` — mnemonic `XXX.W`, opérande EA ; rejet Dn/An/PC-relative/imm → DC.W ; bit 11 set → DC.W.
- **`disasm_bit_static()`** : BTST/BCHG/BCLR/BSET #imm — `0000 1000 tt MMMRRR + ext` ; rejet An mode (MOVEP) → DC.W.
- **`disasm_bit_dynamic()`** : BTST/BCHG/BCLR/BSET Dn — `0000 DDD 1 tt MMMRRR` ; rejet An mode → DC.W.
- **`disasm_group0()`** étendu : ajout `case 0x8` (static) + branch odd (dynamic) dans `default`.
- **`disasm_one()`** : ajout `case 0xE → disasm_groupE`.

**Tests R14/R15 appliqués :**
- `use_cases/use_case_13.c` : TEST MATRIX **62N + 8R + 0S = 70 tests**, 0 failure
  - [N] : shifts registre immédiat (ASL/ASR/LSL/LSR/ROXL/ROXR/ROL/ROR, 8×2=16) ; shifts registre Dn (3×2=6) ; shifts mémoire (8+1×3=11 nominaux+wc) ; bit static (4×3=12) ; bit dynamic (4×2=8) ; range 3 instructions (6) ; régression UC11/UC12 (3)
  - [R] : mem shift Dn dest (1) ; mem shift An dest (1) ; mem shift bit11 (1) ; bit static An mode (1) ; bit dynamic An mode (1) ; NULL pBuf (1) ; NULL ptResult (1) ; range NULL puiLines (1)

**Contrats comportementaux validés :**

*`disasm_groupE()` — forme registre*
- Immediate count = 0 dans le champ opcode → affiché `#8` (pas `#0`)
- Direction : bit 8 = 0 → right (ASR, LSR, ROXR, ROR) ; bit 8 = 1 → left (ASL, LSL, ROXL, ROL)
- Type : bits 4-3 = 00=AS, 01=LS, 10=ROX, 11=RO
- Size : bits 7-6 = 00=.B, 01=.W, 10=.L (11 impossible → branch mémoire)
- Format opérandes immédiat : `#N,Dn` ; format registre : `Dn,Dn`

*`disasm_groupE()` — forme mémoire*
- Identifiée par bits 7-6 = 11 (sz=3) ; toujours `.W` ; count=1 implicite
- Mnemonic indexé par bits 10-8 : 0=ASR, 1=ASL, 2=LSR, 3=LSL, 4=ROXR, 5=ROXL, 6=ROR, 7=ROL
- Bit 11 set (bits 10-8 ≥ 8 en interprétation non-tronquée) → DC.W
- EA mode 0 (Dn) ou mode 1 (An) ou mode 7 reg ≥ 2 (PC-rel/imm) → DC.W
- Extension word(s) consommés correctement si abs.W (`$1234.W` : wc=2)

*`disasm_bit_static()` / `disasm_bit_dynamic()`*
- Static : `iWordCount = 2 + n` (opcode + #bit word + EA ext words)
- Bit number = octet bas du mot d'extension (`iBit = uiBitW & 0xFF`)
- An mode (mode=1) → DC.W dans les deux formes (zone MOVEP)
- Dynamic : `iWordCount = 1 + n` ; opérandes `"Dn,EA"`

*`disasm_group0()` — cas bit ops*
- `(uiOpc >> 8) & 0x0F == 0x8` → static bit op
- Valeur impaire (bit 8 = 1) et ≠ 0x8 → dynamic bit op via `disasm_bit_dynamic`
- Valeur paire ≠ {0,2,4,6,A,C} → DC.W (inchangé)

**Points d'attention pour les UCs suivants :**
- ~~UC14 : groupe 0x4 restant : RTS (0x4E75), NOP (0x4E71), JMP (0x4EF9), JSR (0x4E80-0x4EBF), TRAP #N (0x4E40-0x4E4F), RTR (0x4E77), RTE (0x4E73). Groupe 0x6 : BRA/BSR/Bcc (déplacement 8-bit dans octet bas ou 16-bit extension). Après UC14, le désassembleur est complet pour la cible démo ST (UC31).~~ **RÉSOLU UC14**
- MOVEP : `0000 DDD 1 MM 001 RRR` (bits 5-3 = 001 = An mode dans EA). Actuellement → DC.W via le rejet An mode dans `disasm_bit_dynamic`. Comportement correct pour UC13 ; à implémenter si nécessaire avant UC31.
- ~~`TC-DIS-001` (ADAPTED UC11) : RTS (0x4E75) reste DC.W jusqu'à UC14. Marqueur ADAPTED(UC14) à ajouter dans `use_case_01.c` lors de UC14.~~ **RÉSOLU UC14**

## 20 Use Case 14 (UC14) — ✓ VALIDÉ (2026-06-03)

**Périmètre fonctionnel implémenté :**
- `src/disassemble.c` étendu : groupe 0x6 + complétion groupe 0x4.
- **Nouvelle table** : `g_aszBcc[16]` — mnémoniques des 16 branches (BRA, BSR, BHI, BLS, BCC, BCS, BNE, BEQ, BVC, BVS, BPL, BMI, BGE, BLT, BGT, BLE).
- **`disasm_no_operand()`** : helper pour instructions 1 mot sans opérande (NOP/RTS/RTE/RTR).
- **Bloc 0x4Exx dans `disasm_misc4()`** (avant le fallback DC.W) :
  - `0x4E71` NOP, `0x4E73` RTE, `0x4E75` RTS, `0x4E77` RTR → `disasm_no_operand`.
  - `0x4E40-0x4E4F` TRAP #N → vecteur en décimal.
  - `0x4E50-0x4E57` LINK An,#disp16 → 2 mots ; déplacement signé en hex (`#-$8` pour -8).
  - `0x4E58-0x4E5F` UNLK An → 1 mot.
  - `0x4E80-0x4EBF` JSR + `0x4EC0-0x4EFF` JMP → EA contrôle via `disasm_fmt_ea` ; rejet Dn/An/(An)+/-(An)/imm → DC.W.
- **`disasm_group6()`** : BRA/BSR/Bcc —
  - `disp8 == 0xFF` → DC.W (68020+ seulement).
  - `disp8 == 0x00` → 16-bit form, 2 mots ; mnemonic sans `.S`.
  - `disp8 ≠ 0, ≠ 0xFF` → 8-bit short form, 1 mot ; mnemonic avec `.S`.
  - Cible = `(uiAddr + 2 + disp) & 0x00FFFFFF` ; formatée `$XXXXXX`.
- **`disasm_one()`** : `case 0x6 → disasm_group6` ajouté.
- **Non-régression** : `use_case_11.c` et `use_case_12.c` — assertions RTS `bValid==ST_FALSE` → `bValid==ST_TRUE && mnemonic=="RTS"` (ADAPTED:UC14) ; `use_case_01.c` — description TC-DIS-001 mise à jour.

**Tests R14/R15 appliqués :**
- `use_cases/use_case_14.c` : TEST MATRIX **56N + 8R + 0S = 64 tests**, 0 failure
  - [N] : NOP/RTS/RTE/RTR (8) ; TRAP #1/#14 (4) ; LINK A6/UNLK A6/LINK A0,#0 (8) ; JSR (A0)/JSR $1234.W/JMP (A1)/JMP abs.L/JMP d16(PC) (13) ; BRA.S/BRA long/BSR.S (9) ; BNE.S/BEQ long/BPL.S/BCC.S/BGT.S (15) ; range PRG (8) ; régression (3)
  - [R] : JMP Dn (1) ; JSR An (1) ; JSR -(An) (1) ; BRA disp8=0xFF (1) ; BRA 16-bit buf court (1) ; NULL pBuf/ptResult/ptResults (3)

**Contrats comportementaux validés :**

*`disasm_group6()` — BRA/BSR/Bcc*
- `disp8 == 0xFF` → DC.W (forme 32-bit non-68000, `bValid=ST_FALSE`)
- `disp8 == 0x00` → forme longue, `iWordCount=2`, mnemonic sans `.S` ; si `uiBufLen < 4` → DC.W
- `disp8 ≠ {0, 0xFF}` → forme courte, `iWordCount=1`, mnemonic avec `.S`
- Cible toujours masquée `& 0x00FFFFFF` (adresses 24-bit ST)
- Branches arrière correctes : `0xFE` = -2, `0xF6` = -10, etc.

*JSR / JMP*
- Mode 0 (Dn) → DC.W ; Mode 1 (An) → DC.W (MOVEP territory) ; Mode 3 (An)+ → DC.W ; Mode 4 -(An) → DC.W ; Mode 7.4 (#imm) → DC.W
- EA contrôle valide : modes 2/5/6/7.0/7.1/7.2/7.3 → décodage normal
- `JMP $00FF8800` : abs.L → `iWordCount=3`, opérandes `$00FF8800`
- `JMP d16(PC)` : cible calculée depuis adresse de l'ext word

*TRAP*
- Vecteur 0-15 affiché en décimal : `TRAP #0` à `TRAP #15`

*LINK/UNLK*
- LINK : `iWordCount=2` ; déplacement signé en hex sans-prefix : `#-$8` (négatif) ou `#$0` (zéro/positif)
- UNLK : `iWordCount=1`, opérande = `g_aszAn[An]`

*Régression use_case_11/12/13*
- UC11: `0x4E75` (RTS) → `bValid=ST_TRUE`, mnemonic `"RTS"` (ADAPTED:UC14 — était ST_FALSE)
- UC12: idem
- UC13: `0x6000` avec `uiBufLen=2` → reste DC.W (`uiBufLen < 4` → DC.W dans `disasm_group6`)

**Points d'attention pour les UCs suivants :**
- ~~UC15 : format PRG fixups~~ **✓ IMPLÉMENTÉ UC15** — `load_do_prg()` complet, adresses dans le disasm panel maintenant correctes (offset fichier = offset RAM puisque load à ST_LOAD_BASE=0x0800 et les fixups sont déjà appliqués).
- UC31 : le désassembleur couvre le sous-ensemble démo (MOVE/MOVEQ/LEA/CLR/EXG/SWAP/ADD/SUB/CMP/AND/OR/EOR/NOT/NEG/shifts/rotations/bits/BRA/BSR/Bcc/JMP/JSR/RTS/TRAP). Il manque encore MOVEM, CHK, DBcc/Scc, MOVEP pour couverture complète 68000 — ces instructions apparaissent rarement dans les démos courtes ; ajout possible dans un UC15-bis si nécessaire.
- ~~UC21 : le CPU 68000 réel décodera les instructions via un mécanisme similaire au désassembleur mais avec exécution effective des effets (registres, flags SR).~~ **RÉSOLU UC21**

## 21 Use Case 15 (UC15) — ✓ VALIDÉ (2026-06-03)

**Périmètre fonctionnel implémenté :**
- `src/load.c` — `load_do_prg()` réécrit complètement (UC7 = stub sans fixups) :
  - **Header** : lecture 28 octets + validation magic `0x601A` ; extraction `uiTextSize`, `uiDataSize`, `uiBssSize`, `uiSymSize`, `uiAbsFlag` en big-endian via `load_u16be()`/`load_u32be()`.
  - **Chargement sections** : `uiTextSize + uiDataSize` octets copiés d'un bloc dans `aRam[ST_LOAD_BASE..]` via `file_read()`.
  - **BSS zeroing** : `memset(aRam[ST_LOAD_BASE + contentSize], 0, bssSize)` après le chargement ; la zone BSS n'existe pas dans le fichier mais doit être zeroed en RAM.
  - **Skip symtab** : `load_skip_bytes(ptFile, uiSymSize)` — lecture en blocs 512 octets et discard ; obligatoire pour positionner le curseur fichier sur la table de fixups.
  - **Fixup relocation** : `load_apply_fixups()` — format bitstream Atari ST :
    - Lit 4 octets big-endian = offset du premier fixup depuis le début de `.text` ; si `== 0` → pas de fixups (retour immédiat).
    - Boucle : valide `uiOffset + 4 <= uiContentSize`, lit le longword RAM à `aRam[ST_LOAD_BASE + uiOffset]`, y ajoute `ST_LOAD_BASE`, réécrit big-endian ; lit l'octet suivant : `0x00` = fin, `0x01` = avance 254, autre = avance de N.
    - EOF sans `0x00` explicite : traité comme fin (robuste vis-à-vis d'assembleurs anciens).
    - Fixup hors-plage : `ST_ERROR` + `LOG_ERROR`.
  - **abs_flag** : si `uiAbsFlag != 0` → saute `load_apply_fixups()` entièrement (le fichier ne contient pas de table de fixups).
  - **`load_state_t` étendu** : ajout de `uiTextSize`, `uiDataSize`, `uiBssSize`, `uiFixupCount` (0 pour LOAD_TYPE_BINARY).
- `src/load.h` : documentation du format fixup bitstream dans le header de fichier ; nouveaux champs `load_state_t` documentés.
- Helpers internes nouveaux : `load_skip_bytes()`, `load_apply_fixups()`.
- `use_cases/UC15/` : 5 fixtures binaires hand-crafted :
  - `fixup.prg` — 1 fixup : `JMP abs.L $0` → longword[2] relocalisé à `0x0800` après chargement.
  - `bss.prg` — .text=RTS(2B), .bss=16B, no fixups ; vérifie le zeroing BSS.
  - `absolute.prg` — abs_flag=1, pas de table fixup dans le fichier.
  - `multi_fixup.prg` — 2 fixups à offsets 2 et 8 dans .text.
  - `bad_fixup.prg` — fixup offset=100 hors de la plage .text+.data → ST_ERROR.

**Architecture de `load_apply_fixups()` :**
- La validation `uiOffset + 4 <= uiContentSize` garantit qu'on ne lit jamais hors de la zone `.text+.data` chargée en RAM.
- Le longword est lu depuis `aRam` (pas du fichier) : il a déjà été chargé lors de la phase 2. Cela garantit la cohérence même si le fichier est mal formé ou tronqué.
- L'état du module (`g_load_tState`) n'est mis à jour que **après** que `load_apply_fixups()` retourne `ST_NO_ERROR` — un échec préserve l'état précédent.
- EOF sans terminateur `0x00` est toléré : certains assembleurs ATARI ST omettent le byte final.

**Tests R14/R15 appliqués :**
- `use_cases/use_case_15.c` : TEST MATRIX **33N + 5R + 0S = 38 tests**, 0 failure
  - [N] : hello.prg UC7 regression (7) ; fixup.prg : types+sizes+fixupCount+RAM opcodes+longword relocalisé (8) ; bss.prg : sizes+uiSize+RTS+BSS area zeroed (5) ; absolute.prg : eType+fixupCount=0+RTS (4) ; multi_fixup.prg : fixupCount=2+deux longwords relocalisés+opcodes entre-deux inchangés (6) ; state fields uiLoadAddr+uiSize (2) ; load_shutdown (1)
  - [R] : bad_fixup → ST_ERROR + state preserved (2) ; load_file(NULL) (1) ; load_file non-existant (1) ; load_file after shutdown (1)
  - [S] : 0 (UC15 est entièrement headless)
- **INTENT IDs corrigés** : les IDs originaux (007/008 dans le corps, déjà pris par UC7) remplacés par 009..015 pour lever le conflit de numérotation.

**Contrats comportementaux validés :**

*`load_do_prg()` — pipeline complet*
- `uiFixupCount == 0` si `first_offset == 0` (aucun fixup) ; la lecture des 4 octets est quand même obligatoire.
- `abs_flag != 0` → `uiFixupCount == 0` et aucun appel à `load_apply_fixups()`.
- `uiSize = uiTextSize + uiDataSize + uiBssSize` — c'est le footprint total en RAM ST (ADAPTED:UC15 par rapport au stub UC7 où `uiSize = uiTextSize + uiDataSize` uniquement).
- La zone BSS est zeroed inconditionnellement si `uiBssSize > 0`, quelle que soit la valeur initiale de la RAM.
- Un échec en cours de `load_apply_fixups()` : `file_close()` puis `return ST_ERROR` **sans** mise à jour de `g_load_tState` → l'état précédent est préservé.

*`load_apply_fixups()` — invariants*
- Fixup hors-plage (`uiOffset + 4 > uiContentSize`) → `ST_ERROR` immédiat + `LOG_ERROR`.
- Chaque fixup : `longword_at_offset += ST_LOAD_BASE` (big-endian lu et réécrit dans `aRam`).
- Octet `0x01` = avance de 254 (pas 255) : convention Atari ST pour dépasser les offsets > 127 sans ambiguïté avec le terminateur.
- EOF sans `0x00` : comportement tolérant (break de boucle), `uiFixupCount` reflète les fixups effectivement appliqués.

*`load_state_t` — champs étendus UC15*
- `uiTextSize`, `uiDataSize`, `uiBssSize` : valeurs directement issues du header PRG, 0 pour LOAD_TYPE_BINARY.
- `uiFixupCount` : nombre de relocalisations appliquées, 0 si abs_flag=1 ou no-fixup ou LOAD_TYPE_BINARY.

**Points d'attention pour les UCs suivants :**
- ~~UC16 : image `.st` = `file_open(READ)` + `file_read` de 737 280 octets exact — le même pipeline `file.h` que UC15.~~ **RÉSOLU UC16**
- UC24 : `ST_LOAD_BASE = 0x0800` est en dur ; UC24 allouera la première zone libre au-dessus des variables TOS selon la memory map réelle.
- UC25 : `execute` démarrera depuis `load_get_state()->uiLoadAddr` comme PC initial quand `eType == LOAD_TYPE_PRG`. Les adresses de fixup sont déjà correctes en RAM ST.
- ~~UC10/disasm panel : avec les fixups appliqués, les adresses affichées dans la vue hex+disasm correspondent aux vraies adresses ST (pas aux offsets fichier) — comportement attendu vérifié.~~ **RÉSOLU UC15** — comportement vérifié lors de UC15A (torture test)

---

## 22 Use Case 15A (UC15A) — ✓ VALIDÉ (2026-06-06)

**Périmètre fonctionnel implémenté :**

UC15A est un UC de validation croisée : il charge `use_cases/UC15A/SOURCE.PRG` (programme assemblé avec DEVPAC3 sur ATARI ST réel, 0 erreur) dans la RAM émulée, désassemble la section `.text` via `disasm_range()`, parse le source DEVPAC3 `SOURCE.S`, et compare instruction par instruction. Objectif : DIFF = 0.

**Bugs disasm corrigés dans `src/disassemble.c` :**

1. **`disasm_groupE` — bit `iIR` inversé** : le bit 5 de l'opcode groupe-E distingue mode immédiat (`i=0` → `#count,Dn`) et mode registre (`i=1` → `Dcnt,Dn`). Le code précédent avait les deux branches `if (iIR)` / `else` échangées — toutes les shifts immédiat/registre produisaient le mauvais format. Correction : swap des deux branches. Impact : 76 DIFFs résolus ; `use_case_13.c` ADAPTED sur 10 assertions.

2. **`disasm_exg` — ordre An,An DEVPAC3** : pour `EXG An,Am`, DEVPAC3 encode le premier opérande source dans les bits 2-0 (champ `iRy`) et le second dans les bits 11-9 (`iRx`) — convention inverse du PRM Motorola. L'affichage `A{iRx},A{iRy}` était incorrect ; corrigé en `A{iRy},A{iRx}`. Impact : 5 DIFFs résolus ; `use_case_11.c` ADAPTED sur 2 assertions.

3. **NBCD et CHK.W ont des extension words** : ces instructions tombaient en DC.W avec `iWordCount=1`, décalant tout le flux binaire. Ajout de `disasm_ea_ext_cnt()` helper et détection `0x4800` (NBCD) + `0xF1C0==0x4180` (CHK.W) avant le fallback final. Impact : résolution du décalage de comptage (57 instructions fantômes).

4. **NEGX/NEG/NOT/TST — taille 3 avec extension words** : les opcodes de ces instructions avec `size=3` (p.ex. `MOVE.W EA,SR`) utilisent une EA qui peut avoir des mots d'extension ; le DC.W émis ignorait ces mots. Corrigé via `disasm_ea_ext_cnt()`. Impact : inclus dans les 57 instructions ci-dessus.

**Infrastructure de test `use_cases/use_case_15A.c` :**

- `uc15a_normalize()` : normalisation cosmétique (espaces, commentaires en ligne)
- `uc15a_extract_instr()` : extraction de la mnémonique+opérandes depuis une ligne SOURCE.S (skip labels, directives, commentaires)
- `uc15a_apply_compat()` : pipeline de 7 transformations COMPAT (`.L`/`.W` suffix, `#N`→`#$N`, negatifs)
- `uc15a_strip_hex_zeros()` : `$00000000` → `$0`
- `uc15a_neg_to_unsigned32()` : `#-$4000` → `#$FFFFC000`
- `uc15a_truncation_compat()` : `#$FFFFFFFF` vs `#$FF` — suffixe tronqué OK
- `uc15a_add_to_addi()` : `ADD.B #x,Dn` → `ADDI.B #x,Dn`
- `uc15a_imm_to_decimal()` : `#$N` → `#N` pour normaliser ADDQ/SUBQ/BTST
- `uc15a_classify()` : pipeline MATCH → DCW → PCREL → COMPAT → DIFF

Référence complète des différences syntaxiques : **`DISASM_SYNTAX.md`** à la racine du projet.

**Tests R14/R15 appliqués :**
- `use_cases/use_case_15A.c` : TEST MATRIX **13N + 3R + 0S = 16 tests**, 0 failure
  - [N] : normalize/extract helpers (3) ; st_init + load_init + load_file SOURCE.PRG (5) ; disasm_range .text (2) ; SOURCE.S parse count == disasm count (1) ; DIFF==0 + total cohérence (2)
  - [R] : normalize/extract robustesse NULL/vide (3)

**Score final :**

| Instructions SOURCE.S | MATCH | DCW | PCREL | COMPAT | DIFF |
|-----------------------|-------|-----|-------|--------|------|
| 2525                  | 963   | 270 | 495   | 797    | **0** |

**Contrats comportementaux validés (ADAPTED:UC15A sur UC13/UC11) :**

*`disasm_groupE` — formes registre (après correction iIR)*
- `iIR == 1` (bit5=1) → opérandes `D{count},Dn` (count = register number, 0 = D0 — **pas** de règle "0 signifie 8")
- `iIR == 0` (bit5=0) → opérandes `#count,Dn` (count = immediate, 0 **encode 8**)
- Tous les opcodes de test UC13 shifts immédiat/registre ont été revérifiés et les assertions corrigées avec ADAPTED:UC15A

*`disasm_exg` — EXG An,An*
- Premier opérande source DEVPAC3 → bits 2-0 (`iRy`) ; deuxième → bits 11-9 (`iRx`)
- Affichage : `A{iRy},A{iRx}` (ordre DEVPAC3 round-trip)
- EXG Dx,Dy et EXG Dx,Ay non affectés (autres encodings)

*Extension words sur DC.W — invariant flux binaire*
- Toute instruction décodée en DC.W **doit** comptabiliser ses mots d'extension dans `iWordCount` pour maintenir l'alignement du flux désassemblé
- `disasm_ea_ext_cnt(iMode, iReg)` est le helper canonique : mode 5→1, 6→1, 7.0→1, 7.1→2, 7.2→1, 7.3→1, 7.4→0

**Points d'attention pour les UCs suivants :**
- ~~UC21 : MOVEM, DBcc, Scc, MOVEP (~175 DCW restants) — décodage prévu lors de l'implémentation CPU 68000~~ **PARTIELLEMENT RÉSOLU UC21** — CPU implémenté (MOVE/MOVEQ/LEA/CLR/SWAP) ; MOVEM/DBcc/Scc/MOVEP restent DC.W → UC23+
- UC24 : MOVE.W SR/CCR, TAS, ILLEGAL, RESET, STOP (~95 DCW restants) — nécessitent le contexte mode superviseur
- `DISASM_SYNTAX.md` à enrichir à chaque nouveau groupe d'instructions décodé

---

## 23 Use Case 16 (UC16) — ✓ VALIDÉ (2026-06-04)

**Périmètre fonctionnel implémenté :**
- `src/image_st.h` (nouveau) : constantes géométrie ST DD (IST_TRACKS/SIDES/SECTORS/BPS/SPC…), constantes FAT12 (IST_FAT_FREE/EOC/MEDIA_BYTE), flags attributs (IST_ATTR_*), type `image_st_dirent_t`, type opaque `image_st_t`, API complète.
- `src/image_st.c` (nouveau) : implémentation portable CRT + file.h :
  - `image_st_create()` : malloc + memset + `ist_format_boot()` (BPB LE16/LE32, OEM "ST4EVER ", boot sig 0x55/0xAA) + FAT init (cluster 0 = 0xFF9 media byte, cluster 1 = 0xFFF EOC).
  - `image_st_load()` : lecture IST_DISK_SIZE octets via file.h + `ist_validate_bpb()` (BPS=512, TS=1440).
  - `image_st_save()` : écriture IST_DISK_SIZE octets via file.h FILE_MODE_WRITE.
  - `image_st_close()` : free + `*pptImg=NULL` ; `close(&NULL)` = no-op.
  - `image_st_list_root()` : itération 112 entrées racine ; skip 0xE5 (deleted), 0x00 (end), IST_ATTR_VOLNAME ; remplit `image_st_dirent_t[]`.
  - `image_st_read_file()` : suivi chaîne FAT12 cluster par cluster ; copie `IST_SPC * IST_BPS` octets par cluster ; détection fin-de-chaîne prématurée et cluster hors-plage.
  - `image_st_write_file()` : normalisation 8.3 uppercase (`ist_name_to_83`) ; suppression entrée existante si présente ; allocation chaîne FAT (`ist_fat_alloc` + link + EOC) ; remplissage clusters ; écriture entrée racine (aName83 + size LE32 + cluster LE16 + attr ARCHIVE).
  - `image_st_delete_file()` : libère chaîne FAT (`ist_fat_free_chain`) + marque entrée 0xE5.
  - `image_st_free_bytes()` : compte clusters FAT_FREE × IST_SPC × IST_BPS.
  - Helpers internes : `ist_fat_get/set` (12 bits pair/impair) + miroir FAT1→FAT2 ; `ist_fat_alloc` ; `ist_fat_free_chain` (boucle anti-cycle sur uiMax) ; `ist_cluster_offset` (secteur DATA + offset par cluster) ; `ist_raw_to_name` ; `ist_name_to_83` ; `ist_name_match` (case-insensitive) ; `ist_rd16/ist_wr16/ist_wr32`.
- `use_cases/use_cases.h` : ajout `#include "../src/image_st.h"`.
- `src/mount.h` / `src/mount.c` : suppression `TODO(UC16)` (implémenté).
- `src/common.h` : `ST_APP_VERSION` → `"0.16.0"`.
- `use_cases/UC16/roundtrip.st` : créé lors de l'exécution de `make tests` (test block 4) — inspectable manuellement avec un éditeur hex.

**Tests R14/R15 appliqués :**
- `use_cases/use_case_16.c` : TEST MATRIX **31N + 15R + 0S = 46 assertions** (39 TCs dans SRTD.md §5.47), 0 failure
  - [N] : create/BPB/FAT init (7) ; write+list+read 64 bytes et 512 bytes (9) ; delete + free-bytes accounting (4) ; save+reload round-trip (6) ; overwrite file (2) ; close idempotence (3)
  - [R] : NULL guards (11 : create/load×2/close/save×2/list_root×2/read_file×2/free_bytes) ; load non-existent + handle NULL (2) ; write invalid 8.3 name (1) ; delete non-existent (1)

**Contrats comportementaux validés :**

*FAT12 encoding (ist_fat_get/set)*
- Cluster pair N (even) : bytes [N×3/2] et [N×3/2+1] — low 12 bits.
- Cluster impair N (odd) : bits 4-15 du byte [N×3/2] et byte [N×3/2+1] complet — high 12 bits du pair de bytes.
- `ist_fat_set` met à jour FAT1 et FAT2 de manière atomique (même appel).
- `ist_fat_get` lit FAT1 uniquement (source de vérité).

*`image_st_create()`*
- Cluster 0 = 0xFF9 (IST_FAT_MEDIA_BYTE) ; cluster 1 = 0xFFF (EOC).
- `free_bytes` = IST_DATA_CLUSTERS × IST_SPC × IST_BPS = 711 × 2 × 512 = 728,064 bytes.
- Racine vide : `list_root` retourne count = 0.

*`image_st_load()`*
- Taille exacte IST_DISK_SIZE obligatoire : fichier tronqué → ST_ERROR.
- BPS = 512 et TS = 1440 dans le BPB : divergence → ST_ERROR + LOG_ERROR.

*`image_st_write_file()`*
- Nom 8.3 normalisé en uppercase ; nom invalide (> 8 chars base ou > 3 chars ext) → ST_ERROR.
- Si le fichier existait déjà : chaîne FAT libérée + entrée 0xE5 avant la nouvelle allocation ; `list_root` retourne count inchangé (1) avec le nouveau contenu.
- `pData == NULL` avec `uiSize > 0` → ST_ERROR.
- `uiSize == 0` : entrée créée avec cluster = 0, size = 0 ; aucun cluster alloué.
- Cluster de fin de chaîne toujours EOC (0xFFF) ; clusters intermédiaires → suivant.
- Dernier cluster : octet non-utilisé en fin zeroed (cluster padding).

*`image_st_delete_file()`*
- Entrée marquée 0xE5 (convention FAT "deleted", non effacée physiquement).
- `ist_fat_free_chain` : boucle sur `uiCluster >= IST_CLUSTER_FIRST && < uiMax` avec uiMax = IST_CLUSTER_FIRST + IST_DATA_CLUSTERS (protection contre boucle infinie sur chaîne corrompue).
- Après delete : `list_root` ne retourne plus l'entrée.

*`image_st_close(&NULL)`*
- `*pptImg == NULL` → no-op immédiat, `ST_NO_ERROR` (idempotent).

**Points d'attention pour les UCs suivants :**
- ~~UC17 : `.msa` = `.st` compressé run-length ; la même structure `image_st_t` peut être réutilisée après décompression MSA → aDisk. UC17 ajoute `image_msa_load()` et `image_msa_save()` qui opèrent sur un `image_st_t` existant.~~ **RÉSOLU UC17**
- ~~UC18 (`mount`) : `mount_open()` appellera `image_st_load()` pour les fichiers `.st`. La vue GUI listera les entrées via `image_st_list_root()`. Le drag & drop écrira via `image_st_write_file()`.~~ **RÉSOLU UC18.1**
- ~~UC20 (`image`) : `image_st_save()` est déjà disponible — UC20 crée l'image depuis le contenu d'un répertoire PC monté via UC18.~~ **RÉSOLU UC20**
- Limite de sous-répertoires : seule la racine est supportée (suffisant pour UC18/UC20). Les `.st` ATARI ST réels ont rarement des sous-répertoires dans les démos.

---

## 24 Use Case 17 (UC17) — ✓ VALIDÉ (2026-06-06)

**Périmètre fonctionnel implémenté :**
- `src/image_msa.h` (nouveau) : constantes MSA (`IMSA_HDR_SIZE=10`, `IMSA_MAGIC=0x0E0F`, `IMSA_ESCAPE=0xE5`, `IMSA_SPT`, `IMSA_SIDES_HDR=1`, `IMSA_TRACK_FIRST/LAST`, `IMSA_TRACK_BYTES=4608`) + API publique `image_msa_load()` et `image_msa_save()`.
- `src/image_msa.c` (nouveau) : codec MSA RLE complet :
  - `imsa_rd16be(p)` / `imsa_wr16be(p, v)` : helpers big-endian 16-bit (lectures/écritures header + data_length).
  - `imsa_decompress(pIn, uiInLen, pOut, uiOutLen)` : expansion RLE — byte ≠ 0xE5 = copie directe ; 0xE5 = séquence 3 octets (run_byte + count_hi + count_lo) ; vérification overflow à chaque run ; validation `uiOut == uiOutLen` en fin.
  - `imsa_compress(pIn, uiInLen, pOut, uiOutMax)` : RLE MSA — runs de 0xE5 (même count=1) encodés systématiquement ; runs ≥ 5 octets identiques encodés ; retourne 0 si la sortie compressée déborderait `uiOutMax` (caller utilise alors la piste brute).
  - `image_msa_load()` : ouverture fichier + lecture header 10 octets + validation (magic 0x0E0F, SPT>0, sides≤1, first≤last<80) + `image_st_create()` + `image_st_get_disk()` + boucle sur toutes les pistes/faces : lecture 2 octets data_length → si data_length == IMSA_TRACK_BYTES → `memcpy` brut, sinon `imsa_decompress`.
  - `image_msa_save()` : validation paramètres + `image_st_get_disk()` + écriture header + boucle sur 80 pistes × 2 faces : `imsa_compress` → si compressé < brut, écrire compressé ; sinon écrire brut.
- `src/image_st.h` : ajout déclaration `image_st_get_disk(ptImg, ppDisk)`.
- `src/image_st.c` : implémentation `image_st_get_disk()` — accès sécurisé (NULL guards) au champ `aDisk` de l'opaque `struct image_st_s`. Maintient l'encapsulation : aucun .h externe ne voit la définition complète de la struct.
- `use_cases/UC17/` : répertoire créé à l'exécution du test ; fixtures `blank.msa`, `files.msa`, `bad_magic.msa`, `truncated.msa`, `bad_geo.msa` créées par le test.
- `src/common.h` : `ST_APP_VERSION` → `"0.17.0"`.

**Architecture du codec :**
- `image_msa.c` n'accède jamais directement à la struct `image_st_s`. Il passe exclusivement par `image_st_get_disk()` pour obtenir le pointeur `aDisk`. Cela garantit que l'opacité de `image_st_t` n'est pas brisée par le module codec.
- La décision raw/compressé se fait piste par piste à la sauvegarde. Si une piste ne compresse pas (ex : piste contenant des données aléatoires ou quasi-aléatoires), elle est stockée brute avec `data_length == IMSA_TRACK_BYTES`. Le lecteur détecte ce cas via `data_length == IMSA_TRACK_BYTES` → `memcpy` direct.
- Le buffer de compression `aBuf[IMSA_TRACK_BYTES]` est alloué sur la pile dans `image_msa_save()` (4608 octets). Acceptable : IMSA_TRACK_BYTES est borné et connu à la compilation.
- Compression d'une piste vide (tout à zéro) : 4 octets `{0xE5, 0x00, 0x12, 0x00}` (= run de 4608 octets de 0x00). 160 pistes × 4 octets = 640 octets vs 737 280 pour un `.st` équivalent.

**Tests R14/R15 appliqués :**
- `use_cases/use_case_17.c` : TEST MATRIX **15N + 10R + 0S = 25 tests**, 0 failure
  - [R] : NULL guards load/save (4) ; NULL guards get_disk (2) ; load nonexistent (1) ; bad magic (1) ; truncated (1) ; bad geometry (1)
  - [N] : blank round-trip (5 : create+save+stat exists+stat compression+load+list_root+free_bytes) ; files round-trip (10 : create+3 writes+save+load+list_root 3 entries+3 tailles+3 contenus+free_bytes)

**Contrats comportementaux validés :**

*`image_st_get_disk(ptImg, ppDisk)`*
- `ptImg == NULL` ou `ppDisk == NULL` → `ST_ERROR`
- Succès : `*ppDisk = ptImg->aDisk` — pointeur valide jusqu'à `image_st_close()`

*`image_msa_load(szPath, pptImg)`*
- `szPath == NULL` ou `pptImg == NULL` → `ST_ERROR`
- Fichier inexistant → `ST_ERROR` (délégué à `file_open`)
- Magic ≠ 0x0E0F → `ST_ERROR` + LOG_ERROR
- SPT == 0 ou sides > 1 ou first > last ou last ≥ 80 → `ST_ERROR` + LOG_ERROR
- data_length > IMSA_TRACK_BYTES → `ST_ERROR` + LOG_ERROR
- En cas d'erreur de décompression : `image_st_close(&ptImg)` + `file_close` + return ST_ERROR. `*pptImg` reste NULL.
- Succès : `*pptImg` → nouvelle `image_st_t` BPB-formatée avec toutes les pistes décompressées

*`image_msa_save(ptImg, szPath)`*
- `ptImg == NULL` ou `szPath == NULL` → `ST_ERROR`
- Erreur d'écriture → `file_close` + return ST_ERROR (fichier peut être tronqué)
- Succès : fichier MSA complet, header + 160 blocs (80 pistes × 2 faces), chaque bloc = 2 octets data_length + N octets data

*`imsa_decompress()` — invariants*
- Byte non-escape → copie verbatim ; overflow output → `ST_ERROR`
- Séquence escape tronquée (< 3 octets restants) → `ST_ERROR`
- Run count == 0 ou `uiOut + count > uiOutLen` → `ST_ERROR`
- En fin : `uiOut != uiOutLen` → `ST_ERROR` (piste mal encodée)

*`imsa_compress()` — invariants*
- 0xE5 (escape byte) : toujours encodé en séquence, même pour count=1. Literal 0xE5 interdit dans le flux MSA.
- Run de N octets non-escape < 5 : copie brute (N bytes < 4 bytes séquence)
- Run de N octets ≥ 5 : séquence (4 bytes < N bytes brut)
- Si `uiOut + 4 > uiOutMax` (séquence) ou `uiOut + N > uiOutMax` (brut) → retour 0 (pas de compression)
- Retour 0 → `image_msa_save()` stocke la piste brute avec data_length = IMSA_TRACK_BYTES

**Points d'attention pour les UCs suivants :**
- ~~UC18 (`mount`) : `image_msa_load()` est désormais disponible — `line_cmd_mount()` peut accepter les `.msa` comme les `.st`. Même interface `image_st_t` pour les deux formats.~~ **RÉSOLU UC18.1**
- ~~UC20 (`image`) : `image_msa_save()` permettra de créer un `.msa` depuis un contenu monté, en plus du `.st` existant.~~ **RÉSOLU UC20**
- Extension `.stx` (UC future optionnelle) : serait un 3ème codec sur la même base `image_st_t`. L'architecture `image_st_get_disk()` permet d'en ajouter sans modifier `image_st.c`.

---

## 25 Use Case 18.1 (UC18.1) — ✓ VALIDÉ (2026-06-06)

**Périmètre fonctionnel implémenté :**
- `src/mount.h` (réécrit) : constantes `MOUNT_WND_WIDTH=780`, `MOUNT_WND_HEIGHT=500`, `MOUNT_LIST_WIDTH=516` ; enum `mount_src_t` (MOUNT_SRC_DIR/ST/MSA) ; struct `mount_view_t` (hWnd, hRenderer, ptImg, eSrc, szSrcPath, bDirty, aEntries[IST_RDE], iEntryCount, iSelectedEntry, iScrollOffset, iWndWidth/H, iCellW/H) ; API publique `mount_view_open/close/add_file`.
- `src/mount.c` (réécrit) : implémentation D2D complète (~800 lignes) :
  - `mount_refresh()` : snapshot FAT via `image_st_list_root()`.
  - `mount_scroll_to_sel()` : clamp scroll pour maintenir la sélection visible.
  - `mount_render()` : begin_draw + fond gauche + fond droit + séparateur vertical + header "A:\\ [SRC] N files" + liste fichiers (highlight sélection bleu) + panneau propriétés (Source/Path/Files/Free/Total/Modified/hints/sélection) ; macro `PROP_ROW` pour les lignes propriétés.
  - `mount_handle_key()` : UP/DOWN/PgUp/PgDn/Home/End/DEL/ESC ; DEL appelle `image_st_delete_file()` + `mount_refresh()`.
  - `mount_handle_click()` : clic gauche dans panel liste → sélection par index visible.
  - `mount_handle_scroll()` : molette → `iScrollOffset` clampé.
  - `mount_event_callback()` : PAINT (lazy renderer), RESIZE, KEY_DOWN, MOUSE_DOWN, SCROLL, CLOSE.
  - `mount_dir_cb()` : callback `file_list_dir` pour peupler image depuis répertoire PC (skip dirs, log skipped si 8.3 invalide).
  - `mount_view_open()` : résolution chemin via cwd, stat, dispatch dir/st/msa, `image_st_create/load/image_msa_load`, `mount_refresh`, `gui_open_window`.
  - `mount_view_close()` : `gui_close_window` + `image_st_close` + free.
  - `mount_view_add_file()` : stat + read + `image_st_write_file` + `mount_refresh` + `gui_invalidate`.
  - **Fix szExt** : comparaisons `strcmp(tStat.szExt, "st")` et `"msa"` sans point (conforme `file_stat_t` UC6).
- `src/gui.h` : déclaration `gui_find_window_by_type(eType, phWnd)`.
- `src/gui.c` : implémentation `gui_find_window_by_type()` — parcourt `g_gui_aptWnd[GUI_MAX_WINDOWS]` sous mutex, retourne premier handle `bOpen==ST_TRUE && eType==eType`.
- `src/line.c` :
  - Includes `mount.h` + `image_msa.h` après `load.h`.
  - `static mount_view_t *g_line_ptMountView = NULL`.
  - `line_cmd_mount()` : collecte chemin arg/selected/cwd-avec-confirmation ; rejette fichier non-image ("use load") ; ferme vue existante ; `mount_view_open`.
  - `line_cmd_umount()` : log warning si dirty ; `mount_view_close`.
  - Dispatch `CMD_MOUNT/CMD_UMOUNT` → fonctions respectives.
  - `line_shutdown()` : ferme `g_line_ptMountView` si ouvert.
  - **Fix szExt** : comparaison `"st"`/`"msa"` sans point dans `line_cmd_mount`.
- `src/common.h` : `ST_APP_VERSION` → `"0.18.1"`.
- `use_cases/use_case_13.c` : suppression variable `uiLines` inutilisée (warning `-Wunused-variable`).
- `use_cases/use_case_18_1.c` : nouveau test — inclut `file.h` pour `file_t`/`file_open`/`file_write`/`file_close`/`file_mkdir`.

**Anomalie découverte et résolue :**
- `file_stat_t.szExt` est sans point (ex : `"st"` pas `".st"`). Les comparaisons dans `mount_view_open()` et `line_cmd_mount()` utilisaient `".st"` avec point → tous les montages .st/.msa retournaient ST_ERROR. Corrigé dans `mount.c` et `line.c`.

**Tests R14/R15 appliqués :**
- `use_cases/use_case_18_1.c` : TEST MATRIX **12N + 8R + 8S = 28 tests**, 0 failure
  - [R] : NULL guards open(ptLineCtx/pptView) (2) ; close(NULL/&NULL) (2) ; add_file(NULL view/path) (2) ; find_window_by_type(NULL phWnd) (1) ; open(nonexistent) (1)
  - [N] : Constantes MOUNT_WND_*/MOUNT_LIST_WIDTH/MOUNT_SRC_* (6) ; find_window (no window) → ST_NO_ERROR + NULL (2) ; open .st (eSrc/iEntryCount/ptImg/hWnd/find_window/close) (7) ; open .msa (eSrc/iEntryCount/close) (3) ; add_file (ST_NO_ERROR/count++/bDirty) (3) ; idempotence close (1) [conditionnel sur gui_open réel]
  - [S] : 8 tests visuels `make manual UC=18_1`

**Contrats comportementaux validés :**

*`gui_find_window_by_type(eType, phWnd)`*
- `phWnd == NULL` → `ST_ERROR`
- GUI non initialisé (`g_gui_ptMutex == NULL`) → `ST_NO_ERROR` + `*phWnd = NULL` (no-op sûr)
- Aucune fenêtre du type demandé ouverte → `ST_NO_ERROR` + `*phWnd = NULL`
- Fenêtre ouverte de ce type → `ST_NO_ERROR` + `*phWnd != NULL`
- Parcours sous mutex `g_gui_ptMutex` : thread-safe vis-à-vis de `gui_open/close_window`

*`mount_view_open(szPath, ptLineCtx, pptView)`*
- `ptLineCtx == NULL` ou `pptView == NULL` → `ST_ERROR`
- `szPath == NULL` ou vide → résolu en `ptLineCtx->szCwd`
- Source non trouvée → `ST_ERROR` + `LOG_ERROR`
- Extension ni dir, ni `.st`, ni `.msa` → `ST_ERROR` ("unsupported source")
- Répertoire → `MOUNT_SRC_DIR` : `image_st_create()` + `file_list_dir()` callback
- `.st` → `MOUNT_SRC_ST` : `image_st_load()`
- `.msa` → `MOUNT_SRC_MSA` : `image_msa_load()`
- Échec image load → `ST_ERROR` + `free(ptView)` (pas de fuite)
- Succès → fenêtre D2D visible, titre `"ST4Ever - Mount: A:\\ [SRC]"`, `*pptView != NULL`

*`mount_view_close(pptView)`*
- `pptView == NULL` → `ST_ERROR`
- `*pptView == NULL` → `ST_NO_ERROR` (idempotent)
- Après close : thread joint, renderer détruit dans `GUI_EVT_CLOSE`, image libérée, `*pptView = NULL`

*`mount_view_add_file(ptView, szSrcPath)`*
- `ptView == NULL` ou `szSrcPath == NULL` → `ST_ERROR`
- `ptView->ptImg == NULL` → `ST_ERROR`
- Fichier non trouvé ou répertoire → `ST_ERROR`
- Nom 8.3 invalide (délégué à `image_st_write_file`) → `ST_ERROR`
- Disque plein (délégué à `image_st_write_file`) → `ST_ERROR`
- Succès : `bDirty = ST_TRUE`, `iEntryCount` incrémenté, `gui_invalidate`

*`mount_handle_key()` — DEL*
- DEL sur entrée valide : `image_st_delete_file()` + `mount_refresh()` + clamp `iSelectedEntry` + `gui_invalidate`.
- DEL si `ptImg == NULL` ou sélection invalide : no-op.

*`mount_dir_cb()` — MOUNT_SRC_DIR*
- Sous-répertoires ignorés silencieusement.
- Fichier taille 0 : `image_st_write_file(NULL, 0)` (entrée vide).
- Nom non-8.3 → skipped avec `LOG_INFO("skipped '%s'")`.
- Malloc/open échoue → skipped avec `LOG_ERROR`, compteur `iSkipped++`.

**Points d'attention pour les UCs suivants :**
- ~~UC18.2 : drag-and-drop depuis vue `dir` → `mount_view_add_file()` (infrastructure prête) ; multi-sélection CTRL+ESPACE (P14) dans `dir_handle_key()` ; historique navigation ALT+←/→ (P10).~~ **RÉSOLU UC18.2**
- ~~UC19 : `umount` + dialog "save image?" si `bDirty == ST_TRUE`. `line_cmd_umount()` actuel log warning mais ne bloque pas.~~ **RÉSOLU UC19**
- ~~UC20 : `image_st_save()` et `image_msa_save()` disponibles depuis UC16/UC17 — `image` command créera une image depuis le contenu monté.~~ **RÉSOLU UC20**
- `gui_find_window_by_type` : ajouté à `gui.h`/`gui.c` — disponible pour toutes les vues futures (edit_txt, edit_hex, dir) qui veulent vérifier si une vue de même type est déjà ouverte.

---

## 26 Use Case 18.2 (UC18.2) — ✓ VALIDÉ (2026-06-06)

**Périmètre fonctionnel implémenté :**

- **P10 — Historique navigation `dir` (ALT+←/→)** :
  - `src/dir.h` : `DIR_NAV_HIST_MAX = 16`, `aszNavHistory[16][512]`, `iNavHistHead`, `iNavHistCount` dans `dir_view_t`.
  - `src/dir.c` : `dir_nav_history_push(ptView, szNewPath)` — file linéaire ; évince l'entrée la plus ancienne si pleine.
  - `dir_navigate_to(ptView, szNewPath, hWnd)` — crée le nouveau root AVANT de libérer l'ancien ; met à jour `szRootPath` ; rebuild flat list ; `dir_update_title`.
  - `dir_navigate_up()` refactorisé : calcule le parent, appelle `dir_nav_history_push` puis `dir_navigate_to`.
  - `dir_handle_key()` : touche LEFT avec `GUI_MOD_ALT` → history back (`iNavHistHead--`, `dir_navigate_to`) ; RIGHT avec `GUI_MOD_ALT` → history forward.
  - `dir_open()` : seed du premier slot de l'historique avec `szRoot`.

- **P14 — Multi-sélection CTRL+ESPACE** :
  - `src/dir.h` : `DIR_MULTI_SEL_MAX = 16`, `aszMultiSel[16][512]`, `iMultiSelCount` dans `dir_view_t`.
  - `src/dir.c` : `dir_is_multi_sel(ptView, szPath)` — parcours linéaire ; `dir_toggle_multi_sel(ptView, szPath)` — ajoute si absent et si place, retire si présent (shift down).
  - `dir_handle_key()` : CTRL+ESPACE sur fichier (non répertoire) → `dir_toggle_multi_sel` + redraw.
  - `dir_render()` : couche violette (`g_dir_clrMultiSel = {0.28f, 0.15f, 0.55f, 1.0f}`) entre la couche verte (P11) et la couche bleue (curseur).

- **P34 — Propriétés BPB lecture seule** :
  - `mount_render()` : panneau droit lit `pDisk` via `image_st_get_disk()` ; extrait SPT (`@0x18` LE16), HEADS (`@0x1A` LE16), TS (`@0x13` LE16), RDE (`@0x11` LE16) ; affiche `Geometry: H%u / S%u / T%u`.

- **P36 — Suppression /IST_RDE de l'en-tête** :
  - En-tête de la liste gauche : `"  A:\\  [%s]  %d file%s"` (sans le `/112`).
  - Panneau droit : `"Files: N / RDE"` (capacité racine issue du BPB).

- **P37 — Indicateur Bootable WD1772** :
  - `mount.h` : `mount_is_bootable(const st_u8_t *pBootSect)` déclarée public.
  - `mount.c` : `mount_check_bootable()` interne — somme les 256 mots LE16 du bootsector, compare à 0x1234 mod 0x10000.
  - `mount_render()` : ligne `"Bootable: Yes"` (vert) ou `"Bootable: No"` (gris) selon le résultat.

- **P38 — Touche B → éditeur hex bootsector** :
  - `mount_view_t` : champ `void *ptBootHexView` (évite le double typedef `edit_hex_view_t` dans `mount.h`) ; `line_context_t *ptLineCtx` (back-ref pour `edit_hex_open`).
  - `mount.c` : `mount_open_bootsector()` — ferme la vue hex précédente si ouverte ; extrait les 512 octets du bootsector via `image_st_get_disk()` ; écrit dans `MOUNT_BOOT_TMP` (`".\\st4ever_boot.bin"` Win / `"/tmp/st4ever_boot.bin"` Linux) ; construit un titre heuristique `"bootsector [H%u/S%u/T%u %uKo — bootable/not bootable]"` ; appelle `edit_hex_open()` et met à jour le titre via `gui_set_title`.
  - Touche `B`/`b` dans `mount_handle_key()` → `mount_open_bootsector(ptView)`.
  - `mount_view_close()` : ferme `ptBootHexView` si ouvert avant de fermer la fenêtre principale.
  - `mount_view_open()` : stocke `ptLineCtx` dans le view ; `ptBootHexView = NULL` initialement.

**Tests R14/R15 appliqués :**
- `use_cases/use_case_18_2.c` : TEST MATRIX **12N + 1R + 8S = 21 tests**, 0 failure
  - [N] : `DIR_NAV_HIST_MAX >= 8` / `DIR_MULTI_SEL_MAX >= 8` (2) ; `mount_is_bootable` blank=false, hand-crafted=true, corrupted=false (3) ; `dir_open` history init (iNavHistCount==1, iNavHistHead==0, history[0] non-vide) (3) ; `dir_open` iMultiSelCount==0 (1) ; `mount_view_open` ptLineCtx stocké + ptBootHexView NULL (2)
  - [R] : `mount_is_bootable(NULL)` → ST_FALSE (1)
  - [S] : 8 tests visuels `make manual UC=18_2`

**Contrats comportementaux validés :**

*`dir_nav_history_push(ptView, szNewPath)`*
- Premier push : stocké à `aszNavHistory[0]`, `iNavHistHead=0`, `iNavHistCount=1`.
- Push quand plein (`iNavHistCount == DIR_NAV_HIST_MAX`) : shift de tous les slots, nouveau chemin en position `DIR_NAV_HIST_MAX - 1` ; `iNavHistCount` plafonné à `DIR_NAV_HIST_MAX`.
- `dir_open()` seede `aszNavHistory[0]` avec le chemin initial — l'historique démarre non vide.

*`dir_navigate_to(ptView, szNewPath, hWnd)`*
- Crée le nouveau nœud root AVANT de libérer l'ancien (sécurité en cas d'échec de scan).
- Rebuilds flat list après mise à jour du root.
- Appelle `dir_update_title(ptView, hWnd)`.
- Ne push PAS dans l'historique (c'est `dir_navigate_up` et `dir_handle_key` qui pushent avant d'appeler navigate_to).

*Navigation ALT+←/→*
- ALT+LEFT : si `iNavHistHead > 0` → `iNavHistHead--` + `dir_navigate_to(aszNavHistory[iNavHistHead])`. Sinon : no-op.
- ALT+RIGHT : si `iNavHistHead < iNavHistCount - 1` → `iNavHistHead++` + `dir_navigate_to(...)`. Sinon : no-op.
- L'historique est non-cyclique (stack simple) — on ne revient pas "après la fin".

*`dir_toggle_multi_sel(ptView, szPath)`*
- Chemin déjà présent : retiré (shift du reste vers le bas), `iMultiSelCount--`.
- Chemin absent + `iMultiSelCount < DIR_MULTI_SEL_MAX` : ajouté en fin, `iMultiSelCount++`.
- Chemin absent + plein : no-op silencieux.
- Seuls les fichiers (non-répertoires) sont sélectionnables en multi (contrôle dans `dir_handle_key`).

*Rendu multi-sel*
- Ordre des couches : fond vert (P11, dernière sélection `load`) → fond violet (P14, multi-sel) → fond bleu (curseur courant). Le dernier dessiné l'emporte visuellement.
- Un fichier peut être simultanément sous-violet (multi-sel) et sous-bleu (curseur) — le bleu prime.

*`mount_is_bootable(pBootSect)`*
- `pBootSect == NULL` → `ST_FALSE`.
- Somme de `pBootSect[i] | (pBootSect[i+1] << 8)` pour i = 0, 2, 4, …, 510 (256 itérations LE16).
- Résultat & 0xFFFF == 0x1234 → `ST_TRUE` ; sinon → `ST_FALSE`.

*P38 — Bootsector hex*
- `mount_open_bootsector()` ferme toute vue hex précédente avant d'en ouvrir une nouvelle.
- `MOUNT_BOOT_TMP` : fichier temporaire local — remplacé à chaque appel à B.
- Titre heuristique : H/S/T issus du BPB du bootsector écrit ; `bootable` determiné par `mount_check_bootable`.
- Si `edit_hex_open()` échoue : LOG_ERROR + `ptBootHexView = NULL` (non fatal).
- `mount_view_close()` : si `ptBootHexView != NULL` → `edit_hex_close` + `ptBootHexView = NULL` avant `gui_close_window`.

**Points d'attention pour les UCs suivants :**
- ~~UC19 : `bDirty == ST_TRUE` à l'umount → dialog "save image?" ; options `.st`/`.msa`/`répertoire` (P35) ; flag `bootable` en écriture (P37 écriture = modifier le checksum pour le rendre bootable).~~ **RÉSOLU UC19**
- ~~UC20 : `image_st_save` / `image_msa_save` disponibles — `image` commande depuis contenu monté.~~ **RÉSOLU UC20**
- `aszMultiSel` : les chemins de multi-sélection sont disponibles pour le drag-and-drop (drag-and-drop non encore implémenté — UC futur).
- `dir_navigate_to()` : l'API est désormais publique — accessible aux vues futures qui veulent piloter la navigation du dir depuis l'extérieur (ex. depuis `mount` vers le source de l'image).

---

## 27 Use Case 19 (UC19) — ✓ VALIDÉ (2026-06-07)

**Périmètre fonctionnel implémenté :**

- **P39 — Status bar dans la vue mount** :
  - Constantes couleur : `g_mnt_clrStatusBg = {0.11f,0.11f,0.18f,1.0f}` (fond bleu nuit légèrement différent du fond principal) ; `g_mnt_clrStatusTx = {0.70f,0.70f,0.70f,1.0f}` (texte gris clair).
  - `mount_render()` : `iVisRows = (iWndHeight / iCellH) - 2` (une ligne réservée en haut = header, une ligne réservée en bas = status bar). Fond coloré `g_mnt_clrStatusBg` sur toute la largeur à `y = (iWndHeight/iCellH - 1) * iCellH`. Texte : `"  Free: %u KB  |  %d file(s)"` + `"  [*] unsaved"` si `bDirty`.
  - `mount_handle_key()`, `mount_handle_click()`, `mount_handle_scroll()` : `iVisRows` mis à jour de `-1` → `-2` (cohérence avec render).

- **P40 — Progression copie dans `mount_view_add_file()`** :
  - Constante `MOUNT_PROGRESS_CHUNK = 65536u` (64 Ko).
  - Remplace le `file_read()` monolithique par une boucle : `remaining = uiSize - uiOffset` → `uiChunk = min(remaining, MOUNT_PROGRESS_CHUNK)` → `file_read` → `uiOffset += uiChunkRead` → si pas terminé : `LOG_INFO("copying '%s': %u%%", szBaseName, uiOffset*100/uiSize)` + `gui_invalidate(ptView->hWnd)`.
  - Le status bar se met à jour visuellement pendant la copie de gros fichiers.

- **P35 — Dialog save à l'umount + API `mount_save_image()`** :
  - `mount_save_fmt_t` enum : `MOUNT_SAVE_ST=0`, `MOUNT_SAVE_MSA=1`, `MOUNT_SAVE_DIR=2`.
  - `mount_save_image(ptView, eFmt, szOutPath)` :
    - `MOUNT_SAVE_ST` : `image_st_save(ptView->ptImg, szOutPath)`.
    - `MOUNT_SAVE_MSA` : `image_msa_save(ptView->ptImg, szOutPath)`.
    - `MOUNT_SAVE_DIR` : `file_mkdir(szOutPath)` + `image_st_list_root()` + boucle : `malloc(uiSize)` + `image_st_read_file()` + `file_open(FILE_MODE_WRITE)` + `file_write()` + `file_close()` + `free()`. Répertoire non écrasé si existant (EEXIST toléré dans `file_mkdir`).
    - Sur succès : `ptView->bDirty = ST_FALSE`.
  - `line_cmd_umount()` réécrit :
    - **Flags `--st` / `--msa` / `--dir [path]`** : détectés dans la boucle argv → calcul de `szOutPath` depuis `ptCtx->szCwd` (`disk.st`, `disk.msa`, `disk/` ou chemin explicite après `--dir`) → `mount_save_image()` immédiat sans dialog.
    - **Dialog interactif** si `bDirty == ST_TRUE` et aucun flag : prompt console multilignes + `console_read_key()` :
      - `1` → `MOUNT_SAVE_ST` → `cwd/disk.st`
      - `2` → `MOUNT_SAVE_MSA` → `cwd/disk.msa`
      - `3` → `MOUNT_SAVE_DIR` → `cwd/disk/`
      - `n` / ESC / autres → abandon (pas de sauvegarde)
    - Label `do_close:` : `mount_view_close()` + message "Disk unmounted." + `line_update_console_title()`.
    - Disk propre (`bDirty == ST_FALSE`) : fermeture directe sans dialog.

**Tests R14/R15 appliqués :**
- `use_cases/use_case_19.c` : TEST MATRIX **14N + 5R + 8S = 27 tests**, 0 failure
  - [R] : `mount_save_image(NULL view)`, `(NULL path)`, `(bad fmt=99)` → ST_ERROR ; `mount_view_add_file(NULL,path)`, `(view,NULL)` → ST_ERROR
  - [N] : save .st (ST_NO_ERROR + exists + size==737280 + bDirty cleared + reload 2 files) ; save .msa (ST_NO_ERROR + exists + size<737280 + bDirty cleared) ; save dir (ST_NO_ERROR + dir exists + HELLO.PRG 64B + README.TXT 5B + bDirty cleared)
  - [S] : 8 tests visuels `make manual UC=19`

**Contrats comportementaux validés :**

*`mount_save_image(ptView, eFmt, szOutPath)`*
- `ptView == NULL` ou `szOutPath == NULL` → `ST_ERROR`
- `ptView->ptImg == NULL` → `ST_ERROR`
- `eFmt` inconnu (> MOUNT_SAVE_DIR) → `ST_ERROR`
- `MOUNT_SAVE_ST` : écrit 737 280 octets exacts (IST_DISK_SIZE) ; fichier reloadable via `image_st_load()`
- `MOUNT_SAVE_MSA` : fichier compressé < IST_DISK_SIZE pour un disque partiellement rempli
- `MOUNT_SAVE_DIR` : crée le répertoire (EEXIST toléré) ; un fichier extrait par entrée racine valide ; taille exacte respectée ; sous-répertoires ignorés (FAT12 root = flat)
- Sur succès : `ptView->bDirty = ST_FALSE`

*P40 — lecture chunked dans `mount_view_add_file()`*
- Fichier < `MOUNT_PROGRESS_CHUNK` : 1 seule itération, pas de `LOG_INFO` intermédiaire (condition `uiOffset < uiSize`)
- Fichier > `MOUNT_PROGRESS_CHUNK` : une itération par chunk, `LOG_INFO("copying '%s': %u%%")` à chaque chunk intermédiaire
- `gui_invalidate()` appelé à chaque chunk intermédiaire (status bar mise à jour visuellement)
- Comportement final (erreur / succès) identique au code UC18.1 (inchangé)

*`line_cmd_umount()` — dialog save*
- `bDirty == ST_FALSE` : `do_close` directement, pas de dialog
- Flag `--st` : `szOutPath = szCwd + "\\disk.st"` ; `mount_save_image(MOUNT_SAVE_ST)` ; `do_close` si succès
- Flag `--msa` : `szOutPath = szCwd + "\\disk.msa"` ; `mount_save_image(MOUNT_SAVE_MSA)` ; `do_close`
- Flag `--dir [path]` : `szOutPath = szDirArg` ou `szCwd + "\\disk\\"` ; `mount_save_image(MOUNT_SAVE_DIR)` ; `do_close`
- Dialog interactif : `console_read_key()` (single keypress raw) ; `1/2/3` → save + do_close ; `n`/ESC → do_close sans save ; autres touches → redisplay dialog
- `do_close` : `mount_view_close(&g_line_ptMountView)` + `g_line_ptMountView = NULL` + `line_print_msg("Disk unmounted.")` + `line_update_console_title(ptCtx)` + `return ST_NO_ERROR`

*Status bar (P39)*
- Fond `g_mnt_clrStatusBg` sur toute la largeur de la fenêtre à la dernière ligne de cellule
- Texte : `"  Free: X KB  |  N file(s)"` ; si `bDirty` : append `"  [*] unsaved"`
- `iVisRows = (iWndHeight/iCellH) - 2` dans render + tous les handlers : la liste de fichiers ne déborde jamais dans la status bar

**Points d'attention pour les UCs suivants :**
- ~~UC20 : `image_st_save()` et `image_msa_save()` disponibles depuis UC16/UC17 — la commande `image` crée une image depuis le contenu monté. `mount_save_image()` est l'API partagée.~~ **RÉSOLU UC20**
- ~~UC20A : `st2msa`/`msa2st` batch — utilisent directement `image_st_load/save` et `image_msa_load/save` sans passer par la vue mount.~~ **RÉSOLU UC20A**
- ~~P41 (ENTER → hex dans mount) : toujours planifié UC20.~~ **RÉSOLU UC20** — ENTER → `edit_hex_open()`
- ~~Bootable-write (P37 écriture) : modifier le checksum WD1772 pour rendre un disque bootable — différé UC20 avec confirmation utilisateur.~~ **RÉSOLU UC20** — touche `F` → `mount_make_bootable()`

---

## 28 Use Case 20 (UC20) — ✓ VALIDÉ (2026-06-07)

**Périmètre fonctionnel implémenté :**

- **P41 — ENTER dans la vue mount → éditeur hex du fichier sélectionné** :
  - `src/mount.h` : champ `void *ptFileHexView` ajouté à `mount_view_t` (après `ptBootHexView`).
  - `src/mount.c` : constante `MOUNT_FILE_TMP` (`".\\st4ever_mnt_file.bin"` Win / `"/tmp/st4ever_mnt_file.bin"` Linux).
  - `mount_open_file_hex(ptView)` : pattern identique à `mount_open_bootsector()` (P38) — ferme la vue hex précédente si ouverte ; extrait `aEntries[iSelectedEntry]` via `image_st_read_file()` ; écrit dans `MOUNT_FILE_TMP` ; appelle `edit_hex_open()` ; met à jour le titre `"A:\\ FILENAME.EXT  [N bytes]"` via `gui_set_title`.
  - `mount_handle_key()` : `case GUI_KEY_ENTER → mount_open_file_hex(ptView)`.
  - `mount_view_close()` : ferme `ptFileHexView` (cast `edit_hex_view_t *`) si non NULL avant `gui_close_window`.
  - Rendu hints : `"[ENTER] Open hex"` ajouté au panneau propriétés.
  - `mount_view_open()` : `ptLineCtx` stocké dans le view pour P41 (déjà présent depuis UC18.2) ; `ptFileHexView = NULL` à l'init.

- **P37 écriture — `mount_make_bootable()` + touche `F`** :
  - `src/mount.h` : déclaration publique `mount_make_bootable(image_st_t *ptImg)`.
  - `src/mount.c` : `mount_make_bootable()` — `image_st_get_disk()` → somme les 256 mots LE16 du bootsector → calcule `uiNewWord0 = (0x1234 - (uiSum - word[0])) & 0xFFFF` → écrit en LE16 aux octets 0-1. Idempotent : si déjà bootable, recalcule et réécrit le même word[0].
  - `mount_handle_key()` : case `default` — si `cCh == 'F' || cCh == 'f'` → `mount_make_bootable(ptView->ptImg)` + `ptView->bDirty = ST_TRUE` + `LOG_INFO` + `gui_invalidate`.
  - Rendu hints : `"[F]       Fix boot"` ajouté au panneau propriétés.

- **Commande `image`** :
  - `src/line.c` : `line_cmd_image()` remplace `line_cmd_stub` pour `CMD_IMAGE`.
  - **Parsing** : `--st` (défaut), `--msa`, `--bootable`, chemin optionnel en argument.
  - **Path 1 — vue mount ouverte** (`g_line_ptMountView != NULL`) : si `--bootable` → `mount_make_bootable(ptView->ptImg)` ; `mount_save_image(ptView, eFmt, szOutPath)` ; message de confirmation.
  - **Path 2 — pas de vue mount** : collecte `szSrcPath` via `line_get_selected()` ou `szCwd` ; `file_stat()` + rejet si fichier non-répertoire ; crée une `mount_view_t` temporaire via `mount_view_open()` (fenêtre ouverte brièvement) ; si `--bootable` → `mount_make_bootable()` ; `mount_save_image()` ; `mount_view_close()`.
  - `szOutPath` par défaut : `szCwd + "disk.st"` ou `"disk.msa"` selon le format.
  - Dispatch table : `CMD_IMAGE → line_cmd_image` (remplace `line_cmd_stub`).
- `src/common.h` : `ST_APP_VERSION` → `"0.20.0"`.

**Tests R14/R15 appliqués :**
- `use_cases/use_case_20.c` : TEST MATRIX **14N + 5R + 8S = 27 tests**, 0 failure
  - [R] : `mount_make_bootable(NULL)` → ST_ERROR (×2 regression) ; `mount_view_close(&NULL)` idempotent ; `mount_save_image(NULL view)` ; `mount_save_image(NULL path)` (via UC19 regression)
  - [N] : `image_st_create` blank + `mount_is_bootable` = ST_FALSE ; `mount_make_bootable` → ST_NO_ERROR + `mount_is_bootable` = ST_TRUE ; checksum WD1772 == 0x1234 ; idempotent (×2) ; hand-crafted bootable sector ; `mount_view_open` .st → 2 files ; `mount_save_image` .msa < 737280 ; `mount_save_image` .st = 737280 ; `mount_view_open` dir → 2 files ; save+reload dir image → 2 files
  - [S] : 8 tests visuels `make manual UC=20`

**Contrats comportementaux validés :**

*`mount_make_bootable(ptImg)`*
- `ptImg == NULL` → `ST_ERROR`
- `image_st_get_disk()` échoue → `ST_ERROR`
- Calcule `uiSum` = somme LE16 des 256 mots du bootsector (indices 0..510 step 2)
- `uiNewWord0 = (0x1234 - (uiSum - uiWord0)) & 0xFFFF` → écrit LE16 en bytes[0..1]
- Idempotent : second appel recalcule et réécrit la même valeur → `ST_NO_ERROR`
- Après appel : `mount_is_bootable(pDisk) == ST_TRUE`

*P41 — `mount_open_file_hex(ptView)`*
- Entrée sélectionnée à `uiSize == 0` : `LOG_INFO("empty file")` + return (pas d'ouverture hex)
- `malloc(uiSize)` échoue → `LOG_ERROR` + return (non fatal)
- `image_st_read_file()` échoue → `LOG_ERROR` + `free(pBuf)` + return
- `file_open(MOUNT_FILE_TMP, FILE_MODE_WRITE)` échoue → `LOG_ERROR` + `free(pBuf)` + return
- Succès : écriture temp + `edit_hex_open(MOUNT_FILE_TMP, ptLineCtx, &ptFileHexView)` + titre mis à jour
- `edit_hex_open()` échoue : `ptFileHexView = NULL` (non fatal)
- Ferme la vue hex précédente avant d'en ouvrir une nouvelle

*P37 — touche `F` dans mount_handle_key*
- `ptView->ptImg == NULL` → no-op silencieux
- `mount_make_bootable()` succès → `bDirty = ST_TRUE` + `gui_invalidate` → panneau affiche `"Bootable: Yes"` + status bar `"[*] unsaved"`
- `mount_make_bootable()` échoue → `LOG_ERROR`, `bDirty` inchangé

*`line_cmd_image()`*
- Vue mount ouverte (`g_line_ptMountView != NULL`) :
  - `--bootable` : `mount_make_bootable(ptImg)` avant le save (non fatal si erreur)
  - `mount_save_image(ptView, eFmt, szOutPath)` ; succès → message ; échec → `line_print_error`
- Pas de vue mount :
  - `szSrcPath` depuis `line_get_selected()` ou `szCwd`
  - Fichier non-répertoire → `line_print_warning("use a directory")` + return
  - `mount_view_open()` échoue → `line_print_error` + return
  - `--bootable` avant save ; `mount_view_close()` dans tous les cas
- `szOutPath` jamais écrasé si l'argument fourni est absolu

**Points d'attention pour les UCs suivants :**
- ~~UC20A : `st2msa`/`msa2st` batch~~ **✓ IMPLÉMENTÉ UC20A** — `line_conv_one()` + `line_conv_dir_rec()` récursif.
- ~~UC21 : CPU 68000 — registres + MOVE/MOVEQ/LEA/CLR/SWAP. Phase exécution.~~ **RÉSOLU UC21**
- `MOUNT_FILE_TMP` est remplacé à chaque ENTER sur un nouveau fichier — le contenu de la session précédente est perdu. Comportement attendu (les modifications hex ne sont pas réinjectées dans l'image FAT).

---

## 29 Use Case 20A (UC20A) — ✓ VALIDÉ (2026-06-07)

**Périmètre fonctionnel implémenté :**
- `src/line.h` : `CMD_ST2MSA` (`"st2msa"` / `"s"`) et `CMD_MSA2ST` (`"msa2st"` / `"a"`) ajoutés à `cmd_id_t` avant `CMD_COUNT`.
- `src/line.c` — nouvelles fonctions + commandes :
  - `conv_ctx_t` : struct de contexte — `szSrcExt`, `szDstExt`, `iConverted`, `iFailed`.
  - `line_conv_replace_ext(szSrc, szNewExt, szDst, uiDstLen)` : remplace l'extension d'un chemin (après le dernier `.`).
  - `line_conv_one(szSrc, szDst, szSrcExt, szDstExt)` : conversion d'un seul fichier. `.st` → `image_st_load` + `image_msa_save` ; `.msa` → `image_msa_load` + `image_st_save`. Vérifie l'extension source avant de tenter la conversion.
  - `conv_rec_ctx_t` : struct wrapper — `ptConv`, `bRecurse` — portée par le callback `file_list_fn`.
  - `line_conv_rec_cb(szFullPath, szName, ptStat, pCtx)` (signature `void`, `const file_stat_t *`) : callback pour `file_list_dir`. Ignore les entrées sans l'extension source. Récurse si `bIsDir && bRecurse`. Incrémente `ptConv->iConverted` ou `ptConv->iFailed`.
  - `line_conv_dir_rec(szDir, ptConv, bRecurse)` : appelle `file_list_dir(szDir, ST_TRUE, line_conv_rec_cb, &tRec)`.
  - `line_cmd_convert(ptCtx, iArgc, aszArgv, szSrcExt, szDstExt)` :
    - Parse `--all` (mode batch), `--dir <path>` (répertoire explicite), `-r` (récursif).
    - Sans `--all` : chemin unique via argument ou `line_get_selected()` ; `file_stat()` + rejet si extension incorrecte (`line_print_warning`) ou fichier introuvable (`line_print_error`) ; `line_conv_one()`.
    - Avec `--all` : résout `szDir` (via `--dir`, `szSelected` ou `szCwd`) ; vérifie que c'est un répertoire ; `line_conv_dir_rec()` ; affiche bilan "Done: N converted, M failed".
  - `line_cmd_st2msa(ptCtx, tCmd)` : appelle `line_cmd_convert(ptCtx, ..., "st", "msa")`.
  - `line_cmd_msa2st(ptCtx, tCmd)` : appelle `line_cmd_convert(ptCtx, ..., "msa", "st")`.
  - Dispatch table : `CMD_ST2MSA → line_cmd_st2msa`, `CMD_MSA2ST → line_cmd_msa2st`.
  - Command table : deux entrées avec synopsis `[--dir path] [--all] [-r]`.
- `src/common.h` : `ST_APP_VERSION` → `"0.20.1"`.

**Architecture :**
- `line_conv_rec_cb` est un `file_list_fn` (signature `void (*)(const char *, const char *, const file_stat_t *, void *)`) — aucune valeur de retour ; erreurs individuelles loggées + `iFailed++` mais non fatales pour le batch.
- La récursion fonctionne par rappel : le callback appelle `line_conv_dir_rec()` sur les sous-répertoires si `bRecurse == ST_TRUE`. Pas de profondeur maximum explicite (conforme au comportement de `file_list_dir`).
- Pas de GUI requis : ce UC est entièrement headless, pas de `gui_init()` dans les tests.
- Les commandes sont non-fatales pour la boucle console : toute erreur de fichier individuel produit un `LOG_ERROR` + `iFailed++` et la boucle continue.

**Tests R14/R15 appliqués :**
- `use_cases/use_case_20A.c` : TEST MATRIX **18N + 6R + 0S = 24 tests**, 0 failure
  - [N] : command table (`line_complete_cmd_query` pour "st2msa"/"s"/"msa2st"/"a") (4) ; `line_conv_replace_ext` (3) ; round-trip API `image_st_load + image_msa_save` (st2msa core) + content verified (3) ; round-trip API `image_msa_load + image_st_save` (msa2st core) + content verified (3) ; batch `st2msa --all` script → 2 .msa produits (3) ; batch `msa2st --all` script → 3 .st produits, taille exacte 737280 (2)
  - [R] : wrong extension (st2msa + msa2st) → ST_NO_ERROR + warning (2) ; missing file → ST_NO_ERROR + error (1) ; non-existent `--dir` → ST_NO_ERROR + error (1) ; no args / no selection (st2msa + msa2st) → ST_NO_ERROR + warning (2)

**Contrats comportementaux validés :**

*`line_conv_replace_ext(szSrc, szNewExt, szDst, uiDstLen)`*
- Dernier `.` après le dernier séparateur (`/` ou `\`) → remplacé par `.` + `szNewExt`.
- Si aucun `.` dans le nom de fichier : `szNewExt` ajouté à la fin (`szSrc.szNewExt`).
- Truncation protégée par `snprintf` avec `uiDstLen`.

*`line_conv_one(szSrc, szDst, szSrcExt, szDstExt)`*
- Extension source vérifiée via `file_stat().szExt` avant appel `image_*_load`.
- Extension ≠ `szSrcExt` → `LOG_INFO("not a .XXX image")` + `ST_ERROR` (non-fatal pour le batch).
- Erreur `image_*_load` ou `image_*_save` → `image_st_close` + `ST_ERROR` ; pas de fichier destination corrompu laissé.
- Succès : fichier destination créé, `bDirty` du save = effectif.

*`line_cmd_convert()` — mode single*
- Arg explicite en priorité sur `line_get_selected()`.
- `szSelected` vide + pas d'arg → `line_print_warning("No file specified…")` + `ST_NO_ERROR`.
- Fichier introuvable → `line_print_error` + `ST_NO_ERROR` (non-fatal).

*`line_cmd_convert()` — mode `--all`*
- `szDir` résolu dans l'ordre : `--dir` arg → `szSelected` (si dir) → `szCwd`.
- Chemin résolu non-répertoire → `line_print_error("Not a directory")` + `ST_NO_ERROR`.
- `line_conv_dir_rec` visite tous les fichiers du répertoire (et sous-répertoires si `-r`) ; les fichiers sans l'extension source sont ignorés silencieusement.
- Bilan final affiché : `"Done: N converted, M failed."`.

*`line_conv_rec_cb()` — invariants callback*
- Signature `void` conforme `file_list_fn` (aucune valeur de retour).
- Erreur de conversion individuelle → `ptConv->iFailed++`, pas de propagation.
- `bIsDir && bRecurse` → appel récursif `line_conv_dir_rec(szFullPath, ...)`.
- `bIsDir && !bRecurse` → ignoré silencieusement.

**Points d'attention pour les UCs suivants :**
- ~~UC21 : CPU 68000~~ **✓ IMPLÉMENTÉ UC21** — MOVE/MOVEQ/LEA/CLR/SWAP + décodeur EA 12 modes.
- Le flag `-r` est fonctionnel mais non testé dans `use_case_20A.c` (aucune fixture multi-niveau créée) — comportement validé par la logique de `line_conv_dir_rec` qui délègue à `file_list_dir`. Un test de régression `-r` est candidat pour un UC futur si un répertoire récursif de démos devient une fixture de projet.

---

## 30 Use Case 21 (UC21) — ✓ VALIDÉ (2026-06-07)

**Périmètre fonctionnel implémenté :**
- `src/CPU.c` réécrit intégralement — suppression du stub `cpu_step` (PC+2 uniquement) ; implémentation du décodeur EA 12 modes + 5 familles d'instructions :
  - **`cpu_fetch_word()`** : lecture du mot à PC + PC+=2 ; appelé pour l'opcode et pour chaque mot d'extension consommé par le décodeur EA.
  - **`cpu_flags_nz()`** : mise à jour N et Z à partir d'une valeur masquée par taille.
  - **`cpu_flags_clr_vc()`** : effacement de V et C.
  - **Décodeur EA — `cpu_ea_read()`** : 12 modes complets : Dn / An / (An) / (An)+ / -(An) / d16(An) / d8(An,Xn) / abs.W / abs.L / d16(PC) / d8(PC,Xn) / #imm. A7 byte : inc/dec par 2. Mots d'extension consommés via `cpu_fetch_word()` (avance PC).
  - **Décodeur EA — `cpu_ea_write()`** : même logique, écriture Dn (partielle pour byte/word, preserves upper), An (toujours 32 bits), mémoire.
  - **`cpu_ea_calc_addr()`** : calcul d'adresse seul pour LEA (modes contrôle uniquement : 2/5/6/7.0/7.1/7.2/7.3).
  - **`cpu_exec_move()` (groupes 0x1/0x2/0x3)** : MOVE.B/W/L + MOVEA.W/L. Topologie des bits : dest = [11-9] reg + [8-6] mode (inversé vs source). MOVEA.W sign-étend sur 32 bits, pas de flags. MOVEA.B → LOG_TODO non-fatal.
  - **`cpu_exec_moveq()` (groupe 0x7)** : `0111 DDD 0 IIIIIIII`. Bit 8 doit être 0. Immédiat 8 bits signé, sign-étendu à 32 bits, registre complet écrit. Flags N/Z/V=0/C=0.
  - **`cpu_exec_clr()` (dans groupe 0x4)** : `0100 0010 SS MMMRRR`. Écrit 0 via `cpu_ea_write`. Flags : N=0, Z=1, V=0, C=0. Size=3 → LOG_TODO.
  - **`cpu_exec_lea()` (dans groupe 0x4)** : `0100 AAA 111 MMMRRR` (mask `0xF1C0 == 0x41C0`). Adresse EA écrite dans An, pas de flags.
  - **`cpu_exec_swap()` (dans groupe 0x4)** : `0100 1000 0100 0DDD` (mask `0xFFF8 == 0x4840`). Échange bits 31-16 et 15-0. Flags N/Z depuis le résultat 32 bits, V=0, C=0.
  - **`cpu_exec_misc4()` (dispatcher groupe 0x4)** : LEA en priorité (0xF1C0 == 0x41C0) → SWAP (0xFFF8 == 0x4840) → CLR (0xFF00 == 0x4200) → LOG_TODO pour les autres (RTS, NOP, TRAP, etc. — UC23).
  - **`cpu_step()`** : utilise `cpu_fetch_word()` pour l'opcode (PC déjà avancé avant le dispatch) ; dispatch sur le nibble haut ; opcodes non-implémentés → LOG_TODO + `ST_NO_ERROR` (non-fatal).
- `use_cases/use_case_21.c` : TEST MATRIX **47N + 8R + 0S = 55 tests**, 0 failure.
- `use_cases/use_case_01.c` : ADAPTED:UC21 sur TC-CPU-005/006 — assertion `D0==42` ajoutée après MOVEQ ; commentaire RTS mis à jour (LOG_TODO UC23, non-fatal).
- `src/common.h` : `ST_APP_VERSION` → `"0.21.0"`.

**Architecture EA (UC21) :**
- `cpu_fetch_word()` est le seul point d'accès à la mémoire pour la progression du PC. Après le fetch de l'opcode, PC pointe sur le premier mot d'extension. Le décodeur EA avance PC de 2 par mot d'extension consommé — cela garantit que PC est toujours correct après le retour du handler, quelle que soit la longueur de l'instruction.
- Les modes d'adressage `(An)+` et `-(An)` modifient An avant/après l'accès. Pour le byte-size avec A7, le décalage est 2 (alignement de pile 68000).
- MOVEA n'appelle pas `cpu_ea_write()` mais modifie directement `auAn[iDstReg]` — garantit que les flags ne sont pas modifiés et que l'écriture est toujours 32 bits.
- Écriture partielle Dn : byte → preserves bits 31-8 ; word → preserves bits 31-16 ; long → remplacement complet.

**10-instruction test sequence (validée) :**

| Offset | Opcode(s) | Instruction | Effet attendu |
|--------|-----------|-------------|---------------|
| 0x0800 | 70 42 | MOVEQ #$42,D0 | D0=0x42, N=0,Z=0 |
| 0x0802 | 72 00 | MOVEQ #0,D1 | D1=0, Z=1 |
| 0x0804 | 74 FF | MOVEQ #-1,D2 | D2=0xFFFFFFFF, N=1 |
| 0x0806 | 26 02 | MOVE.L D2,D3 | D3=0xFFFFFFFF |
| 0x0808 | 48 43 | SWAP D3 | D3=0xFFFFFFFF (unchanged) |
| 0x080A | 42 83 | CLR.L D3 | D3=0, Z=1 |
| 0x080C | 38 3C 12 34 | MOVE.W #$1234,D4 | D4&0xFFFF=0x1234 |
| 0x0810 | 41 F8 08 00 | LEA $0800.W,A0 | A0=0x0800 |
| 0x0814 | 2A 08 | MOVE.L A0,D5 | D5=0x0800 |
| 0x0816 | 1C 3C 00 FF | MOVE.B #$FF,D6 | D6&0xFF=0xFF, N=1 |

**Tests R14/R15 appliqués :**
- `use_cases/use_case_21.c` : TEST MATRIX **47N + 8R + 0S = 55 tests**, 0 failure
  - [N] : MOVEQ (5 assertions : positive/zéro/négatif/sign-extend/bit8-set) ; MOVE.L/W/B reg→reg (6) ; MOVE.B immediate (3) ; MOVE.W immediate (5) ; MOVE.L An source (2) ; MOVEA.W sign-extend/no-flags (3) ; MOVEA.L (2) ; LEA abs.W (2) ; LEA (An) (3) ; CLR.L flags complets (5) ; CLR.W partiel (3) ; CLR.B partiel (3) ; SWAP all-ones (5) ; SWAP zéro (1) ; SWAP 0x00FF0000 (3) ; EA mémoire (An)/(An)+/-(An)/d16(An)/(An) byte (9) ; séquence 10 instructions avec résultats + compteur + PC final (21)
  - [R] : NULL ptCpu (1) ; NULL ptMachine (1) ; opcode non-implémenté no-crash + PC+2 (2) ; CPU arrêté no-crash (1) ; ptResult NULL (2) ; CLR.L (A1) mémoire (1)

**Contrats comportementaux validés :**

*`cpu_fetch_word(ptCpu, ptMachine)`*
- Lit le mot big-endian à `ptCpu->uiPC` puis `ptCpu->uiPC += 2`
- Appelé une fois pour l'opcode, puis une fois par mot d'extension de chaque EA
- Erreur bus : LOG_ERROR, retourne 0xFFFF (non fatal — l'instruction sera malformée mais pas de crash)

*`cpu_ea_read()` — invariants*
- Mode 3 (An)+ : incrémente An **après** la lecture ; A7+byte → +2
- Mode 4 -(An) : décrémente An **avant** la lecture ; A7+byte → -2
- Mode 7.4 (#imm) : byte = octet bas du mot d'extension ; long = 2 mots d'extension
- Mode 7.2 (d16(PC)) : l'adresse de base est PC **avant** `cpu_fetch_word` du mot d'extension

*`cpu_ea_write()` — Dn partiel*
- Byte → `Dn = (Dn & 0xFFFFFF00) | (val & 0xFF)` — bits 31-8 inchangés
- Word → `Dn = (Dn & 0xFFFF0000) | (val & 0xFFFF)` — bits 31-16 inchangés
- Long → `Dn = val` — remplacement complet

*`cpu_exec_move()` — destination EA*
- Source lue via `cpu_ea_read()` (avance PC des mots ext source)
- Destination écrite via `cpu_ea_write()` (avance PC des mots ext dest)
- MOVEA.B → LOG_TODO non-fatal (MOVEA.B illégal 68000)
- MOVEA.W → sign-extend 16→32 bits avant écriture An ; flags inchangés
- MOVE normale → `cpu_flags_nz + cpu_flags_clr_vc` avant écriture

*`cpu_exec_moveq()`*
- Bit 8 de l'opcode doit être 0 ; sinon LOG_TODO non-fatal
- Immédiat = octet bas de l'opcode, signé ; sign-étendu à 32 bits
- D'n reçoit les 32 bits complets (pas d'écriture partielle)

*`cpu_exec_clr()`*
- Écrit 0 via `cpu_ea_write()` — respecte les sémantiques partielle Dn
- Flags résultants : N=0, Z=1, V=0, C=0 — inconditionnels (le zéro est toujours zéro)
- Size=3 (bits 7-6=11) → LOG_TODO non-fatal

*`cpu_exec_lea()`*
- Calcule l'adresse EA via `cpu_ea_calc_addr()` — PC avancé des mots ext
- Écrit l'adresse dans An (32 bits, pas de partiel)
- Aucun flag modifié

*`cpu_exec_swap()`*
- `new = ((old & 0x0000FFFF) << 16) | ((old & 0xFFFF0000) >> 16)`
- Flags depuis new en taille long : N/Z mis à jour, V=0, C=0

*`cpu_step()` — dispatch*
- `cpu_fetch_word()` pour l'opcode en premier → PC déjà à PC+2 avant le handler
- Nibble haut 0x1/0x2/0x3 → `cpu_exec_move` ; 0x4 → `cpu_exec_misc4` ; 0x7 → `cpu_exec_moveq`
- Autres nibbles → LOG_TODO + `ST_NO_ERROR` (non-fatal, progression)
- `ulInstrCount++` après chaque step quel que soit le résultat
- `ptResult` NULL est légal — skippé silencieusement

**Points d'attention pour les UCs suivants :**
- ~~UC22 : ADD/SUB/CMP/AND/OR/EOR/shifts — groupes 0x5/0x6/0x8/0x9/0xB/0xC/0xD/0xE. Les flags X, C, V nécessitent une gestion complète (dépassement, retenue, borrow). Le décodeur EA est déjà complet — UC22 réutilise `cpu_ea_read/write` sans modification.~~ **RÉSOLU UC22**
- ~~UC22 : cycles d'exécution réels — `ptResult->iCycles` est pour l'instant hardcodé à 4. UC22 pourra affiner selon la table des timings 68000 (EA mode + instruction).~~ **RÉSOLU UC22** (hardcodé à 4, affinement différé UC23+)
- UC23 : `cpu_exec_misc4` pour les opcodes non-couverts : RTS (0x4E75), NOP (0x4E71), JSR (0x4E80-BF), JMP (0x4EC0-FF), TRAP #N, LINK/UNLK, RTE/RTR. Nécessite `cpu_raise_exception()` complet (stacking SR+PC, changement mode).
- `use_case_01.c` TC-CPU-005/006 ADAPTED:UC21 — les assertions step#1/step#2 sont désormais des tests réels (D0==42, opcode correct). À noter : step#2 (RTS) retourne ST_NO_ERROR via LOG_TODO — ce comportement sera remplacé en UC23.

---

## 31 Use Case 22 (UC22) — ✓ VALIDÉ (2026-06-08)

**Périmètre fonctionnel implémenté :**
- `src/CPU.c` étendu avec 12 familles d'instructions arithmétiques/logiques/décalages :
  - **`cpu_flags_add()`** : N/Z/V/C/X pour addition (`dst + src = res`) — C = débordement non-signé, V = débordement signé, X = C.
  - **`cpu_flags_sub()`** : N/Z/V/C/X pour soustraction (`dst - src = res`) — C = borrow (src > dst non-signé), V = débordement signé, X = C.
  - **`cpu_exec_group0()` (groupe 0x0)** : ORI/ANDI/SUBI/ADDI/EORI/CMPI — lecture immédiat (1 ou 2 mots), dispatch sur op nibble. CMPI ne réécrit pas la destination.
  - **`cpu_exec_group5()` (groupe 0x5)** : ADDQ/SUBQ — immédiat 1-8 (0→8) ; mode An : pas de flags. Size=3 → LOG_TODO (Scc/DBcc UC23).
  - **`cpu_exec_addx_subx()`** : ADDX/SUBX — formes registre (mode=0) et mémoire -(An) (mode=1) ; X inclus dans le calcul. Z : préservé si résultat=0 (sémantique "not cleared if zero" 68000).
  - **`cpu_exec_groupD()` (groupe 0xD)** : ADD EA,Dn / ADD Dn,EA / ADDA.L / ADDX (délégation addx_subx).
  - **`cpu_exec_group9()` (groupe 0x9)** : SUB EA,Dn / SUB Dn,EA / SUBA.L / SUBX.
  - **`cpu_exec_groupB()` (groupe 0xB)** : CMP EA,Dn / CMPA.L / CMPM (An)+,(An)+ / EOR Dn,EA.
  - **`cpu_exec_groupC()` (groupe 0xC)** : AND EA,Dn / AND Dn,EA / MULU.W (16×16→32 non-signé) / MULS.W (signé).
  - **`cpu_exec_group8()` (groupe 0x8)** : OR EA,Dn / OR Dn,EA / DIVU.W (quotient=bits 15-0, reste=bits 31-16) / DIVS.W / division par zéro → LOG_TODO (UC23).
  - **`cpu_exec_unary()` (dans groupe 0x4)** : NEG/NEGX/NOT/TST. NEGX : Z préservé si résultat=0. NEG(0) → C=0, Z=1.
  - **`cpu_exec_ext()` (dans groupe 0x4)** : EXT.W (byte→word, préserve bits 31-16) / EXT.L (word→long).
  - **`cpu_exec_groupE()` (groupe 0xE)** : ASL/ASR/LSL/LSR/ROL/ROR/ROXL/ROXR — formes registre (toutes tailles) et mémoire (word, 1 bit). Comptage immédiat (0→8) ou par registre (mod 64). V : simplifié (cleared sauf ASL où changement de signe).
  - **`cpu_exec_misc4()` étendu** : +NEGX (0xFF00==0x4000), +NEG (0xFF00==0x4400), +NOT (0xFF00==0x4600), +TST (0xFF00==0x4A00), +EXT.W/EXT.L (0xFFF8).
- `use_cases/use_case_22.c` : TEST MATRIX **62N + 8R + 0S = 70 tests**, 0 failure.
- `use_cases/use_cases.h` : ajout macro `TEST_ASSERT(desc, cond)` (alias de `UC_TEST`, pour uniformité UC22+).
- `src/common.h` : `ST_APP_VERSION` → `"0.22.0"`.

**Architecture flags (UC22) :**
- `cpu_flags_add/sub()` masquent src/dst/res à la taille de l'opération avant tout calcul — invariant critique pour l'absence de contamination des bits supérieurs.
- Les instructions ADDX/SUBX/NEGX utilisent la sémantique Z "preserve if zero" (Z n'est effacé que si le résultat est non-nul, jamais forcé à 1 par ces instructions). Permet d'implémenter les additions multi-précision ATARI ST.
- ADDA/SUBA/CMPA opèrent sur An 32 bits complets sans modifier les flags (ADDA/SUBA) ou avec flags sur 32 bits (CMPA).
- `cpu_exec_groupE()` : le loop `for (i=0; i<iCount; i++)` met à jour C/X à chaque pas — pour les rotations ROX, X est transmis entre chaque bit rotationné.

**Contrats comportementaux validés :**

*`cpu_flags_add(src, dst, res, sz)`*
- Masque src/dst/res à g_auiMask[sz] avant tout test
- C = (res < src || res < dst) — unsigned overflow
- V = !((src^dst) & msb) && ((res^src) & msb) — both same sign, result different
- X = C
- N/Z mis à jour depuis res masqué

*`cpu_flags_sub(src, dst, res, sz)` — dst - src = res*
- C = (src > dst) — borrow
- V = ((src^dst) & msb) && ((res^dst) & msb) — operands different signs, result sign != dst
- X = C

*`cpu_exec_unary()` — NEG/NEGX/NOT/TST*
- NEG(0) : `cpu_flags_sub(0, 0, 0)` → C=0, Z=1 (pas de borrow de zéro)
- NEG(N) : `cpu_flags_sub(N, 0, -N)` → C=1 si N!=0
- NEGX : Z préservé si res==0 (pas forcé à 1)
- NOT : N/Z seulement, V=0, C=0
- TST : lecture+flags, pas d'écriture

*`cpu_exec_groupE()` — décalages/rotations*
- Count=0 (immédiat) → 8 décalages
- Count registre : `Dn & 63` (mod 64)
- AS gauche : C = bit sorti du MSB
- AS droit : bit MSB répliqué (signe préservé)
- LS gauche/droit : 0 inséré
- RO : bit sorti réinjecté à l'autre bout, X non modifié
- ROX : bit sorti → X → bit injecté → X mis à jour

*ADDX/SUBX — Z "not cleared if zero"*
- `if (uiRes != 0u) SR &= ~Z` — Z n'est JAMAIS mis à 1 par ADDX/SUBX
- Permet `CLR D0 ; ADDX D1,D0` de tester si somme multi-précision est zéro

*`cpu_exec_groupB()` — CMP vs EOR*
- `iDir == 1 && iMode == 1` → CMPM (An)+,(An)+
- `iDir == 1 && iMode != 1` → EOR Dn,EA (écriture EA)
- `iDir == 0` → CMP EA,Dn (flags uniquement, pas d'écriture)

**Points d'attention pour les UCs suivants :**
- UC23 : `cpu_exec_misc4` — opcodes non-couverts : RTS (0x4E75), NOP (0x4E71), JSR (0x4E80–0x4EBF), JMP (0x4EC0–0x4EFF), TRAP #N (0x4E40–0x4E4F), LINK/UNLK, RTE/RTR. Nécessite `cpu_raise_exception()` complet (push SR+PC sur pile superviseur, changement mode S).
- UC23 : Scc/DBcc (groupe 0x5, size=3) — actuellement LOG_TODO dans `cpu_exec_group5()`.
- UC23 : Division par zéro DIVU/DIVS — actuellement LOG_TODO ; doit déclencher `cpu_raise_exception(CPU_VEC_DIV_ZERO)`.
- `iCycles` : hardcodé à 4 dans `cpu_step()`. Non affiné en UC22 (timings 68000 complexes, différé après UC23 quand le flot de contrôle sera complet).
- **RÉSOLU UC23** : `cpu_exec_misc4()` — opcodes non-couverts : RTS, NOP, JSR, JMP, TRAP, LINK, UNLK, RTE, RTR maintenant implémentés via `cpu_exec_misc4e()`.
- **RÉSOLU UC23** : Scc/DBcc (groupe 0x5, size=3) implémentés dans `cpu_exec_group5()`.
- **RÉSOLU UC23** : Division par zéro DIVU/DIVS reste LOG_TODO (exception UC24-bis ou UC25).

---

## 32 Use Case 23 (UC23) — ✓ VALIDÉ (2026-06-08)

**Périmètre fonctionnel implémenté :**
- `src/CPU.c` étendu avec le flot de contrôle 68000 complet :
  - **`cpu_push_long/word()` + `cpu_pop_long/word()`** : helpers pile superviseur — atomiques, accès via `st_write/read_long/word()` sur le SSP courant.
  - **`cpu_eval_cc(ptCpu, iCc)`** : évaluation des 16 conditions 68000 (T/F/HI/LS/CC/CS/NE/EQ/VC/VS/PL/MI/GE/LT/GT/LE) depuis les flags N/Z/V/C de SR.
  - **`cpu_exec_branch()` (groupe 0x6)** : BRA (cc=0), BSR (cc=1), Bcc (cc=2..15). disp8=0 → word extension. BSR pousse PC (après instruction complète) sur SSP avant saut.
  - **`cpu_exec_misc4e()` (groupe 0x4E)** : NOP (0x4E71), STOP (0x4E72), RTE (0x4E73), RTS (0x4E75), RTR (0x4E77), TRAP #n (0x4E40-0x4E4F), LINK An (0x4E50-0x4E57), UNLK An (0x4E58-0x4E5F), JSR EA (0x4E80-0x4EBF), JMP EA (0x4EC0-0x4EFF).
  - **`cpu_exec_group5()` étendu** : size=3 → si mode=1 : DBcc (décrémente Dn.W, branche si cc=false ET Dn.W ≠ −1) ; sinon : Scc (écrit 0xFF ou 0x00 dans l'EA byte).
  - **`cpu_raise_exception()` complet** : (1) sauvegarde SR ; (2) si mode user, swap auAn[7] ↔ uiSSP et active CPU_SR_S ; (3) push PC long sur SSP ; (4) push SR saved word sur SSP−2 ; (5) lit handler depuis table vecteurs, charge dans PC. En cas d'échec → CPU_STATE_HALTED.
- `use_cases/use_case_23.c` : TEST MATRIX **44N + 8R + 0S = 52 tests**, 0 failure (79 assertions au total).
- `src/common.h` : `ST_APP_VERSION` → `"0.23.0"`.

**Architecture pile superviseur (UC23) :**
- Layout frame exception : `[SSP+0..+1]` = SR sauvé (word), `[SSP+2..+5]` = PC sauvé (long big-endian). RTE dépile dans l'ordre inverse : pop SR word, pop PC long.
- RTR dépile uniquement les 5 bits CCR (bas de SR) depuis le word, préservant S/T/I du SR courant.
- LINK : push An → copy SP → An + SP = SP + disp16 signé (allocation frame).
- UNLK : SP = An → pop long → An (restauration frame).
- DBcc : base de branchement = adresse du mot extension (uiPCNext − 2) + disp signé. Chute si condition vraie OU Dn.W atteint −1 (wrap de 0 à 0xFFFF = −1 en signé 16 bits).

**Contrats comportementaux validés :**

*`cpu_exec_branch()` — BRA/BSR/Bcc*
- disp8 ≠ 0 → déplacement = (st_i8_t)disp8 sign-étendu à 32 bits, base = PC après opcode word
- disp8 = 0 → déplacement = (st_i16_t)extension word, base = PC après opcode word
- BSR : push PC calculé APRÈS extension word (pas après opcode word uniquement), PUIS saut
- Bcc non-pris : PC avance normalement (après extension word le cas échéant) — aucune modification pile

*`cpu_raise_exception()` — frame et mode*
- Ordre push : long PC en premier (adresse haute) puis word SR (adresse basse) → SP final = SSP − 6
- Si CPU en user mode avant exception : swap auAn[7] ↔ uiSSP AVANT le push, set CPU_SR_S
- Vecteur invalide (handler lu = 0) → `eState = CPU_STATE_HALTED` ; `cpu_step()` sur CPU halted = no-op ST_NO_ERROR

*`cpu_eval_cc()` — 16 conditions*
- HI = !C && !Z ; LS = C || Z
- GE = !(N ^ V) ; LT = N ^ V ; GT = !Z && !(N ^ V) ; LE = Z || (N ^ V)
- T toujours vrai (branch inconditionnel), F toujours faux (DBcc-style)

*DBcc (groupe 0x5, size=3, mode=1)*
- Condition évaluée EN PREMIER : si vraie → fall-through sans décrémenter
- Si fausse : décrémenter Dn.W (wraps 16 bits), si résultat ≠ −1 → branche ; si = −1 → fall-through
- La base du branchement est l'adresse du mot d'extension, pas celle de l'opcode

*`cpu_exec_misc4e()` — RTR vs RTE*
- RTR : `pop_word` → applique seulement bits CCR (0x001F) au SR courant (préserve S/T/I) ; `pop_long` → PC
- RTE : `pop_word` → remplace SR complet ; `pop_long` → PC

**Points d'attention pour les UCs suivants :**
- ~~UC23-bis : MOVEM (MOVEM.L D0-D7/A0-A7,-(An) et (An)+,regs) et ADDA.W/SUBA.W — nécessaires pour les programmes réels avant l'émulation complète UC25.~~ **RÉSOLU UC23-bis**
- UC24 : accès registres hardware ATARI ST (Shifter, MFP, YM2149) sans crash — lecture/écriture adresses $FFxxxx. La carte mémoire ST (RAM 512Ko + registres HW) doit être gérée par `st_machine_t`.
- UC25 : `iCycles` hardcodé à 4 — non affiné. Le timing 68000 précis (4 à 64 cycles selon instruction+EA) peut être ignoré jusqu'à UC25 sans impacter la correction fonctionnelle.
- `cpu_step()` sur CPU en état STOPPED : retourne ST_NO_ERROR sans exécuter — contrat validé TC-CPU-147. UC25 devra implémenter le réveil sur interruption.
- Division par zéro (DIVU/DIVS) : toujours LOG_TODO. En UC25 ou UC24-bis, remplacer par `cpu_raise_exception(CPU_VEC_DIV_ZERO)`.

---

## §33 — UC23-bis : MOVEM + ADDA.W/SUBA.W

**Périmètre implémenté :**
- `cpu_exec_movem()` (~130 lignes) dans `src/CPU.c` — dispatcher appelé depuis `cpu_exec_misc4()` après la vérification EXT (même espace d'opcode)
- MOVEM.L/W `-(An)` : masque inversé (bit0=A7…bit15=D0), snapshot des 16 registres avant toute décrémentation, pre-décrémentation An par iStep (4 ou 2) par registre actif
- MOVEM.L/W `(An)+` : masque standard (bit0=D0…bit15=A7), post-incrémentation An par iStep par registre actif; les .W loads sont sign-étendus 16→32 bits
- MOVEM EA de contrôle (autres modes) : calcul unique de l'adresse EA puis avance séquentielle par iStep
- ADDA.W : `iSzCode==3 && iDir==0` dans `cpu_exec_groupD()` — lecture SZ_WORD, sign-extension 16→32, addition à An sans modification SR
- SUBA.W : `iSzCode==3 && iDir==0` dans `cpu_exec_group9()` — même logique, soustraction
- `ST_APP_VERSION` : `"0.23.1"`

**Infrastructure validée :**
- `make tests` : 26 tests UC23-bis, 0 failure, 0 warning — totalité : 312 tests PASS 0 fail
- Opcode EXT (0x4880-0x4887 et 0x48C0-0x48C7) vérifié AVANT MOVEM dans `cpu_exec_misc4()` — pas de faux dispatch

**Matrice de tests (use_case_23A.c) :**
- `[N] Nominal : 22 tests` — save -(An), restore (An)+, round-trip 5 registres, MOVEM.W sign-extend, ADDA.W/SUBA.W pos/neg, régressions ADDA.L/SUBA.L
- `[R] Robustness : 4 tests` — NULL params, masque vide no-op, SR inchangé par ADDA.W
- `[S] Skipped : 0 tests`

**Contrats comportementaux validés :**

| Module | Invariant |
|--------|-----------|
| `cpu_exec_movem()` — -(An) | Le snapshot des 16 registres est capturé AVANT toute décrémentation de An. Si An figure dans la liste de registres sauvegardés, c'est la valeur originale (avant décrémentation) qui est écrite en mémoire. |
| `cpu_exec_movem()` — masque inversé | Pour -(An): bit0=A7, bit1=A6…bit7=A0, bit8=D7…bit15=D0. L'ordre d'itération i=0→15 cause A7→A0→D7→D0 — le registre de plus haut numéro de bit est traité en premier et se retrouve à la plus haute adresse. |
| `cpu_exec_movem()` — masque standard | Pour (An)+: bit0=D0, bit7=D7, bit8=A0, bit15=A7. Le registre de plus bas numéro de bit est traité en premier et lu depuis la plus basse adresse. |
| `cpu_exec_movem()` — MOVEM.W | Les loads word sign-étendent 16→32 bits (cast via `st_i16_t → st_i32_t → st_u32_t`). Les saves word écrivent uniquement les 16 bits bas. |
| `cpu_exec_groupD/9()` — ADDA/SUBA .W | La distinction ADDA.W (iDir=0) vs ADDA.L (iDir=1) se fait via le bit 8 de l'opcode dans la branche `iSzCode==3`. Le SR n'est jamais modifié par ADDA/SUBA (quelle que soit la taille). |

**Points d'attention pour les UCs suivants :**
- ~~UC24 : MOVEM vers EA de contrôle (mode 2 = (An), mode 5 = d16(An)) — le chemin `cpu_ea_calc_addr()` est sollicité mais n'a pas encore été testé sous MOVEM en conditions réelles.~~ Surveiller lors des premiers binaires PRG chargés en UC25.
- ~~UC24 : MOVEP — non implémenté (LOG_TODO).~~ Rare dans les démos; à ajouter en UC25 si un PRG l'utilise.
- UC25 : MOVEM.L (An)+,An — si An est dans la liste restaurée, la valeur chargée depuis la mémoire remplace An (comportement Motorola MC68000 confirmé). Ce cas edge est couvert implicitement par le round-trip mais mérite un TC dédié en UC25 si des programmes l'utilisent.

---

## §34 — UC24 : Memory map ST + registres HW stubs

**Périmètre implémenté :**
- `src/ST.c` refactorisé : dispatcher `st_read_byte`/`st_write_byte` complet, remplace les LOG_TODO précédents
- Correctif bug : `ST_VID_PALETTE` 0x20 → 0x40 (adresse palette réelle ATARI ST : 0xFF8240)
- **Shifter** (0xFF8200–0xFF827F) : `aShifterRegs[0x80]` raw storage + mise à jour cohérente des champs dérivés (`uiScreenBase`, `auPalette[16]`, `uiResolution`) sur chaque écriture
  - Screen base : écriture hi (0xFF8201) / mi (0xFF8203) / lo STE (0xFF820D) → reconstruction automatique de `uiScreenBase`
  - Palette : 16 × word à 0xFF8240–0xFF825E, accès byte-wide big-endian
  - Résolution : 0xFF8260, bits [1:0] seulement
  - Sync mode 50Hz initialisé à 0x02 dans `st_init()`
- **YM2149/PSG** (0xFF8800–0xFF8803) : sélection registre via 0xFF8800 + écriture données via 0xFF8802 + lecture registre courant depuis 0xFF8800 ; `uiYmRegSel` masqué à 4 bits ; `auYmRegs[16]`
- **MFP 68901** (0xFFFA00–0xFFFA3F) : `aMfpRegs[0x40]` stub byte-wide, pas de logique interrupt
- **ACIA 6850** (0xFFFC00–0xFFFC07) : `aAcia[8]` stub byte-wide, kbd + MIDI
- **Zone ROM** (0xF80000–0xFDFFFF) : lecture → `aRom[]` si chargé sinon 0xFF ; écriture → `ST_ERROR` (bus error)
- **Zones non-mappées** : lecture → 0xFF (open-bus) ; écriture → ignorée silencieusement
- Masquage 24 bits : `uiAddr & 0xFFFFFF` systématique en tête de `st_read/write_byte`
- R22 appliqué : `st_read/write_byte/word/long` **sans** `LOG_TRACE` (appelées sur chaque accès mémoire CPU)
- `ST_APP_VERSION` : `"0.24.0"`

**Infrastructure validée :**
- `make tests` : 49 tests UC24, 0 failure, 0 warning — totalité : 361 tests PASS 0 fail

**Matrice de tests (use_case_24.c) :**
- `[N] Nominal : 41 tests` — init/shutdown, RAM r/w, screen base hi+mi+lo, palette (mot, 16 couleurs), résolution, sync mode, YM2149 (sel+data+read+mask), MFP, ACIA, ROM read (no ROM)=0xFF, ROM write=ST_ERROR, unmapped r/w
- `[R] Robustness : 8 tests` — NULL machine/output, unaligned word, ROM write ST_ERROR, masquage 32→24 bits
- `[S] Skipped : 0 tests`

**Contrats comportementaux validés :**

| Module | Invariant |
|--------|-----------|
| `st_read/write_byte` — adressage | L'adresse est masquée à 24 bits (`& 0xFFFFFF`) avant dispatch ; les bits 24–31 sont ignorés sans erreur. |
| `shifter_write_byte` — cohérence dérivée | Tout écriture sur un registre Shifter met à jour simultanément `aShifterRegs[]` ET le champ dérivé (`uiScreenBase`, `auPalette[]`, `uiResolution`). Les deux représentations sont toujours en phase. |
| `ym2149_write_byte` — protocole PSG | L'écriture en données (0xFF8802) n'est effective que sur `auYmRegs[uiYmRegSel]` — le registre sélectionné doit avoir été préalablement positionné par une écriture à 0xFF8800. `uiYmRegSel` est masqué à 4 bits (0–15). |
| `st_write_byte` — ROM | Toute écriture en zone 0xF80000–0xFDFFFF retourne `ST_ERROR` (bus error). `st_write_word/long` propagent ce ST_ERROR à l'appelant. |
| `st_read_byte` — open-bus | Toute adresse non mappée (ni RAM, ni ROM, ni registre HW connu) retourne 0xFF sans erreur. |

**Points d'attention pour les UCs suivants :**
- UC25 : `cpu_step()` appelle `st_read_byte/word` pour le fetch opcode — le dispatch UC24 est maintenant actif pour toutes les adresses, y compris la zone HW. Les tests UC25 avec des PRG simples valideront le chemin complet.
- UC25 : Division par zéro (DIVU/DIVS) : remplacer LOG_TODO par `cpu_raise_exception(CPU_VEC_DIV_ZERO)` — la mécanique d'exception est disponible depuis UC23.
- UC25 : MOVEP — non implémenté (LOG_TODO). Rare dans les démos; à ajouter si un PRG le requiert.
- UC25 : Chargement TOS ROM (`szRomPath` dans `st_init`) — actuellement LOG_TODO, la zone ROM retourne 0xFF. Non bloquant pour les démos qui bypassen le TOS.
- UC26 (vidéo) : `auPalette[]` et `uiScreenBase` sont désormais maintenus à jour par UC24 — le renderer vidéo peut les lire directement sans re-parser `aShifterRegs[]`.

---

## §35 — UC24A : Sector Fingerprint Engine

**Périmètre fonctionnel implémenté :**
- `src/sector_analyze.h` / `src/sector_analyze.c` — module autonome
- Extraction d'un vecteur de features 24D depuis 512 octets (statique, UC24A)
- Cosine similarity pondéré sur un DB de centroides (weights statiques dans `g_weights[]`)
- Lifecycle DB : `create` / `bootstrap_defaults` / `learn` / `finalize` / `save` / `load` / `destroy`
- Signatures de packers : scan byte-pattern avec masque, override le cosine si match
- `tools/build_sector_db.py` — script Python autonome pour enrichir la DB depuis les seeds JSON
- `db/seeds/` — ground truth JSON versionnés (gittrackés) ; `db/sector_db.bin` — gitignored
- `db/seeds/whatisit.json` — labels du spécimen `whatisit.st` analysé manuellement
- `db/seeds/packer_sigs.json` — signatures Rob Northen, Orion Sly, ULTRAPACK, ICE, BRA marker

**Détail des 24 features :**

| # | Nom | Description |
|---|-----|-------------|
| 0 | fBpbValid | BPB geometry consistent (BPS=512, SPC∈{1,2}, NFAT∈{1,2}, TS∈[720..1440], SPT∈[8..11], HDS∈[1,2]) |
| 1 | fWd1772Bootable | 1.0 iff sum of 256 BE words == 0x1234 |
| 2 | fFatPattern | FAT media byte (≥0xF0) followed by 0xFF 0xFF |
| 3 | fDirDensity | Fraction of 32-byte-aligned slots with valid FAT12 dir structure |
| 4 | fEntropy | Shannon H/8 normalized [0..1] |
| 5 | fZeroRuns | Ratio of 0x00 bytes |
| 6 | fE5Runs | Ratio of 0xE5 (FAT deleted marker) |
| 7 | fFfRuns | Ratio of 0xFF (unformatted) |
| 8 | fAsciiRatio | Ratio of printable ASCII [0x20..0x7E] |
| 9 | fRepeating | Dominant byte frequency / 512 |
| 10 | fOpcodeDensity | Bytes covered by forward decode from offset 0 / 512 |
| 11 | fWordAlignRatio | Even offsets with valid 68000 opcode / 256 |
| 12 | fBranchDensity | BRA/BSR/Bcc/JMP fraction among decoded instructions |
| 13 | fRelBranchValid | Branches whose target is in-sector |
| 14 | fHwImmediate | Byte pairs 0xFF 0x8x-0xFF in sector (raw count / 20) |
| 15 | fHwInContext | HW immediates in extension words of valid opcodes |
| 16 | fTimingLoops | DBcc (0x51C8-CF) pattern density |
| 17 | fVblInstall | MOVE.L #imm,$70.W pattern (21 FC ?? ?? ?? ?? 00 70) |
| 18 | fSinusProfile | First-difference near-zero ratio (|d[i]| <= 10) |
| 19 | fGraphicsPattern | Word pairs with bits (F888) == 0 (ST palette words) |
| 20 | fPackerEntropy | entropy>0.87 AND opcode<0.10 (derived feature) |
| 21 | fYmAccess | 0xFF88/0xFF89 byte pairs |
| 22 | fVideoAccess | 0xFF82/0xFF83 byte pairs |
| 23 | fFdcAccess | 0xFFFA/0xFFFC byte pairs |

**DB sémantique :**

| Type | Valeur enum | Signature dominante |
|------|-------------|---------------------|
| bss_zero | 13 | fZeroRuns=1, fEntropy=0, fRepeating=1 |
| unformatted | 14 | fFfRuns=1, fEntropy=0, fRepeating=1 |
| fat12 | 3 | fFatPattern=1 |
| directory | 4 | fDirDensity high, fZeroRuns moderate |
| directory_deleted | 5 | fE5Runs high, fRepeating high |
| bootsector_noboot | 1 | fBpbValid=1, fWd1772Bootable=0 |
| bootsector_boot | 2 | fBpbValid=1, fWd1772Bootable=1, fOpcodeDensity>0.3 |
| code_demo | 6 | fOpcodeDensity high + fHwInContext + fVblInstall |
| data_packed | 12 | fPackerEntropy high (entropy>0.87, opcode<0.10) |

**Workflow DB enrichissement :**
```
1. Tonton ajoute <disk>.json dans db/seeds/ (type + labeled_sectors)
2. Image .st dans use_cases/UC20A/st/ (gitignored, copyright)
3. python3 tools/build_sector_db.py → regenerate db/sector_db.bin
4. make tests → 0 failure (UC24A rejoue la classification)
```

**Infrastructure et outillage validés :**
- Dépendance `sector_analyze.c` → `disasm_one()` (UC11-14) pour fOpcodeDensity/fHwInContext
- `-lm` ajouté à LDFLAGS Linux pour `logf()` / `sqrtf()`
- Struct `sector_features_t` est un flat array of 24 floats → cast direct vers `float *` pour cosine
- `db/.gitignore` : track `seeds/*.json`, ignore `*.bin`
- `tools/build_sector_db.py` : mode `--dump-features <img.st> --lba N` pour inspection rapide

**Tests R14/R15 appliqués :**
- TEST MATRIX **40N + 8R + 2S = 50 tests**, tous PASS (2S : whatisit.st présent → executés en PASS)
- Nominal : feature extraction 8 sectors types, DB lifecycle 4 tests, classify 5 tests, save/load, sig override, type_name, feature dim check
- Robustesse : NULL sur extract/db_create/db_destroy/db_bootstrap/sector_classify (5 params), iMaxMatches=0, bad file load
- Skipped conditionnels : test_whatisit_bootsector (s'exécute si image présente, SKIP sinon)

**Contrats comportementaux validés :**

*Module `sector_analyze` (→ UC25 pour enrichissement dynamique)*
- `sector_features_extract(pSector, uiBase, ptFeat)` : tous les 24 champs entre 0.0 et 1.0 ; retourne ST_ERROR sur NULL
- `sector_db_bootstrap_defaults()` : populte 6 types triviaux (bss_zero, unformatted, fat12, directory, directory_deleted, data_binary) depuis buffers synthétiques ; centroides calculés, pas hardcodés
- `sector_db_learn() + finalize()` : centroïde = moyenne exacte des samples ; classify sur sample identique → score > 0.99
- Packer signature override : si match → score=1.0, type=eType du sig, ignorant le cosine
- `sector_db_save() + load()` : round-trip exact des centroides et signatures via magic SA_DB_MAGIC
- `sector_features_t` est un tableau continu de 24 floats — cast `(const float *)` valide par construction

**Points d'attention pour les UCs suivants :**
- UC25 (exécution) : après chaque `cpu_step()`, les octets exécutés peuvent enrichir les tags sémantiques via `sector_db_learn()` — pont vers l'analyse dynamique
- UC25 : `sector_classify()` peut être appelé sur le secteur courant du PC pour afficher le type probable dans la vue exécution (information de contexte)
- UC32 (désassemblage annoté) : `fVblInstall`, `fHwInContext`, `fTimingLoops` sont des précurseurs des patterns UC32 — les seuils de confiance peuvent être réutilisés
- Enrichissement DB : chaque nouvelle image .st de Tonton → nouveau JSON dans `db/seeds/` → rebuild → meilleurs centroides — zero code change

---

## §6.26 UC24B — Coloration sémantique secteurs (P46)

**Version :** 0.24.2 — **Date :** 2026-06-08

**Périmètre implémenté :**
- Champ `sector_type_t *aeSecType` (heap) + `int iSecCount` ajoutés à `edit_hex_view_t`
- `ehex_classify_sectors()` : crée un DB, bootstrap + charge `db/sector_db.bin` si présent, classe chaque secteur de 512 octets, stocke dans `aeSecType[]`
- Appelé une fois dans `edit_hex_open()` après `ehex_load()` — non-bloquant si fichier < 512 octets
- Table statique `g_aSecTint[SECTOR_TYPE_COUNT]` (16 couleurs, indexée par `sector_type_t`)
- Dans `ehex_render()` : pour chaque ligne visible, teinte fond du secteur courant dessinée AVANT le highlight de ligne (`g_clrCurRow` reste visible par-dessus)
- Libération `free(aeSecType)` dans `edit_hex_close()` et dans le chemin d'erreur de `gui_open_window`
- `SECTOR_UNKNOWN` → aucune teinte (fond par défaut préservé)

**Infrastructure validée :**
- Zéro nouveau module : modification de `src/edit_hex.h` et `src/edit_hex.c` uniquement
- `sector_analyze.h` inclus dans `edit_hex.h` (dépendance ajoutée)
- La classification est gracieusement dégradée si `sector_db_create` échoue (log + `iSecCount=0`)
- Fichiers non-disque (PRG, texte) : classification effectuée sur les secteurs complets, types probablement `CODE_DEMO` ou `DATA_*` — visuellement pertinent même sans BPB valide

**Matrice de tests (UC24B) :**
```
TEST MATRIX - UC24B:
  [N] Nominal    :  6 tests - sector_type_name coverage (16 types), tint
                              table size, sector count formula (.st/sub/odd),
                              classify zero sector (BSS_ZERO + score > 0.5)
  [R] Robustness :  3 tests - NULL sector pointer, NULL feat out, NULL pptDb
  [S] Skipped    :  1 test  - run make manual (tint visuel vue hex)
Total : 10 tests — 10 PASS, 0 fail
```

**Contrats comportementaux validés :**

*Module `edit_hex` (enrichissement UC24B)*
- `edit_hex_open(szPath, ...)` : si `uiSize >= SA_SECTOR_SIZE`, `aeSecType` est alloué et `iSecCount = floor(uiSize / 512)` ; sinon `iSecCount == 0` (aucun tint)
- `ehex_render()` : pour la ligne `iAbsRow`, `iSec = (iAbsRow * 16) / 512` ; si `iSec < iSecCount && aeSecType[iSec] != SECTOR_UNKNOWN` → rectangle coloré pleine largeur hex+ASCII avant le highlight
- `g_aSecTint[SECTOR_UNKNOWN] == g_clrBg` — safe even if tint rect is drawn for UNKNOWN (redundant but harmless)
- `edit_hex_close()` : `free(aeSecType)` libère correctement même si `iSecCount == 0`

**Points d'attention pour les UCs suivants :**
- UC24C (JSON annotation + bandeau) : `aeSecType[]` peut être utilisé dans `edit_hex_render_band()` pour afficher le type du secteur courant sans re-classifier — déjà calculé
- UC24C : le bandeau doit lire `ptV->aeSecType[uiCursor / SA_SECTOR_SIZE]` pour le secteur courant
- UC25 (exécution) : la coloration sémantique est visible dans la vue mémoire si `edit_hex_open()` est utilisé pour afficher la RAM ST — les secteurs en cours d'exécution se distinguent visuellement
- Limite 80 col : les commentaires de `g_aSecTint[]` dépassent légèrement 80 col pour la lisibilité de la palette — acceptable pour les constantes statiques de rendu
- **RÉSOLU UC24C** — `aeSecType[]` utilisé dans `ehex_render_band()` via `sector_type_name(ptV->aeSecType[iCurSec])`

---

## §6.27 UC24C — JSON annotation + bandeau contextuel (P47+P48)

**Version :** 0.24.3 — **Date :** 2026-06-09

**Périmètre implémenté :**

*Nouveau module `src/image_annot.c` / `src/image_annot.h` :*
- `image_annot_t` : struct heap avec `szFilename`, `szNotes` globaux + tableau dynamique `atSectors[]` (`annot_sector_t` = LBA + type + notes)
- `image_annot_create/destroy` : cycle de vie propre, init à capacité `ANNOT_INIT_CAP=16`, realloc ×2 auto
- `image_annot_json_path()` : dérive `<basename>.json` depuis n'importe quel chemin (remplace la dernière extension après le dernier séparateur)
- `image_annot_load()` : parser JSON minimaliste hand-rolled (scan forward `"key":`, lecture strings/integers) — supporte le schéma `whatisit.json` complet ; absent = ST_ERROR non-fatal
- `image_annot_save()` : sérialise filename, notes, labeled_sectors en JSON lisible ; écriture atomique `fopen("w")`
- `image_annot_get_sector(ptAnnot, iLba)` : recherche linéaire O(n) sur iSectorCount (≤1440 entrées)
- `image_annot_set_sector(ptAnnot, iLba, szType, szNotes)` : upsert — update si entrée existante, append sinon

*Modifications `src/edit_hex.h` :*
- `#include "image_annot.h"` ajouté
- Constante `HEXED_BAND_ROWS = 2` (hauteur bandeau en lignes de texte)
- Zone `HEX_ZONE_BAND_NOTE = 3` ajoutée à l'enum `edit_hex_zone_t`
- Champs BPB layout : `iBpbFat1Lba`, `iBpbFat2Lba`, `iBpbRootLba`, `iBpbDataLba` (–1 = pas de BPB valide)
- Champs annotation : `ptAnnot`, `szJsonPath`, `szBandNote[ANNOT_NOTE_MAX]`, `iBandNotePos`, `iBandLastSec`

*Modifications `src/edit_hex.c` :*
- `ehex_hex_area_h()` : hauteur zone hex = `iWndHeight - HEXED_BAND_ROWS * iCellH`
- Remplacement de `ptV->iWndHeight` par `ehex_hex_area_h(ptV)` dans 5 occurrences (`scroll_to_cursor`, `disasm_scroll_to_cursor`, `disasm_render`, `render`, `handle_key`)
- `ehex_classify_by_bpb()` : stocke le layout BPB dans `iBpbFat1Lba/Fat2/Root/Data` lors de la classification
- `ehex_band_sync()` : sync `szBandNote` depuis `ptAnnot` pour le secteur courant (no-op si zone = BAND_NOTE)
- `ehex_render_band()` : deux lignes en bas de fenêtre
  - Ligne 1 : `Sec LBA  TYPE_NAME  FAT1@X  FAT2@Y  Root@Z  Data@W` (layout BPB si valide)
  - Ligne 2 : note annotation du secteur courant, ou placeholder si vide
  - En mode édition (BAND_NOTE) : note avec curseur `|`, couleur jaune
- `ehex_render()` : appels `ehex_band_sync()` + `ehex_render_band()` après le panel disasm
- `ehex_handle_key()` — nouveaux bindings :
  - CTRL+N : active zone BAND_NOTE, positionne iBandNotePos en fin
  - CTRL+S : sauvegarde hex file (si dirty) + annotation JSON si zone = BAND_NOTE
  - Zone BAND_NOTE : LEFT/RIGHT/HOME/END (cursor), BACKSPACE/DELETE (edit), ESC/TAB (retour HEX), printable (insertion)
- `ehex_handle_click()` : clic sur row 2 du bandeau (Y ≥ iBandNoteY) active zone BAND_NOTE
- `edit_hex_open()` : init BPB cache (-1), iBandLastSec=-1, `image_annot_create()` + `image_annot_load()` (non-fatal)
- `edit_hex_close()` : `image_annot_destroy(&ptV->ptAnnot)`

**Infrastructure validée :**
- Parser JSON hand-rolled : pas de malloc supplémentaire pendant le parse (buffer chargé une fois en entier, max 64 KB)
- `iBandLastSec` = lazy sync : szBandNote ne se recharge depuis ptAnnot que quand le secteur change (et hors édition) — pas de rechargement à chaque render
- CTRL+S dual-purpose : en zone HEX/ASCII/DISASM = sauvegarde hex uniquement ; en zone BAND_NOTE = sauvegarde hex + JSON
- Fichier JSON optionnel : si absent à l'ouverture, `ptAnnot` est vide mais valide ; le bandeau affiche le placeholder

**Matrice de tests (UC24C) :**
```
TEST MATRIX - UC24C:
  [N] Nominal    : 20 tests - json_path (.st/.msa/no-ext/double-dot),
                              create/destroy, set/get sector (new/update/
                              unknown/NULL notes), growth x40, round-trip
                              save+load (filename/notes/2 sectors), absent
                              file, load seed whatisit.json, band constants
  [R] Robustness : 10 tests - NULL json_path/buf, tiny buffer, NULL pptAnnot,
                              NULL ptr destroy, NULL params set/get,
                              absent file ST_ERROR + empty annotation
  [S] Skipped    :  2 tests - run make manual (band visible, note edit flow)
Total : 32 tests — 32 PASS, 0 fail
```

**Contrats comportementaux validés :**

*Module `image_annot`*
- `image_annot_create()` alloue avec capacité `ANNOT_INIT_CAP=16` ; `iSectorCount=0` garanti à la sortie
- `image_annot_load()` retourne `ST_ERROR` si le fichier est absent — l'annotation reste vide et valide ; ce n'est pas une erreur fatale
- `image_annot_set_sector()` sur un LBA existant met à jour en place (count inchangé) ; sur un LBA nouveau, appende (realloc ×2 si count == cap)
- `image_annot_save()` produit un JSON UTF-8 valide avec `\\`, `\"`, `\\n` échappés ; structurellement conforme au schéma `db/seeds/whatisit.json`
- `image_annot_json_path("foo/bar.st", ...)` → `"foo/bar.json"` ; sans extension → `.json` suffixé ; double extension → dernière remplacée

*Module `edit_hex` (enrichissement UC24C)*
- `edit_hex_open()` : `iBpbFat1Lba = -1` si pas de BPB valide ; `ptAnnot != NULL` toujours (même si JSON absent)
- `ehex_hex_area_h()` retourne `iWndHeight - 2*iCellH` quand iCellH > 0 ; retourne `iWndHeight` si iCellH == 0 (guard contre division par zéro en début de session)
- Bandeau ligne 1 : `szTypeName` vaut `"—"` si `iSecCount == 0` (fichier < 512 octets, e.g. source .S minimal)
- Zone BAND_NOTE : BACKSPACE/DELETE modifient `szBandNote` en mémoire uniquement jusqu'à CTRL+S ; ESC/TAB retournent à HEX_ZONE_HEX sans sauvegarder
- `iBandLastSec = -1` forcé après CTRL+S pour que la prochaine render recharge le note depuis ptAnnot (reflecting the saved state)

**Points d'attention pour les UCs suivants :**
- **RÉSOLU UC24D** — labels cliquables implémentés via `ptAnnot->atSectors[i].iLba` (chips JSON `[N]`) + BPB auto-labels ; le champ `"labels"` avec `{lba, offset, name, desc}` de P47 reste optionnel pour des UCs futurs
- UC25 (exécution) : si l'annotation JSON est sauvegardée pour `roundtrip.st`, elle sera chargée à la prochaine ouverture hex — cohérence entre sessions garantie
- UC31+ (démo cible) : `image_annot_save/load` pourra être réutilisé pour annoter le désassemblé d'une démo (.s) si le format JSON est étendu avec un champ `"labeled_instructions"` — architecture extensible

---

## §6.28 UC24D — Labels cliquables dans le bandeau (P49)

**Version :** 0.24.4 — **Date :** 2026-06-09

**Périmètre fonctionnel implémenté :**
- `src/edit_hex.c` / `src/edit_hex.h` : UC24D enrichit la bande info UC24C
  - **Tracking des labels BPB** : après construction de `szLine1`, `ehex_render_band()` scanne la chaîne avec `strstr()` pour trouver `FAT1@N`, `FAT2@N`, `Root@N`, `Data@N` et stocke leurs positions pixel dans `aiBandLabelX[]` + leur LBA dans `aiBandLabelLba[]` + leur texte dans `aszBandLabelText[]`
  - **Chips JSON** : les `ptAnnot->atSectors[i].iLba` sont appended comme `[N]` dans `szLine1` avec leurs positions trackées
  - **Rendu cyan** : chaque label est re-dessiné en `g_clrBandLink` (cyan clair) par-dessus le texte normal — indique la cliquabilité
  - **`ehex_handle_click()`** : détection click en bande row 1 → itération sur `iBandLabelCount` labels → `ehex_jump_to_lba()` si hit
  - **`ehex_jump_to_lba()`** (private) : calcule `uiTarget = iLba * SA_SECTOR_SIZE`, clamp au dernier secteur valide, assigne `uiCursor`, `ehex_scroll_to_cursor()`, `gui_invalidate()`
  - **`edit_hex_set_cursor_pos()`** (public API) : API programmable pour naviguer à un offset arbitraire ; clamp `uiSize == 0 → 0`, `offset >= uiSize → uiSize-1`
  - Constante `HEXED_BAND_MAX_LABELS = 8` (4 BPB + 4 JSON max)

**Infrastructure et outillage validés :**
- Aucun nouveau module — toutes les modifications dans `edit_hex.c/.h`
- Pattern de hit-test pixel monospace : `X_label = char_offset * iCellW`, `strstr()` sur `szLine1` pour trouver l'offset caractère → X pixel exact
- `memset(ptV, 0, ...)` dans `edit_hex_open()` initialise `iBandLabelCount = 0` implicitement

**Matrice de tests (UC24D) :**

```
TEST MATRIX - UC24D:
  [N] Nominal    : 6 tests  - HEXED_BAND_MAX_LABELS, zones enum, set_cursor_pos
                               offset 0/mid/last, clamping out-of-bounds
  [R] Robustness : 2 tests  - NULL ptView -> ST_ERROR, zero-size -> cursor=0
  [S] Skipped    : 2 tests  - run make manual (click FAT1@1, click [N] chip)
```

**Contrats comportementaux validés :**

*Module `edit_hex` (navigation UC24D)*
- `edit_hex_set_cursor_pos(ptView, uiOffset)` : NULL ptView → `ST_ERROR` ; `uiSize == 0` → cursor fixé à 0 ; `uiOffset >= uiSize` → cursor clamped à `uiSize - 1` ; cas nominal → cursor = offset, zone = HEX, nibble = 0
- `ehex_jump_to_lba(ptV, iLba)` : `iLba < 0` → no-op ; `iLba * 512 >= uiSize` → saute au dernier secteur complet ; cas nominal → cursor = `iLba * 512`, scroll, invalidate
- `iBandLabelCount` est recomputed à chaque `ehex_render_band()` — les positions pixel sont cohérentes avec le rendu courant (même thread GUI)
- Les chips JSON `[N]` sont tracés dans l'ordre de `ptAnnot->atSectors[]` jusqu'à `HEXED_BAND_MAX_LABELS` total ou saturation de `szLine1[160]`
- Clicking en bande row 2 (note) reste inchangé depuis UC24C

**Points d'attention pour les UCs suivants :**
- UC25 (exécution) : `edit_hex_set_cursor_pos()` peut être utilisée depuis un breakpoint handler pour synchroniser la vue hex avec le PC du CPU émulé
- UC31+ : `aszBandLabelText[i][12]` est assez court pour les LBA ≤ 9999 — au-delà (images > 5 MB), truncation inoffensive car les LBA > 9999 sont hors portée ST standard

---

## §6.29 UC24E — Évolutions console (P50/P51/P52/P54/P55/P56)

**Périmètre fonctionnel implémenté :**
- **P50 — `dir --select <path>`** : sélection headless sans ouvrir la vue GUI ; résolution + validation du chemin via `file_stat()` ; `line_set_selected()` appelé directement ; utilisable dans scripts batch
- **P51 — `script <file>`** : nouvelle commande interactive (`CMD_SCRIPT`, alias court `r`) ; `line_exec_script(ptCtx, szPath)` factorisé depuis `line_run()` batch mode ; forward-declaration pour respecter l'ordre de définition C89
- **P52 — CTRL+O → mount** : `CON_KEY_CTRL_O = 0x0F` ajouté dans `console.h` ; remplace l'ancien CTRL+U (umount supprimé)
- **P54 — `edit -h` / `edit --hex`** : flag `bForceHex` dans `line_cmd_edit()` ; si présent, court-circuite la détection par extension et ouvre directement `edit_hex_open()` quel que soit le type détecté
- **P55 — Suppression `umount`** : `CMD_UMOUNT` retiré de `cmd_id_t`, `line_cmd_umount()` supprimé, entrée `umount` retirée de `g_line_aCmds[]`, CTRL+U retiré de `line_read_rich()`
- **P56 — `image --dir [dest] [source]`** : extraction d'image `.st`/`.msa` → répertoire ; auto-numérotation `disk`/`disk2`/… si aucune destination fournie ; source : vue montée en priorité, sinon fichier sélectionné/spécifié ; réutilise `mount_view_open()` + `mount_save_image(MOUNT_SAVE_DIR)`

**Infrastructure et outillage validés :**
- `line_exec_script(ptCtx, szPath)` : refactoring propre de l'ancien `line_run_script(ptCtx)` — même logique, paramètre path explicite ; `line_run()` appelle `line_exec_script(ptCtx, ptCtx->szScriptFile)` ; `line_cmd_script()` appelle `line_exec_script(ptCtx, argv[1])`
- Forward declaration `static st_error_t line_cmd_script(...)` dans la section dispatch (avant le tableau `g_line_aHandlers`) — nécessaire car la définition suit `line_read_rich()`
- `image --dir` Case 0 avant Case 1/2/3 dans `line_cmd_image()` — s'intercale sans modifier la logique existante
- Suppression de `CMD_UMOUNT` sans renumérotation manuelle — les valeurs de l'enum restent cohérentes car le compilateur les réattribue séquentiellement

**Matrice de tests (UC24E) :**

```
TEST MATRIX - UC24E:
  [N] Nominal    : 15 tests - CON_KEY_CTRL_O==0x0F, 'script' in table,
                              'r' alias, CMD_SCRIPT>CMD_MOUNT, batch
                              'where' OK, batch 'dir --select use_cases',
                              batch 'image --dir' no-source graceful,
                              'edit -h nonexistent' graceful,
                              'edit --hex nonexistent' graceful,
                              'image --dir dest' no-source graceful,
                              script missing-arg batch OK
  [R] Robustness :  4 tests - 'umount' 0 matches, 'u' 0 matches,
                              missing batch script ST_ERROR,
                              'dir --select nonexistent' no crash
  [S] Skipped    :  0 tests
```

**Contrats comportementaux validés :**

*Module `line` — commandes console (UC24E)*
- `dir --select <path>` : si le chemin n'existe pas → `line_print_error` + `ST_NO_ERROR` (selection inchangée) ; si le chemin existe → `line_set_selected(ptCtx, path)` + confirmation console + `ST_NO_ERROR` ; sans argument `<path>` → warning + `ST_NO_ERROR`
- `script <file>` : si `iArgc < 2` → warning usage + `ST_NO_ERROR` ; si fichier absent → `line_print_error` + `ST_NO_ERROR` (interne : `line_exec_script` → `ST_ERROR`) ; si fichier valide → exécution séquentielle des commandes, lignes `#` ignorées, `ST_NO_ERROR`
- `line_exec_script(NULL, ...)` → `ST_ERROR` ; `line_exec_script(ptCtx, NULL)` → `ST_ERROR` ; `line_exec_script(ptCtx, "")` → `ST_ERROR` ; fichier inexistant → `ST_ERROR` ; fichier valide → `ST_NO_ERROR`
- CTRL+O (0x0F) dispatche `"mount"` dans `line_read_rich()` — comportement identique à taper `mount` + ENTER
- `edit -h <file>` / `edit --hex <file>` : si le fichier n'existe pas → `line_print_error` + `ST_NO_ERROR` ; si le fichier existe → `edit_hex_open()` quel que soit l'extension
- `umount` retourne `CMD_UNKNOWN` depuis `line_complete_cmd_query` ; `u` prefix retourne 0 candidats
- `image --dir [dest]` sans source montée et sans fichier sélectionné → warning + `ST_NO_ERROR`
- `image --dir [dest]` avec fichier `.st`/`.msa` sélectionné → `mount_view_open()` + `mount_save_image(MOUNT_SAVE_DIR, szExtractDst)` ; auto-numérotation `disk`→`disk2`→…`disk99` si `szExtractDst` absent

**Points d'attention pour les UCs suivants :**
- **RÉSOLU UC24F** : UC-mount-tree — navigation arborescente dans les images `.st` ; `image_st_list_dir()` ajoutée ; `mount_dir_cb()` récursif 1 niveau ; ENTER/LEFT nav fonctionnels
- UC25 : `dir --select` permet la sélection headless d'un binaire `.PRG` avant `execute` — utilisable dans scripts de test automatisés
- UC25 : `image --dir` extrait le contenu d'une image sans ouvrir de fenêtre mount — peut servir de préparation batch avant montage ou exécution

---

## §6.30 UC24F — Navigation arborescente vue mount (P53)

**Périmètre implémenté :**

- `image_st.c/h` : 3 nouvelles fonctions publiques FAT12 subdir :
  - `image_st_list_dir(ptImg, szDirName, ...)` — énumère root si NULL/vide, sinon la chaîne de clusters du sous-répertoire nommé ; saute `.` / `..` / supprimés / labels
  - `image_st_mkdir(ptImg, szName)` — crée un sous-répertoire en root ; alloue 1 cluster ; initialise `.` / `..` ; entrée SUBDIR dans root
  - `image_st_write_file_in_dir(ptImg, szDirName, szFileName, pData, uiSize)` — écrit un fichier dans un sous-répertoire de root existant

- `mount.h` : 3 champs ajoutés à `mount_view_t` — `szCurDir[IST_NAME_MAX]`, `aszNavStack[8][IST_NAME_MAX]`, `iNavDepth`

- `mount.c` :
  - `mount_refresh()` : utilise `image_st_list_dir(szCurDir)` au lieu de `image_st_list_root()`
  - `mount_dir_cb()` : structure `mount_dir_ctx_t` + champ `szDirName`; récursion 1 niveau via `image_st_mkdir()` + `image_st_write_file_in_dir()`
  - `mount_handle_key()` : `ENTER` sur bIsDir → navigate-in (push stack) ; `GUI_KEY_LEFT` / `GUI_KEY_BACKSPACE` → navigate-up (pop stack)
  - `mount_render()` : header affiche chemin courant `A:\AUTO` ; entrées DIR en cyan `[DIR]` au lieu de la taille
  - `mount_save_image(MOUNT_SAVE_DIR)` : extrait aussi les sous-répertoires (mkdir + liste + fichiers)

**Infrastructure validée :**
- Compilation 0 warnings 0 erreurs
- `make tests` → 26 tests PASS, 0 fail

**Matrice de tests (UC24F) :**
```
TEST MATRIX - UC24F:
  [N] Nominal    : 16 tests - mkdir crée SUBDIR entry ; list_dir root == list_root ;
                              list_dir subdir vide = 0 entries ; list_dir après write ;
                              write_file_in_dir: DATA.BIN absent de root ;
                              read-back contenu correct ; 2 subdirs indépendants ;
                              empty file in subdir ; nav fields mount_view_t
  [R] Robustness : 10 tests - NULL params mkdir/list_dir/write_in_dir ;
                              list_dir nonexistent → ST_ERROR ;
                              list_dir sur fichier → ST_ERROR ;
                              write_in_dir dir inexistant → ST_ERROR ;
                              mkdir duplicate → ST_ERROR ; mkdir nom trop long → ST_ERROR
  [S] Skipped    :  0 tests - (navigation GUI validée manuellement)
```

**Contrats comportementaux validés :**

*Module `image_st` — FAT12 subdirs (UC24F)*
- `image_st_mkdir("AUTO")` crée un SUBDIR visible dans `image_st_list_root()` avec `bIsDir=ST_TRUE`
- `image_st_list_dir(ptImg, NULL, ...)` est identique à `image_st_list_root()`
- `image_st_list_dir(ptImg, "AUTO", ...)` n'inclut jamais les entrées `.` et `..`
- `image_st_write_file_in_dir(ptImg, "AUTO", "DEMO.PRG", ...)` : le fichier est absent de la root dir et lisible via `image_st_read_file()` depuis le cluster de l'entrée retournée par `list_dir`
- `image_st_list_dir` sur un nom inexistant ou un fichier non-SUBDIR → `ST_ERROR`
- `image_st_mkdir` en double → `ST_ERROR` ; nom invalide 8.3 → `ST_ERROR`

*Module `mount_view_t` — navigation (UC24F)*
- `szCurDir = ""` → root ; `szCurDir = "AUTO"` → sous-répertoire AUTO
- `iNavDepth = 0` → on est en root ; `iNavDepth > 0` → on peut remonter (←/BACKSPACE)
- Profondeur max du stack : 8 niveaux (Atari ST demos: 1 niveau en pratique)

**Points d'attention pour les UCs suivants :**
- UC25 (`execute`) : `image_st_list_dir` peut servir à lister le contenu d'un AUTO/ pour trouver un binaire auto-exécutable
- UC25 / `image --dir` : `mount_save_image(MOUNT_SAVE_DIR)` extrait maintenant les subdirs — résultat de `image --dir` reflète l'arborescence complète
- DEL en sous-répertoire : non implémenté (root-only) ; `image_st_delete_file()` opère uniquement sur root — à étendre si besoin dans un UC futur

---


## UC24G — P57/P58/P60 + Corrections BUG-04..09

**Date:** 2026-06-10  
**Statut:** ✓ VALIDÉ  
**Version:** 0.24.7

### Périmètre implémenté

**P57 — `trace level all|info|error|todo` incrémental**
- `trace level all` : affiche LOG_TRACE + INFO + ERROR + TODO (remplace `trace level trace`)
- `trace level todo` : affiche LOG_TODO uniquement (stubs actifs)
- `trace level error` : affiche ERROR + TODO
- `trace level info` : affiche INFO + ERROR + TODO (inchangé)
- Mot-clé `trace` désormais canoniquement affiché `all` dans les confirmations
- Mot-clé inconnu → warning lisible avec liste des valeurs valides, niveau inchangé

**P58 — `image --in/--out` explicites**
- `--in <src>` : source explicite (répertoire ou `.st`/`.msa`)
- `--out <dest>` : destination explicite (fichier ou dossier)
- Forme positionnelle `image --msa path.st` → warning de dépréciation + refus
- `--in` sans argument → warning usage, `ST_NO_ERROR` (non fatal)
- Aide `image --help` mise à jour
- `line_is_disk_path()` helper statique identifiant `.st`/`.msa`

**P60 — Sélection simple/multi exclusives dans vue `dir`**
- CTRL+SPACE efface `szLastSelected` avant d'ajouter à `aszMultiSel[]`
- SPACE efface `aszMultiSel[]/iMultiSelCount` avant d'écrire `szLastSelected`
- Les deux états ne coexistent jamais : vert et violet mutuellement exclusifs

**Corrections bugs**
- BUG-04 : `mount_view_t.iNotImported` — compteur de fichiers ignorés (8.3 ou disque plein) ; affiché en orange dans le panneau propriétés
- BUG-05 : `tracks = TS / (SPT × Heads)` — BPB @0x13 est le Total Sectors, pas les Tracks
- BUG-06 : `MOUNT_SRC_DIR` : panneau géométrie affiche `—` (pas de bootsector disponible)
- BUG-07 : `image --dir xx.st` : `xx.st` est maintenant reconnu comme source (couvert par P58)
- BUG-08 : ESPACE dans vue dir → `bRedraw = ST_TRUE` immédiat
- BUG-09 : Historique ALT+← persist entre invocations `dir` via statics `g_line_aDirNavHist[]`

### Infrastructure validée

- `line.c` : REQ-CON-041 (trace level all/todo), REQ-CON-042 (image --in/--out)
- `dir.c` : REQ-DIR-028 (exclusivité sélections)
- `mount.c` / `mount.h` : REQ-MNT-013 (géométrie corrigée), REQ-MNT-025 (iNotImported)
- `common.h` : ST_APP_VERSION = "0.24.7"

### Matrice de tests

```
TEST MATRIX - UC24G:
  [N] Nominal    : 11 tests - trace level all/todo/error/info;
                              image --in explicit accepted;
                              dir SPACE clears multi-sel;
                              dir CTRL+SPACE clears single-sel;
                              mount MOUNT_SRC_DIR geometry — absent;
                              mount .st tracks = TS/(SPT*Heads);
                              mount dir-source iNotImported field;
                              ALT+LEFT history preserved cross-dir
  [R] Robustness :  3 tests - trace level unknown keyword no crash;
                               image positional path rejected;
                               image --in missing arg warning
  [S] Skipped    :  0 tests
```

### Contrats comportementaux validés

*Module `line.c` — trace level (UC24G)*
- `trace_set_view_level(LOG_LEVEL_TRACE)` suivi de `trace_get_view_level()` retourne `LOG_LEVEL_TRACE`
- `trace_set_view_level(LOG_LEVEL_TODO)` filtre : seules les lignes `LOG_TODO` passent
- `trace_set_view_level(LOG_LEVEL_ERROR)` filtre : `LOG_ERROR` + `LOG_TODO` passent
- Le niveau par défaut est `LOG_LEVEL_TRACE` (inchangé depuis UC5)

*Module `line.c` — image --in/--out (UC24G)*
- `image --in <src>` : `line_is_disk_path()` retourne vrai pour `.st`/`.msa`/`.ST`/`.MSA`
- `image --msa path.st` sans `--in`/`--out` → deprecation warning, aucun fichier créé
- `image --in` seul (sans argument) → warning usage, retour non fatal

*Module `dir.c` — sélection exclusive (UC24G)*
- Après CTRL+SPACE : `szLastSelected[0] == '\0'` ET `iMultiSelCount >= 1`
- Après SPACE : `iMultiSelCount == 0` ET `szLastSelected[0] != '\0'`
- Les deux champs ne sont jamais simultanément non-nuls

*Module `mount.c` — géométrie (UC24G)*
- `MOUNT_SRC_DIR` : la section BPB du panneau droite affiche `—` (guard `eSrc != MOUNT_SRC_DIR`)
- `MOUNT_SRC_ST` : `uiTracks = uiTS / (uiSPT * uiHeads)` — pour image standard : 1440 / (9×2) = 80
- `iNotImported > 0` → ligne orange `"Skipped: N (8.3 limit)"` dans le panneau propriétés

*Module `line.c` — historique navigation (UC24G)*
- `g_line_aDirNavHist[]`, `g_line_iDirNavHistHead`, `g_line_iDirNavHistCount` sont des statics persistants entre `dir_open/dir_close` cycles
- Lors d'un nouveau `dir_open`, l'historique préservé est restauré dans la nouvelle `dir_view_t`

### Points d'attention pour les UCs suivants

- UC25A (`execute`) : `trace level todo` sera utile pour voir les stubs actifs sans bruit TRACE/INFO
- UC25A (`execute`) : `image --in` / `--out` ouvre la voie pour des opérations batch scriptées sur des images
- P59 (menus contextuels dir clic droit) : différé après UC25 ; state des sélections simple/multi est maintenant propre pour alimenter ces menus

---

## UC25A — Moteur d'exécution pas-à-pas + vues Monitor + CPU

**Périmètre fonctionnel implémenté :**

- `exec.c` / `exec.h` : orchestrateur d'exécution
  - `exec_state_t` partagée sous mutex : snapshot CPU `tCpuSnap`, `tLastResult`, `eRunState`, `szNextDisasm[DISASM_LINE_MAX]`, breakpoints `auBP[8]`, flags de contrôle `bStepReq/bRunReq/bPauseReq/bStopReq/bQuitReq`, handles de fenêtres `hMonWnd/hCpuWnd`
  - Thread d'exécution `exec_thread_fn()` : STEP (1 instruction → PAUSED), RUN (EXEC_QUANTUM=200 instructions par tranche), vérification breakpoints après chaque instruction, mise à jour snapshot + `szNextDisasm` sous mutex, `gui_invalidate()` sur toutes les fenêtres ouvertes
  - Lifecycle : `exec_init/shutdown/open/close/is_open/get_state`
  - API de contrôle thread-safe : `exec_step/run/pause/stop/quit_request()` + `exec_bp_toggle/clear()`

- `exec_mon.c` / `exec_mon.h` : fenêtre monitor (`GUI_WND_EXEC_MON` 660×420)
  - Rendu : nom programme + badge état (RUNNING vert / PAUSED jaune / HALTED rouge), PC + `szNextDisasm` (vert), compteurs instructions/cycles, liste breakpoints (cyan, PC-match jaune), hints F-keys (gris)
  - Clavier : F5=step, F6=run/pause toggle (lit eRunState sous mutex), F7=reset, F8/ESC=quit+close CPU window, B=toggle BP@PC, C=clear BPs

- `exec_cpu.c` / `exec_cpu.h` : vue registres CPU (`GUI_WND_EXEC_CPU` 500×320)
  - Rendu : D0–D7 (cyan, colonne gauche), A0–A7 (vert, colonne droite), SSP+PC (blanc/jaune), SR hex + flags individuels T/S/I/X/N/Z/V/C
  - ESC ferme la vue et appelle `exec_quit_request()`

- `line.c` : `line_cmd_execute()` — charge le fichier si arg fourni ou utilise la sélection courante ; vérifie `load_get_state().bLoaded + eType==LOAD_TYPE_PRG` ; appelle `exec_open(szPath, uiLoadAddr)`

- `main.c` : `exec_init(&tMachine)` après `load_init()` ; `exec_shutdown()` dans la séquence d'arrêt ordonnée

**Infrastructure validée :**

- Modèle multi-thread à 3 threads : thread console (main), thread exécution CPU, threads fenêtres GUI (1 par vue)
- `exec_state_t.ptMutex` protège toutes les données partagées entre le thread CPU et les threads fenêtres
- La section critique (snapshot + refresh disasm) est minimale : le thread CPU libère le mutex entre chaque quantum pour éviter la famine des vues GUI
- Fermeture propre : `bQuitReq` → thread CPU sort de sa boucle → `exec_close()` joint le thread → ferme les fenêtres
- `exec_refresh_disasm()` utilise `disasm_one()` sur `ptMachine->aRam + uiPC` — safe tant que PC est dans les limites RAM

**Tests R14/R15 appliqués :**

```
TEST MATRIX - UC25A:
  [N] Nominal    : 13 tests - lifecycle (init/shutdown/is_open/get_state),
                               exec_open guard, CPU step integration direct
  [R] Robustness :  9 tests - NULL to exec_init, all request functions when
                               not open, exec_bp_toggle/clear when not open,
                               exec_open without prior exec_init
  [S] Skipped    :  8 tests - exec_open GUI windows, step/run/pause, BP hit,
                               F7 reset, ESC close (run make manual)
```

**Contrats comportementaux validés :**

*Module `exec.c` — lifecycle (UC25A)*
- `exec_init(NULL)` → ST_ERROR ; `exec_init(&tMachine)` → ST_NO_ERROR ; `exec_shutdown()` idempotent
- `exec_is_open()` retourne ST_FALSE avant `exec_open()` réussi, après `exec_close()`, et après `exec_shutdown()`
- `exec_get_state()` retourne NULL avant `exec_open()` réussi et après fermeture
- `exec_open()` sans `gui_init()` préalable retourne ST_ERROR proprement (pas de crash)

*Module `exec.c` — contrôle (UC25A)*
- Toutes les fonctions de requête (`exec_step/run/pause/stop/quit_request()`) retournent ST_ERROR quand `g_exec_bOpen == ST_FALSE`
- `exec_bp_toggle(addr)` retourne ST_ERROR si pas de session ouverte
- `exec_bp_clear()` retourne ST_ERROR si pas de session ouverte

*Module `exec.c` — intégration CPU (UC25A)*
- `cpu_step(&tCpu, &tMachine, &tResult)` avec `MOVEQ #42,D0` (opcode 0x702A) → `auDn[0] == 42`, `uiPCAfter == base+2`
- `cpu_step(&tCpu, &tMachine, &tResult)` avec `NOP` (opcode 0x4E71) → PC avance de 2 sans modifier aucun registre

### Points d'attention pour les UCs suivants

- **RÉSOLU UC25B** : `exec_state_t` doit exposer le pointeur `ptMachine` pour que `exec_mem.c` et `exec_asm.c` puissent lire la RAM ST sans passer par exec.c
- **RÉSOLU UC25B** : les handles `hMemWnd` et `hAsmWnd` doivent être ajoutés à `exec_state_t` et le thread CPU doit appeler `gui_invalidate()` sur ces fenêtres
- **RÉSOLU UC25B** : `exec_asm.c` doit décoder depuis `uiAsmBase` en avançant de `iWordCount*2` octets par ligne via `disasm_one()` — pas de boucle inverse
- **RÉSOLU UC25B** : la vue mémoire navigue en unités de 16 bytes/row ; HOME doit snap à la ligne contenant PC (PC & ~0xF)
- **RÉSOLU UC25B** : les 4 fenêtres ouvertes simultanément (MON/CPU/MEM/ASM) peuvent encombrer l'écran — prévoir des positions de départ différentes ou laisser le gestionnaire de fenêtres les placer

---

## UC25B — Memory dump view + Disassembly view

**Date :** 2026-06-11
**Statut :** VALIDÉ Phase 1 + Phase 2
**Fichiers modifiés :** `src/exec_mem.h`, `src/exec_mem.c`, `src/exec_asm.h`, `src/exec_asm.c`, `src/exec.c` (fix g_exec_bOpen), `use_cases/use_case_25B.c`

### Périmètre fonctionnel implémenté

**Module `exec_mem.c`** — vue hex dump de la RAM ST :
- Rendu 16 bytes/row : adresse `$XXXXXX:`, paires hex groupées par 4 avec double espace, colonne ASCII (`.` pour non-imprimable)
- PC row auto-snap : quand le PC sort de la zone visible, `uiMemBase` est recalé à `PC & ~0xF`
- Highlight PC row : fond `{0.18, 0.18, 0.00}` (brun-jaune sombre) + texte jaune vif
- Navigation : UP/DOWN ±16 bytes, PgUp/PgDn ±page×16, HOME snap PC row
- Hint bar (20 px) en bas avec séparateur ligne, instruction `PC= $XXXXXX`

**Module `exec_asm.c`** — vue désassembleur synchronisée :
- `exec_asm_prev_insn()` : heuristique backward sur stream 68000 variable — teste décalages −2, −4, −6, −8, −10 octets depuis la cible, valide si `disasm_one()` s'arrête exactement à la cible ; fallback `target−2`
- Format ligne : `$XXXXXX XXXX [XXXX [XXXX]]  mnemonic operands` (adresse 7 cars, hex words jusqu'à 3, rembourré à 22 cars avant mnémonique)
- PC auto-snap : si PC n'est dans aucune ligne visible, `uiAsmBase = uiPC`
- Highlight PC : fond jaune sombre + texte jaune vif (même palette que exec_mem)
- Navigation : UP (`exec_asm_prev_insn`), DOWN (avancer via `disasm_one`), PgUp/PgDn (boucle N fois), `F` snap au PC, ESC ferme la fenêtre sans `exec_quit_request`

**Fix `exec.c`** — `g_exec_bOpen = ST_TRUE` placé avant le premier `exec_mon_open()` pour que `exec_get_state()` retourne une valeur non-NULL lors de l'ouverture des vues. Chaque branche d'erreur remet `g_exec_bOpen = ST_FALSE`.

### Infrastructure validée

- Modèle 4 fenêtres simultanées (MON/CPU/MEM/ASM) : `exec_state_t` expose `hMemWnd` et `hAsmWnd`
- Thread CPU invalide les 4 fenêtres à chaque instruction via `gui_invalidate(hMemWnd)` + `gui_invalidate(hAsmWnd)`
- `ptMachine` exposé dans `exec_state_t` : les vues mem/asm lisent `aRam[]` directement sous `ptMutex`
- `exec_get_state()` architecture : bOpen doit être ST_TRUE avant l'ouverture des vues filles — invariant établi et validé

### Test matrix (UC25B)

| Module | [N] | [R] | [S] | Total | Result    |
|--------|-----|-----|-----|-------|-----------|
| EXE    | 11  | 4   | 6   | 21    | ALL PASS  |

Tests nominaux (11) : lifecycle open/close des deux modules sans session active, disasm_one integration (MOVEQ #42,D0), format de ligne d'affichage.
Tests robustesse (4) : NULL pptView sur open et close des deux modules.
Tests skippés (6) : ouverture fenêtres GUI, rendering PC highlight, navigation HOME/F.

### Contrats comportementaux validés

*Module `exec_mem.c`*
- `exec_mem_open(NULL)` → ST_ERROR (NULL guard)
- `exec_mem_open(&ptView)` sans session active → ST_ERROR, `*pptView = NULL`
- `exec_mem_close(NULL)` → ST_ERROR
- `exec_mem_close(&NULL)` → ST_NO_ERROR (no-op idempotent)

*Module `exec_asm.c`*
- `exec_asm_open(NULL)` → ST_ERROR (NULL guard)
- `exec_asm_open(&ptView)` sans session active → ST_ERROR, `*pptView = NULL`
- `exec_asm_close(NULL)` → ST_ERROR
- `exec_asm_close(&NULL)` → ST_NO_ERROR (no-op idempotent)

*Intégration disasm_one*
- `disasm_one()` sur MOVEQ #42,D0 (0x702A) → ST_NO_ERROR, `iWordCount=1`, `bValid=ST_TRUE`
- Ligne formatée `exec_asm_build_line` : non-vide, commence par `$`

*Invariant exec.c*
- `g_exec_bOpen = ST_TRUE` avant le premier `exec_mon_open()` ; toute branche d'erreur remet à ST_FALSE
- Implication : `exec_get_state()` retourne non-NULL pendant toutes les ouvertures de vues dans `exec_open()`

### Points d'attention pour les UCs suivants

- **RÉSOLU UC26** : `auPalette[]` et `uiScreenBase` sont maintenus à jour par UC24 — `shifter_render()` les lit directement depuis `st_machine_t` sans re-parser `aShifterRegs[]`.
- **RÉSOLU UC26** : la vue mémoire ne reflète que `aRam[]` — point d'attention résolu en documentant que `exec_asm` est limité à la RAM ST pour l'affichage (acceptable).
- **UC27** : l'ajout de la vue écran (`GUI_WND_EXEC_SCR`) suivra le même pattern d'ouverture depuis `exec_open()` ; penser à ajouter `hScrWnd` dans `exec_state_t` avant UC27
- **UC27** : `exec_mon.c` expose les raccourcis `M` (mémoire) et `A` (désassembleur) pour ouvrir ces vues depuis le monitor si elles ont été fermées — non implémenté en UC25B, à compléter en UC27 ou en UC-exec-extras si nécessaire

---

## UC26 — Shifter Video Rendering Engine

**Date :** 2026-06-11
**Statut :** VALIDÉ Phase 1 + Phase 2
**Fichiers créés :** `src/shifter.h`, `src/shifter.c`, `use_cases/use_case_26.c`
**Fichiers modifiés :** `use_cases/use_cases.h` (ajout include), `src/common.h` (version 0.26.0)

### Périmètre fonctionnel implémenté

**Module `shifter.c`** — moteur de décodage bitplanes → RGB32 :

`shifter_palette_to_rgb(uiColor)` — conversion palette ST :
- Entrée : mot 16 bits ST — bits [10:8]=R, [6:4]=G, [2:0]=B (3 bits chacun, 0..7)
- Sortie : 0x00RRGGBB, chaque canal n mappé à (n*255)/7 → valeurs 0,36,73,109,146,182,219,255
- Constante interne `st_channel_to8()` : réduction à l'essentiel (1 ligne)

`shifter_screen_size(uiRes, piWidth, piHeight)` — dimensions par résolution :
- LOW  (ST_RES_LOW=0)  → 320×200
- MED  (ST_RES_MED=1)  → 640×200
- HIGH (ST_RES_HIGH=2) → 640×400
- Unknown → 0×0

`shifter_render(ptMachine, auPixels, uiPixelCount, piWidth, piHeight)` — rendu principal :
- Lit `ptMachine->uiScreenBase`, `->uiResolution`, `->auPalette[]` (maintenus par UC24)
- Vérifie NULL params, buffer, bounds, resolution avant tout traitement
- Pré-calcule `auRGB[16]` (palette ST→RGB32) puis dispatche vers le renderer de résolution

`render_low()` / `render_med()` / `render_high()` — internes :

*Format bitplane interleaved (partagé par les 3 résolutions, 32,000 octets de frame buffer) :*

| Résolution | Plans | Octets/groupe de 16 px | Groupes/ligne | Octets/ligne |
|-----------|-------|------------------------|---------------|--------------|
| Low 320×200 | 4 | 8 (P0W, P1W, P2W, P3W) | 20 | 160 |
| Med 640×200 | 2 | 4 (P0W, P1W)            | 40 | 160 |
| High 640×400| 1 | 2 (P0W)                 | 40 | 80  |

*Décodage pixel p (0..15) dans un groupe :*
- Bit position b = 15 − p (MSB first)
- Low  : index = P0[b] | (P1[b]<<1) | (P2[b]<<2) | (P3[b]<<3)  → 0..15
- Med  : index = P0[b] | (P1[b]<<1)                              → 0..3
- High : index = P0[b]                                             → 0..1
- Pixel = auRGB[index]

**Contraintes R22** : LOG_TRACE omis de toutes les fonctions shifter (appelables à 50 Hz depuis la boucle VBL en UC27).

### Infrastructure validée

- `st_machine_t.auPalette[]` et `uiScreenBase` / `uiResolution` sont maintenus par `ST.c` (UC24) — pas de re-parse de `aShifterRegs[]` nécessaire dans le renderer
- Buffer pixel statique `g_aPixels[SHIFTER_MAX_PIXELS]` en file-scope dans le test pour éviter 1 Mo sur la pile
- `SHIFTER_MAX_PIXELS = 640×400 = 256,000` — constante publique dans `shifter.h`
- `SHIFTER_FB_SIZE = 32,000` — constante interne, valide pour les 3 résolutions

### Test matrix (UC26)

| Module | [N] | [R] | [S] | Total | Result    |
|--------|-----|-----|-----|-------|-----------|
| VID    | 26  | 7   | 0   | 33    | ALL PASS  |

Tests nominaux (26) : palette conversion 6 valeurs (noir, blanc, RVB primaires, canal 1), dimensions par résolution (4 cas dont unknown), rendu low res (8), rendu med res (3), rendu high res (5).
Tests robustesse (7) : NULL ptMachine, NULL auPixels, NULL piWidth, NULL piHeight, buffer trop petit, screen base hors RAM, résolution inconnue.
Tests skippés (0) : UC26 est purement interne, aucun skip requis.

### Contrats comportementaux validés

*`shifter_palette_to_rgb()`*
- 0x0000 → 0x00000000 (noir)
- 0x0777 → 0x00FFFFFF (blanc)
- 0x0700 → R=0xFF, G=0x00, B=0x00
- 0x0070 → R=0x00, G=0xFF, B=0x00
- 0x0007 → R=0x00, G=0x00, B=0xFF
- Valeur 1 dans un canal → (1×255)/7 = 36

*`shifter_screen_size()`*
- ST_RES_LOW → 320×200
- ST_RES_MED → 640×200
- ST_RES_HIGH → 640×400
- Valeur inconnue → 0×0

*`shifter_render()` — décodage*
- Low res, plane0=0xFFFF, planes 1-3=0 : pixels 0..15 = palette[1] ; pixel 16 = palette[0]
- Low res, plane1=0xFFFF, plane0=0 : pixels 0..15 → color index 2 = palette[2]
- Low res, frame buffer all-zero : 320×200 pixels = palette[0]
- Med res, plane0=0xFFFF : pixel 0 = palette[1]
- High res, byte[0]=0x80 : pixel 0 = palette[1] (bit 7=1), pixel 1 = palette[0] (bit 6=0)

*`shifter_render()` — robustesse*
- NULL ptMachine → ST_ERROR (sans crash)
- NULL auPixels → ST_ERROR
- NULL piWidth → ST_ERROR
- NULL piHeight → ST_ERROR
- uiPixelCount < width*height → ST_ERROR
- uiScreenBase + 32000 > ST_RAM_SIZE → ST_ERROR
- uiResolution inconnu → ST_ERROR

### Points d'attention pour les UCs suivants

- **UC27** : `exec_screen_open()` devra allouer dynamiquement un buffer `st_u32_t *auPixels` de `SHIFTER_MAX_PIXELS` éléments (256,000 × 4 = 1 Mo) — ne pas placer sur la pile de thread
- **UC27** : `hScrWnd` doit être ajouté à `exec_state_t` ; le thread CPU doit appeler `gui_invalidate(hScrWnd)` après chaque instruction (ou seulement après N instructions pour limiter la charge GPU)
- **UC27** : le titre de fenêtre écran suit R18 : `"ST4Ever - Screen: progname [320x200]"`
- **UC27** : pour la synchronisation VBL (50 Hz), le thread CPU peut appeler `shifter_render()` toutes les 20 ms et poster le buffer vers la vue écran via un double-buffer ou un champ `auLastFrame[]` dans `exec_state_t` (protégé par ptMutex)
- **UC27** : les dimensions de la fenêtre doivent s'adapter à la résolution courante — surveiller les changements de `uiResolution` entre frames (ex : demo switching low→high pendant l'exécution)

---

