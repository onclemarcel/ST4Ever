#!/usr/bin/env python3
"""
build_sector_db.py - ST4Ever sector database builder

Reads seed JSON files from db/seeds/ and .st image files from
use_cases/UC20A/st/ to compute centroid feature vectors, then
writes db/sector_db.bin.

Also accepts --dump-features to show the feature vector for any
sector of a .st image without writing the database.

Usage:
  python3 tools/build_sector_db.py
  python3 tools/build_sector_db.py --dump-features <img.st> --lba <N>
  python3 tools/build_sector_db.py --out <path>

Must be run from the project root directory.
"""

import struct
import os
import sys
import json
import math
import argparse
import hashlib

# ---------------------------------------------------------------------------
SECTOR_SIZE  = 512
FEATURE_DIM  = 24
DB_MAGIC     = 0x53544442   # 'STDB'
DB_VERSION   = 1

SEEDS_DIR    = "db/seeds"
IMAGES_DIR   = "use_cases/UC20A/st"
DEFAULT_OUT  = "db/sector_db.bin"

# ---------------------------------------------------------------------------
# Sector type taxonomy (must match sector_analyze.h)
# ---------------------------------------------------------------------------
TYPE_MAP = {
    "unknown":            0,
    "bootsector_noboot":  1,
    "bootsector_boot":    2,
    "fat12":              3,
    "directory":          4,
    "directory_deleted":  5,
    "code_demo":          6,
    "code_gemdos":        7,
    "code_packer":        8,
    "data_sinus":         9,
    "data_ascii":        10,
    "data_binary":       11,
    "data_packed":       12,
    "bss_zero":          13,
    "unformatted":       14,
    "protection":        15,
}

# ---------------------------------------------------------------------------
# Feature extraction (mirrors sector_analyze.c)
# ---------------------------------------------------------------------------

def feat_bpb_valid(sec):
    bps  = struct.unpack_from('<H', sec, 0x0B)[0]
    spc  = sec[0x0D]
    nfat = sec[0x10]
    ts   = struct.unpack_from('<H', sec, 0x13)[0]
    spt  = struct.unpack_from('<H', sec, 0x18)[0]
    hds  = struct.unpack_from('<H', sec, 0x1A)[0]
    ok = (bps == 512 and spc in (1, 2) and 1 <= nfat <= 2
          and 720 <= ts <= 1440 and 8 <= spt <= 11 and 1 <= hds <= 2)
    return 1.0 if ok else 0.0

def feat_wd1772_bootable(sec):
    s = sum(struct.unpack_from('>H', sec, i*2)[0] for i in range(256))
    return 1.0 if (s & 0xFFFF) == 0x1234 else 0.0

def feat_fat_pattern(sec):
    return 1.0 if sec[0] >= 0xF0 and sec[1] == 0xFF and sec[2] == 0xFF else 0.0

def feat_dir_density(sec):
    total = SECTOR_SIZE // 32
    valid = 0
    for i in range(total):
        off = i * 32
        first = sec[off]
        attr  = sec[off + 11]
        if first in (0x00, 0xE5, 0x05):
            valid += 1
        elif all(0x20 <= sec[off+j] <= 0x7E for j in range(11)):
            if (attr & 0xC0) == 0:
                valid += 1
    return valid / total

def feat_entropy(sec):
    hist = [0] * 256
    for b in sec:
        hist[b] += 1
    h = 0.0
    for c in hist:
        if c == 0:
            continue
        p = c / SECTOR_SIZE
        h -= p * math.log2(p)
    return h / 8.0

def feat_byte_ratio(sec, val):
    return sec.count(val) / SECTOR_SIZE

def feat_ascii_ratio(sec):
    return sum(1 for b in sec if 0x20 <= b <= 0x7E) / SECTOR_SIZE

def feat_repeating(sec):
    hist = [0] * 256
    for b in sec:
        hist[b] += 1
    return max(hist) / SECTOR_SIZE

def feat_opcode_density(sec):
    """Simplified: ratio of bytes in [0x20..0xFE] range at even offsets."""
    # A full 68000 decoder is not available in Python here;
    # use a proxy: count even-offset words NOT in known-bad ranges.
    valid = 0
    for i in range(0, SECTOR_SIZE - 1, 2):
        w = (sec[i] << 8) | sec[i+1]
        # Exclude obvious non-opcodes: 0x0000, 0xFFFF, Line-A (0xA000),
        # Line-F (0xF000) with all-1 extension, plain DC.W patterns
        if w != 0x0000 and w != 0xFFFF and (w >> 12) != 0xA:
            valid += 1
    # Forward scan approximation: start from 0, count until first
    # word that looks like padding
    covered = 0
    pos = 0
    while pos + 2 <= SECTOR_SIZE:
        w = (sec[pos] << 8) | sec[pos+1]
        if w == 0x0000 and pos > 0:
            break
        if (w >> 12) == 0xF:  # Line-F trap
            break
        covered += 2
        pos     += 2
        if pos >= SECTOR_SIZE:
            break
    return covered / SECTOR_SIZE

