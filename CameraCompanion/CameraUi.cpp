#include "CameraUi.h"
#include "CameraScene.h"
#include "CameraAnimation.h"
#ifdef CAMERA_UI_HOST
#include "UiHost.h"
#else
#include "BoardHardware.h"
#include "BufferedDisplay.h"
#include <Arduino.h>
#include <esp_timer.h>
#include <stdlib.h>
#endif
namespace CameraUi {
namespace {
BufferedDisplay display;View view,frame;
View motionFrom;Motion motion,frameMotion;bool frameAnimated=false,needsSettle=false,frameSettles=false;
uint64_t frameAt=0;uint32_t frameCost=0;
bool displayOK=false,touchDown=false,bootDown=false;uint8_t band=8;
uint32_t lastFrame=0,touchStart=0,bootSince=0;int startX=0,startY=0,lastX=0,lastY=0,maxX=0,maxY=0,startHit=-1;
uint8_t touchModule=0,touchPage=0,touchPeerIndex=0;CameraRadio::Peer touchPeer{};
bool touchHome=false,blockedTouch=false;uint8_t touchOffset=0;Confirm touchConfirm=Confirm::None;Action touchAction=Action::None;
uint32_t touchPairingToken=0;
uint32_t touchSession=0,holdToken=0,holdStarted=0;bool holdUsed=false;
uint32_t navigationGuardUntil=0,batterySampleAt=0,batteryFiltered=0;
char serialLine[96]{};uint8_t serialLength=0;bool overflow=false;
void notice(const char* text){snprintf(view.notice,sizeof view.notice,"%s",text);view.noticeUntil=uint64_t(millis())+1800;}
bool send(CameraRadio::Command c,uint8_t n=0){if(!CameraRadio::request(c,n,view.plan)){notice("未发送，请重试");return false;}
  view.pendingUntil=uint64_t(millis())+500;notice("正在处理");return true;}
void releaseHeld(){if(holdToken)CameraRadio::releaseRemote(holdToken);holdToken=0;}
void remoteTap(Camera::RemoteKey key){if(CameraRadio::pressRemote(key)){view.pendingUntil=uint64_t(millis())+500;notice("按键已发送");}else notice("未发送，请重试");}
void transition(const View& from,Gesture direction,bool bounce=false){motionFrom=from;motion.start(millis(),direction,bounce);band=8;navigationGuardUntil=millis()+(bounce?180:220);}
void go(uint8_t module,uint8_t page,Gesture direction=Gesture::Left){const View from=view;releaseHeld();view.home=false;view.module=module;view.page=page;view.confirm=Confirm::None;view.pressed=-1;navigationGuardUntil=millis()+220;
  if(module==0 && (page==Sony::Shoot || page==Sony::Video || page==Sony::Keys || page==Sony::ManualFocus || page==Sony::Zoom))view.sonyReturn=page;transition(from,direction);}
void home(Gesture direction=Gesture::Right){const View from=view;releaseHeld();view.home=true;view.confirm=Confirm::None;view.pressed=-1;transition(from,direction);}
void back(Gesture direction=Gesture::Right){if(view.confirm!=Confirm::None){view.confirm=Confirm::None;navigationGuardUntil=millis()+220;return;}
  if(sonyPageIndex(view)>=0){home(direction);return;}
  if(view.module==2){switch(view.page){
    case Timer::Home:go(0,Sony::Shoot,direction);break;
    case Timer::Parameters:case Timer::Run:go(2,Timer::Home,direction);break;
    case Timer::Number:go(2,Timer::CountMode,direction);break;
    case Timer::Delay:case Timer::Autofocus:go(2,Timer::Extras,direction);break;
    default:go(2,Timer::Parameters,direction);break;}return;}
  if(!view.page){home(direction);return;}
  if(view.module==0)go(0,Sony::Connection,direction);
  else if(view.module==3)go(3,view.page==Gps::Details || view.page==Gps::Backup?Gps::Home:Gps::Details,direction);
  else if(view.module==4)go(4,view.page==Settings::Profiles || view.page==Settings::Forget?Settings::Camera:view.page==Settings::Diagnostics?Settings::Info:Settings::Home,direction);
  else home(direction);
}
void stopCapture(){releaseHeld();CameraRadio::stop();view.awaitingStart=false;view.pendingUntil=uint64_t(millis())+250;notice("正在停止");}
void activate(Action a){using C=CameraRadio::Command;using A=Action;
  switch(a){
    case A::PairingAccept:case A::PairingReject:
      if(CameraRadio::confirmPairing(a==A::PairingAccept,view.radio.pairingToken))view.pendingUntil=uint64_t(millis())+500;
      else notice("未发送，请重试");break;
    case A::Back:back();break;case A::CancelConfirm:view.confirm=Confirm::None;navigationGuardUntil=millis()+220;break;
    case A::SonyHome:case A::ShootPage:go(0,Sony::Shoot);break;
    case A::Connection:view.connectForPlan=view.module==2;
      if(view.module==0 && cardLink(view))view.sonyReturn=view.page;
      else if(!view.connectForPlan && view.module!=0)view.sonyReturn=Sony::Shoot;
      go(0,Sony::Connection);break;
    case A::Shoot:send(C::Single);break;case A::Focus:send(C::Focus);break;
    case A::VideoPage:go(0,Sony::Video);break;case A::KeysPage:go(0,Sony::Keys);break;
    case A::ReturnControl:go(0,view.sonyReturn);break;
    case A::ManualFocusPage:go(0,Sony::ManualFocus);break;case A::ZoomPage:go(0,Sony::Zoom);break;
    case A::RecordUnknown:case A::RecordStart:case A::RecordStop:
      if(CameraRadio::requestRecord()){view.pendingUntil=uint64_t(millis())+500;notice("等待机身回报");}else notice("未发送，请重试");break;
    case A::AfOn:case A::C1:case A::FocusNear:case A::FocusFar:case A::ZoomWide:case A::ZoomTele:remoteTap(actionKey(a));break;
    case A::Scan:if(send(C::Scan)){view.peer=0;go(0,Sony::Connection);}break;
    case A::Saved:if(send(C::Saved))go(0,Sony::Connection);break;
    case A::CancelConnection:send(C::Disconnect);break;
    case A::Help:go(0,Sony::Help);break;case A::ConnectSelected:send(C::Connect,view.peer);break;
    case A::NextCamera:if(view.radio.count){view.peer=(view.peer+1)%view.radio.count;navigationGuardUntil=millis()+220;}break;
    case A::Disconnect:if(captureEngaged(view.radio) || showVideo(view) || view.radio.remoteBusy){view.confirm=Confirm::Disconnect;navigationGuardUntil=millis()+220;}else send(C::Disconnect);break;
    case A::ConfirmDisconnect:if(send(C::Disconnect)){view.confirm=Confirm::None;view.awaitingStart=false;navigationGuardUntil=millis()+220;}break;
    case A::PlanHome:go(2,Timer::Home);break;case A::PlanParameters:go(2,Timer::Parameters);break;
    case A::PlanInterval:go(2,Timer::Interval);break;case A::PlanCount:go(2,Timer::CountMode);break;
    case A::PlanExtras:go(2,Timer::Extras);break;case A::PlanDelay:go(2,Timer::Delay);break;
    case A::PlanAf:go(2,Timer::Autofocus);break;case A::PlanNumber:if(!view.plan.count)view.plan.count=10;go(2,Timer::Number);break;
    case A::Start:if(send(C::Start)){view.awaitingStart=true;go(2,Timer::Run);}break;
    case A::Run:go(2,Timer::Run);break;case A::Pause:send(C::Pause);break;case A::Resume:send(C::Resume);break;
    case A::Stop:stopCapture();break;case A::Less:adjust(view,false);break;case A::More:adjust(view,true);break;
    case A::Unlimited:view.plan.count=0;go(2,Timer::Parameters);notice("已设为持续拍摄");break;
    case A::AfYes:case A::AfNo:view.plan.autofocus=a==A::AfYes;go(2,Timer::Extras);break;
    case A::GpsOn:case A::GpsOff:send(C::SetSim,a==A::GpsOn);break;
    case A::GpsDetails:go(3,Gps::Details);break;case A::GpsTest:go(3,Gps::Test);break;case A::GpsTime:go(3,Gps::Time);break;case A::GpsHome:go(3,Gps::Home);break;
    case A::GpxPage:go(3,Gps::Backup);break;
    case A::CheckSd:case A::StartGpx:case A::StopGpx:
      if(Gpx::request(a==A::CheckSd?Gpx::Command::Check:a==A::StartGpx?Gpx::Command::Start:Gpx::Command::Stop)){view.pendingUntil=uint64_t(millis())+500;notice("正在处理SD卡");}else notice("未发送，请重试");break;
    case A::FixOn:case A::FixOff:send(C::SetFix,a==A::FixOn);break;
    case A::SettingsCamera:go(4,Settings::Camera);break;case A::SettingsInfo:go(4,Settings::Info);break;case A::Diagnostics:go(4,Settings::Diagnostics);break;
    case A::Profile:go(4,Settings::Profiles);break;
    case A::ProfileAuto:case A::ProfileLegacy:case A::ProfileModern:send(C::SetProfile,uint8_t(a)-uint8_t(A::ProfileAuto));break;
    case A::Forget:if(view.page==Settings::Forget){view.confirm=Confirm::Forget;navigationGuardUntil=millis()+220;}else go(4,Settings::Forget);break;
    case A::ConfirmForget:if(send(C::Forget)){view.confirm=Confirm::None;navigationGuardUntil=millis()+220;}break;
    default:break;
  }
}
void navigate(Gesture g){if(g==Gesture::None || g==Gesture::Tap)return;
  if(pairingPrompt(view)){if(g==Gesture::Right || g==Gesture::Down)activate(Action::PairingReject);return;}
  if(view.home){const View from=view;bool moved=false;
    if(g==Gesture::Up && view.homeOffset+2<kAppCount){view.homeOffset+=2;moved=true;}else if(g==Gesture::Down && view.homeOffset>=2){view.homeOffset-=2;moved=true;}
    if(g==Gesture::Up || g==Gesture::Down)transition(from,g,!moved);return;}
  if(view.confirm!=Confirm::None){if(g==Gesture::Right || g==Gesture::Down)back();return;}
  if(g==Gesture::Left || g==Gesture::Right){
    const int sony=sonyPageIndex(view);if(sony>=0){const int next=sony+(g==Gesture::Left?1:-1);
      if(next>=0 && next<kSonyPageCount)go(0,kSonyPages[next],g);else transition(view,g,true);return;}
    if(view.module==3){const int next=int(view.page)+(g==Gesture::Left?1:-1);if(next>=0 && next<Gps::Count)go(3,uint8_t(next),g);else transition(view,g,true);return;}
    if(view.module==4){const uint8_t main[]={Settings::Home,Settings::Camera,Settings::Info};
      for(int i=0;i<3;++i)if(view.page==main[i]){const int next=i+(g==Gesture::Left?1:-1);if(next>=0 && next<3)go(4,main[next],g);else transition(view,g,true);return;}}
    if(view.module==5 || view.module==6){transition(view,g,true);return;}
  }
  if(g==Gesture::Right){back();return;}
  if(view.module==0 && view.page==Sony::Results){if(g==Gesture::Up && view.peer+1<view.radio.count)++view.peer;else if(g==Gesture::Down && view.peer)--view.peer;navigationGuardUntil=millis()+220;return;}
  if(g==Gesture::Down){back(g);return;}
  if(g==Gesture::Up){
    if(view.module==0 && view.page==Sony::Connection)go(0,Sony::Help,g);
    else if(view.module==3 && view.page==Gps::Home)go(3,Gps::Details,g);
    else if(view.module==3 && view.page==Gps::Details)go(3,Gps::Time,g);
    else if(view.module==4 && view.page==Settings::Info)go(4,Settings::Diagnostics,g);
    else transition(view,g,true);
  }
}
void touch(){view.nowMs=millis();const auto t=BoardHW::readTouch();const auto now=millis();
  if(t.down){if(!touchDown){touchDown=true;touchStart=now;blockedTouch=int32_t(now-navigationGuardUntil)<0 || (displayOK && ((frameAnimated && band<8) || frameSettles || needsSettle)) || (motion.active && !motion.done(now));startX=lastX=t.x;startY=lastY=t.y;maxX=maxY=0;startHit=hit(view,t.x,t.y);
      releaseHeld();holdUsed=false;touchSession=view.radio.session;
      touchHome=view.home;touchOffset=view.homeOffset;touchModule=view.module;touchPage=view.page;touchConfirm=view.confirm;touchPeerIndex=view.peer;touchPairingToken=view.radio.pairingToken;touchAction=view.home && !pairingPrompt(view)?Action::None:hitAction(view,startHit);
      if(!view.home && view.module==0 && view.page==Sony::Results && view.peer<view.radio.count)touchPeer=view.radio.peers[view.peer];}
    lastX=t.x;lastY=t.y;maxX=max(maxX,abs(int(t.x)-startX));maxY=max(maxY,abs(int(t.y)-startY));
    const bool contextOK=touchHome==view.home && touchModule==view.module && touchPage==view.page && touchConfirm==view.confirm && touchSession==view.radio.session && touchPairingToken==view.radio.pairingToken;
    const bool stationary=maxX<16 && maxY<16 && hit(view,lastX,lastY)==startHit;
    if(holdToken){
      if(blockedTouch || !contextOK || !stationary || !ready(view) || !view.radio.remote || captureEngaged(view.radio) || view.radio.recordPending || now-holdStarted>=10000)releaseHeld();
      else CameraRadio::keepRemoteHeld(holdToken);
    }else if(!holdUsed && !blockedTouch && contextOK && stationary && now-touchStart>=350 && now-touchStart<1000 && actionKey(touchAction)!=Camera::RemoteKey::None && touchAction==hitAction(view,startHit)){
      holdUsed=true;holdToken=CameraRadio::holdRemote(actionKey(touchAction));holdStarted=now;if(!holdToken)notice("未发送，请重试");
    }
  }
  else if(touchDown){touchDown=false;releaseHeld();if(blockedTouch || holdUsed)return;const auto g=gesture(lastX-startX,lastY-startY,maxX,maxY,now-touchStart);
    bool same=touchHome==view.home && touchModule==view.module && touchPage==view.page && touchOffset==view.homeOffset && touchConfirm==view.confirm && touchPeerIndex==view.peer && touchPairingToken==view.radio.pairingToken && touchSession==view.radio.session;
    if(!touchHome && touchModule==0 && touchPage==Sony::Results && startHit==0)same=same && view.peer<view.radio.count && !memcmp(touchPeer.address,view.radio.peers[view.peer].address,6) && touchPeer.type==view.radio.peers[view.peer].type;
    const int endHit=hit(view,lastX,lastY);
    if(g==Gesture::Tap && same && endHit==startHit){if(view.home && !pairingPrompt(view) && startHit>=0){if(startHit==kAppCount){if(showRun(view))go(2,Timer::Run);else go(0,Sony::Video);}else{view.connectForPlan=false;go(kApps[startHit].module,0);}}
      else if(touchAction!=Action::None && touchAction==hitAction(view,startHit))activate(touchAction);}
    else if(same)navigate(g);
  }
}
void updateRadio(const CameraRadio::Snapshot& r){const auto previous=view.radio.link;const auto oldToken=view.radio.pairingToken;const auto oldSession=view.radio.session;view.radio=r;
  if(oldSession!=r.session || r.link!=CameraRadio::Link::Ready || !r.remote){releaseHeld();if(touchDown && oldSession!=r.session)blockedTouch=true;}
  if(oldToken!=r.pairingToken){navigationGuardUntil=millis()+220;view.pendingUntil=0;view.pressed=-1;}
  if(oldToken!=r.pairingToken){motion.active=false;frameAnimated=false;needsSettle=true;band=8;}
  if(view.peer>=r.count)view.peer=0;
  if(view.awaitingStart && (captureEngaged(r) || r.error || view.nowMs>=view.pendingUntil+1500))view.awaitingStart=false;
  if(!view.home && view.module==0 && view.confirm==Confirm::None){
    if(view.page==Sony::Connection && previous==CameraRadio::Link::Scanning && r.link==CameraRadio::Link::Idle){view.peer=0;go(0,Sony::Results);}
    if((view.page==Sony::Connection || view.page==Sony::Results || view.page==Sony::Help) && previous!=CameraRadio::Link::Ready && r.link==CameraRadio::Link::Ready){const bool plan=view.connectForPlan;view.connectForPlan=false;go(plan?2:0,plan?uint8_t(Timer::Home):view.sonyReturn);}
  }
}
void status(){const auto r=CameraRadio::snapshot();Serial.printf("%s encrypted=%d bonded=%d authenticated=%d RC=%d GPS=%d SIM=%d profile=%s protocol=%02X mtu=%u\n",CameraRadio::linkName(r.link),r.encrypted,r.bonded,r.authenticated,r.remote,r.location,r.sim,CameraRadio::profileName(r.profile),r.protocol,r.mtu);
  Serial.printf("plan=%s triggers=%lu skipped=%lu rc_acks=%lu gps_acks=%lu code=%d message=%s\n",Camera::phaseName(r.phase),(unsigned long)r.triggers,(unsigned long)r.skipped,(unsigned long)r.rcWrites,(unsigned long)r.gpsWrites,r.error,r.message);
  Serial.printf("video=%s record_pending=%d notify=%d remote_key=%s held=%d busy=%d last_video_ms=%llu\n",Camera::videoName(r.video),r.recordPending,r.notifications,Camera::remoteName(r.remoteKey),r.remoteHeld,r.remoteBusy,(unsigned long long)r.lastVideoNotify);
  const auto gps=Gpx::snapshot();Serial.printf("gpx_mounted=%d recording=%d busy=%d points=%lu error=%d file=%s frame_ms=%lu\n",gps.mounted,gps.recording,gps.busy,(unsigned long)gps.points,gps.error,gps.file,(unsigned long)frameCost);
  Serial.printf("version=%s stage=%s last_failure=%d failure_stage=%s security_code=%d smp_reason=%d disconnect_reason=%d (0x%X) attempts=%lu\n",
    CameraRadio::kVersion,CameraRadio::stageName(r.stage),r.lastFailure,CameraRadio::stageName(r.failureStage),r.securityCode,r.smpReason,r.disconnectReason,unsigned(r.disconnectReason),(unsigned long)r.attempts);
  for(uint8_t i=0;i<r.traceCount;++i){const auto& e=r.trace[i];Serial.printf("%lu ms %s stage=%s code=%d (0x%X)\n",(unsigned long)e.ms,CameraRadio::eventName(e.event),CameraRadio::stageName(e.stage),e.code,unsigned(e.code));}}
void serialCommand(char* s){using C=CameraRadio::Command;char* arg=strchr(s,' ');if(arg)*arg++=0;
  auto number=[&](long lo,long hi,long& out){if(!arg || !*arg)return false;char* end=nullptr;out=strtol(arg,&end,10);return end && !*end && out>=lo && out<=hi;};long n=0;
  if(!strcmp(s,"help"))Serial.println("home status scan connect N saved disconnect shoot focus stop start pause resume\nrecord (REC toggle, not an unconditional start) afon c1 near far wide tele (single pulse)\ninterval SEC(1..86400) count N(0..9999) delay SEC(0..3600) af 0|1\nsim 0|1 fix 0|1 profile 0|1|2 forget YES (disconnected only)");
  else if(!strcmp(s,"home"))home();
  else if(!strcmp(s,"status"))status();else if(!strcmp(s,"scan"))send(C::Scan);
  else if(!strcmp(s,"connect") && number(1,6,n))send(C::Connect,uint8_t(n-1));
  else if(!strcmp(s,"saved"))send(C::Saved);else if(!strcmp(s,"disconnect"))send(C::Disconnect);
  else if(!strcmp(s,"shoot"))send(C::Single);else if(!strcmp(s,"focus"))send(C::Focus);
  else if(!strcmp(s,"record"))activate(Action::RecordUnknown);
  else if(!strcmp(s,"afon"))remoteTap(Camera::RemoteKey::AfOn);else if(!strcmp(s,"c1"))remoteTap(Camera::RemoteKey::C1);
  else if(!strcmp(s,"near"))remoteTap(Camera::RemoteKey::FocusNear);else if(!strcmp(s,"far"))remoteTap(Camera::RemoteKey::FocusFar);
  else if(!strcmp(s,"wide"))remoteTap(Camera::RemoteKey::ZoomWide);else if(!strcmp(s,"tele"))remoteTap(Camera::RemoteKey::ZoomTele);
  else if(!strcmp(s,"stop"))CameraRadio::stop();else if(!strcmp(s,"start"))send(C::Start);
  else if(!strcmp(s,"pause"))send(C::Pause);else if(!strcmp(s,"resume"))send(C::Resume);
  else if(!strcmp(s,"interval") && number(1,86400,n))view.plan.intervalMs=uint32_t(n)*1000;
  else if(!strcmp(s,"count") && number(0,9999,n))view.plan.count=uint16_t(n);
  else if(!strcmp(s,"delay") && number(0,3600,n))view.plan.delayMs=uint32_t(n)*1000;
  else if(!strcmp(s,"af") && number(0,1,n))view.plan.autofocus=n!=0;
  else if(!strcmp(s,"sim") && number(0,1,n))send(C::SetSim,uint8_t(n));
  else if(!strcmp(s,"fix") && number(0,1,n))send(C::SetFix,uint8_t(n));
  else if(!strcmp(s,"profile") && number(0,2,n))send(C::SetProfile,uint8_t(n));
  else if(!strcmp(s,"forget") && arg && !strcmp(arg,"YES"))send(C::Forget);
  else Serial.println("Invalid command; use help");
  Serial.println("Command parsed; status shows asynchronous result");
}
}
void begin(){Serial.begin(115200);pinMode(BoardHW::kBootButton,INPUT_PULLUP);
  analogReadResolution(12);analogSetPinAttenuation(BoardHW::kBatteryAdc,ADC_11db);
  displayOK=BoardHW::beginDisplay();if(displayOK){display.attach(BoardHW::display());BoardHW::setBrightness(65);}
  BoardHW::beginI2C();BoardHW::beginTouch();CameraRadio::begin();Gpx::begin();
  Serial.printf("Camera Companion %s; use help. SIM GPS is OFF by default.\n",CameraRadio::kVersion);
}
void loop(){view.nowMs=millis();updateRadio(CameraRadio::snapshot());view.gpx=Gpx::snapshot();Gpx::offer(view.radio.fix,view.radio.sim&&view.radio.simFix);touch();const uint32_t now=millis();
  if(digitalRead(BoardHW::kBootButton)==LOW){if(!bootSince)bootSince=now;if(!bootDown && now-bootSince>=25){bootDown=true;stopCapture();}}
  else{bootSince=0;bootDown=false;}
  for(uint8_t n=0;n<32 && Serial.available();++n){const int ch=Serial.read();if(ch=='\r')continue;
    if(ch=='\n'){if(overflow)Serial.println("Command too long");else if(serialLength){serialLine[serialLength]=0;serialCommand(serialLine);}serialLength=0;overflow=false;}
    else if(ch>=32 && ch<127){if(size_t(serialLength)+1<sizeof serialLine)serialLine[serialLength++]=char(ch);else overflow=true;}}
  if(band>=8 && now-lastFrame>=uint32_t(motion.active||needsSettle?24:150)){
    if(!batterySampleAt || now-batterySampleAt>=5000){const uint32_t raw=analogReadMilliVolts(BoardHW::kBatteryAdc)*3UL;batteryFiltered=batteryFiltered?(batteryFiltered*3+raw)/4:raw;batterySampleAt=now;}
    view.batteryMv=batteryFiltered;
    view.heap=ESP.getFreeHeap();view.nowMs=uint64_t(esp_timer_get_time()/1000);view.pressed=touchDown && !blockedTouch && maxX<16 && maxY<16 && startHit>=0 && (view.home || holdToken || hitAction(view,startHit)!=Action::None)?int8_t(startHit):-1;frame=view;band=0;lastFrame=now;
    frameAt=view.nowMs;frameMotion=motion;frameAnimated=motion.active;frameSettles=needsSettle;needsSettle=false;}
  if(displayOK && band<8 && Gpx::lockDisplay()){
    display.beginBand(band);if(frameAnimated)renderMotion(display,frame,motionFrom,frameMotion,frameAt,band*40);
    else {Cjk::clipTop=band*40;Cjk::clipBottom=band*40+40;render(display,frame);Cjk::clipTop=0;Cjk::clipBottom=320;}
    display.presentBand();Gpx::unlockDisplay();++band;
    if(band>=8){frameCost=uint32_t(uint64_t(esp_timer_get_time()/1000)-frameAt);
      if(frameAnimated && (frameMotion.done(frameAt) || frameCost>90)){motion.active=false;needsSettle=!frameMotion.done(frameAt);lastFrame=now-150;}
      frameAnimated=frameSettles=false;}
  }
  delay(2);
}
}
