#include "CameraScene.h"
#include <cassert>
#include <cstdio>
namespace CameraRadio {
const char* linkName(Link){return "READY";}
const char* profileName(Profile p){static const char* names[]={"Auto","Legacy","Modern"};return names[uint8_t(p)];}
}
using namespace CameraUi;
int scenes=0;
void save(const View& v,const char* dir,const char* name){page=v.module;subview=v.page;
  if(!v.home){for(uint8_t i=0;i<controls(v);++i){const auto box=buttonRect(v,i);assert(box.w==156 && box.h==64);assert(Cjk::width(buttonText(v,i),Cjk::Face::Body)<=144);
    for(int y=box.y;y<box.y+box.h;++y)for(int x=box.x;x<box.x+box.w;++x)assert(hit(v,x,y)==i);}
    assert(hit(v,86,303)==4);
  }
  Arduino_GFX canvas;render(canvas,v);char path[512];snprintf(path,sizeof path,"%s/%s.ppm",dir,name);canvas.save(path);++scenes;
}
int main(int argc,char** argv){assert(argc==2);const char* dir=argv[1];
  const char* dot="·";assert(Cjk::decode(dot)==0xb7 && !*dot);const char* word="模";assert(Cjk::decode(word)==0x6a21 && !*word);
  assert(kStatusItems[0].x==4 && kStatusItems[3].x+kStatusItems[3].w==168);
  for(int i=1;i<4;++i)assert(kStatusItems[i].x-(kStatusItems[i-1].x+kStatusItems[i-1].w)==8);
  for(uint32_t battery:{0u,2500u,3970u,4500u})for(bool sim:{false,true}){
    View v;v.batteryMv=battery;v.radio.sim=sim;Arduino_GFX bar;statusBar(bar,v);
    for(const auto& box:kStatusItems){int first=320,last=-1;for(int y=0;y<28;++y)for(int x=box.x;x<box.x+box.w;++x)if(bar.pixels[y*172+x]){first=std::min(first,y);last=std::max(last,y);}
      if(first!=7 || last!=21)std::fprintf(stderr,"Status ink bounds %d..%d, battery=%u sim=%d x=%d\n",first,last,battery,sim,box.x);
      assert(first==7 && last==21);}
    v.home=false;v.module=0;v.page=Sony::Home;Arduino_GFX app;render(app,v);
    assert(std::equal(bar.pixels.begin(),bar.pixels.begin()+28*172,app.pixels.begin()));
  }
  assert(gesture(-80,2,80,2,200)==Gesture::Left);assert(gesture(80,2,80,2,200)==Gesture::Right);
  assert(gesture(2,-80,2,80,200)==Gesture::Up);assert(gesture(2,80,2,80,200)==Gesture::Down);
  assert(gesture(2,2,2,2,200)==Gesture::Tap);assert(gesture(0,0,80,2,200)==Gesture::None);
  assert(gesture(60,60,60,60,200)==Gesture::None);assert(gesture(0,0,0,0,1500)==Gesture::None);
  const uint8_t modules[]={0,2,3,4,5,6};
  for(int mode=0;mode<7;++mode)for(auto m:modules)for(uint8_t p=0;p<pages(m);++p){View v;v.home=false;v.module=m;v.page=p;v.heap=123456;v.nowMs=12345;
    v.plan.count=9999;v.plan.intervalMs=86400000;v.plan.delayMs=3600000;v.radio.activePlan=v.plan;v.radio.activePlan.count=30;
    v.radio.count=mode?6:0;v.peer=mode?5:0;v.radio.link=mode==0?CameraRadio::Link::Idle:mode==2?CameraRadio::Link::Reconnecting:mode==3?CameraRadio::Link::Discovering:CameraRadio::Link::Ready;
    v.radio.sim=mode!=0;v.radio.simFix=mode!=2;v.radio.fix=Camera::simulate(123450);v.radio.location=v.radio.remote=mode!=0 && mode!=3;
    v.radio.saved=mode!=0;snprintf(v.radio.name,sizeof v.radio.name,"A very long camera device name");snprintf(v.radio.savedName,sizeof v.radio.savedName,"Saved Sony ILCE-7M4 123456");
    for(auto& peer:v.radio.peers){snprintf(peer.name,sizeof peer.name,"Longest camera name test");peer.rssi=-100;peer.type=1;}
    v.radio.mtu=517;v.radio.error=mode==6?-144:0;v.radio.triggers=9999;v.radio.skipped=9999;v.radio.gpsWrites=99999;
    v.radio.profile=CameraRadio::Profile(mode%3);v.radio.phase=mode==2?Camera::Phase::Paused:mode==4?Camera::Phase::Wait:mode==5?Camera::Phase::Done:mode==6?Camera::Phase::Error:Camera::Phase::Idle;
    v.radio.captureKind=mode>=2?CameraRadio::CaptureKind::Interval:CameraRadio::CaptureKind::Single;
    char name[64];snprintf(name,sizeof name,"state_%d_m%u_p%u",mode,m,p);save(v,dir,name);
  }
  for(int offset=0;offset<=kAppCount-2;++offset)for(int state=0;state<5;++state){View v;v.homeOffset=offset;v.batteryMv=state?4150:0;v.radio.sim=state==1;v.radio.link=state?CameraRadio::Link::Ready:CameraRadio::Link::Idle;
    if(state>=2){v.radio.captureKind=CameraRadio::CaptureKind::Interval;v.radio.phase=state==2?Camera::Phase::Wait:state==3?Camera::Phase::Paused:Camera::Phase::Done;}
    for(int i=0;i<2;++i){const auto& box=kAppSlots[i];assert(Cjk::width(kApps[offset+i].name,Cjk::Face::Body)<=156);for(int y=box.y;y<box.y+box.h;++y)for(int x=box.x;x<box.x+box.w;++x)assert(homeHit(v,x,y)==offset+i);}
    char name[64];snprintf(name,sizeof name,"home_%d_%d",offset,state);save(v,dir,name);
  }
  for(int i=0;i<4;++i){View v;v.home=false;v.module=i<2?0:4;v.page=i<2?uint8_t(Sony::Connection):uint8_t(Settings::Forget);v.confirm=i<2?Confirm::Disconnect:Confirm::Forget;
    v.radio.link=i%2?CameraRadio::Link::Ready:CameraRadio::Link::Idle;v.radio.saved=true;snprintf(v.radio.savedName,sizeof v.radio.savedName,"ILCE-7M4");char name[32];snprintf(name,sizeof name,"confirm_%d",i);save(v,dir,name);}
  for(auto m:modules)for(uint8_t p=0;p<pages(m);++p){View v;v.home=false;v.module=m;v.page=p;v.nowMs=1000;v.pendingUntil=1500;v.noticeUntil=2000;snprintf(v.notice,sizeof v.notice,"未发送，请重试");v.awaitingStart=m==2 && p==Timer::Run;
    char name[32];snprintf(name,sizeof name,"pending_m%u_p%u",m,p);save(v,dir,name);}
  for(bool home:{false,true})for(auto prompt:{CameraRadio::PairingPrompt::Compare,CameraRadio::PairingPrompt::Display})for(uint32_t code:{1u,999999u}){
    View v;v.home=home;v.module=0;v.page=Sony::Home;v.radio.pairingPrompt=prompt;v.radio.pairingCode=code;v.radio.pairingToken=1;v.radio.link=CameraRadio::Link::Pairing;
    assert(hitAction(v,hit(v,86,303))==Action::PairingReject && !menu(v));assert(!cardLink(v));
    assert(hitAction(v,hit(v,86,174))==(prompt==CameraRadio::PairingPrompt::Compare?Action::PairingAccept:Action::None));
    char name[64];snprintf(name,sizeof name,"pairing_%d_%d_%u",home,int(prompt),code);save(v,dir,name);
  }
  const char* demos[]={"demo_home0","demo_home1","demo_home2","demo_home3","demo_sony_menu","demo_shoot","demo_timer","demo_parameters","demo_running","demo_connect","demo_gps","demo_nikon"};
  for(int i=0;i<12;++i){View v;v.home=i<4;v.homeOffset=i<4?i:0;v.module=i==10?3:i==11?6:i>=6 && i<=8?2:0;v.page=i==5?uint8_t(Sony::Shoot):i==7?uint8_t(Timer::Parameters):i==8?uint8_t(Timer::Run):i==9?uint8_t(Sony::Connection):0;
    v.batteryMv=3970;v.heap=174080;v.radio.link=CameraRadio::Link::Ready;v.radio.remote=v.radio.location=v.radio.saved=true;v.radio.sim=true;v.radio.fix=Camera::simulate(123450);
    v.radio.phase=i==8?Camera::Phase::Paused:Camera::Phase::Idle;v.radio.captureKind=i==8?CameraRadio::CaptureKind::Interval:CameraRadio::CaptureKind::Single;
    v.radio.triggers=3;snprintf(v.radio.name,sizeof v.radio.name,"Sony A7 IV");snprintf(v.radio.savedName,sizeof v.radio.savedName,"Sony A7 IV");save(v,dir,demos[i]);
  }
  std::printf("Rendered %d actual firmware scenes; layout errors=%d\n",scenes,errors);assert(errors==0);
}
