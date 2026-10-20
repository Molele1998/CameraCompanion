#pragma once
#include "CameraModel.h"
#include <stdio.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
namespace Gpx {
inline bool validFix(const Camera::Fix& f){return f.valid && isfinite(f.lat) && isfinite(f.lon) && isfinite(f.altitude) && fabs(f.lat)<=90 && fabs(f.lon)<=180 && fabs(f.altitude)<=20000 && f.epoch>=1577836800ULL && f.epoch<4102444800ULL;}
// Keep a closing XML tail after every successful point. FAT/power failures
// are still not atomic; fsync cannot guarantee survival of sudden power loss.
class Writer {
 public:
  uint32_t points=0;bool simulated=true;
  bool open(FILE* file,bool simulation){
    if(file_ || !file)return false;file_=file;simulated=simulation;points=0;newSegment_=false;lastGenerated_=0;
    if(setvbuf(file_,nullptr,_IONBF,0)!=0)return false;
    const char* header=simulation?
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<gpx version=\"1.1\" creator=\"Camera Companion\" xmlns=\"http://www.topografix.com/GPX/1/1\"><metadata><name>SIMULATED GPS</name><desc>Synthetic positions and test UTC, not satellite data.</desc></metadata><trk><name>SIMULATED GPS</name><trkseg>\n":
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<gpx version=\"1.1\" creator=\"Camera Companion\" xmlns=\"http://www.topografix.com/GPX/1/1\"><trk><name>GNSS track</name><trkseg>\n";
    if(!write(header))return false;tail_=ftell(file_);return tail_>=0 && write(kTail) && sync();
  }
  bool append(const Camera::Fix& f){
    if(!file_ || !validFix(f) || (points && f.generatedMs<=lastGenerated_))return false;
    const auto d=Camera::utc(f.epoch);char point[256];
    double longitude=round(f.lon*1e7)/1e7;if(longitude>=180)longitude=-180; // GPX longitude excludes +180, including values rounded to it.
    const bool segment=points && (newSegment_ || f.generatedMs-lastGenerated_>10000);
    const int n=snprintf(point,sizeof point,"%s<trkpt lat=\"%.7f\" lon=\"%.7f\"><ele>%.2f</ele><time>%04u-%02u-%02uT%02u:%02u:%02uZ</time></trkpt>\n",segment?"</trkseg><trkseg>\n":"",f.lat,longitude,f.altitude,d.year,d.month,d.day,d.hour,d.minute,d.second);
    if(n<=0 || size_t(n)>=sizeof point || fseek(file_,tail_,SEEK_SET)!=0 || fwrite(point,1,size_t(n),file_)!=size_t(n))return false;
    tail_=ftell(file_);if(tail_<0 || !write(kTail) || !sync())return false;
    ++points;lastGenerated_=f.generatedMs;newSegment_=false;return true;
  }
  void gap(){newSegment_=true;}
  bool close(){if(!file_)return true;const bool ok=sync();const int rc=fclose(file_);file_=nullptr;return ok && rc==0;}
  bool isOpen()const{return file_!=nullptr;}
 private:
  FILE* file_=nullptr;long tail_=0;uint64_t lastGenerated_=0;bool newSegment_=false;
  static constexpr const char* kTail="</trkseg></trk></gpx>\n";
  bool write(const char* s){const size_t n=strlen(s);return fwrite(s,1,n,file_)==n;}
  bool sync(){
#ifdef CAMERA_GPX_HOST
    if(gpxHostFailSync)return false;
#endif
    if(fflush(file_)!=0 || ferror(file_))return false;
#ifdef _WIN32
    return _commit(_fileno(file_))==0;
#else
    return fsync(fileno(file_))==0;
#endif
  }
};
}
