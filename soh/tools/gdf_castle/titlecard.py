"""A place-name title card (IA8, 144 x 24, as the game's own): white letters with a dark rim, in our own font
rendering (DejaVu Sans Condensed Bold, a free font), written as an o2r texture resource."""
import struct
from PIL import Image, ImageDraw, ImageFont, ImageFilter

FONT = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
W, H = 144, 24


def render(text, size=19):
    big = 4
    font = ImageFont.truetype(FONT, size * big)
    while True:
        box = font.getbbox(text)
        tw, th = box[2] - box[0], box[3] - box[1]
        if tw <= (W - 6) * big:
            break
        size -= 1
        font = ImageFont.truetype(FONT, size * big)
    m = Image.new('L', (W * big, H * big), 0)
    d = ImageDraw.Draw(m)
    d.text(((W * big - tw) // 2 - box[0], (H * big - th) // 2 - box[1]), text, fill=255, font=font)
    glyph = m.resize((W, H), Image.LANCZOS)
    rim = glyph.filter(ImageFilter.MaxFilter(3)).filter(ImageFilter.MaxFilter(3))
    data = bytearray()
    for y in range(H):
        for x in range(W):
            g = glyph.getpixel((x, y)) / 255.0
            r = rim.getpixel((x, y)) / 255.0
            i = round(15 * g)
            a = round(15 * max(g, min(1.0, r * 1.4)))
            data.append((i << 4) | a)
    return bytes(data)


def resource(data):
    head = b'\0\0\0\0' + b'XETO' + b'\0\0\0\0' + b'\xef\xbe\xad\xde' * 2 + b'\0' * 0x2C
    return head + struct.pack('<IIII', 8, W, H, len(data)) + data


def preview(data, path):
    im = Image.new('RGBA', (W, H))
    for i, b in enumerate(data):
        I = (b >> 4) * 17
        im.putpixel((i % W, i // W), (I, I, I, (b & 15) * 17))
    bg = Image.new('RGBA', (W, H), (90, 120, 90, 255))
    bg.alpha_composite(im)
    bg.resize((W * 4, H * 4), Image.NEAREST).save(path)


if __name__ == '__main__':
    import sys
    preview(render(sys.argv[1]), sys.argv[2])
