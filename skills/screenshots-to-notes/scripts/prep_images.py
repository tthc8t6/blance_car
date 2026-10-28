"""Prepare screenshots / photos for the screenshots-to-notes skill.

  prep <src_dir> [<src_dir> ...] --out <images_dir> [--max 1600]
      EXIF-rotate, downscale, save as jpg, write manifest.json with suggested batches.
  crop <image> <left> <top> <right> <bottom> --out <png> [--scale 2]
      Cut a region (fractions 0-1) and enlarge it, for re-reading one unclear symbol.
  merge <note_dir> --name <note.md>
      _parts/_front.md + _parts/_part_*.md -> <note_dir>/<note.md>, backing up the old note.

Console output is ASCII only (this machine's ACP is 1252); details go to manifest.json (UTF-8).
"""
import argparse
import json
import re
import sys
from datetime import datetime
from pathlib import Path

from PIL import Image, ImageOps

try:
    import pillow_heif
    pillow_heif.register_heif_opener()
    HEIF_OK = True
except ImportError:
    HEIF_OK = False

EXTS = {".jpg", ".jpeg", ".png", ".heic", ".heif", ".webp", ".bmp"}
EXIF_ORIENTATION = 0x0112
EXIF_DATETIME_ORIGINAL = 0x9003
EXIF_IFD = 0x8769

# A "session" is a run of shots with no big number/time gap (one recording, one chapter).
# Sessions longer than MAX_PER_BATCH are split evenly; sessions shorter than MIN_SESSION
# join the previous batch, so no sub-agent is spawned for one or two stray images.
MAX_PER_BATCH = 8
MIN_SESSION = 3
MAX_NUMBER_GAP = 3        # IMG_4742 -> IMG_4763 is a gap of 21: new recording session
MAX_TIME_GAP_MIN = 30


def image_number(name):
    m = re.search(r"(\d+)(?!.*\d)", Path(name).stem)
    return int(m.group(1)) if m else None


def taken_time(img, path):
    try:
        exif = img.getexif()
        raw = exif.get_ifd(EXIF_IFD).get(EXIF_DATETIME_ORIGINAL) or exif.get(0x0132)
        if raw:
            return datetime.strptime(str(raw).strip(), "%Y:%m:%d %H:%M:%S")
    except Exception:
        pass
    return datetime.fromtimestamp(path.stat().st_mtime)


