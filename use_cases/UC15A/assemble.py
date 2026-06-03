#!/usr/bin/env python3
"""
assemble.py -- Assemble tous les fichiers ./SRC/*.S en ./PRG/*.PRG
via GEN.TTP lance dans Hatari.

Architecture (simplifiee) :
  1. Un minuscule PRG lanceur est genere en Python pur (68K hand-coded).
     Il appelle Pexec(0, "C:\\GEN.TTP", "SRC\\FOO.S", NULL) puis Pterm(0).
     Pas de basepage injection, pas de debugger chain.
  2. Hatari autostart ce PRG via --auto.
  3. Un script debugger minimal (un seul VBL timeout) ferme Hatari apres N VBLs.

Structure attendue :
  ./GEN.TTP          assembleur TOS (DEVPAC3)
  ./SRC/FOO.S        source(s) DEVPAC3
  ./PRG/FOO.PRG      binaires generes
"""

import struct
import subprocess
import sys
import os
import shutil
import glob

# -- Configuration -------------------------------------------------------------

HATARI_BIN_CANDIDATES = [
    r"C:\Users\march\hatari-v2.6.1\hatari.exe",
    "hatari",
    "hatari.exe",
]

TOS_ROM  = r"C:\msys\mingw64\share\hatari\tos104fr.rom"
GEN_TTP  = "./GEN.TTP"      # relatif au repertoire courant (= C:\ dans Hatari)
SRC_DIR  = "./SRC"
PRG_DIR  = "./PRG"

# VBLs attendus avant fermeture Hatari (50 Hz). Avec --fast-forward, chaque
# VBL emule est quasi-instantane, donc meme 2000 VBLs = quelques ms reelles.
ASSEMBLE_VBLS = 2000

HATARI_EXTRA_OPTS = [
    #"--tos",          TOS_ROM,
    "--memsize",      "1",
    "--fast-boot",    "yes",
    "--fast-forward", "yes",
    "--timer-d",      "yes",
    "--sound",        "off",
    "--confirm-quit", "no",
    "--log-level",    "fatal",
]

# -- Helpers binaires 68K -------------------------------------------------------

def _cconws(pea_offset, str_offset):
    """
    Cconws (GEMDOS 9) : affiche une chaine NUL-terminee sur la console TOS.
    PEA str(PC) + MOVE.W #9,-(SP) + TRAP #1 + ADDQ.L #6,SP = 12 bytes.
    pea_offset : offset de l'instruction PEA dans la section text.
    str_offset : offset de la chaine dans la section text.
    """
    # PEA d16(PC) : EA = (pea_offset + 2) + d16
    d16 = str_offset - (pea_offset + 2)
    return (
        b'\x48\x7A' + struct.pack('>h', d16) +  # PEA str(PC)       4 bytes
        b'\x3F\x3C\x00\x09' +                    # MOVE.W #9,-(SP)   4 bytes
        b'\x4E\x41' +                             # TRAP #1           2 bytes
        b'\x5D\x8F'                               # ADDQ.L #6,A7      2 bytes
    )  # total: 12 bytes

def _crawcin():
    """
    Crawcin (GEMDOS 7) : attend une touche sans echo.
    MOVE.W #7,-(SP) + TRAP #1 + ADDQ.L #2,SP = 8 bytes.
    """
    return (
        b'\x3F\x3C\x00\x07' +  # MOVE.W #7,-(SP)   4 bytes
        b'\x4E\x41' +           # TRAP #1            2 bytes
        b'\x55\x8F'             # ADDQ.L #2,A7       2 bytes
    )  # total: 8 bytes


# -- PRG lanceur DEBUG (interactif) -------------------------------------------
#
# Structure du code (86 octets = 0x56) :
#   0x00 CCONWS(msg1)     12 bytes  "ST4Ever> "
#   0x0C CCONWS(src_disp) 12 bytes  src_tos + CRLF
#   0x18 CRAWCIN           8 bytes  --- pause 1 : verifier les args ---
#   0x20 PEXEC            24 bytes  Pexec(0, GEN.TTP, tail, NULL)
#   0x38 CCONWS(msg2)     12 bytes  "Done - press key\r\n"
#   0x44 CRAWCIN           8 bytes  --- pause 2 : voir le resultat ---
#   0x4C PTERM            10 bytes
#   0x56 <data>
#
# PEA d16(PC) :  EA = (adresse du mot d16) + d16
#   = (pea_offset + 2) + d16  =>  d16 = target - (pea_offset + 2)

