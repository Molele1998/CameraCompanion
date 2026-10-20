"""Assemble actual C++ renderer output with synthetic fixtures and no system font."""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--input', type=Path, required=True)
args = parser.parse_args()
output = args.input
for path in output.glob('*.ppm'):
    with Image.open(path) as picture:
        picture.save(path.with_suffix('.png'))
names = ['demo_home0', 'demo_home1', 'demo_home2', 'demo_home3', 'demo_sony_menu',
         'demo_shoot', 'demo_timer', 'demo_parameters', 'demo_running',
         'demo_connect', 'demo_gps', 'demo_nikon']
labels = ['Sony / DJI', 'DJI / Nikon', 'Nikon / Settings', 'Settings / GPS',
          'Sony menu', 'Shutter / Focus', 'Interval plan', 'Plan parameters',
          'Running plan', 'Camera connection', 'GPS simulation', 'Nikon placeholder']
font = ImageFont.load_default(size=14)
sheet = Image.new('RGB', (784, 1132), '#e4e7e9')
draw = ImageDraw.Draw(sheet)
draw.text((12, 8), 'Synthetic preview from public firmware renderer; not a device screenshot',
          font=font, fill='#333333')
for index, name in enumerate(names):
    x = index % 4 * 196 + 12
    y = index // 4 * 364 + 40
    with Image.open(output / (name + '.png')) as picture:
        sheet.paste(picture, (x, y))
    draw.text((x, y + 326), labels[index], font=font, fill='#333333')
sheet.save(output / 'overview.png')
strip = Image.new('RGB', (540, 132), '#e4e7e9')
ImageDraw.Draw(strip).text((12, 7), 'Status bar: synthetic data, 3x', font=font, fill='#333333')
with Image.open(output / 'demo_shoot.png') as picture:
    strip.paste(picture.crop((0, 0, 172, 28)).resize((516, 84), Image.Resampling.NEAREST), (12, 36))
strip.save(output / 'status-bar.png')
print('Generated public preview images.')