def feat_word_align_ratio(sec):
    valid = 0
    for i in range(0, SECTOR_SIZE - 1, 2):
        w = (sec[i] << 8) | sec[i+1]
        if w != 0x0000 and w != 0xFFFF and (w >> 12) not in (0xA, 0xF):
            valid += 1
    return valid / 256

def feat_branch_density(sec):
    branches = 0
    instrs   = 0
    for i in range(0, SECTOR_SIZE - 1, 2):
        w = (sec[i] << 8) | sec[i+1]
        if w == 0x0000:
            break
        instrs += 1
        hi = w >> 8
        if hi in range(0x60, 0x70):   # BRA/BSR/Bcc
            branches += 1
        if hi in (0x4E, 0x4A):        # JMP/JSR
            branches += 1
    return (branches / instrs) if instrs > 0 else 0.0

def feat_rel_branch_valid(sec):
    branches = 0
    rel_ok   = 0
    for i in range(0, SECTOR_SIZE - 3, 2):
        hi = sec[i]
        if hi in range(0x60, 0x70):
            branches += 1
            # Short branch: offset in low byte
            off = sec[i+1]
            if off != 0x00 and off != 0xFF:
                signed_off = off - 256 if off >= 128 else off
                target = i + 2 + signed_off
                if 0 <= target < SECTOR_SIZE:
                    rel_ok += 1
    return (rel_ok / branches) if branches > 0 else 0.0

def feat_hw_immediate(sec):
    count = 0
    for i in range(SECTOR_SIZE - 1):
        if sec[i] == 0xFF and sec[i+1] >= 0x80:
            count += 1
    return min(1.0, count / 20.0)

def feat_hw_in_context(sec):
    count = 0
    for i in range(0, SECTOR_SIZE - 3, 2):
        w = (sec[i] << 8) | sec[i+1]
        hi = w >> 8
        # MOVE.L #imm,ea or LEA ea,An: check extension words for HW addr
        if hi in (0x20, 0x21, 0x22, 0x41, 0x43):
            for j in range(1, 5):
                if i + j*2 + 1 < SECTOR_SIZE:
                    ew = (sec[i+j*2] << 8) | sec[i+j*2+1]
                    if (ew >> 8) == 0xFF and (ew & 0xFF) >= 0x80:
                        count += 1
    return min(1.0, count / 4.0)

def feat_timing_loops(sec):
    count = 0
    for i in range(0, SECTOR_SIZE - 1, 2):
        if sec[i] == 0x51 and 0xC8 <= sec[i+1] <= 0xCF:
            count += 1
    return min(1.0, count / 4.0)

def feat_vbl_install(sec):
    for i in range(SECTOR_SIZE - 7):
        if (sec[i] == 0x21 and sec[i+1] == 0xFC
                and sec[i+6] == 0x00 and sec[i+7] == 0x70):
            return 1.0
    return 0.0

def feat_sinus_profile(sec):
    small = sum(1 for i in range(1, SECTOR_SIZE)
                if abs(sec[i] - sec[i-1]) <= 10)
    return small / (SECTOR_SIZE - 1)

def feat_graphics_pattern(sec):
    count = sum(1 for i in range(0, SECTOR_SIZE, 2)
                if ((sec[i] << 8 | sec[i+1]) & 0xF888) == 0)
    return count / 256

def feat_hw_byte_pair(sec, hi1, hi2):
    count = sum(1 for i in range(SECTOR_SIZE - 1)
                if sec[i] == 0xFF and sec[i+1] in (hi1, hi2))
    return min(1.0, count / 4.0)

def extract_features(sec, lba=0):
    """Returns a list of 24 float features for a 512-byte sector."""
    fOpcode = feat_opcode_density(sec)
    fEntropy = feat_entropy(sec)

    fPackerEntropy = fEntropy if (fEntropy > 0.87 and fOpcode < 0.10) else 0.0

    return [
        feat_bpb_valid(sec),
        feat_wd1772_bootable(sec),
        feat_fat_pattern(sec),
        feat_dir_density(sec),
        fEntropy,
        feat_byte_ratio(sec, 0x00),
        feat_byte_ratio(sec, 0xE5),
        feat_byte_ratio(sec, 0xFF),
        feat_ascii_ratio(sec),
        feat_repeating(sec),
        fOpcode,
        feat_word_align_ratio(sec),
        feat_branch_density(sec),
        feat_rel_branch_valid(sec),
        feat_hw_immediate(sec),
        feat_hw_in_context(sec),
        feat_timing_loops(sec),
        feat_vbl_install(sec),
        feat_sinus_profile(sec),
        feat_graphics_pattern(sec),
        fPackerEntropy,
        feat_hw_byte_pair(sec, 0x88, 0x89),
        feat_hw_byte_pair(sec, 0x82, 0x83),
        feat_hw_byte_pair(sec, 0xFA, 0xFC),
    ]

