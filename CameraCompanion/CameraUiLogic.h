#pragma once
#include "CameraRadio.h"
#include "GpxLogger.h"
#include <stdlib.h>
namespace CameraUi {
struct Rect {int x,y,w,h;bool contains(int px,int py)const{return px>=x && py>=y && px<x+w && py<y+h;}};
constexpr Rect kButtons[2]={{8,144,156,64},{8,216,156,64}};
constexpr Rect kMenuRows[3]={{8,64,156,64},{8,136,156,64},{8,208,156,64}};
constexpr Rect kCard={8,64,156,72},kHomeButton={8,288,156,32};
constexpr Rect kAppSlots[2]={{8,44,156,112},{8,168,156,112}};
// Four columns span the line, with 4px outside and 8px between columns.
constexpr Rect kStatusItems[4]={{4,7,24,15},{36,7,50,15},{94,7,42,15},{144,7,24,15}};
struct App {uint8_t module,icon;const char* name;};
constexpr App kApps[]={{0,0,"索尼相机"},{5,4,"大疆相机"},{6,5,"尼康相机"},{4,3,"设置"},{3,2,"GPS设置"}};
constexpr uint8_t kAppCount=sizeof(kApps)/sizeof(kApps[0]);
namespace Sony {enum Page:uint8_t {Shoot,Connection,Help,Results,Video,Keys,ManualFocus,Zoom,Count};}
namespace Timer {enum Page:uint8_t {Home,Parameters,Interval,CountMode,Extras,Delay,Autofocus,Run,Number,Count};}
namespace Gps {enum Page:uint8_t {Home,Backup,Details,Test,Time,Count};}
namespace Settings {enum Page:uint8_t {Home,Camera,Profiles,Forget,Info,Diagnostics,Count};}
enum class Gesture:uint8_t {None,Tap,Left,Right,Up,Down};
enum class Confirm:uint8_t {None,Disconnect,Forget};
enum class Action:uint8_t {
  None,Back,CancelConfirm,SonyHome,ShootPage,Shoot,Focus,Connection,Scan,Saved,CancelConnection,Help,ConnectSelected,NextCamera,Disconnect,ConfirmDisconnect,
  PlanHome,PlanParameters,PlanInterval,PlanCount,PlanExtras,PlanDelay,PlanAf,PlanNumber,Start,Run,Pause,Resume,Stop,Less,More,Unlimited,AfYes,AfNo,
  GpsOn,GpsOff,GpsDetails,GpsTest,GpsTime,FixOn,FixOff,GpsHome,SettingsCamera,SettingsInfo,Diagnostics,Profile,ProfileAuto,ProfileLegacy,ProfileModern,Forget,ConfirmForget,PairingAccept,PairingReject,
  VideoPage,KeysPage,ManualFocusPage,ZoomPage,RecordUnknown,RecordStart,RecordStop,AfOn,C1,FocusNear,FocusFar,ZoomWide,ZoomTele,ReturnControl,GpxPage,CheckSd,StartGpx,StopGpx
};
constexpr uint8_t kSonyPages[]={Sony::Shoot,Sony::Video,Sony::ManualFocus,Sony::Zoom,Sony::Keys,Sony::Connection};
constexpr uint8_t kSonyPageCount=sizeof(kSonyPages)/sizeof(kSonyPages[0]);
struct View {
  uint8_t module=0,page=0,homeOffset=0,peer=0,sonyReturn=Sony::Shoot;Camera::Plan plan{};CameraRadio::Snapshot radio{};
  bool home=true,awaitingStart=false,connectForPlan=false;Confirm confirm=Confirm::None;int8_t pressed=-1;uint32_t heap=0,batteryMv=0;
  Gpx::Snapshot gpx{};
  uint64_t nowMs=0,pendingUntil=0,noticeUntil=0;char notice[80]{};
};
inline bool captureEngaged(const CameraRadio::Snapshot& r){using P=Camera::Phase;return r.phase!=P::Idle && r.phase!=P::Done && r.phase!=P::Error;}
inline bool intervalEngaged(const CameraRadio::Snapshot& r){return r.captureKind==CameraRadio::CaptureKind::Interval && captureEngaged(r);}
inline bool intervalStatus(const CameraRadio::Snapshot& r){return r.captureKind==CameraRadio::CaptureKind::Interval && r.phase!=Camera::Phase::Idle;}
inline bool showRun(const View& v){return v.awaitingStart || intervalStatus(v.radio);}
inline bool showVideo(const View& v){return v.radio.recordPending || v.radio.video==Camera::Video::Recording;}
inline bool ready(const View& v){return v.radio.link==CameraRadio::Link::Ready;}
inline bool linking(const View& v){using L=CameraRadio::Link;const auto s=v.radio.link;return s==L::Starting || s==L::Scanning || s==L::Connecting || s==L::Pairing || s==L::Discovering || s==L::Disconnecting || s==L::Reconnecting;}
inline bool configurable(const View& v){return !ready(v) && !linking(v);}
inline bool pending(const View& v){return v.nowMs<v.pendingUntil;}
inline bool pairingPrompt(const View& v){return v.radio.pairingPrompt!=CameraRadio::PairingPrompt::None;}
inline bool canRemote(const View& v){return ready(v) && v.radio.remote && !captureEngaged(v.radio) && !v.radio.remoteBusy && !v.radio.recordPending;}
inline bool canStart(const View& v){return canRemote(v) && v.radio.video!=Camera::Video::Recording;}
inline Camera::RemoteKey actionKey(Action action){using A=Action;using K=Camera::RemoteKey;
  switch(action){case A::AfOn:return K::AfOn;case A::C1:return K::C1;case A::FocusNear:return K::FocusNear;case A::FocusFar:return K::FocusFar;case A::ZoomWide:return K::ZoomWide;case A::ZoomTele:return K::ZoomTele;default:return K::None;}}
inline Gesture gesture(int dx,int dy,int maxDx,int maxDy,uint32_t duration){const int ax=abs(dx),ay=abs(dy);
  if(duration<1500){if(ax>=45 && ax*10>ay*14)return dx<0?Gesture::Left:Gesture::Right;if(ay>=45 && ay*10>ax*14)return dy<0?Gesture::Up:Gesture::Down;}
  if(maxDx<16 && maxDy<16 && duration<1000)return Gesture::Tap;return Gesture::None;
}
inline uint8_t pages(uint8_t module,uint8_t=0){switch(module){case 0:return Sony::Count;case 2:return Timer::Count;case 3:return Gps::Count;case 4:return Settings::Count;default:return 1;}}
inline const char* title(uint8_t module){if(module==2)return "间隔拍摄";for(const auto& a:kApps)if(a.module==module)return a.name;return "相机伴侣";}
inline int sonyPageIndex(const View& v){if(v.module!=0)return -1;for(uint8_t i=0;i<kSonyPageCount;++i)if(kSonyPages[i]==v.page)return i;return -1;}
inline bool menu(const View& v){return !pairingPrompt(v) && v.confirm==Confirm::None && ((v.module==0 && v.page==Sony::Shoot)||(v.module==2 && v.page==Timer::Parameters)||(v.module==4 && v.page==Settings::Profiles));}
inline const char* backLabel(const View& v){if(pairingPrompt(v))return "取消配对";if(v.confirm!=Confirm::None)return "取消";
  if(sonyPageIndex(v)>=0)return "返回桌面";
  if(v.module==2){if(v.page==Timer::Home)return "返回拍摄";if(v.page==Timer::Parameters || v.page==Timer::Run)return "返回计划";if(v.page==Timer::Number)return "返回张数";if(v.page==Timer::Delay || v.page==Timer::Autofocus)return "返回选项";return "返回参数";}
  if(!v.page)return "返回桌面";if(v.module==0)return "返回连接";
  if(v.module==3)return v.page==Gps::Details || v.page==Gps::Backup?"返回定位":"返回详情";
  if(v.module==4)return v.page==Settings::Profiles || v.page==Settings::Forget?"返回选项":v.page==Settings::Diagnostics?"返回信息":"返回设置";
  return "返回桌面";
}
inline bool requestAction(Action a){switch(a){
  case Action::Shoot:case Action::Focus:case Action::Scan:case Action::Saved:case Action::ConnectSelected:case Action::Disconnect:case Action::ConfirmDisconnect:
  case Action::Start:case Action::Pause:case Action::Resume:case Action::GpsOn:case Action::GpsOff:case Action::FixOn:case Action::FixOff:
  case Action::ProfileAuto:case Action::ProfileLegacy:case Action::ProfileModern:case Action::ConfirmForget:case Action::PairingAccept:case Action::CheckSd:case Action::StartGpx:return true;default:return false;}}
struct Button {Action action;const char* text;bool enabled=true,danger=false;};
inline Button rawButton(const View& v,uint8_t i){using A=Action;const bool first=i==0;const auto& r=v.radio;
  if(pairingPrompt(v))return first?(r.pairingPrompt==CameraRadio::PairingPrompt::Compare?Button{A::PairingAccept,"一致，配对"}:Button{A::None,"等待相机确认",false}):Button{A::PairingReject,"取消配对",true,true};
  if(v.confirm!=Confirm::None){if(!first)return {A::CancelConfirm,"取消"};return v.confirm==Confirm::Forget?Button{A::ConfirmForget,"确认忘记",configurable(v)&&r.saved,true}:Button{A::ConfirmDisconnect,"停止并断开",true,true};}
  switch(v.module){
    case 0:switch(v.page){
      case Sony::Shoot:
        if(i==1)return {intervalEngaged(r)?A::Run:A::PlanHome,"间隔拍摄"};
        if(i==2)return captureEngaged(r) || v.awaitingStart?Button{A::Stop,"停止拍摄",true,true}:Button{A::Focus,"对焦",canStart(v)};
        if(!ready(v))return linking(v)?Button{A::None,"正在连接",false}:Button{A::Connection,"连接相机"};
        return {A::Shoot,"拍照",canStart(v)};
      case Sony::Video:case Sony::Keys:case Sony::ManualFocus:case Sony::Zoom:
        if(!ready(v))return first?(linking(v)?Button{A::None,"正在连接",false}:Button{A::Connection,"先连接相机"}):Button{A::Connection,"连接管理"};
        if(captureEngaged(r))return first?(intervalEngaged(r)?Button{A::Run,"查看拍摄计划"}:Button{A::None,"正在拍摄",false}):Button{A::Stop,"停止拍摄",true,true};
        if(v.page==Sony::Video){if(!first)return {A::Connection,"连接管理"};
          if(r.recordPending)return {A::None,"等待相机确认",false};
          if(r.video==Camera::Video::Recording)return {A::RecordStop,"停止录像",canRemote(v),true};
          if(r.video==Camera::Video::Stopped)return {A::RecordStart,"开始录像",canRemote(v)};
          return {A::RecordUnknown,"发送录像键",canRemote(v)};}
        if(v.page==Sony::Keys)return first?Button{A::AfOn,"AF-ON",canRemote(v)}:Button{A::C1,"C1",canRemote(v)};
        if(v.page==Sony::ManualFocus)return first?Button{A::FocusNear,"焦点移近",canRemote(v)}:Button{A::FocusFar,"焦点移远",canRemote(v)};
        return first?Button{A::ZoomWide,"广角 W",canRemote(v)}:Button{A::ZoomTele,"长焦 T",canRemote(v)};
      case Sony::Connection:
        if(ready(v))return first?Button{A::ReturnControl,"返回操控"}:Button{A::Disconnect,"断开连接",true,true};
        if(linking(v))return first?Button{A::None,r.link==CameraRadio::Link::Pairing?"请在相机确认":r.link==CameraRadio::Link::Scanning?"正在扫描":"等待连接",false}:Button{A::CancelConnection,"取消连接",r.link!=CameraRadio::Link::Starting};
        return first?(r.saved?Button{A::Saved,"连接此相机"}:Button{A::Scan,"扫描相机"}):(r.saved?Button{A::Scan,"配对新相机"}:Button{A::Help,"配对说明"});
      case Sony::Help:return first?Button{A::Scan,"开始扫描",configurable(v)}:Button{A::Back,"返回连接"};
      case Sony::Results:if(!r.count)return first?Button{A::Scan,"重新扫描",configurable(v)}:Button{A::Help,"配对说明"};
        return first?Button{A::ConnectSelected,"连接此相机",configurable(v)&&v.peer<r.count}:r.count>1?Button{A::NextCamera,v.peer+1<r.count?"下一台相机":"第一台相机"}:Button{A::Scan,"重新扫描",configurable(v)};
    }break;
    case 2:switch(v.page){
      case Timer::Home:if(!first)return {A::PlanParameters,"调整参数"};if(intervalEngaged(r))return {A::Run,"查看进行中的"};if(!ready(v))return {A::Connection,"先连接相机"};return {A::Start,"开始拍摄",canStart(v)};
      case Timer::Parameters:return i==0?Button{A::PlanInterval,"拍摄间隔"}:i==1?Button{A::PlanCount,"拍摄张数"}:Button{A::PlanExtras,"延时与对焦"};
      case Timer::Interval:return first?Button{A::Less,v.plan.intervalMs<=1000?"已到最小":"减少",v.plan.intervalMs>1000}:Button{A::More,v.plan.intervalMs>=86400000?"已到最大":"增加",v.plan.intervalMs<86400000};
      case Timer::CountMode:return first?Button{A::PlanNumber,"设置具体张数"}:Button{A::Unlimited,"持续拍摄"};
      case Timer::Number:return first?Button{A::Less,v.plan.count<=1?"已到最小":"减少",v.plan.count>1}:Button{A::More,v.plan.count>=9999?"已到最大":"增加",v.plan.count<9999};
      case Timer::Extras:return first?Button{A::PlanDelay,"启动延时"}:Button{A::PlanAf,"每张对焦"};
      case Timer::Delay:return first?Button{A::Less,!v.plan.delayMs?"已到最小":"减少",v.plan.delayMs>0}:Button{A::More,v.plan.delayMs>=3600000?"已到最大":"增加",v.plan.delayMs<3600000};
      case Timer::Autofocus:return first?Button{A::AfYes,"每张重新对焦"}:Button{A::AfNo,"保持当前对焦"};
      case Timer::Run:
        if(v.awaitingStart)return first?Button{A::None,"正在开始",false}:Button{A::Stop,"停止拍摄",true,true};
        if(r.captureKind!=CameraRadio::CaptureKind::Interval)return first?Button{A::PlanHome,"返回拍摄计划"}:Button{A::None,"暂无拍摄计划",false};
        if(!captureEngaged(r))return first?Button{A::PlanHome,"返回拍摄计划"}:Button{A::None,r.phase==Camera::Phase::Done?"本组已完成":"拍摄已停止",false};
        return first?(r.phase==Camera::Phase::Paused?Button{A::Resume,"继续拍摄",ready(v)&&r.remote&&!r.remoteBusy&&!r.recordPending&&r.video!=Camera::Video::Recording}:Button{A::Pause,"暂停拍摄",r.phase!=Camera::Phase::Stopping}):Button{A::Stop,"停止拍摄",true,true};
    }break;
    case 3:switch(v.page){
      case Gps::Home:return first?(r.sim?Button{A::GpsOff,"关闭发送"}:Button{A::GpsOn,"开启模拟定位"}):Button{A::GpxPage,"GPX备份"};
      case Gps::Backup:
        if(v.gpx.recording)return first?Button{A::StopGpx,"停止并保存",true,true}:Button{A::None,"正在记录轨迹",false};
        return first?Button{A::StartGpx,v.gpx.busy?"正在处理SD卡":"开始GPX备份",v.gpx.available&&v.gpx.mounted&&!v.gpx.busy}:Button{A::CheckSd,"检测SD卡",v.gpx.available&&!v.gpx.busy};
      case Gps::Details:return first?Button{A::GpsTest,"信号测试"}:Button{A::GpsTime,"时间信息"};
      case Gps::Test:return first?(r.simFix?Button{A::FixOff,"模拟失去信号",r.sim}:Button{A::FixOn,"恢复信号",r.sim}):Button{A::GpsHome,"返回定位设置"};
      case Gps::Time:return first?Button{A::GpsHome,"返回定位设置"}:Button{A::GpsTest,"信号测试"};
    }break;
    case 4:switch(v.page){
      case Settings::Home:return first?Button{A::SettingsCamera,"索尼连接选项"}:Button{A::SettingsInfo,"设备信息"};
      case Settings::Camera:return first?Button{A::Profile,"定位兼容方式"}:Button{A::Forget,"配对管理"};
      case Settings::Profiles:{const A actions[]={A::ProfileAuto,A::ProfileLegacy,A::ProfileModern};const char* names[]={"自动识别","旧协议","新协议"};return {actions[i],names[i],configurable(v)&&uint8_t(r.profile)!=i};}
      case Settings::Forget:return first?Button{A::Forget,"忘记此相机",r.saved&&configurable(v),true}:ready(v)||linking(v)?Button{A::Disconnect,"断开连接",true,true}:Button{A::Back,"返回选项"};
      case Settings::Info:return first?Button{A::Diagnostics,"诊断信息"}:Button{A::Back,"返回设置"};
      case Settings::Diagnostics:return first?Button{A::Connection,"连接管理"}:Button{A::Back,"返回信息"};
    }break;
    case 5:case 6:return first?Button{A::None,"功能开发中",false}:Button{A::Back,"返回桌面"};
  }
  return {A::None,"暂不可用",false};
}
inline Button buttonInfo(const View& v,uint8_t i){auto b=rawButton(v,i);const bool remote=actionKey(b.action)!=Camera::RemoteKey::None || b.action==Action::RecordStart || b.action==Action::RecordStop || b.action==Action::RecordUnknown;
  if(pending(v) && (requestAction(b.action) || remote))b.enabled=false;return b;}
inline const char* buttonText(const View& v,uint8_t i){return buttonInfo(v,i).text;}
inline uint8_t controls(const View& v){return menu(v)?3:2;}
inline Rect buttonRect(const View& v,uint8_t i){return menu(v)?kMenuRows[i]:kButtons[i];}
inline bool cardLink(const View& v){return !pairingPrompt(v) && v.confirm==Confirm::None && v.module==0 && (v.page==Sony::Video || v.page==Sony::Keys || v.page==Sony::ManualFocus || v.page==Sony::Zoom);}
inline int homeHit(const View& v,int x,int y){if((showRun(v) || showVideo(v)) && kHomeButton.contains(x,y))return kAppCount;
  for(int i=0;i<2;++i)if(kAppSlots[i].contains(x,y) && v.homeOffset+i<kAppCount)return v.homeOffset+i;return -1;}
inline int hit(const View& v,int x,int y){if(v.home && !pairingPrompt(v))return homeHit(v,x,y);if(kHomeButton.contains(x,y))return 4;
  if(cardLink(v)&&kCard.contains(x,y))return 3;for(uint8_t i=0;i<controls(v);++i)if(buttonRect(v,i).contains(x,y))return i;return -1;}
inline Action hitAction(const View& v,int target){if(target==4)return pairingPrompt(v)?Action::PairingReject:Action::Back;if(target==3 && cardLink(v))return Action::Connection;
  if(target>=0 && target<controls(v)){const auto b=buttonInfo(v,uint8_t(target));return b.enabled?b.action:Action::None;}return Action::None;}
// Sorted presets with clamped ends: no accidental wrap from a maximum to a minimum.
template<size_t N> inline uint32_t stepValue(uint32_t value,const uint32_t (&values)[N],bool more){if(more){for(size_t i=0;i<N;++i)if(values[i]>value)return values[i];return values[N-1];}
  for(size_t i=N;i>0;--i)if(values[i-1]<value)return values[i-1];return values[0];}
inline void adjust(View& v,bool more){
  if(v.page==Timer::Interval){const uint32_t values[]={1000,2000,3000,5000,10000,15000,30000,60000,120000,300000,600000,1800000,3600000,7200000,21600000,43200000,86400000};v.plan.intervalMs=stepValue(v.plan.intervalMs,values,more);}
  else if(v.page==Timer::Delay){const uint32_t values[]={0,2000,5000,10000,30000,60000,300000,600000,1800000,3600000};v.plan.delayMs=stepValue(v.plan.delayMs,values,more);}
  else{const uint32_t values[]={1,5,10,20,30,60,100,300,600,1000,3000,9999};v.plan.count=uint16_t(stepValue(v.plan.count,values,more));}
}
}
