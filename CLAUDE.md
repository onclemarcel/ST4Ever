# Projet : ST4Ever : The Revival Engine for the Timeless ATARI ST

## 0. Historique et Raisons de changement
- 2026-05-30: UC1 validé & pratiques de développement validées au travers de ce document CLAUDE.md
- 2026-05-30: UC2 validé & nouvelles pratiques agréées dans CLAUDE.md
- 2026-05-30: UC3 découpé en UC3.1/UC3.2/UC3.3 (décision co-développeur : D2D dès UC3, menu contextuel différé à UC7/UC18)
- 2026-05-30: UC3.1 validé — msg_queue + Win32 window thread + WndProc
- 2026-05-30: UC3.2 validé — Direct2D + DirectWrite + renderer portable (COBJMACROS + INITGUID requis)
- 2026-05-31: UC3.3 validé — dir.c lazy-load + flat list + ASCII render + clavier/souris/scroll + gui_set_title (R18)
- 2026-05-31: UC4 découpé en UC4.1/UC4.2/UC4.3 (contexte window limité ; raw input isolé en UC4.2 ; complétion/historique en UC4.3)
- 2026-05-31: UC4.1 validé — gui_request_close + ESC/←/→/ESPACE + bShowHidden + dir -a + focus auto + TEST_MANUAL + make manual
- 2026-05-31: P19 appliqué — `trace off` ne ferme plus la vue ; filtre LOG_TRACE seulement
- 2026-06-01: UC4.2 validé — console.h (CON_KEY_*) + pipe/VT100 mintty + Win32 cmd.exe + line_read_rich (insert/edit/CTRL shortcuts) + make manual UC=XX
- 2026-06-01: fix dir.c 4 warnings `-Wformat-truncation` (szPat+3, iPathLen check, szLine+320)
- 2026-06-01: fix focus-retour — `hPrevFgWnd` dans `win_wnd_state_t`, restauration dans `WM_DESTROY` (focus revient au terminal source à la fermeture des vues GUI)
- 2026-06-01: UC4.3 validé — historique ↑↓ circulaire + ~/.st4ever_history + TAB completion (commandes/chemins) + ghost text DIM + prompt contextuel [T][file] + colors on/off (c_*() runtime) + --script batch + mutex ptSelectedMutex + line_set/get_selected() + CON_KEY_TAB
- 2026-06-01: UC4.4 validé — fenêtre GUI_WND_TRACE D2D (trace_view_t ring buffer + mutex + auto-scroll) ; trace_log() append → gui_invalidate() avec garde réentrante ; suppression TODO(UC3)/TODO(UC3.3) ; ST_APP_VERSION 0.4.4
- 2026-06-01: P26 appliqué — `trace` (toggle-open) active LOG_TRACE off par défaut ; `trace on` obligatoire pour activer LOG_TRACE ; `trace off` inchangé
- 2026-06-01: fix focus — fenêtre trace n'accroche plus le focus (read-only) ; `win_gui.c` : bloc P16 conditionné à `eType != GUI_WND_TRACE`
- 2026-06-01: fix stderr — `trace_log()` et `trace_flush_compact()` ne passent plus par stderr ; terminal propre dès que `bOpen=TRUE` ; diagnostic via `st4ever_trace.log` ; `trace_level_ansi()` + macros ANSI supprimées (dead code)
- 2026-06-01: R22 appliqué — purge LOG_TRACE des primitives récurrentes ; §6.9 UC4.4 ajouté avec contrats comportementaux (END key fix, stderr supprimé, focus non volé) (mutex lock/unlock/create/destroy, renderer draw/begin/end, gui invalidate/get_size/msg_post/get, win_gui invalidate/get_size/get_native_handle/set_title) ; LOG_TRACE cantonné aux fonctions UX et cycle de vie
- 2026-06-01: UC5 validé — `where`/`info`/`history [N]` + P8 console title (`SetConsoleTitleA`) + P21 `H` dir hidden-toggle + P22 F5 refresh + P23bis TAB common prefix avant cycle ghost + P24 `colors auto` via `isatty()` + P27 `trace clear` + P28 `trace level trace|info|error` ; `CMD_INFO`/`CMD_HISTORY` ; `trace_clear()`/`trace_set_view_level()` ; `line_update_console_title()` ; ST_APP_VERSION 0.5.0
- 2026-06-01: UC6 validé — `src/file.h` + `src/file.c` : `file_stat`, `file_open/read/write/close`, `file_mkdir`, `file_list_dir` (callback) ; CRT + POSIX portable ; ST_APP_VERSION 0.6.0
- 2026-06-01: UC7 validé — `src/load.h` + `src/load.c` : détection type, chargement binaire raw + PRG stub (magic 0x601A, .text+.data) + fixup TODO(UC15) ; `line_cmd_load()` ; `line_cmd_info()` live ; P11 `szLastSelected` + `g_dir_clrLastSel` (vert sombre) dans `dir_render()` ; `st_machine_t` + `load_init/shutdown` dans `main.c` ; ST_APP_VERSION 0.7.0
- 2026-06-02: UC8 validé — `src/edit_txt.h` + `src/edit_txt.c` : éditeur texte D2D (load/save, numéros de ligne, sélection, CTRL raccourcis, clipboard) ; `gui.h` : `GUI_MOD_CTRL/SHIFT/ALT` + `uiMods` dans `tKey` + API clipboard ; `win_gui.c` : WM_KEYDOWN/WM_CHAR avec modificateurs, CTRL+lettre via codes de contrôle, implémentation clipboard Win32 ; `lx_gui.c` : stubs clipboard ; `line_cmd_edit()` routage texte/binaire ; `g_line_ptEditTxtView` dans `line.c` ; ST_APP_VERSION 0.8.0
- 2026-06-02: UC9 validé — `src/edit_hex.h` + `src/edit_hex.c` : éditeur hex+ASCII D2D (load/save, zones HEX/ASCII, navigation nibble, édition nibble+byte, TAB toggle zone, clic souris, CTRL+S) ; routage `line_cmd_edit()` `.prg/.ttp/.tos/.bin/.raw → edit_hex`, `.st/.msa/.stx → mount` ; `g_line_ptEditHexView` dans `line.c` ; ST_APP_VERSION 0.9.0
- 2026-06-03: UC12 validé — `src/disassemble.c` étendu : ADD/ADDA/ADDI/ADDQ/ADDX, SUB/SUBA/SUBI/SUBQ/SUBX, CMP/CMPA/CMPI/CMPM, MULU/MULS/DIVU/DIVS, AND/ANDI/OR/ORI/EOR/EORI, NOT/NEG/NEGX/TST/EXT.W/EXT.L ; groupes 0x0/0x5/0x8/0x9/0xB/0xC/0xD ; helpers `disasm_dreg_ea`, `disasm_x_a`, `disasm_imm_ea`, `disasm_addq_subq`, `disasm_addx_subx`, `disasm_mul_div`, `disasm_cmpm`, `disasm_unary_ea` ; ST_APP_VERSION 0.12.0
- 2026-06-03: UC13 validé — `src/disassemble.c` étendu : ASL/ASR/LSL/LSR/ROXL/ROXR/ROL/ROR (formes registre immédiat + registre Dn + mémoire .W), BTST/BCHG/BCLR/BSET (#imm statique + Dn dynamique) ; groupe 0xE `disasm_groupE` ; group 0x0 étendu `disasm_bit_static/dynamic` ; tables `g_aszShiftMnem`, `g_aszShiftMemMnem`, `g_aszBitOp` ; ST_APP_VERSION 0.13.0
- 2026-06-03: UC14 validé — `src/disassemble.c` étendu : NOP/RTE/RTS/RTR, TRAP #N, LINK An/UNLK An, JSR/JMP (toutes EA contrôle), BRA/BSR/Bcc court (.S, 8-bit) et long (16-bit) ; groupe 0x6 `disasm_group6` ; `disasm_no_operand` helper ; `g_aszBcc[16]` ; bloc 0x4Exx dans `disasm_misc4` ; `use_case_11.c`+`use_case_12.c` : ADAPTED(UC14) RTS ; ST_APP_VERSION 0.14.0
- 2026-06-04: UC16 validé — `src/image_st.h` + `src/image_st.c` : FAT12 .st raw image (create/load/save/list_root/read_file/write_file/delete_file/free_bytes) ; fixtures roundtrip.st via test ; ST_APP_VERSION 0.16.0
- 2026-06-06: UC17 validé — `src/image_msa.h` + `src/image_msa.c` : MSA RLE codec (imsa_compress/decompress) + `image_msa_load/save` sur `image_st_t` ; `image_st_get_disk()` ajouté à `image_st.h/c` ; round-trip blank + fichiers + 0xE5 escape ; ST_APP_VERSION 0.17.0
- 2026-06-06: UC18.1 validé — `src/mount.h` + `src/mount.c` : vue D2D `GUI_WND_MOUNT` (liste FAT + panneau propriétés) ; `mount_view_open/close/add_file` ; `line_cmd_mount/umount` ; `gui_find_window_by_type` ; fix szExt sans point ; ST_APP_VERSION 0.18.1
- 2026-06-06: UC18.2 validé — P10 historique navigation dir (ALT+←/→ : `aszNavHistory[16]`, `dir_nav_history_push`, `dir_navigate_to`) ; P14 multi-sélection CTRL+ESPACE (`aszMultiSel[16]`, fond violet, files only) ; P34 propriétés BPB lecture seule (H/S/T) ; P36 suppression /IST_RDE de l'en-tête ; P37 détection Bootable WD1772 (`mount_is_bootable`) ; P38 touche B → bootsector vers `edit_hex_open` avec titre heuristique ; ST_APP_VERSION 0.18.2
- 2026-06-06: UC15A validé — torture test désassembleur 68000 vs SOURCE.PRG (DEVPAC3, ATARI ST réel) : 2525 instructions, DIFF=0 ; 5 bugs disasm corrigés (groupE iIR inversé, EXG An,An ordre DEVPAC3, NBCD/CHK/size=3 ext words) ; `DISASM_SYNTAX.md` créé à la racine ; use_case_11/13.c ADAPTED:UC15A
- 2026-06-03: UC15 validé — `src/load.c` `load_do_prg()` complet : header 28 octets, chargement .text+.data à ST_LOAD_BASE, BSS zeroing, table de fixups Atari ST (bitstream : first_offset 4B, 0x00=fin, 0x01=skip 254B, N=advance+fix) ; abs_flag géré (pas de table fixup) ; `load_state_t` étendu (uiTextSize/uiDataSize/uiBssSize/uiFixupCount) ; helpers `load_apply_fixups` + `load_skip_bytes` ; fixtures `use_cases/UC15/` (fixup/bss/absolute/multi_fixup/bad_fixup.prg) ; TODO(UC15) supprimé ; ST_APP_VERSION 0.15.0
- 2026-06-03: UC11 validé — `src/disassemble.c` : désassembleur 68000 réel MOVE/MOVEQ/LEA/CLR/EXG/SWAP/PEA + décodeur EA complet (12 modes) ; `bValid = ST_TRUE` sur instructions reconnues ; `TC-DIS-001 ADAPTED(UC11)` ; ST_APP_VERSION 0.11.0
- 2026-06-02: UC10 validé — `edit_hex.h/c` étendu : panneau désassembleur D2D synchronisé (zone HEX_ZONE_DISASM, cache 512 instructions, `ehex_disasm_cache_update/find_cursor/scroll_to_cursor/render`) ; TAB cycle 3 zones HEX→ASCII→DISASM→HEX ; F2 toggle ; sync bidirectionnel hex↔disasm ; fenêtre plus large (1320px) avec disasm ; `EDIT_HEX_WND_WIDTH_DISASM/STD/HEIGHT` ; ST_APP_VERSION 0.10.0

## 1. Contexte du projet

Ce projet est une application console interactive multi-plateforme développée en C pur à but éducatif permettant de:
- lire/écrire des fichiers ATARI ST (texte ou binaires .PRG, .TTP, .TOS)
- créer/lire des images disques ATARI ST de type .st, .msa, .stx à partir de/vers des répertoires PC
- modifier, compiler, assembler, désassembler, décompiler les programmes ATARI ST
- émuler les binaires ATARI ST via divers vues (CPU, Mémoire, écran) en pas à pas, temps ralenti ou temps réel en fonction des commandes utilisateurs sur les vues et/ou la console interactive
- les vues comprennent : 
    - un éditeur de fichier texte et/ou fichier source d'un binaire (e.g. assembleur d'un .PRG)
    - un éditeur héxadécimal pour l'édition des binaires (e.g. un .PRG, .TTP)
    - une vue assembleur 68000 au format DevPac3 ATARI ST pour les sections binaires pertinentes de la vue hexadécimale (e.g. .text ou toute plage d'adresse fournie par utilisateur)
    - un moniteur d'exécution des binaires ATARI ST permettant la sélection pas à pas, stop, go, temps d'exécution par instruction
    - un visualisateur CPU 68000 avec état des registres pour l'exécution des binaires
    - un visualisateur mémoire ATARI ST pour l'exécution des binaires
    - un émulateur écran/inputs/outputs ATARI ST simple pour l'exécution des binaires
    - un visualisateur d'arborescence fichiers/répertoires pour les sélections/gestions de fichiers et des images disques .st, .msa, .stx
    - une console de trace/logs/erreurs pour l'application et debuggage développeur de ST4Ever

 A noter que l'émulation ATARI ST de ST4Ever est totalement développée from scratch dans un but éducatif : Hatari et STeem sont de très bons émulateurs complet et performants. L'émulation ATARI ST ST4Ever est simplifiée de manière à satisfaire les besoins d'exécution pas-à-pas des fonctions citées ci-dessus.

La phase de "Revival" est une extension qui ST4Ever au-delà de l'émulation ATARI ST vers un portage du code d'une démo ST sous forme de C portable exécutable nativement, sans émulateur 68k, sur de multiples cibles. Le comportement de la démo portée ne sera pas strictement exacte par rapport à l'original ST, mais restera fidèle au "feeling" utilisateur : effets visuels reconnaissables, timing VBL raisonnable, son et inputs présents. L'extension est décrite en détail en section 9.

### 1.1 Objectifs fonctionnels

Chaque commande est accessible par son nom complet, son alias monogramme et un raccourci CTRL. L'éditeur de ligne (UC4) offrira : historique ↑↓, Home/End, tab-completion fichiers/répertoires, pré-affichage en gris de la complétion.

**Commandes validées :**

| Commande | Alias | CTRL       | UC      | Comportement résumé |
|----------|-------|------------|---------|---------------------|
| `help`    | `h`   | CTRL+H     | ✓ UC1   | Liste toutes les commandes avec synopsis ; ignore les arguments (warning) |
| `quit`    | `q`   | CTRL+Q / C | ✓ UC1   | Ferme toutes les vues et quitte proprement ; ignore les arguments (warning) |
| `trace`   | `t`   | CTRL+T     | ✓ UC2   | Sans arg : toggle open/close. **Ouverture : LOG_TRACE off par défaut** (P26). `on` : ouvre + active LOG_TRACE. `off` : filtre LOG_TRACE uniquement — **la vue reste ouverte** (P19). `clear` : vide le ring buffer GUI. `level trace\|info\|error` : filtre d'affichage GUI. Détails §6.2, §6.9, §6.10 |
| `colors`  | `c`   | —          | ✓ UC4.3 | Sans arg : toggle on/off. `on` : active les codes ANSI. `off` : désactive (utile terminal sans VT100 ou redirection fichier). Auto-détecté via `isatty()` au démarrage (P24). Détails §6.8 |
| `where`   | `w`   | CTRL+W     | ✓ UC5   | Affiche le répertoire de travail courant et le fichier sélectionné via `dir`. Détails §6.10 |
| `info`    | `n`   | —          | ✓ UC5   | Dashboard complet : cwd, sélection, état trace (ouvert/LOG_TRACE), colors, historique, disque monté (stub), binaire chargé (stub). Détails §6.10 |
| `history` | `y`   | —          | ✓ UC5   | Sans arg : 10 dernières commandes numérotées. Avec `N` : les N dernières. Détails §6.10 |

**Commandes à implémenter — spécification détaillée dans les sous-sections ci-dessous :**

#### 1.1.3 (`d` | `dir` | `CTRL+D`) 
Cette commande ouvre une vue GUI de type tree view et affiche l'arborescence indentée du contenu du répertoire fourni en argument de la commande. La commande sans paramètre ouvre le répertoire courant. La vue GUI permet :
  - la sélection d'un fichier ou répertoire par clic gauche souris ou par touche 'espace' est surlignée dans la vue GUI et la sélection devient l'argument par défaut des commandes `load`, `edit`, `image`, `mount`, `wf`.
  - l'expansion ou non d'un répertoire par clic gauche souris sur + devant le nom du répertoire
  - la rétractation d'un répertoire expansé par clic gauche souris sur - devant le nom du répertoire
  - la remontée de l'arborescence du répertoire parent via une première ligne '..' en haut de l'arborescence
  - l'affichage d'un menu contextuel par clic droit souris sur un fichier ou répertoire:
       - clic droit sur fichier affiche les commandes (`load`, `edit`)
       - clic droit sur répertoire affiche les commandes (`mount`, `image`)

#### 1.1.4 (`l` | `load` | `CTRL+L`) 
Cette commande prend en argument un nom de fichier (+ path) en entrée ou le fichier sélectionné via la commande `dir`; si aucun fichier n'est sélectionné et aucun paramètre donné, `load` ne fait rien et indique à l'utilisateur de sélectionner un fichier ou entrer un chemin. `load` se comporte de la manière suivante selon les éléments sélectionnés:
   - si l'argument fourni ou par défaut est un fichier : charge ce fichier dans la mémoire émulée de l'ATARI ST à un emplacement libre. Pour une image, un fichier texte, la copie en mémoire est conforme au contenu du fichier. Pour un fichier binaire au format ATARI ST (.PRG, .TTP, .TOS), la montée en mémoire se fait selon les conventions du TOS ATARI ST, en mettant à jour les fixups d'addresse du programme. Ce programme binaire pourra être directement exécuté depuis la mémoire virtuelle ATARI ST par l'émulateur CPU 68000 et/ou par l'émulateur machine ATARI ST.
   - si l'argument fourni ou par défaut est un répertoire: renvoie un message utilisateur pour indiquer que la commande `mount` doit être utilisée pour les répertoires
   - si l'argument fourni ou par défaut est une image disque ATARI ST .st, .msa, .stx: renvoie un message utilisateur pour indiquer que la commande `mount` doit être utilisée pour les images disques.

#### 1.1.5 (`e` | `edit` | `CTRL+E`) 
Cette commande prend en argument un nom de fichier (+ path) en entrée ou le fichier sélectionné via la commande `dir`; si aucun fichier n'est sélectionné et aucun paramètre donné, `edit` ne fait rien et indique à l'utilisateur de sélectionner un fichier ou entrer un chemin. `edit` se comporte de la manière suivante selon les éléments sélectionnés:
   - si l'argument fourni ou par défaut est un répertoire: renvoie un message utilisateur pour indiquer que la commande n'a pas d'effet sur les répertoires
   - si l'argument fourni ou par défaut est un fichier : ouvre une vue d'édition en fonction de la typologie du fichier : 
       - éditeur de texte avec scroll bar et numéro de ligne pour les fichiers texte, 
       - éditeur héxadécimal tabulaire pour les binaires, images, avec scroll bar et addresse en chaque début de ligne et adresse en colonne, avec une vue additionnelle texte ASCII, ainsi qu'une vue texte assembleur 68000 au format DEVPAC3 ATARI ST lorsque la partie hexadécimale correspond à du code exécutable de type .text ou bootsector d'une image disque ou lorsque l'utilisateur sélectionne une plage d'adresse du fichier binaire.
   - Le fichier est éditable et les modifications peuvent être sauvegardées dans les deux formats, avec confirmation utilisateur pour la sauvegarde:
       - l'édition texte des fichiers texte se fait de manière classique via l'éditeur de texte. Si le fichier texte est un fichier assembleur 68000 au format DEVPAC3, un clic droit souris peut permettre la génération d'un binaire ATARI ST .PRG ou .raw (pour un binaire brut) dans le même répertoire que le fichier texte.
       - l'édition hexadécimale est possible par :
           - insertion/remplacement des valeurs hexadecimales aux adresses,
           - la modification ASCII par la vue ASCII change les octets hexadecimaux,
           - la modification de code assembleur 68000 en ligne par recompilation de la section concernée; les impacts binaires sont à déterminer par l'outil d'assemblage et demande de confirmation par utilisateur. La vue assembleur 68000 est cliquable bouton droit pour sauvegarder le fichier texte correspondant au source assembleur.

#### 1.1.6 (`m` | `mount` | `CTRL+M`) 
Cette commande prend un chemin de répertoire en entrée ou le répertoire sélectionné via la commande `dir`; si aucun chemin n'est sélectionné et aucun paramètre donné, le chemin courant de l'application est utilisé, mais une demande de confirmation utilisateur est nécessaire. `mount` se comporte de la manière suivante:
   - émule la montée d'un répertoire dans un disque A:\ ATARI ST en affichant une vue arborescence disquette. 
   - permet l'ajout de fichiers dans l'émulation disquette par clic+glissement souris dans la vue à partir de la vue ouverte par la commande `dir`
   - permet la suppression de fichiers dans l'émulation disquette par clic+glissement souris hors vue
   - la vue complète permet la création d'une image disque à partir d'un clic droit sur la première ligne de l'arborescence nommée "A:\", via la commande `image` en popup. La vue intègre également un bandeau vertical de propriétés de la disquette montée : ces propriétés sont celles du header disk ATARI ST.

#### 1.1.7 (`x` | `execute` | `CTRL+X`)
Cette commande prend en argument le fichier binaire exécutable ATARI ST (.PRG, .TTP ou .TOS) ou celui sélectionné dans la vue `mount` de l'émulation disquette ou dans la vue de la commande `dir`. `execute` comprend les vues suivantes:
    - un moniteur d'exécution des binaires ATARI ST permettant la sélection pas à pas, stop, go, temps d'exécution par instruction
    - un éditeur héxadécimal pour l'édition des binaires (e.g. un .PRG, .TTP) visualisant les instructions en cours d'exécution
    - un visualisateur CPU 68000 avec état des registres pour l'exécution des binaires
    - un visualisateur mémoire ATARI ST pour l'exécution des binaires, avec possibilité de plusieurs vues par plages d'adresse mémoire (plage interruptions, mémoire système, RAM, Vidéo, TOS)
    - un émulateur écran/inputs/outputs ATARI ST simple pour l'exécution des binaires

La commande `execute` privilégie l'exécution pas à pas avec l'ensemble des vues interactives entre elles, cependant l'exécution en temps ralenti, réel ou accéléré reste possible en tâche de fond avec mise en place de breakpoint pour points de visibilité. La vitesse d'exécution des binaires et la synchronisation complète des différentes vues lors de l'exécution sera limitée par la performance CPU/Video de la machine exécutant l'application ST4Ever; le moniteur d'exécution donne les informations d'exécution et de performance en instructions émulées par secondes.

#### 1.1.8 (`u` | `umount` | `CTRL+U`)
Cette commande permet de clore la vue ouverte par la commande `mount` tout en démontant l'émulation disquette A:\ ATARI ST. `umount` simule le retrait de la disquette du lecteur ATARI ST. Si le contenu disquette émulé ne correspond à aucune image .st, .stx ou .msa existante, demande à l'utilisateur s'il souhaite créer une nouvelle image de la disquette en cours de retrait pour ne pas perdre les modifications réalisées lors de la commande `mount`.

#### 1.1.9 (`w` | `where` | `CTRL+W`)
Cette commande permet d'afficher le répertoire de travail ou le fichier sélectionné courant via la commande `dir` ou le répertoire par défaut dans lequel l'application est lancée. La réponse à la commande est fournie par texte console.

#### 1.1.10 (`t` | `trace` | `CTRL+T`)
Validée en UC2 — voir table §1.1 et contrats comportementaux §6.2. La fenêtre trace GUI sera ouverte via `gui_open_window(GUI_WND_TRACE)` en UC3.3 (actuellement : sortie stderr ANSI).

### 1.2 Contraintes de conception
Les vues ouvertes par les commandes sont interactives, non-modale et exécutée dans des threads séparés de l'application. Les vues sont capables de communiquer entre elles de manière dynamique selon leur besoin, par exemple la vue arborescence de la commande `dir` permet de glisser des fichiers dans l'arborescence de l'émulation disquette de la commande `mount`.
Dans les phases d'exécution des binaires, les vues d'émulation CPU 68000, mémoire ATARI ST, vue écran graphique et vue binaire hexadécimale sont mises à jour en cohérence de l'exécution.

L'application ST4Ever est développée pour Windows dans un premier temps, avec des stubs anticipés pour une plateforme Linux. Toutes les logiques qui ne dépendent pas d'une plateforme Windows ou Linux sont en code portable dans ./src de l'arborescence du projet. Le code portable utilise des fonctions d'abstraction appelant des fonctions Windows ou Linux en back-end, développé dans des fichiers ./src/win ou ./src/linux. La contrainte de développement est de maximiser le code portable et minimiser le back-end spécifique au strict nécessaire (Windows calls, DirectX calls pour Windows, Posix/system calls, X11 calls pour Linux)

### 1.3 Phases de développement

Le développement s'effectue par Use Cases (voir section 6), permettant à Claude de progresser phase par phase avec une validation régulière des implémentations fonctionnelles. L'objectif reste la lisibilité du code et de l'avancement pour un aspect didactique et éducatif : plus on prend notre temps, mieux on apprend.

**À chaque Use Case, Claude :**
- Effectue les modifications des sources/includes/Makefile/ressources selon le périmètre fonctionnel du UC, en cohérence avec les objectifs fonctionnels (section 1.1) et les recommandations (section 5).
- Met à jour ce fichier CLAUDE.md selon la recommandation R13 (toutes les sections concernées, en cours et en fin de UC).
- Signale si le UC s'avère trop complexe pour être traité d'un bloc, et propose un découpage en sous-UCs numérotés et insérés dans le tableau de la section 6.

**En fin de Use Case validé (`make tests` : 0 failure), Claude :**
- Met à jour README.md si la progression est visible pour un lecteur extérieur : ce fichier sert de *teaser* public sur l'aspect didactique, le revival ATARI ST des années 90 et les pratiques de co-développement avec Claude, acteur principal du projet guidé par Tonton Marcel.
- Met à jour SRTD.md pour la traçabilité User/System & Software Reqs/Tests.
- Consulte les propositions d'amélioration de la section 7, donne son avis sur l'aspect réaliste ou non de la proposition et propose un traitement dans les UC suivants.
- Fait des propositions d'amélioration UX en section 7 pour arbitrage avant la clôture de l'UC.


## 2. Architecture des composants

Le découpage fonctionnel des composants (rôle, API, dépendances, séquences) est dans **SRTD.md §3–§4**.

Structure du projet :

| Répertoire / Fichier | Contenu |
|---|---|
| `src/` | Code portable : common.h, trace, line, gui, renderer, dir, ST, CPU, disassemble, as, edit, mount, exec |
| `win/` | Backend Windows : win_console, win_gui, win_D2D, win_platform |
| `linux/` | Backend Linux (stubs) : lx_console, lx_gui, lx_X11, lx_platform |
| `use_cases/` | Tests par UC : use_case_NN.c + ressources UC*/ |
| `Makefile` | Build multi-platform : `make` / `make tests` / `make run` / `make clean` |
| `SRTD.md` | Traçabilité requirements/design/tests — référence composants §3–§4 |

> Anciennement "Fichiers clés" avec headers doc de chaque .c/.h. Transférée dans SRTD.md §4 à partir de UC3.1 (2026-05-30). Les headers courants sont dans les sources.


## 3. Packaging
tar -czf project.tar.gz src Makefile README-SPEC.md

## 4. Conventions

### 4.1 Langue
Documentation et code en anglais. Ce fichier CLAUDE.md reste en français.

### 4.2 Nommage — notation hongroise

**Préfixes de variables** (minuscule, collés au nom fonctionnel en PascalCase) :

| Préfixe | Type C                          | Exemples                        |
|---------|---------------------------------|---------------------------------|
| `b`     | `st_bool_t`                     | `bRunning`, `bOpen`             |
| `e`     | `enum *_t`                      | `eCmd`, `eLevel`, `eState`      |
| `i`     | `int`                           | `iLine`, `iArgc`                |
| `ui`    | entier non-signé, `size_t`      | `uiLen`, `uiIndex`              |
| `sz`    | `char *` (chaîne C)             | `szCwd`, `szFmt`, `szFunc`      |
| `p`     | pointeur brut (non-struct)      | `pStart`, `pHandle`             |
| `pt`    | pointeur vers `struct *_t`      | `ptCtx`, `ptMutex`              |
| `ppt`   | pointeur vers `struct **_t`     | `pptMutex`, `pptThread`         |
| `pfn`   | pointeur de fonction            | `pfnEntry`                      |
| `a`     | tableau (précède autre préfixe) | `aszArgv`, `aCmds`              |

Pour les variables de type struct allouées sur la pile (non-pointeur) : préfixe `t`
(e.g. `tCtx`, `tCmd`).

**Variables globales** : `g_<module>_<préfixe><Nom>`, toujours `static` dans leur
`.c` (e.g. `g_trace_bOpen`, `g_line_aCmds`).

**Types fixes** : utiliser `st_u8_t` / `st_u16_t` / `st_u32_t` / `st_i32_t` etc.
(définis dans `common.h`) pour toute donnée à largeur fixe (registres 68000, champs
de structure disque/PRG). Utiliser `int` / `size_t` pour les compteurs et tailles
locales génériques.

**Structures** : nom en `snake_case` terminé par `_t`
(e.g. `parsed_cmd_t`, `line_context_t`).

**Fonctions** : verbe(s) + substantif(s) en `snake_case`
(e.g. `line_init()`, `trace_is_open()`).

**Constantes / macros** : `SCREAMING_SNAKE_CASE` avec préfixe `ST_`
(e.g. `ST_MAX_PATH`, `ST_NO_ERROR`).

### 4.3 Style de code
- Largeur de ligne : 80 colonnes.
- Style d'accolades Allman : `{` sur sa propre ligne pour toutes les structures de
  contrôle et définitions de fonctions.
- Déclarations de variables toutes en tête de fonction (style C89), avant tout code.
  Résultat : l'empreinte mémoire complète de la fonction est visible d'un coup.
- Paramètres de fonction et déclarations de variables alignés verticalement par groupe.
- Pas d'appels de fonctions dans les arguments d'autres appels.
- Préférer des fonctions courtes et bien nommées à du code condensé.

### 4.4 Modèle d'erreur et retours de fonctions

**Code de retour** : toute fonction portable retourne `st_error_t`
(`ST_NO_ERROR` / `ST_ERROR`). Les valeurs fonctionnelles passent par des paramètres
pointeurs `[out]`.

**Visibilité** : toute fonction non déclarée dans un `.h` est `static`.

**`const` sur les paramètres** : `const T *` pour tout paramètre `[in]` pointeur
non modifié.

**Vérification de tous les retours** : chaque appel de fonction — interne, glibc ou
plateforme — a son code de retour testé selon le contrat documenté de la fonction
appelée (référence : MSDN pour Win32, man pages pour POSIX/glibc). Ne pas supposer
qu'une valeur non-nulle signifie succès ; vérifier la valeur exacte attendue
(ex. `WAIT_OBJECT_0`, pas juste `!= 0`).

**Format `LOG_ERROR` selon la couche** :
- Appels Win32 :
  `LOG_ERROR("CreateThread failed: %lu", GetLastError())`
  Si la fonction retourne aussi un code : inclure les deux —
  `LOG_ERROR("WaitForSingleObject failed: result=%lu err=%lu", dwResult, GetLastError())`
- Appels POSIX/glibc :
  `LOG_ERROR("function_name failed: %s", strerror(errno))`
- Allocations mémoire :
  `LOG_ERROR("malloc failed for TypeName")`

**Erreurs non fatales** : si un appel échoue sans bloquer l'application (ex. `fopen`
du fichier log, `localtime`), utiliser `fprintf(stderr, ...)` ou `LOG_INFO` avec
description de la dégradation, puis continuer avec le comportement dégradé documenté.