FEATURE_NAMES = [
    "fBpbValid", "fWd1772Bootable", "fFatPattern", "fDirDensity",
    "fEntropy",  "fZeroRuns",       "fE5Runs",      "fFfRuns",
    "fAsciiRatio","fRepeating",
    "fOpcodeDensity","fWordAlignRatio","fBranchDensity","fRelBranchValid",
    "fHwImmediate","fHwInContext","fTimingLoops","fVblInstall",
    "fSinusProfile","fGraphicsPattern","fPackerEntropy",
    "fYmAccess","fVideoAccess","fFdcAccess",
]

# ---------------------------------------------------------------------------
# Database builder
# ---------------------------------------------------------------------------

class SectorDB:
    def __init__(self):
        self.centroids = {}   # label -> (type_id, [sum_features], count)
        self.sigs      = []   # list of sig dicts

    def learn(self, sec, type_id, label, lba=0):
        feat = extract_features(sec, lba)
        if label not in self.centroids:
            self.centroids[label] = (type_id, [0.0]*FEATURE_DIM, 0)
        tid, acc, cnt = self.centroids[label]
        self.centroids[label] = (
            tid,
            [acc[i] + feat[i] for i in range(FEATURE_DIM)],
            cnt + 1
        )

    def finalize(self):
        result = []
        for label, (tid, acc, cnt) in self.centroids.items():
            if cnt == 0:
                continue
            centroid = [acc[i] / cnt for i in range(FEATURE_DIM)]
            result.append((tid, label, centroid, cnt))
        result.sort(key=lambda x: x[0])
        return result

    def load_packer_sigs(self, sig_file):
        if not os.path.exists(sig_file):
            return
        with open(sig_file) as f:
            data = json.load(f)
        for sig in data.get("signatures", []):
            pattern_hex = sig["pattern"].replace(" ", "")
            mask_hex    = sig.get("mask", "FF" * (len(pattern_hex)//2)).replace(" ", "")
            pattern = bytes.fromhex(pattern_hex)
            mask    = bytes.fromhex(mask_hex)
            self.sigs.append({
                "name":    sig["name"],
                "type":    TYPE_MAP.get(sig.get("type", "protection"), 15),
                "offset":  sig.get("iOffset", -1),
                "pattern": pattern,
                "mask":    mask,
            })

    def save(self, path):
        centroids = self.finalize()
        with open(path, "wb") as f:
            # Header
            f.write(struct.pack('<I', DB_MAGIC))
            f.write(struct.pack('<i', len(centroids)))
            f.write(struct.pack('<i', len(self.sigs)))
            # Centroids
            for tid, label, feat, samples in centroids:
                f.write(struct.pack('<i', tid))
                lb = label.encode()[:31] + b'\x00' * (32 - min(len(label), 31))
                f.write(lb[:32])
                for fv in feat:
                    f.write(struct.pack('<f', fv))
                f.write(struct.pack('<i', samples))
            # Packer sigs
            for sig in self.sigs:
                nm = sig["name"].encode()[:47] + b'\x00'
                nm += b'\x00' * (48 - len(nm))
                f.write(nm[:48])
                f.write(struct.pack('<i', sig["type"]))
                f.write(struct.pack('<i', sig["offset"]))
                pat = sig["pattern"][:16].ljust(16, b'\x00')
                msk = sig["mask"][:16].ljust(16, b'\x00')
                f.write(pat)
                f.write(msk)
                f.write(struct.pack('<i', len(sig["pattern"])))
        print(f"  Saved {len(centroids)} types, {len(self.sigs)} sigs → {path}")


def load_st_image(path):
    with open(path, "rb") as f:
        return f.read()

def process_seed_file(db, seed_path, images_dir):
    with open(seed_path) as f:
        seed = json.load(f)

    img_filename = seed.get("filename", "")
    img_path     = os.path.join(images_dir, img_filename)
    disk         = None

    if os.path.exists(img_path):
        disk = load_st_image(img_path)
        # Verify SHA256 if provided
        sha = seed.get("sha256", "")
        if sha:
            actual = hashlib.sha256(disk).hexdigest()
            if actual != sha:
                print(f"  WARN: SHA256 mismatch for {img_filename}")
    else:
        print(f"  INFO: {img_path} not found (gitignored) — skipping sectors")

    for entry in seed.get("labeled_sectors", []):
        lba    = entry["lba"]
        label  = entry["type"]
        tid    = TYPE_MAP.get(label, 0)
        if disk is None:
            continue
        off = lba * SECTOR_SIZE
        if off + SECTOR_SIZE > len(disk):
            print(f"  WARN: LBA {lba} out of range")
            continue
        sec = disk[off:off + SECTOR_SIZE]
        db.learn(sec, tid, label, lba)
        print(f"    learn LBA {lba:4d} → {label}")


def build_synthetic_defaults(db):
    """Add centroids for trivially-computable types."""
    sec = bytes(SECTOR_SIZE)   # BSS zero
    db.learn(sec, TYPE_MAP["bss_zero"], "bss_zero")

    sec = bytes([0xFF] * SECTOR_SIZE)
    db.learn(sec, TYPE_MAP["unformatted"], "unformatted")

    sec = bytearray(SECTOR_SIZE)
    sec[0] = 0xF9; sec[1] = 0xFF; sec[2] = 0xFF
    sec[3] = 0xFF; sec[4] = 0xFF
    db.learn(bytes(sec), TYPE_MAP["fat12"], "fat12")

    sec = bytes(SECTOR_SIZE)  # all-zero root dir
    db.learn(sec, TYPE_MAP["directory"], "directory")

    sec = bytes([0xE5] * SECTOR_SIZE)
    db.learn(sec, TYPE_MAP["directory_deleted"], "directory_deleted")

    sec = bytes(range(256)) * 2   # sequential bytes = data_binary
    db.learn(sec, TYPE_MAP["data_binary"], "data_binary")


# ---------------------------------------------------------------------------
# --dump-features mode
# ---------------------------------------------------------------------------

def dump_features(img_path, lba, db_path=None):
    disk = load_st_image(img_path)
    off  = lba * SECTOR_SIZE
    if off + SECTOR_SIZE > len(disk):
        print(f"ERROR: LBA {lba} out of range (image {len(disk)} bytes)")
        return
    sec  = disk[off:off + SECTOR_SIZE]
    feat = extract_features(sec, lba)

    print(f"\n=== Feature vector — {img_path} LBA {lba} ===")
    for name, val in zip(FEATURE_NAMES, feat):
        bar = '#' * int(val * 20)
        print(f"  {name:<22} : {val:.3f}  [{bar:<20}]")

    # Classify if DB available
    if db_path and os.path.exists(db_path):
        db = SectorDB()
        build_synthetic_defaults(db)
        centroids = db.finalize()
        print(f"\n  Top-3 matches (synthetic DB only):")
        scores = []
        for tid, label, centroid, _ in centroids:
            dot = sum(feat[i]*centroid[i] for i in range(FEATURE_DIM))
            na  = math.sqrt(sum(x*x for x in feat))
            nb  = math.sqrt(sum(x*x for x in centroid))
            score = dot / (na * nb) if na > 1e-9 and nb > 1e-9 else 0.0
            scores.append((score, label))
        scores.sort(reverse=True)
        for score, label in scores[:3]:
            print(f"    {score:.3f}  {label}")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="ST4Ever sector DB builder")
    parser.add_argument("--out", default=DEFAULT_OUT,
                        help="Output .bin path (default: db/sector_db.bin)")
    parser.add_argument("--dump-features", metavar="IMG",
                        help="Dump feature vector for an image sector")
    parser.add_argument("--lba", type=int, default=0,
                        help="LBA sector number for --dump-features")
    args = parser.parse_args()

    if args.dump_features:
        dump_features(args.dump_features, args.lba, DEFAULT_OUT)
        return

    print("ST4Ever build_sector_db.py")
    print(f"  Seeds dir : {SEEDS_DIR}")
    print(f"  Images dir: {IMAGES_DIR}")
    print(f"  Output    : {args.out}")
    print()

    db = SectorDB()
    build_synthetic_defaults(db)

    # Load packer signatures
    sig_path = os.path.join(SEEDS_DIR, "packer_sigs.json")
    db.load_packer_sigs(sig_path)
    print(f"  Loaded {len(db.sigs)} packer signatures from {sig_path}")

    # Process seed JSON files
    if os.path.isdir(SEEDS_DIR):
        for fn in sorted(os.listdir(SEEDS_DIR)):
            if fn.endswith(".json") and fn != "packer_sigs.json":
                seed_path = os.path.join(SEEDS_DIR, fn)
                print(f"  Processing seed: {fn}")
                process_seed_file(db, seed_path, IMAGES_DIR)

    # Ensure output directory exists
    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    db.save(args.out)


if __name__ == "__main__":
    main()
