# -*- coding: utf-8 -*-
"""
Générateur universel de fichiers DEVPAC à partir du gist m68k-all-instructions.asm

Fonctionnalités :
- Génère ALL.S ou un fichier par mnémonique (--all / --split)
- Injecte valeurs limites pour i8/i16/i32/xxx
- Rotation des registres Data / Address / Index
- Variation pseudo-aléatoire déterministe (--seed N)
- Code DEVPAC propre, stable, idéal pour tests assembleur <-> désassembleur

Usage :
    python generate_m68k_tests.py --input m68k-all-instructions.asm --all
    python generate_m68k_tests.py --input m68k-all-instructions.asm --split
    python generate_m68k_tests.py --input m68k-all-instructions.asm --all --seed 1234
"""
# Directives assembleur AS / DEVPAC à ignorer (pas des mnémoniques 68000).
# Toute ligne dont le premier mot correspond est sautée.
ASSEMBLER_DIRECTIVES = {
    'cpu', 'padding', 'page', 'supmode', 'off', 'on',
    'end', 'text', 'data', 'bss', 'section', 'even', 'odd',
    'dc', 'ds', 'dcb', 'include', 'incbin', 'org', 'rorg',
}

# Instructions 68000 pas encore décodées par le désassembleur ST4Ever.
# Les inclure générerait des tests qui échoueraient (DC.W attendu ≠ mnemonic).
# Retirer une entrée ici quand le désassembleur gagne le support.
UNIMPLEMENTED_MNEMONICS = {
    'movep', 'movem',                      # déplacement multiple / périphérique
    'nbcd', 'abcd', 'sbcd',               # BCD
    'chk',                                 # vérification de borne
    'trapv',                               # TRAP on overflow (0x4E76, pas dans UC14)
    # Alias de branches — produits identiques mais mnemonic diff du désassembleur
    'bhs', 'blo',                          # BHS=BCC, BLO=BCS (g_aszBcc utilise BCC/BCS)
    # DBcc — toutes variantes + alias courants
    'dbf',  'dbra',                        # DBF/DBRA = alias (boucle while N≥0)
    'dbt',                                 # DBT = toujours vrai (jamais boucle)
    'dbcc', 'dbcs', 'dbeq', 'dbge',
    'dbgt', 'dbhi', 'dbhs', 'dble', 'dblo', 'dbls', 'dblt',
    'dbmi', 'dbne', 'dbpl', 'dbvc', 'dbvs',
    # Scc — toutes variantes + alias GEN.TTP
    'sf', 'st',
    'scc', 'scs', 'seq', 'sge', 'sgt',
    'shi', 'shs', 'sle', 'slo', 'sls', 'slt', 'smi',
    'sne', 'spl', 'svc', 'svs',
    'tas', 'illegal', 'reset', 'stop',     # divers privilégiés / spéciaux
    'bkpt',                                # breakpoint 68010+
}

import argparse
import os
import re
import random
from collections import defaultdict

SRC_DIR = "SRC/"

# ------------------------------------------------------------
# 1. Valeurs limites (toujours présentes en premier)
# ------------------------------------------------------------
IMM_VALUES = {
    "b": ["#$00", "#$7F", "#$80", "#$FF"],
    "w": ["#$0000", "#$7FFF", "#$8000", "#$FFFF"],
    "l": ["#$00000000", "#$7FFFFFFF", "#$80000000", "#$FFFFFFFF"],
}

ADDR_VALUES = [
    "$0000",
    "$1234",
    "$7FFF",
    "$8000",
    "$C00000",
    "$00C00000",
]

DISPLACEMENTS = ["0", "2", "-2", "64", "-64", "127", "-128"]

# ------------------------------------------------------------
# 2. Registres pour rotation
# ------------------------------------------------------------
DATA_REGS = [f"D{i}" for i in range(8)]
ADDR_REGS = [f"A{i}" for i in range(7)]  # A7 exclu pour éviter SP
INDEX_REGS = [
    "D0.W", "D1.W", "D2.W", "A0.W", "A1.W", "A2.W",
    "D0.L", "D1.L", "D2.L", "A0.L", "A1.L", "A2.L"
]

# ------------------------------------------------------------
# 3. Générateur circulaire
# ------------------------------------------------------------
def ring(lst):
    """Générateur circulaire infini."""
    while True:
        for x in lst:
            yield x