**Libération avant `return ST_ERROR`** : si des allocations ont réussi avant l'échec,
les libérer dans l'ordre inverse d'allocation et remettre les pointeurs à `NULL`
avant de retourner `ST_ERROR`.

### 4.5 Structure interne des fonctions

Ordre canonique du corps d'une fonction :

```
1. Déclarations de variables locales (toutes en tête)
2. Garde de validation des paramètres  →  LOG_ERROR + return ST_ERROR
3. LOG_TRACE d'entrée (fonctions publiques et handlers)
4. Corps fonctionnel
5. LOG_INFO de sortie (si l'effet est significatif)
6. return
```

**Garde de validation** — toujours en premier après les déclarations :
```c
if (ptParam == NULL)
{
    LOG_ERROR("NULL parameter: ptParam=%p", (void *)ptParam);
    return ST_ERROR;
}
```

**LOG_TRACE** — après la garde, pour les fonctions publiques et les handlers.
Pas dans les micro-helpers internes (< 5 lignes, aucun effet de bord).
Cast `(void *)` obligatoire sur tout pointeur passé avec `%p` :
`LOG_TRACE("ptX=%p uiY=%u", (void *)ptX, uiY)`

**LOG_INFO** — juste avant le `return ST_NO_ERROR` final si la fonction produit
un effet observable. Décrire l'effet : `LOG_INFO("quit requested")` et non
`LOG_INFO("line_cmd_quit OK")`.

