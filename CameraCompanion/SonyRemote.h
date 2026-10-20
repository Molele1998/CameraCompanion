#pragma once
#include <stdint.h>
#include <stddef.h>

namespace Camera {
enum class RemoteKey:uint8_t { None,Record,AfOn,C1,FocusNear,FocusFar,ZoomWide,ZoomTele };
enum class Video:uint8_t { Unknown,Stopped,Recording };
enum class KeyStep:uint8_t { None,Down,Up };
inline bool validKey(RemoteKey key){return key>RemoteKey::None && key<=RemoteKey::ZoomTele;}
// camera-gps-link 3d4aaf5: UP releases the button; it never means "stop movie".
inline size_t remotePayload(RemoteKey key,bool down,uint8_t (&out)[3]){
  if(!validKey(key))return 0;
  static const uint8_t codes[]={0,0x0f,0x15,0x21,0x6b,0x6d,0x47,0x45};
  const bool lens=key>=RemoteKey::FocusNear;
  out[0]=lens?2:1;out[1]=uint8_t(codes[uint8_t(key)]-(down?0:1));out[2]=down?0x50:0;
  return lens?3:2;
}
inline Video videoNotification(const uint8_t* data,size_t n){
  if(!data || n!=3 || data[0]!=2 || data[1]!=0xd5)return Video::Unknown;
  return data[2]==0x20?Video::Recording:data[2]==0?Video::Stopped:Video::Unknown;
}
inline const char* videoName(Video video){return video==Video::Recording?"RECORDING":video==Video::Stopped?"STOPPED":"UNKNOWN";}
inline const char* remoteName(RemoteKey key){
  static const char* names[]={"NONE","RECORD","AF_ON","C1","FOCUS_NEAR","FOCUS_FAR","ZOOM_WIDE","ZOOM_TELE"};
  return validKey(key)?names[uint8_t(key)]:names[0];
}
// Only one button owns FF01 at a time. Even an unsuccessful DOWN may have
// reached the camera, so it is followed by UP. UP failure requires disconnect.
class RemotePress {
 public:
  RemoteKey key=RemoteKey::None;KeyStep pending=KeyStep::None;
  bool held=false,downIssued=false;uint64_t started=0,due=0;
  bool busy()const{return key!=RemoteKey::None;}
  bool start(RemoteKey next,bool hold,uint64_t now){
    if(busy() || !validKey(next) || (hold && next==RemoteKey::Record))return false;
    *this=RemotePress();key=next;held=hold;started=now;return true;
  }
  void release(){releaseWanted_=true;}
  void disconnected(){*this=RemotePress();}
  KeyStep next(uint64_t now){
    if(!busy() || pending!=KeyStep::None)return KeyStep::None;
    if(!downIssued){if(releaseWanted_){disconnected();return KeyStep::None;}downIssued=true;return pending=KeyStep::Down;}
    if(releaseWanted_ || now>=due)return pending=KeyStep::Up;
    return KeyStep::None;
  }
  void ack(KeyStep step,bool ok,uint64_t now){
    if(step==KeyStep::None || step!=pending)return;
    pending=KeyStep::None;
    if(step==KeyStep::Up){if(ok)disconnected();else releaseWanted_=true;return;}
    due=held?started+10000:now+120;
    if(!ok)releaseWanted_=true;
  }
 private:
  bool releaseWanted_=false;
};
}
