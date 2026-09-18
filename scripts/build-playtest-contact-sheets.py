#!/usr/bin/env python3
"""Build local review sheets from the latest per-title renderer captures."""

import argparse
import csv
import pathlib

from PIL import Image, ImageDraw, ImageFont


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("matrix")
    parser.add_argument("output")
    parser.add_argument("--columns", type=int, default=4)
    parser.add_argument("--rows", type=int, default=3)
    args = parser.parse_args()
    with open(args.matrix, encoding="utf-8-sig", newline="") as source:
        records = [row for row in csv.DictReader(source) if row["Evidence"]]
    output = pathlib.Path(args.output)
    output.mkdir(parents=True, exist_ok=True)
    font = ImageFont.load_default(size=18)
    cell_w, image_h, label_h = 400, 225, 58
    page_size = args.columns * args.rows
    written = []
    for page_index in range(0, len(records), page_size):
        page_records = records[page_index:page_index + page_size]
        sheet = Image.new("RGB", (args.columns * cell_w, args.rows * (image_h + label_h)), "#202020")
        draw = ImageDraw.Draw(sheet)
        for index, record in enumerate(page_records):
            shots = sorted(pathlib.Path(record["Evidence"]).glob("t*.png"))
            if not shots:
                continue
            with Image.open(shots[-1]) as screenshot:
                screenshot = screenshot.convert("RGB")
                screenshot.thumbnail((cell_w, image_h))
                col, row = index % args.columns, index // args.columns
                x = col * cell_w + (cell_w - screenshot.width) // 2
                y = row * (image_h + label_h) + (image_h - screenshot.height) // 2
                sheet.paste(screenshot, (x, y))
                label = f"{record['TitleId']}\n{record['Name']}"
                draw.multiline_text((col * cell_w + 6, row * (image_h + label_h) + image_h + 4),
                                    label, fill="white", font=font, spacing=2)
        path = output / f"contact-{page_index // page_size + 1:02d}.jpg"
        sheet.save(path, quality=90)
        written.append(path)
    print("\n".join(str(path) for path in written))


if __name__ == "__main__":
    main()
