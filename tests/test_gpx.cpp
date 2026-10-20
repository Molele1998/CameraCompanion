#define CAMERA_GPX_HOST 1
#include "../CameraCompanion/GpxLogger.cpp"
#include <cassert>
#include <fstream>
using namespace Gpx;
std::string read(const std::string& path){std::ifstream f(path);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv){assert(argc==2);gpxHostRoot=argv[1];assert(!hostExists(gpxHostRoot));assert(hostMkdir(gpxHostRoot));testMs=10000;
  assert(begin() && snapshot().available && !snapshot().mounted && !snapshot().recording);
  assert(request(Command::Start));cycle();assert(!snapshot().recording && snapshot().error==-80 && !hostExists(gpxHostRoot+"/CameraCompanion"));
  SD.inserted=true;assert(request(Command::Check));cycle();assert(snapshot().mounted && !writer.isOpen());
  assert(lockDisplay());assert(!lockDisplay());unlockDisplay(); // SD and LCD share exactly one owner.
  assert(request(Command::Start));cycle();assert(snapshot().recording && writer.isOpen());const std::string file1=live.file,full1=gpxHostRoot+"/CameraCompanion/GPX/"+file1;
  assert(read(full1).find("SIMULATED GPS")!=std::string::npos && read(full1).find("</gpx>")!=std::string::npos);
  offer(Camera::simulate(testMs),false);cycle();assert(snapshot().points==0);
  offer(Camera::simulate(testMs),true);cycle();assert(snapshot().points==1 && read(full1).find("<trkpt lat=")!=std::string::npos && read(full1).find("2026-10-20T00:00:10Z")!=std::string::npos);
  offer(Camera::simulate(testMs),true);cycle();assert(snapshot().points==1);
  testMs+=500;auto f=Camera::simulate(testMs);f.generatedMs=testMs;offer(f,true);cycle();assert(snapshot().points==1); // Rate limit.
  testMs=12000;f=Camera::simulate(testMs);f.lat=91;offer(f,true);cycle();assert(snapshot().points==1);
  testMs=14000;f=Camera::simulate(testMs-4000);offer(f,true);cycle();assert(snapshot().points==1); // Stale.
  testMs=15000;offer(Camera::simulate(testMs),true);cycle();assert(snapshot().points==2 && read(full1).find("</trkseg><trkseg>")!=std::string::npos);
  const auto count=SD.mountCalls;assert(request(Command::Check));cycle();assert(SD.mountCalls==count); // Never remount an open recording.
  assert(request(Command::Stop));cycle();assert(!snapshot().recording && snapshot().mounted && !writer.isOpen());const std::string saved=read(full1);
  assert(request(Command::Start));cycle();assert(snapshot().recording && live.file!=file1 && read(full1)==saved); // Never overwrite previous tracks.
  testMs+=1000;f=Camera::simulate(testMs);f.lon=179.999999999;offer(f,true);cycle();assert(snapshot().points==1);
  assert(read(gpxHostRoot+"/CameraCompanion/GPX/"+live.file).find("lon=\"-180.0000000\"")!=std::string::npos);
  gpxHostFailSync=true;SD.inserted=false;testMs+=1000;offer(Camera::simulate(testMs),true);cycle();assert(!snapshot().recording && !snapshot().mounted && snapshot().error==-84 && !writer.isOpen());
  gpxHostFailSync=false;assert(request(Command::Check));cycle();assert(!snapshot().mounted && snapshot().error==-80);
  SD.inserted=true;assert(request(Command::Check));cycle();assert(snapshot().mounted && !snapshot().recording); // No automatic restart on reinsertion.
  SD.inserted=false;assert(request(Command::Start));cycle();assert(!snapshot().recording && snapshot().error==-80); // Cached cardType is not accepted for start.
  std::puts("PASS: actual GPX worker + real host files: no-card gate/remount, SIM labels, point freshness/rate/segments, flush/close/failure, shared SPI, no overwrite/restart");
}