**Initialisation des structs `[out]`** :
`memset(ptOut, 0, sizeof(*ptOut))` avant toute affectation de champ.

### 4.6 Documentation des `.h`

Chaque fonction publique est documentée dans son `.h` avec :
```c
/*
 * function_name() - Une ligne de description.
 *
 * Parameters:
 *   param1 [in]     : ...
 *   param2 [out]    : ...
 *   param3 [in/out] : ...
 *
 * Returns:
 *   ST_NO_ERROR  ...
 *   ST_ERROR     ...
 */
```

### 4.7 Macros de log (`trace.h`)

| Macro              | Usage                                                                 |
|--------------------|-----------------------------------------------------------------------|
| `LOG_TRACE(fmt,…)` | Entrée de fonction + paramètres clés. Compactée si consécutive.      |
| `LOG_INFO(fmt,…)`  | État fonctionnel significatif (progression, résultat notable).        |
| `LOG_ERROR(fmt,…)` | Erreur interne : NULL, appel système échoué, état incohérent.        |
| `LOG_TODO(fmt,…)`  | Stub ou code à compléter (marque les UC futurs).                     |


## 5. Décisions Techniques & Recommandations Claude AI / Onclemarcel

**R1 — GUI : utilisation de DirectX2D + X11 à but éducatif**
Bien que des librairies telles que SDL2 soient disponible sous MSYS2 (`mingw-w64-x86_64-SDL2`), et fonctionnent sous Linux sans modification, le développement d'un renderer abstrait adapté au juste besoin de la logique de ST4Ever et d'un backend D2D Windows et X11 Linux permet de mieux comprendre les détails de ce type de code portable, au dépend d'une architecture de l'application plus complexe et d'un temps de développement plus long. 

**R2 — Émulateur 68000 : dispatch par table**
Utiliser une table primaire de 256 entrées `[opcode >> 8]` → groupe d'instructions, puis tables secondaires par groupe de modes d'adressage. Chaque handler modifie la structure `cpu68k_t`. Référence : *MC68000 Programmer's Reference Manual* (Motorola, domaine public). Commencer par le sous-ensemble démo : MOVE/MOVEQ/LEA/CLR, ADD/SUB/CMP, BRA/BSR/Bcc/JMP/JSR/RTS, TRAP.

**R3 — Formats disque : priorité .st → .msa, déprioritiser .stx**
`.st` = dump brut secteur (737 280 octets fixes, trivial). `.msa` = `.st` compressé RLE, simple. `.stx` (Pasti) = format de copy-protection avec timing secteur non-standard, CRC tricks, weak bits — extrêmement complexe. La quasi-totalité des démos fonctionne en `.st` ou `.msa`. Réserver `.stx` à une UC future optionnelle.

**R4 — Modèle de threading : message queues par vue**
Chaque vue GUI tourne dans son thread avec une file circulaire bornée (thread-safe via mutex). Le thread console poste des messages aux vues, les vues renvoient des événements (fichier sélectionné, breakpoint atteint...). La file `gui_msg_queue_t` est définie dans `gui.h` (étroitement liée au sous-système GUI). Les primitives mutex/thread (`st_mutex_t`, `st_thread_t`) sont elles dans `common.h`.

**R5 — Line editor riche : UC dédiée**
Sous MSYS2 (mintty), utiliser `ReadConsoleInput()` avec `ENABLE_PROCESSED_INPUT` désactivé pour capturer les séquences escape VT100 (flèches = `\033[A/B`, Home/End = `\033[H/F`). Historique = tableau circulaire de `ST_MAX_HISTORY` entrées. Tab-completion = `FindFirstFile()` / `opendir()` sur le préfixe courant. Scope UC4.

**R6 — Format PRG : bien documenté, direct**
Header 28 octets (magic `0x601A`, tailles .text/.data/.bss, flags). Fixups = bitstream : `0x00`=fin, `0x01`=sauter 254 octets, sinon offset depuis dernier fixup. Implémentable en ~200 lignes propres. Référence : *Atari ST Internals* (Abacus Books, trouvable en PDF).

**R7 — DEVPAC3 : documenter les différences 68000 standard**
Directives clés : `DC.B/W/L`, `DS.B/W/L`, `EVEN`, `SECTION TEXT/DATA/BSS`, labels locaux `.loop`. VASM (compilateur open-source) implémente la syntaxe Motorola compatible DEVPAC3 — bon code de référence pour l'assembleur et le désassembleur.

**R8 — Assets de test dans use_cases/UC*/**
Inclure dès UC1 : un binaire 68000 minimal hand-crafté (`MOVEQ #42,D0 + RTS` = 4 octets de .text), un header PRG valide autour, une image `.st` de 737 280 octets avec FAT12 valide, et un `.S` DEVPAC3 minimal. Ces fichiers servent de fixtures stables pour tous les UCs suivants.

**R9 — TOS minimal pour les démos**
La majorité des démos contourne le TOS et écrit directement dans les registres matériel. Implémenter en priorité : Line-A (`0xA000-0xA0FF` : affichage, fontes), les registres vidéo Shifter (`0xFF8200-0xFF827F` : palette, résolution, adresse écran), YM2149 (`0xFF8800-0xFF8803` : son). GEMDOS peut rester en stub pour les premières démos.

**R10 — Ordre de développement recommandé**
Phase 1 (UC1–UC9)  : Infrastructure console + Win32 GUI + éditeurs texte/hex.
Phase 2 (UC10–UC14): Désassembleur 68000 (UC10–13) + vue intégrée hex+ASCII+désasm (UC14).
Phase 3 (UC15–UC20): Formats fichiers/disque : PRG fixups, .st/.msa, mount/image.
Phase 4 (UC21–UC30): CPU 68000 + émulation ST complète + assembleur DEVPAC3.
Phase 5 (UC31)     : Démo cible via émulateur ST4Ever — référence visuelle + pivot vers revival.
Phase 6 (UC32–UC39): Extension Revival (§9) — analyse, HAL, Pi Zero bare metal, PC natif.

**R11 — Standard de compilation : `-std=gnu99`**
Les macros `LOG_TRACE`, `LOG_INFO`, `LOG_ERROR`, `LOG_TODO` utilisent la syntaxe `##__VA_ARGS__` (extension GNU) pour permettre un appel sans argument variadique : `LOG_ERROR("message")`. Cette syntaxe est standard pour tous les compilateurs GCC/MinGW-W64. Le Makefile doit utiliser `-std=gnu99` et non `-std=c99`. Les flags `-DST_PLATFORM_WINDOWS` / `-DST_PLATFORM_LINUX` ne doivent **pas** être passés depuis le Makefile : `common.h` les détecte automatiquement via `_WIN32` et `__linux__` définis par GCC, évitant une redéfinition.

**R12 — Exécution des tests depuis la racine du projet**
La cible `make tests` doit être exécutée depuis la racine du projet (là où se trouvent `src/`, `use_cases/`, etc.). Les binaires de test `tests/use_case_NN` utilisent des chemins relatifs pour accéder aux fichiers de test (e.g. `"use_cases/UC01/hello.prg"`). Lancer `tests/use_case_01` directement depuis `tests/` échoue au chargement du PRG de test. La cible `make tests` inclut ce rappel dans son en-tête d'exécution.

**R13 — Maintenance de ce fichier CLAUDE.md**

CLAUDE.md est l'interface de projet entre sessions de conversation. Sa mise à jour relève de Claude à chaque UC, **en cours et en fin**. Sections concernées et règles :

- **Section 2** : recopier ici le header documentaire de tout fichier `.c` nouveau ou modifié, directement depuis les sources. Ce contrat documentaire est la mémoire entre sessions.
- **Section 4** : ajouter toute nouvelle pratique d'implémentation devant s'appliquer uniformément aux UCs suivants (nommage, style, modèle d'erreur…).
- **Section 5** : ajouter toute nouvelle recommandation technique avec numéro séquentiel (R_N_) et date. Ne jamais renuméroter les recommandations existantes.
- **Section 6 (tableau)** : mettre à jour le statut du UC courant ; réviser le tableau après décision d'arbitrage (amélioration/découpage).
- **Section 6.x (sous-section UC validé)** : créer ou compléter selon le modèle de la section 6.1. Elle doit inclure : périmètre implémenté, infrastructure validée, matrice de tests (N/R/S), anomalies résolues, et un paragraphe **"Contrats comportementaux validés"** par module — ces contrats assurent la traçabilité verticale : besoin utilisateur (1.1) → invariant technique (6.x) → preuve exécutable (`use_case_XX.c`). Ils guident les sessions futures lors de l'implémentation réelle.
- **Section 7** : créer si des améliorations fonctionnelles/UX émergent pendant le UC. Les soumettre à validation pour planification parmi les UCs existants ou en tant que nouveau UC.

**Règles absolues :**
- Ne pas modifier une section déjà validée sans noter la raison et le numéro du UC qui impose le changement.
- Ne jamais supprimer de contenu historique : utiliser les marquages `ADAPTED: UCN` ou des notes de changement inline.

**R14 — Non-régression des tests use_case_XX : stratégie "intent-stable, assertion-évolutive"**
Chaque `use_case_XX.c` valide l'**intention fonctionnelle** du UC, pas les détails d'implémentation du stub. Quand un UC ultérieur fait évoluer un comportement, on adapte l'assertion, pas l'intention. Règles :

1. **Documenter l'intention séparément de l'assertion** : chaque bloc de test commence par un commentaire INTENT structuré incluant la chaîne de traçabilité complète issue de SRTD.md :
   ```c
   /* INTENT[INT-XXX-NNN → TC-XXX-NNN → REQ-XXX-NNN → UFR-XXX-NNN]:
    * description du comportement attendu de façon immuable */
   ```
   Quand plusieurs TCs ou REQs consécutifs partagent le même INTENT, les noter en plage (`TC-XXX-001..003`) ou en liste (`REQ-XXX-001,004`). Quand une assertion est mise à jour, ajouter `/* ADAPTED: UCN - raison */` sur la ligne précédente.

2. **Politique lors de l'implémentation d'un UCN** : avant de valider UCN, lancer `make tests`. Si un test antérieur échoue :
   - Comportement stub → réel (attendu) : mettre à jour l'assertion + commenter `ADAPTED: UCN`
   - Régression vraie (inattendu) : corriger le bug, pas le test

3. **Macro `TEST_SKIP` pour tests incompatibles headless** : quand un test UC antérieur suppose un stub mais que l'implémentation réelle requiert un display ou une ressource système (ex : `gui_init` ouvre une vraie fenêtre en UC3), protéger via `#ifdef ST_TEST_LEVEL_UCNN` — le Makefile définit `-DST_TEST_LEVEL_UC01` lors de la compilation de `use_case_01`. La variante `TEST_SKIP("raison")` log le skip sans échouer.

4. **Règle absolue** : `make tests` doit passer entièrement (**0 warning, 0 failure**) avant tout commit d'un nouveau UC. Les tests skippés sont acceptables ; les tests en échec ou les warnings de compilation bloquent.

5. **Ne jamais supprimer un test** : si un comportement antérieur est définitivement remplacé, remplacer l'assertion par un `TEST_SKIP` avec l'explication, pour conserver la traçabilité de l'intention originale.

**R15 — Couverture de tests : nominal et robustesse**
Deux axes de couverture, calibrés par couche. Pas de mesure gcov (stubs faussent les métriques, trop lourd pour projet éducatif). Pas de mocks — utiliser des fonctions stub explicites dans `use_cases/` si nécessaire. Validation GUI reste manuelle.

*Critères par couche :*

| Couche | Nominal | Robustesse | Skip headless |
|---|---|---|---|
| `trace`, `line` (C pur) | 100 % fonctions publiques | tous les `ST_ERROR` documentés | — |
| `ST`, `CPU`, `disasm` | 100 % (par instruction 68k) | NULL + bounds + opcode inconnu | — |
| Fichiers / PRG | 100 % | format KO + fichier vide | — |
| Platform `win/lx` | nominal | partiel | si besoin display |
| GUI / renderer | skip | skip | oui (validation manuelle) |
| Threading | nominal | partiel | oui (UC4+) |

*Cas robustesse prioritaires :*
- `NULL` sur chaque paramètre pointeur → `ST_ERROR` sans crash
- Valeurs limites : `0`, taille max, adresse `0xFFFFFF + 1`
- Formats invalides : PRG magic incorrect, image `.st` tronquée, opcode 68000 inconnu
- État incohérent : double `init`, appel sans `init` préalable, double `close`
- Concurrence : accès simultané à `gui_msg_queue_t` (UC4+)

*Flags 68000 (UC11-UC23)* : pour chaque instruction arithmétique, vérifier explicitement les flags SR (N, Z, V, C, X) contre la spec Motorola, pas seulement le résultat du calcul.

