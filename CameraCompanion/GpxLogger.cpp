#include "GpxLogger.h"
#ifdef CAMERA_GPX_HOST
#include "GpxHost.h"
#else
#include "BoardHardware.h"
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <esp_timer.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#endif
#include "GpxWriter.h"
namespace Gpx {
namespace {
struct Sample {Camera::Fix fix;bool available=false;};
Snapshot live,published;portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
QueueHandle_t controls=nullptr,samples=nullptr;SemaphoreHandle_t bus=nullptr;
Writer writer;bool sdActive=false;uint32_t sequence=0;uint64_t lastPointMs=UINT64_MAX,lastOffered=UINT64_MAX;
bool lastAvailable=false;
uint64_t now(){return uint64_t(esp_timer_get_time()/1000);}
const char* mountPoint(){
#ifdef CAMERA_GPX_HOST
  return gpxHostRoot.c_str();
#else
  return "/camera_sd";
#endif
}
void publish(){portENTER_CRITICAL(&mux);published=live;portEXIT_CRITICAL(&mux);}
struct BusLock {BusLock(){xSemaphoreTake(bus,portMAX_DELAY);}~BusLock(){xSemaphoreGive(bus);}};
void failed(int error){writer.close();live.recording=live.mounted=live.busy=false;live.error=error;publish();}
bool mount(){
  live.busy=true;live.mounted=false;publish();
  if(sdActive)SD.end();sdActive=false;
  digitalWrite(BoardHW::kLcdCs,HIGH);
  sdActive=SD.begin(BoardHW::kSdCs,SPI,4000000,mountPoint(),4,false);
  live.mounted=sdActive && SD.cardType()!=CARD_NONE;
  if(!live.mounted){if(sdActive)SD.end();sdActive=false;live.error=-80;}
  else live.error=0;
  live.busy=false;publish();return live.mounted;
}
bool startFile(){
  if(!mount())return false;
  const char* dir="/CameraCompanion/GPX";
  if((!SD.exists("/CameraCompanion") && !SD.mkdir("/CameraCompanion")) || (!SD.exists(dir) && !SD.mkdir(dir))){failed(-81);return false;}
  char path[80],full[180];bool found=false;
  for(uint8_t i=0;i<32;++i){snprintf(live.file,sizeof live.file,"SIM_%06lX.gpx",(unsigned long)((esp_random()+sequence++)&0xffffff));snprintf(path,sizeof path,"%s/%s",dir,live.file);if(!SD.exists(path)){found=true;break;}}
  if(!found){failed(-82);return false;}
  snprintf(full,sizeof full,"%s%s",mountPoint(),path);
  FILE* f=fopen(full,"wb+");if(!f){failed(-83);return false;}
  if(!writer.open(f,true)){failed(-84);return false;}
  live.points=0;lastPointMs=UINT64_MAX;live.recording=true;live.error=0;publish();return true;
}
void cycle(){
  Command command;
  if(xQueueReceive(controls,&command,pdMS_TO_TICKS(20))==pdTRUE){
    if(command==Command::Stop){BusLock lock;live.busy=true;publish();const bool ok=writer.close();live.recording=live.busy=false;if(!ok){live.mounted=false;live.error=-84;}publish();}
    else if(!live.recording){BusLock lock;if(command==Command::Check)mount();else {live.busy=true;publish();startFile();live.busy=false;publish();}}
  }
  Sample sample;
  if(xQueueReceive(samples,&sample,0)!=pdTRUE || !live.recording)return;
  if(!sample.available || !validFix(sample.fix) || sample.fix.generatedMs>now() || now()-sample.fix.generatedMs>3000){writer.gap();return;}
  if(lastPointMs!=UINT64_MAX && (sample.fix.generatedMs<=lastPointMs || sample.fix.generatedMs-lastPointMs<1000))return;
  BusLock lock;
  if(!writer.append(sample.fix)){failed(-84);return;}
  lastPointMs=sample.fix.generatedMs;live.points=writer.points;publish();
}
void task(void*){for(;;)cycle();}
}
bool begin(){
  controls=xQueueCreate(4,sizeof(Command));samples=xQueueCreate(1,sizeof(Sample));bus=xSemaphoreCreateMutex();
  if(!controls || !samples || !bus){live.error=-85;publish();return false;}
  live.available=true;publish();
  if(xTaskCreate(task,"camera_gpx",6144,nullptr,1,nullptr)!=pdPASS){live.available=false;live.error=-85;publish();return false;}
  return true;
}
bool request(Command command){return controls && snapshot().available && command<=Command::Stop && xQueueSend(controls,&command,0)==pdTRUE;}
void offer(const Camera::Fix& fix,bool available){if(!samples || (fix.generatedMs==lastOffered && available==lastAvailable))return;
  lastOffered=fix.generatedMs;lastAvailable=available;const Sample sample{fix,available};xQueueOverwrite(samples,&sample);}
Snapshot snapshot(){portENTER_CRITICAL(&mux);const auto s=published;portEXIT_CRITICAL(&mux);return s;}
bool lockDisplay(){return !bus || xSemaphoreTake(bus,0)==pdTRUE;}
void unlockDisplay(){if(bus)xSemaphoreGive(bus);}
}
