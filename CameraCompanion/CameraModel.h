#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <string.h>

namespace Camera {
struct SonyAdv { bool sony=false, pairing=false, remote=false,powerKnown=false,poweredOn=false; uint8_t protocol=0; };
inline SonyAdv parseSony(const uint8_t* p,size_t n) {
  SonyAdv a;
  if(!p || n<8 || p[0]!=0x2d || p[1]!=1 || p[2]!=3 || p[3]!=0) return a;
  a.sony=true;a.protocol=p[4];
  for(size_t i=8;i+2<n;i+=3) {if(p[i]==0x21){a.powerKnown=true;a.poweredOn=(p[i+1]&0x40)!=0;}if(p[i]==0x22) {
    const uint16_t flags=p[i+1]|(uint16_t(p[i+2])<<8);
    a.pairing=(flags&0x40)!=0;
    a.remote=(flags&(a.protocol==0x65?4:2))!=0;
  }}
  return a;
}
struct DateTime { uint16_t year; uint8_t month,day,hour,minute,second; };
inline DateTime utc(uint64_t epoch) {
  uint64_t days=epoch/86400;
  DateTime d{1970,1,1,uint8_t(epoch/3600%24),uint8_t(epoch/60%60),uint8_t(epoch%60)};
  auto leap=[](int y){return y%4==0 && (y%100!=0 || y%400==0);};
  while(days>=uint64_t(leap(d.year)?366:365)) { days-=leap(d.year)?366:365;++d.year; }
  static const uint8_t months[]={31,28,31,30,31,30,31,31,30,31,30,31};
  for(;;) { const uint8_t n=months[d.month-1]+(d.month==2 && leap(d.year)?1:0);
    if(days<n) break;
    days-=n;++d.month;
  }
  d.day=uint8_t(days+1);return d;
}
struct Fix { double lat=0,lon=0,altitude=0;uint64_t epoch=0,generatedMs=0;bool valid=false; };
// An explicit synthetic route, not the user's location. 100m radius, 1m/s.
// The date is a test clock; no CC13 time-setting command is sent to the camera.
inline Fix simulate(uint64_t elapsedMs,uint64_t epoch=1792454400ULL) {
  const double t=(elapsedMs/1000)*0.01;
  Fix f;f.lat=22.540000+100.0*sin(t)/111320.0;
  f.lon=114.050000+100.0*(cos(t)-1.0)/(111320.0*cos(22.54*0.0174532925199433));
  f.altitude=20+sin(t*0.2);f.epoch=epoch+elapsedMs/1000;f.generatedMs=elapsedMs;f.valid=true;return f;
}
inline void be16(uint8_t* p,uint16_t v){p[0]=uint8_t(v>>8);p[1]=uint8_t(v);}
inline void be32(uint8_t* p,uint32_t v){p[0]=uint8_t(v>>24);p[1]=uint8_t(v>>16);p[2]=uint8_t(v>>8);p[3]=uint8_t(v);}
inline size_t locationPayload(const Fix& f,bool tz,int16_t offset,int16_t dst,uint8_t (&out)[95]) {
  if(!f.valid || !isfinite(f.lat) || !isfinite(f.lon) || fabs(f.lat)>90 || fabs(f.lon)>180 ||
     f.epoch<1577836800ULL || f.epoch>=4102444800ULL) return 0;
  const size_t n=tz?95:91;memset(out,0,sizeof out);be16(out,uint16_t(n-2));
  out[2]=8;out[3]=2;out[4]=0xfc;out[5]=tz?3:0;out[8]=out[9]=out[10]=0x10;
  be32(out+11,uint32_t(int32_t(llround(f.lat*1e7))));
  be32(out+15,uint32_t(int32_t(llround(f.lon*1e7))));
  const auto d=utc(f.epoch);be16(out+19,d.year);out[21]=d.month;out[22]=d.day;
  out[23]=d.hour;out[24]=d.minute;out[25]=d.second;
  if(tz){be16(out+91,uint16_t(offset));be16(out+93,uint16_t(dst));}
  return n;
}
enum class Action:uint8_t { None,HalfDown,FullDown,FullUp,HalfUp };
inline uint8_t commandByte(Action a) {
  switch(a){case Action::HalfDown:return 7;case Action::FullDown:return 9;
    case Action::FullUp:return 8;case Action::HalfUp:return 6;default:return 0;}
}
enum class Phase:uint8_t { Idle,Countdown,FocusWait,Hold,Release,Wait,Stopping,Paused,Done,Error };
inline const char* phaseName(Phase p) {
  static const char* names[]={"IDLE","COUNTDOWN","FOCUS","PRESS","RELEASE","INTERVAL","STOPPING","PAUSED","DONE","ERROR"};
  return names[uint8_t(p)];
}
struct Plan { uint32_t intervalMs=5000,delayMs=0;uint16_t count=10;bool autofocus=false,focusOnly=false; };
class Capture {
 public:
  Phase phase=Phase::Idle;Plan plan{};uint32_t completed=0,skipped=0;
  uint64_t due=0;bool full=false,half=false;const char* error="";
  bool running() const {return phase!=Phase::Idle && phase!=Phase::Paused && phase!=Phase::Done && phase!=Phase::Error;}
  bool start(uint64_t now,const Plan& p) {
    if(running() || pending_!=Action::None || p.intervalMs<1000 || p.intervalMs>86400000 || p.delayMs>3600000 || p.count>9999) return false;
    *this=Capture();plan=p;phase=Phase::Countdown;due=now+p.delayMs;scheduled_=due;return true;
  }
  void stop(){finish_=Phase::Idle;cleanup_=0;phase=Phase::Stopping;}
  void pause(){if(running()){finish_=Phase::Paused;cleanup_=0;phase=Phase::Stopping;}}
  bool resume(uint64_t now){if(phase!=Phase::Paused)return false;phase=Phase::Countdown;due=scheduled_=now;return true;}
  void disconnected(){pending_=Action::None;full=half=false;if(running() || phase==Phase::Paused){phase=Phase::Error;error="Link lost";}}
  Action next(uint64_t now,bool ready,bool shutterBusy=false,bool focusKnown=false,bool focused=false) {
    if(!ready || pending_!=Action::None)return Action::None;
    if(phase==Phase::Stopping) {
      if(cleanup_>=2){phase=finish_;return Action::None;}
      cleanupPending_=true;return issue(cleanup_?Action::HalfUp:Action::FullUp);
    }
    cleanupPending_=false;
    if(phase==Phase::Countdown || phase==Phase::Wait) {
      if(now<due)return Action::None;
      if(shutterBusy) {
        if(now-due>120000){error="Camera busy";finish_=Phase::Error;cleanup_=0;phase=Phase::Stopping;}
        return Action::None;
      }
      if(now>scheduled_+plan.intervalMs) {
        const uint64_t lost=(now-scheduled_)/plan.intervalMs;
        skipped+=uint32_t(lost);scheduled_+=lost*plan.intervalMs;
      }
      return issue(plan.autofocus || plan.focusOnly?Action::HalfDown:Action::FullDown);
    }
    if(phase==Phase::FocusWait) {
      if(focusKnown && !focused && now<focusStarted_+1500)return Action::None;
      if(focusKnown && !focused){error="Focus timeout";finish_=Phase::Error;cleanup_=0;phase=Phase::Stopping;return Action::None;}
      if(now<due && !focused)return Action::None;
      return issue(plan.focusOnly?Action::HalfUp:Action::FullDown);
    }
    if(phase==Phase::Hold && now>=due)return issue(Action::FullUp);
    if(phase==Phase::Release)return issue(Action::HalfUp);
    return Action::None;
  }
  void ack(Action a,bool ok,uint64_t now) {
    if(pending_!=a || a==Action::None)return;
    pending_=Action::None;
    if(ok && a==Action::FullUp)full=false;
    if(ok && a==Action::HalfUp)half=false;
    if(phase==Phase::Stopping) {
      if(cleanupPending_){if(!ok){phase=Phase::Error;error="Release failed";return;}++cleanup_;if(cleanup_==2)phase=finish_;}
      return;
    }
    if(!ok){error="Write failed";finish_=Phase::Error;cleanup_=0;phase=Phase::Stopping;return;}
    switch(a){
      case Action::HalfDown:half=true;phase=Phase::FocusWait;focusStarted_=now;due=now+(plan.focusOnly?1500:800);break;
      case Action::FullDown:full=true;phase=Phase::Hold;due=now+120;break;
      case Action::FullUp:full=false;if(half)phase=Phase::Release;else complete(now);break;
      case Action::HalfUp:half=false;if(plan.focusOnly)phase=Phase::Done;else complete(now);break;
      default:break;
    }
  }
 private:
  Action pending_=Action::None;bool cleanupPending_=false;uint8_t cleanup_=0;
  uint64_t scheduled_=0,focusStarted_=0;Phase finish_=Phase::Idle;
  Action issue(Action a){pending_=a;if(a==Action::FullDown)full=true;if(a==Action::HalfDown)half=true;return a;}
  void complete(uint64_t now) {
    ++completed;
    if(plan.count && completed>=plan.count){phase=Phase::Done;return;}
    scheduled_+=plan.intervalMs;
    if(scheduled_<=now){const uint64_t lost=(now-scheduled_)/plan.intervalMs+1;skipped+=uint32_t(lost);scheduled_+=lost*plan.intervalMs;}
    due=scheduled_;phase=Phase::Wait;
  }
};
}
