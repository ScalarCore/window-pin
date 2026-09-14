from pathlib import Path
from PIL import Image, ImageDraw

root = Path(__file__).parent
scale = 4
im = Image.new('RGBA', (256 * scale, 256 * scale))
d = ImageDraw.Draw(im)
def box(coords):
    return tuple(int(v * scale) for v in coords)
d.rounded_rectangle(box((8, 8, 248, 248)), radius=52 * scale, fill='#2563EB')
# A crisp white pushpin silhouette; large enough to read at tray size.
points = [(86, 48), (170, 48), (170, 70), (156, 78),
          (156, 116), (183, 144), (183, 158), (139, 158),
          (128, 215), (117, 158), (73, 158), (73, 144),
          (100, 116), (100, 78), (86, 70)]
d.polygon([(x * scale, y * scale) for x, y in points], fill='white')
im = im.resize((256, 256), Image.Resampling.LANCZOS)
im.save(root / 'WindowPin.ico', sizes=[(16,16), (20,20), (24,24), (32,32), (40,40), (48,48), (64,64), (128,128), (256,256)])
