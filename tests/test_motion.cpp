#include "../CameraCompanion/BufferedDisplay.cpp"
#include "CameraAnimation.h"
#include <cassert>
namespace CameraRadio {const char* profileName(Profile){return "AUTO";}}
using namespace CameraUi;
int main(){View from,target;from.home=true;target.home=false;target.page=Sony::Video;target.radio.link=CameraRadio::Link::Ready;target.radio.remote=true;
  Arduino_GFX oldCanvas,newCanvas;render(oldCanvas,from);render(newCanvas,target);
  for(auto direction:{Gesture::Left,Gesture::Right,Gesture::Up,Gesture::Down})for(bool bounce:{false,true}){
    Motion m;m.start(0,direction,bounce);assert(m.distance(0)==0 && m.done(m.duration));if(bounce)assert(m.distance(m.duration)==0);
    int last=0;for(int t=0;t<=m.duration;++t){const int d=abs(m.distance(t));assert(d<=(bounce?14:direction==Gesture::Up || direction==Gesture::Down?292:172));if(!bounce)assert(d>=last);last=d;}
    for(uint64_t at:{uint64_t(0),uint64_t(m.duration/2),uint64_t(m.duration)}){
      Arduino_GFX panel;BufferedDisplay display;display.attach(&panel);for(int band=0;band<8;++band){display.beginBand(band);renderMotion(display,target,from,m,at,band*40);display.presentBand();}
      const bool vertical=direction==Gesture::Up || direction==Gesture::Down;const int oldOffset=m.distance(at),span=vertical?292:172,newOffset=oldOffset+(direction==Gesture::Left || direction==Gesture::Up?span:-span);
      auto sample=[&](const Arduino_GFX& canvas,int x,int y,int offset,uint16_t& result){const int sx=x-(vertical?0:offset),sy=y-(vertical?offset:0);if(sx<0 || sx>=172 || sy<28 || sy>=320)return false;result=canvas.pixels[sy*172+sx];return true;};
      for(int y=0;y<320;++y)for(int x=0;x<172;++x){uint16_t expected=0;if(y<28)expected=newCanvas.pixels[y*172+x];else if(bounce)sample(newCanvas,x,y,oldOffset,expected);else {sample(oldCanvas,x,y,oldOffset,expected);sample(newCanvas,x,y,newOffset,expected);}assert(panel.pixels[y*172+x]==expected);}
    }
  }
  // Steady band glyph clipping must exactly match a full scene.
  Arduino_GFX panel;BufferedDisplay display;display.attach(&panel);for(int band=0;band<8;++band){display.beginBand(band);Cjk::clipTop=band*40;Cjk::clipBottom=band*40+40;render(display,target);display.presentBand();}Cjk::clipTop=0;Cjk::clipBottom=320;assert(panel.pixels==newCanvas.pixels);
  // Exercise every low-level primitive; real Arduino_GFX uses all four paths.
  auto primitives=[](Arduino_GFX& d){d.writePixelPreclipped(0,28,0xffff);d.writeFastHLine(-5,30,185,0x1234);d.writeFastVLine(85,-10,350,0x4321);d.writeFillRectPreclipped(-12,250,200,100,0x5555);};
  Arduino_GFX reference;primitives(reference);
  for(int dx:{-7,7})for(int dy:{-9,9}){Arduino_GFX p;BufferedDisplay b;b.attach(&p);for(int band=0;band<8;++band){b.beginBand(band);b.clearBand();b.setViewport(dx,dy,28,320);primitives(b);b.presentBand();}
    for(int y=0;y<320;++y)for(int x=0;x<172;++x){const int sx=x-dx,sy=y-dy;const uint16_t expected=y>=28 && sx>=0 && sx<172 && sy>=28 && sy<320?reference.pixels[sy*172+sx]:0;assert(p.pixels[y*172+x]==expected);}}
  std::puts("PASS: actual 40-row buffer + animation renderer: all directions/edges/midpoints/endpoints/fixed status, bounded easing/bounce, clipped glyph equivalence");
}
