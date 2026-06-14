# ST4Ever — Guide de Reconstruction de Binaires Atari ST
## Manuel de Rétro-Ingénierie Structurée : .PRG, .TOS, Disques Bootables

**Projet :** ST4Ever — Émulateur pédagogique Atari ST  
**Handle :** Tonton Marcel  
**Cible :** Binaires 68000 Atari ST (1986–1993)  
**Version :** 1.1 (mis à jour 2026-06-14 — audit hooks ST4Ever réels)

---

## Avant-propos

Ce guide est à la fois un manuel méthodologique de rétro-ingénierie
pour tout programme Atari ST des années 80, et une spécification
fonctionnelle pour les Use Cases ST4Ever qui concernent l'analyse
statique et dynamique de binaires 68k.

Chaque phase décrit :
- Le **problème** à résoudre
- La **méthode** manuelle sur vrai ATARI ST
- La **fonction ST4Ever** correspondante : ce qui **existe déjà** (avec UC),
  ce qui est **à construire** (avec UC cible proposé), et la **commande
  console** ST4Ever disponible le cas échéant
- Les **patterns 68k** caractéristiques à reconnaître

Ce guide s'applique à tout binaire Atari ST : programme GEM (.PRG),
programme TOS (.TOS), accessoire de bureau (.ACC), programme AUTO
(.PRG dans /AUTO), secteur de boot, et démos demoscene.

### Convention de lecture

```
✓ UC15   = implémenté, validé dans ce Use Case
~ UC32   = partiellement couvert, à enrichir dans ce UC
✗ UC32A  = manquant, à implémenter dans ce UC proposé
```

---

## Prérequis et outillage

### Sur machine réelle
- Atari ST/STE avec cartouche debugger MonST - SidecarTridge étant un bon candidat
- Assembleur Devpac 3 pour vérification manuelle

### Outillage statique
- **ST4Ever** (ce projet) : désassembleur principal, vue hexa, annotations...
- The Atari Compendium (référence TOS/GEMDOS/AES/VDI)

---

## Vue d'ensemble de la méthode

```
Phase 0 │ Identification du type de binaire
Phase 1 │ Analyse du header GEMDOS (28 octets)
Phase 2 │ Traitement de la table de relocation
Phase 3 │ Cartographie des segments
Phase 4 │ Point d'entrée et runtime de démarrage
Phase 5 │ Identification des patterns d'appel
Phase 6 │ Reconnaissance des trap calls système
Phase 7 │ Détection des procédures et fonctions
Phase 8 │ Reconstruction du graphe de flot de contrôle
Phase 9 │ Identification du compilateur/langage source
Phase A │ Annotation sémantique et nommage
Phase B │ Cas spéciaux : disques bootables et démos
```

---

## Phase 0 — Identification du type de binaire

### Problème

Avant toute analyse, il faut savoir à quoi on a affaire. Un .PRG GEM,
un secteur de boot demoscene, et un .ACC ont des structures et des
points d'entrée radicalement différents.

### Méthode

Lire les 4 premiers octets et comparer :

| Magic bytes        | Type                          |
|--------------------|-------------------------------|
| `60 1A xx xx`      | .PRG/.TOS/.ACC — header GEMDOS standard |
| `00 FF xx xx`      | Secteur de boot Atari ST (FAT12) |
| `4E 75` au début   | Fragment de code nu (RTS immédiat = stub) |
| `60 xx` au début   | BRA court vers code réel (packer/loader) |
| `00 00 00 00`      | Secteur non-boot ou données pures |

Un binaire dont les 2 premiers octets sont `60 1A` est un exécutable
GEMDOS. Tout autre pattern indique un secteur de boot, un overlay, ou
un binaire packé.

### Cas particulier : binaires packés

La majorité des démos et jeux des années 80–90 sont compressés
(CrunchMania, ICE, UPX-68k, Automation packer...). Le décompresseur
est en tête de TEXT, se décompresse lui-même en RAM, puis saute au
vrai code. **Reconnaître ce cas dès la Phase 0 est critique** — tenter
de désassembler un binaire packé produit du bruit pur.

Pattern typique d'un packer : le segment TEXT est anormalement petit,
le BSS est anormalement grand, et les 10–20 premières instructions
écrivent en masse en mémoire avant un `JMP (An)`.

### Fonctions ST4Ever correspondantes

**Ce qui existe déjà :**

```c
/* ✓ UC7 — Détection par extension de fichier lors du chargement */
st_error_t load_file(const char *szPath);
/* → identifie .prg/.ttp/.tos → LOAD_TYPE_PRG vs LOAD_TYPE_BINARY */
/* → rejette .st/.msa (signale "use mount") */
const load_state_t *load_get_state(void);  /* eType = LOAD_TYPE_PRG/BINARY */

/* ✓ UC24A — Classification de contenu par vecteur de features 24D */
st_error_t sector_features_extract(const st_u8_t *pSector,
                                    st_u32_t uiBase,
                                    sector_features_t *ptFeat);
st_error_t sector_classify(const sector_db_t *ptDb,
                            const sector_features_t *ptFeat,
                            const st_u8_t *pSector,
                            sector_match_t *aMatches,
                            int iMaxMatches, int *piCount);
/* → détecte SECTOR_CODE_PACKER (packer), SECTOR_CODE_DEMO, SECTOR_CODE_GEMDOS */
/* → fVblInstall, fHwInContext, fPackerEntropy dans sector_features_t */
```

**Ce qui manque — à construire :**

```c
/* ✗ UC32A — Identification depuis octets bruts (pas chemin fichier) */
/* Nouveau module src/prg.h/c */
typedef enum prg_bin_type_e {
    PRG_BIN_GEMDOS,       /* magic $601A         */
    PRG_BIN_BOOT_SECTOR,  /* $00FF ou $0003 BPB  */
    PRG_BIN_PACKED,       /* BRA + densité ops   */
    PRG_BIN_RAW_CODE,     /* opcodes 68k valides */
    PRG_BIN_UNKNOWN
} prg_bin_type_t;

st_error_t prg_identify_binary(const st_u8_t *pBuf,
                                size_t uiBufLen,
                                prg_bin_type_t *peType,
                                st_u32_t *puiConfidence);
/* Heuristique : magic bytes + fOpcodeDensity (réutilise sector_features) */
```

**Commande console ST4Ever :**

```
load <file.prg>           → détecte PRG, affiche TEXT/DATA/BSS dans info
edit --hex <file.prg>     → ouvrir n'importe quel binaire en hex (P54/UC24E)
```

---

## Phase 1 — Analyse du header GEMDOS

### Problème

Le header GEMDOS (28 octets) donne les dimensions exactes des
segments et les flags de chargement. C'est la carte du territoire.

### Structure du header (offset depuis début du fichier)

```
+$00  word   Magic           : doit être $601A
+$02  long   TEXT_size       : taille du segment code en octets
+$06  long   DATA_size       : taille du segment données initialisées
+$0A  long   BSS_size        : taille du segment non initialisé
+$0E  long   SYMTAB_size     : taille table de symboles (0 si absente)
+$12  long   Reserved        : toujours 0 (réservé par Motorola)
+$16  long   Flags           : flags TPA (voir ci-dessous)
+$1A  word   ABSFLAG         : 0=relocatable, non-zero=absolu (PC-relatif)
```

Flags TPA (offset +$16) :
```
bit 0   FASTLOAD    : ne pas effacer BSS avant chargement
bit 1   TTMEMLOAD   : charger en RAM TT (Atari TT030 uniquement)
bit 2   READABLE    : segment TEXT lisible en user mode
bit 3   WRITABLE    : segment TEXT modifiable (code auto-modifiant)
```

### Ce qu'on en tire immédiatement

- `TEXT_size` : étendue exacte du code à désassembler
- `DATA_size` : zone de données initialisées, commence à `$1C + TEXT_size`
- `BSS_size` : zone de variables globales, allouée dynamiquement
- `SYMTAB_size > 0` : symboles présents → les noms de procédures sont
  récupérables directement (format DRI Object File)
- `ABSFLAG != 0` : code 100% PC-relatif, pas de table de relocation à
  traiter, tout BSR/BRA/JMP est relatif au PC

### Calcul des adresses en mémoire

GEMDOS charge le binaire à une adresse `base` variable. L'offset dans
le fichier se convertit en adresse mémoire par :
```
addr_mem = base + $1C + offset_dans_TEXT
```

Sous ST4Ever, `base = ST_LOAD_BASE = 0x0800` est visible dans les registres dès le premier
breakpoint au point d'entrée.

### Lecture de la table de symboles (format DRI)

Si `SYMTAB_size > 0`, la table est à l'offset :
```
$1C + TEXT_size + DATA_size + SYMTAB_size
```

Chaque entrée DRI fait 14 octets :
```
+0   char[8]   nom tronqué à 8 caractères (pas null-terminé si plein)
+8   word      type : $0200=TEXT, $0400=DATA, $0100=BSS, $0A00=externe
+A   long      valeur (offset depuis début du segment)
```

Pour GFA compilé avec Symbol Table activée, on trouve directement les
entrées `_PROC_NA` (7 caractères + underscore initial) avec leur
offset dans TEXT.

### Fonctions ST4Ever correspondantes

**Ce qui existe déjà :**

