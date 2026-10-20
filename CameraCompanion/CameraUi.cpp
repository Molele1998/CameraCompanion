#include "CameraUi.h"
#include "CameraScene.h"
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
bool displayOK=false,touchDown=false,bootDown=false;uint8_t band=8;
uint32_t lastFrame=0,touchStart=0,bootSince=0;int startX=0,startY=0,lastX=0,lastY=0,maxX=0,maxY=0,startHit=-1;
uint8_t touchModule=0,touchPage=0,touchPeerIndex=0;CameraRadio::Peer touchPeer{};
bool touchHome=false,blockedTouch=false;uint8_t touchOffset=0;Confirm touchConfirm=Confirm::None;Action touchAction=Action::None;
uint32_t touchPairingToken=0;
uint32_t navigationGuardUntil=0,batterySampleAt=0,batteryFiltered=0;
char serialLine[96]{};uint8_t serialLength=0;bool overflow=false;
void notice(const char* text){snprintf(view.notice,sizeof view.notice,"%s",text);view.noticeUntil=uint64_t(millis())+1800;}
bool send(CameraRadio::Command c,uint8_t n=0){if(!CameraRadio::request(c,n,view.plan)){notice("未发送，请重试");return false;}
  view.pendingUntil=uint64_t(millis())+500;notice("正在处理");return true;}
