#pragma once
#include <algorithm>
#include "CameraScene.h"
namespace CameraUi {
struct Motion {
  bool active=false,bounce=false;Gesture direction=Gesture::Left;uint64_t started=0;uint16_t duration=220;
  void start(uint64_t now,Gesture g,bool edge=false){active=true;bounce=edge;direction=g;started=now;duration=edge?180:220;}
  int distance(uint64_t now)const{
    const uint64_t elapsed=now>started?now-started:0;const int t=int(elapsed>=duration?1024:elapsed*1024/duration);
    const int sign=direction==Gesture::Left || direction==Gesture::Up?-1:1;
    if(bounce)return sign*int(int64_t(4)*t*(1024-t)*14/(1024*1024));
    const int left=1024-t;const int ease=1024-int(int64_t(left)*left*left/(1024*1024));
    return sign*(direction==Gesture::Up || direction==Gesture::Down?292:172)*ease/1024;
  }
  bool done(uint64_t now)const{return now>=started && now-started>=duration;}
};
template<class Display> inline void renderMotion(Display& g,const View& target,const View& from,const Motion& motion,uint64_t now,int bandTop){
  g.setViewport();g.clearBand(kBg);Cjk::clipTop=bandTop;Cjk::clipBottom=bandTop+40;statusBar(g,target);
  const bool vertical=motion.direction==Gesture::Up || motion.direction==Gesture::Down;
  const int offset=motion.distance(now),span=vertical?292:172;
  auto layer=[&](const View& v,int offsetLayer){const int dx=vertical?0:offsetLayer,dy=vertical?offsetLayer:0;
    g.setViewport(dx,dy,28,320);Cjk::clipTop=std::max<int>(28,bandTop-dy);Cjk::clipBottom=std::min<int>(320,bandTop+40-dy);render(g,v);};
  if(motion.bounce)layer(target,offset);
  else {layer(from,offset);layer(target,offset+(motion.direction==Gesture::Left || motion.direction==Gesture::Up?span:-span));}
  g.setViewport();Cjk::clipTop=0;Cjk::clipBottom=320;
}
}