# ------------------------------------------------------------
# 4. Expansion des placeholders + rotation + pseudo-aléatoire
# ------------------------------------------------------------
def expand_placeholders(line, size_suffix, rng):
    """
    Remplace i8/i16/i32/xxx et injecte registres + index + displacement.
    rng = instance de random.Random(seed)
    """
    variants = []

    # Détection immédiats
    if "#i8" in line:
        key = "b"
        placeholder = "#i8"
    elif "#i16" in line:
        key = "w"
        placeholder = "#i16"
    elif "#i32" in line:
        key = "l"
        placeholder = "#i32"
    else:
        key = None
        placeholder = None

    imm_list = IMM_VALUES.get(key, [None])
    addr_list = ADDR_VALUES if "xxx" in line else [None]

    # Valeurs pseudo-aléatoires supplémentaires (mais après les limites)
    # Préfixe $ obligatoire pour DEVPAC3 (ex: #$3F, pas #3F)
    if key == "b":
        imm_list = imm_list + [f"#${rng.randint(0, 255):02X}"]
    elif key == "w":
        imm_list = imm_list + [f"#${rng.randint(0, 65535):04X}"]
    elif key == "l":
        imm_list = imm_list + [f"#${rng.randint(0, 2**32 - 1):08X}"]

    # Génération combinée
    for imm in imm_list:
        for addr in addr_list:
            l = line

            # Remplacement immédiat
            if placeholder and imm:
                l = l.replace(placeholder, imm)

            # Remplacement adresse symbolique
            if addr:
                l = l.replace("xxx", addr)

            # Injection registre Data
            if "DREG" in l:
                l = l.replace("DREG", next(expand_placeholders.data_cycle))

            # Injection registre Address
            if "AREG" in l:
                l = l.replace("AREG", next(expand_placeholders.addr_cycle))

            # Injection displacement
            if "DISP" in l:
                l = l.replace("DISP", next(expand_placeholders.disp_cycle))

            # Injection index register
            if "XREG" in l:
                l = l.replace("XREG", next(expand_placeholders.index_cycle))

            variants.append(l)

    return variants

# cycles globaux
expand_placeholders.data_cycle = ring(DATA_REGS)
expand_placeholders.addr_cycle = ring(ADDR_REGS)
expand_placeholders.index_cycle = ring(INDEX_REGS)
expand_placeholders.disp_cycle = ring(DISPLACEMENTS)

# ------------------------------------------------------------
# 5. Parsing d'une ligne du gist
# ------------------------------------------------------------
def parse_gist_line(line):
    """Extrait (mnemonic, size, operands) d'une ligne du gist.

    Retourne None pour :
    - lignes vides ou commentaires purs
    - directives assembleur (cpu, padding, end, dc, …)
    - équates  (contiennent '=')
    - mnémoniques non encore supportés par le désassembleur ST4Ever
    """
    code = line.split(";", 1)[0].strip()
    if not code:
        return None

    # Équates : « xxx = $200 », « i8 = 126 », etc.
    if "=" in code:
        return None

    parts = code.split(None, 1)
    if len(parts) == 1:
        mnemonic_full = parts[0]
        operands = ""
    else:
        mnemonic_full, operands = parts

    m = re.match(r"([a-z]+)(\.[bwl])?$", mnemonic_full, re.IGNORECASE)
    if not m:
        return None

    mnemonic = m.group(1).upper()
    size = (m.group(2) or "").lower()

    # Directives assembleur (CPU, PADDING, PAGE, SUPMODE, END, …)
    if mnemonic.lower() in ASSEMBLER_DIRECTIVES:
        return None

    # Instructions non encore décodées par le désassembleur ST4Ever
    if mnemonic.lower() in UNIMPLEMENTED_MNEMONICS:
        return None

    # Remplacement des registres du gist par placeholders
    operands = operands.replace("d0", "DREG")
    operands = operands.replace("a0", "AREG")
    operands = operands.replace("i16(", "DISP(")
    operands = operands.replace("i8(", "DISP(")
    operands = operands.replace(",d1.", ",XREG.")
    operands = operands.replace(",a1.", ",XREG.")

    return mnemonic, size, operands.strip()

# ------------------------------------------------------------
# 6. Génération complète
# ------------------------------------------------------------
def generate_from_gist(input_path, mode_all=True, out_dir=".", seed=None):
    rng = random.Random(seed)

    with open(input_path, "r", encoding="utf-8") as f:
        lines = f.readlines()

    by_mnemonic = defaultdict(list)

    for line in lines:
        stripped = line.strip()
        if not stripped or stripped.startswith(";"):
            continue

        parsed = parse_gist_line(stripped)
        if not parsed:
            continue

        mnemonic, size, operands = parsed
        base_line = f"{mnemonic}{size} {operands}".strip()

        expanded = expand_placeholders(base_line, size, rng)
        by_mnemonic[mnemonic].extend(expanded)

    if mode_all:
        out_path = os.path.join(out_dir, SRC_DIR + "ALL.S")
        with open(out_path, "w", encoding="utf-8") as out:
            out.write("        TEXT\n\n")
            for mnemonic in sorted(by_mnemonic.keys()):
                out.write(f"; ===== {mnemonic} =====\n")
                for l in by_mnemonic[mnemonic]:
                    out.write(l.upper() + "\n")
                out.write("\n")
            out.write("        END\n")
        print(f"Fichier {out_path} généré.")
    else:
        for mnemonic, instrs in by_mnemonic.items():
            out_path = os.path.join(out_dir, SRC_DIR + f"{mnemonic}.S")
            with open(out_path, "w", encoding="utf-8") as out:
                out.write("        TEXT\n\n")
                out.write(f"; ===== {mnemonic} =====\n")
                for l in instrs:
                    out.write(l.upper() + "\n")
                out.write("\n        END\n")
            print(f"Fichier {out_path} généré.")

# ------------------------------------------------------------
# 7. CLI
# ------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True)
    parser.add_argument("--all", action="store_true")
    parser.add_argument("--split", action="store_true")
    parser.add_argument("--seed", type=int, default=None)
    parser.add_argument("--outdir", default=".")
    args = parser.parse_args()

    if not args.all and not args.split:
        args.all = True

    generate_from_gist(args.input, mode_all=args.all, out_dir=args.outdir, seed=args.seed)

if __name__ == "__main__":
    main()
