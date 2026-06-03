#!/usr/bin/env python3
"""
assemble.py -- Assemble tous les fichiers ./SRC/*.S en ./PRG/*.PRG
via GEN.TTP lance dans Hatari.

Structure attendue :
  ./GEN.TTP          assembleur TOS
  ./SRC/FOO.S        source(s)
  ./PRG/FOO.PRG      binaires generes (crees automatiquement)
"""

import subprocess
import sys
import os
import shutil
import tempfile
import glob

# -- Configuration -------------------------------------------------------------

HATARI_BIN  = "hatari"
TOS_ROM     = r"C:\msys\mingw64\share\hatari\tos104fr.rom"
GEN_TTP     = "./GEN.TTP"
SRC_DIR     = "./SRC"
PRG_DIR     = "./PRG"

HATARI_EXTRA_OPTS = [
    "--tos",     TOS_ROM,
    "--memsize", "1",
]

# -- Helpers -------------------------------------------------------------------

def find_hatari():
    if shutil.which(HATARI_BIN):
        return HATARI_BIN
    for p in os.environ.get("PATH", "").split(os.pathsep):
        candidate = os.path.join(p, HATARI_BIN + ".exe")
        if os.path.isfile(candidate):
            return candidate
    return None

def to_tos_path(host_abs, work_dir):
    """Chemin hote absolu -> chemin TOS relatif a work_dir (ex. SRC\\FOO.S)."""
    rel = os.path.relpath(host_abs, work_dir)
    return rel.replace('/', '\\').upper()

# -- Script debugger Hatari ----------------------------------------------------

def build_debugger_script(arg_tos):
    """
    Genere le script debugger Hatari qui ecrit arg_tos dans la basepage
    du programme autodemarre, puis quitte Hatari apres Pterm().

    Format basepage TOS a l'offset 0x80 :
      octet 0     : longueur de la cmdline (sans NUL final)
      octets 1..N : argument ASCII
      octet N+1   : 0x00
    """
    arg_bytes = bytes([len(arg_tos)]) + arg_tos.encode('ascii') + b'\x00'
    write_cmds = "\n".join(
        "wb BASEPAGE+{} {}".format(hex(0x80 + i), hex(v))
        for i, v in enumerate(arg_bytes)
    )
    # Attention : ce script doit etre ecrit en ASCII pur (pas d'accents, pas de tirets em)
    return (
        "# Script debugger Hatari -- genere par assemble.py\n"
        "# Injecte \"{}\" dans la basepage, quitte apres Pterm.\n"
        "\n"
        "# Breakpoint unique au premier arret (juste apres boot)\n"
        "b pc=PC :once\n"
        "c\n"
        "{}\n"
        "\n"
        "# Quitter quand le programme appelle Pterm (d0=76)\n"
        "b d0==76 :once\n"
        "c\n"
        "quit\n"
    ).format(arg_tos, write_cmds)

# -- Lancement d'un assemblage -------------------------------------------------

def assemble_one(hatari_bin, work_dir, src_abs, prg_abs):
    """
    Lance Hatari pour assembler src_abs -> prg_abs.
    Retourne True si le .PRG a ete produit, False sinon.
    """
    src_tos = to_tos_path(src_abs, work_dir)          # ex. SRC\ADDX.S
    gen_tos = "C:\\" + to_tos_path(                   # C:\GEN.TTP
        os.path.abspath(GEN_TTP), work_dir
    )

    # GEN.TTP produit le .PRG dans le meme repertoire que la source
    intermediate_prg = os.path.join(
        os.path.dirname(src_abs),
        os.path.splitext(os.path.basename(src_abs))[0].upper() + ".PRG"
    )

    dbg_content = build_debugger_script(src_tos)

    # Fichier temporaire en ASCII pur
    tmp = tempfile.NamedTemporaryFile(
        mode='w', suffix='.ini', delete=False, encoding='ascii', errors='replace'
    )
    tmp.write(dbg_content)
    tmp.close()

    cmd = (
        [hatari_bin]
        + HATARI_EXTRA_OPTS
        + [
            "--gemdos-drive", work_dir,
            "--auto",         gen_tos,
            "--parse",        tmp.name,
        ]
    )

    print("  Commande : {}".format(' '.join(cmd)))
    ok = False
    try:
        subprocess.run(cmd, check=True)

        if os.path.isfile(intermediate_prg):
            os.makedirs(os.path.dirname(prg_abs), exist_ok=True)
            shutil.move(intermediate_prg, prg_abs)
            ok = True
        else:
            print("  [ATTENTION] fichier intermediaire '{}' absent.".format(
                intermediate_prg), file=sys.stderr)
    except subprocess.CalledProcessError as e:
        print("  [ERREUR] Hatari a retourne le code {}.".format(
            e.returncode), file=sys.stderr)
    finally:
        os.unlink(tmp.name)

    return ok

# -- Verifications prealables --------------------------------------------------

def check_requirements(hatari_bin):
    errors = []
    if not hatari_bin:
        errors.append("'{}' introuvable dans le PATH.".format(HATARI_BIN))
    if not os.path.isfile(GEN_TTP):
        errors.append("Assembleur '{}' absent du repertoire courant.".format(GEN_TTP))
    if not os.path.isdir(SRC_DIR):
        errors.append("Repertoire source '{}' introuvable.".format(SRC_DIR))
    if errors:
        for e in errors:
            print("[ERREUR] {}".format(e), file=sys.stderr)
        sys.exit(1)

# -- Point d'entree ------------------------------------------------------------

if __name__ == "__main__":
    hatari_bin = find_hatari()
    check_requirements(hatari_bin)

    work_dir = os.path.abspath(".")
    os.makedirs(PRG_DIR, exist_ok=True)

    sources = sorted(
        glob.glob(os.path.join(SRC_DIR, "*.S")) +
        glob.glob(os.path.join(SRC_DIR, "*.s"))
    )

    if not sources:
        print("[ERREUR] Aucun fichier .S trouve dans '{}'.".format(SRC_DIR),
              file=sys.stderr)
        sys.exit(1)

    print("[INFO] TOS ROM    : {}".format(TOS_ROM))
    print("[INFO] Assembleur : {}".format(os.path.abspath(GEN_TTP)))
    print("[INFO] Sources    : {} fichier(s) dans '{}'".format(len(sources), SRC_DIR))
    print("[INFO] Sortie     : '{}'".format(PRG_DIR))
    print()

    success, failure = [], []

    for src_abs in [os.path.abspath(s) for s in sources]:
        stem    = os.path.splitext(os.path.basename(src_abs))[0].upper()
        prg_abs = os.path.abspath(os.path.join(PRG_DIR, stem + ".PRG"))

        print("[{}] {} -> {}".format(
            stem,
            os.path.relpath(src_abs),
            os.path.relpath(prg_abs)
        ))

        if assemble_one(hatari_bin, work_dir, src_abs, prg_abs):
            size = os.path.getsize(prg_abs)
            print("  [OK] {} ({} octets)".format(prg_abs, size))
            success.append(stem)
        else:
            print("  [ECHEC] {}".format(stem), file=sys.stderr)
            failure.append(stem)
        print()

    print("-" * 50)
    print("Resultat : {} OK, {} echec(s)".format(len(success), len(failure)))
    if failure:
        print("Echecs : {}".format(', '.join(failure)), file=sys.stderr)
        sys.exit(1)