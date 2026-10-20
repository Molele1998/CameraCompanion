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
  for(uint32_t battery:{0u,2500u,3970u,4500u})for(bool sim:{false,true})for(auto video:{Camera::Video::Unknown,Camera::Video::Stopped,Camera::Video::Recording}){
    View v;v.batteryMv=battery;v.radio.sim=sim;v.radio.link=CameraRadio::Link::Ready;v.radio.video=video;Arduino_GFX bar;statusBar(bar,v);
    for(const auto& box:kStatusItems){int first=320,last=-1;for(int y=0;y<28;++y)for(int x=box.x;x<box.x+box.w;++x)if(bar.pixels[y*172+x]){first=std::min(first,y);last=std::max(last,y);}
      if(first!=7 || last!=21)std::fprintf(stderr,"Status ink bounds %d..%d, battery=%u sim=%d x=%d\n",first,last,battery,sim,box.x);
      assert(first==7 && last==21);}
    v.home=false;v.module=0;v.page=Sony::Shoot;Arduino_GFX app;render(app,v);
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
  for(int offset=0;offset<kAppCount;offset+=2)for(int state=0;state<5;++state){View v;v.homeOffset=offset;v.batteryMv=state?4150:0;v.radio.sim=state==1;v.radio.link=state?CameraRadio::Link::Ready:CameraRadio::Link::Idle;
    if(state>=2){v.radio.captureKind=CameraRadio::CaptureKind::Interval;v.radio.phase=state==2?Camera::Phase::Wait:state==3?Camera::Phase::Paused:Camera::Phase::Done;}
    for(int i=0;i<2;++i){const auto& box=kAppSlots[i];if(offset+i>=kAppCount){assert(homeHit(v,86,box.y+30)==-1);continue;}assert(Cjk::width(kApps[offset+i].name,Cjk::Face::Body)<=156);for(int y=box.y;y<box.y+box.h;++y)for(int x=box.x;x<box.x+box.w;++x)assert(homeHit(v,x,y)==offset+i);}
    char name[64];snprintf(name,sizeof name,"home_%d_%d",offset,state);save(v,dir,name);
  }
  for(int i=0;i<4;++i){View v;v.home=false;v.module=i<2?0:4;v.page=i<2?uint8_t(Sony::Connection):uint8_t(Settings::Forget);v.confirm=i<2?Confirm::Disconnect:Confirm::Forget;
    v.radio.link=i%2?CameraRadio::Link::Ready:CameraRadio::Link::Idle;v.radio.saved=true;snprintf(v.radio.savedName,sizeof v.radio.savedName,"ILCE-7M4");char name[32];snprintf(name,sizeof name,"confirm_%d",i);save(v,dir,name);}
  for(auto m:modules)for(uint8_t p=0;p<pages(m);++p){View v;v.home=false;v.module=m;v.page=p;v.nowMs=1000;v.pendingUntil=1500;v.noticeUntil=2000;snprintf(v.notice,sizeof v.notice,"未发送，请重试");v.awaitingStart=m==2 && p==Timer::Run;
    char name[32];snprintf(name,sizeof name,"pending_m%u_p%u",m,p);save(v,dir,name);}
  for(bool home:{false,true})for(auto prompt:{CameraRadio::PairingPrompt::Compare,CameraRadio::PairingPrompt::Display})for(uint32_t code:{1u,999999u}){
    View v;v.home=home;v.module=0;v.page=Sony::Shoot;v.radio.pairingPrompt=prompt;v.radio.pairingCode=code;v.radio.pairingToken=1;v.radio.link=CameraRadio::Link::Pairing;
    assert(hitAction(v,hit(v,86,303))==Action::PairingReject && !menu(v));assert(!cardLink(v));
    assert(hitAction(v,hit(v,86,174))==(prompt==CameraRadio::PairingPrompt::Compare?Action::PairingAccept:Action::None));
    char name[64];snprintf(name,sizeof name,"pairing_%d_%d_%u",home,int(prompt),code);save(v,dir,name);
  }
  const char* demos[]={"demo_home0","demo_home1","demo_home2","demo_home3","demo_sony_menu","demo_shoot","demo_timer","demo_parameters","demo_running","demo_connect","demo_gps","demo_nikon"};
  for(int i=0;i<12;++i){View v;v.home=i<4;v.homeOffset=i<3?i*2:0;v.module=i==10?3:i==11?6:i>=6 && i<=8?2:0;v.page=i==5?uint8_t(Sony::Shoot):i==7?uint8_t(Timer::Parameters):i==8?uint8_t(Timer::Run):i==9?uint8_t(Sony::Connection):0;
    v.batteryMv=3970;v.heap=174080;v.radio.link=CameraRadio::Link::Ready;v.radio.remote=v.radio.location=v.radio.saved=true;v.radio.sim=true;v.radio.fix=Camera::simulate(123450);
    v.radio.phase=i==8?Camera::Phase::Paused:Camera::Phase::Idle;v.radio.captureKind=i==8?CameraRadio::CaptureKind::Interval:CameraRadio::CaptureKind::Single;
    v.radio.triggers=3;snprintf(v.radio.name,sizeof v.radio.name,"Sony A7 IV");snprintf(v.radio.savedName,sizeof v.radio.savedName,"Sony A7 IV");save(v,dir,demos[i]);
  }
  for(uint8_t index=0;index<kSonyPageCount;++index){View v;v.home=false;v.page=kSonyPages[index];char name[32];snprintf(name,sizeof name,"sony_function_%u",index);save(v,dir,name);assert(sonyPageIndex(v)==index);}
  for(int state=0;state<9;++state){View v;v.home=false;v.page=Sony::Video;v.nowMs=50000;v.batteryMv=3970;v.radio.link=CameraRadio::Link::Ready;v.radio.remote=true;
    v.radio.video=state>=5 && state<=6?Camera::Video::Recording:state>=3 && state<=4?Camera::Video::Stopped:Camera::Video::Unknown;
    v.radio.notifications=state!=0;v.radio.recordPending=state==2 || state==4 || state==6;v.radio.lastVideoNotify=40000;v.radio.videoSince=10000;
    v.radio.error=state==7?-61:0;if(state==8){v.radio.phase=Camera::Phase::Wait;v.radio.captureKind=CameraRadio::CaptureKind::Interval;}
    char name[40];snprintf(name,sizeof name,"video_state_%d",state);save(v,dir,name);
    Arduino_GFX canvas;render(canvas,v);const char* expected=v.radio.recordPending?"等待相机确认":v.radio.video==Camera::Video::Recording?"正在录像":v.radio.video==Camera::Video::Stopped?"录像已停止":"录像状态未知";
    assert(std::find(canvas.textSamples.begin(),canvas.textSamples.end(),expected)!=canvas.textSamples.end());
    if(v.radio.video==Camera::Video::Unknown)assert(std::find(canvas.textSamples.begin(),canvas.textSamples.end(),"正在录像")==canvas.textSamples.end());
    assert((std::find(canvas.textSamples.begin(),canvas.textSamples.end(),"录制 00:00:40")!=canvas.textSamples.end())==(v.radio.video==Camera::Video::Recording));
    for(const auto& text:canvas.textSamples){assert(text.find("秒前")==std::string::npos);if(v.radio.video!=Camera::Video::Recording)assert(text.find("录制 ")==std::string::npos);}
  }
  const struct {uint64_t seconds;const char* label;} timerCases[]={{0,"录制 00:00:00"},{59,"录制 00:00:59"},{60,"录制 00:01:00"},{3599,"录制 00:59:59"},{3600,"录制 01:00:00"},{360061,"录制 100:01:01"}};
  for(const auto& sample:timerCases){View v;v.home=false;v.page=Sony::Video;v.radio.link=CameraRadio::Link::Ready;v.radio.remote=true;v.radio.video=Camera::Video::Recording;
    v.radio.videoSince=4294967000ULL;v.nowMs=v.radio.videoSince+sample.seconds*1000;Arduino_GFX canvas;render(canvas,v);
    assert(std::find(canvas.textSamples.begin(),canvas.textSamples.end(),sample.label)!=canvas.textSamples.end());}
  for(uint8_t p:{uint8_t(Sony::Keys),uint8_t(Sony::ManualFocus),uint8_t(Sony::Zoom)})for(bool busy:{false,true})for(auto video:{Camera::Video::Unknown,Camera::Video::Stopped,Camera::Video::Recording}){
    View v;v.home=false;v.page=p;v.radio.link=CameraRadio::Link::Ready;v.radio.remote=true;v.radio.remoteBusy=v.radio.remoteHeld=busy;v.radio.video=video;
    v.radio.remoteKey=p==Sony::Keys?Camera::RemoteKey::AfOn:p==Sony::ManualFocus?Camera::RemoteKey::FocusNear:Camera::RemoteKey::ZoomWide;v.pressed=busy?0:-1;
    char name[48];snprintf(name,sizeof name,"remote_p%u_b%d_v%d",p,busy,int(video));save(v,dir,name);
  }
  for(bool pending:{false,true}){View v;v.home=true;v.radio.link=CameraRadio::Link::Ready;v.radio.video=pending?Camera::Video::Unknown:Camera::Video::Recording;v.radio.recordPending=pending;save(v,dir,pending?"home_video_pending":"home_recording");assert(homeHit(v,86,303)==kAppCount);}
  const char* remoteDemos[]={"demo_remote_photo","demo_remote_connection","demo_video_unknown","demo_video_pending","demo_video_recording","demo_remote_keys","demo_remote_focus","demo_remote_zoom"};
  for(int i=0;i<8;++i){View v;v.home=false;v.page=i==0?Sony::Shoot:i==1?Sony::Connection:i<5?Sony::Video:i==5?Sony::Keys:i==6?Sony::ManualFocus:Sony::Zoom;v.nowMs=50000;v.batteryMv=3970;
    v.radio.link=CameraRadio::Link::Ready;v.radio.remote=v.radio.notifications=true;v.radio.video=i==4?Camera::Video::Recording:Camera::Video::Unknown;v.radio.recordPending=i==3;v.radio.lastVideoNotify=v.radio.videoSince=47000;save(v,dir,remoteDemos[i]);}
  for(int i=0;i<6;++i){View v;v.home=false;v.module=3;v.page=Gps::Backup;v.gpx.available=true;v.gpx.mounted=i>0 && i!=5;v.gpx.busy=i==1;v.gpx.recording=i==3 || i==4;v.gpx.points=i==4?999999:20;v.gpx.error=i==5?-84:0;
    v.radio.sim=v.radio.simFix=true;v.radio.fix=Camera::simulate(12000);
    snprintf(v.gpx.file,sizeof v.gpx.file,"SIM_ABC123.gpx");char name[32];snprintf(name,sizeof name,"gpx_state_%d",i);save(v,dir,name);}
  std::printf("Rendered %d actual firmware scenes; layout errors=%d\n",scenes,errors);assert(errors==0);
}
