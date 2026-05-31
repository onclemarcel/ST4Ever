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

Chaque commande est accessible par son nom complet, son alias monogramme et un raccourci CTRL. L'éditeur de ligne (UC4) offrira : historique ↑↓, Home/End, tab-completion fichiers/répertoires, pré-affichage en gris de la complétion.

**Commandes validées :**

| Commande | Alias | CTRL       | UC      | Comportement résumé |
|----------|-------|------------|---------|---------------------|
| `help`   | `h`   | CTRL+H     | ✓ UC1   | Liste toutes les commandes avec synopsis ; ignore les arguments (warning) |
| `quit`   | `q`   | CTRL+Q / C | ✓ UC1   | Ferme toutes les vues et quitte proprement ; ignore les arguments (warning) |
| `trace`  | `t`   | CTRL+T     | ✓ UC2   | Sans arg : toggle open/close. `on` : ouvre + active LOG_TRACE. `off` : désactive + ferme. Détails §6.2 |

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

- **Phase 1 — Implémentation** : modifications sources/includes/Makefile, `make tests` 0 failure, `make manual` validé. Claude rend la main à Tonton Marcel pour relecture et validation fonctionnelle.
- **Phase 2 — Documentation** : 
  - mise à jour CLAUDE.md (§4 pratiques, §5 recommandations, §6 tableau + sous-section UC validé)
  - mise à jour traçabilité dans `use_cases_XX.c` (INTENT, ADAPTED, TEST MATRIX)
  - mise à jour SRTD.md (UFRs, REQs, Tables fonctions, Tests, Matrices, Open Items) en conséquence des nouveaux contrats fonctionnels de CLAUDE.md (§6.x) et des modifications des `use_cases_XX.c`.
  - Analyse des nouvelles propositions d'amélioration par Onclemarcel, si besoin et proposition amélioration UX par Claude, si approprié suite à la phase d'implémentation de l'UC.

Règle : **un UC n'est clos que lorsque les deux phases sont complètes** et §7 ne contient plus de propositions non arbitrées pour ce UC. Claude initie la Phase 2 sans demande explicite dès que Phase 1 est validée par Tonton Marcel.

**R20 — Mise à jour de `ST_VERSION` à chaque UCx.y** *(établie UC3.3, 2026-05-31)*

`ST_VERSION` dans `common.h` est mis à jour à chaque clôture d'UC. Format : `"MAJOR.MINOR.PATCH"` avec :
- `MAJOR` = 0 (pas encore de release publique)
- `MINOR` = numéro UC principal (ex : 3 pour UC3.x, 4 pour UC4)
- `PATCH` = numéro sous-UC (ex : 3 pour UC3.3, 0 pour UC4)

Exemples : `"0.3.3"` après UC3.3, `"0.4.0"` après UC4. La mise à jour version fait partie du checklist de clôture Phase 2 (R19). Claude ne clôt pas un UC sans avoir mis à jour `ST_VERSION`.


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
| UC4.2 | raw input | `win_console_set_raw()` + `win_console_read_key()` + termios-first (MSYS2) + `line_read_rich()` : édition char-à-char, ←/→/Home/End/Del/Backspace, CTRL+C/D/Q | raw line editor |
| UC4.4 | trace view GUI | Fenêtre `GUI_WND_TRACE` D2D : scroll texte append-only, couleurs par niveau ; `trace.c` → `gui_msg_queue_t` → thread fenêtre ; suppression TODO(UC3.3) de trace.h/trace.c | trace view remplace stderr |
| UC4.3 | complétion + historique + extras | Historique ↑↓ circulaire + `~/.st4ever_history` + tab-completion + ghost-text + prompt contextuel + `colors on/off` + `--script file` | line editor complet |
| UC5 | `where`, `info` | Répertoire courant + état sélection (where) ; dashboard global état application : cwd, fichier sélectionné, trace, disque monté, binaire chargé (info) ; **P8** : `SetConsoleTitleA` statut automatique ; **P21** : touche `H` toggle hidden dans vue dir ; **P22** : F5 refresh vue dir | affichage path + dashboard + titre console |
| UC5-bis | prefs | Module `prefs.c` : lecture/écriture `%APPDATA%\ST4Ever\prefs.ini` ; mémorisation position/taille fenêtres par type (P7) — **optionnel**, après UC5 | save/restore position fenêtre dir |
| UC6 | plateforme | Abstraction fichiers : open/read/write/stat/mkdir, listing répertoire | tests lecture/écriture |
| UC7 | `load` | Chargement fichier texte/binaire, détection type, buffer mémoire ; indicateur visuel sélection active dans vue `dir` (P11 : couleur secondaire sur ligne sélectionnée, ≠ highlight navigation) | load .txt, .bin, .PRG stub |
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
| UC18 | `mount` | Vue arbre disquette Win32/GDI + X11, drag & drop depuis `dir` ; sélection multiple dans `dir` via CTRL+ESPACE/SHIFT+ESPACE (P14, même répertoire uniquement) pour drag & drop multi-fichiers ; historique navigation `dir` ALT+←/→ Précédent/Suivant (P10) | monter répertoire en A:\ |
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

**P7 — Mémorisation position/taille des fenêtres** → **ACCEPTÉ — UC5-bis (optionnel, après UC5)**

Architecture conservée : console terminal + vues GUI (voir décision D1 ci-dessous). Module `prefs.c` (fichier INI `%APPDATA%\ST4Ever\prefs.ini`) différé après UC5, quand les vues principales seront stables.

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

## 8. Licence & attribution
Pas de redistribution prévue à ce jour