def make_debug_launcher_prg(gen_tos, src_tos):
    """
    Lanceur interactif avec affichage console et pauses (debug).
    Le programme affiche les arguments, attend une touche, appelle GEN.TTP,
    affiche "Done", attend une touche, puis termine.
    """
    CODE_SIZE = 0x56   # taille fixe de la section code

    # -- Donnees --
    msg1     = b"ST4Ever> \x00"                               # 10 bytes (even)
    src_disp = src_tos.encode("ascii") + b"\r\n\x00"
    if len(src_disp) % 2: src_disp += b"\x00"                # pad to even
    msg2     = b"Done - press any key...\r\n\x00"             # 26 bytes (even)
    prog_b   = gen_tos.encode("ascii") + b"\x00"
    if len(prog_b) % 2: prog_b += b"\x00"                    # pad to even
    tail_b   = bytes([len(src_tos)]) + src_tos.encode("ascii") + b"\x00"

    # Offsets absolus dans la section text
    off_msg1     = CODE_SIZE
    off_src_disp = off_msg1 + len(msg1)
    off_msg2     = off_src_disp + len(src_disp)
    off_prog     = off_msg2 + len(msg2)
    off_tail     = off_prog + len(prog_b)

    # -- Code --
    code = bytearray()

    # 0x00: CCONWS(msg1)
    code += _cconws(0x00, off_msg1)         # 12 bytes → 0x00..0x0B
    # 0x0C: CCONWS(src_disp)
    code += _cconws(0x0C, off_src_disp)     # 12 bytes → 0x0C..0x17
    # 0x18: CRAWCIN — pause 1
    code += _crawcin()                       #  8 bytes → 0x18..0x1F

    # 0x20: PEXEC(0, prog, tail, NULL)
    # CLR.L -(SP) at 0x20, extension du PEA tail a 0x24, du PEA prog a 0x28
    d16_tail = off_tail - 0x24
    d16_prog = off_prog - 0x28
    code += b"\x42\xA7"                                       # CLR.L -(SP)
    code += b"\x48\x7A" + struct.pack(">h", d16_tail)        # PEA tail(PC)
    code += b"\x48\x7A" + struct.pack(">h", d16_prog)        # PEA prog(PC)
    code += b"\x3F\x3C\x00\x00"                              # MOVE.W #0,-(SP)
    code += b"\x3F\x3C\x00\x4B"                              # MOVE.W #$4B,-(SP)
    code += b"\x4E\x41"                                       # TRAP #1
    code += b"\x4F\xEF\x00\x10"                              # LEA 16(SP),SP
    # 0x38: CCONWS(msg2)
    code += _cconws(0x38, off_msg2)         # 12 bytes → 0x38..0x43
    # 0x44: CRAWCIN — pause 2
    code += _crawcin()                       #  8 bytes → 0x44..0x4B

    # 0x4C: PTERM(0)
    code += b"\x3F\x3C\x00\x00"                              # MOVE.W #0,-(SP)
    code += b"\x3F\x3C\x00\x4C"                              # MOVE.W #$4C,-(SP)
    code += b"\x4E\x41"                                       # TRAP #1

    assert len(code) == CODE_SIZE, \
        "code size {} != expected {}".format(len(code), CODE_SIZE)

    code += msg1 + src_disp + msg2 + prog_b + tail_b

    text_size = len(code)
    header  = struct.pack(">H", 0x601A)
    header += struct.pack(">I", text_size)
    header += struct.pack(">I", 0) * 5         # data/bss/sym/reserved/flags
    header += struct.pack(">H", 1)             # abs_flag=1
    assert len(header) == 28

    return bytes(header) + bytes(code)


