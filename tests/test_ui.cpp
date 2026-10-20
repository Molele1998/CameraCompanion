#define CAMERA_UI_HOST 1
#include "../CameraCompanion/CameraUi.cpp"
#include <cassert>
using namespace CameraUi;
using C=CameraRadio::Command;
void press(int x,int y){BoardHW::sample={true,uint16_t(x),uint16_t(y)};touch();}
void release(){uiNow+=40;BoardHW::sample.down=false;touch();}
void tap(int x,int y){uiNow+=300;press(x,y);release();}
void reset(){view=View();motion=Motion();frameMotion=Motion();frameAnimated=needsSettle=frameSettles=false;band=8;displayOK=false;BoardHW::sample.down=false;touchDown=false;blockedTouch=false;holdToken=0;holdUsed=false;navigationGuardUntil=0;uiNow=1000;CameraRadio::sent.clear();CameraRadio::released.clear();CameraRadio::renewed.clear();CameraRadio::acceptRequests=true;view.radio.link=CameraRadio::Link::Idle;view.gpx.available=true;Gpx::sent.clear();Gpx::acceptRequests=Gpx::displayAvailable=true;}
void setReady(){view.radio.link=CameraRadio::Link::Ready;view.radio.remote=true;view.radio.session=10;view.radio.phase=Camera::Phase::Idle;}
int main(){
  assert(kAppCount==5 && kApps[0].module==0 && kApps[1].module==5 && kApps[2].module==6 && kApps[3].module==4 && kApps[4].module==3);
  reset();tap(86,90);assert(!view.home && view.module==0 && view.page==Sony::Shoot && menu(view) && controls(view)==3);
  assert(buttonInfo(view,1).action==Action::PlanHome && buttonInfo(view,2).action==Action::Focus);
  assert(buttonInfo(view,0).action==Action::Connection);tap(86,90);assert(view.page==Sony::Connection && CameraRadio::sent.empty());
  tap(86,174);assert(CameraRadio::sent.back().command==C::Scan && view.page==Sony::Connection);
  view.radio.link=CameraRadio::Link::Scanning;auto r=view.radio;r.link=CameraRadio::Link::Idle;r.count=1;updateRadio(r);assert(view.page==Sony::Results);
  r.link=CameraRadio::Link::Ready;r.remote=true;updateRadio(r);assert(view.page==Sony::Shoot);
  uiNow+=600;tap(86,90);assert(CameraRadio::sent.back().command==C::Single);
  const auto count=CameraRadio::sent.size();press(86,90);release();assert(CameraRadio::sent.size()==count); // Immediate repeated shutter is suppressed.
  uiNow+=600;tap(86,246);assert(CameraRadio::sent.back().command==C::Focus);
  CameraRadio::sent.clear();uiNow+=600;press(86,90);uiNow+=200;press(86,174);release();assert(CameraRadio::sent.empty());
  go(0,Sony::Shoot);uiNow+=300;press(86,90);uiNow+=2000;release();assert(CameraRadio::sent.empty());
  uiNow+=300;press(40,90);uiNow+=200;press(120,90);release();assert(!view.home && view.page==Sony::Shoot && CameraRadio::sent.empty());
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
  view.page=Sony::Shoot;view.radio.saved=true;press(86,90);setReady();release();assert(CameraRadio::sent.empty()); // Label/meaning changed from Connect to Shoot.
  reset();view.home=false;view.module=4;view.page=Settings::Forget;view.radio.saved=true;tap(86,174);assert(view.confirm==Confirm::Forget && CameraRadio::sent.empty());
  tap(86,303);assert(view.confirm==Confirm::None && CameraRadio::sent.empty());tap(86,174);tap(86,174);assert(CameraRadio::sent.back().command==C::Forget);
  reset();setReady();view.home=false;view.module=4;view.page=Settings::Profiles;for(int i=0;i<3;++i)assert(!buttonInfo(view,i).enabled);tap(86,160);assert(CameraRadio::sent.empty());
  reset();view.home=false;view.module=2;view.page=Timer::Home;tap(86,174);assert(view.module==0 && view.page==Sony::Connection && view.connectForPlan);
  r=view.radio;r.link=CameraRadio::Link::Ready;r.remote=true;updateRadio(r);assert(view.module==2 && view.page==Timer::Home && CameraRadio::sent.empty());
  CameraRadio::acceptRequests=false;tap(86,174);assert(!view.awaitingStart && view.page==Timer::Home && view.noticeUntil>uiNow);
  reset();tap(86,210);assert(view.module==5 && !buttonInfo(view,0).enabled);tap(86,303);navigate(Gesture::Up);assert(view.homeOffset==2);tap(86,90);assert(view.module==6 && !buttonInfo(view,0).enabled && CameraRadio::sent.empty());
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
  // The app opens directly to photo + interval. Horizontal navigation stays
  // within Sony and clamps at both ends; brands are selected on the desktop.
  reset();tap(86,90);assert(sonyPageIndex(view)==0 && view.page==Sony::Shoot);
  for(uint8_t i=1;i<kSonyPageCount;++i){navigate(Gesture::Left);assert(view.module==0 && view.page==kSonyPages[i] && sonyPageIndex(view)==i);}
  for(int i=0;i<20;++i)navigate(Gesture::Left);assert(view.module==0 && view.page==Sony::Connection);
  for(int i=int(kSonyPageCount)-2;i>=0;--i){navigate(Gesture::Right);assert(view.module==0 && view.page==kSonyPages[i]);}
  navigate(Gesture::Right);assert(!view.home && view.page==Sony::Shoot);tap(86,303);assert(view.home);
  reset();go(0,Sony::Shoot);uiNow+=300;press(140,90);uiNow+=100;press(40,90);release();assert(view.page==Sony::Video && CameraRadio::sent.empty()); // Swipe starting on shutter changes function, never shoots.
  reset();go(3,Gps::Home);navigate(Gesture::Right);assert(!view.home && view.module==3 && view.page==Gps::Home);
  for(int i=0;i<20;++i)navigate(Gesture::Left);assert(view.module==3 && view.page==Gps::Time);navigate(Gesture::Right);assert(view.module==3 && view.page==Gps::Test);
  reset();go(4,Settings::Home);navigate(Gesture::Left);assert(view.module==4 && view.page==Settings::Camera);navigate(Gesture::Left);assert(view.page==Settings::Info);
  navigate(Gesture::Left);assert(view.module==4 && view.page==Settings::Info);navigate(Gesture::Right);assert(view.module==4 && view.page==Settings::Camera);
  reset();go(5,0);navigate(Gesture::Left);navigate(Gesture::Right);assert(!view.home && view.module==5);go(6,0);navigate(Gesture::Left);assert(view.module==6);
  reset();setReady();go(0,Sony::Video);assert(buttonInfo(view,0).action==Action::RecordUnknown);
  tap(86,174);assert(CameraRadio::sent.size()==1 && CameraRadio::sent.back().command==C::Record && view.radio.video==Camera::Video::Unknown);
  uiNow+=600;view.radio.recordPending=true;assert(!buttonInfo(view,0).enabled);tap(86,174);assert(CameraRadio::sent.size()==1);
  view.radio.recordPending=false;view.radio.video=Camera::Video::Stopped;uiNow+=600;press(86,174);view.radio.video=Camera::Video::Recording;release();assert(CameraRadio::sent.size()==1);
  tap(86,174);assert(CameraRadio::sent.size()==2 && CameraRadio::sent.back().command==C::Record);
  view.home=true;tap(86,303);assert(view.module==0 && view.page==Sony::Video);
  // Connection detour returns to the selected remote control page.
  reset();go(0,Sony::Video);tap(86,174);assert(view.page==Sony::Connection && view.sonyReturn==Sony::Video);
  r=view.radio;r.link=CameraRadio::Link::Ready;r.remote=true;r.session=10;updateRadio(r);assert(view.page==Sony::Video);
  activate(Action::Connection);tap(86,174);assert(view.page==Sony::Video);
  const uint8_t controlPages[]={Sony::Keys,Sony::ManualFocus,Sony::Zoom};
  const Camera::RemoteKey keys[]={Camera::RemoteKey::AfOn,Camera::RemoteKey::C1,Camera::RemoteKey::FocusNear,Camera::RemoteKey::FocusFar,Camera::RemoteKey::ZoomWide,Camera::RemoteKey::ZoomTele};
  for(int i=0;i<6;++i){reset();setReady();go(0,controlPages[i/2]);tap(86,i%2?246:174);assert(CameraRadio::sent.back().command==C::RemotePulse && CameraRadio::sent.back().value==uint8_t(keys[i]));}
  // Holding begins only after stationary dwell. Releasing, leaving the button,
  // navigating or losing the link all invoke the out-of-band release API.
  reset();setReady();go(0,Sony::ManualFocus);uiNow+=300;press(86,174);uiNow+=349;press(86,174);assert(CameraRadio::sent.empty());
  ++uiNow;press(86,174);assert(holdToken && CameraRadio::sent.size()==1 && CameraRadio::sent.back().command==C::RemoteHold);
  view.radio.remoteBusy=true;view.radio.remoteKey=Camera::RemoteKey::FocusNear;uiNow+=100;press(86,174);assert(!CameraRadio::renewed.empty() && holdToken);
  release();assert(CameraRadio::released.size()==1 && CameraRadio::sent.size()==1); // No extra tap after long press.
  reset();setReady();go(0,Sony::Zoom);uiNow+=300;press(86,174);uiNow+=100;press(86,100);uiNow+=400;press(86,100);release();assert(CameraRadio::sent.empty()); // Swipe before arm never moves lens.
  reset();setReady();go(0,Sony::Zoom);uiNow+=300;press(86,174);uiNow+=400;press(86,174);uiNow+=10;press(86,246);assert(!holdToken && CameraRadio::released.size()==1);release();assert(CameraRadio::sent.size()==1);
  reset();setReady();go(0,Sony::Keys);uiNow+=300;press(86,246);uiNow+=400;press(86,246);home();assert(!holdToken && CameraRadio::released.size()==1);release();assert(CameraRadio::sent.size()==1);
  reset();setReady();go(0,Sony::Zoom);uiNow+=300;press(86,174);uiNow+=400;press(86,174);r=view.radio;r.session=11;r.link=CameraRadio::Link::Failed;updateRadio(r);assert(!holdToken && CameraRadio::released.size()==1);release();assert(CameraRadio::sent.size()==1);
  reset();setReady();go(0,Sony::ManualFocus);uiNow+=300;press(86,174);uiNow+=400;press(86,174);uiNow+=10000;press(86,174);assert(!holdToken && CameraRadio::released.size()==1);release();assert(CameraRadio::sent.size()==1);
  reset();setReady();go(0,Sony::Zoom);CameraRadio::acceptRequests=false;uiNow+=300;press(86,174);uiNow+=400;press(86,174);release();assert(CameraRadio::sent.empty() && !holdToken && view.noticeUntil>uiNow);
  reset();setReady();go(0,Sony::Video);uiNow+=300;press(86,174);r=view.radio;r.session=11;updateRadio(r);release();assert(CameraRadio::sent.empty());
  reset();setReady();go(0,Sony::Shoot);view.radio.video=Camera::Video::Recording;assert(!buttonInfo(view,0).enabled && !buttonInfo(view,2).enabled && buttonInfo(view,1).action==Action::PlanHome);
  go(0,Sony::ManualFocus);assert(buttonInfo(view,0).enabled);view.radio.phase=Camera::Phase::Paused;assert(buttonInfo(view,0).action==Action::None && buttonInfo(view,1).action==Action::Stop);
  reset();navigate(Gesture::Up);assert(view.homeOffset==2 && motion.active && motion.direction==Gesture::Up);navigate(Gesture::Up);assert(view.homeOffset==4);navigate(Gesture::Up);assert(view.homeOffset==4 && motion.bounce);
  navigate(Gesture::Down);assert(view.homeOffset==2);navigate(Gesture::Down);assert(view.homeOffset==0);navigate(Gesture::Down);assert(view.homeOffset==0 && motion.bounce);
  reset();setReady();go(0,Sony::Video);press(86,174);release();assert(CameraRadio::sent.empty()); // A new press during a transition cannot trigger an unseen target.
  reset();go(3,Gps::Backup);assert(!buttonInfo(view,0).enabled);tap(86,174);assert(Gpx::sent.empty());tap(86,246);assert(Gpx::sent.back()==Gpx::Command::Check);
  view.gpx.mounted=true;uiNow+=600;tap(86,174);assert(Gpx::sent.back()==Gpx::Command::Start);view.gpx.recording=true;tap(86,174);assert(Gpx::sent.back()==Gpx::Command::Stop);
  reset();setReady();CameraRadio::state=view.radio;Gpx::state=view.gpx;view.home=false;view.page=Sony::Video;frame=view;displayOK=true;
  motion.start(uiNow,Gesture::Left);frameMotion=motion;frameAnimated=true;frameAt=uiNow-100;band=7;loop();assert(!motion.active && needsSettle); // Slow animation frame settles instead of blocking controls repeatedly.
  std::puts("PASS: actual UI: all remote entries/movie states/connection return + dwell/slide/release/navigation/disconnect/limit/queue + original hierarchy/capture/pairing regressions");
}
