#pragma once
#include "RadioHost.h"
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#endif
#include <cassert>
inline std::string gpxHostRoot;inline bool gpxHostFailSync=false;
inline bool hostExists(const std::string& path){struct stat info{};return stat(path.c_str(),&info)==0;}
inline bool hostMkdir(const std::string& path){
#ifdef _WIN32
  return _mkdir(path.c_str())==0;
#else
  return mkdir(path.c_str(),0700)==0;
#endif
}
constexpr int HIGH=1,CARD_NONE=0,pdPASS=1;constexpr uint32_t portMAX_DELAY=0xffffffff;
inline int SPI=0;inline void digitalWrite(int,int){}
namespace BoardHW {constexpr int kLcdCs=14,kSdCs=4;}
struct HostSd {
  bool inserted=false,mounted=false;int mountCalls=0,endCalls=0;
  bool begin(int,int&,int,const char* root,int,bool format){assert(!format && gpxHostRoot==root);++mountCalls;return mounted=inserted;}
  void end(){mounted=false;++endCalls;}int cardType(){return mounted?1:CARD_NONE;}
  bool exists(const char* path){return hostExists(gpxHostRoot+path);}
  bool mkdir(const char* path){return hostMkdir(gpxHostRoot+path);}
};inline HostSd SD;
struct HostMutex {bool owned=false;};using SemaphoreHandle_t=HostMutex*;
inline SemaphoreHandle_t xSemaphoreCreateMutex(){return new HostMutex;}
inline int xSemaphoreTake(SemaphoreHandle_t m,uint32_t){if(m->owned)return 0;m->owned=true;return pdTRUE;}
inline void xSemaphoreGive(SemaphoreHandle_t m){assert(m->owned);m->owned=false;}
inline int xTaskCreate(void(*)(void*),const char*,uint32_t,void*,int,void*){return pdPASS;}
inline int pdMS_TO_TICKS(int n){return n;}
inline int xQueueOverwrite(HostQueue* q,const void* data){assert(q->capacity==1);q->data.clear();return xQueueSend(q,data,0);}