# -- PRG lanceur 68K (genere en Python pur) ------------------------------------
#
# Code 68K genere (PC-relatif, abs_flag=1 => pas de fixup) :
#
#   0x00: CLR.L  -(SP)                ; env = NULL (4 bytes)
#   0x02: PEA    prog_str(PC)         ; -> "C:\GEN.TTP\0"
#   0x06: PEA    tail_str(PC)         ; -> cmdline tail (len + "SRC\FOO.S\0")
#   0x0A: MOVE.W #0,    -(SP)         ; mode = 0 (Load & Execute)
#   0x0E: MOVE.W #0x4B, -(SP)         ; GEMDOS Pexec
#   0x12: TRAP   #1
#   0x14: LEA    16(SP), SP           ; nettoyage pile (2+2+4+4+4=16 bytes)
#   0x18: MOVE.W #0,    -(SP)         ; exitcode = 0
#   0x1C: MOVE.W #0x4C, -(SP)         ; GEMDOS Pterm
#   0x20: TRAP   #1
#   ---- donnees ----
#   0x22: prog_str  (NUL-terminate, longueur paire)
#   0x22+N: tail_str (octet longueur + chaine + NUL)

def make_launcher_prg(genttp_tos, src_tos):
    """
    Genere un PRG TOS minimal qui appelle GEN.TTP avec src_tos en argument.

    genttp_tos : chemin TOS de GEN.TTP  ex. "C:\\GEN.TTP"
    src_tos    : chemin TOS du source   ex. "SRC\\ADD.S"
    Retourne les bytes du PRG (header 28 octets + code).
    """
    # Chaine du programme a lancer
    prog_bytes = genttp_tos.encode("ascii") + b"\x00"
    if len(prog_bytes) % 2:
        prog_bytes += b"\x00"           # padding pour alignement word

    # Tail GEMDOS : [longueur][chaine][NUL]  (meme format que BASEPAGE+0x80)
    tail_bytes = bytes([len(src_tos)]) + src_tos.encode("ascii") + b"\x00"

    # Offsets dans la section text (relatifs au debut du code = 0x00)
    PROG_OFF = 0x22
    TAIL_OFF = PROG_OFF + len(prog_bytes)

    # Convention GEMDOS Pexec(mode, prog, tail, env) :
    # Les parametres sont empiles a l'ENVERS (env en premier, prog en dernier
    # avant mode+function). Ordre des PEA : tail D'ABORD, prog ENSUITE.
    #
    # PEA d16(PC) :  EA = (adresse du mot d16) + d16
    #   PEA tail a 0x02 : mot d16 a 0x04  → d16 = TAIL_OFF - 0x04
    #   PEA prog a 0x06 : mot d16 a 0x08  → d16 = PROG_OFF - 0x08
    d16_tail_first  = TAIL_OFF - 0x04   # PEA tail en premier (0x02)
    d16_prog_second = PROG_OFF - 0x08   # PEA prog en second  (0x06)

    code = bytearray()
    code += b"\x42\xA7"                                         # CLR.L -(SP)       env=NULL
    code += b"\x48\x7A" + struct.pack(">h", d16_tail_first)    # PEA tail(PC)  ← tail en 1er
    code += b"\x48\x7A" + struct.pack(">h", d16_prog_second)   # PEA prog(PC)  ← prog en 2nd
    code += b"\x3F\x3C\x00\x00"                                # MOVE.W #0,-(SP)   mode
    code += b"\x3F\x3C\x00\x4B"                                # MOVE.W #$4B,-(SP) Pexec
    code += b"\x4E\x41"                                    # TRAP #1
    code += b"\x4F\xEF\x00\x10"                           # LEA 16(SP),SP
    code += b"\x3F\x3C\x00\x00"                           # MOVE.W #0,-(SP)   exitcode
    code += b"\x3F\x3C\x00\x4C"                           # MOVE.W #$4C,-(SP) Pterm
    code += b"\x4E\x41"                                    # TRAP #1

    assert len(code) == 0x22, "Taille code incorrecte: {}".format(len(code))

    code += prog_bytes
    code += tail_bytes

    text_size = len(code)
    header  = struct.pack(">H", 0x601A)  # magic
    header += struct.pack(">I", text_size)
    header += struct.pack(">I", 0)       # data_size
    header += struct.pack(">I", 0)       # bss_size
    header += struct.pack(">I", 0)       # sym_size
    header += struct.pack(">I", 0)       # reserved
    header += struct.pack(">I", 0)       # flags
    header += struct.pack(">H", 1)       # abs_flag = 1 (PC-relatif, pas de fixup)

    assert len(header) == 28
    return bytes(header) + bytes(code)

