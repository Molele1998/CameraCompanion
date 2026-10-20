"""Build original small device icons from local vector drawing instructions."""
from pathlib import Path
import math
from PIL import Image, ImageDraw
ROOT=Path(__file__).resolve().parents[1]
assets=ROOT/'assets';assets.mkdir(exist_ok=True)
scale=4
def rect(draw,box,fill=None,outline=None,width=1,radius=0):
    box=tuple(round(v*scale) for v in box)
    draw.rounded_rectangle(box,radius=round(radius*scale),fill=fill,outline=outline,width=round(width*scale))
def circle(draw,box,fill=None,outline=None,width=1):draw.ellipse(tuple(round(v*scale) for v in box),fill=fill,outline=outline,width=round(width*scale))
def line(draw,points,fill='white',width=2):draw.line([(round(x*scale),round(y*scale)) for x,y in points],fill=fill,width=round(width*scale),joint='curve')
def polygon(draw,points,fill):draw.polygon([(round(x*scale),round(y*scale)) for x,y in points],fill=fill)
allruns=[]
for index,(name,bg) in enumerate([('sony',(43,116,235,255)),('timer',(243,155,35,255)),('gps',(35,181,112,255)),('settings',(111,117,132,255)),('dji',(121,91,205,255)),('nikon',(235,177,34,255))]):
    im=Image.new('RGBA',(64*scale,64*scale));d=ImageDraw.Draw(im);rect(d,(0,0,63,63),fill=bg,radius=16)
    if index==0:
        rect(d,(10,23,54,50),outline='white',width=2,radius=4);rect(d,(21,16,43,24),outline='white',width=2,radius=2)
        circle(d,(22,27,42,47),outline='white',width=2);circle(d,(46,28,49,31),fill='white')
    elif index==1:
        circle(d,(11,11,53,53),outline='white',width=2);line(d,[(32,18),(32,32),(44,40)],width=2);circle(d,(30,30,34,34),fill='white')
    elif index==2:
        polygon(d,[(18,31),(47,17),(33,48),(29,35)],'white');polygon(d,[(29,31),(40,24),(33,38),(32,33)],bg)
    elif index==4:
        line(d,[(19,19),(45,45)],width=3);line(d,[(19,45),(45,19)],width=3)
        for x,y in [(18,18),(46,18),(18,46),(46,46)]:circle(d,(x-8,y-8,x+8,y+8),outline='white',width=2)
        rect(d,(25,24,39,40),fill='white',radius=4)
    elif index==5:
        rect(d,(11,23,53,48),outline='white',width=2,radius=4);polygon(d,[(23,23),(27,15),(39,15),(43,23)],'white')
        circle(d,(23,26,43,46),outline='white',width=2);line(d,[(14,18),(20,18)],width=2)
    else:
        points=[]
        for i in range(64):
            angle=i*math.pi/32;radius=22 if i%8 in (1,2,3,4) else 18
            points.append((32+radius*math.cos(angle),32+radius*math.sin(angle)))
        polygon(d,points,'white');circle(d,(18,18,46,46),fill=bg);circle(d,(25,25,39,39),outline='white',width=2)
    im=im.resize((64,64),Image.Resampling.LANCZOS);im.save(assets/(name+'.png'))
    encoded=[]
    pixels=im.get_flattened_data() if hasattr(im,'get_flattened_data') else im.getdata()
    for red,green,blue,alpha in pixels:
        a=(alpha+8)//17
        color=((red>>3)<<11)|((green>>2)<<5)|(blue>>3)
        encoded.append((a<<16)|color if a else 0)
    runs=[];i=0
    while i<len(encoded):
        j=i+1
        while j<len(encoded) and encoded[j]==encoded[i] and j-i<4095:j+=1
        runs.append(((j-i)<<20)|encoded[i]);i=j
    allruns.append(runs)
lines=['#pragma once','#include "ChineseFont.h"','namespace AppIcons {']
for i,runs in enumerate(allruns):lines.append('inline const uint32_t icon%d[]={\n'%i+',\n'.join(','.join('0x%08xu'%n for n in runs[k:k+12]) for k in range(0,len(runs),12))+'\n};')
lines.append('inline const uint32_t* icons[]={icon0,icon1,icon2,icon3,icon4,icon5};')
lines.append('''inline void draw(Arduino_GFX& g,uint8_t app,int x,int y,uint16_t bg){const uint32_t* run=icons[app%6];int position=0;
while(position<4096){const uint32_t code=*run++;int n=int(code>>20);const int alpha=(code>>16)&15;const uint16_t color=Cjk::blend(uint16_t(code),bg,alpha);
while(n){const int row=position/64,col=position%64;const int count=n<64-col?n:64-col;if(alpha)g.fillRect(x+col,y+row,count,1,color);position+=count;n-=count;}}
}
}''')
(ROOT/'CameraCompanion/AppIcons.h').write_text('\n'.join(lines),encoding='utf-8')
print('Original antialiased icons:',sum(len(r)*4 for r in allruns),'RLE bytes')

# Monochrome status icons share an exact 24x15 visual box with the status text.
status=[]
for name in ['battery','camera']:
    im=Image.new('L',(24*scale,15*scale));d=ImageDraw.Draw(im)
    if name=='battery':
        rect(d,(0.5,0.5,20.5,14.2),outline=255,width=1.4,radius=2.5)
        rect(d,(21,5,23.5,10),fill=255,radius=0.8)
    else:
        rect(d,(0.5,4.5,23.2,14.2),outline=255,width=1.4,radius=2)
        rect(d,(7,0.5,17,5),outline=255,width=1.4,radius=1)
        circle(d,(8,6,16,13.5),outline=255,width=1.3)
    im=im.resize((24,15),Image.Resampling.LANCZOS)
    alpha=[(a+8)//17 for a in im.get_flattened_data()]
    assert min(i//24 for i,a in enumerate(alpha) if a)==0
    assert max(i//24 for i,a in enumerate(alpha) if a)==14
    values=[(alpha[i]<<4)|alpha[i+1] for i in range(0,len(alpha),2)]
    status.append('inline const uint8_t %s[]={%s};'%(name,','.join(str(a) for a in values)))
status=['#pragma once','#include "ChineseFont.h"','namespace StatusIcons {']+status
status.append("""inline void draw(Arduino_GFX& g,bool isCamera,int x,int y,uint16_t color){const uint8_t* data=isCamera?camera:battery;
uint16_t palette[16];for(int a=0;a<16;++a)palette[a]=Cjk::blend(color,0,a);
for(int i=0;i<360;++i){const uint8_t packed=data[i/2],a=(i&1)?packed&15:packed>>4;if(a)g.fillRect(x+i%24,y+i/24,1,1,palette[a]);}}
}""")
(ROOT/'CameraCompanion/StatusIcons.h').write_text('\n'.join(status),encoding='utf-8')