def split_batches(items):
    """Group sorted items into sessions by number/time gap, then size them into batches."""
    sessions, cur = [], []
    for it in items:
        if cur:
            prev = cur[-1]
            num_gap = (it["number"] - prev["number"]) if it["number"] is not None and prev["number"] is not None else 0
            time_gap = abs((datetime.fromisoformat(it["taken"]) - datetime.fromisoformat(prev["taken"])).total_seconds()) / 60
            if num_gap > MAX_NUMBER_GAP or time_gap > MAX_TIME_GAP_MIN:
                sessions.append(cur)
                cur = []
        cur.append(it)
    if cur:
        sessions.append(cur)

    batches = []
    for s in sessions:
        if len(s) < MIN_SESSION and batches:
            batches[-1].extend(s)
            continue
        n = -(-len(s) // MAX_PER_BATCH)          # ceil: 10 -> 2 batches of 5, not 8 + 2
        size = -(-len(s) // n)
        batches += [s[i:i + size] for i in range(0, len(s), size)]
    names = ["0"] + [chr(ord("A") + i) for i in range(25)]
    return [
        {"name": names[i] if i < len(names) else str(i), "count": len(b),
         "first": b[0]["name"], "last": b[-1]["name"], "images": [x["name"] for x in b]}
        for i, b in enumerate(batches)
    ]


def cmd_prep(args):
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    files = []
    for d in args.src:
        files += [p for p in Path(d).iterdir() if p.is_file() and p.suffix.lower() in EXTS]

    heic = [p for p in files if p.suffix.lower() in (".heic", ".heif")]
    if heic and not HEIF_OK:
        print(f"ERROR: {len(heic)} HEIC files found but pillow-heif is not installed.")
        print("Ask the user, then run:  pip install pillow-heif")
        return 2

    items, rotated, failed = [], 0, []
    for p in sorted(files, key=lambda p: (image_number(p.name) or 0, p.name)):
        try:
            with Image.open(p) as img:
                orient = img.getexif().get(EXIF_ORIENTATION, 1)
                when = taken_time(img, p)
                fixed = ImageOps.exif_transpose(img).convert("RGB")
                fixed.thumbnail((args.max, args.max), Image.LANCZOS)
                dst = out / (p.stem + ".jpg")
                fixed.save(dst, "JPEG", quality=85)
        except Exception as e:
            failed.append({"src": str(p), "error": repr(e)})
            continue
        rotated += orient not in (1, None)
        items.append({
            "name": p.stem, "src": str(p), "out": str(dst), "number": image_number(p.name),
            "taken": when.isoformat(timespec="seconds"), "exif_orientation": orient,
            "rotated": orient not in (1, None), "size": list(fixed.size),
        })

    manifest = {
        "created": datetime.now().isoformat(timespec="seconds"),
        "sources": [str(Path(d).resolve()) for d in args.src],
        "max_edge": args.max, "count": len(items), "rotated": rotated,
        "batches": split_batches(items), "images": items, "failed": failed,
    }
    (out / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")

    print(f"OK: {len(items)} images -> {out.name}/  (rotated {rotated}, failed {len(failed)})")
    for b in manifest["batches"]:
        print(f"  batch {b['name']}: {b['count']:>2}  {b['first']} .. {b['last']}")
    print("Details: manifest.json in the output folder (read it with the Read tool).")
    return 1 if failed else 0


def cmd_crop(args):
    with Image.open(args.image) as img:
        img = ImageOps.exif_transpose(img).convert("RGB")
        w, h = img.size
        box = (int(args.left * w), int(args.top * h), int(args.right * w), int(args.bottom * h))
        part = img.crop(box)
        if args.scale != 1:
            part = part.resize((part.width * args.scale, part.height * args.scale), Image.LANCZOS)
        Path(args.out).parent.mkdir(parents=True, exist_ok=True)
        part.save(args.out)
    print(f"OK: crop {box} x{args.scale} -> {part.size[0]}x{part.size[1]}")
    return 0


PART_SUMMARY_HEADINGS = ("## 本批存疑汇总", "## 本批逻辑断层")


def clean_part(text):
    """Drop the agent banner and per-batch summary sections, demote headings one level."""
    out, skipping, fence = [], False, False
    for line in text.splitlines():
        if line.lstrip().startswith("```"):
            fence = not fence
        if not fence and line.startswith("## "):
            skipping = line.strip().startswith(PART_SUMMARY_HEADINGS)
        if skipping or "本文件由批次" in line:
            continue
        if not fence and re.match(r"#{1,5} ", line):
            line = "#" + line
        out.append(line)
    body = "\n".join(out).strip()
    return re.sub(r"(\n-{3,}\s*)+$", "", body)   # trailing --- left by removed sections


def cmd_merge(args):
    root = Path(args.note_dir)
    parts_dir = root / "_parts"
    front = parts_dir / "_front.md"
    if not front.exists():
        print("ERROR: _parts/_front.md not found; write it first (see references/note-layout.md).")
        return 2
    parts = sorted(p for p in parts_dir.glob("_part_*.md") if not p.stem.endswith(".old"))
    if not parts:
        print("ERROR: no _parts/_part_*.md found.")
        return 2

    target = root / args.name
    if target.exists():
        backup = parts_dir / (target.stem + ".prev.md")
        backup.write_text(target.read_text(encoding="utf-8"), encoding="utf-8")
        print(f"backup: _parts/{backup.name}")

    chunks = [front.read_text(encoding="utf-8").strip()] + [clean_part(p.read_text(encoding="utf-8")) for p in parts]
    target.write_text("\n\n---\n\n".join(chunks) + "\n", encoding="utf-8")
    print(f"OK: front + {len(parts)} parts ({', '.join(p.stem[6:] for p in parts)}) -> {target.name}")
    return 0


def main():
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("prep")
    p.add_argument("src", nargs="+")
    p.add_argument("--out", required=True)
    p.add_argument("--max", type=int, default=1600)
    c = sub.add_parser("crop")
    c.add_argument("image")
    for k in ("left", "top", "right", "bottom"):
        c.add_argument(k, type=float)
    c.add_argument("--out", required=True)
    c.add_argument("--scale", type=int, default=2)
    m = sub.add_parser("merge")
    m.add_argument("note_dir")
    m.add_argument("--name", required=True)
    args = ap.parse_args()
    return {"prep": cmd_prep, "crop": cmd_crop, "merge": cmd_merge}[args.cmd](args)


if __name__ == "__main__":
    sys.exit(main())