# -- Scripts debugger (minimal) ------------------------------------------------

def build_quit_scripts(work_dir):
    """
    Genere deux fichiers dans work_dir/_dbg_tmp :
      start.ini : arme un timeout VBL puis quitte
      quit.ini  : ferme Hatari
    Retourne le chemin de start.ini.
    """
    tmp = os.path.join(work_dir, "_dbg_tmp")
    os.makedirs(tmp, exist_ok=True)

    quit_ini  = os.path.join(tmp, "quit.ini")
    start_ini = os.path.join(tmp, "start.ini")

    with open(quit_ini, "w", encoding="ascii") as f:
        f.write("quit\n")

    with open(start_ini, "w", encoding="ascii") as f:
        f.write((
            "# Fermer Hatari apres {} VBLs (GEN.TTP a assemble depuis longtemps)\n"
            "b VBL = VBL+{} :trace :once :file {}\n"
        ).format(ASSEMBLE_VBLS, ASSEMBLE_VBLS,
                 quit_ini.replace("\\", "\\\\")))

    return start_ini

# -- Helpers -------------------------------------------------------------------

def find_hatari():
    for c in HATARI_BIN_CANDIDATES:
        if os.path.isfile(c):
            return c
        if shutil.which(c):
            return c
    return None

def to_tos_path(host_abs, work_dir):
    """Chemin hote -> chemin TOS relatif au GEMDOS root (C:\\)."""
    rel = os.path.relpath(host_abs, work_dir)
    return rel.replace("/", "\\").upper()

# -- Assemblage d'un fichier ---------------------------------------------------

def find_output_prg(work_dir, stem):
    """
    Cherche STEM.PRG dans tout work_dir (GEN.TTP peut le produire dans C:\
    ou dans C:\SRC\ selon sa version). Retourne le chemin trouve ou None.
    """
    target = stem.upper() + ".PRG"
    for root, _dirs, files in os.walk(work_dir):
        for fname in files:
            if fname.upper() == target:
                return os.path.join(root, fname)
    return None


def assemble_one(hatari_bin, work_dir, src_abs, prg_abs, _unused,
                 debug_prg=False):
    """Lance Hatari pour assembler src_abs -> prg_abs. Retourne True si OK."""

    src_tos = to_tos_path(src_abs, work_dir)
    gen_tos = "C:\\" + to_tos_path(os.path.abspath(GEN_TTP), work_dir)
    run_prg = os.path.join(work_dir, "RUN.PRG")
    stem    = os.path.splitext(os.path.basename(src_abs))[0].upper()

    # Ecrire le lanceur PRG (debug ou production)
    if debug_prg:
        launcher = make_debug_launcher_prg(gen_tos, src_tos)
        print("  [DEBUG] RUN.PRG interactif — lisez l'ecran Hatari et appuyez sur une touche.")
    else:
        launcher = make_launcher_prg(gen_tos, src_tos)
    with open(run_prg, "wb") as f:
        f.write(launcher)

    cmd = (
        [hatari_bin]
        + HATARI_EXTRA_OPTS
        + [
            "--harddrive", work_dir,
            "--auto",      "C:\\RUN.PRG",
        ]
    )
    print("  cmd: {}".format(
        " ".join('"{}"'.format(a) if " " in a else a for a in cmd)
    ))

    ok     = False
    proc   = None
    import time
    try:
        proc = subprocess.Popen(cmd)

        # Polling : on attend que STEM.PRG apparaisse n'importe ou dans work_dir.
        # Hatari GEMDOS HD reflète les ecritures TOS en temps reel sur l'hote.
        deadline = time.time() + 30   # 30 s max (fast-forward : en pratique < 5 s)
        found    = None
        while time.time() < deadline:
            time.sleep(0.2)
            found = find_output_prg(work_dir, stem)
            if found:
                time.sleep(0.5)   # laisser GEN.TTP finir l'ecriture
                break

        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()

        if found:
            os.makedirs(os.path.dirname(prg_abs), exist_ok=True)
            if found != prg_abs:
                shutil.move(found, prg_abs)
            ok = True
            print("  [FOUND] {} -> {}".format(
                os.path.relpath(found), os.path.relpath(prg_abs)))
        else:
            print("  [ERREUR] {} non produit apres 30 s.".format(
                stem + ".PRG"), file=sys.stderr)

    except Exception as e:
        print("  [ERREUR] {}".format(e), file=sys.stderr)
        if proc:
            proc.kill()
    #finally:
        #if os.path.isfile(run_prg):
        #    os.remove(run_prg)

    return ok

