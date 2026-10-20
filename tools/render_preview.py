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
labels = ['Page 1: Sony / DJI', 'Page 2: Nikon / Settings', 'Page 3: GPS', 'Back to page 1',
          'Sony photo page', 'Shutter / Focus', 'Interval plan', 'Plan parameters',
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

def gallery(filename, items, heading):
    rows=(len(items)+3)//4
    picture=Image.new('RGB',(784,rows*364+40),'#e4e7e9')
    drawing=ImageDraw.Draw(picture)
    drawing.text((12,8),heading,font=font,fill='#333333')
    for index,(name,label) in enumerate(items):
        x=index%4*196+12;y=index//4*364+40
        with Image.open(output/(name+'.png')) as scene:picture.paste(scene,(x,y))
        drawing.text((x,y+326),label,font=font,fill='#333333')
    picture.save(output/filename)

gallery('remote-controls.png',[
    ('demo_remote_photo','1/6 Photo / Interval / Focus'),('demo_video_unknown','2/6 Video: state unknown'),
    ('demo_remote_focus','3/6 Manual focus (MF)'),('demo_remote_zoom','4/6 Bluetooth zoom'),
    ('demo_remote_keys','5/6 AF-ON / C1'),('demo_remote_connection','6/6 Connection'),
    ('demo_video_pending','Video: waiting for camera'),('demo_video_recording','Video: recording duration')],
    'Remote controls: synthetic data from public firmware renderer')
gallery('gpx-backup.png',[
    ('gpx_state_0','No SD: start disabled'),('gpx_state_2','SD ready: backup off'),
    ('gpx_state_3','Simulated GPS track'),('gpx_state_5','Write failure: stopped')],
    'SD GPX backup: synthetic data, real SD behavior requires testing')

pairing=Image.new('RGB',(784,404),'#e4e7e9');drawing=ImageDraw.Draw(pairing)
drawing.text((12,8),'Pairing: synthetic example codes, no device secrets',font=font,fill='#333333')
for index,(name,label) in enumerate([
    ('pairing_0_1_1','Compare: explicit confirmation'),('pairing_0_1_999999','Compare: synthetic code'),
    ('pairing_0_2_1','Display: enter on camera'),('pairing_0_2_999999','Display: synthetic code')]):
    x=index*196+12
    # PairingPrompt values are defined in CameraRadio.h; no real PIN is read.
    if not (output/(name+'.png')).exists():
        raise RuntimeError('Missing synthetic pairing scene: '+name)
    with Image.open(output/(name+'.png')) as scene:pairing.paste(scene,(x,40))
    drawing.text((x,366),label,font=font,fill='#333333')
pairing.save(output/'pairing-preview.png')
print('Generated public preview images.')