void go(uint8_t module,uint8_t page){view.home=false;view.module=module;view.page=page;view.confirm=Confirm::None;view.pressed=-1;navigationGuardUntil=millis()+220;}
void home(){view.home=true;view.confirm=Confirm::None;view.pressed=-1;navigationGuardUntil=millis()+220;}
void back(){if(view.confirm!=Confirm::None){view.confirm=Confirm::None;navigationGuardUntil=millis()+220;return;}
  if(view.module==2){switch(view.page){
    case Timer::Home:go(0,Sony::Home);break;
    case Timer::Parameters:case Timer::Run:go(2,Timer::Home);break;
    case Timer::Number:go(2,Timer::CountMode);break;
    case Timer::Delay:case Timer::Autofocus:go(2,Timer::Extras);break;
    default:go(2,Timer::Parameters);break;}return;}
  if(!view.page){home();return;}
  if(view.module==0)go(0,view.page==Sony::Help || view.page==Sony::Results?Sony::Connection:Sony::Home);
  else if(view.module==3)go(3,view.page==Gps::Details?Gps::Home:Gps::Details);
  else if(view.module==4)go(4,view.page==Settings::Profiles || view.page==Settings::Forget?Settings::Camera:view.page==Settings::Diagnostics?Settings::Info:Settings::Home);
  else home();
}
void stopCapture(){CameraRadio::stop();view.awaitingStart=false;view.pendingUntil=uint64_t(millis())+250;notice("正在停止");}
void activate(Action a){using C=CameraRadio::Command;using A=Action;
  switch(a){
    case A::PairingAccept:case A::PairingReject:
      if(CameraRadio::confirmPairing(a==A::PairingAccept,view.radio.pairingToken))view.pendingUntil=uint64_t(millis())+500;
      else notice("未发送，请重试");break;
    case A::Back:back();break;case A::CancelConfirm:view.confirm=Confirm::None;navigationGuardUntil=millis()+220;break;
    case A::SonyHome:go(0,Sony::Home);break;case A::ShootPage:go(0,Sony::Shoot);break;
    case A::Connection:view.connectForPlan=view.module==2;go(0,Sony::Connection);break;
    case A::Shoot:send(C::Single);break;case A::Focus:send(C::Focus);break;
    case A::Scan:if(send(C::Scan)){view.peer=0;go(0,Sony::Connection);}break;
    case A::Saved:if(send(C::Saved))go(0,Sony::Connection);break;
    case A::CancelConnection:send(C::Disconnect);break;
    case A::Help:go(0,Sony::Help);break;case A::ConnectSelected:send(C::Connect,view.peer);break;
    case A::NextCamera:if(view.radio.count){view.peer=(view.peer+1)%view.radio.count;navigationGuardUntil=millis()+220;}break;
    case A::Disconnect:if(captureEngaged(view.radio)){view.confirm=Confirm::Disconnect;navigationGuardUntil=millis()+220;}else send(C::Disconnect);break;
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
  if(view.home){if(g==Gesture::Up && view.homeOffset+2<kAppCount)++view.homeOffset;else if(g==Gesture::Down && view.homeOffset)--view.homeOffset;navigationGuardUntil=millis()+220;return;}
  if(view.confirm!=Confirm::None){if(g==Gesture::Right || g==Gesture::Down)back();return;}
  if(view.page==0 && view.module!=2 && (g==Gesture::Left || g==Gesture::Right)){
    int index=-1;for(uint8_t i=0;i<kAppCount;++i)if(kApps[i].module==view.module)index=i;
    const int next=index+(g==Gesture::Left?1:-1);if(next<0)home();else if(next<kAppCount)go(kApps[next].module,0);return;}
  if(g==Gesture::Right){back();return;}
  if(view.module==0 && view.page==Sony::Results){if(g==Gesture::Up && view.peer+1<view.radio.count)++view.peer;else if(g==Gesture::Down && view.peer)--view.peer;navigationGuardUntil=millis()+220;return;}
  if(g==Gesture::Down){back();return;}
  if(g==Gesture::Up){
    if(view.module==0 && view.page==Sony::Connection)go(0,Sony::Help);
    else if(view.module==3 && view.page==Gps::Home)go(3,Gps::Details);
    else if(view.module==3 && view.page==Gps::Details)go(3,Gps::Time);
    else if(view.module==4 && view.page==Settings::Info)go(4,Settings::Diagnostics);
  }
}
void touch(){view.nowMs=millis();const auto t=BoardHW::readTouch();const auto now=millis();
  if(t.down){if(!touchDown){touchDown=true;touchStart=now;blockedTouch=int32_t(now-navigationGuardUntil)<0;startX=lastX=t.x;startY=lastY=t.y;maxX=maxY=0;startHit=hit(view,t.x,t.y);
      touchHome=view.home;touchOffset=view.homeOffset;touchModule=view.module;touchPage=view.page;touchConfirm=view.confirm;touchPeerIndex=view.peer;touchPairingToken=view.radio.pairingToken;touchAction=view.home && !pairingPrompt(view)?Action::None:hitAction(view,startHit);
      if(!view.home && view.module==0 && view.page==Sony::Results && view.peer<view.radio.count)touchPeer=view.radio.peers[view.peer];}
    lastX=t.x;lastY=t.y;maxX=max(maxX,abs(int(t.x)-startX));maxY=max(maxY,abs(int(t.y)-startY));}
  else if(touchDown){touchDown=false;if(blockedTouch)return;const auto g=gesture(lastX-startX,lastY-startY,maxX,maxY,now-touchStart);
    bool same=touchHome==view.home && touchModule==view.module && touchPage==view.page && touchOffset==view.homeOffset && touchConfirm==view.confirm && touchPeerIndex==view.peer && touchPairingToken==view.radio.pairingToken;
    if(!touchHome && touchModule==0 && touchPage==Sony::Results && startHit==0)same=same && view.peer<view.radio.count && !memcmp(touchPeer.address,view.radio.peers[view.peer].address,6) && touchPeer.type==view.radio.peers[view.peer].type;
    const int endHit=hit(view,lastX,lastY);
    if(g==Gesture::Tap && same && endHit==startHit){if(view.home && !pairingPrompt(view) && startHit>=0){if(startHit==kAppCount)go(2,Timer::Run);else{view.connectForPlan=false;go(kApps[startHit].module,0);}}
      else if(touchAction!=Action::None && touchAction==hitAction(view,startHit))activate(touchAction);}
    else if(same)navigate(g);
  }
}
void updateRadio(const CameraRadio::Snapshot& r){const auto previous=view.radio.link;const auto oldToken=view.radio.pairingToken;view.radio=r;
  if(oldToken!=r.pairingToken){navigationGuardUntil=millis()+220;view.pendingUntil=0;view.pressed=-1;}
  if(view.peer>=r.count)view.peer=0;
  if(view.awaitingStart && (captureEngaged(r) || r.error || view.nowMs>=view.pendingUntil+1500))view.awaitingStart=false;
  if(!view.home && view.module==0 && view.confirm==Confirm::None){
    if(view.page==Sony::Connection && previous==CameraRadio::Link::Scanning && r.link==CameraRadio::Link::Idle){view.peer=0;go(0,Sony::Results);}
    if((view.page==Sony::Connection || view.page==Sony::Results || view.page==Sony::Help) && previous!=CameraRadio::Link::Ready && r.link==CameraRadio::Link::Ready){const bool plan=view.connectForPlan;view.connectForPlan=false;go(plan?2:0,plan?uint8_t(Timer::Home):uint8_t(Sony::Shoot));}
  }
}
void status(){const auto r=CameraRadio::snapshot();Serial.printf("%s encrypted=%d bonded=%d authenticated=%d RC=%d GPS=%d SIM=%d profile=%s protocol=%02X mtu=%u\n",CameraRadio::linkName(r.link),r.encrypted,r.bonded,r.authenticated,r.remote,r.location,r.sim,CameraRadio::profileName(r.profile),r.protocol,r.mtu);
  Serial.printf("plan=%s triggers=%lu skipped=%lu rc_acks=%lu gps_acks=%lu code=%d message=%s\n",Camera::phaseName(r.phase),(unsigned long)r.triggers,(unsigned long)r.skipped,(unsigned long)r.rcWrites,(unsigned long)r.gpsWrites,r.error,r.message);
  Serial.printf("version=0.2.2 stage=%s last_failure=%d failure_stage=%s security_code=%d smp_reason=%d disconnect_reason=%d (0x%X) attempts=%lu\n",
    CameraRadio::stageName(r.stage),r.lastFailure,CameraRadio::stageName(r.failureStage),r.securityCode,r.smpReason,r.disconnectReason,unsigned(r.disconnectReason),(unsigned long)r.attempts);
  for(uint8_t i=0;i<r.traceCount;++i){const auto& e=r.trace[i];Serial.printf("%lu ms %s stage=%s code=%d (0x%X)\n",(unsigned long)e.ms,CameraRadio::eventName(e.event),CameraRadio::stageName(e.stage),e.code,unsigned(e.code));}}
void serialCommand(char* s){using C=CameraRadio::Command;char* arg=strchr(s,' ');if(arg)*arg++=0;
  auto number=[&](long lo,long hi,long& out){if(!arg || !*arg)return false;char* end=nullptr;out=strtol(arg,&end,10);return end && !*end && out>=lo && out<=hi;};long n=0;
  if(!strcmp(s,"help"))Serial.println("home status scan connect N saved disconnect shoot focus stop start pause resume\ninterval SEC(1..86400) count N(0..9999) delay SEC(0..3600) af 0|1\nsim 0|1 fix 0|1 profile 0|1|2 forget YES (disconnected only)");
  else if(!strcmp(s,"home"))home();
  else if(!strcmp(s,"status"))status();else if(!strcmp(s,"scan"))send(C::Scan);
  else if(!strcmp(s,"connect") && number(1,6,n))send(C::Connect,uint8_t(n-1));
  else if(!strcmp(s,"saved"))send(C::Saved);else if(!strcmp(s,"disconnect"))send(C::Disconnect);
  else if(!strcmp(s,"shoot"))send(C::Single);else if(!strcmp(s,"focus"))send(C::Focus);
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
  BoardHW::beginI2C();BoardHW::beginTouch();CameraRadio::begin();
  Serial.println("Camera Companion 0.2.2; use help. SIM GPS is OFF by default.");
}
void loop(){view.nowMs=millis();updateRadio(CameraRadio::snapshot());touch();const uint32_t now=millis();
  if(digitalRead(BoardHW::kBootButton)==LOW){if(!bootSince)bootSince=now;if(!bootDown && now-bootSince>=25){bootDown=true;stopCapture();}}
  else{bootSince=0;bootDown=false;}
  for(uint8_t n=0;n<32 && Serial.available();++n){const int ch=Serial.read();if(ch=='\r')continue;
    if(ch=='\n'){if(overflow)Serial.println("Command too long");else if(serialLength){serialLine[serialLength]=0;serialCommand(serialLine);}serialLength=0;overflow=false;}
    else if(ch>=32 && ch<127){if(size_t(serialLength)+1<sizeof serialLine)serialLine[serialLength++]=char(ch);else overflow=true;}}
  if(band>=8 && now-lastFrame>=150){
    if(!batterySampleAt || now-batterySampleAt>=5000){const uint32_t raw=analogReadMilliVolts(BoardHW::kBatteryAdc)*3UL;batteryFiltered=batteryFiltered?(batteryFiltered*3+raw)/4:raw;batterySampleAt=now;}
    view.batteryMv=batteryFiltered;
    view.heap=ESP.getFreeHeap();view.nowMs=uint64_t(esp_timer_get_time()/1000);view.pressed=touchDown && !blockedTouch && maxX<16 && maxY<16 && startHit>=0 && (view.home || hitAction(view,startHit)!=Action::None)?int8_t(startHit):-1;frame=view;band=0;lastFrame=now;}
  if(displayOK && band<8){display.beginBand(band++);render(display,frame);display.presentBand();}
  delay(2);
}
}