*Convention dans les `use_case_XX.c`* : étiqueter chaque `TEST_ASSERT` avec `[N]` (nominal) ou `[R]` (robustesse) dans le libellé. Ajouter en tête de fichier un bloc bilan :
```c
/* TEST MATRIX - UCnn:
 *   [N] Nominal    : X tests  - all public functions in foo.h, bar.h
 *   [R] Robustness : Y tests  - NULL params, bounds, invalid formats
 *   [S] Skipped    : Z tests  - require display (validated manually)
 */
```

**R16 — Tests manuels : macro `TEST_MANUAL` et cible `make manual`** *(planifié UC4, 2026-05-30)*

Certains tests exigent une validation visuelle (fenêtres GUI, rendu écran) ou une interaction utilisateur et ne peuvent pas être automatisés headless. Séparation stricte :

- **`make tests`** = automatique, 0 interaction, CI-friendly. Les tests non-automatisables sont marqués `TEST_SKIP`.
- **`make manual`** = session de validation humaine. Compile les `use_case_NN` avec `-DST_MANUAL_TEST` et les exécute. Les blocs `TEST_MANUAL` affichent une question et attendent `y/n`.

Macro à ajouter dans `use_cases.h` lors de UC4 :
```c
#ifdef ST_MANUAL_TEST
#define TEST_MANUAL(desc, question) \
    do { \
        printf("  [MANUAL] %s\n  > %s (y/n): ", (desc), (question)); \
        fflush(stdout); \
        { char _c = (char)getchar(); while (getchar() != '\n'); \
          if (_c == 'y' || _c == 'Y') printf("  [PASS]  %s\n", (desc)); \
          else { printf("  [FAIL]  %s\n", (desc)); g_uc_fails++; } } \
    } while (0)
#else
#define TEST_MANUAL(desc, question) \
    printf("  [SKIP] %s (run make manual)\n", (desc))
#endif
```

Convention : remplacer les `TEST_SKIP("[S] ...")` existants qui ont une question visuelle claire par `TEST_MANUAL(desc, question)` lors de l'implémentation réelle du module concerné. Le bloc matriciel d'en-tête devient `[S] Z tests - run make manual`.

**R17 — Traçabilité REQ ↔ fonction dans SRTD.md §4.x** *(établie UC3.1, 2026-05-30)*

Chaque fonction publique (et chaque fonction interne non-triviale) d'un module doit être associée à au moins un REQ dans la table **Public API** / **Key internal functions** de la section §4.x correspondante de SRTD.md.

Format de la table enrichie :

```
| Function          | REQ(s)                   | Description           |
|-------------------|--------------------------|-----------------------|
| trace_init(bOpen) | REQ-TRC-001, REQ-TRC-002 | Init log file…        |
| format_entry(…)   | REQ-TRC-015              | Build entry format    |
```

Règles :
- **Avant d'implémenter** une nouvelle fonction publique : vérifier qu'un REQ existe. Sinon, créer le REQ dans §2.x avant d'écrire le code.
- **Lors de la mise à jour** d'un §4.x : toujours remplir la colonne REQ pour toutes les fonctions du module.
- **Fonctions internes** < 5 lignes sans effet de bord documentable (helpers purs) : REQ optionnel, marquer `—` si non applicable.
- La colonne REQ est la source de vérité pour la revue de couverture : toute fonction sans REQ est un gap à combler.

Sens de la traçabilité complète : `UFR → REQ → §4.x (table fonctions) → .c (implémentation)`

**R18 — Titre de fenêtre dynamique pour chaque vue GUI** *(établie UC3.1, planifiée UC3.3, 2026-05-30)*

Chaque vue GUI met à jour sa barre de titre en fonction du contexte courant via `SetWindowTextA` (Win32) / `XStoreName` (X11). Format : `ST4Ever - <Type>: <contexte>`.

Exemples :
- `ST4Ever - Dir: C:\demos\atari\`
- `ST4Ever - Edit: ENCHANT.PRG [hex]`
- `ST4Ever - Execute: ENCHANT.PRG — step 42`
- `ST4Ever - Trace`

Règles :
- Le titre est mis à jour dans le handler `GUI_EVT_PAINT` (ou à tout changement de contexte significatif) via l'API publique de la vue concernée.
- La mise en œuvre est à la charge de chaque UC de vue (UC3.3 dir, UC8 edit, UC25 execute…).
- Sur Windows : `SetWindowTextA(ptState->hWnd, szTitle)` depuis `gui_platform_window_set_title()` à ajouter dans `gui_backend.h`.

**R19 — Flow en 2 phases par UC : code/tests puis documentation/traçabilité** *(établie UC3.3, 2026-05-31)*

Chaque UC suit deux phases distinctes avant clôture :

- **Phase 1 — Implémentation** : modifications sources/includes/Makefile, `make tests` **0 warning, 0 failure**, mise à jour CLAUDE.md (§0 historique, §1.1 si commande validée, §4 pratiques, §5 recommandations, §6 tableau + sous-section §6.x UC validé, `ST_APP_VERSION`). Claude rend la main à Tonton Marcel pour relecture, passage des tests manuels (`make manual`) et validation fonctionnelle.
- **Phase 2 — Documentation** :
  - Mise à jour traçabilité dans `use_cases_XX.c` (INTENT, ADAPTED, TEST MATRIX).
  - Mise à jour SRTD.md (UFRs, REQs, Tables fonctions, Tests, Matrices, Open Items) en conséquence des nouveaux contrats fonctionnels de CLAUDE.md §6.x et des modifications des `use_cases_XX.c`.
  - **Revue §7** : analyser chaque proposition non arbitrée du UC courant — donner l'avis Claude (ACCEPTÉ / REFUSÉ / MODIFIÉ + justification) et planifier dans le tableau §6 ou clore. Déposer toute nouvelle proposition UX/fonctionnelle émergente du UC dans §7 (avec numéro P séquentiel) avant de demander l'arbitrage de Tonton Marcel.

Règle : **un UC n'est clos que lorsque les deux phases sont complètes** et §7 ne contient plus de propositions non arbitrées pour ce UC. Claude initie la Phase 2 sans demande explicite dès que Phase 1 est validée par Tonton Marcel.

**R20 — Mise à jour de `ST_VERSION` à chaque UCx.y** *(établie UC3.3, 2026-05-31)*

`ST_VERSION` dans `common.h` est mis à jour à chaque clôture d'UC. Format : `"MAJOR.MINOR.PATCH"` avec :
- `MAJOR` = 0 (pas encore de release publique)
- `MINOR` = numéro UC principal (ex : 3 pour UC3.x, 4 pour UC4)
- `PATCH` = numéro sous-UC (ex : 3 pour UC3.3, 0 pour UC4)

Exemples : `"0.3.3"` après UC3.3, `"0.4.0"` après UC4. La mise à jour version fait partie du checklist de clôture Phase 2 (R19). Claude ne clôt pas un UC sans avoir mis à jour `ST_VERSION`.

**R21 — Stratégie raw-mode Windows : détection runtime pipe/console** *(établie UC4.2, 2026-06-01)*

MINGW64 (le compilateur du projet) ne fournit pas `termios.h`. La stratégie "termios-first" de R5 est remplacée par une détection runtime via `GetFileType(GetStdHandle(STD_INPUT_HANDLE))` dans `win/win_console.c` :

- `FILE_TYPE_PIPE` (mintty/MSYS2) : stdin est un pipe anonyme Windows. Mintty délivre les octets un par un au fil de la frappe sans buffering ligne côté OS — aucune configuration raw-mode n'est requise. Lecture via `ReadFile()` ; timeout ESC via `PeekNamedPipe()` en polling 5 ms / 50 ms max ; décodage VT100 dans `console_decode_csi()`.
- `FILE_TYPE_CHAR` (cmd.exe) : `SetConsoleMode` sans `ENABLE_PROCESSED_INPUT` / `LINE_INPUT` / `ECHO_INPUT` + `ReadConsoleInputA()` (VK codes pré-décodés, pas de parsing VT100).

Sur Linux, `termios` reste la stratégie unique (dans `linux/lx_console.c`). Cette règle s'applique à tout futur code d'entrée console Windows.

**R22 — Politique LOG_TRACE : cantonner aux fonctions UX et cycle de vie** *(établie UC4.4, 2026-06-01)*

`LOG_TRACE` n'est pertinent que pour répondre à *"quelle action utilisateur a déclenché ce chemin ?"*. Il est contre-productif dans les fonctions appelées en boucle par le moteur de rendu ou les primitives de synchronisation.

**Fonctions sans LOG_TRACE (primitives récurrentes) :**

| Couche | Fonctions purgées |
|---|---|
| `win/win_platform.c` | `platform_mutex_lock/unlock/create/destroy` |
| `src/renderer.c` | `renderer_begin/end_draw`, `renderer_fill/draw_rect`, `renderer_draw_line/text/bitmap`, `renderer_get_font_metrics` |
| `win/win_D2D.c` | `renderer_platform_begin/end_draw`, `renderer_platform_fill/draw_rect`, `renderer_platform_draw_line/text`, `renderer_platform_get_font_metrics` |
| `src/gui.c` | `gui_invalidate`, `gui_get_size`, `gui_msg_post`, `gui_msg_get` |
| `win/win_gui.c` | `gui_platform_window_invalidate/get_size/get_native_handle/set_title` |

**Fonctions conservant LOG_TRACE (cycle de vie + UX) :**
- Cycle de vie modules : `trace_init/open/close/shutdown`, `gui_init/shutdown`, `renderer_create/resize/destroy`
- Cycle de vie fenêtres : `gui_open/close_window`, `gui_request_close`, `gui_platform_window_create/close`, `gui_platform_init/shutdown`
- Dispatch et navigation : tous les `line_cmd_*`, `dir_handle_key/click`, `dir_open/close`, `dir_navigate_up`, `dir_activate_sel`
- Threads : `platform_thread_create/join/destroy`
- Infrastructure partagée : `gui_msg_create/destroy`, `gui_set_title`

**Règle d'application pour les UCs futurs** : avant d'ajouter `LOG_TRACE` dans une nouvelle fonction, vérifier si elle peut être appelée plus d'une fois par action utilisateur (paint loop, mutex, query) — si oui, omettre le LOG_TRACE.


## 6. Use Cases

Les étapes de développement fonctionnelles sont formalisées en Use Cases, permettant de développer et valider chaque cas d'usage de l'application et d'enrichir le projet avec de plus amples détails, dont les recommendations Claude AI de la section 4, et planifier le reste des Use Cases en TODO/stubs dans le projet. La liste actuelle des Use Cases est:

| # | Commande(s) | Description | Valide |
|---|---|---|---|
| UC1 | `help`, `quit`, `trace -t` | Prototype complet, stubs, console + trace fonctionnels | ✓ VALIDÉ 2026-05-30 |
| UC2 | `trace on/off/toggle` | Gestion complète de la trace console | ✓ VALIDÉ 2026-05-30 |
| UC3.1 | GUI infra | msg_queue (portable) + win_gui.c window/thread/WndProc + gui_platform_* | ✓ VALIDÉ 2026-05-30 |
| UC3.2 | GUI render | win_D2D.c (Direct2D COM pur C) + renderer.c portable | ✓ VALIDÉ 2026-05-30 |
| UC3.3 | `dir` | dir.c : scan FS + arbre + rendu D2D + clavier/souris/scroll/sélection + line_cmd_dir | ✓ VALIDÉ 2026-05-31 |
| UC4.1 | UX dir + make manual | `gui_request_close()` non-bloquant + ESC/←/→/ESPACE séparés de ENTER + filtre `.*` + `dir -a` + focus auto + `TEST_MANUAL` + `make manual` | ✓ VALIDÉ 2026-05-31 |
| UC4.2 | raw input | `console.h` (CON_KEY_*) + pipe/VT100 mintty (R21) + Win32 cmd.exe + `line_read_rich()` : insert/←/→/Home/End/Del/Backspace, CTRL shortcuts, ESC + `make manual UC=XX` | ✓ VALIDÉ 2026-06-01 |
| UC4.3 | complétion + historique + extras | Historique ↑↓ circulaire + `~/.st4ever_history` + tab-completion commandes/chemins + ghost-text DIM + prompt contextuel `[T][file]` + `colors on/off` + `--script file` + mutex `ptSelectedMutex` + `line_set/get_selected()` | ✓ VALIDÉ 2026-06-01 |
| UC4.4 | trace view GUI | Fenêtre `GUI_WND_TRACE` D2D : scroll texte append-only, couleurs par niveau ; `trace.c` ring buffer + mutex + garde réentrante ; suppression TODO(UC3)/TODO(UC3.3) | ✓ VALIDÉ 2026-06-01 |
| UC5 | `where`, `info`, `history` | Répertoire courant + état sélection (where) ; dashboard global état application : cwd, fichier sélectionné, trace, disque monté, binaire chargé (info) ; **P8** : `SetConsoleTitleA` statut automatique ; **P21** : touche `H` toggle hidden dans vue dir ; **P22** : F5 refresh vue dir ; **P23bis** : TAB préfixe commun avant cycle ghost ; **P24** : `colors auto` via `isatty()` au démarrage ; **P25** : commande `history [N]` ; **P27** : `trace clear` ; **P28** : `trace level trace\|info\|error` filtre vue | ✓ VALIDÉ 2026-06-01 |
| UC5-bis | prefs | Module `prefs.c` : lecture/écriture `%APPDATA%\ST4Ever\prefs.ini` ; mémorisation position/taille fenêtres par type (P7) — **différé après UC10** : les types de vues (edit hex, edit txt, disassembly) doivent être stables avant de persister leurs positions | save/restore position fenêtre dir/edit |
| UC6 | plateforme | Abstraction fichiers : open/read/write/stat/mkdir, listing répertoire | ✓ VALIDÉ 2026-06-01 |
| UC7 | `load` | Chargement fichier texte/binaire, détection type, buffer mémoire ; indicateur visuel sélection active dans vue `dir` (P11 : couleur secondaire sur ligne sélectionnée, ≠ highlight navigation) | ✓ VALIDÉ 2026-06-01 |
| UC8 | `edit` texte | Vue éditeur texte D2D : scroll, numéros de ligne, sélection, clipboard, sauvegarde ; `GUI_MOD_*` + `uiMods` dans `gui_event_t` ; API clipboard `gui_clipboard_set/get_text` | ✓ VALIDÉ 2026-06-02 |
| UC9 | `edit` hex | Vue hex+ASCII D2D : adresses, 16 bytes/row, zones HEX/ASCII (TAB), édition nibble+byte, clic souris, CTRL+S save | ✓ VALIDÉ 2026-06-02 |
| UC10 | `edit` | Vue intégrée hex+ASCII+désasm en colonnes synchronisées | ✓ VALIDÉ 2026-06-02 |
| UC11 | interne | Désassembleur 68000 : MOVE/MOVEQ/LEA/CLR/EXG/SWAP/PEA | ✓ VALIDÉ 2026-06-03 |
| UC12 | interne | Désassembleur : ADD/SUB/CMP/MUL/DIV/AND/OR/EOR/NOT/NEG | ✓ VALIDÉ 2026-06-03 |
| UC13 | interne | Désassembleur : shifts, rotations, BTST/BSET/BCLR/BCHG | ✓ VALIDÉ 2026-06-03 |
| UC14 | interne | Désassembleur : BRA/BSR/Bcc/JMP/JSR/RTS/RTR/RTE/TRAP | ✓ VALIDÉ 2026-06-03 |
| UC15 | interne | Format PRG : parser header + sections + fixups + chargement mémoire ST | ✓ VALIDÉ 2026-06-03 |
| UC16 | interne | Image `.st` : lecture/écriture raw + FAT12 | ✓ VALIDÉ 2026-06-04 |
| UC15A | interne | Torture test désassembleur : SOURCE.PRG (DEVPAC3, ATARI ST réel) vs disasm_range() — 2525 instructions, DIFF=0 ; 5 bugs disasm corrigés | ✓ VALIDÉ 2026-06-06 |
| UC17 | interne | Image `.msa` : décompression/compression RLE | ✓ VALIDÉ 2026-06-06 |
| UC18.1 | `mount`, `umount` | Vue D2D `GUI_WND_MOUNT` : liste FAT12 + panneau propriétés ; `mount_view_open/close/add_file` ; `line_cmd_mount/umount` ; `gui_find_window_by_type` | ✓ VALIDÉ 2026-06-06 |
| UC18.2 | `mount`, `dir` | P10 historique navigation dir (ALT+←/→) ; P14 multi-select CTRL+ESPACE (violet) ; P34 BPB géométrie lecture seule ; P36 suppression /112 en-tête ; P37 Bootable WD1772 ; P38 touche B → bootsector hex | ✓ VALIDÉ 2026-06-06 |
| UC19 | `umount` | Démontage + dialog save .st/.msa/répertoire (P35) ; status bar mount (P39) ; progression copie (P40) | dialog save |
| UC20 | `image` | Création .st / .msa depuis le contenu monté ; Enter → hex dans mount (P41) | image valide et montable |
| UC21 | interne | CPU 68000 : registres + MOVE/MOVEQ/LEA/CLR/SWAP | step 10 instructions |
| UC22 | interne | CPU 68000 : ADD/SUB/CMP/AND/OR/EOR/shifts | exécution programme arithmétique |
| UC23 | interne | CPU 68000 : BRA/Bcc/JSR/RTS/TRAP + vecteurs exception | appel/retour de fonction |
| UC24 | interne | Memory map ST + registres HW stubs (Shifter, MFP, YM2149) | accès registres sans crash |
| UC25 | `execute` | Moteur pas-à-pas + vues CPU + mémoire | step + breakpoint sur .PRG simple |
| UC26 | interne | Émulation vidéo ST (Shifter : low/med/high res, palette 16 couleurs) | rendu écran correct |
| UC27 | `execute` | Vue écran Win32/GDI + X11 + synchronisation VBL | démo statique visible |
| UC28 | interne | Line-A traps + registres Shifter/YM2149 minimaux | démo 1 plan visible |
| UC29 | interne | XBIOS/GEMDOS minimaux (palette, écran base, VBL wait) | démo dynamique |
| UC30 | interne | Assembleur DEVPAC3 : directives + instructions de base | .S → .PRG re-exécutable |
| UC31 | all | Démo cible via émulateur ST4Ever (choisie parmi candidats §9.4) : référence visuelle validée + désassemblé complet extrait — **pivot émulation → revival** | démo reconnue + .s extrait |
| UC32 | interne | Analyse annotée du désassemblé : patterns automatiques (VBL, accès HW $FFxxxx, GEMDOS/XBIOS) + session collaborative manuelle — livrable : `use_cases/UC32/demo.s` commenté | désassemblé .s annoté clos par Tonton Marcel |
| UC33 | interne | Squelette C portable : flot de contrôle + structures de données + HAL stubs (`hal_vbl_wait`, `hal_palette_set`, `hal_ym_write`…) — compile sur PC avec stubs no-op | squelette C compile, aucun crash |
| UC34 | interne | Toolchain cross `arm-none-eabi-gcc` + harness test PC (backend `pi0_pc/` dans Makefile ST4Ever) | démo C tourne sur PC via harness D2D/X11 |
| UC35 | interne | Pi Zero bare metal OS minimal : vecteurs ARM, stack, BSS, UART debug, timer 50 Hz, framebuffer GPU mailbox BCM2835 | écran couleur uni synchronisé VBL sur Pi Zero réel |
| UC36 | interne | Renderer Pi Zero : framebuffer RGB, modes basse/moyenne résolution ST simulés, palette 16 couleurs, sync VBL | rendu visuel HAL vidéo fonctionnel |
| UC37 | interne | Son + inputs bare metal : YM2149 via émulateur AY-3 soft (ARM) + sortie PWM BCM2835, clavier/joystick GPIO ou USB HID minimal | son reconnaissable + inputs actifs |
| UC38 | interne | Intégration complète démo cible sur Pi Zero : code C UC33 + HAL UC35–37 + toolchain UC34 | démo comportement reconnaissable sur Pi Zero bare metal |
| UC39 | interne | Backend PC natif D2D/X11 : nouvelle plateforme dans Makefile ST4Ever, réutilise HAL validée sur Pi Zero | démo native sur PC sans émulateur 68k — **objectif revival** |

### 6.1 Use Case 01 (UC1) — VALIDÉ (2026-05-30)

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
- UC2 : validé — `line_cmd_trace()` complète : toggle, on, off, unknown arg, extra args warning
- UC3 : `gui_init` réel ouvre une fenêtre Win32 → les tests UC1 qui appellent des stubs GUI devront utiliser `#ifdef ST_TEST_LEVEL_UC01` + `TEST_SKIP` (mécanisme R14 en place)
- UC21 : `cpu_step` réel changera le comportement des tests UC1 step — assertions marquées `ADAPTED`
- UC24 : `st_read_byte` hors-RAM devra lever ST_ERROR pour les zones vraiment non mappées — test marqué `ADAPTED`

