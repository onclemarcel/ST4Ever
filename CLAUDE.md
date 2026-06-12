# Project : ST4Ever : The Revival Engine for the Timeless ATARI ST

## 0. Historique des versions de CLAUDE.md
- 2026-05-28: Démarrage du Projet ST4Ever : l'ensemble des éléments du projet sont gérés sous Github - [ST4EVER](https://github.com/onclemarcel/ST4Ever)
- 2026-06-07: UC22 codé; tests use_cases_22.c en cours; Process d'implémentation R19 interrompu (cf section 5 - R19 pour les étapes de développement d'un Use Case)
- 2026-06-07: Restructuration documentaire : SRTD.md archivé → scindé en **2 - SR.md** (UFR/REQ) et **3 - TC.md** (TCs) ; **6 - UC.md** enrichi (§1.1 table dépendances inter-UC, ~45 marqueurs RÉSOLU UCxx) ; R17 révisé (traçabilité Impl. column UC22+)
- 2026-06-08: UC22 Codé/Testé : ADD/SUB/CMP/AND/OR/EOR/shifts + NEG/NOT/TST/EXT/ADDQ/SUBQ/ADDI/SUBI/CMPI/ANDI/ORI/EORI/MULU/MULS/DIVU/DIVS + ADDX/SUBX + rotations ASL/ASR/LSL/LSR/ROL/ROR/ROXL/ROXR — 70 tests PASS 0 fail
- 2026-06-08: UC23 Codé/Testé : BRA/BSR/Bcc(14 cond) + NOP/STOP/RTE/RTS/RTR/TRAP/LINK/UNLK/JSR/JMP + Scc/DBcc + cpu_raise_exception (pile d'exception complète) — 79 tests PASS 0 fail
- 2026-06-08: UC23-bis Codé/Testé : MOVEM.L/W -(An)/(An)+ (snapshot+reversed-mask) + ADDA.W/SUBA.W sign-extend 16→32 bits — 26 tests PASS 0 fail
- 2026-06-08: UC24 Codé/Testé : Memory map ST complet — dispatcher Shifter/YM2149/MFP/ACIA + ROM bus error + bug palette offset 0x20→0x40 corrigé — 49 tests PASS 0 fail
- 2026-06-08: UC24A Codé/Testé : Sector Fingerprint Engine — vecteur 24D + cosine pondéré + DB bootstrap/learn/finalize/save/load + signatures packers + 42 tests PASS 0 fail
- 2026-06-08: UC24B Codé/Testé : Coloration sémantique secteurs dans hex_edit — aeSecType heap + ehex_classify_sectors() + table g_aSecTint[16] + tint rendu par row — 10 tests PASS 0 fail
- 2026-06-09: UC24C Codé/Testé : JSON annotation + bandeau contextuel — image_annot.c (load/save/get/set, parser hand-rolled) + ehex_render_band() 2 lignes (LBA/type/BPB + note éditable) + CTRL+N/CTRL+S + HEX_ZONE_BAND_NOTE — 32 tests PASS 0 fail
- 2026-06-09: UC24D Codé/Testé : Labels cliquables dans le bandeau — FAT1/FAT2/Root/Data en cyan cliquables + chips JSON [N] + ehex_jump_to_lba() + edit_hex_set_cursor_pos() API publique — 10 tests PASS 0 fail
- 2026-06-10: Revue console + 1-OC.md mis à jour par Tonton — 3 bugs corrigés (trace level warning, image --msa .st→.msa Case 3, auto-complétion espaces `\ `) + P50-P56 déposées + DOC UFR-MNT-010 noté — 0 régression tests
- 2026-06-10: UC24E Codé/Testé : `dir --select` headless (P50) + `script <file>` commande interactive (P51) + CTRL+O→mount (P52) + `edit -h/--hex` force hex (P54) + suppression `umount` (P55) + `image --dir` extraction .st/.msa → répertoire (P56) — 19 tests PASS 0 fail
- 2026-06-10: UC24F Codé/Testé : Navigation arborescente vue mount (P53) — `image_st_mkdir`/`image_st_list_dir`/`image_st_write_file_in_dir` + mount ENTER→subdir/LEFT→parent + `mount_dir_cb` récursif 1 niveau + `mount_save_image` extrait subdirs — 26 tests PASS 0 fail
- 2026-06-10: Revue UC24E/UC24F — 6 bugs corrigés : BUG-04 (iNotImported + avertissement Skipped dans panneau mount), BUG-05 (T1440→T80 via TS/SPT/Heads), BUG-06 (Geometry '—' sur mount répertoire local), BUG-07 (image --dir xx.st : xx.st source pas dest), BUG-08 (ESPACE vue dir → rafraîchissement immédiat), BUG-09 (historique ALT+← persistant entre sessions dir) — 0 régression tests
- 2026-06-10: UC24G Codé/Testé : P57 `trace level all|info|error|todo` incrémental + P58 `image --in/--out` explicites + P60 sélection simple/multi exclusive dans vue dir + Phase 2 DOC-01/DOC-02 SR.md restructuré + TC.md UC24G (14 tests) + use_case_24G.c — 14 tests PASS 0 fail
- 2026-06-11: UC25A Codé/Testé : moteur d'exécution pas-à-pas `exec.c` (thread CPU + état partagé mutex + breakpoints EXEC_BP_MAX=8) + `exec_mon.c` (vue monitor F5/F6/F7/F8/B/C) + `exec_cpu.c` (vue registres D0-D7/A0-A7/PC/SSP/SR) + `line_cmd_execute()` + `exec_init/shutdown` dans main.c — 30 tests PASS (13N+9R+8S) 0 fail
- 2026-06-11: UC25B Codé/Testé : vue mémoire `exec_mem.c` (hex dump 16 bytes/row, PC row highlight yellow, UP/DN/PgUp/PgDn/HOME snap) + vue désassembleur `exec_asm.c` (disasm_one forward, PC highlight, UP/DN/PgUp/PgDn/F snap) + fix exec_open (g_exec_bOpen=TRUE avant view opens) — 21 tests PASS (11N+4R+6S) 0 fail
- 2026-06-11: BUG-10 + edit backup — BUG-10 : correction LF-only (`bEndOfBuf` avant `*p='\0'` dans `etxt_load`) + `edit backup [on|off]` + `edit_txt_set/get_backup()` + `szBackupPath` + sauvegarde `_YYYYMMDD_HHMMSS.bak` à l'ouverture, suppression/conservation à la fermeture — use_case_08.c : 36→43 tests (20N+11R+12S) Phase 2 : UFR-EDT-007, REQ-EDT-014..017, TC-EDT-037..043
- 2026-06-11: UC26 Codé/Testé : moteur rendu Shifter — `shifter.h/c` + `shifter_render()` décodage bitplanes→RGB32 (low/med/high res, 4/2/1 plans, palette 16 couleurs ST 3-bit→8-bit) — 33 tests PASS (26N+7R) 0 fail
- 2026-06-11: UC27 Codé/Testé : vue écran D2D — `exec_screen.h/c` + `renderer_platform_draw_bitmap()` D2D (BGRA+ALPHA_IGNORE, nearest-neighbour) + `exec_screen_open/close` intégrés dans `exec_open/close` + `hScrWnd` dans `exec_state_t` + invalidation dans exec thread — 21 tests PASS (7N+4R+10S) 0 fail

*L'historique des versions antérieures peut être récupéré via le change log github*

## 1. Contexte du projet (mis à jour par Tonton)

***Contexte***: Ce projet est une application console interactive multi-plateforme développée en C pur à but éducatif permettant de:
- lire/écrire des fichiers ATARI ST (texte ou binaires .PRG, .TTP, .TOS)
- créer/lire/extraire des images disques ATARI ST de type .st/.msa à partir de/vers des répertoires PC
- visualiser, modifier, compiler, assembler, désassembler, décompiler les programmes ATARI ST et/ou les disques démos
- émuler les binaires ATARI ST via divers vues (CPU, Mémoire, écran) en pas à pas, temps ralenti ou temps réel 

***User Interaction***: La console interactive permet l'interaction utilisateur via des commandes et des vues GUI associées comprenant:
- un éditeur de fichier texte et/ou fichier source d'un binaire (e.g. assembleur d'un .PRG)
- un éditeur héxadécimal pour l'édition des binaires (e.g. un .PRG, .TTP)
- une vue assembleur 68000 au format DevPac3 ATARI ST pour les sections binaires pertinentes de la vue hexadécimale (e.g. .text ou toute plage d'adresse fournie par utilisateur)
- un visualisateur d'arborescence fichiers/répertoires pour les sélections/gestions de fichiers et des images disques .st, .msa, .stx
- une console de trace/logs/erreurs pour l'application et debuggage développeur de ST4Ever
- un moniteur d'exécution des binaires ATARI ST permettant la sélection pas à pas, stop, go, temps d'exécution par instruction
    - un visualisateur CPU 68000 avec état des registres pour l'exécution des binaires
    - un visualisateur mémoire ATARI ST pour l'exécution des binaires
    - un émulateur écran/inputs/outputs ATARI ST simple pour l'exécution des binaires

 ***Objectif éducatif***: ST4Ever est un projet totalement développé from scratch dans un but d'apprentissage du développement en C pur de chaque brique/étape. Cf section R0 pour les principes documentaires du projet afin de maîtriser et maintenir chaque étape de développement.

***Revival***: La phase de "Revival" est une extension de ST4Ever au-delà de l'émulation ATARI ST vers un portage du code d'une démo ST sous forme de C portable exécutable nativement, sans émulateur 68k, sur de multiples cibles. Le comportement de la démo portée ne sera pas strictement exacte par rapport à l'original ST, mais restera fidèle au "feeling" utilisateur : effets visuels reconnaissables, timing VBL raisonnable, son et inputs présents. 

### 1.1 Concepts Opérationnels

***Voir document externe "1 - OC.md"***: le document de Concept Opérationnel contient toutes les informations fonctionnelles visibles utilisateur permettant de cadrer le projet et le découpage du projet en "Use Cases" = Bloc fonctionnel auto-porteur codé / testé / validé entre Claude et Tonton (moi-même) (cf section 6 - Use Cases)

### 1.2 Phases de développement

Le développement s'effectue par Use Cases (voir section 6 - Use Cases), permettant à Claude de progresser phase par phase avec une validation régulière des implémentations fonctionnelles par Tonton. L'objectif reste la lisibilité du code et de l'avancement pour un aspect didactique et éducatif : plus on prend notre temps, mieux on apprend (Voir section 5 - R19 pour le processus de développement)

## 2. Architecture des composants

Le découpage fonctionnel des composants (rôle, API, dépendances, séquences) est dans **SRTD.md §3–§4**.


## 3. Licence & attribution

Le projet est en cours de développement, Pas de redistribution prévue à ce jour
>Si toutefois ST4Ever venait à être redistribué, il le sera sous licence GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007

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

CLAUDE.md est l'interface de projet entre sessions de conversation. Sa mise à jour relève de Claude à chaque UC, **en cours et en fin**. Voir R19 pour les détails de la phase d'implémentation et la phase de documentation.

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

**R17 — Traçabilité REQ ↔ fonction** *(établie UC3.1 2026-05-30 — révisée 2026-06-07)*

**§4.x SRTD.md supprimé.** La traçabilité fonction↔REQ est désormais portée à deux niveaux :

1. **Dans `2 - SR.md` §2** : chaque table REQ inclut une colonne `Impl.` avec le(s) nom(s) de fonction(s) — format compact, UC22+.

```
| REQ-TRC-001 | trace shall be initializable | ✓ UC1 | trace_init() |
```

2. **Dans les `.h`** : chaque docstring de fonction publique indique ses REQ parents (optionnel, ajouté progressivement).

Règles :
- **Avant d'implémenter** une nouvelle fonction publique : vérifier qu'un REQ existe dans `2 - SR.md §2`. Sinon, créer le REQ avant d'écrire le code.
- **Lors de la Phase 2** d'un UC : remplir la colonne `Impl.` pour chaque nouveau REQ du UC.
- **Fonctions internes** < 5 lignes sans effet de bord (helpers purs) : REQ optionnel, marquer `—`.
- **UC1–UC21** : traçabilité fonction↔REQ dans l'archivé `SRTD.md §4` — pas de migration rétroactive.

Sens de la traçabilité complète : `UFR (2-SR.md §1) → REQ (2-SR.md §2, col. Impl.) → .c → TC (3-TC.md) → INT (use_case_XX.c)`

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

- **Phase 1 — Implémentation** Pendant cette phase, à partir du tableau de la section 6 - Use Cases, Claude: 
  - identifie dans **1 - OC.md** les concepts opérationnels associés au Use Case. 
  - Analyse la complexité d'implémentation en terme de taille de contexte/tokens de la conversation Claude et/ou nombre de lignes de code à générer approximativement, complexité algorithmique à implémenter
  - Propose un découpage en sous-UC pour simplifier l'implémentation
  - Modifie les sources/includes/Makefile associés au Use case, 
  - `make tests` **0 warning, 0 failure**, 
  - `ST_APP_VERSION` à jour
  - mise à jour CLAUDE.md:
    - **Section 0** Historique : Remplace la dernière ligne (UCxx / date) avec les informations synthétiques de l'UC implémenté, date, résumé des blocs fonctionnels associé en une phrase. Tous les fichiers, CLAUDE.md inclus, sont gérés sous Git.
    - **Section 4** : ajouter toute nouvelle pratique d'implémentation devant s'appliquer uniformément aux UCs suivants (nommage, style, modèle d'erreur…).
    - **Section 5** : ajouter toute nouvelle recommandation technique avec numéro séquentiel (R_N_) et date. Ne jamais renuméroter les recommandations existantes.
    - **Section 7**: Propose des améliorations UX/algorithmiques en supplément des concepts opérationnels pour une meilleure expérience utilisateur et "revival" de l'ATARI ST
    - **Section 6 (tableau)** : mettre à jour le statut du UC courant ; réviser le tableau après décision d'arbitrage (amélioration/découpage);valide le UC en cours et modifie les autres UC si nécessaire/décidé pendant le UC en cours
    - **Section 7** : créer si des améliorations fonctionnelles/UX/algorithmique émergeant pendant le UC. Les soumettre à validation pour planification parmi les UCs existants ou en tant que nouveau UC. L'objectif est de proposer des concepts opérationnels supplémentaires pour améliorer l'expérience utilisateur et la pertinence du revival ATARI ST.
  - Met à jour le fichier **6 - UC.md**: 
    - créer ou compléter selon le modèle de la section 1 de **6 - UC.md"**: périmètre implémenté, infrastructure validée, matrice de tests (N/R/S), anomalies résolues
    - un paragraphe **"Contrats comportementaux validés"** par module — ces contrats assurent la traçabilité verticale : Concepts Opérationnels (**1 - OC.md**) → invariant technique (**6 - UC.md**) → preuve exécutable (`use_case_XX.c`). Ils guident les sessions futures lors de l'implémentation réelle.
    - un paragraphe **Points d'attention pour les UCs suivants :** permettant de tracer les points clés entre UC - Claude met à jour le tableau de CLAUDE.md section 6 en cohérence avec les sections **Points d'attention** de **6 - UC.md**. Les points d'attention résolus sont marqués explicitement dans **6 - UC.md** avec la mention **RÉSOLU UCxx** (cf modèle en section 1 de **6 - UC.md**)
  - rend la main à Tonton Marcel pour relecture, passage des tests manuels (`make manual`), validation fonctionnelle.

- **Phase 2 — Documentation Concepts Operationnels / UFR / REQ / TC / INT** :
  - Mise à jour de **Tonton**:
    - **1 - OC.md**: mise à jour des concepts opérationnels sur la base de la nouvelle section ajoutée par Claude dans **6 - UC.md**.

  - Mise à jour de **Claude**:
    - Mise à jour traçabilité dans `use_cases_XX.c` (INTENT, ADAPTED, TEST MATRIX).
    - **2 - SR.md**: UFRs, REQs, Tables fonctions  en conséquence des nouveaux contrats fonctionnels de **6 - UC.md**
    - **3 - TC.md**: Tests, Matrices, Open Items et des modifications des `use_cases_XX.c`.
  
  - **Revue §7** : analyser chaque proposition non arbitrée du UC courant — donner l'avis Claude (ACCEPTÉ / REFUSÉ / MODIFIÉ + justification) et planifier dans le tableau §6 ou clore. Déposer toute nouvelle proposition UX/fonctionnelle émergente du UC dans §7 (avec numéro P séquentiel) avant de demander l'arbitrage de Tonton Marcel.

Règle : **un UC n'est clos que lorsque les deux phases sont complètes** et §7 ne contient plus de propositions non arbitrées pour ce UC. 

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
| `src/ST.c` | `st_read_byte/word/long`, `st_write_byte/word/long` |

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
| UC8 | `edit` texte | Vue éditeur texte D2D : scroll, numéros de ligne, sélection, clipboard, sauvegarde ; `GUI_MOD_*` + `uiMods` dans `gui_event_t` ; API clipboard `gui_clipboard_set/get_text` ; BUG-10 LF-only fix (`bEndOfBuf`) ; `edit backup [on\|off]` + `_YYYYMMDD_HHMMSS.bak` (UFR-EDT-007) | ✓ VALIDÉ 2026-06-02 + bak 2026-06-11 |
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
| UC19 | `umount` | Démontage + dialog save .st/.msa/répertoire (P35) ; status bar mount (P39) ; progression copie (P40) | ✓ VALIDÉ 2026-06-07 |
| UC20 | `image` | Création .st / .msa depuis le contenu monté ; Enter → hex dans mount (P41) ; `mount_make_bootable()` touche F (P37 write) | ✓ VALIDÉ 2026-06-07 |
| UC20A | `st2msa`, `msa2st` | Conversion batch .st↔.msa (P42) : `--all` traite tous les fichiers du répertoire `dir`, `--dir <path>` répertoire explicite, `-r` récursif sous-répertoires | ✓ VALIDÉ 2026-06-07 |
| UC21 | interne | CPU 68000 : registres + MOVE/MOVEQ/LEA/CLR/SWAP | ✓ VALIDÉ 2026-06-07 |
| UC22 | interne | CPU 68000 : ADD/SUB/CMP/AND/OR/EOR/shifts | ✓ VALIDÉ 2026-06-08 |
| UC23 | interne | CPU 68000 : BRA/BSR/Bcc(14 cond) + NOP/STOP/RTE/RTS/RTR/TRAP/LINK/UNLK/JSR/JMP + Scc/DBcc + cpu_raise_exception | ✓ VALIDÉ 2026-06-08 |
| UC23-bis | interne | CPU 68000 : MOVEM (save/restore registres, -(An)/(An)+) + ADDA.W/SUBA.W | ✓ VALIDÉ 2026-06-08 |
| UC24 | interne | Memory map ST + registres HW stubs (Shifter, MFP, YM2149) | ✓ VALIDÉ 2026-06-08 |
| UC24A | interne | Sector Fingerprint Engine : vecteur 24D + cosine pondéré + DB seeds/build/load/save + signatures packers | ✓ VALIDÉ 2026-06-08 |
| UC24B | `edit` hex | **P46** — Coloration sémantique secteurs dans hex_edit : passe `sector_classify()` à l'ouverture → `aeSecType[iSectorCount]` dans `edit_hex_view_t` → teinte fond par type (boot/FAT/dir/BSS/code/packer) dans `ehex_render()`. Fichiers clés : `src/edit_hex.c`, `src/edit_hex.h`, `src/sector_analyze.h`. Aucun nouveau module. | ✓ VALIDÉ 2026-06-08 |
| UC24C | `edit` hex | **P47+P48** — JSON annotation (`<basename>.json` colocalisé avec `.st`) + bandeau contextuel : nouveau module `src/image_annot.c`/`.h` (`image_annot_load/save`), champ `labels[]`/`labeled_sectors[]` ; bandeau `HEXED_BAND_H` px en bas de hex_edit (`edit_hex_render_band()`) : type/score secteur courant, BPB décodé si valide (FAT1/FAT2/root dir/data LBA), zone notes éditable CTRL+S → JSON. Dépend de UC24B. | ✓ VALIDÉ 2026-06-09 |
| UC24D | `edit` hex | **P49** — Labels cliquables dans le bandeau → navigation adresse : labels JSON + auto-labels BPB (`FAT1`, `FAT2`, `Root dir`, `Data`) cliquables → `edit_hex_set_cursor_pos(lba*512+offset)`. Dépend de UC24C. | ✓ VALIDÉ 2026-06-09 |
| UC24E | console | **P50/P51/P52/P54/P55/P56** — Évolutions console : `dir --select` headless + commande `script <file>` interactive + CTRL+O→mount + `edit --hex/-h` force hex + suppression `umount` (remplacé par image/ESC) + `image --dir` extraction image→répertoire | ✓ VALIDÉ 2026-06-10 |
| UC24F | `mount` | **P53** — Navigation arborescente vue mount : `image_st_mkdir()` + `image_st_list_dir()` + `image_st_write_file_in_dir()` + récursivité `mount_dir_cb()` 1 niveau + ENTER/LEFT nav + extraction subdirs `mount_save_image()` | ✓ VALIDÉ 2026-06-10 |
| UC24G | `trace`, `image`, `dir` | **P57** `trace level all\|info\|error\|todo` incrémental + **P58** `image --in/--out` explicites + **P60** sélection simple/multi exclusives dans vue dir — Phase 2 : DOC-01 (UFR-CON-099..105→SW reqs) + DOC-02 (§1.5 SR.md→derived reqs) | ✓ VALIDÉ 2026-06-10 |
| UC25A | `execute` | Moteur d'exécution pas-à-pas `exec.c` (thread CPU + mutex + BP×8 + STEP/RUN/PAUSE/STOP) + `exec_mon.c` (vue monitor F5/F6/F7/F8/B/C) + `exec_cpu.c` (D0-D7/A0-A7/PC/SSP/SR) + `line_cmd_execute()` | ✓ VALIDÉ 2026-06-11 |
| UC25B | `execute` | Vue mémoire `exec_mem.c` (dump RAM paginé + navigation) + vue désassembleur `exec_asm.c` (colonnes hex+disasm synchronisées avec PC highlight) | ✓ VALIDÉ 2026-06-11 |
| UC26 | interne | Shifter : moteur rendu bitplanes→RGB32 (low/med/high res, palette 16 couleurs, `shifter_render()`) | ✓ VALIDÉ 2026-06-11 |
| UC27 | `execute` | Vue écran D2D `exec_screen.c` (640×400) + `renderer_platform_draw_bitmap()` BGRA nearest-neighbour + aspect-ratio scale + `hScrWnd` dans exec_state_t | ✓ VALIDÉ 2026-06-11 |
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

### Arbitrage UC15 (2026-06-03)

*UC15 est un UC de validation interne (load .PRG in ST Memory). Aucune proposition UX/fonctionnelle n'a émergé — UC15 est clos.*

---

### Arbitrage UC15A (2026-06-06)

*UC15A est un UC de validation interne (torture test désassembleur). Aucune proposition UX/fonctionnelle n'a émergé — UC15A est clos.*

---

### Arbitrage UC16 (2026-06-04)

*UC16 est un UC purement interne (FAT12 .st image). Aucune proposition UX/fonctionnelle n'a émergé — UC16 est clos.*

---

### Arbitrage UC17 (2026-06-06)

*UC17 est un UC purement interne (codec MSA RLE). Aucune proposition UX/fonctionnelle n'a émergé — UC17 est clos.*

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

---

### Arbitrage UC19 (2026-06-07)

*UC19 implémente P35/P39/P40. Propositions P41 et Bootable-write déjà planifiées à UC20. Aucune nouvelle proposition UX/fonctionnelle n'a émergé — UC19 est clos.*

---

### Arbitrage UC20 (2026-06-07)

*UC20 implémente P41 (ENTER → hex), P37-write (F → bootable), et la commande `image`. Aucune nouvelle proposition UX/fonctionnelle n'a émergé — UC20 est clos.*

---

### Arbitrage UC20A (2026-06-07)

**P42 — Commandes batch `st2msa` / `msa2st`** → **ACCEPTÉ — UC20A**

Conversion en lot d'images disque dans un répertoire, en exploitant directement `image_st_load/save` et `image_msa_load/save` (UC16/UC17, déjà validés). Périmètre :
- `st2msa [--dir <path>] [--all] [-r]` : convertit les fichiers `.st` trouvés en `.msa` du même nom dans le même répertoire.
- `msa2st [--dir <path>] [--all] [-r]` : inverse.
- Sans `--dir` : utilise le répertoire courant de `dir` (via `line_get_selected` ou `szCwd`).
- `--all` : traite tous les fichiers du type source trouvés (sans `--all`, ne traite que le fichier sélectionné).
- `-r` : descend récursivement dans les sous-répertoires via `file_list_dir()` en boucle.
- Option `--zip` : **EXCLUE** — hors scope ATARI ST ; un script bash externe gère l'extraction des archives ZIP.

| Proposition | Décision | UC cible |
|-------------|----------|----------|
| P42 (st2msa / msa2st batch) | ACCEPTÉ (sans --zip) | UC20A |

*Proposition P42 arbitrée — section §7 à jour.*

---

### Arbitrage UC21 (2026-06-07)

*UC21 est un UC purement interne (émulateur CPU 68000). Aucune proposition UX/fonctionnelle n'a émergé — UC21 est clos.*

---

### Arbitrage UC23 (2026-06-08)

**P43 — MOVEM : save/restore registres (manquant pour vrais programmes assembleur)** → **ACCEPTÉ — UC23-bis**

Avis Claude : MOVEM.L (sauvegarde/restauration de listes de registres sur la pile) n'est pas dans le périmètre UC23 mais est nécessaire dès que le désassembleur ou l'émulateur rencontre du code C compilé ou de l'assembleur DEVPAC standard (prologue/épilogue de fonction). Coût moyen (bitmask 16 bits, loop sur 8 Dn + 8 An, modes 4 et 3). Planifié UC23-bis.

**P44 — ADDA.W / SUBA.W : sign-extension word manquante sur address registers** → **ACCEPTÉ — UC23-bis**

Les handlers groupD/group9 gèrent déjà sz=3 → ADDA.L. La forme ADDA.W (sz=2 pour les An) n'est pas encore implémentée. Coût faible (un bloc sz==2 dans groupD/group9 quand iMode==1). Planifié UC23-bis avec MOVEM.

| Proposition | Décision | UC cible |
|-------------|----------|----------|
| P43 (MOVEM) | ACCEPTÉ | UC23-bis |
| P44 (ADDA.W/SUBA.W) | ACCEPTÉ | UC23-bis |

*Propositions P43/P44 arbitrées — UC23 est clos.*

---

### Propositions UC24B — Hex viewer sémantique (2026-06-08)

**P46 — Coloration sémantique par secteur dans la vue hex_edit** → **À ARBITRER**

Contexte : UC24A dispose maintenant d'un moteur de classification de secteurs
(`sector_classify()`). La vue hex_edit affiche les octets bruts sans aucun repère
sémantique. Proposition : à l'ouverture d'une image disque dans hex_edit, passer
tous les secteurs en revue via `sector_classify()` (une seule passe, résultats
cachés dans un tableau `aeType[iSectorCount]`), puis teinter chaque ligne de 16
octets avec une couleur de fond discrète selon le type de secteur :

| Type UC24A        | Couleur fond (suggestion) |
|-------------------|--------------------------|
| `BOOTSECTOR_*`    | violet foncé             |
| `FAT12`           | bleu foncé               |
| `DIRECTORY_*`     | teal                     |
| `BSS_ZERO`        | gris très foncé          |
| `UNFORMATTED`     | rouge brique sombre      |
| `CODE_DEMO`       | vert foncé               |
| `PROTECTION`      | orange brique            |
| `DATA_*`          | beige/ocre               |

La couleur se superpose discrètement au fond blanc standard — la lisibilité des
octets et de l'ASCII reste prioritaire (alpha ~30-40%). Le calcul est incrémental
(1 classify par secteur de 512 octets = ~5 µs, 1440 secteurs = < 10 ms total).

Avis Claude : **ACCEPTÉ** — coût faible, valeur pédagogique élevée. Un seul nouveau
champ `aeSecType[ST_MSA_MAX_SECTORS]` dans `edit_hex_view_t` + teinte dans la boucle
de rendu `edit_hex_render_row()`. Ne nécessite pas P47/P48. Cible naturelle : **UC24B**.

**P47 — Fichier JSON d'annotation colocalisé avec l'image disque** → **À ARBITRER**

Proposition : un fichier `<basename>.json` colocalisé avec le `.st` (même répertoire,
même nom de base) stocke l'annotation sémantique persistante de l'image. Structure
conforme au modèle `db/seeds/whatisit.json` déjà posé :

```json
{
  "filename": "image.st",
  "sha256": "...",
  "disk_type": "demo|app|tool|unknown",
  "origin": "...",
  "geometry": { "heads": 2, "spt": 9, "tracks": 80 },
  "notes": "...",
  "boot": { "boots_on_real_st": false, "packer_name": "", ... },
  "labeled_sectors": [
    { "lba": 0, "type": "bootsector_noboot", "notes": "..." },
    { "lba": 1, "type": "fat12",            "notes": "..." }
  ],
  "labels": [
    { "lba": 0, "offset": 0,  "name": "bpb_bps",       "desc": "Bytes per sector" },
    { "lba": 0, "offset": 11, "name": "root_dir_start", "desc": "First root dir LBA" }
  ]
}
```

**Correction de la contrainte de répertoire** : la proposition initiale limitait le
JSON au répertoire `use_cases/UC20A/st/`. Avis Claude : cette contrainte est trop
rigide — le JSON doit simplement être colocalisé avec le `.st` (convention
`<basename>.json`), quel que soit le répertoire. Cela fonctionne partout où
`edit_hex_open()` est appelé. Nouveau module `src/image_annot.c` / `image_annot.h`
avec `image_annot_load()` / `image_annot_save()` (JSON text, pas binaire).

Le JSON est optionnel : s'il est absent, hex_edit fonctionne normalement. S'il est
présent, il enrichit la vue. L'utilisateur peut créer/modifier le JSON manuellement
ou via le bandeau (P48).

Avis Claude : **ACCEPTÉ MODIFIÉ** (contrainte répertoire assouplie). Coût moyen
(parser JSON minimaliste à écrire, ou réutiliser un include C99 single-header comme
`jsmn.h` ~400 lignes). Cible naturelle : **UC24B** (base nécessaire pour P48/P49).

**P48 — Bandeau contextuel secteur dans la vue hex_edit** → **À ARBITRER**

Proposition : un panneau de contexte permanent (bandeau horizontal en bas ou colonne
droite de la fenêtre hex_edit) affiche les informations sémantiques du secteur
courant, mis à jour à chaque déplacement du curseur quand le secteur change :

- **Bandeau secteur courant** : numéro LBA, type UC24A (ex : `FAT12`), score,
  notes du JSON si disponible.
- **Bandeau bootsector** (permanent si BPB valide) : champs BPB décodés — BPS, SPC,
  NFAT, TS, SPT, HDS, root dir capacity, FAT1 LBA, FAT2 LBA, root dir LBA,
  first data LBA, total clusters. Ces valeurs permettent de naviguer
  logiquement dans l'image sans calculer les offsets à la main.
- **Champ "notes" éditable** : zone de texte dans le bandeau pour éditer la note du
  secteur courant. CTRL+S sauvegarde dans le JSON (P47). Ainsi l'annotation du JSON
  devient un jeu d'enfant pendant l'analyse.

Le bandeau rend `edit_hex_view_t` plus "IDE d'analyse" que simple visualisateur.
Implémentation : `HEXED_BAND_H` pixels réservés en bas de la fenêtre, rendu par
`edit_hex_render_band()`.

Avis Claude : **ACCEPTÉ** — valeur pédagogique et pratique maximale. Le bandeau BPB
décodé élimine le besoin de calculer `first_data_lba = fat_lba + nfat*spf + 14` à
la main. Dépend de P47 (JSON) pour les notes éditables. Coût moyen. Cible : **UC24B**.

**P49 — Labels cliquables dans le bandeau → navigation par adresse** → **À ARBITRER**

Proposition : les labels définis dans le JSON (champ `"labels"` de P47) s'affichent
dans le bandeau (P48) sous forme de liens cliquables. Cliquer sur un label déplace
le curseur hex à l'adresse correspondante (`lba * 512 + offset`).

Pour le bootsector BPB valide, les labels BPB standard sont générés automatiquement
sans intervention JSON (ex : clic sur "root dir" → jump à `root_dir_lba * 512`) :

| Label auto (BPB valide) | Adresse calculée |
|-------------------------|-----------------|
| `FAT1`                  | `fat1_lba * 512` |
| `FAT2`                  | `fat2_lba * 512` |
| `Root dir`              | `root_dir_lba * 512` |
| `Data cluster 2`        | `data_start_lba * 512` |

Les labels custom du JSON s'ajoutent à la liste auto. Un clic sur une entrée
`labeled_sectors` dans le bandeau saute au début de ce secteur.

Ce mécanisme transforme l'image disque en un document navigable, analogue à un
debugger source-level mais pour le binaire brut d'une disquette.

Avis Claude : **ACCEPTÉ** — coût faible une fois P47/P48 en place (event click →
calcul offset → `edit_hex_set_cursor_pos()`). Cible : **UC24B** (après P47/P48).

| Proposition | Décision provisoire | UC cible proposée |
|-------------|---------------------|-------------------|
| P46 (coloration sémantique secteurs)     | ACCEPTÉ — **IMPLÉMENTÉ UC24B** | ✓ clos |
| P47 (JSON annotation colocalisé)         | ACCEPTÉ — **IMPLÉMENTÉ UC24C** | ✓ clos |
| P48 (bandeau contextuel secteur/BPB)     | ACCEPTÉ — **IMPLÉMENTÉ UC24C** | ✓ clos |
| P49 (labels cliquables → navigation)     | ACCEPTÉ — **IMPLÉMENTÉ UC24D** | ✓ clos |

**Découpage contexte conversation** (décidé 2026-06-08) :
- **UC24B** : P46 seul — ~150 lignes, zéro nouveau module. Conversation légère.
- **UC24C** : P47+P48 couplés — nouveau module `image_annot.c` + rendu bandeau. Conversation moyenne.
- **UC24D** : P49 seul — navigation click, s'appuie sur UC24C validé. Conversation légère.

**Points d'entrée pour démarrage à froid** :
- API classification disponible : `sector_classify()` dans `src/sector_analysis.h`
- Vue hex actuelle : `src/edit_hex.c` / `src/edit_hex.h` — struct `edit_hex_view_t`
- Modèle JSON de référence : `db/seeds/whatisit.json` (format déjà défini et documenté)
- Dépendance UC24B→C→D : séquentielle (chaque UC valide avant le suivant)

*Propositions P46–P49 déposées et découpées, en attente d'arbitrage de Tonton Marcel.*

---

### Arbitrage UC20A post-validation (2026-06-08)

**P45 — Support des formats MSA étendus (spt > 9, tracks > 80)** → **À ARBITRER**

Contexte : `msa2st --all` sur un répertoire de test a révélé que 3 des 4 fichiers MSA
ne se convertissent pas. Analyse header :

| Fichier | spt | tracks | Taille | Résultat |
|---------|-----|--------|--------|----------|
| 7.msa  | 10  | 81     | 829 Ko | REJETÉ   |
| 8.msa  | 10  | 81     | 829 Ko | REJETÉ   |
| 9.msa  | 10  | 82     | 840 Ko | REJETÉ   |
| 10.msa | 9   | 80     | 720 Ko | OK ✓     |

Les 3 fichiers sont des MSA valides en format **étendu ATARI ST** (10 spt × 2 sides ×
81-82 tracks), utilisé par certaines démos et utilitaires exploitant la capacité
maximale du WD1772. ST4Ever ne les supporte pas car `image_st_t` a un buffer fixe
de 737 280 octets (standard DD : 9×2×80×512).

**Correction appliquée (Option A)** : message d'erreur explicite dans
`image_msa_load()` — "extended format not supported spt=10 tracks=81 needs=829440
bytes (max IST_DISK_SIZE=737280)" — à la place de l'ancien "bad geometry" générique.

**Correction complète (Option B)** : rendre `image_st_t` à géométrie variable
(buffer dynamique, champs spt/tracks/sides). Touche `image_st.h/.c`, `image_msa.c`,
et tous les consommateurs. Scope significatif.

Avis Claude : **DIFFÉRÉ**. Si la démo cible UC31 est en format standard (9 spt,
80 tracks — c'est le cas de la quasi-totalité des démos ATARI ST connues), ce UC
n'est pas bloquant avant UC31. Si une démo cible exige le format étendu, activer
l'Option B avant UC31. À confirmer lors du choix de la démo en UC31.

---

### Bugs et évolutions 2026-06-10 — Revue console + 2-SR.md §1.1

*(Tonton Marcel a mis à jour 1-OC.md suite à la revue de 2-SR.md §1.1.)*

#### Bugs corrigés (implémenter immédiatement)

**BUG-01 — `trace level info` émettait un warning parasite** → **CORRIGÉ**

La vérification générique `iArgc > 2` dans `line_cmd_trace()` se déclenchait avant
le branchement `level`, qui prend légitimement un 3ème argument. Fix : exclure
`level` et `clear` de cette garde (les deux branches gèrent déjà leur propre
vérification d'arité).

**BUG-02 — `image --msa` sur un fichier `.st` créait un `.st`** → **CORRIGÉ**

`line_cmd_image()` ignorait les fichiers `.st`/`.msa` sélectionnés et ne proposait
que le Case "open mount" ou "directory→image". Ajout du Case 2 : si la sélection
(ou l'argument) est un fichier `.st` ou `.msa` existant, le charger comme source
et le sauvegarder au format cible (extension remplacée automatiquement).
Comportement : `image --msa path/xx.st` → `path/xx.msa` ; `image --st path/xx.msa`
→ `path/xx.st`. Distinct des commandes `st2msa`/`msa2st` (batch sur répertoire).

**BUG-03 — Noms de fichiers avec espaces non fonctionnels en auto-complétion** → **CORRIGÉ**

Deux problèmes liés :

1. *Parser* (`line_parse_cmd`) : `strtok_r` découpait sur tous les espaces sans
   reconnaître l'échappement `\ `. Remplacé par `line_next_token_unescape()` qui
   traite `\ `, `\(`, `\)` comme des littéraux dans un token.

2. *Auto-complétion TAB* :
   - Le scan de token cherchant `uiTokenStart` ne distinguait pas l'espace unescapé
     (séparateur) de l'espace escapé `\ ` (partie du nom). Corrigé.
   - `line_complete_path_query()` retournait des noms bruts avec espaces, provoquant
     une confusion à l'insertion. Ajout de `line_escape_path()` : les espaces et
     parenthèses sont escapés dans les candidats retournés. Le prefix entrant est
     d'abord unescapé via `line_unescape_path()` pour la comparaison filesystem.
   - Résultat : `load My` + TAB → `load My\ File.prg` (suffixe `\ File.prg`
     inséré, puis unescape dans le parser → argv[1] = `"My File.prg"`).

---

#### Propositions nouvelles — À arbitrer par Tonton Marcel

**P50 — `dir --select <path>` : sélectionner un fichier/répertoire sans GUI** → **À ARBITRER**

Permet de sélectionner directement un fichier ou répertoire depuis la console sans
ouvrir de fenêtre GUI. Utile dans les scripts (`--script`) pour pré-sélectionner une
cible avant un `load`, `mount`, `image`, etc.
`dir --select path/to/file.prg` → équivalent à naviguer et appuyer ESPACE dans la
vue dir, mais en mode headless.

Avis Claude : **ACCEPTÉ** — coût faible : `line_cmd_dir()` détecte `--select`,
appelle `line_set_selected()` directement sans `dir_open()`. Aucun nouveau module.
Très utile pour les tests automatisés via `--script`.

**P51 — Commande `script <file>` depuis une session interactive** → **À ARBITRER**

Actuellement, l'exécution d'un script nécessite de lancer ST4Ever avec
`ST4Ever --script <file>` depuis l'extérieur. P51 ajoute la commande `script
<file>` utilisable depuis une session active, sans quitter l'application.
Comportement : ouvre `<file>`, exécute chaque ligne comme une commande console,
affiche les retours, puis rend la main à l'utilisateur. En cas d'erreur sur une
ligne : log + continue (pas d'arrêt du script).

Avis Claude : **ACCEPTÉ** — `--script` existe déjà dans `line_run_script()` (UC4.3).
La commande `script <file>` réutilise cette fonction directement depuis
`line_cmd_script()`. Coût minimal. Planifier dans le prochain UC console (potentiellement UC5-bis ou UC25-pre).

**P52 — Raccourci CTRL+O pour ouvrir le mount** → **À ARBITRER**

CTRL+M est déjà utilisé (MOVE terminal standard). CTRL+O (`Open`) est libre.
Comportement : CTRL+O dans la console → `mount_open` sur la sélection courante
(même effet que taper `mount` + ENTER).

Avis Claude : **ACCEPTÉ** — même pattern que les autres raccourcis CTRL (CTRL+D→dir,
CTRL+L→load, CTRL+E→edit). Un seul `case CON_KEY_CTRL_O` dans la boucle
`line_read_rich()` → `line_shortcut(ptCtx, szBuf, "mount")`. Coût nul.
Planifiable dès le prochain UC qui touche `line_read_rich()`.

**P53 — Navigation arborescente dans la vue mount (répertoire local + sous-répertoires FAT12)** → **À ARBITRER**

UFR-CON-070 mentionne "from a given directory". Deux problèmes confirmés par Tonton
Marcel le 2026-06-10 :

1. **`mount ./répertoire` incomplet** : `mount_dir_cb()` ligne ~850 ignore
   silencieusement tous les sous-répertoires locaux (`if (bIsDir) return;`).
   Résultat : `mount ./linux` n'affiche que les fichiers situés à la racine du
   répertoire (sous-répertoires absents).

2. **Images `.st` avec sous-répertoires non navigables** : `image_st_list_root()`
   retourne les entrées FAT12 du root dir, y compris les dossiers, mais ceux-ci
   apparaissent comme des fichiers de 0 octet dans la vue mount sans possibilité
   de les ouvrir ou naviguer dedans.

**P53 étendu** — proposition révisée : navigation arborescente dans la vue mount
avec le même principe +/- que la vue `dir` :

*Côté local → image (.st création)* :
- Lors du montage d'un répertoire local, `mount_dir_cb` crée les sous-répertoires
  FAT12 correspondants via `image_st_mkdir()` (à créer) et y place récursivement
  les fichiers enfants. Les noms sont tronqués au format 8.3 ATARI. Alerte si un
  fichier dépasse 512 Ko ou si le nombre total d'entrées FAT12 est insuffisant.

*Côté image .st → navigation* :
- Les entrées de répertoire FAT12 (attr `IST_ATTR_DIR`) dans la liste mount sont
  affichables comme nœuds expansibles. Un clic ou ENTER sur un dossier navigue
  dedans (nouveau `image_st_list_dir(ptImg, szDirPath, ...)` à créer). Un raccourci
  `←`/backspace remonte au niveau parent (chemin courant conservé dans `mount_view_t`).
- Les champs BPB dans le panneau propriétés restent valides (inchangés) ; les
  sous-répertoires n'ont pas de BPB propre.

*État BPB sur montage depuis répertoire local* :
- Les champs BPB standard (head/spt/tracks) sont affichés comme `—` car non définis
  par un bootsector réel. Le reste du panneau (taille, espace libre, dirty) est
  calculé normalement.

Avis Claude : **ACCEPTÉ — coût significatif**. Nécessite `image_st_mkdir()` +
`image_st_list_dir()` dans `image_st.c`, modification `mount_dir_cb()` (récursivité),
et extension `mount_view_t` (chemin courant + pile de navigation). À planifier en
**UC-mount-tree** (nouveau UC après UC24E, avant UC25).

**P54 — `edit --hex <file>` force l'ouverture en hexadécimal** → **À ARBITRER**

Permet d'ouvrir n'importe quel fichier binaire (dump mémoire, `.txt`, etc.) en vue
hex, sans détection automatique du type. Déjà noté P33 (arbitré ACCEPTÉ, cible
UC10-bis). La numérotation P54 est une ré-émergence confirmant la priorité.

Avis Claude : **ACCEPTÉ — DÉJÀ ARBITRÉ sous P33**. Vérifier si implémenté dans UC10.
Si non, reporter en UC25-pre ou UC26-pre. Un flag `-h`/`--hex` dans `line_cmd_edit()`
force `edit_hex_open()` quel que soit le type détecté.

**P55 — Supprimer la commande `umount` : fermeture de la fenêtre mount suffit** → **ACCEPTÉ**

Arbitrage révisé 2026-06-10 (Tonton Marcel) : `mount` dans un script n'a aucun sens
actuellement — il n'existe pas de commande console permettant de manipuler le
contenu d'une image de façon headless (ajout/suppression de fichiers dans l'image).
Dès lors, `umount --st/--msa/--dir` dans un script n'aurait d'intérêt que si le
script pouvait d'abord modifier l'image via mount, ce qui n'est pas le cas.

Avec P56 (`image --dir`), toutes les opérations batch de conversion sont couvertes
sans ouvrir de vue mount :
- Créer image .st/msa depuis répertoire → `image --st mydir/` ou `image --msa mydir/`
- Convertir .st ↔ .msa → `image --msa x.st` / `image --st x.msa`
- Extraire image → répertoire → `image --dir x.st` (P56)

La commande `umount` est donc supprimée. ESC ou WM_CLOSE sur la fenêtre mount
déclenche déjà la boîte de dialogue save (si dirty). La commande `help` est mise à
jour. Le raccourci CTRL+U dans la console est libéré.

Planifié UC24E (même UC que P50/P52/P56 — coût nul : simple suppression de
l'entrée CMD_UMOUNT dans la table des commandes + mise à jour `help`).

**P56 — `image --dir [<folder>] [<path/xx.st|msa>]` : extraire image → répertoire local** → **À ARBITRER**

Complément de la commande `image` pour l'opération inverse : extraire le contenu
d'une image `.st` ou `.msa` dans un répertoire local. Sans argument folder, le
répertoire cible est `./disk` (ou `./disk[N]` si `disk` existe déjà). Exemple :
`image --dir path/xx.st` → crée `./disk/` contenant tous les fichiers de l'image.
`image --dir myfolder path/xx.msa` → crée `myfolder/`.

Avis Claude : **ACCEPTÉ** — `mount_save_image(..., MOUNT_SAVE_DIR, szDstDir)` existe
déjà (UC19). Il suffit d'ajouter le flag `--dir` dans `line_cmd_image()` et de gérer
la numérotation `disk[N]`. Coût faible. Peut s'intégrer dans le même UC que P50/P51
ou dans un UC console dédié.

---

#### Point documentaire (action SR.md)

**DOC — UFR-MNT-010 à reclasser en UFR-CON-075** → **TRAITÉ**

UFR-MNT-010 existait dans l'archivé `SRTD.md` et dans `3 - TC.md`. L'équivalent
fonctionnel UFR-CON-075 est déjà présent dans `2 - SR.md` §1.1 (UC20). La
référence dans `3 - TC.md` (tableau UFR traceability UC20) a été mise à jour de
`UFR-MNT-010` → `UFR-CON-075`. Aucune modification de `use_case_20.c` requise.

| Proposition | Avis Claude | UC cible proposée |
|-------------|-------------|-------------------|
| BUG-01 (trace level warning)       | CORRIGÉ 2026-06-10 | ✓ clos |
| BUG-02 (image --msa .st→.msa)      | CORRIGÉ 2026-06-10 | ✓ clos |
| BUG-03 (espaces auto-complétion)   | CORRIGÉ 2026-06-10 | ✓ clos |
| P50 (dir --select headless)        | IMPLÉMENTÉ UC24E | ✓ clos |
| P51 (commande script interactive)  | IMPLÉMENTÉ UC24E | ✓ clos |
| P52 (CTRL+O → mount)               | IMPLÉMENTÉ UC24E | ✓ clos |
| P53 (navigation arborescente mount)| ACCEPTÉ — coût signif. | UC-mount-tree |
| P54 (edit --hex) = P33             | IMPLÉMENTÉ UC24E | ✓ clos |
| P55 (supprimer umount)             | IMPLÉMENTÉ UC24E | ✓ clos |
| P56 (image --dir extraction)       | IMPLÉMENTÉ UC24E | ✓ clos |
| DOC UFR-MNT-010→UFR-CON-075        | TRAITÉ 2026-06-10 | ✓ clos |

---

### Arbitrage UC24E (2026-06-10)

*Toutes les propositions P50/P51/P52/P54/P55/P56 ont été implémentées et validées
dans UC24E. La proposition P53 (navigation arborescente mount) a été acceptée avec
coût significatif et planifiée en UC-mount-tree. Aucune nouvelle proposition
UX/fonctionnelle n'a émergé pendant l'implémentation — UC24E est clos.*

**P50 — `dir --select <path>` headless** → **IMPLÉMENTÉ — UC24E**

`line_cmd_dir()` détecte `--select`, valide le chemin via `file_stat()`,
appelle `line_set_selected()` directement sans `dir_open()`. Utile dans les
scripts batch pour pré-sélectionner une cible.

**P51 — Commande `script <file>` interactive** → **IMPLÉMENTÉ — UC24E**

`line_exec_script()` (refactorisation de `line_run_script()`) est réutilisée
par `line_cmd_script()`. Alias `r`. En cas d'erreur sur une ligne : LOG_ERROR +
continue (pas d'arrêt). Forward declaration ajoutée pour satisfaire C89.

**P52 — CTRL+O → mount** → **IMPLÉMENTÉ — UC24E**

`CON_KEY_CTRL_O 0x0F` dans `console.h` ; remplace `CON_KEY_CTRL_U` (supprimé
avec P55). Case dans `line_read_rich()` → `line_shortcut(ptCtx, szBuf, "mount")`.

**P54 — `edit -h` / `edit --hex` force hex** → **IMPLÉMENTÉ — UC24E**

Flag `bForceHex` dans `line_cmd_edit()`. Avec `-h`/`--hex`, `edit_hex_open()`
est appelé quel que soit le type détecté par `file_stat()`.

**P55 — Supprimer la commande `umount`** → **IMPLÉMENTÉ — UC24E**

`CMD_UMOUNT` supprimé de `cmd_id_t`, `line_cmd_umount()` (~175 lignes) supprimée,
entrée retirée de `g_line_aCmds[]` et `g_line_aHandlers[]`. ESC / WM_CLOSE
sur la fenêtre mount déclenche déjà la boîte de dialogue save.

**P56 — `image --dir [folder]` extraction image → répertoire** → **IMPLÉMENTÉ — UC24E**

Flag `bExtractDir` dans `line_cmd_image()`. Source : mount view ouverte ou
fichier `.st`/`.msa` en sélection/argument. Répertoire cible : `disk` →
`disk2` … → `disk99` (auto-numérotation). Utilise `mount_view_open()` +
`mount_save_image(MOUNT_SAVE_DIR, szDstDir)`.

---

### Arbitrage UC24F (2026-06-10)

*UC24F implémente P53 (navigation arborescente mount). Les bugs et évolutions
ci-dessous ont émergé lors de la revue UC24E/UC24F — corrections appliquées
2026-06-10, 0 régression.*

#### Bugs corrigés UC24E/UC24F (2026-06-10)

**BUG-04 — `mount ./linux` : seulement 2 fichiers sur 4 visibles** → **CORRIGÉ**

Cause : les fichiers `lx_console.c` et `lx_platform.c` ont un nom de base > 8
caractères (contrainte FAT12 8.3). Ils sont silencieusement ignorés par
`mount_dir_cb()`. Fix : champ `iNotImported` ajouté à `mount_view_t` ; panneau
propriétés affiche `Skipped: N (8.3 limit)` en orange quand N > 0. Même
comportement pour `mount ./use_cases/test` (fichiers `.st`/`.msa` trop grands).

**BUG-05 — Geometry T1440 au lieu de T80** → **CORRIGÉ**

BPB offset 0x13 = `Total Sectors` (1440) et non `Tracks`. Fix : lire comme
`uiTS`, calculer `uiTracks = uiTS / (uiSPT × uiHeads)` = 1440 / 18 = 80.

**BUG-06 — Geometry affiche H2/S9/T1440 sur mount répertoire local** → **CORRIGÉ**

P53 spécifiait `—` pour la géométrie sur montage de répertoire local (pas de
bootsector réel). Fix : lorsque `ptView->eSrc == MOUNT_SRC_DIR`, les lignes
Geometry et Bootable affichent `—` au lieu des valeurs BPB synthétiques.

**BUG-07 — `image --dir path/xx.st` : xx.st traité comme répertoire de sortie** → **CORRIGÉ**

Fix : dans l'analyse des arguments de `--dir`, si le token suivant se termine
en `.st`/`.msa`, il n'est PAS consommé comme `szExtractDst` mais laissé pour
être capturé comme `szOutPath` (source) par la suite du parseur.

**BUG-08 — ESPACE dans vue dir : surlignage vert non immédiat** → **CORRIGÉ**

`bRedraw = ST_TRUE` était absent du handler SPACE simple dans
`dir_handle_key()`. Ajouté — le fond vert (P11 szLastSelected) s'affiche
dès l'appui barre espace, sans attendre le déplacement du curseur.

**BUG-09 — ALT+LEFT ne fonctionne pas entre deux commandes `dir`** → **CORRIGÉ**

Chaque appel `dir` fermait l'ancien view (réinitialisant l'historique) avant
d'ouvrir le nouveau. Fix : dans `line_cmd_dir()`, l'historique du view précédent
est sauvegardé dans les statiques `g_line_aDirNavHist/Head/Count` de `line.c`,
puis restauré dans le nouveau view avec le nouveau chemin ajouté en tête. Ainsi
`dir ./use_cases` → `dir ./linux` → ALT+← revient à `./use_cases`.

---

### Propositions nouvelles UC24F — À arbitrer par Tonton Marcel

**DOC-01 — UFR-CON-099...105 devraient être en section SW requirements** → **À ARBITRER**

UFR-CON-099 (commande `umount` supprimée, donc cette UFR n'a plus de sens
utilisateur) ; UFR-CON-100..102 sont des SW reqs liés à UFR-CON-098 ;
UFR-CON-103..105 sont des SW reqs liés à UFR-CON-070. Ces exigences s'appuient
sur l'architecture logicielle, pas sur des besoins utilisateur visibles.
Avis Claude : **ACCEPTÉ** — action SR.md : supprimer UFR-CON-099 (commande
supprimée), déplacer UFR-CON-100..105 en section 2 SW requirements avec
rationale "design constraint". Traçabilité TC.md + use_cases_24[E|F].c à
mettre à jour. Impact : Phase 2 uniquement, aucune modification de code.

**DOC-02 — §1.5 de "2 - SR.md" : exigences dérivées** → **À ARBITRER**

La section §1.5 contient des exigences d'architecture/design (derived
requirements) sans lien direct avec un UFR. Avis Claude : **ACCEPTÉ** —
créer une section 2.x "Derived Requirements / Design Constraints" avec
rationale "internal abstraction" et traça UFR `—`. Impact : documentation
uniquement.

**P57 — `trace level` incrémental : [all|info|error|todo]** → **À ARBITRER**

Situation actuelle : `error` n'affiche que LOG_ERROR ; `info` affiche
INFO+TODO+ERROR ; `trace` = identique à `info` si LOG_TRACE désactivés par
défaut (P26 — ambigu). Proposition incrémentale :
- `trace level todo` → LOG_TODO uniquement (fonctions stub actives)
- `trace level error` → LOG_ERROR + LOG_TODO (debug : erreurs + rationale stubs)
- `trace level info` → LOG_INFO + LOG_ERROR + LOG_TODO (tout sauf appels fonctions)
- `trace level all` → toutes les traces (remplace `trace level trace`, plus clair)

Avis Claude : **ACCEPTÉ** — sémantique plus claire et incrémentale. `all` est
plus explicite que `trace` pour "afficher les LOG_TRACE". Coût faible :
modification de `line_cmd_trace()` et `trace_view_t.eMinLevel` enum. Cible
naturelle : **UC25-pre** ou regroupé avec un UC console futur.

**P58 — Commande `image` : --in/--out explicites** → **À ARBITRER**

Situation actuelle : `image --msa path/xx.st` est ambigu (xx.st est-il l'entrée
ou la sortie ?). BUG-02 (corrigé) et BUG-07 (corrigé) en découlent. Proposition :
- Disque monté par `mount` = source par défaut (via `info` → Disk Mounted).
- `--in` : supersède la source par défaut (`--in ./my_dir` ou `--in x.st`).
- `--out` : fichier/répertoire de sortie explicite (`--out ./yopla.msa`).
- Sans `--out` : nom par défaut `./disk.st`, `./disk.msa`, ou `./disk/`.
- Exemple complet : `image --in ./mydir --st --out ./yopla.st`.
- Forme sans option : `image --msa` = crée `./disk.msa` depuis la vue montée.
- Toute forme `image --msa path/xx.st` sans `--in`/`--out` → warning et refus.

Avis Claude : **ACCEPTÉ — coût moyen**. Refactorisation complète du parseur de
`line_cmd_image()`. Implique aussi la mise à jour de `help` et des tests UC24E.
Cible : **UC25-pre** ou UC dédié "image-v2".

**P59 — Menus contextuels clic droit dans vue `dir`** → **À ARBITRER**

TODO UC7 et TODO UC18 mentionnent des menus contextuels pour la vue `dir`
(clic droit sur fichier/répertoire : Open, Load, Edit, Mount, Copy, Delete...).
Avis Claude : **ACCEPTÉ DIFFÉRÉ** — coût élevé (nouveau sous-système menu popup
Win32/X11), cohérent avec P14 (sélection multiple) et les opérations mount.
Cible : après UC25 quand les opérations principales (load/edit/mount/execute)
seront toutes stables.

**P60 — Sélection simple et multi-sélection exclusives dans vue `dir`** → **À ARBITRER**

Comportement actuel défectueux : ESPACE sélectionne en vert, CTRL+ESPACE
commence une multi-sélection en violet sans effacer la sélection verte. Le
retour à la sélection simple (ESPACE après multi) ne retire pas les violets.
Proposition : exclusivité stricte — début de multi-sélection annule la
sélection simple (efface szLastSelected et retire le vert) ; retour à la
sélection simple par ESPACE sur une ligne sélectionnée ou ESC. Comportement
analogue à l'explorateur Windows.
Avis Claude : **ACCEPTÉ** — coût faible, cohérence UX importante. Dans
`dir_handle_key()` : au début du premier CTRL+ESPACE, effacer
`szLastSelected` + `gui_invalidate`. Au ESPACE simple quand `iMultiSelCount > 0`,
vider tout le tableau multi-sélection avant de sélectionner la nouvelle entrée.
Cible : **UC25-pre** ou regroupé avec les corrections dir.

| Proposition | Décision Tonton Marcel | UC cible |
|-------------|------------------------|----------|
| DOC-01 (UFR-CON-099..105 → SW reqs)     | ACCEPTÉ — **IMPLÉMENTÉ Phase 2 UC24G** | ✓ clos |
| DOC-02 (§1.5 SR.md → derived reqs)      | ACCEPTÉ — **IMPLÉMENTÉ Phase 2 UC24G** | ✓ clos |
| P57 (trace level all\|info\|error\|todo) | ACCEPTÉ — **IMPLÉMENTÉ UC24G** | ✓ clos |
| P58 (image --in/--out explicites)        | ACCEPTÉ — **IMPLÉMENTÉ UC24G** | ✓ clos |
| P59 (menus contextuels dir clic droit)   | DIFFÉRÉ | après UC25 |
| P60 (sélection simple/multi exclusives)  | ACCEPTÉ — **IMPLÉMENTÉ UC24G** | ✓ clos |

*Toutes les propositions P57/P58/P60 + DOC-01/DOC-02 ont été implémentées et validées dans UC24G. P59 différée. §7 ne contient plus de propositions non arbitrées pour UC24G — UC24G est clos.*

---

### Arbitrage UC25A (2026-06-11)

*UC25A est un UC de moteur d'exécution + vues GUI. Aucune proposition UX/fonctionnelle n'a émergé pendant l'implémentation — UC25A est clos (Phase 1 + Phase 2 effectuées).*

---

### Arbitrage UC25B (2026-06-11)

*UC25B implémente exec_mem.c (vue hex dump RAM) et exec_asm.c (vue désassembleur). Fix exec_open (g_exec_bOpen=TRUE avant les view opens pour que exec_get_state() fonctionne). Aucune proposition UX/fonctionnelle n'a émergé. Phase 2 effectuée : REQ-EXE-029..032 dans SR.md §2.20, TC-EXE-031..051 dans TC.md §5.93, section UC25B dans 6-UC.md, INTENT markers INT-EXE-031..051 dans use_case_25B.c — UC25B est clos (Phase 1 + Phase 2).*

---

### Arbitrage UC26 (2026-06-11)

*UC26 est un UC purement interne (moteur de rendu Shifter). Le module `shifter.h/c` fournit `shifter_render()`, `shifter_palette_to_rgb()` et `shifter_screen_size()` — consommé par exec_screen en UC27. Aucune proposition UX/fonctionnelle n'a émergé — UC26 est clos.*

---

### Arbitrage UC27 (2026-06-11)

*UC27 implémente la vue écran D2D et `renderer_platform_draw_bitmap()`. Décisions techniques notables :*
- *Format pixel : `shifter_render()` sort `0x00RRGGBB` — en mémoire little-endian bytes [B,G,R,0x00]. D2D avec `DXGI_FORMAT_B8G8R8A8_UNORM` + `D2D1_ALPHA_MODE_IGNORE` interprète exactement [B,G,R,ignoré] → couleurs correctes sans conversion.*
- *Interpolation : `D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR` préserve l'aspect pixel art ST (pas de flou).*
- *Scale : aspect-ratio preserving avec black bars (letterbox/pillarbox) lors du resize.*
- *Bitmap créé et détruit chaque PAINT : acceptable pour un émulateur éducatif (D2D coalesce les WM_PAINT).*

*Aucune proposition UX/fonctionnelle n'a émergé — UC27 est clos.*

---

## 8. NOTES à propos de l'extension : ST Revival sur Pi Zero Bare Metal

### 8.1 Vision et objectif

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

### 8.2 Relation avec les UC existants

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

### 8.3 Description détaillée des UC d'extension

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

### 8.4 Recommandations spécifiques à l'extension

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
