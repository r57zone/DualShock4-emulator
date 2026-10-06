"""Render reproducible matched-input GIFs and metrics through the real C++ mapper.

Dependencies: Pillow, numpy. GIFs/CSV/JSON are generated artifacts, not repository binaries.
"""
import argparse
import csv
import io
import json
import math
import pathlib
import subprocess

import numpy as np
from PIL import Image, ImageDraw, ImageFont

MODES = {
    "legacy": ("legacy", "linear", 0.60, 0.12, 0.0),
    "absolute": ("absolute", "linear", 0.60, 0.12, 0.20),
    "relative_linear": ("relative", "linear", 0.60, 0.12, 0.0),
    "relative": ("relative", "quadratic", 0.60, 0.12, 0.0),
}
COLORS = {"legacy": "#9aabc0", "absolute": "#5be0db", "relative_linear": "#bd9cff", "relative": "#ffbc66"}
LABELS = {"legacy": "Legacy 2.2", "absolute": "Improved absolute", "relative_linear": "Relative / linear", "relative": "Relative / quadratic"}
PATTERNS = ["A  Small circle", "B  Slow diagonal", "C  Fast upward swipe", "D  Zig-zag", "E  Graffiti-like curves", "F  Small target adjustments"]


def simulate(engine, mode, frames):
    args = MODES[mode]
    text = "".join(f"{active},{x:.12f},{y:.12f},{dt:.12f}\n" for active, x, y, dt in frames)
    result = subprocess.run([str(engine), *map(str, args)], input=text, text=True, capture_output=True, check=True)
    return [{key: float(value) for key, value in row.items()} for row in csv.DictReader(io.StringIO(result.stdout))]


def pattern_input(pattern, t):
    if pattern == 0:
        angle = 2 * math.pi * t / 3
        return 0.23 * math.cos(angle), 0.23 * math.sin(angle)
    if pattern == 1:
        magnitude = 0.14 + 0.12 * min(t / 3, 1)
        return magnitude / math.sqrt(2), -magnitude / math.sqrt(2)
    if pattern == 2:
        return (0, -0.98) if 0.25 < t < 0.90 else (0, 0)
    if pattern == 3:
        return (0.65 if int(t / 0.5) % 2 == 0 else -0.65), -0.16
    if pattern == 4:
        magnitude = 0.32 + 0.48 * (0.5 + 0.5 * math.sin(2 * math.pi * t / 3))
        angle = -1.1 + 1.4 * math.sin(2 * math.pi * t / 2.4)
        return magnitude * math.cos(angle), magnitude * math.sin(angle)
    if t <= 1.25:
        return 0.30, 0
    if t <= 1.75:
        return 0, 0
    angle = 2 * math.pi * (t - 1.75) / 1.25
    return 0.17 * math.cos(angle), 0.17 * math.sin(angle)


def timeline(fps=20):
    frames, labels = [], []
    for pattern in range(6):
        frames.append((0, 0, 0, 0.05)); labels.append((pattern, -0.05))
        frames.append((1, 0, 0, 0.05)); labels.append((pattern, 0))
        for i in range(1, fps * 3 + 1):
            t = i / fps
            x, y = pattern_input(pattern, t)
            frames.append((1, x, y, 1 / fps)); labels.append((pattern, t))
        for i in range(5):
            frames.append((0, 0, 0, 1 / fps)); labels.append((pattern, 3 + (i + 1) / fps))
    return frames, labels


