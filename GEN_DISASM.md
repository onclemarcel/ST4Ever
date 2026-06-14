# GEN.TTP — Feuille de route désassemblage DEVPAC3 HiSoft

> **Objectif UC30F** : désassembler l'assembleur DEVPAC3 d'origine (`tools/ST/GEN.TTP`,
> HiSoft 1985-1993), annoter ses blocs fonctionnels, et comparer l'architecture avec
> celle construite en UC30A-E. Double valeur : pédagogie reverse engineering 68000
> et extraction de patterns pour la phase Revival (UC33+).

---

## 1. Contexte et objectifs

### 1.1 Qu'est-ce que GEN.TTP ?

`GEN.TTP` est le cœur de l'assembleur **DEVPAC3** d'HiSoft (Genesys Assembler,
copyright 1985-1993). C'est le binaire utilisé pour produire `use_cases/UC15A/SOURCE.PRG`
— la référence de torture test de UC30E. En désassemblant GEN.TTP, on ferme la boucle :

```
SOURCE.S  ─(GEN.TTP)──▶  SOURCE.PRG   [déjà validé UC15A + UC30E]
SOURCE.S  ─(ST4Ever)──▶  SOURCE.PRG   [byte-exact UC30E ✓]
GEN.TTP   ─(ST4Ever)──▶  GEN_DISASM.s [UC30F — ce document]
```

### 1.2 Anatomie du binaire

| Champ | Valeur | Source |
|-------|--------|--------|
| Taille fichier | 69 768 octets | `wc -c` |
| Magic | `0x601A` (BRA.S over header) | PRG header [0..1] |
| `.text` | 69 370 octets (`0x10EFA`) | PRG header [2..5] |
| `.data` | 0 octets | PRG header [6..9] |
| `.bss` | 0 octets | PRG header [10..13] |
| Symbol table | 0 octets (stripped) | PRG header [14..17] |
| Reloc table | 370 octets (120 fixups réels + 246 marqueurs skip 0x01) | fin de fichier |
| `prgflags` | `0x0007` (fastload + TTram) | PRG header [22..23] |
| `absflag` | `0x0000` (relocatable) | PRG header [24..25] |

**Conséquence** : pas de table de symboles DRI → toutes les fonctions sont des
`sub_XXXX` ; les 366 fixups indiquent les longwords absolus à remplacer à l'exécution.

### 1.3 Structure d'entrée (.text[0x0000..0x001E])

```
.text:
  +0x0000: 601C          BRA.S   .entry          ; saute par-dessus le bloc version
  +0x0002: 9529          dc.w    $9529           ; checksum interne ?
  +0x0004: 7B008696      dc.l    $7B008696       ; numéro de version HiSoft ?
  +0x0008: ...           dc.b    "Gen(C) HiSoft 1985-93",0   ; 22 octets
  +0x001E: .entry:
  +0x001E: 4EB9 0000B8BE JSR     sub_0000B8BE    ; premier appel → init TTP
```

Premier fixup à `.text[0x0020]` = le longword `0x0000B8BE` de ce JSR.
`sub_0000B8BE` est à l'offset 0xB8BE dans `.text` (=47294 octets depuis le début).

---

## 2. Ce qui est automatisé par ST4Ever (maintenant)

### 2.1 Navigation interactive

```
# Ouvrir GEN.TTP en vue hex+disasm (UC10)
load tools/ST/GEN.TTP
edit tools/ST/GEN.TTP
```

La vue `edit` affiche simultanément hex, ASCII et désassemblage 68000 synchronisé.
Naviguer avec PgUp/PgDn, cliquer sur une ligne pour centrer le désassemblage.

```
# Forcer la vue hex (sans détection de type)
edit -h tools/ST/GEN.TTP
```

### 2.2 Navigation dans la structure via le bandeau (UC24C/D)

Si l'image est une `.st`, le bandeau contextuel (UC24C) affiche les infos de secteur.
Pour un `.PRG` isolé, le bandeau n'est pas actif — naviguer à l'adresse voulue via
la vue hex directement.

### 2.3 Génération automatique via le harness UC30F

Le fichier `use_cases/use_case_30F.c` produit automatiquement, via `make tests` :

| Sortie | Contenu | Localisation |
|--------|---------|--------------|
| `GEN_DISASM.s` | Désassemblage complet .text (≈14 000 lignes) | `use_cases/UC30F/` |
| `GEN_STRINGS.txt` | Chaînes imprimables ≥ 5 chars avec offsets | `use_cases/UC30F/` |
| `GEN_RELOC.txt` | Table des 366 fixups (adresses des longwords relocatables) | `use_cases/UC30F/` |
| `GEN_HEADER.txt` | Décodage complet du header PRG | `use_cases/UC30F/` |