### 6.2 Use Case 02 (UC2) — VALIDÉ (2026-05-30)

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
- UC3 : `gui_init` réel ouvre une fenêtre Win32 → les tests UC1/UC2 qui appellent des stubs GUI devront utiliser `#ifdef ST_TEST_LEVEL_UCxx` + `TEST_SKIP` (mécanisme R14 en place)
- UC4 : le dispatch stdin complet permettra d'automatiser les 4 tests [S] actuellement manuels

### 6.3 Use Case 03.1 (UC3.1) — VALIDÉ (2026-05-30)

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
- UC3.2 : WM_PAINT actuel fait `FillRect` GDI → sera remplacé par `renderer_begin/end_draw` (D2D)
- UC3.2 : `win_D2D.c` doit implémenter `renderer_create()` prenant un `gui_window_t` et lisant le HWND depuis `ptWnd->pPlatform` (cast `win_wnd_state_t *`)
- UC3.3 : `dir.c` utilisera `gui_platform_window_invalidate()` pour forcer le repaint après navigation
- UC4 : le spin-wait 1ms de `gui_msg_get(bBlock=TRUE)` sera remplacé par une variable de condition ou un Win32 Event dans la queue
- UC4 : les 3 tests [S] (fenêtre visible) seront convertis en `TEST_MANUAL`
- UC3.3 (fin) : compléter SRTD.md §4 avec les sections détaillées GUI — §4.8 `gui.c`/`gui_backend.h`, §4.9 `win_gui.c`/`win_D2D.c`, §4.10 `dir.c` — sur le modèle de §4.2–§4.7 (rôle, API, dépendances, séquence). Différé car la GUI est construite en 3 sous-UCs ; un §4.x complet après UC3.3 sera plus cohérent.

### 6.4 Use Case 03.2 (UC3.2) — VALIDÉ (2026-05-30)

*(Résumé : win_D2D.c Direct2D COM pur C + renderer.c portable. Voir commit history pour détails.)*

**Périmètre :** `win/win_D2D.c` (renderer D2D complet : create, resize, begin/end_draw, fill_rect, draw_rect, draw_line, draw_text, draw_bitmap, destroy) + `src/renderer.c` (couche portable + NULL-guards). `gui_platform_get_native_handle()` et `gui_platform_window_set_title()` ajoutés à `win_gui.c`.

**Tests :** `use_case_03_2.c` — **7N + 10R + 5S = 22 tests**, 0 failure.

---

### 6.5 Use Case 03.3 (UC3.3) — VALIDÉ (2026-05-31)

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
- UC4 : écriture `ptLineCtx->szSelected` depuis le thread fenêtre sans verrou → UC4 ajoutera un mutex sur `line_context_t`
- UC4 : spin-wait 1ms dans `gui_msg_get(bBlock=TRUE)` → remplacer par Win32 Event/variable de condition
- UC4 : les 4 tests [S] seront convertis en `TEST_MANUAL` avec `make manual`
- UC7/UC18 : menu contextuel (clic droit sur fichier/répertoire) différé à ces UCs
- SRTD.md §4.8–§4.10 (`gui.c`, `win_gui.c`/`win_D2D.c`, `dir.c`) à rédiger en fin d'UC3 complet

### 6.6 Use Case 04.1 (UC4.1) — VALIDÉ (2026-05-31)

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
- UC4.2 : `win_console_set_raw()` + `win_console_read_key()` — stratégie termios-first (MSYS2/mintty), fallback Win32 ReadConsoleInput (cmd.exe). `console_key_t` à définir dans `src/line.h` ou `src/common.h` en réutilisant `gui_key_t`
- UC4.2 : `line_read_rich()` remplace `fgets()` dans `line_run()` — affichage temps réel du buffer avec `\r\033[2K` (effacer ligne + réafficher)
- UC4.3 : mutex sur `line_context_t.szSelected` (write depuis thread fenêtre, note déjà dans UC3.3) — à ajouter avec le module d'historique
- SRTD.md §4.8–§4.10 (`gui.c`, `win_gui.c`/`win_D2D.c`, `dir.c`) à rédiger en Phase 2 complète (UC3+UC4.1 ensemble, différé par accord)

### 6.7 Use Case 04.2 (UC4.2) — VALIDÉ (2026-06-01)

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
- UC4.4 : `console_restore()` doit être appelé avant l'affichage initial de la fenêtre trace GUI pour éviter interférence entre mode raw et stdout.

### 6.8 Use Case 04.3 (UC4.3) — VALIDÉ (2026-06-01)

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
- UC5 : `line_cmd_where()` pourra utiliser `line_get_selected()` pour afficher le fichier sélectionné courant.
- UC5 : P21 (touche `H` toggle hidden dans vue dir) et P22 (F5 refresh) touchent `dir_handle_key()` — même scope que UC5 `where`/`info`.
- UC18 : P10 (historique navigation dir ALT+←/→) + P14 (sélection multiple CTRL+ESPACE) toujours en attente de `mount` context.

### 6.9 Use Case 04.4 (UC4.4) — ✓ VALIDÉ (2026-06-01)

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
- UC4.4 Phase 2 (documentation SRTD.md) : à compléter après validation manuelle complète.
- UC5 : `console_restore()` doit être appelé avant l'ouverture de la fenêtre trace GUI si trace est ouvert en mode raw.
- UC5 : P21/P22 touchent `dir_handle_key()` — même scope que UC5.
- UC18 : P10/P14 toujours en attente de `mount` context.

### 6.10 Use Case 05 (UC5) — ✓ VALIDÉ (2026-06-01)

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
- UC6 : abstraction fichiers — `line_cmd_where()` pourra enrichir son affichage avec des infos stat sur la sélection.
- UC7 : `line_cmd_info()` rempira les stubs "disk mounted" et "binary loaded" au fur et à mesure des UCs.
- UC18 : P10 (historique navigation dir) + P14 (sélection multiple) toujours en attente de `mount` context.

### 6.11 Use Case 06 (UC6) — ✓ VALIDÉ (2026-06-01)

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
- UC8-10 : `edit` utilisera `file_open(FILE_MODE_READ)` + `file_open(FILE_MODE_WRITE)` pour chargement et sauvegarde.
- UC16 : image `.st` = `file_open(READ)` + `file_read` de 737 280 octets exact.
- UC18 : `file_list_dir` alimentera la vue mount (listing du répertoire monté).
- `file_stat` n'est pas récursif : pas d'équivalent `stat64` nécessaire pour l'instant (fichiers PRG/ST restent < 4 Go).

### 6.12 Use Case 07 (UC7) — ✓ VALIDÉ (2026-06-01)

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
- UC8-10 : `edit` utilisera `load_get_state()` pour savoir si un fichier est déjà chargé et proposer de l'éditer directement en mémoire émulée.
- ~~UC15 : `load_do_prg()` TODO(UC15)~~ **✓ IMPLÉMENTÉ UC15** — fixup relocation complet.
- UC24 : `ST_LOAD_BASE = 0x0800` est en dur ; UC24 devra allouer la première zone libre au-dessus des variables TOS selon la memory map réelle.
- UC25 : `execute` démarrera depuis `load_get_state()->uiLoadAddr` comme PC initial (si eType == LOAD_TYPE_PRG).
- `line_cmd_info()` stub "Disk mounted" reste à remplir lors de UC18 (`mount`).

### 6.13 Use Case 08 (UC8) — ✓ VALIDÉ (2026-06-02)

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
- UC9 : `edit_hex_open(szPath, ptLineCtx, pptView)` — même architecture `GUI_WND_EDIT_HEX` ; `line_cmd_edit()` redirigera les `.prg/.ttp/.tos/.st/.msa` vers `edit_hex_open()`.
- UC10 : `edit.c` dispatcher intégré (hex+ASCII+désasm synchronisés) — `edit_open()` choisit entre `edit_txt` et `edit_hex`.
- UC18 (mount) : `GUI_MOD_SHIFT` est désormais transmis par `win_gui.c` → `dir_handle_key()` peut exploiter SHIFT+ESPACE pour la sélection multiple P14 sans modification du backend.
- Encodage : `WM_CHAR` limité au code page ANSI courant (0xFF mask) — suffisant pour les sources ATARI ST ASCII. Support UTF-8 différé.