```c
/* ✓ UC15 — Parsing interne du header dans load.c (non exporté) */
/* load_file() valide le magic $601A, lit les 28 octets */
/* Le résultat partiel est exposé via load_state_t : */
typedef struct load_state_s {
    st_u32_t uiTextSize;   /* .text section size */
    st_u32_t uiDataSize;   /* .data section size */
    st_u32_t uiBssSize;    /* .bss  section size */
    st_u32_t uiFixupCount; /* fixups appliqués   */
    /* ⚠ MANQUANTS : uiSymSize, uiFlags, bAbsFlag */
} load_state_t;

/* ✓ UC15 — Constantes header déjà définies dans load.h */
#define ST_LOAD_BASE       0x0800u   /* adresse de chargement ST4Ever */
#define ST_PRG_MAGIC       0x601Au
#define ST_PRG_HEADER_SIZE 28u
```

**Ce qui manque — à construire :**

```c
/* ✗ UC32A — Header exposé comme structure publique (src/prg.h) */
typedef struct prg_hdr_s {
    st_u16_t uiMagic;      /* $601A                */
    st_u32_t uiTextSize;
    st_u32_t uiDataSize;
    st_u32_t uiBssSize;
    st_u32_t uiSymSize;    /* ← manquant dans load_state_t */
    st_u32_t uiReserved;
    st_u32_t uiFlags;      /* ← manquant dans load_state_t */
    st_u16_t uiAbsFlag;    /* ← manquant dans load_state_t */
} prg_hdr_t;

st_error_t prg_parse_header(const st_u8_t *pBuf,
                             size_t uiBufLen,
                             prg_hdr_t *ptHdr);

/* ✗ UC32A — Lecture table de symboles DRI */
typedef struct prg_symbol_s {
    char     szName[9];    /* 8 chars + NUL */
    st_u16_t uiType;       /* $0200=TEXT $0400=DATA $0100=BSS */
    st_u32_t uiValue;      /* offset depuis début du segment */
} prg_symbol_t;

typedef struct prg_symtab_s {
    prg_symbol_t *atSymbols;
    int           iCount;
} prg_symtab_t;

st_error_t prg_parse_symtab(const st_u8_t *pBuf,
                              const prg_hdr_t *ptHdr,
                              prg_symtab_t *ptSymtab);
st_error_t prg_symtab_destroy(prg_symtab_t *ptSymtab);
st_error_t prg_symtab_lookup(const prg_symtab_t *ptSymtab,
                               st_u32_t uiOffset,
                               const prg_symbol_t **pptSym);
```

**Commande console ST4Ever :**

```
load <file.prg>    → charge + info affiche TEXT/DATA/BSS/fixups
info               → dashboard : taille binaire chargé, adresse RAM
```

---

## Phase 2 — Traitement de la table de relocation

### Problème

Si le binaire est relocatable (ABSFLAG = 0), GEMDOS patche certaines
adresses longword dans le code et les données après chargement. Sans
ce traitement, le désassembleur interprétera des adresses relatives
comme des opcodes, produisant du nonsense.

### Localisation de la table

```
offset_reloc = $1C + TEXT_size + DATA_size + SYMTAB_size
```

### Format de la table de relocation Atari ST

```
long    premier_offset   relatif au début de TEXT (0 si table vide)
─ puis une suite de bytes ─
$00     fin de table
$01     avancer de 2 octets (offset courant += 2)
autre   avancer de ce nombre d'octets (doit être pair)
```

Chaque offset pointé est un longword dans TEXT ou DATA qui contient
une adresse absolue que GEMDOS devra ajuster à `base + valeur`.

### Ce que ça nous apporte

Marquer tous ces offsets comme **"adresse absolue embeddée"** permet
de distinguer :
- Les vraies instructions 68k (à désassembler)
- Les longwords qui sont des données d'adresse (à ne PAS désassembler)

C'est particulièrement important dans les tables de sauts (`SELECT/CASE`
compilé) et les pointeurs de callbacks VBL/HBL.

### Algorithme de parcours

```c
uint32_t u32Pos = read_long(pu8Reloc);  /* premier offset */
if (u32Pos == 0) return;                /* table vide */

mark_as_address(u32Pos);

while (true) {
    uint8_t u8Step = *pu8Reloc++;
    if (u8Step == 0x00) break;
    if (u8Step == 0x01) { u32Pos += 2; continue; }
    u32Pos += u8Step;
    mark_as_address(u32Pos);
}
```

### Fonctions ST4Ever correspondantes

**Ce qui existe déjà :**

```c
/* ✓ UC15 — Application automatique des fixups dans load_file() */
/* Le déplacement est appliqué en RAM ST lors du chargement. */
/* load_state_t.uiFixupCount indique le nombre de fixups appliqués. */
/* Exemple : load "ENCHANT.PRG" → uiFixupCount = 247 */

/* Le format RLE de la table est déjà implémenté dans load.c :
 * $01 = sauter 254 octets (attention : DIFFERENT de la doc standard)
 * $00 = fin de table
 * N   = avancer de N octets, puis patcher le longword
 * Note : la valeur $01 saute 254 octets (0xFE) dans ST4Ever, conforme
 * à la spec Atari ST (p. 30 Atari ST Internals). */
```

**Ce qui manque — à construire :**

```c
/* ✗ UC32A — Table de relocation exposée comme bitmap consultable */
/* Le désassembleur statique en a besoin pour éviter de décoder */
/* les longwords d'adresse comme des opcodes.                   */

typedef struct prg_reloc_set_s {
    st_u8_t *pBitmap;     /* 1 bit par byte du TEXT+DATA */
    st_u32_t uiTextLen;   /* taille du TEXT couvert     */
} prg_reloc_set_t;

st_error_t prg_build_reloc_set(const st_u8_t *pBuf,
                                const prg_hdr_t *ptHdr,
                                prg_reloc_set_t *ptReloc);
/* API query */
st_bool_t prg_reloc_is_addr(const prg_reloc_set_t *ptReloc,
                              st_u32_t uiOffset);
st_error_t prg_reloc_set_destroy(prg_reloc_set_t *ptReloc);
```

**Commande console ST4Ever :**

```
load <file.prg>    → applique automatiquement les fixups, affiche le count
                     dans info : "Fixups: 247"
```

---

## Phase 3 — Cartographie des segments

### Problème

Avant le désassemblage instruction par instruction, construire une
carte de qui fait quoi dans les segments. Aller ligne par ligne sans
cette carte revient à lire un roman en commençant au milieu.

### Carte type d'un .PRG Atari ST

```
┌─────────────────────────────────────────────────────┐
│ Header GEMDOS         $00–$1B (28 octets fixes)     │
├─────────────────────────────────────────────────────┤
│ TEXT segment          $1C → $1C + TEXT_size         │
│  ├ Startup / runtime  premiers centaines d'octets   │
│  ├ Tables de dispatch (jump tables SELECT/CASE)     │
│  ├ Runtime library    (PRINT, math, gestion str...) │
│  └ Code utilisateur   (PROCEDUREs, FUNCTIONs)       │
├─────────────────────────────────────────────────────┤
│ DATA segment          $1C + TEXT_size → ...         │
│  ├ Chaînes constantes (messages, noms de fichiers)  │
│  ├ Tables de données  (sinus précalculés, palettes) │
│  └ Pointeurs init.    (adresses patchées par reloc) │
├─────────────────────────────────────────────────────┤
│ SYMTAB                (si présente)                 │
├─────────────────────────────────────────────────────┤
│ Reloc table           (si ABSFLAG == 0)             │
└─────────────────────────────────────────────────────┘
```

BSS n'est pas dans le fichier — il est alloué par GEMDOS au chargement.

### Heuristiques de découpage du TEXT

Pour distinguer startup/runtime/code-utilisateur dans TEXT sans symboles :

1. **Startup** : commence à l'offset 0 du TEXT, contient les premiers
   `MOVEA.L` sur SP, initialisations de registres globaux (A5, A6 si
   frame global), `JSR` vers le vrai `main`.

