#!/usr/bin/env python3
"""
Render a rotating 3D GIF of the VEXTREME PCB using KiCad's raytracer.

    python3 images/render-pcb-gif.py
    python3 images/render-pcb-gif.py --out images/vextreme-v1.0-white.gif --step 10

Requires KiCad 10 (kicad-cli), Python 3.8+, Pillow and NumPy:

    pip install pillow numpy

How it works:
  1. Copies the board to a temp dir and injects a stackup with a white solder
     mask, black silkscreen and ENIG finish (the real board file is untouched).
  2. Renders one frame per --step degrees around the vertical axis with
     `kicad-cli pcb render`, starting on the component (bottom) side.
  3. Post-processes each frame: neutralises the slight warm tint, lowers the
     black point so the silkscreen reads black, and composites it onto a
     lavender/grey gradient.
  4. Builds a looping GIF with a shared palette, holding on each side.

Lighting comes from KiCad's 3D viewer raytracing settings (--quality user), so
changing those settings in KiCad changes the render.
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_BOARD = ROOT / "hardware" / "vextreme.kicad_pcb"
DEFAULT_OUT = ROOT / "images" / "vextreme-v1.0-white.gif"
DEFAULT_KICAD_CLI = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"

# White mask is ~95% opaque so copper traces show through faintly.
STACKUP = """\t(setup
\t\t(stackup
\t\t\t(layer "F.SilkS" (type "Top Silk Screen") (color "#000000FF"))
\t\t\t(layer "F.Paste" (type "Top Solder Paste"))
\t\t\t(layer "F.Mask" (type "Top Solder Mask") (color "#FFFFFFF2") (thickness 0.01))
\t\t\t(layer "F.Cu" (type "copper") (thickness 0.035))
\t\t\t(layer "dielectric 1" (type "core") (color "FR4 natural") (thickness 1.51) (material "FR4") (epsilon_r 4.5) (loss_tangent 0.02))
\t\t\t(layer "B.Cu" (type "copper") (thickness 0.035))
\t\t\t(layer "B.Mask" (type "Bottom Solder Mask") (color "#FFFFFFF2") (thickness 0.01))
\t\t\t(layer "B.Paste" (type "Bottom Solder Paste"))
\t\t\t(layer "B.SilkS" (type "Bottom Silk Screen") (color "#000000FF"))
\t\t\t(copper_finish "ENIG")
\t\t\t(dielectric_constraints no)
\t\t)
"""

BG_TOP = (204, 204, 230)
BG_BOTTOM = (102, 102, 128)
BLACK_POINT = 70


def prepare_board(board: Path, workdir: Path) -> Path:
    """Copy the project into workdir with a white/black stackup injected."""
    src = board.parent
    for name in ("fp-lib-table", board.with_suffix(".kicad_pro").name):
        if (src / name).exists():
            shutil.copy(src / name, workdir / name)
    if (src / "libs").exists():
        os.symlink(src / "libs", workdir / "libs")  # 3D models via ${KIPRJMOD}/libs
    text = board.read_text(encoding="utf-8")
    if "(stackup" in text:
        sys.exit("board already has a stackup; edit its colors in KiCad instead")
    if text.count("\t(setup\n") != 1:
        sys.exit("could not find the (setup section in the board file")
    out = workdir / board.name
    out.write_text(text.replace("\t(setup\n", STACKUP, 1), encoding="utf-8")
    return out


def render(kicad_cli, board, png, angle, width, height, zoom):
    subprocess.run(
        [kicad_cli, "pcb", "render", "-o", str(png),
         "-w", str(width), "-h", str(height), "--side", "top",
         "--rotate", f"0,{angle},0", "--zoom", str(zoom),
         "--quality", "user", "--perspective", "--use-board-stackup-colors",
         "--background", "transparent", str(board)],
        check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def gradient(w, h):
    t = np.linspace(0, 1, h)[:, None]
    col = np.array(BG_TOP) + (np.array(BG_BOTTOM) - np.array(BG_TOP)) * t
    rgb = np.repeat(col[:, None, :], w, axis=1).astype(np.uint8)
    return Image.fromarray(rgb, "RGB").convert("RGBA")


def white_balance_gains(png):
    """Per-channel gains that make the brightest board pixels neutral white."""
    a = np.asarray(Image.open(png).convert("RGBA")).astype(np.float32)
    px = a[a[..., 3] > 250][:, :3]
    bright = px[px.mean(1) > 230].mean(0)
    return bright.max() / bright


def finish(png, gains):
    a = np.asarray(Image.open(png).convert("RGBA")).astype(np.float32)
    rgb = np.clip(a[..., :3] * gains, 0, 255)
    rgb = np.clip((rgb - BLACK_POINT) / (255 - BLACK_POINT), 0, 1) * 255
    a[..., :3] = rgb
    fg = Image.fromarray(a.astype(np.uint8), "RGBA")
    return Image.alpha_composite(gradient(*fg.size), fg).convert("RGB")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--board", type=Path, default=DEFAULT_BOARD)
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT)
    ap.add_argument("--kicad-cli", default=os.environ.get("KICAD_CLI", DEFAULT_KICAD_CLI))
    ap.add_argument("--step", type=int, default=10, help="degrees per frame (default 10)")
    ap.add_argument("--frame-ms", type=int, default=150)
    ap.add_argument("--hold-bottom-ms", type=int, default=4000)
    ap.add_argument("--hold-top-ms", type=int, default=3500)
    ap.add_argument("--width", type=int, default=736)
    ap.add_argument("--height", type=int, default=804)
    ap.add_argument("--zoom", type=float, default=0.75)
    args = ap.parse_args()

    if 180 % args.step:
        sys.exit("--step must divide 180 evenly")
    if not shutil.which(args.kicad_cli) and not Path(args.kicad_cli).exists():
        sys.exit(f"kicad-cli not found: {args.kicad_cli} (set --kicad-cli or KICAD_CLI)")

    # Start on the component (bottom) side, which is more interesting.
    angles = [(180 + i * args.step) % 360 for i in range(360 // args.step)]

    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        board = prepare_board(args.board.resolve(), tmp)
        pngs = []
        for i, angle in enumerate(angles):
            png = tmp / f"f{i:03d}.png"
            print(f"rendering {i + 1}/{len(angles)} ({angle} deg)", flush=True)
            render(args.kicad_cli, board, png, angle, args.width, args.height, args.zoom)
            pngs.append(png)

        # Balance against the LED (top) side: it is mostly bare mask.
        gains = white_balance_gains(pngs[len(pngs) // 2])
        frames = [finish(p, gains) for p in pngs]

    w, h = frames[0].size
    mosaic = Image.new("RGB", (w, h * len(frames[::3])))
    for i, im in enumerate(frames[::3]):
        mosaic.paste(im, (0, i * h))
    palette = mosaic.quantize(colors=256, method=Image.Quantize.MEDIANCUT)
    gif = [im.quantize(palette=palette, dither=Image.Dither.NONE) for im in frames]

    n = len(gif)
    durations = [args.frame_ms] * n
    durations[0] = args.hold_bottom_ms
    durations[n // 2] = args.hold_top_ms
    args.out.parent.mkdir(parents=True, exist_ok=True)
    gif[0].save(args.out, save_all=True, append_images=gif[1:], duration=durations,
                loop=0, optimize=True, disposal=1)
    print(f"wrote {args.out} ({args.out.stat().st_size // 1024} KB, {n} frames, {w}x{h})")


if __name__ == "__main__":
    main()