```bash
make tests   # génère tous les fichiers UC30F automatiquement
```

### 2.4 Analyse statique future (UC32A — non encore implémenté)

```
analyze tools/ST/GEN.TTP        # scanner fonctions, TRAPs GEMDOS/XBIOS
analyze --cfg tools/ST/GEN.TTP  # CFG statique, VBL handlers (UC32B)
annotate tools/ST/GEN.TTP       # annotation sémantique (UC32C)
```

Ces commandes sont prévues en UC32A/B/C et amélioreront significativement le
travail d'annotation de GEN.TTP. UC30F pose les bases manuelles que UC32 automatisera.

---

## 3. Ce qui requiert une revue/activité manuelle

### 3.1 Identification des frontières de fonctions

`GEN_DISASM.s` est un flux continu. Sans table de symboles, les frontières de
fonctions ne sont pas marquées. Repères manuels :

- **`RTS` / `RTE` / `JMP (Ax)`** → fin de fonction probable
- **`LINK A6,#-N`** → prologue de fonction avec frame (pattern compilateur typique)
- **`UNLK A6` précédant `RTS`** → épilogue correspondant
- **Cluster de `JSR`** dans la séquence → routine d'orchestration
- **Saut depuis le premier fixup** vers `sub_0000B8BE` → premier bloc à identifier

### 3.2 Identification des tables de données

Le désassembleur traite tout le contenu de `.text` comme du code, mais GEN.TTP
contient des tables de données embarquées dans `.text` :

- Tables de mnémoniques (string pools vus dans `GEN_STRINGS.txt`)
- Tables de sauts (`JMP 0(PC,Dn.W)` + table de longwords)
- Hash tables pour les symboles (si elles existent dans `.text`)

Repérer dans `GEN_DISASM.s` les patterns :
```
dc.w    $4142      ; données ASCII mal-interprétées comme opcodes
dc.w    $0000      ; alignements
```

### 3.3 Reconstruction des structures de données

GEN.TTP maintient en RAM (dans `.bss` synthétique — ici dans la stack frame) :
- Un dictionnaire de symboles (hash table ou arbre binaire ?)
- Un buffer d'entrée pour le lexer
- Un PC assembleur (adresse courante en passe 1 et 2)
- Une liste de fixups en construction

Ces structures sont visibles par les patterns d'accès offset `N(A6)` (variables
locales) ou `N(A5)` / `N(A4)` (variables globales depuis un pointeur de base).

---

## 4. Carte mémoire statique (avant annotation)

Données déduites de l'analyse automatique :

| Adresse `.text` | Contenu identifié | Méthode |
|-----------------|-------------------|---------|
| `0x0000` | BRA.S `.entry` | header structure |
| `0x0002` | `dc.w $9529` — checksum? | header structure |
| `0x0004` | `dc.l $7B008696` — version? | header structure |
| `0x0008` | `"Gen(C) HiSoft 1985-93\0"` | strings |
| `0x001E` | `.entry` : `JSR sub_B8BE` | entry point analysis |
| `0xB8BE` | `sub_0000B8BE` — init principale? | first fixup + JSR target |
| `0x....` | tables d'erreurs (strings) | `GEN_STRINGS.txt` |
| `0x....` | table des mnémoniques | `GEN_STRINGS.txt` + contexte |

> **Note** : les adresses sont relatives au début de `.text` (= adresse de chargement
> TOS = variable). En pratique TOS charge GEN.TTP en RAM libre, typiquement
> vers `0x00001000` ou plus haut selon le TOS. Le désassembleur de ST4Ever utilise
> la base `0x00000000` pour les cibles de branchement — les offsets sont cohérents.

---

## 5. Feuille de route d'annotation manuelle

Ordre recommandé — du plus facile au plus complexe.

### Phase A — Entry point & structure TTP (priorité 1)

**Fichier** : `GEN_DISASM.s` à partir de `.text[0x001E]`

GEN.TTP est un `.TTP` : TOS lui passe des arguments sur la ligne de commande
(chemin source, options). La séquence d'entrée typique :

