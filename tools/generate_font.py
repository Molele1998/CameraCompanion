"""Generate Camera Companion bitmap glyphs from Noto Sans SC (SIL OFL 1.1)."""
import argparse, hashlib, math, re
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
ROOT=Path(__file__).resolve().parents[1]
EXPECTED_SHA256='a3041811a78c361b1de50f953c805e0244951c21c5bd412f7232ef0d899af0da'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--font',type=Path,required=True,help='Noto Sans SC variable TTF; see docs/FONT.md')
args=parser.parse_args()
font_sha=hashlib.sha256(args.font.read_bytes()).hexdigest()
if font_sha!=EXPECTED_SHA256:raise SystemExit('Font hash mismatch; obtain the documented Noto Sans SC input')
app=ROOT/'CameraCompanion';text=chr(0x7167)
for p in app.iterdir():
 if p.suffix in {'.h','.cpp'} and p.name!='ChineseFont.h':
  text+=''.join(re.findall(r'[\u4e00-\u9fff\u2000-\u206f\u3000-\u303f\uff00-\uffef\u00b7]',p.read_text(encoding='utf-8')))
chars=sorted(set(chr(c) for c in range(32,127))|set(text))
out=['#pragma once','#include <Arduino_GFX_Library.h>','#include <stdint.h>',
'// Camera Companion bitmap glyph subset derived from Noto Sans SC; SIL OFL 1.1.',
'// Copyright 2014-2021 Adobe; see licenses/NotoSansSC-OFL.txt.',
'// Modified bitmap representation; source TTF SHA-256: '+font_sha,
'namespace Cjk {','enum class Face:uint8_t { Small,Body,Title,Large };',
'struct Glyph {uint32_t code,offset;uint8_t advance;};',
'struct Font {const Glyph* glyphs;const uint8_t* bytes;uint16_t count;uint8_t width,height;};']
metadata=[]
for index,size in enumerate([16,20,22,32]):
 selected=chars if index<3 else sorted(set('0123456789'+chr(0x6301)+chr(0x7eed)+'?'))
 font=ImageFont.truetype(str(args.font),size);font.set_variation_by_axes([600 if index==2 else 400])
 width=size+4;height=size+4;data=[];glyphs=[]
 cap_height={16:12,20:15,22:17,32:24}[size];cap_top={16:2,20:3,22:3,32:4}[size]
 # Rasterize the cap reference using real ink bounds (not line-height metrics).
 ref=Image.new('L',(size*3,size*3));ImageDraw.Draw(ref).text((0,0),'H',font=font,fill=255);ref_box=ref.getbbox()
 for ch in selected:
  raw=Image.new('L',(size*3,size*3));ImageDraw.Draw(raw).text((0,0),ch,font=font,fill=255)
  im=Image.new('L',(width,height));box=raw.getbbox()
  if box:
   ink=raw.crop(box);h=box[3]-box[1];w=box[2]-box[0]
   if 0x4e00<=ord(ch)<=0x9fff:
    target_h=size-1;y=1;factor=target_h/h
   elif ch in '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ':
    target_h=cap_height;y=cap_top;factor=target_h/h
   else:
    factor=cap_height/(ref_box[3]-ref_box[1]);target_h=max(1,round(h*factor));y=cap_top+round((box[1]-ref_box[1])*factor)
   target_w=min(width-2,max(1,round(w*factor)))
   ink=ink.resize((target_w,target_h),Image.Resampling.LANCZOS);im.paste(ink,(1,y))
  advance=math.ceil(font.getlength(ch))+1
  glyphs.append('{0x%04x,%du,%d}'%(ord(ch),len(data),advance))
  for y in range(height):
   for x in range(0,width,2):
    data.append(((im.getpixel((x,y))+8)//17<<4)|((im.getpixel((x+1,y))+8)//17))
 out.append('inline const uint8_t data%d[]={\n'%index+',\n'.join(','.join('0x%02x'%v for v in data[i:i+32]) for i in range(0,len(data),32))+'\n};')
 out.append('inline const Glyph glyph%d[]={\n'%index+',\n'.join(glyphs)+'\n};')
 metadata.append('{glyph%d,data%d,%d,%d,%d}'%(index,index,len(glyphs),width,height))
out.append('inline const Font fonts[]={'+','.join(metadata)+'};')

out.append("\ninline uint32_t decode(const char*& s){const uint8_t a=uint8_t(*s++);if(a<128)return a;\n if(a>=0xc2 && a<=0xdf && (uint8_t(s[0])&0xc0)==0x80){const uint32_t b=uint8_t(*s++);return ((a&31)<<6)|(b&63);}\n if(a>=0xe0 && a<=0xef && s[0] && s[1] && (uint8_t(s[0])&0xc0)==0x80 && (uint8_t(s[1])&0xc0)==0x80){const uint32_t b=uint8_t(*s++),c=uint8_t(*s++);const uint32_t cp=((a&15)<<12)|((b&63)<<6)|(c&63);return cp>=0x800 && (cp<0xd800 || cp>0xdfff)?cp:0xfffd;}\n if(a>=0xf0 && a<=0xf4 && s[0] && s[1] && s[2] && (uint8_t(s[0])&0xc0)==0x80 && (uint8_t(s[1])&0xc0)==0x80 && (uint8_t(s[2])&0xc0)==0x80){const uint32_t b=uint8_t(*s++),c=uint8_t(*s++),d=uint8_t(*s++);const uint32_t cp=((a&7)<<18)|((b&63)<<12)|((c&63)<<6)|(d&63);return cp>=0x10000 && cp<=0x10ffff?cp:0xfffd;}\n return 0xfffd;}\ninline const Glyph* glyph(uint32_t cp,Face face){const auto& f=fonts[uint8_t(face)];int lo=0,hi=f.count;\n while(lo<hi){const int mid=(lo+hi)/2;if(f.glyphs[mid].code<cp)lo=mid+1;else hi=mid;}\n return lo<f.count && f.glyphs[lo].code==cp?&f.glyphs[lo]:nullptr;}\ninline int width(const char* s,Face face=Face::Body){int n=0;while(*s){const auto* g=glyph(decode(s),face);n+=g?g->advance:fonts[uint8_t(face)].width;}return n;}\ninline uint16_t blend(uint16_t fg,uint16_t bg,int a){const int b=15-a;\n return uint16_t((((((fg>>11)&31)*a+((bg>>11)&31)*b+7)/15)<<11)|\n (((((fg>>5)&63)*a+((bg>>5)&63)*b+7)/15)<<5)|\n (((fg&31)*a+(bg&31)*b+7)/15));}\ninline void draw(Arduino_GFX& g,int x,int y,const char* s,uint16_t color,uint16_t bg=0,Face face=Face::Body){\n const auto& f=fonts[uint8_t(face)];\n#ifdef CAMERA_FONT_HOST\n g.fontBox(x,y,width(s,face),f.height,s);\n#endif\n uint16_t palette[16];for(int i=0;i<16;++i)palette[i]=blend(color,bg,i);\n while(*s){const auto cp=decode(s);const auto* ch=glyph(cp,face);\n  if(!ch){\n#ifdef CAMERA_FONT_HOST\n   ++errors;\n#endif\n   ch=glyph('?',face);\n  }\n  if(!ch)continue;\n  const uint8_t* bits=f.bytes+ch->offset;\n  for(int row=0;row<f.height;++row)for(int col=0;col<f.width;++col){const uint8_t value=bits[(row*f.width+col)/2];const uint8_t a=(col&1)?(value&15):(value>>4);\n   if(a)g.fillRect(x+col,y+row,1,1,palette[a]);}\n  x+=ch->advance;\n }\n}\n}\n")
(app/'ChineseFont.h').write_text('\n'.join(out)+'\n',encoding='utf-8')
print('Generated',len(chars),'OFL glyphs; font hash:',font_sha)
