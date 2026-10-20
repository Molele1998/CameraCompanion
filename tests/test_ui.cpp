#define CAMERA_UI_HOST 1
#include "../CameraCompanion/CameraUi.cpp"
#include <cassert>
using namespace CameraUi;
using C=CameraRadio::Command;
void press(int x,int y){BoardHW::sample={true,uint16_t(x),uint16_t(y)};touch();}
void release(){uiNow+=40;BoardHW::sample.down=false;touch();}
void tap(int x,int y){uiNow+=300;press(x,y);release();}
void reset(){view=View();touchDown=false;blockedTouch=false;navigationGuardUntil=0;uiNow=1000;CameraRadio::sent.clear();CameraRadio::acceptRequests=true;view.radio.link=CameraRadio::Link::Idle;}
void setReady(){view.radio.link=CameraRadio::Link::Ready;view.radio.remote=true;view.radio.phase=Camera::Phase::Idle;}
int main(){
  assert(kAppCount==5 && kApps[0].module==0 && kApps[1].module==5 && kApps[2].module==6 && kApps[3].module==4 && kApps[4].module==3);
  reset();tap(86,90);assert(!view.home && view.module==0 && view.page==Sony::Home && menu(view));
  tap(86,90);assert(view.page==Sony::Shoot && CameraRadio::sent.empty());
  assert(buttonInfo(view,0).action==Action::Scan);tap(86,174);assert(CameraRadio::sent.back().command==C::Scan && view.page==Sony::Connection);
  view.radio.link=CameraRadio::Link::Scanning;auto r=view.radio;r.link=CameraRadio::Link::Idle;r.count=1;updateRadio(r);assert(view.page==Sony::Results);
  r.link=CameraRadio::Link::Ready;r.remote=true;updateRadio(r);assert(view.page==Sony::Shoot);
  uiNow+=600;tap(86,174);assert(CameraRadio::sent.back().command==C::Single);
  const auto count=CameraRadio::sent.size();press(86,174);release();assert(CameraRadio::sent.size()==count); // Immediate repeated shutter is suppressed.
  uiNow+=600;tap(86,246);assert(CameraRadio::sent.back().command==C::Focus);
  CameraRadio::sent.clear();uiNow+=600;press(86,174);uiNow+=200;press(86,90);release();assert(CameraRadio::sent.empty());
  uiNow+=300;press(86,174);uiNow+=2000;release();assert(CameraRadio::sent.empty());
  uiNow+=300;press(40,174);uiNow+=200;press(120,174);release();assert(view.page==Sony::Home && CameraRadio::sent.empty());
  tap(86,160);assert(view.module==2 && view.page==Timer::Home);tap(86,246);assert(view.page==Timer::Parameters && menu(view));
  tap(86,90);assert(view.page==Timer::Interval);view.plan.intervalMs=1000;tap(86,174);assert(view.plan.intervalMs==1000);
  view.plan.intervalMs=86400000;tap(86,246);assert(view.plan.intervalMs==86400000);tap(86,174);assert(view.plan.intervalMs==43200000);
  tap(86,303);assert(view.page==Timer::Parameters);tap(86,160);assert(view.page==Timer::CountMode);
  tap(86,246);assert(view.plan.count==0 && view.page==Timer::Parameters);tap(86,160);tap(86,174);assert(view.page==Timer::Number && view.plan.count==10);
  view.plan.count=1;tap(86,174);assert(view.plan.count==1);view.plan.count=9999;tap(86,246);assert(view.plan.count==9999);
  tap(86,303);assert(view.page==Timer::CountMode);tap(86,303);assert(view.page==Timer::Parameters);
  tap(86,240);assert(view.page==Timer::Extras);tap(86,246);assert(view.page==Timer::Autofocus);tap(86,174);assert(view.plan.autofocus && view.page==Timer::Extras);
  tap(86,303);tap(86,303);assert(view.page==Timer::Home);setReady();tap(86,174);assert(view.page==Timer::Run && view.awaitingStart && CameraRadio::sent.back().command==C::Start);
  tap(86,246);assert(!view.awaitingStart && CameraRadio::sent.back().value==255); // Stop stays available while start is pending.
  reset();setReady();view.home=false;view.module=2;view.page=Timer::Run;view.radio.captureKind=CameraRadio::CaptureKind::Interval;view.radio.phase=Camera::Phase::Wait;
  press(86,174);view.radio.phase=Camera::Phase::Paused;release();assert(CameraRadio::sent.empty());tap(86,174);assert(CameraRadio::sent.back().command==C::Resume);
  view.home=true;view.radio.activePlan.count=10;view.plan.count=999;tap(86,303);assert(view.module==2 && view.page==Timer::Run && view.radio.activePlan.count==10);
  view.radio.phase=Camera::Phase::Done;assert(buttonInfo(view,0).action==Action::PlanHome && !buttonInfo(view,1).enabled);tap(86,174);assert(view.page==Timer::Home);
  reset();setReady();view.home=false;view.module=0;view.page=Sony::Connection;view.radio.phase=Camera::Phase::Wait;view.radio.captureKind=CameraRadio::CaptureKind::Interval;
  tap(86,246);assert(view.confirm==Confirm::Disconnect && CameraRadio::sent.empty());tap(86,246);assert(view.confirm==Confirm::None && CameraRadio::sent.empty());
  tap(86,246);tap(86,174);assert(CameraRadio::sent.back().command==C::Disconnect);
  reset();view.home=false;view.module=0;view.page=Sony::Results;view.radio.count=1;view.radio.peers[0].address[0]=1;
  press(86,174);view.radio.peers[0].address[0]=2;release();assert(CameraRadio::sent.empty());
  view.page=Sony::Shoot;view.radio.saved=true;press(86,174);setReady();release();assert(CameraRadio::sent.empty()); // Label/meaning changed from Connect to Shoot.
  reset();view.home=false;view.module=4;view.page=Settings::Forget;view.radio.saved=true;tap(86,174);assert(view.confirm==Confirm::Forget && CameraRadio::sent.empty());
  tap(86,303);assert(view.confirm==Confirm::None && CameraRadio::sent.empty());tap(86,174);tap(86,174);assert(CameraRadio::sent.back().command==C::Forget);
  reset();setReady();view.home=false;view.module=4;view.page=Settings::Profiles;for(int i=0;i<3;++i)assert(!buttonInfo(view,i).enabled);tap(86,160);assert(CameraRadio::sent.empty());
  reset();view.home=false;view.module=2;view.page=Timer::Home;tap(86,174);assert(view.module==0 && view.page==Sony::Connection && view.connectForPlan);
  r=view.radio;r.link=CameraRadio::Link::Ready;r.remote=true;updateRadio(r);assert(view.module==2 && view.page==Timer::Home && CameraRadio::sent.empty());
  CameraRadio::acceptRequests=false;tap(86,174);assert(!view.awaitingStart && view.page==Timer::Home && view.noticeUntil>uiNow);
  reset();view.homeOffset=1;tap(86,90);assert(view.module==5 && !buttonInfo(view,0).enabled);tap(86,303);view.homeOffset=2;tap(86,90);assert(view.module==6 && !buttonInfo(view,0).enabled && CameraRadio::sent.empty());
  reset();press(86,174);r=view.radio;r.link=CameraRadio::Link::Pairing;r.pairingPrompt=CameraRadio::PairingPrompt::Compare;r.pairingCode=12345;r.pairingToken=101;updateRadio(r);release();assert(CameraRadio::sent.empty());
  assert(view.home && hitAction(view,hit(view,86,174))==Action::PairingAccept);
  tap(86,174);assert(CameraRadio::sent.back().command==C::PairingReply && CameraRadio::sent.back().value==1 && CameraRadio::pairingReplyToken==101 && view.home);
  CameraRadio::sent.clear();uiNow+=600;press(86,174);r.pairingToken=102;updateRadio(r);release();assert(CameraRadio::sent.empty());
  tap(86,246);assert(CameraRadio::sent.back().command==C::PairingReply && CameraRadio::sent.back().value==0 && CameraRadio::pairingReplyToken==102);
  CameraRadio::sent.clear();r.pairingPrompt=CameraRadio::PairingPrompt::Display;r.pairingToken=103;updateRadio(r);tap(86,174);assert(CameraRadio::sent.empty());
  tap(86,303);assert(CameraRadio::sent.back().command==C::PairingReply && !CameraRadio::sent.back().value);
  CameraRadio::sent.clear();r.pairingPrompt=CameraRadio::PairingPrompt::None;r.pairingToken=0;press(86,246);updateRadio(r);release();assert(CameraRadio::sent.empty());
  reset();r=view.radio;r.link=CameraRadio::Link::Pairing;r.pairingPrompt=CameraRadio::PairingPrompt::Compare;r.pairingToken=104;updateRadio(r);
  uiNow+=300;press(30,174);uiNow+=100;press(130,174);release();assert(CameraRadio::sent.size()==1 && CameraRadio::sent.back().value==0); // Swipe cancels; never accepts.
  reset();r=view.radio;r.pairingPrompt=CameraRadio::PairingPrompt::Compare;r.pairingToken=105;updateRadio(r);CameraRadio::acceptRequests=false;tap(86,174);assert(CameraRadio::sent.empty() && view.noticeUntil>uiNow);
  std::puts("PASS: actual UI hierarchy, five desktop entries, direct shoot/focus, connection return, bounded editors, explicit continuous mode, run/pause/stop, confirmations, stale actions, debounce and queue failures");
}