def measure(engine):
    results = {}
    rng = np.random.default_rng(20261005)
    noise = rng.normal(0, 0.015, (120, 2))
    idle_noise = rng.normal(0, 0.025, (120, 2))
    for mode in MODES:
        row = {}
        for magnitude in (0.01, 0.12004, 0.13, 0.20, 0.30, 0.80, 1.0):
            frames = [(1, 0, 0, 0)] + [(1, magnitude, 0, 1 / 60)] * 60
            data = simulate(engine, mode, frames)
            row[f"input_{magnitude:.5f}_dx_after_1s"] = data[-1]["x"] - 959.5
            row[f"input_{magnitude:.5f}_first_frame_dx"] = data[1]["x"] - data[0]["x"]
        # One second settled endpoints approximated with 5 seconds, then full reversal.
        data = simulate(engine, mode, [(1, 1, 0, 1 / 60)] * 300 + [(1, -1, 0, 1 / 60)])
        row["full_reversal_step_at_60hz"] = abs(data[-1]["x"] - data[-2]["x"])
        row["full_reversal_step_speed_at_60hz"] = row["full_reversal_step_at_60hz"] * 60
        # Open-loop velocity task: target is 60 units right of the activation center.
        target_frames = [(1, 0, 0, 0)] + [(1, 0.3, 0, 0.01)] * 125 + [(1, 0, 0, 0.01)] * 50
        target = simulate(engine, mode, target_frames)
        row["target_overshoot_60units"] = max(0, max(point["x"] for point in target) - 1019.5)
        row["target_final_error_60units"] = abs(target[-1]["x"] - 1019.5)
        quiet = [(1, 0, 0, 0)] + [(1, 0.25, 0, 1 / 60)] * 120
        noisy = [(1, 0, 0, 0)] + [(1, 0.25 + nx, ny, 1 / 60) for nx, ny in noise]
        clean, perturbed = simulate(engine, mode, quiet), simulate(engine, mode, noisy)
        row["active_jitter_rms_vs_clean"] = float(np.sqrt(np.mean([
            (a["wire_x"] - b["wire_x"]) ** 2 + (a["wire_y"] - b["wire_y"]) ** 2
            for a, b in zip(clean[31:], perturbed[31:])
        ])))
        idle = simulate(engine, mode, [(1, 0, 0, 0)] + [(1, nx, ny, 1 / 60) for nx, ny in idle_noise])
        row["idle_jitter_rms_from_center"] = float(np.sqrt(np.mean([
            (p["wire_x"] - 960) ** 2 + (p["wire_y"] - 471) ** 2 for p in idle[1:]
        ])))
        # Compare integration at polling rates without wall/bounds saturation.
        row["polling_dx_2s"] = {}
        for hz in (30, 60, 250, 1000):
            data = simulate(engine, mode, [(1, 0, 0, 0)] + [(1, 0.3, 0, 1 / hz)] * (2 * hz))
            row["polling_dx_2s"][str(hz)] = data[-1]["x"] - 959.5
        results[mode] = row
    results["conditions"] = {
        "sensitivity": 0.6, "radial_deadzone": 0.12, "absolute_smoothing_seconds": 0.2,
        "relative_speed_units_per_second": 1920, "metric_polling_hz": 60,
        "active_jitter": "Same seeded sigma=0.015 x/y perturbations around x=0.25; RMS deviation from clean trace after 0.5s, 2s total.",
        "idle_jitter": "Seeded sigma=0.025 x/y around neutral, 2s; RMS wire-coordinate distance from center.",
        "overshoot": "Open-loop velocity task: x=0.3 for 1.25s then neutral for 0.5s, target=center+60. Position mode normally needs a different command; this is not a universal accuracy score.",
        "quantization": "Stick commands quantized to signed 16-bit, output rounded to integer coordinates; hidden double accumulator retained.",
    }
    results["sensitivity_sweep"] = []
    original = MODES["relative"]
    for sensitivity in (0.4, 0.6, 0.8):
        MODES["relative"] = ("relative", "quadratic", sensitivity, 0.12, 0)
        sample = simulate(engine, "relative", [(1,0,0,0)] + [(1,0.2,0,0.01)] * 100)
        fast = simulate(engine, "relative", [(1,0,0,0)] + [(1,0.8,0,0.01)] * 100)
        results["sensitivity_sweep"].append({"sensitivity": sensitivity,
            "20_percent_units_per_second": sample[-1]["x"] - 959.5,
            "80_percent_units_per_second": fast[-1]["x"] - 959.5,
            "full_vertical_traverse_seconds": 941 / (1920 * sensitivity)})
    MODES["relative"] = original
    return results


