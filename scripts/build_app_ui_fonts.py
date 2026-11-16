"""Build bounded row-major 1-bit Lato cuts for the owner-test app designs."""
from pathlib import Path
import argparse
import struct
from PIL import Image, ImageDraw, ImageFont

parser = argparse.ArgumentParser()
parser.add_argument("font_directory", type=Path)
parser.add_argument("apps", nargs="+")
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
for app in args.apps:
    target = root / "apps" / app
    (target / "art").mkdir(exist_ok=True)
    for style, size in [("b", 28), ("b", 36), ("b", 20), ("r", 20)]:
        face = "Lato-Black.ttf" if style == "b" else "Lato-Regular.ttf"
        font = ImageFont.truetype(str(args.font_directory / face), size, layout_engine=ImageFont.Layout.BASIC)
        records, pixels = bytearray(), bytearray()
        for code in range(32, 256):
            char = chr(code) if code < 127 or code >= 160 else "?"
            left, top, right, bottom = font.getbbox(char, anchor="ls")
            width, height = right - left, bottom - top
            offset = 224 * 7 + len(pixels)
            records.extend(struct.pack("<BBbbbH", width, height, round(font.getlength(char)), left, top, offset))
            if not width or not height:
                continue
            image = Image.new("1", (width, height), 1)
            ImageDraw.Draw(image).text((-left, -top), char, font=font, anchor="ls", fill=0)
            pixels.extend(image.tobytes())
        assert len(records + pixels) < 65536
        (target / "art" / f"lato-{style}{size}.bin").write_bytes(records + pixels)
    (target / "FONT-LICENSE.txt").write_bytes((args.font_directory / "OFL-Lato.txt").read_bytes())