1. `JSR sub_B8BE` → init générale (stack, variables, écran)
2. Appel à `Pterm0` / `Pterm` (GEMDOS TRAP #1) si erreur fatale précoce
3. Lecture `BASEPAGE` (A0 = `$04(SP)` à l'entrée TOS) → chemin d'accès
4. Parsing command line (buffer à `$80` du basepage)

**Annotation cible** :
```asm
.entry:             ; .text[0x001E]
    JSR     init_gen        ; sub_B8BE — init générale + parsing CLI
    ...
```

### Phase B — Lexer / tokenizer (priorité 1)

**Comment le trouver** : chercher dans `GEN_STRINGS.txt` les strings correspondant
aux directives (`TEXT`, `CODE`, `DATA`, `BSS`, `EVEN`, `INCLUDE=`...). L'offset
de ces strings pointe vers les tables de la routine de lookup de token.

Pattern typique d'un lexer 68000 :
```asm
    MOVE.B  (A0)+,D0        ; lire caractère courant
    CMP.B   #' ',D0         ; espace → ignorer
    BEQ.S   skip_space      ; ...
    CMP.B   #';',D0         ; commentaire → fin de ligne
    ...
    LEA     mnemonic_table,A1
    JSR     lookup_mnemonic
```

**Annotation cible** : fonctions `lexer_next_token`, `lookup_mnemonic`,
`skip_whitespace`, `parse_string`, `parse_number`.

### Phase C — Table des symboles (priorité 1)

GEN.TTP utilise probablement une **hash table** ou un **arbre binaire** pour les
symboles. Repères :

- Recherche d'un pattern `MULU #N,D0` / `AND.W #mask,D0` → hash function
- Accès à un tableau de pointeurs en RAM → bucket table
- Pattern `CMP ... BEQ ... JSR suivant` → résolution de collision linéaire

**Annotation cible** : fonctions `sym_lookup`, `sym_add`, `sym_hash`,
`sym_get_value`.

### Phase D — Moteur 2 passes (priorité 2)

Les deux passes partagent probablement la même routine de dispatch instruction,
avec un flag de passe en variable globale. Repères :

- Variable booléenne testée (`TST.B N(A6)` ou `TST.W gPass`) → sélecteur passe
- Passe 1 : incrémente un PC assembleur, résout les forward references éventuelles
- Passe 2 : émet les octets (`BSR emit_byte` / `JSR write_word`)

**Annotation cible** : `assemble_pass1`, `assemble_pass2`, `emit_byte`,
`emit_word`, `emit_long`.

### Phase E — Encodeur EA (priorité 2)

Comparer avec `as_parse_ea()` et `as_ea_emit()` de UC30A-E. Repères :

- Une fonction avec beaucoup de branches sur mode d'adressage (0-7)
- `MOVEQ #6,D0` / `MULU #n,D0` → calcul MMMRRR
- `CMP.B #'(',...` / `CMP.B #')',...` → parsing `(An)`, `(An)+`, `-(An)`

**Annotation cible** : `parse_ea`, `encode_ea`, `emit_ea_extension`.

### Phase F — Encodeur ALU/MOVE/branchements (priorité 2)

Après le lexer, un grand dispatch sur le mnémonique. Comparer avec les
`as_encode_*()` de UC30C/D. Rechercher :

- Un saut multiple `JMP 0(PC,D0.W)` ou `JMP (A0)` → dispatch table
- Des valeurs immédiates 0x2000, 0x3000, 0xC000, 0x8000 → opcode bases

### Phase G — Output : écriture du PRG (priorité 3)

Repères :
- Appels GEMDOS `Fcreate` (TRAP #1, D0=0x3C) → création fichier
- Écriture du header 28 octets : `0x601A`, tailles sections
- Construction de la reloc table (liste de fixups)
- Appel `Fwrite` en boucle → flush des sections

**Annotation cible** : `prg_write_header`, `prg_write_text`, `emit_fixup`,
`prg_write_reloc`.

### Phase H — Error reporting (priorité 3)

Les messages d'erreur de `GEN_STRINGS.txt` pointent vers les fonctions de rapport :
- `"undefined symbol"` → trouver le BSR/LEA vers cette string
- `"phasing error"` → idem
- Ces fonctions affichent via GEMDOS `Cconws` (TRAP #1, D0=0x09) ou Line-A

---

## 6. Comparaison architecturale prévue avec UC30A-E

| Aspect | GEN.TTP (1988) | ST4Ever UC30A-E (2026) |
|--------|----------------|------------------------|
| Langage | 68000 assembleur pur | C pur (C89/gnu99) |
| Structure | monolithique .text | modules séparés (as.c ~3500L) |
| Symbol table | hash ou BST en RAM | tableau linéaire 4096 entrées (`as_sym_t`) |
| Passes | 2 passes sur fichier | 2 passes en mémoire |
| EA parsing | vraisemblablement hand-rolled | `as_parse_ea()` 12 modes |
| Optimisations | MOVEQ substit., BRA.S, LEA→ADD | non (byte-exact match) |
| Error reporting | GEMDOS Cconws | `LOG_ERROR` + `as_err()` |
| Output | Fcreate/Fwrite PRG | `fwrite` PRG via `as_emit_*` |
| Reloc | construction incrémentale | liste RLE en fin de passe 2 |

> La colonne de droite sera mise à jour au fil de l'annotation de GEN_DISASM.s.

---

## 7. Patterns 68000 à extraire pour UC33 (Revival)

UC30F alimente directement UC33 (squelette C portable). Patterns à documenter :

1. **Dispatch par table** (`JMP 0(PC,D0.W)`) → analogie `switch()` en C
2. **Frame calling convention** (`LINK A6 / UNLK A6`) → stack layout des fonctions
3. **String lookup linéaire** → si GEN.TTP utilise une table triée + comparaison
4. **Hash function** → si symboles par hash : l'algo peut être reproduit en C
5. **Buffer management** → comment GEN.TTP gère ses buffers d'entrée/sortie sans malloc
6. **Optimiseur inline** → MOVEQ substitution, BRA.S: comment l'optimiseur est intégré
   dans la passe 2 sans nécessiter de passe supplémentaire

---

## 8. Workflow session d'annotation (pour reproduire)

### Étape 1 — Générer les fichiers automatiques (une fois)

```bash
make tests    # produit GEN_DISASM.s, GEN_STRINGS.txt, GEN_RELOC.txt
```

### Étape 2 — Ouvrir GEN_DISASM.s dans un éditeur texte

Le fichier fait ~14 000 lignes. Chercher les adresses clés :

```
grep -n "0000B8BE\|sub_0000B8BE" use_cases/UC30F/GEN_DISASM.s
grep -n "BRA.S\|LINK\|UNLK\|RTS" use_cases/UC30F/GEN_DISASM.s | head -50
```

### Étape 3 — Cross-référencer avec GEN_STRINGS.txt

Chaque string listée avec son offset permet de trouver le code qui la référence :

```
grep -n "undefined" use_cases/UC30F/GEN_STRINGS.txt   # offset de la string
# Chercher cet offset dans GEN_DISASM.s pour trouver le LEA/PEA référençant la string
grep -n "00004xxx" use_cases/UC30F/GEN_DISASM.s        # (remplacer xxx par l'offset)
```

### Étape 4 — Annoter GEN_DISASM.s

Remplacer les labels auto-générés par des noms sémantiques :

```asm
; AVANT (généré automatiquement)
sub_0000B8BE:
    LINK    A6,#-$01FE
    JSR     sub_00009192
    ...

; APRÈS (annoté manuellement)
gen_init:           ; entry point principal — init TTP + parsing CLI
    LINK    A6,#-$01FE      ; frame 510 octets de variables locales
    JSR     tty_init        ; init console + écran (sub_9192)
    ...
```

### Étape 5 — Itérer par Phase (A → H)

Suivre l'ordre de la section 5 ci-dessus. Chaque phase produit un paragraphe
de contrats comportementaux pour `6 - UC.md §6.30F`.

### Étape 6 — Validation croisée

Pour chaque bloc annoté, vérifier la cohérence :
- Le code annoté `sym_hash()` produit-il des accès à une table de taille 2^N ?
- La fonction `emit_byte()` écrit-elle effectivement via Fwrite ou un buffer ?
- `parse_ea()` couvre-t-elle les 12 modes EA de UC30B ?

---

## 9. Fichiers produits par UC30F

| Fichier | Généré par | Contenu |
|---------|-----------|---------|
| `use_cases/UC30F/GEN_DISASM.s` | `use_case_30F.c` (automatique) | Désassemblage complet .text, ≈14 000 lignes |
| `use_cases/UC30F/GEN_STRINGS.txt` | `use_case_30F.c` (automatique) | Chaînes imprimables avec offsets hex |
| `use_cases/UC30F/GEN_RELOC.txt` | `use_case_30F.c` (automatique) | Table des 366 fixups |
| `use_cases/UC30F/GEN_HEADER.txt` | `use_case_30F.c` (automatique) | Décodage header PRG |
| `use_cases/UC30F/GEN_DISASM_annotated.s` | **Manuel** (Tonton + Claude) | Version annotée avec noms de fonctions |

---

*Dernière mise à jour : 2026-06-14 — UC30F Phase 1*
