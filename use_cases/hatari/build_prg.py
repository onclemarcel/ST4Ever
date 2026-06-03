import os
import subprocess

SRC_DIR = "SRC"
BUILD_DIR = "."
PRG_DIR = "PRG"

for fname in os.listdir(SRC_DIR):
    if not fname.lower().endswith(".s"):
        continue

    src = os.path.join(SRC_DIR, fname)
    prg = os.path.join(BUILD_DIR, fname.replace(".S", ".PRG"))
    os.system(f"cp {src} {BUILD_DIR}/SOURCE.S")
    
    # Générer un script Hatari spécifique
    with open(os.path.join(BUILD_DIR, "build.cmd"), "w") as f:
        f.write(f"wait 500\n")
        f.write(f"break 0xFC0000\n")
        f.write(f"wait\n")
        f.write('run "GEN.TTP" "SOURCE.S"\n')
        f.write("wait\nquit\n")

    # Copier le fichier source dans le dossier build
    

    # Lancer Hatari
    subprocess.run([
        "hatari",
        "--parse", "build.cmd",
        "--tos", "C:\\msys\\mingw64\\share\\hatari\\tos104fr.rom",
        "--harddrive", BUILD_DIR,
        "--sound", "off",
        "--fast-forward", "yes",
        "--log-level", "debug",
        "--trace", "gemdos",
        "--log-file", "hatari.log"
    ])
    
    # Remove .S from current dir
    os.system(f"rm ./SOURCE.S")

    if (os.path.isfile(prg)):
        print(f"PRG généré : {prg}")
        os.system(f"mv {prg} {PRG_DIR}/")
    