# -- Verifications prealables --------------------------------------------------

def check_requirements(hatari_bin):
    errors = []
    if not hatari_bin:
        errors.append("Hatari introuvable. Ajuster HATARI_BIN_CANDIDATES.")
    if not os.path.isfile(TOS_ROM):
        errors.append("TOS ROM absente : {}".format(TOS_ROM))
    if not os.path.isfile(GEN_TTP):
        errors.append("Assembleur absent : {}".format(GEN_TTP))
    if not os.path.isdir(SRC_DIR):
        errors.append("Repertoire sources absent : {}".format(SRC_DIR))
    if errors:
        for e in errors:
            print("[ERREUR] {}".format(e), file=sys.stderr)
        sys.exit(1)

# -- Point d'entree ------------------------------------------------------------

if __name__ == "__main__":
    import argparse as _ap
    _p = _ap.ArgumentParser()
    _p.add_argument("--trace", action="store_true",
                    help="Active --trace os_base (diagnostic)")
    _p.add_argument("--only", metavar="STEM",
                    help="N'assemble qu'un fichier (ex: --only ADD)")
    _p.add_argument("--debug-prg", action="store_true",
                    help="Lance RUN.PRG interactif (Cconws + pause clavier)")
    _args = _p.parse_args()

    if _args.trace:
        HATARI_EXTRA_OPTS += [
            "--trace",      "os_base",
            "--trace-file", os.path.join(os.path.abspath("."), "hatari_trace.log"),
        ]

    if _args.debug_prg:
        # Mode interactif : desactiver fast-forward pour voir l'ecran TOS
        for opt in ("--fast-forward", "yes"):
            try: HATARI_EXTRA_OPTS.remove(opt)
            except ValueError: pass

    hatari_bin = find_hatari()
    check_requirements(hatari_bin)

    work_dir = os.path.abspath(".")
    os.makedirs(PRG_DIR, exist_ok=True)

    sources = sorted(
        glob.glob(os.path.join(SRC_DIR, "*.S")) +
        glob.glob(os.path.join(SRC_DIR, "*.s"))
    )
    if _args.only:
        stem_f = _args.only.upper()
        sources = [s for s in sources
                   if os.path.splitext(os.path.basename(s))[0].upper() == stem_f]

    if not sources:
        print("[ERREUR] Aucun source .S dans '{}'.".format(SRC_DIR),
              file=sys.stderr)
        sys.exit(1)

    print("[INFO] Hatari     : {}".format(hatari_bin))
    print("[INFO] TOS ROM    : {}".format(TOS_ROM))
    print("[INFO] Assembleur : {}".format(os.path.abspath(GEN_TTP)))
    print("[INFO] Sources    : {} fichier(s)".format(len(sources)))
    print("[INFO] Sortie     : '{}'".format(PRG_DIR))
    print()

    success, failure = [], []

    for src_abs in [os.path.abspath(s) for s in sources]:
        stem    = os.path.splitext(os.path.basename(src_abs))[0].upper()
        prg_abs = os.path.abspath(os.path.join(PRG_DIR, stem + ".PRG"))

        print("[{}] {} -> {}".format(
            stem, os.path.relpath(src_abs), os.path.relpath(prg_abs)))

        if assemble_one(hatari_bin, work_dir, src_abs, prg_abs, None,
                        debug_prg=_args.debug_prg):
            size = os.path.getsize(prg_abs)
            print("  [OK] {} octets".format(size))
            success.append(stem)
        else:
            print("  [ECHEC]", file=sys.stderr)
            failure.append(stem)
        print()

    print("-" * 60)
    print("Resultat : {} OK, {} echec(s)".format(len(success), len(failure)))
    if failure:
        print("Echecs : {}".format(", ".join(failure)), file=sys.stderr)
        sys.exit(1)
