#pragma once
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
inline int errors=0; inline uint8_t page=0,subview=0;
constexpr int GFX_NOT_DEFINED=-1;
struct TextBox { int x,y,w,h; };
class Arduino_GFX {
public:
  Arduino_GFX(int16_t=172,int16_t=320){}
  virtual ~Arduino_GFX()=default;
  virtual bool begin(int32_t=GFX_NOT_DEFINED){return true;}
  std::vector<uint16_t> pixels = std::vector<uint16_t>(172*320);
  std::vector<TextBox> texts;
  std::vector<std::string> textSamples;
  void fontBox(int x,int y,int w,int h,const char* s){textSamples.emplace_back(s);TextBox b{x,y,w,h};if(w && (x<0||y<0||x+w>172||y+h>320)){std::fprintf(stderr,"Font bounds: %s (%d,%d %dx%d)\n",s,x,y,w,h);++errors;}
    for(const auto& a:texts)if(w && a.w && b.x<a.x+a.w && b.x+b.w>a.x && b.y<a.y+a.h && b.y+b.h>a.y){std::fprintf(stderr,"Font overlap: %s m%u p%u\n",s,page,subview);++errors;}texts.push_back(b);}
  int tx=0,ty=0,size=2; uint16_t ink=0;
  virtual void writePixelPreclipped(int16_t x,int16_t y,uint16_t c){writeFillRectPreclipped(x,y,1,1,c);}
  virtual void writeFastHLine(int16_t x,int16_t y,int16_t w,uint16_t c){writeFillRectPreclipped(x,y,w,1,c);}
  virtual void writeFastVLine(int16_t x,int16_t y,int16_t h,uint16_t c){writeFillRectPreclipped(x,y,1,h,c);}
  void fillRect(int x,int y,int w,int h,uint16_t c){writeFillRectPreclipped(int16_t(x),int16_t(y),int16_t(w),int16_t(h),c);}
  virtual void writeFillRectPreclipped(int16_t x,int16_t y,int16_t w,int16_t h,uint16_t c) {
    for(int yy=std::max<int>(0,y); yy<std::min<int>(320,y+h); ++yy)
      for(int xx=std::max<int>(0,x);xx<std::min<int>(172,x+w);++xx) pixels[yy*172+xx]=c;
  }
  void draw16bitRGBBitmap(int16_t x,int16_t y,uint16_t* data,int16_t w,int16_t h){for(int yy=0;yy<h;++yy)for(int xx=0;xx<w;++xx)if(x+xx>=0 && x+xx<172 && y+yy>=0 && y+yy<320)pixels[(y+yy)*172+x+xx]=data[yy*w+xx];}
  void fillScreen(uint16_t c) { texts.clear();textSamples.clear(); fillRect(0,0,172,320,c); }
  void drawFastHLine(int x,int y,int w,uint16_t c){fillRect(x,y,w,1,c);}
  void drawFastVLine(int x,int y,int h,uint16_t c){fillRect(x,y,1,h,c);}
  void drawRect(int x,int y,int w,int h,uint16_t c) {
    drawFastHLine(x,y,w,c);drawFastHLine(x,y+h-1,w,c);
    drawFastVLine(x,y,h,c);drawFastVLine(x+w-1,y,h,c);
  }
  void fillRoundRect(int x,int y,int w,int h,int radius,uint16_t c){
    for(const auto &a:texts)if(a.w&&x<a.x+a.w&&x+w>a.x&&y<a.y+a.h&&y+h>a.y){
      std::fprintf(stderr,"Control covers text m%u v%u at %d,%d\\n",page,subview,a.x,a.y);++errors;
    }
    for(int yy=0;yy<h;++yy)for(int xx=0;xx<w;++xx){const int dx=xx<radius?radius-xx-1:xx>=w-radius?xx-(w-radius):0;const int dy=yy<radius?radius-yy-1:yy>=h-radius?yy-(h-radius):0;if(dx*dx+dy*dy<=radius*radius)fillRect(x+xx,y+yy,1,1,c);}
  }
  void drawRoundRect(int x,int y,int w,int h,int radius,uint16_t c){for(int yy=0;yy<h;++yy)for(int xx=0;xx<w;++xx){const int dx=xx<radius?radius-xx-1:xx>=w-radius?xx-(w-radius):0;const int dy=yy<radius?radius-yy-1:yy>=h-radius?yy-(h-radius):0;if(dx*dx+dy*dy<=radius*radius && (xx==0||yy==0||xx==w-1||yy==h-1||dx*dx+dy*dy>(radius-1)*(radius-1)))fillRect(x+xx,y+yy,1,1,c);}}
  void fillCircle(int x,int y,int r,uint16_t c) {
    for(int yy=-r; yy<=r; ++yy) for(int xx=-r;xx<=r;++xx)
      if(xx*xx+yy*yy<=r*r)fillRect(x+xx,y+yy,1,1,c);
  }
  void drawCircle(int x,int y,int r,uint16_t c){int px=r,py=0,err=0;while(px>=py){const int dx[]={px,py,-py,-px,-px,-py,py,px},dy[]={py,px,px,py,-py,-px,-px,-py};for(int i=0;i<8;++i)fillRect(x+dx[i],y+dy[i],1,1,c);++py;err+=1+2*py;if(2*(err-px)+1>0){--px;err+=1-2*px;}}}
  void drawLine(int x,int y,int xx,int yy,uint16_t c) {
    int dx=std::abs(xx-x),sx=x<xx?1:-1,dy=-std::abs(yy-y),sy=y<yy?1:-1,err=dx+dy;
    for(;;){ fillRect(x,y,1,1,c);if(x==xx&&y==yy)break;int e=2*err;
      if(e>=dy){err+=dy;x+=sx;}if(e<=dx){err+=dx;y+=sy;}}
  }
  void save(const char *path) {
    FILE *f=std::fopen(path,"wb");assert(f);std::fprintf(f,"P6\n172 320\n255\n");
    for(auto c:pixels){unsigned char rgb[]={static_cast<unsigned char>(((c>>11)&31)*255/31),
      static_cast<unsigned char>(((c>>5)&63)*255/63),static_cast<unsigned char>((c&31)*255/31)};std::fwrite(rgb,1,3,f);}
    std::fclose(f);
  }
};
