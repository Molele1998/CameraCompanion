#include "CameraModel.h"
#include "SonyRemote.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace Camera;
int main() {
  const uint8_t adv[] = {0x2d,1,3,0,0x64,0,'A','1',0x22,0xc2,0,0x21,0x40,0};
  auto a = parseSony(adv, sizeof adv);
  assert(a.sony && a.protocol == 0x64 && a.pairing && a.remote);
  for (size_t n=0;n<4;n++) assert(!parseSony(adv,n).sony);
  auto fix=simulate(0); auto later=simulate(1000);
  assert(fix.valid && fix.epoch + 1 == later.epoch);
  assert(std::abs(later.lat-fix.lat)<0.00002 && std::abs(later.lon-fix.lon)<0.00002);
  uint8_t data[95];
  assert(locationPayload(fix,true,480,0,data)==95);
  assert(data[0]==0 && data[1]==0x5d && data[5]==3 && data[91]==1 && data[92]==0xe0);
  assert(locationPayload(fix,false,0,0,data)==91 && data[1]==0x59 && data[5]==0);
  fix.lat=-33.1234567;fix.lon=-70.7654321;assert(locationPayload(fix,true,-180,0,data)==95);
  assert(data[11]==uint8_t(uint32_t(int32_t(-331234567))>>24) && data[91]==0xff && data[92]==0x4c);
  fix.lat=91;assert(locationPayload(fix,true,0,0,data)==0);fix.lat=0;
  fix.valid=false; assert(locationPayload(fix,true,0,0,data)==0);
  const auto dt=utc(1792454399); const auto dt2=utc(1792454400);
  assert(dt.hour==23 && dt.minute==59 && dt.second==59 && dt2.hour==0 && dt2.day==dt.day+1);
  Capture c; Plan p; p.count=2;p.intervalMs=5000;p.delayMs=2000;
  assert(c.start(0,p)); assert(c.next(1999,true)==Action::None);
  assert(c.next(2000,true)==Action::FullDown); c.ack(Action::FullDown,true,2040);
  assert(c.next(2100,true)==Action::None);
  assert(c.next(2200,true)==Action::FullUp); c.ack(Action::FullUp,true,2240);
  assert(c.completed==1); assert(c.next(6999,true)==Action::None);
  assert(c.next(7000,true)==Action::FullDown);
  c.stop(); c.ack(Action::FullDown,true,7010);
  assert(c.next(7011,true)==Action::FullUp); c.ack(Action::FullUp,true,7012);
  assert(c.next(7013,true)==Action::HalfUp);c.ack(Action::HalfUp,true,7014);
  assert(!c.running() && c.completed==1);
  assert(c.start(0,p)); c.next(2000,true);c.disconnected();
  assert(c.next(3000,true)==Action::None && c.phase==Phase::Error);
  assert(c.start(0,p));c.next(2000,true);c.pause();c.ack(Action::FullDown,true,2010);
  c.next(2011,true);c.ack(Action::FullUp,true,2012);c.next(2013,true);c.ack(Action::HalfUp,true,2014);
  assert(c.phase==Phase::Paused);assert(c.resume(10000));assert(c.next(10000,true)==Action::FullDown);
  c.ack(Action::FullDown,false,10010);assert(c.next(10020,true)==Action::FullUp);
  Capture af;Plan focus;focus.focusOnly=true;assert(af.start(0,focus));assert(af.next(0,true)==Action::HalfDown);af.ack(Action::HalfDown,true,10);
  assert(af.next(800,true)==Action::None);assert(af.next(1510,true)==Action::HalfUp);af.ack(Action::HalfUp,true,1511);assert(af.completed==0 && af.phase==Phase::Done);
  // Fixed wire fixtures from the reference App, not computed expectations.
  const uint8_t down[][3]={{1,0x0f,0},{1,0x15,0},{1,0x21,0},{2,0x6b,0x50},{2,0x6d,0x50},{2,0x47,0x50},{2,0x45,0x50}};
  const uint8_t up[][3]={{1,0x0e,0},{1,0x14,0},{1,0x20,0},{2,0x6a,0},{2,0x6c,0},{2,0x46,0},{2,0x44,0}};
  for(uint8_t i=0;i<7;++i){uint8_t out[3]{};const auto key=RemoteKey(i+1);const size_t n=i<3?2:3;
    assert(remotePayload(key,true,out)==n && memcmp(out,down[i],n)==0);
    assert(remotePayload(key,false,out)==n && memcmp(out,up[i],n)==0);}
  uint8_t out[3]{};assert(!remotePayload(RemoteKey::None,true,out) && !remotePayload(RemoteKey(255),true,out));
  const uint8_t recording[]={2,0xd5,0x20},stopped[]={2,0xd5,0},invalid[]={2,0xd5,0x21},focusStatus[]={2,0x3f,0x20};
  assert(videoNotification(recording,3)==Video::Recording && videoNotification(stopped,3)==Video::Stopped);
  assert(videoNotification(invalid,3)==Video::Unknown && videoNotification(focusStatus,3)==Video::Unknown && videoNotification(recording,2)==Video::Unknown && videoNotification(nullptr,3)==Video::Unknown);
  RemotePress button;assert(!button.start(RemoteKey::Record,true,0));assert(button.start(RemoteKey::C1,false,0));
  assert(button.next(0)==KeyStep::Down && button.next(100)==KeyStep::None);button.ack(KeyStep::Down,true,100);
  assert(button.next(219)==KeyStep::None && button.next(220)==KeyStep::Up);button.ack(KeyStep::Up,true,221);assert(!button.busy());
  assert(button.start(RemoteKey::FocusFar,true,0));button.release();assert(button.next(0)==KeyStep::None && !button.busy());
  assert(button.start(RemoteKey::ZoomWide,true,0));assert(button.next(0)==KeyStep::Down);button.release();button.ack(KeyStep::Down,true,50);
  assert(button.next(51)==KeyStep::Up);button.ack(KeyStep::Up,true,52);
  assert(button.start(RemoteKey::AfOn,true,0));assert(button.next(0)==KeyStep::Down);button.ack(KeyStep::Down,false,1);
  assert(button.next(2)==KeyStep::Up);button.ack(KeyStep::Up,false,3);assert(button.busy());button.disconnected();assert(!button.busy());
  assert(button.start(RemoteKey::FocusNear,true,0));button.next(0);button.ack(KeyStep::Down,true,1);
  assert(button.next(9999)==KeyStep::None && button.next(10000)==KeyStep::Up);
  std::puts("PASS: protocol, GPS trajectory, clock, capture timing and cancellation");
}
