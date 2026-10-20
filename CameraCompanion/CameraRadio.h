#pragma once
#include "CameraModel.h"
namespace CameraRadio {
constexpr uint8_t kMaxCameras=6;
enum class Link:uint8_t { Starting,Idle,Scanning,Connecting,Pairing,Discovering,Ready,Disconnecting,Failed,Reconnecting };
enum class Profile:uint8_t { Auto,Legacy,Modern };
enum class CaptureKind:uint8_t { Single,Focus,Interval };
enum class Stage:uint8_t { Idle,Scanning,Connecting,Mtu,Security,Settling,Discovery,Ready };
enum class Event:uint8_t { Scan,ConnectStart,ConnectResult,LinkEstablished,SecurityStart,SecurityResult,Mtu,Error,Disconnected,Ready,MtuStart,PairingAction,PairingReply,DiscoveryStart,PairingComplete };
enum class PairingPrompt:uint8_t { None,Compare,Display };
struct TraceEntry {uint32_t ms=0;Event event=Event::Scan;Stage stage=Stage::Idle;int code=0;};
struct Peer { char name[32]{};uint8_t address[6]{};uint8_t type=0,protocol=0;int8_t rssi=0;bool pairing=false,remote=false; };
struct Snapshot {
  Link link=Link::Starting;Profile profile=Profile::Auto;
  Peer peers[kMaxCameras]{};uint8_t count=0;char name[32]{};char savedName[32]{};
  bool saved=false,encrypted=false,bonded=false,authenticated=false,remote=false,location=false,notifications=false;
  bool sim=false,simFix=true,focusKnown=false,focused=false,shutterKnown=false,shutterBusy=false;
  uint8_t protocol=0;uint16_t mtu=23;int error=0;char message[80]="Starting BLE";
  Stage stage=Stage::Idle,failureStage=Stage::Idle;int lastFailure=0,disconnectReason=0,securityCode=0,smpReason=-1;
  uint32_t attempts=0;TraceEntry trace[12]{};uint8_t traceCount=0;
  PairingPrompt pairingPrompt=PairingPrompt::None;uint32_t pairingCode=0,pairingToken=0;
  Camera::Phase phase=Camera::Phase::Idle;uint32_t triggers=0,skipped=0,gpsWrites=0,rcWrites=0;
  CaptureKind captureKind=CaptureKind::Single;Camera::Plan activePlan{};
  uint64_t due=0,lastNotify=0,lastGps=0;Camera::Fix fix{};
};
enum class Command:uint8_t { Scan,Connect,Saved,Disconnect,Forget,Single,Focus,Start,Pause,Resume,SetSim,SetFix,SetProfile,PairingReply };
bool begin();
bool request(Command command,uint8_t value=0,const Camera::Plan& plan=Camera::Plan());
bool confirmPairing(bool accept,uint32_t token);
void stop(); // Out-of-band: cannot be blocked by a full ordinary command queue.
Snapshot snapshot();
const char* linkName(Link state);
const char* profileName(Profile profile);
const char* stageName(Stage stage);
const char* eventName(Event event);
}
