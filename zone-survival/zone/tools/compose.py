import sys
from PIL import Image, ImageDraw, ImageFont
ppm, txt, out = sys.argv[1:4]
img = Image.open(ppm).convert("RGBA")
ov = Image.new("RGBA", img.size, (0,0,0,0)); d = ImageDraw.Draw(ov)
fonts = {}
def F(sz):
    sz = int(round(sz))
    if sz not in fonts: fonts[sz] = ImageFont.truetype("/home/claude/zone/assets/DejaVuSans.ttf", sz)
    return fonts[sz]
for line in open(txt, encoding="utf-8"):
    p = line.rstrip("\n").split("\t")
    if len(p) < 8: continue
    x, y, sz, r, g, b, a = float(p[0]), float(p[1]), float(p[2]), int(p[3]), int(p[4]), int(p[5]), int(p[6])
    d.text((x, y - sz*0.12), p[7], font=F(sz*0.92), fill=(r, g, b, a))
Image.alpha_composite(img, ov).convert("RGB").save(out)