### 6.14 Use Case 09 (UC9) — ✓ VALIDÉ (2026-06-02)

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
- UC10 : `edit.c` dispatcher intégré — `edit_open()` choisit entre `edit_txt` et `edit_hex` selon l'extension ; les vues hex+ASCII et disassembly 68000 seront synchronisées (curseur partagé).
- UC11–14 : désassembleur 68000 → vue disassembly dans UC10 (overlay sur la zone hex des sections `.text` d'un PRG).
- UC15 : `load_do_prg()` fixup relocation → permet de charger le PRG correctement relocalisé en RAM ST avant `edit_hex`.
- Undo (CTRL+Z) pour le hex editor : non implémenté en UC9 (mode remplacement, fichiers petits → l'utilisateur peut re-ouvrir). À planifier si besoin en UC9-bis.

### 6.15 Use Case 10 (UC10) — ✓ VALIDÉ (2026-06-02)

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
- UC11–14 : le désassembleur réel remplacera les DC.W stubs. `ehex_disasm_cache_update()` appellera toujours `disasm_range()` sans modification — le cache sera automatiquement valide. La seule différence : `iWordCount` peut être > 1, donc `iDisasmCursorIdx` ne sera plus triviallement `uiCursor / 2`. `ehex_disasm_find_cursor()` gère déjà ce cas via la comparaison `uiAddr ≤ uiCursor < uiAddr_next`.
- UC15 : fixup PRG → les adresses dans le disasm panel seront correctes par rapport à la RAM ST (pas juste des offsets dans le fichier). `uiAddr` dans `disasm_result_t` sera l'adresse ST réelle.
- UC18 (mount) : disasm des secteurs boot d'une image `.st` — le secteur 0 est potentiellement un programme 68000. La sélection de plage (`szDisasmRange` UI) peut être un UC futur.
- P32 (SHIFT+flèches sélection plage) : maintenant que DISASM zone existe, P32 peut cibler la vue hex pour surligner les octets correspondant à une instruction disasm sélectionnée — à arbitrer.
- P33 (`edit hex` forcer hex) : applicable à UC10 — une commande `edit hex <file>` forcerait l'ouverture hex même pour les fichiers non-binaires.

### 6.16 Use Case 11 (UC11) — ✓ VALIDÉ (2026-06-03)

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
- UC10 : le panneau disasm affiche maintenant les vraies mnémoniques pour MOVE/MOVEQ/LEA/CLR/EXG/SWAP/PEA — sans modification de `edit_hex.c` (appelle `disasm_range()` déjà).
- UC12 : groupe 0100 contient aussi NOT/NEG/TST/NBCD/EXT — décodage complémentaire dans `disasm_misc4()`.
- UC12 : groupe 0110 (ADD/SUB immediat) + groupe 1000 (OR/DIVU) + 1001 (SUB/SUBX) + 1011 (CMP/EOR) + 1100 (AND/MULU) + 1101 (ADD/ADDX).
- UC13 : groupe 1110 (shifts/rotations) + BTST/BSET/BCLR/BCHG (groupe 0000 bits 7-6 ≠ 11).
- UC14 : groupe 0100 (`4E75` RTS, `4E74` RTD, `4E73` RTE, `4EF9` JMP) + `6xxx` (Bcc/BRA/BSR) + `4E80-4EBF` (JSR).
- `TC-DIS-001` (ADAPTED UC11) : reste valide — `bValid = ST_FALSE` pour 0x4E75 (RTS, non décodé jusqu'à UC14). A mettre à jour en ADAPTED(UC14) dans use_case_01.c en UC14.

### 6.17 Use Case 12 (UC12) — ✓ VALIDÉ (2026-06-03)

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

### 6.18 Use Case 13 (UC13) — ✓ VALIDÉ (2026-06-03)

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
- UC14 : groupe 0x4 restant : RTS (0x4E75), NOP (0x4E71), JMP (0x4EF9), JSR (0x4E80-0x4EBF), TRAP #N (0x4E40-0x4E4F), RTR (0x4E77), RTE (0x4E73). Groupe 0x6 : BRA/BSR/Bcc (déplacement 8-bit dans octet bas ou 16-bit extension). Après UC14, le désassembleur est complet pour la cible démo ST (UC31).
- MOVEP : `0000 DDD 1 MM 001 RRR` (bits 5-3 = 001 = An mode dans EA). Actuellement → DC.W via le rejet An mode dans `disasm_bit_dynamic`. Comportement correct pour UC13 ; à implémenter si nécessaire avant UC31.
- `TC-DIS-001` (ADAPTED UC11) : RTS (0x4E75) reste DC.W jusqu'à UC14. Marqueur ADAPTED(UC14) à ajouter dans `use_case_01.c` lors de UC14.

### 6.19 Use Case 14 (UC14) — ✓ VALIDÉ (2026-06-03)

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
- UC21 : le CPU 68000 réel décodera les instructions via un mécanisme similaire au désassembleur mais avec exécution effective des effets (registres, flags SR).

### 6.20 Use Case 15 (UC15) — ✓ VALIDÉ (2026-06-03)

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
- UC16 : image `.st` = `file_open(READ)` + `file_read` de 737 280 octets exact — le même pipeline `file.h` que UC15.
- UC24 : `ST_LOAD_BASE = 0x0800` est en dur ; UC24 allouera la première zone libre au-dessus des variables TOS selon la memory map réelle.
- UC25 : `execute` démarrera depuis `load_get_state()->uiLoadAddr` comme PC initial quand `eType == LOAD_TYPE_PRG`. Les adresses de fixup sont déjà correctes en RAM ST.
- UC10/disasm panel : avec les fixups appliqués, les adresses affichées dans la vue hex+disasm correspondent aux vraies adresses ST (pas aux offsets fichier) — comportement attendu vérifié.

### 6.21 Use Case 16 (UC16) — ✓ VALIDÉ (2026-06-04)

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
- UC17 : `.msa` = `.st` compressé run-length ; la même structure `image_st_t` peut être réutilisée après décompression MSA → aDisk. UC17 ajoute `image_msa_load()` et `image_msa_save()` qui opèrent sur un `image_st_t` existant.
- UC18 (`mount`) : `mount_open()` appellera `image_st_load()` pour les fichiers `.st`. La vue GUI listera les entrées via `image_st_list_root()`. Le drag & drop écrira via `image_st_write_file()`.
- UC20 (`image`) : `image_st_save()` est déjà disponible — UC20 crée l'image depuis le contenu d'un répertoire PC monté via UC18.
- Limite de sous-répertoires : seule la racine est supportée (suffisant pour UC18/UC20). Les `.st` ATARI ST réels ont rarement des sous-répertoires dans les démos.

---

### Arbitrage UC16 (2026-06-04)

*UC16 est un UC purement interne (FAT12 .st image). Aucune proposition UX/fonctionnelle n'a émergé — UC16 est clos.*

---

### 6.22 Use Case 15A (UC15A) — ✓ VALIDÉ (2026-06-06)

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
- UC21 : MOVEM, DBcc, Scc, MOVEP (~175 DCW restants) — décodage prévu lors de l'implémentation CPU 68000
- UC24 : MOVE.W SR/CCR, TAS, ILLEGAL, RESET, STOP (~95 DCW restants) — nécessitent le contexte mode superviseur
- `DISASM_SYNTAX.md` à enrichir à chaque nouveau groupe d'instructions décodé

---

### Arbitrage UC15A (2026-06-06)

*UC15A est un UC de validation interne (torture test désassembleur). Aucune proposition UX/fonctionnelle n'a émergé — UC15A est clos.*

---

### 6.23 Use Case 17 (UC17) — ✓ VALIDÉ (2026-06-06)

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
- UC18 (`mount`) : `image_msa_load()` est désormais disponible — `line_cmd_mount()` peut accepter les `.msa` comme les `.st`. Même interface `image_st_t` pour les deux formats.
- UC20 (`image`) : `image_msa_save()` permettra de créer un `.msa` depuis un contenu monté, en plus du `.st` existant.
- Extension `.stx` (UC future optionnelle) : serait un 3ème codec sur la même base `image_st_t`. L'architecture `image_st_get_disk()` permet d'en ajouter sans modifier `image_st.c`.

---

### Arbitrage UC17 (2026-06-06)

*UC17 est un UC purement interne (codec MSA RLE). Aucune proposition UX/fonctionnelle n'a émergé — UC17 est clos.*

---

### 6.24 Use Case 18.1 (UC18.1) — ✓ VALIDÉ (2026-06-06)

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
- UC18.2 : drag-and-drop depuis vue `dir` → `mount_view_add_file()` (infrastructure prête) ; multi-sélection CTRL+ESPACE (P14) dans `dir_handle_key()` ; historique navigation ALT+←/→ (P10).
- UC19 : `umount` + dialog "save image?" si `bDirty == ST_TRUE`. `line_cmd_umount()` actuel log warning mais ne bloque pas.
- UC20 : `image_st_save()` et `image_msa_save()` disponibles depuis UC16/UC17 — `image` command créera une image depuis le contenu monté.
- `gui_find_window_by_type` : ajouté à `gui.h`/`gui.c` — disponible pour toutes les vues futures (edit_txt, edit_hex, dir) qui veulent vérifier si une vue de même type est déjà ouverte.

---

## 7. Propositions d'améliorations

*Claude et Tonton Marcel déposent ici leurs propositions UX/fonctionnelles au fil des UCs. Avant de clore un UC, les propositions sont passées en revue ensemble : celles agréées sont planifiées dans le tableau section 6 et retirées d'ici. **Un UC est clos quand §7 ne contient plus de propositions non arbitrées pour cet UC.***

*Les propositions P1–P5 issues de UC1/UC2 ont été agréées et planifiées (UC4/UC5). Les propositions P6–P8 issues de UC3.1 ont été arbitrées ci-dessous — UC3.1 est donc clos.*

---

### Arbitrage UC1/UC2 (2026-05-30) — *reconstitué depuis UC4/UC5, original non persisté en git*

**P1 — Prompt contextuel toggleable** → **ACCEPTÉ — UC4**

Le prompt affiche l'état courant (trace active, disque monté, fichier sélectionné) en plus du simple `ST4Ever>`. Toggleable pour ne pas encombrer l'affichage.

**P2 — Commande `colors on/off`** → **ACCEPTÉ — UC4**

Activer ou désactiver les couleurs ANSI de la console (utile pour les terminaux sans support VT100 ou pour la redirection vers fichier).

**P3 — Historique persistant `~/.st4ever_history`** → **ACCEPTÉ — UC4**

L'historique des commandes (navigable ↑↓) est sauvegardé entre sessions dans un fichier, comme un shell Unix standard.

**P4 — Mode batch `--script file`** → **ACCEPTÉ — UC4**

Lancer ST4Ever avec un fichier de commandes à exécuter séquentiellement, sans interaction utilisateur. Utile pour les scénarios de test répétables.

**P5 — `make manual` et macro `TEST_MANUAL`** → **ACCEPTÉ — UC4 (R16)**

Séparer les tests automatiques (`make tests`) des tests visuels (`make manual`). La macro `TEST_MANUAL(desc, question)` affiche une question y/n à l'utilisateur pour valider les comportements non automatisables (GUI, rendu). Formalisé en R16.

---

### Arbitrage UC3.1 (2026-05-30)

**P6 — Titre de fenêtre dynamique** → **ACCEPTÉ — R18 + UC3.3**

Chaque vue GUI affiche dans sa barre de titre le contexte courant (`ST4Ever - Dir: C:\demos\`, `ST4Ever - Edit: ENCHANT.PRG [hex]`…). Convention posée en R18 §5, appliquée dès UC3.3 et systématisée dans chaque UC de vue suivant.

**P7 — Mémorisation position/taille des fenêtres** → **ACCEPTÉ — UC5-bis (différé après UC10)**

Architecture conservée : console terminal + vues GUI (voir décision D1 ci-dessous). Module `prefs.c` (fichier INI `%APPDATA%\ST4Ever\prefs.ini`) différé après UC10 : les types de vues edit (hex, txt, disassembly) doivent être stables avant que leurs positions/tailles soient pertinentes à persister. Implémenter `prefs.c` sur une fenêtre `dir` seule serait prématuré — l'API doit couvrir d'emblée tous les types `GUI_WND_*`.

**P8 — Indicateur d'état dans la barre de titre de la console** → **ACCEPTÉ — UC5**

`SetConsoleTitleA` mis à jour à chaque changement d'état (trace, disque monté, fichier sélectionné). Intégré dans UC5 (`where`/`info`) comme effet de bord automatique. Architecture console conservée (voir D1).

**D1 — Décision architecturale : console terminal conservée (2026-05-30)**

P7 et P8 ont soulevé la question : app console (actuelle) vs app GUI multi-fenêtres où la console serait elle-même une fenêtre. Décision : **conserver l'architecture console + vues GUI satellites**. Raisons : (1) transformer la console en fenêtre GUI exigerait un mini-émulateur de terminal (texte scrollable, curseur, input) — travail disproportionné hors objectif Atari ST ; (2) UC4 (line editor) est déjà la bonne UX dans mintty ; (3) `SetConsoleTitleA` (P8) couvre le besoin de statut global sans refonte. L'option "console GUI" reste ouverte comme UC long terme si nécessaire.

---

### Arbitrage UC3.3 (2026-05-31)

**P9 — ESC ferme la vue dir** → **IMPLÉMENTÉ — UC4.1**

`gui_request_close(hWnd)` non-bloquant dans `gui.c` ; ESC dans `dir_handle_key()` ; `gui_close_window()` join conditionnel (thread déjà terminé → retour immédiat). Applicable à toutes les vues futures.

**P10 — Navigation historique dir (pile Précédent/Suivant ALT+←/→)** → **DIFFÉRÉ — UC18**

Coût moyen (`szHistory[N]` + gestion lifetime des nœuds). À réfléchir en contexte `mount` où la navigation dir est directement couplée au drag & drop.

**P11 — Indicateur sélection active dans la vue dir** → **ACCEPTÉ — UC7**

`szLastSelected` dans `dir_view_t`, couleur de fond secondaire (vert sombre) dans `dir_render()`. UC7 est le bon moment : c'est là que la sélection active est consommée par `load`.

**P12 — Flèches ←/→ pour expand/collapse** → **IMPLÉMENTÉ — UC4.1**

LEFT = collapse dir expanded ; RIGHT = expand dir collapsed (lazy-load si besoin). Dans `dir_handle_key()`.

**P13 — ESPACE = sélection pure, sans expand** → **IMPLÉMENTÉ — UC4.1**

ENTER = action (`dir_activate_sel`), ESPACE = `szSelected` mis à jour sans expand/collapse. Séparés dans `dir_handle_key()`.

**P14 — Sélection multiple CTRL+ESPACE/SHIFT+ESPACE** → **DIFFÉRÉ — UC18**

La contrainte "même répertoire uniquement" est raisonnable. Réfléchir en contexte `mount` où la sélection multiple est directement consommée pour le drag & drop multi-fichiers.

**P15 — Masquer fichiers cachés `.*` par défaut** → **IMPLÉMENTÉ — UC4.1**

Filtre `cFileName[0] == '.'` dans `dir_node_load_children(ptNode, bShowHidden)` + option `dir -a` pour tout voir.

**P16 — Focus auto fenêtre dir à l'ouverture** → **IMPLÉMENTÉ — UC4.1**

`SetForegroundWindow()` + `SetFocus()` après `ShowWindow()` dans `win_wnd_thread()`.

**P17 — Recommandation traçabilité 2 phases par UC** → **ACCEPTÉ — R19 §5** *(2026-05-31)*

Formalisé en R19 : Phase 1 (code/tests) + Phase 2 (documentation) obligatoires avant clôture UC. Claude initie Phase 2 sans demande explicite.

**P18 — Version `ST_VERSION` à chaque UCx.y** → **ACCEPTÉ — R20 §5** *(2026-05-31)*

Formalisé en R20 : format `MAJOR.MINOR.PATCH`, mise à jour dans `common.h` en Phase 2 de clôture UC.

**P19 Trace off ne devrait pas clore la console de trace mais seulement filtrer les LOG_TRACE** → **IMPLÉMENTÉ (2026-05-31)**

Avis Claude : ACCEPTÉ — sémantique claire et cohérente : `trace off` = "silence les LOG_TRACE" (filtre), `trace` (no arg) = "toggle la visibilité de la vue" (ouverture/fermeture). Implémenté en 2 fichiers : `src/line.c` (suppression de `trace_close()` + nouveau message utilisateur), `use_cases/use_case_02.c` (ADAPTED markers sur INT-TRC-018/019). `SRTD.md` mis à jour : UFR-CON-031, REQ-TRC-016 ajouté, INT-TRC-012, TC-TRC-015/016. `CLAUDE.md` §6.2 contrat mis à jour.

**P20 TODO(UC3) de trace.h a été oublié** → **ACCEPTÉ — planifié UC4.4**

Avis Claude : ACCEPTÉ — le TODO(UC3.3) dans `trace.c:216` et `trace.h:24` prévoyait de remplacer la sortie stderr par une vraie fenêtre GUI. Oublié pendant UC3.3. Avec l'infra D2D disponible, la vue trace est une fenêtre D2D avec scroll de texte append-only coloré par niveau (plus simple que `dir` — pas d'arbre ni de lazy-load). Mécanisme : `trace.c` poste les entrées via `gui_msg_queue_t` vers le thread fenêtre `GUI_WND_TRACE`. P19 et P20 sont complémentaires : avec P20, la sémantique P19 ("trace off" = filtre, "trace" = toggle vue) gagne encore plus de sens. UC4.4 ajouté au tableau §6.

---

### Arbitrage UC4.1 (2026-05-31)

**P21 — Touche `H` pour toggle hidden files depuis la vue dir** → **ACCEPTÉ — UC5**

Depuis la vue dir ouverte, touche `H` toggle `bShowHidden` + rechargement des enfants du nœud courant + rebuild flat + redraw. Symétrique avec `dir -a` en ligne de commande. Coût : 1 case dans `dir_handle_key()` + appel `dir_node_reload_children()`. Différé de UC4.2 (qui ne touche que le line editor console) à UC5, où `dir_handle_key()` sera de nouveau en scope.

**P22 — F5 pour rafraîchir la vue dir** → **ACCEPTÉ — UC5**

Touche F5 = `dir_node_reload_children(ptView->ptRoot)` + rebuild flat + redraw. Utile dès UC7 quand `load` sera actif et que des fichiers peuvent apparaître/disparaître pendant une session. Coût : 1 case dans `dir_handle_key()`. Différé de UC4.2 à UC5 avec P21 (même scope : `dir_handle_key()`).

**P23 Acceptation de la proposition d'auto-completion**

Lorsqu'il existe plusieurs possibilités d'auto-completion dans les noms de fichiers en argument des commandes, les possibilités s'affichent en color DIM mais sans possibilité de valider l'entièreté de la proposition par Entrée (sans valider la commande), puis un deuxième appui Entrée valide la commande complète.

Avis Claude : **REFUSÉ tel quel — mécanisme TAB standard suffit.** Le double-Enter casse la sémantique fondamentale "Enter = exécuter" (invariant de UC1 à UC4.3). Un premier Enter qui insérerait le ghost sans exécuter surprendrait l'utilisateur habitué à tout shell. L'implémentation UC4.3 est déjà conforme au standard : 1 candidat → insertion immédiate du suffixe par TAB ; N candidats → TAB cycle le ghost DIM ; toute touche non-TAB efface le ghost. Si l'on veut améliorer, la bonne direction est le **préfixe commun** : le premier TAB sur N candidats insère d'abord le plus long préfixe commun aux N candidats (comme bash), puis les TABs suivants cyclent. Cela reste 100 % TAB-driven et ne touche pas Enter.

→ **Proposition reformulée — P23bis (déposée ci-dessous, à arbitrer)**

---

### Arbitrage UC4.3 (2026-06-01)

**P23bis — TAB : insertion du préfixe commun avant le cycle** → **ACCEPTÉ — UC5**

Sur N candidats, le premier TAB insère le plus long préfixe commun dans le buffer ; les TABs suivants cyclent les candidats en ghost DIM. Comportement bash standard.

**P24 — `colors auto` : détection ANSI automatique au démarrage** → **ACCEPTÉ — UC5**

`bColors = isatty(STDOUT_FILENO)` dans `line_init()` : ANSI activé si TTY, désactivé si pipe/fichier. `colors on/off` reste disponible pour forcer.

**P25 — Commande `history [N]` : afficher les N dernières commandes** → **ACCEPTÉ — UC5**

Sans arg : 10 dernières numérotées. Avec arg N : N dernières. Nouvelle `line_cmd_history()` + `CMD_HISTORY`.

**P26 — `trace` (toggle-open) : LOG_TRACE off par défaut** → **ACCEPTÉ — APPLIQUÉ UC4.4**

Avis Claude : ACCEPTÉ — sans ce filtre, chaque `platform_mutex_lock/unlock` dans la boucle de rendu génère une rafale de LOG_TRACE à chaque repaint (vu lors de UC4.4 comme cause du stack overflow). L'utilisateur doit utiliser `trace on` explicitement pour activer LOG_TRACE. Sémantique résultante : `trace` = "ouvre la console pour voir INFO/ERROR/TODO" ; `trace on` = "débogage détaillé avec TRACE". `trace off` inchangé (P19).

---

### Arbitrage UC4.4 (2026-06-01)

**P27 — `trace clear` : vider le ring buffer GUI** → **ACCEPTÉ — IMPLÉMENTÉ UC5**

Avis Claude : ACCEPTÉ, faible coût. Sous-commande `trace clear` : efface le ring buffer `aszLines[]`, remet `iHead/iCount/iScrollOff` à zéro, `gui_invalidate()`. Même scope que `line_cmd_trace()` et `trace_view_t`. UC5 est le bon endroit puisque c'est le dernier UC console/trace.

**P28 — Filtre de niveau minimal dans la vue trace (indépendant du filtre fichier)** → **ACCEPTÉ — IMPLÉMENTÉ UC5**

Avis Claude : ACCEPTÉ — la valeur est réelle : `trace level info` cache les lignes TRACE dans la fenêtre GUI sans les supprimer du fichier log (utile pour voir INFO/ERROR en live sans bruit TRACE). Coût moyen : champ `eMinLevel` dans `trace_view_t` + filtre dans `trace_render()` + cas dans `line_cmd_trace()` (`trace level trace|info|error`). Cohérent avec P26 (TRACE off par défaut). Planifié UC5 pour rester groupé avec les autres améliorations trace/console.

---

### Arbitrage UC5 (2026-06-01)

*Toutes les propositions P21–P28 planifiées pour UC5 ont été implémentées et validées dans ce UC. Aucune proposition UC5 ouverte en §7 — UC5 est clos.*

---

### Arbitrage UC6 (2026-06-01)

*UC6 est un UC purement infrastructure (abstraction fichiers). Aucune proposition UX/fonctionnelle n'a émergé. Aucune proposition §7 en attente — UC6 est clos.*

---

### Arbitrage UC7 (2026-06-01)

**P11 — Indicateur sélection active dans la vue dir** → **IMPLÉMENTÉ — UC7**

P11 était arbitrée ACCEPTÉ depuis UC3.3 et planifiée à UC7. Implémentation conforme :
- `szLastSelected[ST_MAX_PATH]` dans `dir_view_t`, mis à jour sur ENTER et SPACE (fichiers uniquement).
- `g_dir_clrLastSel = {0.04f, 0.28f, 0.10f, 1.0f}` (vert sombre) rendu avant le fond bleu de navigation → les deux couleurs sont visuellement distinctes.

*Aucune autre proposition ouverte pour UC7. Aucune nouvelle proposition UX/fonctionnelle n'a émergé pendant l'implémentation — UC7 est clos.*

---

### Arbitrage UC8 (2026-06-02)

**P29 — Undo (CTRL+Z) dans l'éditeur texte** → **IMPLÉMENTÉ — UC8 (2026-06-02)**

Ring buffer 20 niveaux, snapshot complet + groupage inserts consécutifs. Contrats dans §6.13.

**P30 — Find/Replace (CTRL+F / CTRL+H)** → **DIFFÉRÉ — UC10 ou après**

Avis Claude : ACCEPTÉ. Barre de saisie en bas de fenêtre (style éditeur minimal). À planifier à UC10 quand la famille edit (txt + hex + intégré) est stable.

*Aucune autre proposition ouverte pour UC8. UC8 est clos.*

---

### Arbitrage UC9 (2026-06-02)

**P31 — Undo (CTRL+Z) pour l'éditeur hex** → **DIFFÉRÉ — UC9-bis si besoin**

Avis Claude : ACCEPTÉ différé. Le mode remplacement avec fichiers ATARI ST (< 900 KB) rend l'undo moins critique qu'en texte : l'utilisateur peut re-ouvrir le fichier original. À implémenter si Tonton Marcel en ressent le besoin (même architecture ring-buffer que UC8).

Avis Tonton: OK pour différer UC10, qui va synchroniser les vues => voir comment gérer le CTRL+Z : est-ce pour la vue texte seule ? ou les 2 ? en fonction des modifications cela peut être complexe à gérer (e.g. une modif byte hex, puis une modif text assembleur, que faire en cas de 2 CTRL+Z : normalement appliquer seulement celui de la vue active, mais cela peut ne pas paraître logique à l'utilisateur s'il peut modifier les 2 vues...) ou sinon simplement ne pas implémenter le CTRL+Z dans la vue hex.

**P32 — SHIFT+flèches pour sélectionner une plage**

Utile pour la vue synchronisée assembleur / hex pour visualiser la partie binaire vs code: un surlignage peut apparaître dans les 2 vues lorsqu'un opcode/operand complet est sélectionné et correspond à une instruction assembleur ou vice-versa, une instruction surlignée voit sa correspondance surlignée. Peut-être utile également pour repérer les boucles et conditions dans hex et assembleur, ou encore les structures de données.

Avis Claude : **ACCEPTÉ DIFFÉRÉ — après UC14**. UC10 a posé l'infrastructure de synchronisation hex↔disasm. La sélection par plage prend son plein sens après UC14 (vrai désassembleur avec instructions multi-mots) — sinon toutes les instructions font 2 bytes et la sélection est triviale. Planifier après UC14, soit en UC14-bis, soit intégré dans UC14 si le scope le permet.

**P33 — Commande 'edit hex' pour forcer une ouverture hex d'un fichier**

Permet de visualiser n'importe quel fichier binaire (e.g. memory dump pendant execution UCxx) ou autre, qui ne soit pas par défaut .PRG, .TTP, .TOS. Par contre, la limite de taille reste inchangée, car on reste dans le domaine ATARI ST.

Avis Claude : **ACCEPTÉ — UC10-bis ou intégré dans une UC ultérieure**. Coût faible : `line_cmd_edit()` reconnaît un flag `-h` / `--hex` (`edit -h <file>` ou `edit --hex <file>`) et force `edit_hex_open()` quel que soit le type détecté. Limite EDIT_HEX_MAX_SIZE inchangée. Utile pour ouvrir un dump mémoire ou un `.txt` en hex. À planifier avec P32.

*P32/P33 arbitrées dans UC10 (2026-06-02). UC9 Phase 2 close.*

---

### Arbitrage UC10 (2026-06-02)

*UC10 ajoute le panneau désassembleur synchronisé au hex editor (infrastructure, stub DC.W). P32/P33 issues de UC9 ont été arbitrées ci-dessus. Aucune nouvelle proposition UX/fonctionnelle n'a émergé pendant l'implémentation de UC10 — UC10 est clos.*

---

### Arbitrage UC11 (2026-06-03)

*UC11 est un UC purement interne (décodeur EA + 7 instructions 68000). Aucune proposition UX/fonctionnelle n'a émergé — UC11 est clos.*

---

### Arbitrage UC12 (2026-06-03)

*UC12 est un UC purement interne (arithmétique, logique, multiplication, division). Aucune proposition UX/fonctionnelle n'a émergé — UC12 est clos.*

---

### Arbitrage UC13 (2026-06-03)

*UC13 est un UC purement interne (shifts, rotations, bit ops). Aucune proposition UX/fonctionnelle n'a émergé — UC13 est clos.*

---

### Arbitrage UC14 (2026-06-03)

*UC14 est un UC purement interne (contrôle de flux 68000). Aucune proposition UX/fonctionnelle n'a émergé — UC14 est clos.*


---

### Arbitrage UC18.1 (2026-06-06)

*UC18.1 implémente la vue D2D mount + commandes mount/umount. Propositions ouvertes de §7 déjà planifiées en UC18.2 (P10, P14). Propositions P34–P38 déposées par Tonton Marcel et arbitrées ci-dessous.*

**P34 — Panneau latéral : ajout des propriétés head/sector/tracks modifiables** → **ACCEPTÉ MODIFIÉ — UC18.2**

Les disquettes ATARI ST ont différents formats (head/sectors/tracks) en fonction des démos ou applications, le standard étant 720Ko obtenus par 2 heads / 9 sectors / 80 tracks & 512 BPS. Mais certaines disquettes peuvent augmenter leur capacité par 2 heads / 11 sectors / 82 tracks et être correctement lues par le lecteur de disquette et le WD1772. Le panneau latéral de mount peut offrir le moyen de modifier les paramètres dans la limite du raisonnable (head 1-2, sectors 9-11, tracks 79-82), ces paramètres seront utilisés lors de la sauvegarde image .st / .msa

Avis Claude : ACCEPTÉ MODIFIÉ. Les démos ATARI ST utilisent très rarement les formats non-standard (9 secteurs / 80 pistes est quasi-universel) ; afficher les propriétés BPB en lecture seule couvre 95 % du besoin. **UC18.2 ajoute l'affichage lecture seule** des paramètres BPB dans le panneau propriétés (heads, sectors/track, tracks, BPS — déjà dans `image_st_t`, coût nul) ; **UC20 ajoute la modification** lors de la création d'image.

**P35 — Prévoir une sauvegarde .st / .msa et répertoire en local en option** → **ACCEPTÉ — UC19**

Afin de permettre un reversement .st => répertoire local, la sauvegarde de l'image disquette lors de la commande umount peut se faire par une boite de dialogue avec 3 cases à cocher: l'une génère un fichier disk.st, la deuxième un fichier disk.msa, et la 3ème un répertoire 'disk' avec le contenu identique à la fenêtre 'mount' : par contre, ce répertoire ne peut remplacer le répertoire source utilisé par 'mount' afin de ne pas supprimer de fichier en local. Schéma de principe d'une suppression de fichier dans la disquette:
- ouverture disquette par 'mount' du répertoire sélectionné par 'dir'
- travail sur le contenu de la disquette, par exemple, DEL sur un fichier
- 'umount' ouvre la boite de dialogue ou 'umount --st --msa --dir' génère disk.st, disk.msa et un nouveau répertoire 'disk' dans le répertoire source de 'dir'
- la fenêtre 'mount' est close, le répertoire source est inchangé

Avis Claude : ACCEPTÉ. La boîte de dialogue à 3 cases est le périmètre naturel de UC19 (`umount`). UC18.1 a déjà posé `bDirty` — UC19 exploitera ce flag. L'option `umount --st --msa --dir` sans dialog est pratique pour les scripts. Le répertoire cible `disk/` est toujours créé à côté de la source, jamais en remplacement. Planifié UC19.

**P36 - Première ligne A:\ indique le nombre de fichiers sur 112** → **ACCEPTÉ — UC18.2**

Information " / 112" perturbante dans la première ligne (même s'il s'agit de la limite du nombre de fichier de la root dir, comme indiqué dans le header). Proposition d'intégrer cette information dans le panneau des propriétés - pas besoin que cette propriété soit modifiable, sauf si on veut conserver une cohérence de la gestion des propriétés avec P34.

Avis Claude : ACCEPTÉ. L'info est bien une propriété de disquette mais redondante dans l'en-tête de la liste. UC18.2 : déplacer dans le panneau propriétés ("Root dir capacity: 112 entries") et simplifier l'en-tête à `"A:\\ [SRC]  N files"` sans le `/112`. Si P34 ajoute les propriétés BPB, RDE (Root Directory Entries) y trouve naturellement sa place.

**P37 - Ajouter la propriété modifiable "Bootable Disk" dans mount** → **ACCEPTÉ MODIFIÉ — UC18.2 lecture seule, UC19 écriture**

Permet de rendre le disk bootable (modif bootsector) et/ou de détecter s'il est bootable

Avis Claude : ACCEPTÉ avec découpage. La détection "bootable" sur ATARI ST est plus riche que le seul checksum WD1772 : elle comprend la présence d'un `JMP`/`BRA` en octets[0:1] du bootsector, la somme de contrôle (mots 16-bit big-endian ≡ 0x1234 mod 0x10000), et d'autres indicateurs comme la présence du dossier AUTO sur la FAT. UC18.2 ajoute l'indicateur "Bootable: YES/NO" en lecture seule dans le panneau propriétés (critère checksum WD1772 + JMP/BRA). Des images de test ATARI ST réelles (boot / non-boot) valideront les cas lors de UC18.2. La modification du checksum (rendre bootable) est différée à UC19 avec confirmation utilisateur.

**P38 - Analyse bootsector de l'image disque** → **ACCEPTÉ — UC18.2**

Un raccourci clavier permet d'ouvrir un panneau affichant le contenu du bootsector, en particulier s'il contient du code ou tout autre contenu (e.g. texte, signature, loader, ...). Le panneau peut contenir une vue hexadécimale basée sur celle existante UC9 en accompagnement des infos du panneau. Sans surcharger la solution technique, une fenêtre UC9 peut simplement remplacer le panneau et indiquer les informations clés du bootsector dans le titre de la fenêtre (bootable disk, H2/S9/T80, 720ko -- boot contains code(e.g. medium res code)/loader(e.g. not FAT12 filesystem)/packer(e.g. typical demo packers - Orion Sly Packer)/signature(e.g. text print on boot)/protection(e.g. Rob Northern Copylock, Yoda Lock-O-matic, ...)) ou même indiquer ces messages directement dans la console (Bootsector Infos: ...)

Avis Claude : ACCEPTÉ. Réutiliser `edit_hex_open()` sur les 512 octets du bootsector est la bonne approche — aucun nouveau composant. UC18.2 : raccourci `B` dans `mount_handle_key()` → extrait `aDisk[0..511]` dans un fichier temporaire → `edit_hex_open()`. Le titre de la fenêtre indique les infos clés détectées par une heuristique simple : `"ST4Ever - Edit: bootsector [H2/S9/T80 720Ko — code]"`. Cette détection fait office de **placeholder** : Tonton Marcel proposera un dataset de bootsectors types pour enrichir et calibrer la détection (code/loader/packer/signature/protection) dans un UC ultérieur dédié. La heuristique initiale UC18.2 se limite aux critères les plus discriminants (bootable + JMP/BRA vs BPB vs texte en clair).

---

| Proposition | Décision | UC cible |
|-------------|----------|----------|
| P34 (géométrie BPB lecture seule) | ACCEPTÉ MODIFIÉ | UC18.2 |
| P35 (dialog umount save .st/.msa/dir) | ACCEPTÉ | UC19 |
| P36 (supprimer /112 de l'en-tête liste) | ACCEPTÉ | UC18.2 |
| P37 (indicateur Bootable lecture seule) | ACCEPTÉ MODIFIÉ | UC18.2 |
| P38 (analyse bootsector via edit_hex) | ACCEPTÉ | UC18.2 |

*Toutes les propositions P34–P38 ont été arbitrées — UC18.1 est clos.*

---

### 6.25 Use Case 18.2 (UC18.2) — ✓ VALIDÉ (2026-06-06)

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
- UC19 : `bDirty == ST_TRUE` à l'umount → dialog "save image?" ; options `.st`/`.msa`/`répertoire` (P35) ; flag `bootable` en écriture (P37 écriture = modifier le checksum pour le rendre bootable).
- UC20 : `image_st_save` / `image_msa_save` disponibles — `image` commande depuis contenu monté.
- `aszMultiSel` : les chemins de multi-sélection sont disponibles pour le drag-and-drop (UC18.2 ne l'implémente pas encore) et pour la commande `load` multi-fichiers future.
- `dir_navigate_to()` : l'API est désormais publique — accessible aux vues futures qui veulent piloter la navigation du dir depuis l'extérieur (ex. depuis `mount` vers le source de l'image).

---

### Arbitrage UC18.2 (2026-06-06)

**P39 — Status bar dans la vue mount** → **À ARBITRER**

Avis Claude : La vue mount pourrait afficher une barre de statut persistante en bas de fenêtre (1 ligne, fond légèrement différent) indiquant : nombre de fichiers sélectionnés (multi-sel P14), espace libre, et l'état dirty `[*]`. Cela évite de chercher ces informations dans le panneau droit. Coût moyen : `MOUNT_STATUS_H` constante, réduction de `iWndHeight` utilisable dans `mount_render()`. À arbitrer pour UC19 ou laissé comme amélioration cosmétique optionnelle.

**P40 — Barre de progression lors de la copie de gros fichiers dans mount** → **À ARBITRER**

Avis Claude : `mount_view_add_file()` est synchrone et peut bloquer plusieurs secondes pour des fichiers > 500 Ko (limite FAT12). Une `LOG_INFO` de progression par tranche de 64 Ko dans la boucle de lecture + `gui_invalidate` pour afficher "Copying… X%" dans la status bar (P39). Coût faible si P39 est implémenté. Différer à UC19 où le périmètre `add_file` sera de nouveau en scope.

**P41 — Raccourci `Enter` dans mount → ouvrir le fichier sélectionné en hex** → **À ARBITRER**

Avis Claude : ENTER sur une entrée de la liste mount → `edit_hex_open(szEntryPath)` en extrayant d'abord le fichier dans un temp. Coût moyen (même pattern que P38). Alternative : double-clic. Utile pour inspecter un binaire PRG dans l'image sans le démonter. À planifier UC20 ou après.

| Proposition | Décision | UC cible |
|-------------|----------|----------|
| P39 (status bar mount) | ACCEPTÉ | UC19 |
| P40 (progression copie) | ACCEPTÉ | UC19 |
| P41 (Enter → hex dans mount) | ACCEPTÉ | UC20 |

*Toutes les propositions P39–P41 ont été arbitrées — UC18.2 est clos.*


## 8. Licence & attribution
Pas de redistribution prévue à ce jour

## 9. Extension : ST Revival sur Pi Zero Bare Metal

### 9.1 Vision et objectif

Cette extension prolonge ST4Ever au-delà de l'émulation ATARI ST vers
un "revival" authentique : porter le code d'une démo ST sous forme de
C portable exécutable nativement, sans émulateur 68k, sur trois cibles :

- **Pi Zero bare metal** : cible principale, ARM1176, accès HW direct,
  sans OS — fonctionnellement analogue au TOS (pas de multitasking,
  pas de MMU complexe). C'est la cible la plus contraignante et celle
  qui force la bonne abstraction.
- **PC Windows** : backend Direct2D existant, réutilisé tel quel.
- **PC Linux** : backend X11 existant, réutilisé tel quel.

Le comportement de la démo portée ne sera pas pixel-perfect par rapport
à l'original ST, mais restera fidèle au "feeling" utilisateur : effets
visuels reconnaissables, timing VBL raisonnable, son et inputs présents.

La démo de référence pourrait être l'une des **Cudly Demos** des TCB (The CareBears) (UC31).

### 9.2 Relation avec les UC existants

UC31 change de statut : il n'est plus l'objectif terminal du projet
mais le **point de pivot** entre la phase émulation et la phase revival.
UC31 produit la démo tournant via émulateur ST — cette référence visuelle
et le désassemblé extrait alimentent directement UC32.

Séquence globale révisée :

```

UC1–UC30 Infrastructure ST4Ever (inchangée)
UC31 Démo via émulateur ST — référence visuelle + extraction désasm
UC32 Analyse annotée du désassemblé — patterns automatiques + manuel
UC33 Squelette C portable — flot de contrôle + HAL stubs
UC34 Toolchain cross + harness test PC
UC35 Pi Zero bare metal OS minimal
UC36 Renderer Pi Zero — framebuffer, palette, VBL
UC37 Son + inputs bare metal
UC38 Démo Enchanted Land sur Pi Zero — "revival bare metal"
UC39+ Backend PC natif D2D/X11 — "revival PC sans émulation"
```

### 9.3 Description détaillée des UC d'extension

**UC32 — Analyse annotée du désassemblé**

UC32 produit une version enrichie du désassemblé UC11–UC14 via deux
niveaux d'analyse :

*Niveau automatique (Claude) :*
Le désassemblé 68k est parcouru par un analyseur de patterns qui détecte
et annote les structures typiques des démos ST avec une probabilité
estimée :

| Pattern détecté | Annotation produite |
|---|---|
| `move.l #handler,$70.w` | `; [VBL_INSTALL ~95%]` |
| Accès `$FF8200`–`$FF827F` | `; [HW: video_base_hi]` etc. |
| Accès `$FF8800`–`$FF8803` | `; [HW: YM2149_reg/data]` |
| Accès `$FFFA00`–`$FFFA2F` | `; [HW: MFP_reg]` |
| `trap #1` + D0 | `; [GEMDOS call: Pterm0 etc.]` |
| `trap #14` + D0 | `; [XBIOS call: Vsync etc.]` |
| `dbra` + bloc `move.w (A0)+,(A1)+` | `; [BLITTER_SOFT ~70%]` |
| Table 32 mots oscillants 0–200 | `; [DATA: sinus_table ~80%]` |
| Densité HW élevée par bloc | `; [BLOCK: graphic_engine ~75%]` |

*Niveau manuel (Tonton Marcel + Claude) :*
Session d'analyse collaborative sur le désassemblé annoté. Claude
propose des identifications de blocs fonctionnels (décompacteur, scroller,
effet raster, loader), Tonton Marcel valide et nomme. Ce travail produit
un désassemblé commenté qui est le matériau source de UC33.

UC32 est le seul UC du projet sans livrable entièrement automatisable —
son critère de clôture est la validation par Tonton Marcel que les blocs
fonctionnels principaux sont identifiés et nommés.

**UC33 — Squelette C portable avec HAL stubs**

À partir du désassemblé commenté UC32, production d'un programme C
répliquant le flot de contrôle et les structures de données de la démo.
Chaque accès HW est remplacé par un appel à une fonction stub HAL :
`hal_set_video_base()`, `hal_vbl_wait()`, `hal_palette_set()`,
`hal_ym_write()`, etc. Le squelette compile et s'exécute sur PC
(avec stubs no-op) sans aucun code HW réel. C'est la HAL qui définit
le contrat entre la logique démo et les backends suivants.

**UC34 — Toolchain cross + harness test PC**

Installation et validation du toolchain `arm-none-eabi-gcc` pour
cross-compilation ARM bare metal. Mise en place d'un harness de test PC :
le même code C portable UC33 compile avec un backend Win/D2D ou X11
(réutilisant l'infrastructure ST4Ever existante) pour valider la logique
sans passer par le Pi Zero. Ce harness est l'équivalent Pi Zero de ce
que l'émulateur ST est pour le 68k.

**UC35 — Pi Zero bare metal OS minimal**

OS bare metal ARM minimaliste fonctionnellement analogue au TOS :
démarrage (vecteurs ARM, stack, BSS clear), UART debug, timer
(émulation VBL à 50 Hz), framebuffer via GPU mailbox BCM2835,
pas de multitasking, pas de MMU. Le critère de clôture est l'affichage
d'un écran de couleur unie synchronisé VBL sur Pi Zero réel.

**UC36 — Renderer Pi Zero**

Implémentation des fonctions HAL vidéo sur Pi Zero : framebuffer RGB,
modes basse/moyenne résolution ST simulés, palette 16 couleurs,
synchronisation VBL. Réutilise les interfaces HAL définies en UC33.

**UC37 — Son + inputs bare metal**

Émulation YM2149 logicielle via un émulateur AY-3 open source (~300
lignes C, ex. `ayumi` ou équivalent) tournant sur le cœur ARM ; la
sortie PCM est envoyée vers le PWM BCM2835. Cette approche donne un
timing plus prévisible qu'une attaque directe du PWM et un son plus
fidèle qu'une approximation par registres. Lecture clavier/joystick
via GPIO ou USB HID minimal. Complète la HAL pour tous les services
requis par la démo.

**UC38 — Démo cible sur Pi Zero bare metal**

Intégration complète : code C UC33 + HAL UC35–UC37 + toolchain UC34.
La démo (choisie en UC31, voir §9.4) s'exécute sur Pi Zero bare metal.
Critère de clôture : comportement visuel et sonore reconnaissable par
rapport à la référence émulateur UC31. C'est l'objectif terminal de
la branche Pi Zero.

**UC39+ — Backend PC natif D2D/X11**

La HAL étant définie et validée sur Pi Zero (le cas le plus contraint),
l'implémentation du backend PC est l'ajout d'une nouvelle plateforme
dans l'architecture existante ST4Ever. La démo tourne sur PC sans
émulateur 68k — c'est l'objectif §1 original du projet.

### 9.4 Recommandations spécifiques à l'extension

**R_EXT_1 — HAL comme contrat inter-UC**
Les signatures des fonctions HAL définies en UC33 sont contractuelles :
elles ne changent pas entre UC34 et UC39. Tout besoin nouveau d'une
démo future s'ajoute à la HAL sans modifier l'existant.

**R_EXT_2 — Pi Zero comme cible de référence, pas PC**
La HAL est dimensionnée par les contraintes Pi Zero (pas de float HW,
mémoire limitée, pas d'OS). Le backend PC ne peut qu'être plus simple,
jamais plus contraint. Concevoir pour Pi Zero d'abord garantit la
portabilité descendante.

**R_EXT_3 — UC32 : livrable = fichier .s annoté**
Le désassemblé commenté UC32 est versionné dans `use_cases/UC32/`
au même titre que les fixtures UC01. C'est la source de vérité pour
UC33 — toute modification de l'analyse se fait sur ce fichier.

**R_EXT_4 — Harness PC UC34 : réutiliser gui.c/renderer.c**
Le harness de test PC n'est pas un nouveau projet — c'est une nouvelle
cible dans le Makefile ST4Ever existant, avec un backend `pi0_pc/`
ajouté à côté de `win/` et `linux/`. La logique de threading et de
message queue est réutilisée telle quelle.

**R_EXT_5 — Sélection de la démo cible (décision UC31)**
La démo cible n'est pas fixée avant UC31 ; elle est choisie pendant
cet UC à partir de trois candidats explorés avec ST4Ever lui-même :

| Candidat | Source | Critères à vérifier en UC31 |
|---|---|---|
| Effet issu de `ggnkua/Atari_ST_Sources` | GitHub — sources ST collectées | < 10 Ko .text, pas d'opcodes illégaux, pas STE |
| Démo custom basée sur tutoriaux Perihelion (`nguillaumin/perihelion-m68k-tutorials`) | GitHub + site html | VBL + palette + YM2149 + scroll ; code conçu pour la HAL |
| FujiBoink (`larsbrinkhoff/FujiBoink`) | GitHub — source disponible | Inspecter taille .text et features HW |

Critère de sélection : section `.text` < 10 Ko, pas d'opcode STE ni
illégal, couvre VBL + Shifter + YM2149. La démo custom Perihelion est
le choix le plus sûr si aucun candidat "historique" ne satisfait les
critères — son code pédagogique maximise l'utilité éducative d'UC32.

**R_EXT_6 — Émulateur YM2149 : préférer un soft émulateur ARM**
Plutôt qu'une attaque directe du PWM BCM2835 par registres YM2149,
UC37 intègre un émulateur AY-3/YM2149 open source (ex. `ayumi`,
MIT License, ~400 lignes C portable) qui génère un buffer PCM envoyé
au PWM. Avantages : timing prévisible, son plus fidèle, portabilité
vers le backend PC (même émulateur, sortie vers SDL_audio ou PortAudio
en UC39). La HAL expose `hal_ym_write(reg, val)` — l'implémentation
sous-jacente (émulateur soft vs accès direct) est transparente pour
UC33.