2. **Runtime library** : blocs de code sans correspondance dans les
   symboles DRI, appelés depuis de nombreux endroits différents
   (forte in-degree dans le graphe d'appels). Fonctions comme
   `print_string`, `float_add`, `str_concat`, etc.

3. **Code utilisateur** : blocs avec pattern prologue/épilogue
   `LINK A6 / MOVEM / ... / MOVEM / UNLK / RTS`. En GFA, chaque
   PROCEDURE compilée commence obligatoirement par ce pattern.

4. **Tables de données dans TEXT** : zones entre fonctions,
   contenant des longwords qui sont dans la table de relocation, ou
   des words constituant des offsets de jump table.

### Fonctions ST4Ever correspondantes

**Ce qui existe déjà :**

```c
/* ✓ UC15 — Frontières de segments connues après load_file() */
/* load_state_t donne uiTextSize, uiDataSize, uiBssSize */
/* Le TEXT est en RAM ST à ST_LOAD_BASE (0x0800) */
/* Le DATA suit immédiatement le TEXT */

/* ✓ UC11-14 — Désassemblage linéaire du TEXT disponible */
st_error_t disasm_range(const st_u8_t *pBuf, size_t uiBufLen,
                          st_u32_t uiAddr,
                          disasm_result_t *ptResults,
                          size_t uiMaxLines, size_t *puiLines);
/* → produit le listing brut, base de la cartographie */

/* ✓ UC24A — Classification de contenu secteur (granularité 512 octets) */
/* sector_features_t contient : fOpcodeDensity, fBranchDensity,        */
/* fHwInContext, fVblInstall... — utile pour détecter les zones de code */
/* ⚠ Granularité secteur (512B), pas byte : insuffisant pour Phase 3   */

/* ✓ UC24B — Coloration sémantique dans la vue hex */
/* ehex_classify_sectors() → aeSecType[] → teinte fond par type */
/* → visualisation immédiate des zones code/FAT/BSS dans un .PRG/.st */
```

**Ce qui manque — à construire :**

```c
/* ✗ UC32A — Carte byte-par-byte du TEXT (src/prg.h) */
typedef enum prg_seg_byte_e {
    PRG_BYTE_UNKNOWN = 0,
    PRG_BYTE_CODE,        /* opcode ou extension word */
    PRG_BYTE_DATA,        /* donnée embeddée (adresse relocatable) */
    PRG_BYTE_JUMP_TABLE,  /* word dans une jump table SELECT/CASE */
    PRG_BYTE_STRING       /* ASCII en DATA ou dans TEXT */
} prg_seg_byte_t;

typedef struct prg_segmap_s {
    prg_seg_byte_t *aeBytes; /* heap, taille = uiTextSize + uiDataSize */
    st_u32_t        uiLen;
} prg_segmap_t;

st_error_t prg_map_segments(const prg_hdr_t *ptHdr,
                              const prg_reloc_set_t *ptReloc,
                              const prg_symtab_t *ptSymtab,
                              prg_segmap_t *ptMap);
/* Algorithme : 
 * 1. Marquer les offsets relocatés comme PRG_BYTE_DATA
 * 2. Désassemblage forward depuis offset 0 → marquer comme PRG_BYTE_CODE
 * 3. Heuristiques ASCII sur les zones restantes → PRG_BYTE_STRING */
st_error_t prg_segmap_destroy(prg_segmap_t *ptMap);
```

**Commande console ST4Ever :**

```
load <file.prg>          → charge en RAM ST (frontières connues)
edit <file.prg>          → vue intégrée hex+ASCII+disasm (UC10)
                           coloration sémantique par secteur (UC24B)
```

---

## Phase 4 — Point d'entrée et runtime de démarrage

### Problème

Identifier où commence réellement le code utilisateur, en distinguant
le runtime de démarrage injecté par le compilateur.

### Point d'entrée GEMDOS

GEMDOS saute toujours à `base + $1C` (premier octet du TEXT). C'est
l'entrée universelle. Souvent un `BRA.W` ou `JMP.L` vers le vrai
`main` compilé.

### Runtime de démarrage type (GFA Basic compilé)

Les premières instructions du TEXT GFA font typiquement :

```asm
; Démarrage GFA runtime
MOVEA.L  4(SP),A5         ; récupère basepage depuis la pile TOS
LEA      var_globales,A4  ; base des variables globales (A4 = gbl ptr)
JSR      init_runtime     ; initialise heap, gestion strings, etc.
JSR      _main_gfa        ; saute au premier PROCEDURE/code utilisateur
MOVE.W   #0,-(SP)
TRAP     #1               ; Pterm(0) — fin du programme
```

Le registre `A4` est souvent le **global data pointer** en GFA compilé
(équivalent de A5 en Pure C). Le reconnaître évite de confondre
`N(A4)` (variable globale) avec `N(A6)` (variable locale).

### Runtime de démarrage type (Pure C / Lattice C)

```asm
; Startup C classique sur 68k
MOVEA.L  4(SP),A0         ; basepage
MOVE.L   $C(A0),D0        ; taille TEXT+DATA+BSS depuis basepage
ADDI.L   #$100,D0         ; + taille basepage
MOVE.L   D0,-(SP)
MOVE.L   A0,-(SP)
MOVE.W   #$4A,-(SP)       ; Mshrink
TRAP     #1               ; réduire la mémoire allouée
...
JSR      _main             ; appel du main C
```

La séquence `Mshrink` est un marqueur quasi-universel des exécutables
compilés en C sur Atari ST.

### Sous ST4Ever : procédure pratique

**Dans ST4Ever (dynamique) :**
1. `load <file.prg>` → charge à ST_LOAD_BASE (0x0800)
2. `execute` → ouvre exec_mon (F5=step, F6=run, F8=stop)
3. Vue CPU (registres D0-D7/A0-A7/PC/SR) — UC25A
4. F5 step par step depuis le point d'entrée jusqu'au premier LINK A6

### Fonctions ST4Ever correspondantes

**Ce qui existe déjà :**

```c
/* ✓ UC25A — Point d'entrée fixe après load_file() */
/* exec_open() démarre le CPU à PC = ST_LOAD_BASE = 0x0800   */
/* (premier octet du TEXT, conforme à GEMDOS)                */
st_error_t exec_open(void);        /* ouvre monitor + CPU + screen */

/* ✓ UC25A/B — Step-by-step dynamique (outils de diagnostic) */
/* exec_state_t.bStepReq → F5 dans exec_mon                  */
/* exec_state_t.tCpuSnap → vue CPU UC25A (D0-D7, A0-A7, SR)  */
/* exec_mem.c → vue RAM UC25B                                 */
/* exec_asm.c → vue désasm UC25B (PC highlighted)            */
```

**Ce qui manque — à construire :**

```c
/* ✗ UC32A — Détection statique du type de runtime (src/prg_analyze.h) */
typedef enum prg_runtime_e {
    PRG_RUNTIME_GFA,      /* A4=gbl_ptr, TRAP#1 dense, pas Mshrink */
    PRG_RUNTIME_C,        /* Mshrink au startup, _main en symbole  */
    PRG_RUNTIME_ASM,      /* pas de prologue, MOVEA.L 4(SP),A0    */
    PRG_RUNTIME_UNKNOWN
} prg_runtime_t;

st_error_t prg_find_entry(const st_u8_t *pText,
                            size_t uiTextLen,
                            const prg_segmap_t *ptMap,
                            st_u32_t *puiUserCodeOffset,
                            prg_runtime_t *peRuntime,
                            st_u32_t *puiConfidence);
/* Heuristiques :
 * GFA  : présence de MOVEA.L 4(SP),Ax + LEA xxx,A4 dans les 30 premiers mots
 * C    : séquence Mshrink (MOVE.W #$4A,-(SP) / TRAP #1) dans les 40 premiers
 * ASM  : aucun pattern runtime, premier opcode utile immédiat              */
```

**Commande console ST4Ever :**

```
load <file.prg>    → charge, PC = 0x0800 prêt pour execute
execute            → ouvre 4 vues : monitor / CPU / RAM / écran (UC25A/B/27)
                     F5 = step, F6 = run, F7 = pause, F8 = stop
                     B = breakpoint au PC courant, C = clear BP
```

---

## Phase 5 — Identification des patterns d'appel 68k

### Problème

Reconnaître les conventions d'appel employées pour segmenter
correctement le code en unités fonctionnelles.

### 5.1 Convention GFA Basic compilé (PROCEDURE/FUNCTION)

#### Prologue standard

```asm
_PROC:
    LINK    A6,#-N              ; $4E56 FFXX — N octets de locaux
    MOVEM.L D2-D7/A2-A5,-(SP)  ; $48E7 — sauvegarde registres
```

#### Épilogue standard

```asm
    MOVEM.L (SP)+,D2-D7/A2-A5  ; $4CDF — restauration
    UNLK    A6                  ; $4E5E
    RTS                         ; $4E75
```

#### Appel de procédure (côté appelant)

```asm
    MOVE.L  arg2,-(SP)          ; push args droite → gauche
    MOVE.L  arg1,-(SP)
    BSR.W   _PROC
    ADDQ.L  #8,SP               ; nettoyage pile (2 args × 4 octets)
```

#### Prologue minimal (sans locaux)

```asm
_PROC_SIMPLE:
    MOVEM.L D2-D7/A2-A5,-(SP)
    ; ... code ...
    MOVEM.L (SP)+,D2-D7/A2-A5
    RTS                         ; pas de LINK/UNLK
```

#### GOSUB (sans paramètres, sans retour de valeur)

```asm
    BSR.W   _LABEL
    ; ...
_LABEL:
    ; ... code ...
    RTS                         ; ni LINK ni MOVEM — leaf subroutine
```

#### Valeurs de retour (FUNCTION)

```
D0.W   : type % (integer 16 bits), avec directive $F%
D0.L   : type & (long 32 bits), avec directive $F&
D0.L   : type ! (boolean 32 bits, 0 ou $FFFFFFFF)
FP reg : type # (float 6 octets) — via FPU software runtime
D0.L   : type $ (string) — pointeur vers descripteur de chaîne
```

### 5.2 Convention Pure C / Lattice C (GCC 68k)

```asm
_func:
    LINK    A6,#-N
    MOVEM.L D2-D7/A2-A5,-(SP)
    ; paramètres à 8(A6), 12(A6), 16(A6)...
    ; retour dans D0 (int/long) ou A0 (pointeur)
    MOVEM.L (SP)+,D2-D7/A2-A5
    UNLK    A6
    RTS
```

Différence clé avec GFA : en Pure C, les paramètres word sont souvent
passés en word sur la pile (pas promus en long), et le nettoyage peut
être fait par l'appelé (`RTD #N` sur 68010/020, ou `UNLK + ADD.W + RTS`).

### 5.3 Convention assembleur pur (demoscene)

Pas de convention fixe. Patterns fréquents :

```asm
; Fastcall registres (très courant en démo)
    MOVEA.L screen_ptr,A0
    MOVE.W  count,D0
    BSR     effet_plasma
    ; l'appelé utilise directement A0, D0 — pas de pile

; Ou bien : paramètres en variables globales
    MOVE.L  D0,param_global
    BSR     ma_routine
    ; la routine lit param_global directement
```

### 5.4 Layout de la pile après LINK A6,#-N

```
Adresse croissante →

SP →  [variables locales N octets : -N(A6) à -2(A6)]
      [registres sauvés par MOVEM                   ]
A6 →  [ancienne valeur de A6    +0(A6)              ]
      [adresse de retour         +4(A6)              ]
      [1er argument              +8(A6)              ]
      [2ème argument             +12(A6)             ]
      [3ème argument             +16(A6)             ]
```

### Fonctions ST4Ever correspondantes

**Ce qui existe déjà :**

```c
/* ✓ UC11-14 — Désassemblage unitaire et de plage (briques de base) */
st_error_t disasm_one(const st_u8_t *pBuf, size_t uiBufLen,
                       st_u32_t uiAddr, disasm_result_t *ptResult);
st_error_t disasm_range(const st_u8_t *pBuf, size_t uiBufLen,
                          st_u32_t uiAddr,
                          disasm_result_t *ptResults,
                          size_t uiMaxLines, size_t *puiLines);
/* disasm_result_t contient auWords[], szMnemonic[], szOperands[]  */
/* → tout le scan de prologues repose sur ces structures           */

/* ✓ UC30A-C — L'assembleur valide le round-trip disasm↔assemble */
/* (confirme que disasm_result_t est exploitable pour re-assembler) */
```

**Ce qui manque — à construire :**

```c
/* ✗ UC32A — Scanner de prologues dans le TEXT (src/prg_analyze.h) */
typedef struct prg_func_s {
    st_u32_t uiOffset;       /* offset dans le TEXT            */
    st_u32_t uiEndOffset;    /* offset fin de fonction estimée */
    int      iLocalBytes;    /* N dans LINK A6,#-N (0 si absent) */
    int      iArgBytes;      /* déduit du ADDQ après les BSR    */
    st_bool_t bHasLink;      /* ST_TRUE si prologue LINK/UNLK   */
    st_bool_t bHasMovem;     /* ST_TRUE si MOVEM registres      */
    char     szName[32];     /* depuis symtab DRI ou auto-généré */
} prg_func_t;

typedef struct prg_funclist_s {
    prg_func_t *atFuncs;
    int         iCount;
} prg_funclist_t;

st_error_t prg_scan_prologues(const st_u8_t *pText,
                                size_t uiTextLen,
                                const prg_reloc_set_t *ptReloc,
                                const prg_symtab_t *ptSymtab,
                                prg_funclist_t *ptFuncs);

st_error_t prg_infer_func_signature(const st_u8_t *pText,
                                     size_t uiTextLen,
                                     const prg_funclist_t *ptFuncs,
                                     int iFuncIdx);
/* Examine les BSR.W vers cette fonction → ADDQ après chaque BSR */
/* → déduit iArgBytes                                             */
st_error_t prg_funclist_destroy(prg_funclist_t *ptFuncs);
```

**Commande console ST4Ever :**

```
execute            → step dans exec_asm, reconnaître LINK/MOVEM visuellement
                     (analyse dynamique, pas statique)
```

> **Note :** L'analyse statique des prologues (Phase 5) est une
> opération batch sur le fichier brut — indépendante de l'exécution.
> Elle sera disponible via une commande `analyze <file.prg>` (UC32A).

---

## Phase 6 — Reconnaissance des trap calls système

### Problème

Les appels aux couches TOS (GEMDOS, BIOS, XBIOS, Line-A, AES, VDI)
sont le cœur fonctionnel de tout programme Atari ST. Les identifier
donne immédiatement la sémantique des blocs de code environnants.

### 6.1 Structure d'un appel TRAP

```asm
; Paramètres empilés AVANT le numéro de fonction (dernier push)
MOVE.L   param2,-(SP)
MOVE.W   param1,-(SP)
MOVE.W   #opcode,-(SP)      ; toujours le DERNIER push
TRAP     #vecteur
ADDQ.L   #N,SP              ; nettoyage (sauf Pterm qui ne revient pas)
```

| TRAP # | Sous-système | Exemple |
|--------|-------------|---------|
| `TRAP #1` | GEMDOS | fichiers, mémoire, processus |
| `TRAP #2` | GEM (AES/VDI) | interface graphique |
| `TRAP #13` | BIOS | I/O bas niveau, keyboard |
| `TRAP #14` | XBIOS | hardware direct, son, vidéo |

### 6.2 Opcodes GEMDOS fréquents (TRAP #1)

| Opcode | Nom | Signature pile |
|--------|-----|----------------|
| $00 | Pterm0 | aucun paramètre |
| $01 | Cconin | aucun |
| $02 | Cconout | W:caractère |
| $09 | Cconws | L:ptr_string |
| $1A | Fsetdta | L:ptr_dta |
| $1C | Dfree | L:ptr_buf, W:drive |
| $3B | Dsetpath | L:ptr_path |
| $3C | Fcreate | W:attr, L:ptr_name |
| $3D | Fopen | W:mode, L:ptr_name |
| $3E | Fclose | W:handle |
| $3F | Fread | L:count, L:ptr_buf, W:handle |
| $40 | Fwrite | L:count, L:ptr_buf, W:handle |
| $41 | Fdelete | L:ptr_name |
| $48 | Malloc | L:size |
| $49 | Mfree | L:ptr |
| $4A | Mshrink | L:new_size, L:ptr, W:0 |
| $4B | Pexec | L:env, L:args, L:name, W:mode |
| $4C | Pterm | W:code_retour |

### 6.3 Opcodes XBIOS fréquents (TRAP #14)

| Opcode | Nom | Usage courant |
|--------|-----|--------------|
| $00 | Initmous | initialisation souris |
| $05 | Setscreen | changement résolution/adresse écran |
| $07 | Setcolor | palette |
| $08 | Setpalette | palette complète (16 couleurs) |
| $09 | Setcolor | couleur individuelle |
| $0A | Setprt | mode imprimante |
| $0F | Rsconf | configuration RS232 |
| $14 | Getrez | résolution courante |
| $15 | Setrez | changement résolution |
| $22 | Vsync | attente retour balayage vertical |
| $25 | Supexec | exécution en mode superviseur |

### 6.4 Line-A ($A000–$A00F)

Les appels Line-A sont identifiables par le mot $A0xx :
```asm
DC.W    $A000               ; Line-A Init
DC.W    $A001               ; Put Pixel
DC.W    $A002               ; Get Pixel
DC.W    $A007               ; Filled Rectangle
DC.W    $A00F               ; BitBlt (blitter logiciel)
```

Bien que déclarés "non supportés" par Atari depuis TOS 1.04, ils
restent massivement utilisés dans les programmes des années 86–90.

### 6.5 Appels GEM (AES/VDI via TRAP #2)

Les appels AES et VDI passent par une structure de tableaux en mémoire
plutôt que par la pile directement. Le pattern est :
```asm
LEA      contrl,A0           ; tableau de contrôle AES/VDI
; ... remplir contrl, intin, intout, addrin, addrout ...
MOVE.W   #$C8,-(SP)         ; code AES ($C8) ou VDI ($73)
TRAP     #2
ADDQ.L   #2,SP
```

### Fonctions ST4Ever correspondantes

**Ce qui existe déjà (dispatch dynamique — pendant l'exécution) :**

```c
/* ✓ UC29 — Dispatcher GEMDOS (TRAP #1) et XBIOS (TRAP #14) */
/* Intercepte dans cpu_step() AVANT que le 68000 pousse le frame */
st_error_t tos_gemdos(cpu68k_t *ptCpu, st_machine_t *ptMachine);
st_error_t tos_xbios(cpu68k_t *ptCpu, st_machine_t *ptMachine);
/* Fonctions GEMDOS impl. : Pterm0 ($00), Cconws ($09), Pterm ($4C) */
/* Fonctions XBIOS impl.  : Setscreen ($03), Setpalette ($05),      */
/*                          Setcolor ($07), Vsync ($25)              */
/* → toutes les autres : LOG_TODO + retour ST_NO_ERROR (stub)        */

/* ✓ UC28 — Dispatcher Line-A ($A000-$A0FF) */
st_error_t linea_dispatch(cpu68k_t *ptCpu, st_machine_t *ptMachine,
                            st_u16_t uiOpcode);
/* Impl. : $A000 LINEA_INIT (bloc param RAM), $A001 PUT_PIXEL       */
/* Stubs : $A002 GET_PIXEL, $A007 FILL_RECT, $A00F BITBLT           */
```

**Ce qui manque — à construire (analyse statique) :**

```c
/* ✗ UC32A — Scanner statique des appels TRAP dans le TEXT */
typedef struct prg_trap_call_s {
    st_u32_t uiOffset;        /* offset de l'instruction TRAP dans TEXT */
    int      iTrapVec;        /* 1=GEMDOS, 13=BIOS, 14=XBIOS, 2=GEM   */
    st_u16_t uiOpcode;        /* numéro de fonction ($00=Pterm0, etc.)  */
    char     szName[32];      /* "Pterm0", "Fopen", "Vsync", ...        */
    int      iArgBytes;       /* octets empilés (déduit du ADDQ suivant)*/
    st_u32_t uiPushOffset;    /* offset premier MOVE.W #opcode,-(SP)    */
} prg_trap_call_t;

typedef struct prg_traplist_s {
    prg_trap_call_t *atCalls;
    int              iCount;
} prg_traplist_t;

st_error_t prg_scan_trap_calls(const st_u8_t *pText,
                                size_t uiTextLen,
                                prg_traplist_t *ptTraps);
/* Algorithme : chercher l'opcode TRAP ($4E4x), remonter le stream   */
/* pour trouver MOVE.W #opcode,-(SP) → déduire nom + paramètres      */
st_error_t prg_traplist_destroy(prg_traplist_t *ptTraps);

/* Table de décodage GEMDOS/XBIOS (constantes statiques dans prg_analyze.c) */
const char *prg_gemdos_name(st_u16_t uiOpcode);
const char *prg_xbios_name(st_u16_t uiOpcode);
const char *prg_bios_name(st_u16_t uiOpcode);
```

**Commande console ST4Ever :**

```
execute            → TRAP #1/#14 interceptés dynamiquement, LOG_INFO affiché
                     dans la vue trace (ex : "XBIOS Vsync")
trace level info   → filtre la vue trace pour voir uniquement les TRAPs
```

---

## Phase 7 — Détection des procédures et fonctions

### Problème

Après les phases préliminaires, reconnaître les unités fonctionnelles
du programme pour passer d'un flux d'octets à un graphe de fonctions.

### 7.1 Détection par signature de prologue

Scanner le TEXT pour les patterns (en tenant compte de ST_SegmentMap) :

**Pattern A — Prologue GFA complet :**
```
$4E56           LINK A6,#-N     (N quelconque, souvent pair)
$48E7 xxxx      MOVEM.L ...     (masque de registres)
```
→ Fonction avec locaux et sauvegarde de registres.

**Pattern B — Prologue léger :**
```
$48E7 xxxx      MOVEM.L Dx/Ax,-(SP)
```
Suivi d'un `$4CDF xxxx MOVEM.L (SP)+,... / $4E75 RTS`.
→ Fonction sans variables locales.

**Pattern C — Leaf function :**
```
(aucun LINK, aucun MOVEM)
...
$4E75           RTS
```
→ Petite fonction ou GOSUB sans overhead.

**Pattern D — Prologue C pur :**
```
$4E56 FFXX      LINK A6,#-N
$48E7 3F3E      MOVEM.L D2-D7/A2-A5,-(SP)  (masque standard Pure C)
```

### 7.2 Délimitation de la fin d'une fonction

La fin est marquée par l'épilogue (UNLK + RTS, ou MOVEM + RTS), mais
attention aux fonctions qui ne reviennent jamais (`Pterm`, boucles
infinies VBL). Heuristiques :
- RTS après UNLK = fin normale
- `JMP (An)` sans RTS = fin par dispatch (switch) ou tail call
- BRA vers un autre LINK = tail call optimisé

### 7.3 Déduction des paramètres

Examiner le code **après** chaque `BSR target` :

```asm
BSR.W   _PROC
ADDQ.L  #4,SP       → 1 paramètre long (4 octets)
ADDQ.L  #8,SP       → 2 paramètres long, ou 1 long + 1 ptr
ADD.W   #16,SP      → 4 paramètres long
LEA     8(SP),SP    → 2 paramètres long (alternative ADDQ)
```

Et à l'intérieur de la procédure, les accès à `N(A6)` avec N ≥ 8
indiquent des paramètres, N < 0 des locaux.

### 7.4 Nommage automatique

Sans table de symboles, générer des noms temporaires :
```
sub_0042A       : fonction à l'offset $42A
leaf_00180      : leaf function à $180
proc_arg2_00380 : fonction avec 2 args à $380
```

Avec table de symboles DRI : utiliser directement le nom symbolique
(dé-tronquer les 7 caractères GFA en ajoutant `...` si tronqué).

### Fonctions ST4Ever correspondantes

**Ce qui existe déjà :**

```c
/* ✓ UC11-14 — disasm_result_t est la brique de base */
/* szMnemonic[] permet de chercher "LINK", "MOVEM.L", "RTS", "UNLK"  */
/* auWords[] permet de filtrer les masques exacts ($4E56, $48E7, etc.)*/

/* ✓ UC24A — sector_classify() fournit un score CODE_GEMDOS vs CODE_DEMO */
/* → indicateur de densité de fonctions (forte densité BSR = code GFA)    */
```

**Ce qui manque — à construire :**

```c
/* ✗ UC32A — Table complète des fonctions (src/prg_analyze.h) */
/* (extension de prg_scan_prologues de Phase 5 — même module) */
st_error_t prg_build_functable(const st_u8_t *pText,
                                size_t uiTextLen,
                                const prg_reloc_set_t *ptReloc,
                                const prg_symtab_t *ptSymtab,
                                prg_funclist_t *ptFuncs);
/* = prg_scan_prologues() + résolution des noms DRI + nommage auto */
/* + déduction des signatures par analyse des BSR callers           */
```

**Commande console ST4Ever :**

```
execute + F5       → naviguer pas-à-pas dans la vue exec_asm, voir
                     chaque LINK/UNLK/RTS visuellement
```

---

## Phase 8 — Reconstruction du graphe de flot de contrôle

### Problème

Transformer le listing linéaire en un graphe orienté (CFG — Control
Flow Graph) pour comprendre la structure logique du programme.

### 8.1 Types d'arcs dans le graphe

| Instruction | Arc |
|------------|-----|
| `BSR / JSR` | arc d'appel vers cible (call edge) |
| `BRA / BCC` | arc de branchement (branch edge) |
| `DBcc` | arc de boucle (back edge si cible < offset courant) |
| `RTS / RTR` | arc de retour (synthétique) |
| `JMP (An)` | arc indirect (cible inconnue statiquement) |
| `JMP table(PC,Dn*2)` | arc de dispatch (jump table) |

### 8.2 Résolution des jump tables (SELECT/CASE GFA)

Le compilateur GFA transforme `SELECT/CASE` en :
```asm
MOVE.W   test_val,D0
CMP.W    #max_case,D0
BHI.W    default_label
ADD.W    D0,D0              ; × 2 (chaque entrée = 1 word = offset relatif)
MOVE.W   table(PC,D0.W),D0  ; lire offset dans table
JMP      table(PC,D0.W)     ; sauter
table:
    DC.W    case0_label - table
    DC.W    case1_label - table
    ...
```

Les `DC.W` de la table sont des données dans le TEXT — **les marquer
comme non-code** dans `prg_segmap_t` est essentiel.

### 8.3 Détection des boucles

Un arc `BRA` ou `DBcc` dont la cible est **inférieure** à l'offset
courant est un back-edge = boucle. Les patterns fréquents :

```asm
; FOR ... NEXT compilé en GFA
    MOVE.L  init,D7
loop:
    ; ... corps ...
    ADDQ.L  #step,D7
    CMP.L   limit,D7
    BLE.W   loop

; REPEAT ... UNTIL
repeat_top:
    ; ... corps ...
    TST.L   condition
    BEQ.W   repeat_top

; WHILE ... WEND
while_check:
    TST.L   condition
    BEQ.W   while_end
    ; ... corps ...
    BRA.W   while_check
while_end:
```

### 8.4 Gestion des fonctions VBL

Les handlers VBL (démos) sont installés via XBIOS Setexc ou par
écriture directe en $0070 (vecteur VBL). Ils sont des fonctions
sans appel BSR depuis le code principal — leur détection exige de
chercher les écritures sur ces adresses vecteur.

```asm
MOVE.L   #vbl_handler,$70.W     ; installation directe du vecteur VBL
; ou
MOVE.W   #$19,-(SP)             ; Setexc
MOVE.L   #vbl_handler,-(SP)
MOVE.W   #$1A,-(SP)
TRAP     #14
```

Le handler VBL lui-même se termine par `RTE` (pas `RTS`).

### Fonctions ST4Ever correspondantes

**Ce qui existe déjà :**

```c
/* ✓ UC25A/B — CFG dynamique via step/breakpoints */
/* L'exec engine (UC25A) reconstruit implicitement le CFG en live :  */
/*   exec_state_t.tCpuSnap.uiPC = position courante                  */
/*   exec_state_t.auBP[EXEC_BP_MAX] = breakpoints (jusqu'à 8)       */
/*   → poser un BP sur un LINK = détecter une entrée de fonction     */
/*   → poser un BP sur un RTE = détecter un handler VBL/interrupt    */

/* ✓ UC24A — fVblInstall dans sector_features_t */
/* float fVblInstall : pattern MOVE.L #imm,$70.W détecté à l'extraction */
/* → flag heuristique "ce secteur installe un handler VBL"              */

/* ✓ UC15A — torture test disasm valide disasm_range() sur 2525 instrs */
/* La précision est suffisante pour un CFG statique forward-scan        */
```

**Ce qui manque — à construire :**

```c
/* ✗ UC32B — CFG statique par fonction (src/prg_cfg.h) */
/* (module séparé de prg_analyze — plus complexe) */
typedef struct prg_bb_s {        /* basic block */
    st_u32_t uiStart;
    st_u32_t uiEnd;
    st_u32_t auSucc[2];  /* jusqu'à 2 successeurs (branch/fall-through) */
    int      iSuccCount;
    st_bool_t bIsLoopHead;
} prg_bb_t;

typedef struct prg_cfg_s {
    prg_bb_t *atBlocks;
    int       iBlockCount;
} prg_cfg_t;

st_error_t prg_build_cfg(const st_u8_t *pText,
                           st_u32_t uiFuncStart,
                           st_u32_t uiFuncEnd,
                           const prg_reloc_set_t *ptReloc,
                           prg_cfg_t *ptCfg);

st_error_t prg_resolve_jump_table(const st_u8_t *pText,
                                   st_u32_t uiTableOffset,
                                   st_u32_t uiNumEntries,
                                   st_u32_t *puiTargets,
                                   prg_segmap_t *ptMap);

/* ✗ UC32B — Détection des handlers d'interruption installés */
st_error_t prg_find_irq_handlers(const st_u8_t *pText,
                                   const prg_traplist_t *ptTraps,
                                   prg_funclist_t *ptHandlers);
/* Cherche les patterns :
 *   MOVE.L #addr,$70.W  → VBL handler à addr
 *   Setexc($19, addr)   → VBL handler via XBIOS
 *   MOVE.L #addr,$68.W  → HBL handler                */
st_error_t prg_cfg_destroy(prg_cfg_t *ptCfg);
```

**Commande console ST4Ever :**

```
execute + BP       → poser breakpoints aux cibles de BSR/JMP pour
                     reconstruire le CFG manuellement (approche dynamique)
```

---

## Phase 9 — Identification du compilateur et du langage source

### Problème

Savoir quel compilateur a produit le binaire oriente toute l'analyse :
les conventions d'appel, les patterns de runtime, les optimisations
présentes ou absentes changent complètement.

### 9.1 Signatures de compilateurs Atari ST

**GFA Basic compilé :**
- Prologue `LINK A6,#-N` + `MOVEM.L D2-D7/A2-A5`
- Présence d'un registre global base (souvent A4) chargé au startup
- Séquences `MOVE.W #opcode,-(SP) / TRAP #1 / ADDQ #N,SP` très denses
- Pas de `Mshrink` en startup (GFA gère sa propre mémoire)
- Chaînes de caractères avec descripteur 6 octets en DATA
- Symboles DRI tronqués à 7 caractères avec underscore initial `_PROC_NA`

**Pure C (Holger Gille) :**
- Startup avec `Mshrink` obligatoire
- Convention registres : D2-D7/A2-A6 callee-save, D0/D1/A0/A1 scratch
- Noms symboliques non tronqués (jusqu'à 32 chars en Pure C 1.1+)
- Code très compact, optimisations agressives (pas de MOVEM si inutile)

**Lattice C / HiSoft C :**
- Similaire Pure C mais séquences de startup légèrement différentes
- `__main` en symbole explicite

**Assembleur pur (DEVPAC, SEKA, GenST) :**
- Pas de prologue/épilogue standardisé
- Pas de runtime de démarrage
- Code 100% PC-relatif si prévu pour être chargé par un loader
- Souvent un simple `MOVEA.L 4(SP),A0` pour récupérer la basepage

**OMIKRON Basic compilé :**
- Différent de GFA : prologue souvent sans LINK, paramètres en registres
- Runtime distinct reconnaissable à ses vecteurs de trap custom

### 9.2 Heuristique de scoring

Calculer un score pour chaque compilateur candidat en comptant les
occurrences de leurs patterns caractéristiques :

```
score_GFA    += 1 pour chaque LINK A6 + MOVEM D2-D7/A2-A5
score_PURC   += 1 pour Mshrink au startup
score_PURC   += 1 pour MOVEM D2-D7/A2-A6 (A6 inclus, pas A5)
score_ASM    += 1 pour absence totale de LINK dans les fonctions
score_ASM    += 1 pour usage intensif de registres comme params
```

### Fonctions ST4Ever correspondantes

**Ce qui existe déjà :**

```c
/* ✓ UC24A — Classification au niveau secteur (granularité 512 octets) */
/* sector_type_t distingue :
 *   SECTOR_CODE_DEMO    → code avec accès HW ($FF8xxx, VBL install)
 *   SECTOR_CODE_GEMDOS  → code avec TRAP #1/#14, sans HW direct
 *   SECTOR_CODE_PACKER  → self-décompresseur (heuristique entropie)
 * → couvre partiellement Phase 9 mais PAS le compilateur (GFA vs C vs ASM) */

/* ✓ UC24A — sector_features_t contient des features utiles */
/* fHwImmediate, fVblInstall → indicateurs ASM/démo         */
/* fBranchDensity + fOpcodeDensity → densité de code        */
/* Ces features peuvent être réutilisées dans prg_identify_compiler() */
```

**Ce qui manque — à construire :**

```c
/* ✗ UC32A — Identification compilateur (src/prg_analyze.h) */
typedef enum prg_compiler_e {
    PRG_COMP_GFA,
    PRG_COMP_PURC,
    PRG_COMP_LATTICE,
    PRG_COMP_ASM_DEVPAC, /* assembleur pur DEVPAC/SEKA/GenST */
    PRG_COMP_OMIKRON,
    PRG_COMP_UNKNOWN
} prg_compiler_t;

st_error_t prg_identify_compiler(const st_u8_t *pText,
                                   size_t uiTextLen,
                                   const prg_segmap_t *ptMap,
                                   prg_compiler_t *peCompiler,
                                   st_u32_t *puiConfidence);
/* Réutilise sector_features_extract() sur les 512 premiers octets  */
/* + compte LINK A6 / MOVEM masques / Mshrink / densité BSR vs JMP  */
```

**Commande console ST4Ever :**

```
(aucune commande dédiée actuellement)
→ à venir avec `analyze <file.prg>` UC32A
```

---

## Phase A — Annotation sémantique et nommage

### Problème

Transformer le désassemblage brut en quelque chose de lisible, en
propageant les informations collectées dans les phases précédentes.

### A.1 Priorités d'annotation

1. **Noms depuis la table de symboles DRI** (priorité absolue)
2. **Noms des trap calls** (Fopen, Vsync, etc.)
3. **Noms des fonctions identifiées** (vbl_handler, main, etc.)
4. **Noms générés** (sub_XXXX, leaf_XXXX)
5. **Commentaires automatiques** sur les constantes GEMDOS

### A.2 Commentaires automatiques sur les instructions

```asm
MOVE.W   #$3D,-(SP)     ; Fopen
MOVE.W   #0,-(SP)       ; mode = lecture seule
MOVE.L   A0,-(SP)       ; ptr nom de fichier
TRAP     #1             ; GEMDOS
ADDQ.L   #8,SP
```

→ Le désassembleur annote avec le nom de la fonction GEMDOS et les
noms des paramètres connus.

### A.3 Reconnaissance de patterns algorithmiques (UC32)

Basé sur le système de détection probabiliste défini en UC32 :

| Pattern | Indice 68k |
|---------|-----------|
| Table de sinus | `MOVE.W (An)+,Dx` dans une boucle `DBcc`, valeurs dans [-$8000,$7FFF] périodiques |
| Blitter loop | `MOVE.W #val,$FF8A3C.W` (endmask) + `MOVE.B #$x,$FF8A3E` (op) |
| VBL handler | Entrée sans BSR entrant + `RTE` en sortie |
| GEMDOS trap | Pattern push + TRAP #1 identifié Phase 6 |
| Palette ST | 16 words consécutifs en DATA dans plage $000–$777 |
| Scrolltext | Boucle sur tableau de bytes en ASCII |
| Copper-bar | Boucle courte modifiant `$FF8240.W`–`$FF825E.W` |

### Fonctions ST4Ever correspondantes

**Ce qui existe déjà :**

```c
/* ✓ UC24C/D — Annotation JSON disque + bandeau hex edit */
st_error_t image_annot_load(const char *szImagePath,
                              image_annot_t *ptAnnot);
st_error_t image_annot_save(const char *szImagePath,
                              const image_annot_t *ptAnnot);
/* → annotation par secteur LBA (notes, type, labels cliquables) */
/* → spécifique aux images disque .st/.msa, PAS aux .PRG en tant que tels */

/* ✓ UC24A — sector_classify() + sector_type_name() */
/* → classification probabiliste + score de confiance par secteur    */
/* → SECTOR_DATA_SINUS détecte les tables sinus avec fSinusProfile   */

/* ✓ UC24B — coloration sémantique dans la vue hex (ehex_classify) */
/* → visualisation immédiate des zones CODE_DEMO/FAT12/BSS_ZERO      */

/* ✓ UC10 — Vue intégrée hex + disasm synchronisée */
/* → désassemblage aligné avec les octets bruts (UC10, UC11-14)      */
/* → bonne base pour annoter visuellement un .PRG dans l'éditeur hex */
```

**Ce qui manque — à construire :**

```c
/* ✗ UC32C — Annotation d'un listing disasm (src/prg_annotate.h) */
/* Enrichit disasm_result_t[] avec noms de symboles + TRAP noms    */
typedef struct prg_annot_s {
    char szLabel[32];     /* nom de la cible (si BSR/JMP vers fonction) */
    char szComment[64];   /* commentaire automatique (TRAP, pattern, ...) */
} prg_annot_t;

st_error_t prg_annotate_listing(disasm_result_t *ptResults,
                                  size_t uiCount,
                                  const prg_funclist_t *ptFuncs,
                                  const prg_traplist_t *ptTraps,
                                  const prg_symtab_t *ptSymtab,
                                  prg_annot_t *ptAnnots);

/* ✗ UC32C — Détection de patterns algorithmiques (spec UC32 §8.3) */
typedef struct prg_pattern_s {
    char     szName[32];   /* "vbl_handler", "sinus_table", etc. */
    st_u32_t uiOffset;     /* offset dans TEXT ou DATA */
    st_u32_t uiLen;        /* taille estimée du pattern */
    float    fConfidence;  /* 0.0-1.0 */
} prg_pattern_t;

typedef struct prg_patternlist_s {
    prg_pattern_t *atPatterns;
    int            iCount;
} prg_patternlist_t;

st_error_t prg_detect_patterns(const st_u8_t *pText,
                                 const st_u8_t *pData,
                                 const prg_funclist_t *ptFuncs,
                                 prg_patternlist_t *ptPatterns);
/* Réutilise les features de sector_features_t :
 *   fVblInstall → chercher RTE en fin de bloc
 *   fSinusProfile → valider périodicité sur 256 words
 *   fHwImmediate dans boucle courte → copper-bar
 *   fTimingLoops (DBcc dense) → blitter loop               */
st_error_t prg_patternlist_destroy(prg_patternlist_t *ptPatterns);
```

**Commandes console ST4Ever :**

```
edit <file.prg>           → vue hex+disasm intégrée (UC10), annotation manuelle
                             via bande et JSON (UC24C/D) si image disque
edit --hex <file.prg>     → hex brut, coloration sémantique secteurs (UC24B)
(future) annotate <file>  → rapport textuel automatique (UC32C)
```

---

## Phase B — Cas spéciaux : disques bootables et démos

### B.1 Secteur de boot Atari ST

Le secteur de boot est le premier secteur de la piste 0, côté 0.
Il fait 512 octets et a sa propre structure :

```
+$000  3 octets   BRA.S +$1E (saut par-dessus le BPB)
+$003  8 octets   OEM string ("ATARI ST" ou autre)
+$00B  word       Bytes per sector (généralement 512)
+$00D  byte       Sectors per cluster
+$00E  word       Reserved sectors
+$010  byte       Number of FATs
+$011  word       Root entries
+$013  word       Total sectors
+$015  byte       Media descriptor
+$016  word       Sectors per FAT
+$018  word       Sectors per track
+$01A  word       Number of sides
+$01C  word       Hidden sectors
+$01E  ...        Code de boot (494 octets restants)
+$1FE  word       Checksum (somme de tous les words = $1234 si bootable)
```

Si la checksum vaut $1234, GEMDOS exécute le secteur de boot.
Le code commence à `+$1E` (après le BPB), se termine par une
instruction de fin (souvent `CLR.W -(SP) / TRAP #1` pour Pterm,
ou un `JMP` vers du code chargé depuis le disque).

### B.2 Spécificités des démos demoscene

Les démos 68k des années 86–93 ont des caractéristiques distinctives :

**Accès hardware direct :**
```asm
MOVE.W   #$2700,SR              ; désactive interruptions (mode superviseur)
MOVE.L   #vbl_handler,$70.W     ; installe VBL directement en mémoire
MOVE.B   #$0,$FFFA07.W          ; ACIA control
MOVE.W   #%0000111111111111,$FF8240.W  ; palette
```

**Démarrage typique de démo :**
```asm
start:
    MOVE.W   #$2700,SR          ; supervisor + interruptions off
    LEA      new_stack,A7       ; nouvelle pile (démos gèrent leur stack)
    MOVE.W   #$22,-(SP)         ; Setexc VBL
    MOVE.L   #vbl,-(SP)
    TRAP     #14
    ADDQ.L   #6,SP
    ; ... boucle principale ...
mainloop:
    STOP     #$2300             ; attendre une interruption (VBL)
    BRA      mainloop
```

**Handler VBL typique :**
```asm
vbl_handler:
    MOVEM.L  D0-D7/A0-A6,-(SP)  ; sauvegarde TOUT (contexte d'interruption)
    ; ... effet graphique ...
    MOVEM.L  (SP)+,D0-D7/A0-A6
    RTE                          ; retour d'interruption (pas RTS)
```

### B.3 Séquence d'analyse d'un disque bootable

1. Lire secteur 0 → vérifier checksum $1234
2. Parser BPB → comprendre la structure FAT du disque
3. Désassembler le code boot (offset $1E, 494 octets max)
4. Identifier si le boot charge des secteurs supplémentaires (loader)
5. Si loader : trouver les secteurs chargés, les désassembler
6. Identifier le point de transfert final vers le programme principal

### Fonctions ST4Ever correspondantes

**Ce qui existe déjà :**

```c
/* ✓ UC16/UC18 — FAT12 complet : lecture BPB, structure secteur */
/* image_st_t contient les 737280 octets avec accès direct secteur */
/* → secteur 0 = pSector[0..511] = bootsector brut               */

/* ✓ UC18.2 — Détection bootable + vue hex bootsector */
/* Touche B dans la vue mount → extrait les 512 octets du bootsector */
/* dans un fichier temp → edit_hex_open() sur le bootsector          */
/* Panneau propriétés mount : "Bootable: YES/NO" + checksum WD1772   */

/* ✓ UC24A — Fingerprint bootsector */
sector_type_t eType;  /* SECTOR_BOOTSECTOR_BOOT ou SECTOR_BOOTSECTOR_NOBOOT */
/* sector_features_t.fWd1772Bootable = 1.0 si checksum == $1234       */
/* sector_features_t.fBpbValid = validité géométrie BPB                */
/* sector_features_t.fOpcodeDensity = densité de code dans boot code   */

/* ✓ UC24B-D — Coloration + bande + labels BPB cliquables */
/* edit_hex_set_cursor_pos() → navigation vers FAT1/FAT2/Root/Data     */
/* Bande hex : type/score secteur + BPB décodé + notes JSON éditables  */

/* ✓ UC11-14 — disasm_range() pour le code boot */
/* disasm_range(pSector + 0x1E, 512 - 0x1E - 2, base, ...) */
/* → désassemble les 494 octets du code de boot             */

/* ✓ UC24A — sector_features_t.fPackerEntropy */
/* → détecte les bootsectors contenant un décompresseur (entropie élevée) */
```

**Ce qui manque — à construire :**

```c
/* ✗ UC32A — API publique dédiée bootsector (src/boot_analyze.h) */
/* (actuellement intégré dans image_st.c et mount.c — non exporté) */
typedef struct boot_sector_s {
    st_u8_t  aRaw[512];          /* octets bruts                       */
    st_bool_t bBpbValid;         /* BPB géométrie cohérente            */
    st_bool_t bBootable;         /* checksum WD1772 == $1234           */
    st_u16_t uiBps;              /* bytes per sector                   */
    st_u8_t  uiSpc;              /* sectors per cluster                */
    st_u8_t  uiNFats;            /* number of FATs                     */
    st_u16_t uiRde;              /* root directory entries             */
    st_u16_t uiTs;               /* total sectors                      */
    st_u16_t uiSpf;              /* sectors per FAT                    */
    st_u16_t uiSpt;              /* sectors per track                  */
    st_u16_t uiHeads;            /* number of heads                    */
    st_bool_t bHasCode;          /* opcodes valides détectés à +$1E    */
    st_bool_t bIsPacked;         /* décompresseur détecté              */
    char     szOem[9];           /* OEM string (+$003)                 */
} boot_sector_t;

st_error_t boot_parse_sector(const st_u8_t *pSector,
                               boot_sector_t *ptBoot);

st_error_t boot_disassemble(const boot_sector_t *ptBoot,
                              disasm_result_t *ptResults,
                              size_t uiMaxLines,
                              size_t *puiLines);
/* → disasm_range(ptBoot->aRaw + 0x1E, 512 - 0x1E - 2, base, ...) */
```

**Commandes console ST4Ever :**

```
mount <image.st>          → vue mount avec analyse BPB, touche B=bootsector hex
edit --hex <image.st>     → vue hex + bande BPB décodée + labels FAT1/FAT2/Root
sector_classify           → via UC24A en interne (affiché dans bande hex)
disasm_range interne      → via edit_hex (colonne disasm synchronisée, UC10)
```

---

## Récapitulatif : Phases × ST4Ever — État courant

| Phase | Description | Module ST4Ever existant | UC | Module à créer | UC cible |
|-------|-------------|------------------------|----|----------------|----------|
| 0 | Type de binaire | `load_file()` + `sector_classify()` | UC7/UC24A | `prg_identify_binary()` → `prg.h` | UC32A |
| 1 | Header GEMDOS + symboles | `load_state_t` (partiel) | UC15 | `prg_parse_header()` + `prg_parse_symtab()` → `prg.h` | UC32A |
| 2 | Table de relocation | `load_file()` applique, `uiFixupCount` | UC15 | `prg_build_reloc_set()` → `prg.h` | UC32A |
| 3 | Cartographie segments | `load_state_t` frontières + `disasm_range()` | UC15/UC11-14 | `prg_map_segments()` → `prg.h` | UC32A |
| 4 | Point d'entrée + runtime | `exec_open()` @ ST_LOAD_BASE | UC25A | `prg_find_entry()` statique → `prg_analyze.h` | UC32A |
| 5 | Patterns d'appel | `disasm_range()` briques | UC11-14 | `prg_scan_prologues()` → `prg_analyze.h` | UC32A |
| 6 | Trap calls système | `tos_gemdos()`, `tos_xbios()`, `linea_dispatch()` (dynamique) | UC28/UC29 | `prg_scan_trap_calls()` statique → `prg_analyze.h` | UC32A |
| 7 | Table des fonctions | `disasm_range()` + `sector_classify()` | UC11-14/UC24A | `prg_build_functable()` → `prg_analyze.h` | UC32A |
| 8 | Graphe de flot | `exec` step+BP (dynamique), `fVblInstall` (UC24A) | UC25A/B | `prg_build_cfg()` + `prg_find_irq_handlers()` → `prg_cfg.h` | UC32B |
| 9 | Identification compilateur | `sector_classify()` (niveau secteur) | UC24A | `prg_identify_compiler()` → `prg_analyze.h` | UC32A |
| A | Annotation sémantique | `image_annot_*()` (disque), `sector_classify()`, bande hex | UC24A-D | `prg_annotate_listing()` + `prg_detect_patterns()` → `prg_annotate.h` | UC32C |
| B | Boot + démos | `sector_features_extract()` + vue B mount + `disasm_range()` | UC16/UC18.2/UC24A | `boot_parse_sector()` + `boot_disassemble()` → `boot_analyze.h` | UC32A |

---

## Propositions d'Use Cases ST4Ever pour les phases manquantes

### UC32A — Moteur d'analyse statique PRG

**Scope :** Phases 0–7 + 9 + B (modules `prg.h`, `prg_analyze.h`, `boot_analyze.h`)

**Nouvelles commandes console :**
```
analyze <file.prg>        → rapport textuel complet :
                             - Type binaire + confiance
                             - Header : TEXT/DATA/BSS/SYMTAB/FLAGS/ABSFLAG
                             - Symboles DRI (si présents)
                             - Type runtime : GFA/PureC/ASM + confiance
                             - Fonctions détectées (offset, args, type)
                             - TRAPs : Fopen($3D), Vsync($22), etc.
                             - Compilateur estimé + score
analyze --boot <image.st> → analyse bootsector : BPB, bootable, code/packer
```

**Nouveaux modules :**
- `src/prg.h/c` : header, symtab DRI, reloc bitmap, segmap — ~400 lignes
- `src/prg_analyze.h/c` : runtime detector, prologue scanner, trap scanner,
  compiler identifier, function table — ~600 lignes
- `src/boot_analyze.h/c` : boot sector parser, disassembly wrapper — ~150 lignes

**Dépendances :** UC15 (PRG load), UC11-14 (disasm), UC24A (sector features)

**Critère de validation :** `analyze ENCHANT.PRG` produit un rapport avec
type=GFA ou ASM + liste de fonctions trouvées (≥ 5 prologues détectés) +
liste de TRAPs.

---

### UC32B — CFG statique et détection interruptions

**Scope :** Phase 8 (module `prg_cfg.h`)

**Nouvelles commandes console :**
```
analyze --cfg <file.prg>  → graphe de flot : blocs de base + arcs
                             + handlers VBL/HBL identifiés
```

**Nouveau module :**
- `src/prg_cfg.h/c` : basic blocks, arcs BSR/BRA/DBcc/JMP, jump table
  resolver, interrupt handler finder — ~500 lignes

**Dépendances :** UC32A (prg_analyze, segmap, funclist)

**Critère de validation :** démo cible UC31 → CFG extrait → VBL handler
identifié + install vector confirmé à $0070.

---

### UC32C — Annotation sémantique et patterns

**Scope :** Phase A (module `prg_annotate.h`)

**Nouvelles commandes console :**
```
annotate <file.prg>       → .s désassemblé enrichi avec labels + commentaires
                             "use_cases/UC32/<demo>.s" versionné
annotate --patterns <file>→ liste des patterns détectés avec scores
```

**Nouveau module :**
- `src/prg_annotate.h/c` : listing annotator, pattern detector (sinus,
  VBL, blitter, copper-bar, scrolltext, palette) — ~400 lignes

**Dépendances :** UC32A (traplist, funclist, symtab), UC32B (CFG pour
détection VBL sans BSR entrant)

**Critère de validation :** démo cible → patterns détectés avec confiance
> 80% pour VBL handler + au moins 1 autre pattern (sinus, copper, blitter)
→ .s commenté versionné dans `use_cases/UC32/`.

---

> **Relation avec UC32 du CLAUDE.md :** Les UC32A/B/C ci-dessus sont le
> découpage technique de UC32 tel que défini au §8 de CLAUDE.md. UC32
> dans CLAUDE.md inclut la phase collaborative manuelle (Tonton Marcel
> valide les annotations) — UC32A/B/C produisent les outils automatiques,
> la session collaborative reste distincte.

---

## Conventions de nommage ST4Ever dans ce module

Conformément aux conventions du projet (notation hongroise, préfixe module,
snake_case, style Allman) :

```c
/* Types opaques — snake_case_t */
typedef struct prg_hdr_s          prg_hdr_t;
typedef struct prg_symbol_s       prg_symbol_t;
typedef struct prg_symtab_s       prg_symtab_t;
typedef struct prg_reloc_set_s    prg_reloc_set_t;
typedef struct prg_segmap_s       prg_segmap_t;
typedef struct prg_func_s         prg_func_t;
typedef struct prg_funclist_s     prg_funclist_t;
typedef struct prg_trap_call_s    prg_trap_call_t;
typedef struct prg_traplist_s     prg_traplist_t;
typedef struct prg_bb_s           prg_bb_t;
typedef struct prg_cfg_s          prg_cfg_t;
typedef struct prg_pattern_s      prg_pattern_t;
typedef struct prg_patternlist_s  prg_patternlist_t;
typedef struct prg_annot_s        prg_annot_t;
typedef struct boot_sector_s      boot_sector_t;

/* Énumérations */
typedef enum prg_bin_type_e {
    PRG_BIN_GEMDOS,
    PRG_BIN_BOOT_SECTOR,
    PRG_BIN_PACKED,
    PRG_BIN_RAW_CODE,
    PRG_BIN_UNKNOWN
} prg_bin_type_t;

typedef enum prg_compiler_e {
    PRG_COMP_GFA,
    PRG_COMP_PURC,
    PRG_COMP_LATTICE,
    PRG_COMP_ASM_DEVPAC,
    PRG_COMP_OMIKRON,
    PRG_COMP_UNKNOWN
} prg_compiler_t;

typedef enum prg_runtime_e {
    PRG_RUNTIME_GFA,
    PRG_RUNTIME_C,
    PRG_RUNTIME_NONE,    /* assembleur pur */
    PRG_RUNTIME_UNKNOWN
} prg_runtime_t;

typedef enum prg_seg_byte_e {
    PRG_BYTE_UNKNOWN,
    PRG_BYTE_CODE,
    PRG_BYTE_DATA,
    PRG_BYTE_JUMP_TABLE,
    PRG_BYTE_STRING
} prg_seg_byte_t;

/* Codes de retour — convention projet */
/* ST_NO_ERROR (0) et ST_ERROR (non-zero) depuis common.h */
```

---

## Notes finales

### Sur la précision des heuristiques

Aucune heuristique n'est parfaite sur des binaires de 40 ans.
L'objectif de ST4Ever est de produire un **score de confiance** pour
chaque décision plutôt qu'une réponse binaire. Un pattern
reconnu à 90% doit être annoté comme tel, laissant à l'analyste
humain la décision finale.

### Sur l'ordre des phases

Les phases 0–4 sont **strictement séquentielles** (chaque phase
utilise les résultats de la précédente). Les phases 5–A peuvent
itérer : identifier une fonction peut révéler de nouvelles cibles
BSR, qui révèlent de nouvelles fonctions, etc. Implémenter une
boucle de fixpoint est recommandé pour UC32B (CFG).

### Lien avec UC32 (détection probabiliste)

La Phase A (`prg_detect_patterns`) est la mise en œuvre
directe du système de scoring UC32 défini dans la spec ST4Ever :
confidence scores pour VBL handlers, blitter loops, sinus tables,
GEMDOS traps, etc. Ce guide fournit les patterns 68k concrets que
UC32 doit chercher.

### Ce qui fonctionne DÉJÀ aujourd'hui (sans UC32A/B/C)

Pour une première séance d'analyse sur machine réelle + ST4Ever :

1. `load <demo.prg>` → TEXT/DATA/BSS size dans `info`
2. `edit <demo.prg>` → vue hex+disasm UC10, parcours du TEXT
3. `edit --hex <demo.prg>` → coloration sémantique secteurs (UC24B)
4. `execute` + F5/F6 → step dynamique, breakpoints, vue CPU/RAM/écran
5. `mount <disk.st>` + B → bootsector hex avec BPB décodé

Ces commandes couvrent les Phases 1, 3 (partiel), 4 (dynamique), B
sans aucun développement supplémentaire.

---

*ST4Ever — Tonton Marcel — Document vivant, mise à jour par session*
*v1.1 : audit hooks réels ST4Ever, UC mappings, propositions UC32A/B/C*
