# Projet : ST4Ever : The Revival Engine for the Timeless ATARI ST

## 0. Historique et Raisons de changement
2026-05-30: UC1 validé & pratiques de développement validées au travers de ce document CLAUDE.md
2026-05-30: UC2 validé & nouvelles pratiques agréées dans CLAUDE.md

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

De futures évolutions du projet seront les utilitaires annexes tels qu'une version GEN.TTP de Devpac3 portée sur PC pour la génération des binaires sur PC sans utiliser d'émulateur tel qu'Hatari ou STeem. Ou encore le développement d'un décompilateur assembleur 68000 vers C pur pour recompiler le programme sous msys2 sur PC.
 
L'objectif ultime est la production de code source en C pur compilé sous Msys2 à partir d'images disque de démos ATARI ST pour les compiler au format PC et les exécuter sous Msys2 en fenêtre graphique Windows ou Linux sans émulation ATARI ST (d'où le 'revival'...)

### 1.1 Objectifs fonctionnels
- Application console interactive avec commandes : `help`, `quit`, `dir`, `load`, `edit`, `image`, `mount`, `umount`, `where`, `trace on/off` (les commandes ont un équivalent monogramme: 'h', 'q', 'd', 'l', 'e', 'i', 'm', 'u', 'w', 't' ainsi que des raccourcis clavier CTRL+H, CTRL+Q ou CTRL+C, CTRL+D, CTRL+L, CTRL+E, CTRL+M, CTRL+U, CTRL+W, CTRL+T)
- Éditeur de ligne riche avec gestion de l'historique des commandes par flèches haut et bas, home et end, tab-completion pour l'affichage des fichiers et/ou répertoires, et le pré-affichage en gris de la commande en cours d'écriture avec complément par la touche tab (seul le premier mot de la ligne de commande est une commande, les arguments sont en tab-completion sur les noms de répertoire/fichiers).


#### 1.1.1 (`h` | `help` | `CTRL+H`) 
Cette commande ouvre la liste des commandes disponibles ainsi qu'un résumé de ces commandes et de leurs arguments. Elle ne prend pas d'argument et ignore les éventuels arguments fournis en avertissant l'utilisateur.

#### 1.1.2 (`q` | `quit` | `CTRL+Q`) 
Cette commande ferme toutes les vues ouvertes et quitte proprement l'application. Elle ne prend pas d'argument et ignore les éventuels arguments fournis en avertissant l'utilisateur.
 
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
Cette commande prend on/off en paramètre, ou toggle le statut courant si aucun paramètre est entré. Elle ouvre une fenêtre Trace Console (affichage coloré des logs) permettant d'afficher :
  - les logs de trace permettant d'affichier les fonctions des fichiers *.c de l'application exécutées avec un résumé très court. Les traces ont une gestion de compactation par nombre de passage lorsque les fonctions récurrentes sont tracées consécutivement, pour éviter la production de trop nombreux logs textes. 
  - les logs d'infos développeur permettant d'afficher régulièrement du contenu de progression de l'application, 
  - les logs d'erreurs internes (pointeurs null, fonctions KO, erreurs permettant le debuggage), 
  - les logs 'to-do' permettant d'identifier les fonctions/lignes où du code doit être enrichi.

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


## 2. Fichiers clés

Files marked ***bold*** are generated by Claude and present in the repository.
NOTE: TODO(UC*) fait référence aux Use Cases listés en section 5. de ce document

src/ 
- ***``main.c``***: 
```
/*
 * main.c - ST4Ever application entry point
 *
 * Responsibilities:
 *   1. Parse command-line options (-t, -h).
 *   2. Initialise platform-specific subsystems (win_console_init).
 *   3. Initialise the trace subsystem (trace_init).
 *   4. Initialise the GUI subsystem (gui_init - stubbed UC3).
 *   5. Initialise and run the interactive console (line_init/run).
 *   6. Perform orderly shutdown in reverse initialisation order.
 *
 * Command-line options:
 *   -t          Open the trace console at startup.
 *   -h / --help Print usage text and exit.
 */
```

- ***``common.h``***: 
```
/*
 * common.h - ST4Ever foundational types, return codes and macros
 *
 * Included by all source files. Defines the type conventions,
 * error propagation model, platform detection, utility macros,
 * and the portable mutex / thread abstractions used throughout
 * the project.
 *
 * Platform-specific implementations of st_mutex_t and st_thread_t
 * live in win/win_platform.c (Windows) and linux/lx_platform.c
 * (Linux).
 */
```

- ***``trace.h``***:
```
/*
 * trace.h - ST4Ever trace and debug console
 *
 * Provides four log levels for runtime diagnostics:
 *
 *   LOG_TRACE  function entry with key parameter values.
 *              Compacted: consecutive calls from the same function
 *              are collapsed into a single entry with a repeat count
 *              to prevent log flooding in tight loops.
 *
 *   LOG_INFO   functional state information useful for following
 *              application progress at a higher level.
 *
 *   LOG_ERROR  internal errors: NULL pointers, failed system calls,
 *              unexpected state. Always identifies the source location.
 *
 *   LOG_TODO   marks stub functions or code paths not yet implemented.
 *              Helps track work remaining across the codebase.
 *
 * For UC1 the trace output is written to:
 *   - st4ever_trace.log  (always, when initialized)
 *   - stderr with ANSI colours (when trace console is open)
 *
 * TODO(UC3): Replace the stderr output with a dedicated Win32 GDI window (Windows)
 * or X11 window (Linux) opened by gui_create_trace_window().
 */
```

- ***``trace.c``***:
```
/*
 * trace.c - ST4Ever trace and debug console implementation
 */
```

- ***``line.h``***:
```
/*
 * line.h - ST4Ever console line reader and command dispatcher
 *
 * Manages the interactive console loop: displays the prompt,
 * reads user input, parses the command and its arguments,
 * and dispatches to the appropriate command handler.
 *
 * Command syntax:
 *   <command> [arg1 [arg2 ...]]
 *   ^-- first word only, matched against the command table
 *              ^-- tab-completed against files/dirs (UC4)
 *
 * For UC1 the input is a plain fgets() read.
 * TODO(UC4): replace with the rich line editor (history, tab-complete,
 *            pre-completion ghost text, arrow-key navigation).
 */
```

- ***``line.c``***: 
```
/*
 * line.c - ST4Ever console line reader and command dispatcher
 *
 * UC1: basic fgets() input loop with help, quit and trace handlers.
 *      All other commands dispatch to line_cmd_stub() which logs
 *      LOG_TODO and prints a "not yet implemented" message.
 * UC2: full trace on/off/toggle logic in line_cmd_trace().
 *
 * TODO(UC4): Replace fgets() with the rich line editor
 *            (history, tab-completion, ghost-text pre-completion,
 *             arrow-key navigation via win_console / lx_console).
 */
```

- ***``gui.h``***:
```
/*
 * gui.h - ST4Ever portable GUI window and event abstraction
 *
 * Provides a platform-independent interface for creating and managing
 * GUI windows.  Each view opened by a user command (dir, edit, mount,
 * execute...) is a gui_window_t running its message/event loop in a
 * dedicated thread.
 *
 * Platform backends:
 *   win/win_gui.c  - Win32: CreateWindowEx, WndProc, GetMessage pump
 *   linux/lx_gui.c - X11:   XCreateWindow, XNextEvent loop
 *
 * Threading model (R4):
 *   gui_open_window() spawns a platform_thread_create() thread that
 *   owns the window and runs its event loop.  The console thread
 *   communicates with views via msg_queue_t (defined below).
 *   Views post events back via their registered gui_event_fn callback,
 *   which is called from the view thread.
 *
 * TODO(UC3): Implement gui_open_window() and platform backends.
 */
```

- ***``gui.c``***: 
```
/*
 * gui.c - ST4Ever GUI subsystem stub
 *
 * Portable logic only.  Platform backends:
 *   win/win_gui.c  - Win32 window creation and message pump
 *   linux/lx_gui.c - X11 window creation and event loop
 *
 * TODO(UC3): Implement gui_init, gui_open_window, message queues.
 */
```

- ***``renderer.h``***:
```
/*
 * renderer.h - ST4Ever portable 2D rendering abstraction
 *
 * Provides a minimal, efficient 2D drawing API used by all GUI views
 * and by the Atari ST screen emulator.  All coordinates are in pixels,
 * origin top-left of the client area.
 *
 * Platform backends:
 *   win/win_D2D.c  - Direct2D (ID2D1HwndRenderTarget) + DirectWrite
 *   linux/lx_X11.c - Xlib (XRender extension for compositing)
 *
 * Lifecycle per frame:
 *   renderer_begin_draw()
 *     renderer_fill_rect() / renderer_draw_line() / draw_text() ...
 *   renderer_end_draw()     <- swaps / presents the frame
 *
 * The renderer_t handle is created once per window and destroyed when
 * the window closes.  Resize events trigger renderer_resize().
 *
 * Font model:
 *   A fixed-pitch monospace font is the primary font (hex editor,
 *   disassembler, CPU view, memory view).  A proportional font is
 *   used for UI chrome.  Font handles are created by the renderer
 *   and cached internally.
 *
 * TODO(UC3): Implement renderer_create() and all draw primitives.
 */
```

- ***``renderer.c``***: 
```
/*
 * renderer.c - ST4Ever renderer stub
 *
 * TODO(UC3): Implement via win_D2D.c (Direct2D) / lx_X11.c (XRender).
 */
```

- ***``dir.h``***:
```
/*
 * dir.h - ST4Ever directory tree view
 *
 * Implements the `dir` command: opens a Win32/X11 window showing an
 * indented, expandable tree of the target directory.  A selected
 * file/directory is stored in line_context_t.szSelected and becomes
 * the default argument for load, edit, mount, image.
 *
 * TODO(UC3): Implement using gui.h / renderer.h.
 */
```

- ***``dir.c``***: 
```
/*
 * dir.c - ST4Ever directory tree view implementation (stub)
 * TODO(UC3): Implement dir_open with Win32/X11 window + tree rendering.
 */
```

- ***``mount.h``***:
```
/*
 * mount.h - ST4Ever floppy disk emulation view
 *
 * Emulates an Atari ST floppy drive A:\ from a PC directory or a
 * .st / .msa disk image.  Provides a GUI tree view to browse content,
 * drag files in/out, and trigger `image` creation.
 *
 * TODO(UC18): GUI tree view for the mounted floppy.
 * TODO(UC16): .st image read/write.
 * TODO(UC17): .msa image read/write.
 */
```

- ***``mount.c``***: 
```
/*
 * mount.c - ST4Ever floppy disk emulation - portable logic
 *
 * Implements the `mount` command: emulates an Atari ST floppy drive
 * A:\ from a PC host directory or a .st / .msa disk image.
 *
 * This file contains portable logic only.  GUI tree view and
 * drag-and-drop are delegated to the platform backend via gui.h.
 * Disk image read/write is delegated to ST.c (TODO UC16/UC17).
 *
 * TODO(UC18): GUI tree view for the mounted floppy content.
 * TODO(UC16): .st raw sector image read/write.
 * TODO(UC17): .msa RLE-compressed image read/write.
 * TODO(UC19): umount with optional image save dialog.
 */
```

- ***``ST.h``***:
```
/*
 * ST.h - ST4Ever Atari ST machine emulation core
 *
 * Models the Atari ST hardware memory map, hardware registers,
 * and provides read/write access to the emulated address space.
 *
 * Memory map (24-bit address space, 0x000000 - 0xFFFFFF):
 *
 *   0x000000 - 0x0007FF   Exception vectors (68000 vector table)
 *   0x000800 - 0x07FFFF   TOS / RAM  (varies by ST model)
 *   0x080000 - 0xEFFFFF   Cartridge / expansion (mostly unused)
 *   0xF00000 - 0xF3FFFF   Cartridge ROM
 *   0xF40000 - 0xF7FFFF   Reserved
 *   0xF80000 - 0xFBFFFF   TOS ROM (192KB on STF, 256KB on STE)
 *   0xFC0000 - 0xFDFFFF   TOS ROM (upper mirror / cartridge)
 *   0xFE0000 - 0xFEFFFF   Reserved
 *   0xFF8000 - 0xFF8FFF   Hardware registers:
 *     0xFF8200 - 0xFF827F    Video (Shifter): palette, resolution,
 *                             screen base, horizontal/vertical regs
 *     0xFF8800 - 0xFF8803    YM2149 (PSG) - sound + keyboard + ports
 *     0xFF8A00 - 0xFF8A3F    Blitter (STE only)
 *     0xFFFA00 - 0xFFFA3F    MFP 68901 - timers, UART, interrupts
 *     0xFFFC00 - 0xFFFC07    ACIA 6850 - keyboard + MIDI
 *   0xFFFF82 - 0xFFFFFF   Reserved / mirrors
 *
 * TODO(UC15): Implement PRG loading and fixup relocation.
 * TODO(UC24): Implement hardware register read/write handlers.
 */
```

- ***``ST.c``***: 
```
/*
 * ST.c - Atari ST machine emulation (stub)
 * TODO(UC15): Load PRG + fixups.
 * TODO(UC24): Hardware register read/write handlers.
 */
```

- ***``CPU.h``***:
```
/*
 * CPU.h - ST4Ever Motorola 68000 CPU emulator
 *
 * Models the MC68000 register file and execution state.
 *
 * Register file:
 *   D0-D7   Data registers (32-bit, byte/word/long operations)
 *   A0-A7   Address registers (32-bit; A7 = USP stack pointer)
 *   A7'     Supervisor stack pointer (SSP, accessible in supervisor mode)
 *   PC      Program counter (24-bit effective, stored as 32-bit)
 *   SR      Status register: T S . . I2 I1 I0 . . . X N Z V C
 *             T  = trace mode
 *             S  = supervisor mode (0=user, 1=supervisor)
 *             I2-I0 = interrupt mask (0-7)
 *             X  = extend flag
 *             N  = negative flag
 *             Z  = zero flag
 *             V  = overflow flag
 *             C  = carry flag
 *
 * Opcode dispatch strategy (R2):
 *   Primary table: 256 entries indexed by (opcode >> 8).
 *   Each entry points to a handler group that decodes the full
 *   opcode including effective address mode and size field.
 *   See CPU.c for the table definitions.
 *
 * TODO(UC21): Implement MOVE/MOVEQ/LEA/CLR/SWAP.
 * TODO(UC22): Implement ADD/SUB/CMP/AND/OR/EOR/shifts.
 * TODO(UC23): Implement BRA/Bcc/JSR/RTS/TRAP + exception vectors.
 */
```

- ***``CPU.c``***:
```
/*
 * CPU.c - MC68000 CPU emulator (stub)
 *
 * TODO(UC21): MOVE/MOVEQ/LEA/CLR/SWAP
 * TODO(UC22): ADD/SUB/CMP/AND/OR/EOR/shifts
 * TODO(UC23): BRA/Bcc/JSR/RTS/TRAP + exception vectors
 */
```

- ***``disassemble.h``***:
```
/*
 * disassemble.h - ST4Ever 68000 → DEVPAC3 disassembler
 *
 * Converts raw binary opcodes to Motorola 68000 assembly source
 * in DEVPAC3 Atari ST format.
 *
 * Dispatch strategy: primary table on (opcode >> 12), then per-group
 * secondary decoding of size field and effective address mode.
 *
 * Output format matches DEVPAC3:
 *   MOVE.L  D0,D1
 *   LEA     $FF8200,A0
 *   BRA.S   .loop
 *   DC.W    $4E75     ; unrecognised opcode
 *
 * TODO(UC11): MOVE/MOVEQ/LEA/PEA/CLR/EXG/SWAP
 * TODO(UC12): ADD/SUB/CMP/MULU/DIVS/AND/OR/EOR/NOT/NEG
 * TODO(UC13): ASL/ASR/LSL/LSR/ROL/ROR/ROXL/ROXR/BTST/BSET/BCLR/BCHG
 * TODO(UC14): BRA/BSR/Bcc/JMP/JSR/RTS/RTR/RTE/TRAP/ILLEGAL
 */
```

- ***``disassemble.c``*** :
```
/*
 * disassemble.c - 68000 disassembler stub
 * TODO(UC11-UC14): implement instruction decode tables.
 */
```

- ***``as.h``***:
```
/*
 * as.h - ST4Ever DEVPAC3 assembler (text → Atari ST .PRG / .raw)
 *
 * Assembles 68000 source in DEVPAC3 Atari ST syntax into a binary.
 * Output is either a relocatable .PRG (with header + fixup list)
 * or a raw position-dependent binary (.raw).
 *
 * DEVPAC3 directives supported (TODO UC30):
 *   SECTION TEXT / DATA / BSS
 *   DC.B / DC.W / DC.L     define byte / word / longword constants
 *   DS.B / DS.W / DS.L     reserve uninitialized space
 *   EQU / SET              symbolic constants
 *   EVEN                   align to word boundary
 *   ORG                    set assembly origin (raw only)
 *   INCLUDE                include another source file
 *   MACRO / ENDM           macro definition
 *
 * TODO(UC30): Implement tokeniser, symbol table, first + second pass.
 */
```

- ***``as.c``***:
```
/*
 * as.c - DEVPAC3 assembler stub
 * TODO(UC30): tokeniser, symbol table, two-pass assembly.
 */
```

- ***``edit_txt.h``***:
```
/*
 * edit_txt.h - ST4Ever Text / source editor view
 * TODO(UC8): implement this module.
 */
```

- ***``edit_txt.c``***: 
```
/*
 * edit_txt.c - Text / source editor view (stub)
 * TODO(UC8): implement.
 */
```

- ***``edit_hex.h``***:
```
/*
 * edit_hex.h - ST4Ever Hex + ASCII editor view
 * TODO(UC9): implement this module.
 */
```

- ***``edit_hex.c``***: 
```
/*
 * edit_hex.c - Hex + ASCII editor view (stub)
 * TODO(UC9): implement.
 */
```

- ***``edit.h``***:
```
/*
 * edit.h - ST4Ever Integrated editor dispatcher (edit command)
 * TODO(UC10): implement this module.
 */
```

- ***``edit.c``***:
```
/*
 * edit.c - Integrated editor dispatcher (edit command) (stub)
 * TODO(UC10): implement.
 */
```

- ***``exec.h``***:
```
/*
 * exec.h - ST4Ever Execution engine + view synchronisation
 * TODO(UC25): implement this module.
 */
```

- ***``exec.c``***: 
```
/*
 * exec.c - Execution engine + view synchronisation (stub)
 * TODO(UC25): implement.
 */
```

- ***``exec_mon.h``***:
```
/*
 * exec_mon.h - ST4Ever Execution monitor (step/run/breakpoint)
 * TODO(UC25): implement this module.
 */
```

- ***``exec_mon.c``***: 
```
/*
 * exec_mon.c - Execution monitor (step/run/breakpoint) (stub)
 * TODO(UC25): implement.
 */
```

- ***``exec_mem.h``***:
```
/*
 * exec_mem.h - ST4Ever Memory view during execution
 * TODO(UC25): implement this module.
 */
```

- ***``exec_mem.c``***: 
```
/*
 * exec_mem.c - Memory view during execution (stub)
 * TODO(UC25): implement.
 */
```

- ***``exec_cpu.h``***:
```
/*
 * exec_cpu.h - ST4Ever CPU 68000 register view during execution
 * TODO(UC25): implement this module.
 */
```

- ***``exec_cpu.c``***: 
```
/*
 * exec_cpu.c - CPU 68000 register view during execution (stub)
 * TODO(UC25): implement.
 */
```

- ***``exec_asm.h``***:
```
/*
 * exec_asm.h - ST4Ever Disassembly view during execution
 * TODO(UC25): implement this module.
 */
```

- ***``exec_asm.c``***: 
```
/*
 * exec_asm.c - Disassembly view during execution (stub)
 * TODO(UC25): implement.
 */
```

- ***``exec_screen.h``***:
```
/*
 * exec_screen.h - ST4Ever Atari ST screen emulation view
 * TODO(UC27): implement this module.
 */
```

- ***``exec_screen.c``***: 
```
/*
 * exec_screen.c - Atari ST screen emulation view (stub)
 * TODO(UC27): implement.
 */
```

win/
- ***``win_console.c``***:
```
/*
 * win_console.c - ST4Ever Windows console initialisation
 *
 * Enables Virtual Terminal Processing (ANSI / VT100 escape sequences)
 * on stdout and stderr so that ANSI colour codes produced by trace.c
 * and line.c render correctly in Windows 10+ cmd.exe and MSYS2/mintty.
 *
 * Also sets the console output code page to UTF-8 (65001).
 *
 * Note: mintty (the MSYS2 terminal) handles ANSI natively without
 * SetConsoleMode, so these calls are non-fatal if they fail (mintty
 * does not expose a real Win32 console handle).
 */
```

- ***``win_gui.c``***:
```
/*
 * win_gui.c - Win32 window management backend for gui.h
 *
 * Each gui_window_t is backed by a win_wnd_state_t that holds:
 *   - The Win32 HWND
 *   - The dedicated thread running the Win32 message pump
 *   - The event callback and user context from gui_wnd_desc_t
 *
 * Win32 window class "ST4EverView" is registered once in gui_init().
 * All ST4Ever windows share this class; the window type drives
 * default sizing and the WndProc event routing.
 *
 * Message pump architecture:
 *   The thread spawned by gui_open_window() calls CreateWindowEx(),
 *   then enters a GetMessage() / TranslateMessage() / DispatchMessage()
 *   loop.  WM_PAINT, WM_KEYDOWN, WM_LBUTTONDOWN etc. are translated
 *   into gui_event_t and forwarded to the registered gui_event_fn
 *   callback.
 *
 * TODO(UC3): Implement win_wnd_create(), win_wnd_proc(),
 *            message pump thread, WM_PAINT → renderer integration.
 */
```

- ***``win_D2D.c``***: 
```
/*
 * win_D2D.c - Direct2D + DirectWrite renderer backend for renderer.h
 *
 * Each renderer_t is backed by a win_d2d_ctx_t that holds:
 *
 *   ID2D1Factory*           - Created once per process in renderer_init_factory()
 *   ID2D1HwndRenderTarget*  - One per window; created in renderer_create()
 *   IDWriteFactory*         - One per process
 *   IDWriteTextFormat*      - One per font id (MONO / UI)
 *   ID2D1SolidColorBrush*   - Re-created per color change or cached
 *
 * Direct2D coordinate system: float pixels, top-left origin,
 * matches renderer_rect_t exactly.
 *
 * DirectWrite font selection:
 *   RENDERER_FONT_MONO → "Consolas" (fallback: "Courier New")
 *   RENDERER_FONT_UI   → "Segoe UI" (fallback: "Arial")
 *
 * Begin/End draw maps directly to:
 *   ID2D1HwndRenderTarget::BeginDraw()
 *   ID2D1HwndRenderTarget::EndDraw()
 *
 * TODO(UC3): Include <d2d1.h>, <dwrite.h> and implement all functions.
 *            Link with: -ld2d1 -ldwrite -lole32 -loleaut32
 */
```

- ***``win_platform.c``***:
```
/*
 * win_platform.c - ST4Ever Windows platform abstractions
 *
 * Implements portable mutex and thread primitives declared in
 * common.h using Win32 CRITICAL_SECTION and CreateThread.
 *
 * TODO(UC4): Implement platform_thread_create/join/destroy.
 * TODO(UC3): Mutex used by gui_msg_queue (implement with UC3).
 */
```

linux/
- ***``lx_console.c``***: 
```
/*
 * lx_console.c - Linux console initialisation stub
 *
 * On Linux the terminal (xterm, GNOME Terminal, etc.) supports ANSI
 * escape sequences natively.  No initialisation is needed for UC1.
 *
 * TODO(UC4): Implement raw-mode line editor using termios:
 *   - tcgetattr / tcsetattr(TCSANOW) to switch to raw mode
 *   - read() byte-by-byte, parse VT100 escape sequences
 *     (\033[A = up arrow, \033[B = down, \033[H = home, \033[F = end)
 *   - Restore canonical mode on shutdown
 */
```

- ***``lx_gui.c``***: 
```
/*
 * lx_gui.c - X11 window management backend for gui.h
 *
 * Each gui_window_t is backed by an lx_wnd_state_t that holds:
 *   - Xlib Display* and Window (XID)
 *   - The dedicated thread running XNextEvent loop
 *   - The event callback and user context from gui_wnd_desc_t
 *
 * X11 window creation per window (UC3):
 *   Display *pDpy = XOpenDisplay(NULL)
 *   Window   xWnd = XCreateSimpleWindow(pDpy, ...)
 *   XSelectInput(pDpy, xWnd, ExposureMask | KeyPressMask |
 *                ButtonPressMask | StructureNotifyMask)
 *   XMapWindow(pDpy, xWnd)
 *
 * Event loop translates XEvent → gui_event_t → gui_event_fn callback.
 *
 * TODO(UC3): Implement lx_wnd_create(), XNextEvent loop thread,
 *            XDestroyWindow in lx_gui_close_window().
 */
```

- ***``lx_X11.c``***: 
```
/*
 * lx_X11.c - X11 / XRender renderer backend for renderer.h
 *
 * Each renderer_t is backed by an lx_renderer_ctx_t holding:
 *   Display*    - shared with lx_gui.c
 *   Window      - the target XID from lx_gui.c
 *   GC          - graphics context for basic draw primitives
 *   XftDraw*    - Xft anti-aliased text rendering (optional, UC3+)
 *   Pixmap      - double-buffer pixmap for flicker-free rendering
 *
 * Double-buffer strategy:
 *   All drawing goes to the off-screen Pixmap.
 *   renderer_end_draw() calls XCopyArea(pDpy, pixmap, window, ...)
 *   then XFlush().
 *
 * Colour model:
 *   renderer_color_t (float RGBA) → XAllocColor (XColor with 16-bit RGB).
 *   Colours are cached in a small LRU table to avoid exhausting colormaps.
 *
 * Text rendering:
 *   RENDERER_FONT_MONO  → XLoadFont "fixed" or Xft "monospace:size=10"
 *   RENDERER_FONT_UI    → Xft "sans:size=10"
 *
 * TODO(UC3): Include <X11/Xlib.h>, <X11/Xft/Xft.h> and implement.
 *            Link with: -lX11 -lXft -lfontconfig
 */
```
- ***``lx_platform.c``***: 
```
/*
 * lx_platform.c - Linux platform abstractions
 *
 * Implements portable mutex and thread primitives declared in
 * common.h using POSIX pthread_mutex_t and pthread_create.
 */
```

use_cases/
- ***``use_cases.h``***: 
```
/*
 * use_cases.h - ST4Ever common include for all use case test programs
 *
 * Each use_case_NN.c file includes this header so it gets all the
 * module headers and standard I/O without repeating includes.
 *
 * Test programs are stand-alone executables compiled by the Makefile
 * against the full library objects (excluding main.o).
 */
```

- use_case_*.c: fichiers de test par Use Cases permettant de valider l'avancée du projet

- ***``use_case_01.c``***:
```
/*
 * use_case_01.c - UC1 Validation: Console prototype + Trace subsystem
 *
 * Tests all components active in UC1:
 *   1.  Trace init with console open at startup (-t flag behaviour)
 *   2.  One log entry of each level: TRACE, INFO, ERROR, TODO
 *   3.  Trace compaction: verify consecutive TRACE from same function
 *       are counted, not duplicated
 *   4.  trace_close() / trace_is_open() consistency
 *   5.  trace_open() / trace_is_open() consistency
 *   6.  trace_set_trace_enabled(FALSE) suppresses LOG_TRACE
 *   7.  trace_set_trace_enabled(TRUE) re-enables LOG_TRACE
 *   8.  line_init() + line_shutdown() without running the loop
 *   9.  ST machine init / read / write / shutdown
 *  10.  CPU init from reset vectors written into ST RAM
 *  11.  cpu_step() on the hand-crafted hello.prg in UC01/
 *  12.  trace_shutdown() clean close
 *
 * Exit code: 0 = all tests passed, 1 = one or more failures.
 */

```
use_cases/UC*/
-  les fichiers ressources ou de tests nécessaires aux use cases (e.g. binaires ATARI ST, dummy images, fichier assembleur 68000 de test, bootsectors, ...)

Makefile : détecte automatiquement les fichiers sources et includes, gère le multi-platform, la compilation GCC sous MSys2 pour Windows ou terminal pour Linux, place les .o dans un répertoire de build, place les exécutables dans ./release, compile les use cases de tests et place les exécutables de tests dans ./tests
  - make : génère l'ensemble des fichiers ./build, ./release, ./tests
  - make tests : exécute les tests
  - make run : exécute l'applicatif
  - make clean : supprime les dossiers de ./build, ./release, ./tests

README.md : Fournit une description du projet synthétique en anglais
CLAUDE.md : Ce fichier en français trace le contexte projet, les décisions techniques, recommandations, découpage projet en Use Cases, Conventions, Fichiers clés. Ce fichier est maintenu au fil des conversations et modifications projet, comme indiqué en recommandation R13


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
Phase 1 (UC1-UC5) : Infrastructure console + Win32/X11 GUI framework. Phase 2 (UC6-UC12) : Fichiers, éditeurs, formats disque. Phase 3 (UC13-UC20) : Désassembleur, assembleur, CPU 68000. Phase 4 (UC21-UC26) : Émulation ST complète, exécution. Phase 5 (UC27+) : Démos.

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

1. **Documenter l'intention séparément de l'assertion** : chaque bloc de test commence par un commentaire `/* INTENT: ... */` décrivant le comportement attendu de façon immuable. Quand une assertion est mise à jour, ajouter `/* ADAPTED: UCN - raison */` sur la ligne précédente.

2. **Politique lors de l'implémentation d'un UCN** : avant de valider UCN, lancer `make tests`. Si un test antérieur échoue :
   - Comportement stub → réel (attendu) : mettre à jour l'assertion + commenter `ADAPTED: UCN`
   - Régression vraie (inattendu) : corriger le bug, pas le test

3. **Macro `TEST_SKIP` pour tests incompatibles headless** : quand un test UC antérieur suppose un stub mais que l'implémentation réelle requiert un display ou une ressource système (ex : `gui_init` ouvre une vraie fenêtre en UC3), protéger via `#ifdef ST_TEST_LEVEL_UCNN` — le Makefile définit `-DST_TEST_LEVEL_UC01` lors de la compilation de `use_case_01`. La variante `TEST_SKIP("raison")` log le skip sans échouer.

4. **Règle absolue** : `make tests` doit passer entièrement (0 = all passed) avant tout commit d'un nouveau UC. Les tests skippés sont acceptables ; les tests en échec bloquent.

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


## 6. Use Cases

Les étapes de développement fonctionnelles sont formalisées en Use Cases, permettant de développer et valider chaque cas d'usage de l'application et d'enrichir le projet avec de plus amples détails, dont les recommendations Claude AI de la section 4, et planifier le reste des Use Cases en TODO/stubs dans le projet. La liste actuelle des Use Cases est:

| # | Commande(s) | Description | Valide |
|---|---|---|---|
| UC1 | `help`, `quit`, `trace -t` | Prototype complet, stubs, console + trace fonctionnels | ✓ VALIDÉ 2026-05-30 |
| UC2 | `trace on/off/toggle` | Gestion complète de la trace console | ✓ VALIDÉ 2026-05-30 |
| UC3 | `dir` | Vue arbre Win32/GDI + X11, navigation clavier/souris, sélection fichier | ouverture, scroll, sélection |
| UC4 | console | Line editor riche : historique ↑↓, Home/End, tab-completion, ghost-text ; prompt contextuel toggleable (trace/disk/fichier) ; `colors on/off` ; historique persistant `~/.st4ever_history` ; `--script file` batch mode ; `make manual` + macro `TEST_MANUAL` | navigation + complétion + prompt état |
| UC5 | `where`, `info` | Répertoire courant + état sélection (where) ; dashboard global état application : cwd, fichier sélectionné, trace, disque monté, binaire chargé (info) | affichage path + dashboard |
| UC6 | plateforme | Abstraction fichiers : open/read/write/stat/mkdir, listing répertoire | tests lecture/écriture |
| UC7 | `load` | Chargement fichier texte/binaire, détection type, buffer mémoire | load .txt, .bin, .PRG stub |
| UC8 | `edit` texte | Vue éditeur texte Win32/GDI + X11 : scroll, numéros de ligne, sauvegarde | édition + save .S et .TXT |
| UC9 | `edit` hex | Vue hex/ASCII Win32/GDI + X11 : adresses, scroll, édition octets | navigation + modification |
| UC10 | `edit` | Vue intégrée hex+ASCII+désasm en colonnes synchronisées | sync curseur entre vues |
| UC11 | interne | Désassembleur 68000 : MOVE/MOVEQ/LEA/CLR/EXG/SWAP/PEA | désasm binaire de test |
| UC12 | interne | Désassembleur : ADD/SUB/CMP/MUL/DIV/AND/OR/EOR/NOT/NEG | désasm complet d'un .PRG |
| UC13 | interne | Désassembleur : shifts, rotations, BTST/BSET/BCLR/BCHG | |
| UC14 | interne | Désassembleur : BRA/BSR/Bcc/JMP/JSR/RTS/RTR/RTE/TRAP | désasm programme complet |
| UC15 | interne | Format PRG : parser header + sections + fixups + chargement mémoire ST | load .PRG en mémoire émulée |
| UC16 | interne | Image `.st` : lecture/écriture raw + FAT12 | mount/browse image .st |
| UC17 | interne | Image `.msa` : décompression/compression RLE | round-trip .msa |
| UC18 | `mount` | Vue arbre disquette Win32/GDI + X11, drag & drop depuis `dir` | monter répertoire en A:\ |
| UC19 | `umount` | Démontage + sauvegarde image si modifiée | dialog save |
| UC20 | `image` | Création .st / .msa depuis le contenu monté | image valide et montable |
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
| UC31 | all | Exécution d'une démo ST complète connue (ex. Enchanted Land intro) | **objectif final** |

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
- `trace off` : séquence `trace_set_trace_enabled(FALSE)` puis `trace_close()`. Idempotent si la console est déjà fermée (`trace_close()` est idempotent — contrat UC1).
- `trace <inconnu>` : warning consommateur, pas de changement d'état.
- Args superflus (`trace on foo`) : warning, puis traitement normal du premier arg.
- Après `trace off`, `LOG_INFO/ERROR/TODO` restent actifs (fichier log seulement) ; `LOG_TRACE` silencieux. `trace_set_trace_enabled(TRUE)` réactive `LOG_TRACE` immédiatement.

**Points d'attention pour les UCs suivants :**
- UC3 : `gui_init` réel ouvre une fenêtre Win32 → les tests UC1/UC2 qui appellent des stubs GUI devront utiliser `#ifdef ST_TEST_LEVEL_UCxx` + `TEST_SKIP` (mécanisme R14 en place)
- UC4 : le dispatch stdin complet permettra d'automatiser les 4 tests [S] actuellement manuels

## 7. Propositions d'améliorations

*Claude et Tonton Marcel déposent ici leurs propositions UX/fonctionnelles au fil des UCs. Avant de clore un UC, les propositions sont passées en revue ensemble : celles agréées sont planifiées dans le tableau section 6 et retirées d'ici. **Un UC est clos quand §7 ne contient plus de propositions non arbitrées pour cet UC.***

*Section vide — toutes les propositions P1–P5 issues de UC1/UC2 ont été agréées et planifiées dans le tableau UC (UC4 et UC5 étendus).*

## 8. Licence & attribution
Pas de redistribution prévue à ce jour