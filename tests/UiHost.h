#pragma once
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstdint>
using std::max;
inline uint32_t uiNow=1000;
inline uint32_t millis(){return uiNow;}
inline int64_t esp_timer_get_time(){return int64_t(uiNow)*1000;}
constexpr int INPUT_PULLUP=1,ADC_11db=3,LOW=0;
inline void pinMode(int,int){}inline int digitalRead(int){return 1;}
inline void analogReadResolution(int){}inline void analogSetPinAttenuation(int,int){}
inline uint32_t analogReadMilliVolts(int){return 1370;}
inline void delay(int n){uiNow+=uint32_t(n);}
struct HostSerial {void begin(int){}void println(const char*){}void printf(const char*,...){}int available(){return 0;}int read(){return 0;}};
inline HostSerial Serial;
struct HostEsp {uint32_t getFreeHeap(){return 123456;}};inline HostEsp ESP;
namespace BoardHW {
constexpr int kBootButton=9,kBatteryAdc=0;
struct TouchSample{bool down;uint16_t x,y;};inline TouchSample sample{false,0,0};
inline TouchSample readTouch(){return sample;}inline bool beginDisplay(){return true;}inline bool beginI2C(){return true;}inline bool beginTouch(){return true;}inline void setBrightness(int){}
inline Arduino_GFX panel;inline Arduino_GFX* display(){return &panel;}
}
class BufferedDisplay:public Arduino_GFX {public:void attach(Arduino_GFX*){}void beginBand(uint8_t){}void presentBand(){}void clearBand(uint16_t=0){}void setViewport(int16_t=0,int16_t=0,int16_t=0,int16_t=320){}};
namespace Gpx {
inline Snapshot state;inline std::vector<Command> sent;inline bool acceptRequests=true,displayAvailable=true;
inline bool begin(){return true;}inline bool request(Command c){if(!acceptRequests)return false;sent.push_back(c);return true;}
inline void offer(const Camera::Fix&,bool){}inline Snapshot snapshot(){return state;}
inline bool lockDisplay(){return displayAvailable;}inline void unlockDisplay(){}
}
namespace CameraRadio {
struct Sent {Command command;uint8_t value;Camera::Plan plan;};inline std::vector<Sent> sent;inline Snapshot state;
inline bool acceptRequests=true;
inline uint32_t nextHold=100;inline std::vector<uint32_t> renewed,released;
inline bool begin(){return true;}
inline bool request(Command c,uint8_t v,const Camera::Plan& p){if(!acceptRequests)return false;sent.push_back({c,v,p});return true;}
inline uint32_t pairingReplyToken=0;
inline bool confirmPairing(bool accept,uint32_t token){if(!acceptRequests)return false;pairingReplyToken=token;sent.push_back({Command::PairingReply,uint8_t(accept),{}});return true;}
inline bool requestRecord(){return request(Command::Record,uint8_t(state.video),{});}
inline bool pressRemote(Camera::RemoteKey key){return request(Command::RemotePulse,uint8_t(key),{});}
inline uint32_t holdRemote(Camera::RemoteKey key){if(!request(Command::RemoteHold,uint8_t(key),{}))return 0;return ++nextHold;}
inline void keepRemoteHeld(uint32_t token){renewed.push_back(token);}
inline void releaseRemote(uint32_t token){released.push_back(token);}
inline void stop(){sent.push_back({Command::Pause,255,{}});}
inline Snapshot snapshot(){return state;}
inline const char* linkName(Link){return "READY";}inline const char* profileName(Profile){return "AUTO";}
inline const char* stageName(Stage){return "SECURITY";}inline const char* eventName(Event){return "DISCONNECT";}
}