def font(size, bold=False):
    filename = pathlib.Path("C:/Windows/Fonts") / ("segoeuib.ttf" if bold else "segoeui.ttf")
    return ImageFont.truetype(str(filename), size) if filename.exists() else ImageFont.load_default(size=size)


def render_gif(output, modes, data, frames, labels):
    width = 640 * len(modes)
    height = 540
    images = []
    paths = {mode: [] for mode in modes}
    last_pattern = -1
    for i, ((active, sx, sy, dt), (pattern, t)) in enumerate(zip(frames, labels)):
        if pattern != last_pattern:
            paths = {mode: [] for mode in modes}
            last_pattern = pattern
        image = Image.new("RGB", (width, height), "#101923")
        draw = ImageDraw.Draw(image)
        draw.text((22, 10), PATTERNS[pattern], font=font(22, True), fill="#f4f8fc")
        draw.text((22, 43), "Matched right-stick input  |  20 fps demo / 50 ms simulation step", font=font(15), fill="#b4c5d7")
        for column, mode in enumerate(modes):
            left = column * 640 + 24
            draw.text((left, 76), LABELS[mode], font=font(22, True), fill=COLORS[mode])
            args = MODES[mode]
            draw.text((left, 105), f"sens={args[2]:.2f}  dz={args[3]:.2f}  curve={args[1]}  tau={args[4]:.2f}s", font=font(15), fill="#c8d3df")
            box = (left, 138, left + 590, 138 + 590 * 941 / 1919)

            def screen(x, y):
                return box[0] + x / 1919 * 590, box[1] + y / 1919 * 590

            draw.rectangle(box, fill="#1b2b3a", outline="#718498", width=2)
            for x in (480, 960, 1440):
                px, _ = screen(x, 0); draw.line((px, box[1], px, box[3]), fill="#304353")
            for y in (235, 471, 706):
                _, py = screen(0, y); draw.line((box[0], py, box[2], py), fill="#304353")
            draw.text((box[0]+7, box[1]+5), "0,0", font=font(12), fill="#aab8c7")
            draw.text((box[2]-72, box[3]-22), "1919,941", font=font(12), fill="#aab8c7")
            if pattern == 5:
                target_x, target_y = screen(1019.5, 470.5)
                draw.ellipse((target_x-6, target_y-6, target_x+6, target_y+6), outline="#fff4b1", width=2)
                draw.text((target_x+12, target_y+10), "target +60", font=font(12), fill="#fff4b1")
            point = data[mode][i]
            if active:
                paths[mode].append(screen(point["wire_x"], point["wire_y"]))
            if len(paths[mode]) > 1:
                draw.line(paths[mode], fill=COLORS[mode], width=2)
            if pattern in (0, 1, 5):
                # A true-coordinate detail inset keeps very slow motions visible.
                inset = (box[0]+12, box[1]+30, box[0]+228, box[1]+136)
                draw.rectangle(inset, fill="#111e2a", outline="#60758b")
                draw.text((inset[0]+5, inset[1]+4), "Center detail 3.25x", font=font(12), fill="#b4c5d7")
                zoom = []
                for original_x, original_y in paths[mode]:
                    tx = (original_x - box[0]) / 590 * 1919
                    ty = (original_y - box[1]) / 590 * 1919
                    zx = inset[0]+108+(tx-959.5)
                    zy = inset[1]+53+(ty-470.5)
                    if inset[0]+3 <= zx <= inset[2]-3 and inset[1]+22 <= zy <= inset[3]-3:
                        zoom.append((zx,zy))
                if len(zoom) > 1: draw.line(zoom, fill=COLORS[mode], width=2)
                zx = inset[0]+108+(point["wire_x"]-959.5)
                zy = inset[1]+53+(point["wire_y"]-470.5)
                if point["active"] and inset[0]+3 <= zx <= inset[2]-3 and inset[1]+22 <= zy <= inset[3]-3:
                    draw.ellipse((zx-3,zy-3,zx+3,zy+3),fill=COLORS[mode])
                if pattern == 5:
                    zx,zy=inset[0]+168,inset[1]+53
                    draw.ellipse((zx-6,zy-6,zx+6,zy+6),outline="#fff4b1",width=2)
            px, py = screen(point["wire_x"], point["wire_y"])
            if point["active"]:
                draw.ellipse((px-6, py-6, px+6, py+6), fill=COLORS[mode], outline="#ffffff", width=2)
            draw.text((left, 444), f"Touch ({int(point['wire_x'])}, {int(point['wire_y'])})   {'DOWN' if active else 'UP'}", font=font(18, True), fill="#edf4fa")
            # Right-stick inset: the physical command is identical in every panel.
            cx, cy = left + 534, 466
            draw.ellipse((cx-29, cy-29, cx+29, cy+29), outline="#9aacbe", width=2)
            draw.ellipse((cx-29*.12, cy-29*.12, cx+29*.12, cy+29*.12), outline="#536677")
            draw.line((cx, cy, cx+27*sx, cy+27*sy), fill="#ffffff", width=2)
            draw.ellipse((cx+27*sx-4, cy+27*sy-4, cx+27*sx+4, cy+27*sy+4), fill="#ffffff")
            draw.text((left, 471), f"Stick x={sx:+.2f} y={sy:+.2f}  |  t={max(0,t):.2f}s", font=font(15), fill="#b4c5d7")
        draw.text((22, 511), "Synthetic controls demo; each section starts a new contact. Trails show transmitted coordinates.", font=font(14), fill="#b4c5d7")
        images.append(image.quantize(colors=96))
    images[0].save(output, save_all=True, append_images=images[1:], duration=50, loop=0, optimize=False, disposal=2)
    # Representative section-end contact, before finger-up, for visual inspection.
    images[2*67 - 6].convert("RGB").save(output.with_suffix(".png"))
    print(f"Rendered {output.name}: {len(images)} frames, {output.stat().st_size:,} bytes", flush=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--engine", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    frames, labels = timeline()
    data = {mode: simulate(args.engine, mode, frames) for mode in MODES}
    for mode, rows in data.items():
        with (args.output / (mode + "_trace.csv")).open("w", newline="") as file:
            writer = csv.DictWriter(file, fieldnames=list(rows[0]) + ["pattern", "time", "stick_x", "stick_y"])
            writer.writeheader()
            for point, (pattern, t), frame in zip(rows, labels, frames):
                writer.writerow({**point, "pattern": PATTERNS[pattern], "time": t, "stick_x": frame[1], "stick_y": frame[2]})
    metrics = measure(args.engine)
    (args.output / "metrics.json").write_text(json.dumps(metrics, indent=2), encoding="utf-8")
    columns = ["Mode", "20% / 1s dx", "30% / 1s dx", "idle jitter RMS", "active jitter RMS", "target overshoot", "target final error"]
    lines = ["| " + " | ".join(columns) + " |", "|" + "---|" * len(columns)]
    for mode in MODES:
        m = metrics[mode]
        values = [LABELS[mode], f"{m['input_0.20000_dx_after_1s']:.2f}", f"{m['input_0.30000_dx_after_1s']:.2f}",
                  f"{m['idle_jitter_rms_from_center']:.2f}", f"{m['active_jitter_rms_vs_clean']:.2f}",
                  f"{m['target_overshoot_60units']:.2f}", f"{m['target_final_error_60units']:.2f}"]
        lines.append("| " + " | ".join(values) + " |")
    lines.append("\nAll values are touchpad coordinate units. Absolute is a position, relative is distance over one second. See metrics.json conditions for the deliberately shared open-loop velocity target task.")
    (args.output / "metrics.md").write_text("\n".join(lines), encoding="utf-8")
    print("\n".join(lines), flush=True)
    render_gif(args.output / "absolute_mode.gif", ["absolute"], data, frames, labels)
    render_gif(args.output / "relative_mode.gif", ["relative"], data, frames, labels)
    render_gif(args.output / "absolute_vs_relative.gif", ["absolute", "relative"], data, frames, labels)
    render_gif(args.output / "linear_vs_quadratic.gif", ["relative_linear", "relative"], data, frames, labels)
    render_gif(args.output / "legacy_vs_absolute.gif", ["legacy", "absolute"], data, frames, labels)


if __name__ == "__main__":
    main()
