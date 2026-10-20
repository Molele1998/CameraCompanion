#pragma once
#include "CameraUiLogic.h"
#include "ChineseFont.h"
#include "AppIcons.h"
#include "StatusIcons.h"
#include <stdio.h>
namespace CameraUi {
constexpr uint16_t rgb(int r,int g,int b){return uint16_t(((r>>3)<<11)|((g>>2)<<5)|(b>>3));}
constexpr uint16_t kBg=0,kInk=rgb(245,245,247),kDim=rgb(153,153,160),kPanel=rgb(28,28,30);
constexpr uint16_t kBlue=rgb(45,124,246),kGreen=rgb(48,209,132),kAmber=rgb(255,176,64),kRed=rgb(255,94,100);
inline void txt(Arduino_GFX& g,int x,int y,const char* s,uint16_t color=kInk,Cjk::Face face=Cjk::Face::Small,uint16_t bg=kBg,int limit=156){
  char out[160]{};size_t n=0;const char* p=s;
  while(*p){const char* prev=p;Cjk::decode(p);const size_t len=size_t(p-prev);if(n+len>=sizeof(out)-4)break;memcpy(out+n,prev,len);out[n+len]=0;
    if(Cjk::width(out,face)>limit){out[n]=0;while(n && Cjk::width(out,face)+Cjk::width("...",face)>limit){--n;while(n && (uint8_t(out[n])&0xc0)==0x80)--n;out[n]=0;}strcat(out,"...");break;}n+=len;
  }
  Cjk::draw(g,x,y,out,color,bg,face);
}
inline void center(Arduino_GFX& g,int y,const char* s,uint16_t color=kInk,Cjk::Face face=Cjk::Face::Body,uint16_t bg=kPanel){Cjk::draw(g,(172-Cjk::width(s,face))/2,y,s,color,bg,face);}
inline const char* linkText(CameraRadio::Link s){using L=CameraRadio::Link;switch(s){case L::Starting:return "正在启动";case L::Idle:return "尚未连接";case L::Scanning:return "正在扫描";case L::Connecting:return "正在连接";case L::Pairing:return "请在相机确认";case L::Discovering:return "读取相机功能";case L::Ready:return "相机已连接";case L::Disconnecting:return "正在断开";case L::Failed:return "连接未成功";case L::Reconnecting:return "等待相机开机";}return "";}
inline const char* phaseText(Camera::Phase p){using P=Camera::Phase;switch(p){case P::Idle:return "待机";case P::Countdown:return "准备拍摄";case P::FocusWait:return "正在对焦";case P::Hold:return "触发快门";case P::Release:return "释放快门";case P::Wait:return "等待下一张";case P::Stopping:return "正在停止";case P::Paused:return "已暂停";case P::Done:return "计划完成";case P::Error:return "拍摄已停止";}return "";}
// Actual ink bounds, rather than font cell bounds, share y=7..21 (15px).
// Latin at 20px and Chinese at 16px have matching cap/ideograph heights here.
inline void statusBar(Arduino_GFX& g,const View& v){char b[16];const bool voltage=v.batteryMv>=2500 && v.batteryMv<=4500;
  StatusIcons::draw(g,false,kStatusItems[0].x,7,kDim);
  if(voltage)snprintf(b,sizeof b,"%.1fV",v.batteryMv/1000.0);else snprintf(b,sizeof b,"--V");
  Cjk::draw(g,kStatusItems[1].x+(kStatusItems[1].w-Cjk::width(b,Cjk::Face::Body))/2,4,b,kInk,kBg,Cjk::Face::Body);
  const char* label=v.radio.sim?"模拟":"GPS";const auto gpsFace=v.radio.sim?Cjk::Face::Small:Cjk::Face::Body;
  const uint16_t color=v.radio.sim?(v.radio.simFix?kAmber:kRed):kDim;
  Cjk::draw(g,kStatusItems[2].x+(kStatusItems[2].w-Cjk::width(label,gpsFace))/2,v.radio.sim?6:4,label,color,kBg,gpsFace);
  const auto link=v.radio.link;const uint16_t cam=link==CameraRadio::Link::Ready?kGreen:linking(v)?kAmber:kDim;
  StatusIcons::draw(g,true,kStatusItems[3].x,7,cam);
}
inline const char* screenTitle(const View& v){if(v.confirm!=Confirm::None)return "操作确认";
  if(v.module==0){switch(v.page){case Sony::Shoot:return "拍摄";case Sony::Connection:return "连接相机";case Sony::Help:return "配对说明";case Sony::Results:return "选择相机";default:return "索尼相机";}}
  if(v.module==2){switch(v.page){case Timer::Parameters:return "拍摄参数";case Timer::Interval:return "拍摄间隔";case Timer::CountMode:case Timer::Number:return "拍摄张数";case Timer::Extras:return "更多参数";case Timer::Delay:return "启动延时";case Timer::Autofocus:return "每张对焦";case Timer::Run:return "拍摄状态";default:return "间隔拍摄";}}
  if(v.module==3){switch(v.page){case Gps::Details:return "定位详情";case Gps::Test:return "信号测试";case Gps::Time:return "时间信息";default:return "GPS设置";}}
  if(v.module==4){switch(v.page){case Settings::Camera:return "索尼连接选项";case Settings::Profiles:return "定位兼容方式";case Settings::Forget:return "配对管理";case Settings::Info:return "设备信息";case Settings::Diagnostics:return "诊断信息";default:return "设置";}}
  return title(v.module);
}
inline void duration(char* out,size_t n,uint32_t ms){if(ms && ms%3600000==0)snprintf(out,n,"%lu小时",(unsigned long)(ms/3600000));else if(ms && ms%60000==0)snprintf(out,n,"%lu分钟",(unsigned long)(ms/60000));else snprintf(out,n,"%lu秒",(unsigned long)(ms/1000));}
inline void menuSubtitle(const View& v,uint8_t i,char* out,size_t n){out[0]=0;
  if(v.module==0)snprintf(out,n,"%s",i==0?"单张照片与对焦":i==1?(intervalEngaged(v.radio)?"已有拍摄计划进行中":"倒计时与间隔"):linkText(v.radio.link));
  else if(v.module==2){if(i==0)duration(out,n,v.plan.intervalMs);else if(i==1){if(v.plan.count)snprintf(out,n,"%u张",v.plan.count);else snprintf(out,n,"持续，手动停止");}else snprintf(out,n,"延时与每张对焦");}
  else if(v.module==4)snprintf(out,n,"%s",uint8_t(v.radio.profile)==i?"当前选项":configurable(v)?"点按选择":"请先断开相机");
}
inline void footer(Arduino_GFX& g,const View& v){g.fillRoundRect(8,288,156,32,10,kPanel);g.drawLine(24,299,19,304,kDim);g.drawLine(19,304,24,309,kDim);
  const char* s=backLabel(v);txt(g,36,291,s,kInk,Cjk::Face::Small,kPanel,120);
}
inline void desktop(Arduino_GFX& g,const View& v){g.fillScreen(kBg);statusBar(g,v);
  for(uint8_t slot=0;slot<2;++slot){const uint8_t app=v.homeOffset+slot;if(app>=kAppCount)break;const auto& box=kAppSlots[slot];const bool pressed=v.pressed==app;
    if(pressed)g.fillRoundRect(20,box.y,132,112,18,kPanel);
    AppIcons::draw(g,kApps[app].icon,54,box.y+6,pressed?kPanel:kBg);center(g,box.y+79,kApps[app].name,kInk,Cjk::Face::Body,pressed?kPanel:kBg);
    if(kApps[app].module==0 && showRun(v))g.fillCircle(135,box.y+14,4,kGreen);
  }
  g.fillRoundRect(167,65,2,185,1,kPanel);g.fillRoundRect(167,65+int(v.homeOffset)*103/(kAppCount-2),2,82,1,rgb(95,95,102));
  if(showRun(v)){g.fillRoundRect(8,288,156,32,10,kPanel);const auto phase=v.radio.phase;
    const char* label=v.awaitingStart?"准备拍摄 · 查看":phase==Camera::Phase::Paused?"已暂停 · 查看":phase==Camera::Phase::Done?"已完成 · 查看":phase==Camera::Phase::Error?"拍摄停止 · 查看":"拍摄中 · 查看";
    center(g,291,label,phase==Camera::Phase::Error?kAmber:kGreen,Cjk::Face::Small,kPanel);
  }else center(g,292,"上下滑动",kDim,Cjk::Face::Small,kBg);
}
inline const char* errorHint(const CameraRadio::Snapshot& r){switch(r.error){case -2:return "请先断开相机";case -20:return "检查蓝牙遥控设置";case -24:return "检查机身后重试";case -30:case -31:return "确认相机开机并配对";case -32:return "请手动重新连接";default:return r.error?"检查相机连接状态":"";}}
inline void card(Arduino_GFX& g,const View& v){const auto& r=v.radio;char h[96]{},line[96]{},hint[96]{};uint16_t color=kInk;
  if(v.confirm==Confirm::Forget){snprintf(h,sizeof h,"忘记此相机？");snprintf(line,sizeof line,"%s",r.savedName);snprintf(hint,sizeof hint,"仅删除这台相机");color=kAmber;}
  else if(v.confirm==Confirm::Disconnect){snprintf(h,sizeof h,"断开连接？");snprintf(line,sizeof line,"将停止当前拍摄");snprintf(hint,sizeof hint,"之后需重新连接");color=kAmber;}
  else if(v.module==0){switch(v.page){
    case Sony::Shoot:
      snprintf(h,sizeof h,"%s",captureEngaged(r)?phaseText(r.phase):linkText(r.link));color=ready(v)?kGreen:kInk;
      snprintf(line,sizeof line,"%s",r.error?errorHint(r):ready(v)?(r.remote?"普通单张拍摄":"检查蓝牙遥控设置"):r.saved?"已保存相机配对":"先连接后即可拍摄");
      snprintf(hint,sizeof hint,"点此管理连接");break;
    case Sony::Connection:
      snprintf(h,sizeof h,"%s",linkText(r.link));snprintf(line,sizeof line,"%s",r.link==CameraRadio::Link::Pairing?"确认机身配对提示":r.saved?r.savedName:"先在机身打开配对");
      snprintf(hint,sizeof hint,"%s",r.error?errorHint(r):r.link==CameraRadio::Link::Reconnecting?"会自动尝试重连":v.connectForPlan?"连接后返回计划":"连接后进入拍摄");break;
    case Sony::Help:snprintf(h,sizeof h,"先在机身操作");snprintf(line,sizeof line,"蓝牙开启 > 配对");snprintf(hint,sizeof hint,"再点下方开始扫描");break;
    case Sony::Results:
      if(r.count && v.peer<r.count){snprintf(h,sizeof h,"%s",r.peers[v.peer].name);snprintf(line,sizeof line,"第 %u / %u 台相机",v.peer+1,r.count);snprintf(hint,sizeof hint,"信号 %d dBm",r.peers[v.peer].rssi);}
      else{snprintf(h,sizeof h,"未找到相机");snprintf(line,sizeof line,"确认机身开启配对");snprintf(hint,sizeof hint,"手机请先断开连接");}break;
  }}
  else if(v.module==2){if(v.page==Timer::Home || v.page==Timer::Run){const bool run=v.page==Timer::Run;const auto& p=run && !v.awaitingStart?r.activePlan:v.plan;char time[24];duration(time,sizeof time,p.intervalMs);
      if(run){snprintf(h,sizeof h,"%s",v.awaitingStart?"准备开始":r.captureKind!=CameraRadio::CaptureKind::Interval?"没有运行计划":phaseText(r.phase));color=r.phase==Camera::Phase::Error?kAmber:kGreen;
        if(p.count)snprintf(line,sizeof line,"已发 %lu / %u 次",(unsigned long)r.triggers,p.count);else snprintf(line,sizeof line,"已发 %lu 次 · 持续",(unsigned long)r.triggers);
        if(r.phase==Camera::Phase::Wait || r.phase==Camera::Phase::Countdown)snprintf(hint,sizeof hint,"下张 %lu 秒后",(unsigned long)(r.due>v.nowMs?(r.due-v.nowMs+999)/1000:0));
        else snprintf(hint,sizeof hint,"%s",r.phase==Camera::Phase::Error?errorHint(r):r.phase==Camera::Phase::Paused?"继续后恢复计划":r.phase==Camera::Phase::Done?"请核对机内照片":"停止键随时可用");
      }else{snprintf(h,sizeof h,"间隔 %s",time);if(p.count)snprintf(line,sizeof line,"目标 %u 张",p.count);else snprintf(line,sizeof line,"持续拍摄，手动停止");
        char delay[24];duration(delay,sizeof delay,p.delayMs);snprintf(hint,sizeof hint,"延时%s · 对焦%s",delay,p.autofocus?"开":"关");}
    }else if(v.page==Timer::CountMode){snprintf(h,sizeof h,"选择张数模式");if(v.plan.count)snprintf(line,sizeof line,"当前 %u 张",v.plan.count);else snprintf(line,sizeof line,"当前为持续拍摄");snprintf(hint,sizeof hint,"持续模式需手动停止");}
    else if(v.page==Timer::Extras){snprintf(h,sizeof h,"用于下次计划");char delay[24];duration(delay,sizeof delay,v.plan.delayMs);snprintf(line,sizeof line,"启动延时 %s",delay);snprintf(hint,sizeof hint,"每张对焦：%s",v.plan.autofocus?"开启":"关闭");}
    else if(v.page==Timer::Autofocus){snprintf(h,sizeof h,"选择对焦方式");snprintf(line,sizeof line,"当前：%s",v.plan.autofocus?"每张重新对焦":"保持当前对焦");snprintf(hint,sizeof hint,"点选后返回上一级");}
  }
  else if(v.module==3){if(v.page==Gps::Home){snprintf(h,sizeof h,"%s",r.sim?"模拟定位开启":"定位发送关闭");color=r.sim?kAmber:kInk;
      snprintf(line,sizeof line,"%s",!r.sim?"仅用于坐标测试":!ready(v)?"等待相机连接":!r.location?"定位服务未就绪":!r.simFix?"模拟信号已丢失":"模拟轨迹运行中");
      if(r.sim && r.gpsWrites)snprintf(hint,sizeof hint,"已确认写入 %lu 次",(unsigned long)r.gpsWrites);else snprintf(hint,sizeof hint,"尚未接入真实GPS");
    }else if(v.page==Gps::Details){snprintf(h,sizeof h,"模拟坐标");snprintf(line,sizeof line,"%.6f",r.fix.lat);snprintf(hint,sizeof hint,"%.6f",r.fix.lon);color=kAmber;}
    else if(v.page==Gps::Test){snprintf(h,sizeof h,"%s",r.simFix?"模拟信号正常":"模拟信号丢失");snprintf(line,sizeof line,"仅测试停止新写入");snprintf(hint,sizeof hint,"%s",r.sim?"机身可能保留旧坐标":"先开启模拟定位");color=kAmber;}
    else{const auto d=Camera::utc(r.fix.epoch);snprintf(h,sizeof h,"测试 UTC 时间");snprintf(line,sizeof line,"%04u-%02u-%02u",d.year,d.month,d.day);snprintf(hint,sizeof hint,"%02u:%02u:%02u UTC",d.hour,d.minute,d.second);color=kAmber;}
  }
  else if(v.module==4){switch(v.page){
    case Settings::Home:snprintf(h,sizeof h,"通用设置");snprintf(line,sizeof line,"连接与设备信息");snprintf(hint,sizeof hint,"配对管理在连接选项");break;
    case Settings::Camera:snprintf(h,sizeof h,"索尼连接设置");snprintf(line,sizeof line,"当前 %s",CameraRadio::profileName(r.profile));snprintf(hint,sizeof hint,"%s",configurable(v)?"点选下方设置":"修改前请断开相机");break;
    case Settings::Forget:snprintf(h,sizeof h,"%s",r.saved?"已保存的相机":"没有保存相机");snprintf(line,sizeof line,"%s",r.saved?r.savedName:"首次连接后会保存");snprintf(hint,sizeof hint,"%s",configurable(v)?"忘记操作需要确认":"请先断开连接");break;
    case Settings::Info:snprintf(h,sizeof h,"相机伴侣 0.2.2");snprintf(line,sizeof line,"空闲内存 %lu KB",(unsigned long)(v.heap/1024));snprintf(hint,sizeof hint,"ESP32-C6 · 172x320");break;
    case Settings::Diagnostics:snprintf(h,sizeof h,"错误 %d",r.error);snprintf(line,sizeof line,"%s",r.error && r.failureStage==CameraRadio::Stage::Security?"配对阶段中断":errorHint(r));
      if(r.disconnectReason)snprintf(hint,sizeof hint,"上次断开 %d",r.disconnectReason);else snprintf(hint,sizeof hint,"USB status 查看详情");break;
  }}
  else{snprintf(h,sizeof h,"功能尚未开放");snprintf(line,sizeof line,"后续加入相机支持");snprintf(hint,sizeof hint,"当前无法连接相机");}
  g.fillRoundRect(kCard.x,kCard.y,kCard.w,kCard.h,14,kPanel);
  txt(g,18,65,h,color,Cjk::Face::Body,kPanel,136);txt(g,18,91,line,kInk,Cjk::Face::Small,kPanel,136);
  const bool showNotice=v.confirm==Confirm::None && v.notice[0] && v.nowMs<v.noticeUntil;
  txt(g,18,113,showNotice?v.notice:hint,showNotice?kAmber:kDim,Cjk::Face::Small,kPanel,136);
  if(cardLink(v)){g.drawLine(154,117,159,122,kDim);g.drawLine(159,122,154,127,kDim);}
}
inline void editor(Arduino_GFX& g,const View& v){g.fillRoundRect(kCard.x,kCard.y,kCard.w,kCard.h,14,kPanel);char b[24];const char* unit="秒";uint32_t value=0;
  if(v.page==Timer::Number){value=v.plan.count;unit="张";}else{const uint32_t ms=v.page==Timer::Interval?v.plan.intervalMs:v.plan.delayMs;
    if(ms && ms%3600000==0){value=ms/3600000;unit="小时";}else if(ms && ms%60000==0){value=ms/60000;unit="分钟";}else value=ms/1000;}
  txt(g,18,65,"用于下次拍摄计划",kDim,Cjk::Face::Small,kPanel,136);snprintf(b,sizeof b,"%lu",(unsigned long)value);
  const int w=Cjk::width(b,Cjk::Face::Large),u=Cjk::width(unit,Cjk::Face::Small),x=(172-w-u-6)/2;
  Cjk::draw(g,x,89,b,kInk,kPanel,Cjk::Face::Large);Cjk::draw(g,x+w+6,100,unit,kDim,kPanel,Cjk::Face::Small);
}
inline void renderPairing(Arduino_GFX& g,const View& v){g.fillScreen(kBg);statusBar(g,v);txt(g,10,32,"相机配对",kInk,Cjk::Face::Title,kBg,154);
  g.fillRoundRect(kCard.x,kCard.y,kCard.w,kCard.h,14,kPanel);
  center(g,66,v.radio.pairingPrompt==CameraRadio::PairingPrompt::Compare?"请核对双方数字":"请在相机输入",kDim,Cjk::Face::Small,kPanel);
  char code[8];snprintf(code,sizeof code,"%06lu",(unsigned long)(v.radio.pairingCode%1000000));center(g,90,code,kInk,Cjk::Face::Large,kPanel);
  for(uint8_t i=0;i<2;++i){const auto b=buttonInfo(v,i);const auto box=kButtons[i];const uint16_t bg=b.danger?rgb(44,20,23):b.enabled?kBlue:kPanel;
    g.fillRoundRect(box.x,box.y,box.w,box.h,14,bg);center(g,box.y+17,b.text,b.danger?kRed:b.enabled?kInk:kDim,Cjk::Face::Body,bg);}
  footer(g,v);
}
inline void render(Arduino_GFX& g,const View& v){if(pairingPrompt(v)){renderPairing(g,v);return;}if(v.home){desktop(g,v);return;}g.fillScreen(kBg);statusBar(g,v);txt(g,10,32,screenTitle(v),kInk,Cjk::Face::Title,kBg,154);
  if(menu(v)){for(uint8_t i=0;i<3;++i){const auto info=buttonInfo(v,i);const auto box=buttonRect(v,i);const uint16_t bg=v.pressed==i && info.enabled?rgb(48,48,54):kPanel;
      g.fillRoundRect(box.x,box.y,box.w,box.h,14,bg);txt(g,18,box.y+4,info.text,info.enabled?kInk:kDim,Cjk::Face::Body,bg,132);
      char subtitle[64];menuSubtitle(v,i,subtitle,sizeof subtitle);txt(g,18,box.y+35,subtitle,kDim,Cjk::Face::Small,bg,132);
      if(info.enabled){g.drawLine(151,box.y+17,156,box.y+22,kDim);g.drawLine(156,box.y+22,151,box.y+27,kDim);}
    }}
  else{if(v.confirm==Confirm::None && v.module==2 && (v.page==Timer::Interval || v.page==Timer::Number || v.page==Timer::Delay))editor(g,v);else card(g,v);
    for(uint8_t i=0;i<2;++i){const auto info=buttonInfo(v,i);const auto box=buttonRect(v,i);uint16_t bg=!info.enabled?kPanel:info.danger?rgb(44,20,23):i==0?kBlue:kPanel;
      const uint16_t ink=!info.enabled?kDim:info.danger?kRed:kInk;if(v.pressed==i && info.enabled)bg=info.danger?rgb(72,26,31):i==0?rgb(31,84,166):rgb(53,53,57);
      g.fillRoundRect(box.x,box.y,box.w,box.h,14,bg);center(g,box.y+17,info.text,ink,Cjk::Face::Body,bg);
    }}
  footer(g,v);
}
}
