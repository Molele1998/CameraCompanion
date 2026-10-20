#include "CameraRadio.h"
#ifdef CAMERA_HOST_TEST
#include "RadioHost.h"
#else
#include <Arduino.h>
#include <Preferences.h>
#include <sdkconfig.h>
#include <esp_timer.h>
#include <esp_random.h>
#if !defined(CONFIG_BT_NIMBLE_ENABLED) || !CONFIG_BT_NIMBLE_ENABLED
#error "CameraCompanion requires the ESP32-C6 NimBLE backend (Arduino-ESP32 3.3.12)."
#endif
extern "C" {
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_att.h"
#include "host/ble_sm.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
void ble_store_config_init(void);
}
#endif
#include <atomic>

namespace CameraRadio {
namespace {
struct Input {Command command;uint8_t value;Camera::Plan plan;uint32_t token=0;};
struct Saved {uint32_t magic;Peer peer;};
Snapshot live,published;portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
QueueHandle_t commands=nullptr;std::atomic<bool> stopWanted{false};
Preferences prefs;Saved saved{};Peer target{};Camera::Capture capture;
ble_npl_callout timer;uint8_t ownType=0;uint16_t conn=BLE_HS_CONN_HANDLE_NONE;
bool synced=false,connecting=false,wanted=false,wasReady=false;
bool watching=false,watchFound=false,disconnectWanted=false;
uint64_t reconnectSince=0,disconnectDeadline=0;bool slowWatch=false;
uint32_t generation=0;uint64_t setupDeadline=0,retryAt=0,nextGps=0,lastPublish=0;
uint64_t lastSimSample=UINT64_MAX;
uint64_t discoverAt=0;uint32_t pairingSequence=0;
uint8_t retries=0;
struct Handles {uint16_t rs=0,re=0,ls=0,le=0,control=0,status=0,statusEnd=0,cccd=0,loc=0,flags=0,lock=0,enable=0;} h;
enum class Op:uint8_t {None,Mtu,Services,RemoteChars,LocationChars,Flags,Lock,Enable,Descriptors,Subscribe,Remote,Location};
Op op=Op::None;uint64_t opDeadline=0;uint8_t setupStep=0;
Camera::Action action=Camera::Action::None;bool requireTz=true;
uint64_t nowMs(){return uint64_t(esp_timer_get_time()/1000);}
template<size_t N> void text(char (&out)[N],const char* s){snprintf(out,N,"%s",s?s:"");}
void trace(Event event,int code=0){if(live.traceCount==12){memmove(live.trace,live.trace+1,sizeof(live.trace[0])*11);--live.traceCount;}
  live.trace[live.traceCount++]={uint32_t(nowMs()),event,live.stage,code};}
void message(const char* s,int error=0){text(live.message,s);live.error=error;
  if(error && (live.lastFailure!=error || live.failureStage!=live.stage)){live.lastFailure=error;live.failureStage=live.stage;trace(Event::Error,error);}}
void publish(){live.phase=capture.phase;live.triggers=capture.completed;live.skipped=capture.skipped;live.due=capture.due;
  live.activePlan=capture.plan;
  portENTER_CRITICAL(&mux);published=live;portEXIT_CRITICAL(&mux);lastPublish=nowMs();}
void* context(){return reinterpret_cast<void*>(uintptr_t(generation));}
bool current(uint16_t handle,void* arg){return conn==handle && conn!=BLE_HS_CONN_HANDLE_NONE && uintptr_t(arg)==generation;}
void clearOp(){op=Op::None;opDeadline=0;}
void startOp(Op kind){op=kind;opDeadline=nowMs()+4000;}
void clearPairing(){live.pairingPrompt=PairingPrompt::None;live.pairingCode=live.pairingToken=0;}
bool uuid(const ble_uuid_t* u,uint16_t id,uint16_t service) {
  if(u->type==BLE_UUID_TYPE_16)return reinterpret_cast<const ble_uuid16_t*>(u)->value==id;
  if(u->type!=BLE_UUID_TYPE_128)return false;
  const uint8_t sony[16]={255,255,255,255,255,255,255,255,255,255,uint8_t(service),uint8_t(service>>8),uint8_t(id),uint8_t(id>>8),0,128};
  const uint8_t standard[16]={251,52,155,95,128,0,0,128,0,16,0,0,uint8_t(id),uint8_t(id>>8),0,0};
  const auto* bytes=reinterpret_cast<const ble_uuid128_t*>(u)->value;
  return memcmp(bytes,sony,16)==0 || memcmp(bytes,standard,16)==0;
}
bool modern(){return live.profile==Profile::Modern || (live.profile==Profile::Auto && live.protocol==0x65);}
void abortLink(const char* why,int code) {
  message(why,code);clearOp();clearPairing();wanted=false;watching=watchFound=disconnectWanted=false;ble_gap_disc_cancel();capture.disconnected();
  if(conn!=BLE_HS_CONN_HANDLE_NONE){live.link=Link::Disconnecting;ble_gap_terminate(conn,BLE_ERR_REM_USER_CONN_TERM);}
  else if(connecting){live.link=Link::Disconnecting;ble_gap_conn_cancel();}
  else live.link=Link::Failed;
}
void savePeer() {
  Saved record{};record.magic=0x43414d31;record.peer=target;
  ble_gap_conn_desc desc{};
  if(ble_gap_conn_find(conn,&desc)==0) {
    const uint8_t zeros[6]={};
    if(memcmp(desc.peer_id_addr.val,zeros,6)!=0){memcpy(record.peer.address,desc.peer_id_addr.val,6);record.peer.type=desc.peer_id_addr.type;}
  }
  if(memcmp(&record,&saved,sizeof record)!=0) {
    if(prefs.putBytes("peer",&record,sizeof record)!=sizeof record){message("Pair save failed; link remains active",-70);return;}
    saved=record;
  }
  live.saved=true;text(live.savedName,record.peer.name);
}
int gap(ble_gap_event* event,void* arg);
void connectPeer(const Peer& peer) {
  if(conn!=BLE_HS_CONN_HANDLE_NONE || connecting){message("Disconnect current camera first",-2);return;}
  ble_gap_disc_cancel();target=peer;h=Handles();clearOp();clearPairing();discoverAt=0;++generation;
  watching=watchFound=false;disconnectWanted=false;
  live.encrypted=live.bonded=live.authenticated=live.remote=live.location=live.notifications=false;
  live.focusKnown=live.shutterKnown=false;live.smpReason=-1;live.protocol=peer.protocol;text(live.name,peer.name);
  live.mtu=23;wasReady=live.saved && !memcmp(saved.peer.address,peer.address,6) && ((saved.peer.type&1)==(peer.type&1));wanted=true;connecting=true;
  live.link=Link::Connecting;live.stage=Stage::Connecting;++live.attempts;trace(Event::ConnectStart);
  message("Connecting: camera must be awake");setupDeadline=nowMs()+35000;
  ble_addr_t addr{};addr.type=peer.type;memcpy(addr.val,peer.address,6);
  const int rc=ble_gap_connect(ownType,&addr,30000,nullptr,gap,context());
  if(rc){connecting=false;live.link=Link::Failed;message("Connect could not start",rc);retryAt=nowMs()+5000;}
}
void settleDiscovery(){
  if(!wanted || live.link==Link::Disconnecting)return;
  clearPairing();live.link=Link::Pairing;live.stage=Stage::Settling;discoverAt=nowMs()+500;
  setupDeadline=discoverAt+20000;setupStep=0;requireTz=true;message("Encrypted: waiting for camera bond setup");
}
void startSecurity(){
  if(!wanted || live.link==Link::Disconnecting)return;
  if(live.encrypted){settleDiscovery();return;}
  live.link=Link::Pairing;live.stage=Stage::Security;message("Confirm pairing on the camera");setupDeadline=nowMs()+60000;
  const int rc=ble_gap_security_initiate(conn);trace(Event::SecurityStart,rc);
  if(rc && rc!=BLE_HS_EALREADY)abortLink("Pairing could not start",rc);
}
int mtuDone(uint16_t handle,const ble_gatt_error* err,uint16_t mtu,void* arg) {
  if(!current(handle,arg) || op!=Op::Mtu)return 0;
  trace(Event::Mtu,err->status);if(err->status==0)live.mtu=mtu;
  clearOp();startSecurity();return 0;
}
int services(uint16_t handle,const ble_gatt_error* err,const ble_gatt_svc* svc,void* arg) {
  if(!current(handle,arg) || op!=Op::Services)return 0;
  if(err->status==0 && svc){
    if(uuid(&svc->uuid.u,0xff00,0xff00)){h.rs=svc->start_handle;h.re=svc->end_handle;}
    if(uuid(&svc->uuid.u,0xdd00,0xdd00)){h.ls=svc->start_handle;h.le=svc->end_handle;}
  } else {clearOp();if(err->status!=BLE_HS_EDONE){abortLink("Service discovery failed",err->status);return 0;}setupStep=1;}
  return 0;
}
int chars(uint16_t handle,const ble_gatt_error* err,const ble_gatt_chr* chr,void* arg) {
  if(!current(handle,arg) || (op!=Op::RemoteChars && op!=Op::LocationChars))return 0;
  const bool remote=op==Op::RemoteChars;
  if(err->status==0 && chr){
    if(remote){
      if(h.status && h.statusEnd==0 && chr->def_handle>h.status)h.statusEnd=chr->def_handle-1;
      if(uuid(&chr->uuid.u,0xff01,0xff00) && (chr->properties & BLE_GATT_CHR_PROP_WRITE))h.control=chr->val_handle;
      if(uuid(&chr->uuid.u,0xff02,0xff00) && (chr->properties & BLE_GATT_CHR_PROP_NOTIFY))h.status=chr->val_handle;
    } else {
      if(uuid(&chr->uuid.u,0xdd11,0xdd00) && (chr->properties & BLE_GATT_CHR_PROP_WRITE))h.loc=chr->val_handle;
      if(uuid(&chr->uuid.u,0xdd21,0xdd00))h.flags=chr->val_handle;
      if(uuid(&chr->uuid.u,0xdd30,0xdd00))h.lock=chr->val_handle;
      if(uuid(&chr->uuid.u,0xdd31,0xdd00))h.enable=chr->val_handle;
    }
  }else{
    clearOp();if(err->status!=BLE_HS_EDONE){abortLink("Characteristic discovery failed",err->status);return 0;}
    if(remote && !h.statusEnd)h.statusEnd=h.re;
    setupStep=remote?2:3;
  }
  return 0;
}
int flagsDone(uint16_t handle,const ble_gatt_error* err,ble_gatt_attr* attr,void* arg) {
  if(!current(handle,arg) || op!=Op::Flags)return 0;
  uint8_t flags[7]{};
  if(!err->status && attr && OS_MBUF_PKTLEN(attr->om)>=5 && os_mbuf_copydata(attr->om,0,5,flags)==0)requireTz=(flags[4]&2)!=0;
  clearOp();setupStep=4;return 0;
}
int descriptors(uint16_t handle,const ble_gatt_error* err,uint16_t,const ble_gatt_dsc* dsc,void* arg) {
  if(!current(handle,arg) || op!=Op::Descriptors)return 0;
  if(err->status==0 && dsc){if(uuid(&dsc->uuid.u,0x2902,0))h.cccd=dsc->handle;}
  else {clearOp();setupStep=7;}
  return 0;
}
int wrote(uint16_t handle,const ble_gatt_error* err,ble_gatt_attr*,void* arg) {
  if(!current(handle,arg))return 0;
  const Op done=op;clearOp();const bool ok=err->status==0;
  switch(done){
    case Op::Lock:if(ok)setupStep=5;else{live.location=false;setupStep=6;message("GPS lock rejected; RC can still work",err->status);}break;
    case Op::Enable:live.location=ok;setupStep=6;if(!ok)message("GPS enable rejected; try Legacy profile",err->status);break;
    case Op::Subscribe:live.notifications=ok;setupStep=8;break;
    case Op::Remote:
      capture.ack(action,ok,nowMs());
      if(ok){++live.rcWrites;message("Remote write acknowledged");}
      else{message("RC rejected: check Bluetooth Rmt Ctrl",err->status);
        if(action==Camera::Action::FullUp || action==Camera::Action::HalfUp)abortLink("Release failed: check camera",err->status);}
      break;
    case Op::Location:
      if(ok){++live.gpsWrites;live.lastGps=nowMs();message("SIM GPS write acknowledged");}
      else message("GPS write rejected; check camera menu",err->status);
      nextGps=nowMs()+5000;break;
    default:break;
  }
  return 0;
}
void write(uint16_t attribute,const uint8_t* data,size_t n,Op kind) {
  startOp(kind);int rc;
  if(n>size_t(ble_att_mtu(conn)-3)){
    os_mbuf* om=ble_hs_mbuf_from_flat(data,uint16_t(n));
    if(!om){clearOp();abortLink("BLE buffer unavailable",BLE_HS_ENOMEM);return;}
    rc=ble_gattc_write_long(conn,attribute,0,om,wrote,context());
  }else rc=ble_gattc_write_flat(conn,attribute,data,uint16_t(n),wrote,context());
  if(rc){ble_gatt_error error{};error.status=rc;wrote(conn,&error,nullptr,context());}
}
void setup() {
  int rc=0;const uint8_t on=1;
  switch(setupStep){
    case 0:startOp(Op::Services);rc=ble_gattc_disc_all_svcs(conn,services,context());break;
    case 1:if(!h.rs){setupStep=2;break;}startOp(Op::RemoteChars);rc=ble_gattc_disc_all_chrs(conn,h.rs,h.re,chars,context());break;
    case 2:if(!h.ls){setupStep=3;break;}startOp(Op::LocationChars);rc=ble_gattc_disc_all_chrs(conn,h.ls,h.le,chars,context());break;
    case 3:if(!h.flags){setupStep=4;break;}startOp(Op::Flags);rc=ble_gattc_read(conn,h.flags,flagsDone,context());break;
    case 4:
      if(!modern()){live.location=h.loc!=0;setupStep=6;break;}
      if(!h.loc || !h.lock || !h.enable){live.location=false;setupStep=6;message("Modern GPS endpoints not available",-12);break;}
      write(h.lock,&on,1,Op::Lock);break;
    case 5:write(h.enable,&on,1,Op::Enable);break;
    case 6:
      if(!h.status || h.statusEnd<=h.status){setupStep=8;break;}
      startOp(Op::Descriptors);rc=ble_gattc_disc_all_dscs(conn,h.status,h.statusEnd,descriptors,context());break;
    case 7:if(!h.cccd){setupStep=8;break;} {const uint8_t notify[]={1,0};write(h.cccd,notify,2,Op::Subscribe);}break;
    case 8:
      live.remote=h.control!=0;
      if(!live.remote && !live.location){abortLink("No supported RC/GPS endpoints",-13);break;}
      live.link=Link::Ready;live.stage=Stage::Ready;trace(Event::Ready);wasReady=true;retries=0;nextGps=nowMs();savePeer();
      if(!live.error)message("Ready: test with camera in Still/Single");break;
  }
  if(rc){clearOp();abortLink("GATT setup could not start",rc);}
}
void scanResult(const ble_gap_disc_desc& desc) {
  ble_hs_adv_fields f{};if(ble_hs_adv_parse_fields(&f,desc.data,desc.length_data)!=0)return;
  const auto info=Camera::parseSony(f.mfg_data,f.mfg_data_len);
  int existing=-1;for(uint8_t i=0;i<live.count;++i)if(live.peers[i].type==desc.addr.type && !memcmp(live.peers[i].address,desc.addr.val,6)){existing=i;break;}
  if(!info.sony && existing<0)return;
  if(desc.event_type!=BLE_HCI_ADV_RPT_EVTYPE_ADV_IND && desc.event_type!=BLE_HCI_ADV_RPT_EVTYPE_SCAN_RSP)return;
  if(existing<0){if(live.count==kMaxCameras)return;existing=live.count++;}
  auto& p=live.peers[existing];memcpy(p.address,desc.addr.val,6);p.type=desc.addr.type;p.rssi=desc.rssi;
  if(info.sony){p.protocol=info.protocol;p.pairing=info.pairing;p.remote=info.remote;}
  if(f.name && f.name_len){const size_t n=f.name_len<sizeof(p.name)-1?f.name_len:sizeof(p.name)-1;memcpy(p.name,f.name,n);p.name[n]=0;
    for(size_t i=0;i<n;++i)if(uint8_t(p.name[i])<32 || uint8_t(p.name[i])>126)p.name[i]='?';}
  if(!p.name[0])text(p.name,"Sony camera");
}
int gap(ble_gap_event* e,void* arg) {
  if(e->type!=BLE_GAP_EVENT_DISC && e->type!=BLE_GAP_EVENT_DISC_COMPLETE && e->type!=BLE_GAP_EVENT_CONNECT && uintptr_t(arg)!=generation)return 0;
  switch(e->type){
    case BLE_GAP_EVENT_DISC:
      if(watching){ble_hs_adv_fields f{};if(ble_hs_adv_parse_fields(&f,e->disc.data,e->disc.length_data)!=0)break;
        const auto info=Camera::parseSony(f.mfg_data,f.mfg_data_len);
        if(info.sony && !memcmp(target.address,e->disc.addr.val,6) && ((target.type&1)==(e->disc.addr.type&1)) && (!info.powerKnown || info.poweredOn)){
          target.protocol=info.protocol;watching=false;watchFound=true;ble_gap_disc_cancel();}}
      else if(live.link==Link::Scanning)scanResult(e->disc);break;
    case BLE_GAP_EVENT_DISC_COMPLETE:
      if(watching){watching=false;retryAt=nowMs()+500;}
      if(live.link==Link::Scanning){live.link=Link::Idle;message(live.count?"Select a camera with up/down":"No Sony camera: start camera Pairing");}break;
    case BLE_GAP_EVENT_CONNECT:
      if(uintptr_t(arg)!=generation){if(!e->connect.status)ble_gap_terminate(e->connect.conn_handle,BLE_ERR_REM_USER_CONN_TERM);break;}
      connecting=false;trace(Event::ConnectResult,e->connect.status);
      if(e->connect.status){live.link=Link::Failed;message(wanted?"Connect failed; camera awake?":"Connection cancelled",e->connect.status);retryAt=nowMs()+(wasReady?500:5000);break;}
      conn=e->connect.conn_handle;
      if(!wanted){ble_gap_terminate(conn,BLE_ERR_REM_USER_CONN_TERM);break;}
      // camera-gps-link mode 1: MTU callback comes before createBond().
      live.link=Link::Pairing;live.stage=Stage::Mtu;message("Negotiating camera MTU before pairing");setupDeadline=nowMs()+65000;
      startOp(Op::Mtu);trace(Event::MtuStart);
      {const int rc=ble_gattc_exchange_mtu(conn,mtuDone,context());if(rc){ble_gatt_error error{};error.status=rc;mtuDone(conn,&error,23,context());}}break;
#ifdef BLE_GAP_EVENT_LINK_ESTAB
    case BLE_GAP_EVENT_LINK_ESTAB:
      if(e->link_estab.conn_handle==conn)trace(Event::LinkEstablished,e->link_estab.status);break;
#endif
#ifdef BLE_GAP_EVENT_PARING_COMPLETE
    case BLE_GAP_EVENT_PARING_COMPLETE:
      if(e->pairing_complete.conn_handle==conn){live.smpReason=e->pairing_complete.status;trace(Event::PairingComplete,e->pairing_complete.status);}break;
#endif
    case BLE_GAP_EVENT_ENC_CHANGE:
      if(e->enc_change.conn_handle!=conn)break;
      live.securityCode=e->enc_change.status;trace(Event::SecurityResult,e->enc_change.status);
      {ble_gap_conn_desc d{};const bool secure=e->enc_change.status==0 && ble_gap_conn_find(conn,&d)==0 && d.sec_state.encrypted;
      live.encrypted=secure;live.bonded=secure && d.sec_state.bonded;live.authenticated=secure && d.sec_state.authenticated;
      if(!secure){
        if(live.link==Link::Disconnecting && !wanted)break; // Keep a local rejection/cancel/timeout as the primary cause.
        if(e->enc_change.status==BLE_HS_ENOTCONN){
          // NimBLE reports this while destroying an interrupted SM procedure,
          // before the GAP disconnect callback supplies the actual HCI reason.
          // Preserve our earlier timeout/cancellation: it caused this event.
          if(live.link!=Link::Disconnecting && wanted){
            message("Link lost during pairing; see disconnect reason",BLE_HS_ENOTCONN);
            live.link=Link::Disconnecting;if(!wasReady)wanted=false;
          }
        }else abortLink("Encryption failed; see status details",e->enc_change.status?e->enc_change.status:-17);
        break;}
      // The peer can initiate security before our MTU callback. Never overlap
      // service discovery with that outstanding ATT operation.
      settleDiscovery();}
      break;
    case BLE_GAP_EVENT_DISCONNECT:
      if(e->disconnect.conn.conn_handle!=conn)break;
      live.disconnectReason=e->disconnect.reason;trace(Event::Disconnected,e->disconnect.reason);
      if(live.stage==Stage::Security && live.error==BLE_HS_ENOTCONN && e->disconnect.reason)
        message("Link dropped during pairing; see HCI reason",e->disconnect.reason);
      else if(wanted && !wasReady && !live.error)message("Camera disconnected before setup completed",e->disconnect.reason?e->disconnect.reason:-18);
      conn=BLE_HS_CONN_HANDLE_NONE;++generation;connecting=false;disconnectWanted=false;clearOp();clearPairing();capture.disconnected();
      live.encrypted=live.bonded=live.authenticated=live.remote=live.location=live.notifications=false;
      live.focusKnown=live.shutterKnown=false;live.link=live.error?Link::Failed:Link::Idle;
      if(wanted && wasReady){retryAt=nowMs()+100;reconnectSince=nowMs();slowWatch=false;message("Link lost; plan stopped, reconnecting");}
      else if(!wanted && !live.error)message("Disconnected");
      live.stage=Stage::Idle;publish();break;
    case BLE_GAP_EVENT_NOTIFY_RX:
      if(e->notify_rx.conn_handle!=conn || e->notify_rx.attr_handle!=h.status)break;
      {uint8_t b[3]{};if(OS_MBUF_PKTLEN(e->notify_rx.om)!=3 || os_mbuf_copydata(e->notify_rx.om,0,3,b)!=0)break;
      live.lastNotify=nowMs();if(b[0]==2 && b[1]==0x3f){live.focusKnown=true;live.focused=b[2]==0x20;}
      if(b[0]==2 && b[1]==0xa0){live.shutterKnown=true;live.shutterBusy=b[2]==0x20;}
      if(b[0]==2 && b[1]==0xc3 && b[2]==0){live.remote=false;message("Enable Bluetooth Rmt Ctrl, reconnect",-14);}}
      break;
    case BLE_GAP_EVENT_REPEAT_PAIRING:abortLink("Old bond: forget this camera on both sides",-15);return BLE_GAP_REPEAT_PAIRING_IGNORE;
    case BLE_GAP_EVENT_PASSKEY_ACTION:
      if(e->passkey.conn_handle!=conn || !wanted || live.link==Link::Disconnecting)break;
      trace(Event::PairingAction,e->passkey.params.action);
      if(e->passkey.params.action==BLE_SM_IOACT_NUMCMP){
        if(e->passkey.params.numcmp>999999){abortLink("Invalid numeric pairing challenge",-16);break;}
        live.pairingPrompt=PairingPrompt::Compare;live.pairingCode=e->passkey.params.numcmp;
        if(!++pairingSequence)++pairingSequence;live.pairingToken=pairingSequence;message("Compare codes on camera and board");publish();
      }else if(e->passkey.params.action==BLE_SM_IOACT_DISP){
        if(live.pairingPrompt!=PairingPrompt::None){abortLink("Unexpected second pairing prompt",-16);break;}
        ble_sm_io io{};io.action=BLE_SM_IOACT_DISP;io.passkey=esp_random()%1000000;
        live.pairingPrompt=PairingPrompt::Display;live.pairingCode=io.passkey;
        if(!++pairingSequence)++pairingSequence;live.pairingToken=pairingSequence;
        const int rc=ble_sm_inject_io(conn,&io);
        if(rc)abortLink("Could not supply pairing display code",rc);else message("Enter displayed code on the camera");publish();
      }else if(e->passkey.params.action!=BLE_SM_IOACT_NONE)abortLink("Camera requested unsupported pairing input",-16);
      break;
    default:break;
  }
  return 0;
}
void handle(const Input& in) {
  switch(in.command){
    case Command::Scan:
      if(conn!=BLE_HS_CONN_HANDLE_NONE || connecting){message("Disconnect before scanning",-2);break;}
      wanted=false;watching=watchFound=false;ble_gap_disc_cancel();live.count=0;memset(live.peers,0,sizeof live.peers);live.stage=Stage::Scanning;trace(Event::Scan);
      {ble_gap_disc_params p{};p.itvl=160;p.window=80;p.passive=0;p.filter_duplicates=0;
      const int rc=ble_gap_disc(ownType,8000,&p,gap,nullptr);
      if(rc){live.link=Link::Failed;message("Scan failed",rc);}else{live.link=Link::Scanning;message("Camera Pairing first, then scan");}}
      break;
    case Command::Connect:if(in.value<live.count){retries=0;connectPeer(live.peers[in.value]);}else message("Camera selection changed",-3);break;
    case Command::Saved:if(live.saved){retries=0;connectPeer(saved.peer);}else message("No saved camera",-4);break;
    case Command::Disconnect:
      clearPairing();wanted=false;watching=watchFound=false;retryAt=0;ble_gap_disc_cancel();message("Disconnecting");
      if(conn!=BLE_HS_CONN_HANDLE_NONE && live.link==Link::Ready && live.remote){capture.stop();disconnectWanted=true;disconnectDeadline=nowMs()+3000;}
      else if(conn!=BLE_HS_CONN_HANDLE_NONE){clearOp();live.link=Link::Disconnecting;ble_gap_terminate(conn,BLE_ERR_REM_USER_CONN_TERM);}
      else if(connecting){live.link=Link::Disconnecting;ble_gap_conn_cancel();}
      else{ble_gap_disc_cancel();live.link=Link::Idle;message("Disconnected");}break;
    case Command::Forget:
      if(conn!=BLE_HS_CONN_HANDLE_NONE || connecting){message("Disconnect before forgetting",-2);break;}
      wanted=false;watching=watchFound=false;retryAt=0;ble_gap_disc_cancel();
      if(live.saved){ble_addr_t a{};a.type=saved.peer.type;memcpy(a.val,saved.peer.address,6);const int rc=ble_store_util_delete_peer(&a);
        if(rc && rc!=BLE_HS_ENOENT){message("Could not delete selected camera bond",rc);break;}
        prefs.remove("peer");saved=Saved();live.saved=false;live.savedName[0]=0;}
      message("Saved peer removed; remove it on camera");break;
    case Command::Single:case Command::Focus:case Command::Start:
      if(live.link!=Link::Ready || !live.remote){message("Camera RC not ready",-20);break;}
      {Camera::Plan p=in.plan;if(in.command!=Command::Start){p=Camera::Plan();p.count=1;p.focusOnly=in.command==Command::Focus;}
      if(!capture.start(nowMs(),p))message("Stop current plan before starting",-21);
      else {live.captureKind=in.command==Command::Start?CaptureKind::Interval:p.focusOnly?CaptureKind::Focus:CaptureKind::Single;
        message(p.focusOnly?"Focusing":"Capture plan started");}}break;
    case Command::Pause:capture.pause();message("Pause: releasing shutter");break;
    case Command::Resume:
      if(live.link!=Link::Ready || !live.remote || !capture.resume(nowMs()))message("No paused plan ready",-22);
      else message("Capture resumed from now");break;
    case Command::SetSim:live.sim=in.value!=0;message(live.sim?"SIM GPS enabled: synthetic coordinates":"GPS sending disabled");break;
    case Command::SetFix:live.simFix=in.value!=0;message(live.simFix?"SIM fix restored":"SIM signal loss: no new location writes");break;
    case Command::SetProfile:
      if(conn!=BLE_HS_CONN_HANDLE_NONE || connecting || in.value>2){message("Disconnect to change profile",-2);break;}
      live.profile=Profile(in.value);prefs.putUChar("profile",in.value);message("Profile set; reconnect camera");break;
    case Command::PairingReply:
      // A delayed tap/queued reply from an earlier link must never authorize a
      // new pairing. The request token changes with each on-screen challenge.
      if(conn==BLE_HS_CONN_HANDLE_NONE || !wanted || live.link==Link::Disconnecting || live.pairingPrompt==PairingPrompt::None || !in.token || in.token!=live.pairingToken)break;
      if(live.pairingPrompt==PairingPrompt::Display){if(!in.value)abortLink("Pairing cancelled on board",0);break;}
      {ble_sm_io io{};io.action=BLE_SM_IOACT_NUMCMP;io.numcmp_accept=in.value!=0;clearPairing();trace(Event::PairingReply,in.value!=0);
      if(!in.value){abortLink("Pairing rejected on board",0);ble_sm_inject_io(conn,&io);break;}
      const int rc=ble_sm_inject_io(conn,&io);if(rc)abortLink("Pairing confirmation failed",rc);else message("Confirmed on board; confirm on camera too");}break;
  }
}
void tick(ble_npl_event*) {
  const auto now=nowMs();
  if(stopWanted.exchange(false)){
    xQueueReset(commands);
    if(live.link==Link::Ready && live.remote){capture.stop();message("Stop requested: releasing shutter");}
    else if(live.link==Link::Ready){capture.disconnected();message("Capture stopped; GPS link remains");}
    else {wanted=false;watching=watchFound=false;retryAt=0;ble_gap_disc_cancel();
      if(conn!=BLE_HS_CONN_HANDLE_NONE || connecting)abortLink("Stopped",0);
      else {capture.disconnected();live.link=Link::Idle;message("Stopped");}}
  }
  Input in{};for(uint8_t n=0;n<4 && xQueueReceive(commands,&in,0)==pdTRUE;++n){
    if(disconnectWanted && in.command!=Command::Disconnect){message("Disconnect in progress",-23);continue;}handle(in);}
  if(live.simFix && now/1000!=lastSimSample){lastSimSample=now/1000;live.fix=Camera::simulate(lastSimSample*1000);}
  live.fix.valid=live.simFix;
  if(op!=Op::None && now>=opDeadline){clearOp();abortLink("GATT timeout: plan stopped",-30);}
  if(connecting && conn==BLE_HS_CONN_HANDLE_NONE && live.link!=Link::Disconnecting && now>=setupDeadline)abortLink("Connection timeout: check camera power",-31);
  if(conn!=BLE_HS_CONN_HANDLE_NONE && live.link!=Link::Ready && live.link!=Link::Disconnecting && now>=setupDeadline)abortLink("Pair/setup timeout; check camera screen",-31);
  if(conn!=BLE_HS_CONN_HANDLE_NONE && live.stage==Stage::Settling && live.link!=Link::Disconnecting && op==Op::None && now>=discoverAt){
    live.link=Link::Discovering;live.stage=Stage::Discovery;trace(Event::DiscoveryStart);message("Discovering encrypted camera services");}
  if(conn!=BLE_HS_CONN_HANDLE_NONE && live.link==Link::Discovering && op==Op::None)setup();
  if(live.link==Link::Ready && op==Op::None){
    const auto a=capture.next(now,live.remote,live.shutterKnown && live.shutterBusy,live.focusKnown,live.focused);
    if(a!=Camera::Action::None){action=a;if(a==Camera::Action::HalfDown){live.focusKnown=false;live.focused=false;}
      const uint8_t bytes[]={1,Camera::commandByte(a)};write(h.control,bytes,2,Op::Remote);}
    else if(live.sim && live.simFix && live.location && now>=nextGps && !capture.full && !capture.half &&
            (!capture.running() || (capture.phase==Camera::Phase::Wait && capture.due>now+1000))){
      constexpr int16_t timezoneMinutes=480;
      uint8_t data[95]{};const size_t n=Camera::locationPayload(live.fix,requireTz || timezoneMinutes!=0,timezoneMinutes,0,data);
      if(n)write(h.loc,data,n,Op::Location);
    }
  }
  if(disconnectWanted && ((op==Op::None && !capture.running()) || now>=disconnectDeadline)){
    disconnectWanted=false;live.link=Link::Disconnecting;ble_gap_terminate(conn,BLE_ERR_REM_USER_CONN_TERM);
  }
  if(capture.phase==Camera::Phase::Error && live.link==Link::Ready && capture.error[0])message(capture.error,-24);
  if(conn==BLE_HS_CONN_HANDLE_NONE && !connecting && wanted && live.link!=Link::Scanning){
    if(watchFound){if(retries++<10)connectPeer(target);else{wanted=false;watchFound=false;message("Reconnect limit reached; connect manually",-32);}}
    else if(wasReady){
      const bool slow=now-reconnectSince>=60000;
      if(watching && slow!=slowWatch){watching=false;ble_gap_disc_cancel();retryAt=now+20;}
      if(!watching && now>=retryAt){ble_gap_disc_params p{};p.passive=0;p.filter_duplicates=0;p.itvl=slow?320:80;p.window=slow?32:80;
        const int rc=ble_gap_disc(ownType,BLE_HS_FOREVER,&p,gap,nullptr);
        if(!rc){watching=true;slowWatch=slow;live.link=Link::Reconnecting;message("Waiting for saved camera to power on");}
        else{retryAt=now+1000;message("Reconnect scan retry",rc);}}
    }else if(retryAt && now>=retryAt){
      if(retries++<10){retryAt=now+30000;connectPeer(target);}else{wanted=false;message("Reconnect limit reached; connect manually",-32);}
    }
  }
  if(now-lastPublish>=100)publish();
  ble_npl_callout_reset(&timer,ble_npl_time_ms_to_ticks32(20));
}
void sync(){if(ble_hs_util_ensure_addr(0)!=0 || ble_hs_id_infer_auto(0,&ownType)!=0){message("BLE address unavailable",-40);publish();return;}
  synced=true;live.link=Link::Idle;message("Ready to scan for Sony camera");publish();ble_npl_callout_reset(&timer,ble_npl_time_ms_to_ticks32(20));}
void reset(int reason){live.link=Link::Failed;message("BLE host reset; reconnect manually",reason);clearPairing();capture.disconnected();wanted=false;conn=BLE_HS_CONN_HANDLE_NONE;++generation;clearOp();connecting=watching=watchFound=false;publish();}
void host(void*){nimble_port_run();nimble_port_freertos_deinit();}
} // namespace
bool begin() {
  commands=xQueueCreate(12,sizeof(Input));if(!commands){message("Command queue allocation failed",-50);publish();return false;}
  if(!prefs.begin("c6_camera",false)){message("Camera settings unavailable",-51);publish();return false;}
  if(prefs.getBytesLength("peer")==sizeof saved){prefs.getBytes("peer",&saved,sizeof saved);
    if(saved.magic==0x43414d31 && saved.peer.type<=3){saved.peer.name[31]=0;live.saved=true;text(live.savedName,saved.peer.name);}else saved=Saved();}
  const uint8_t profile=prefs.getUChar("profile",0);live.profile=profile<=2?Profile(profile):Profile::Auto;
  if(live.saved){target=saved.peer;wanted=wasReady=true;retryAt=nowMs()+100;reconnectSince=nowMs();}
  const int rc=nimble_port_init();if(rc){message("NimBLE initialization failed",rc);publish();return false;}
  ble_svc_gap_init();ble_svc_gatt_init();ble_svc_gap_device_name_set("C6 Camera Companion");
  ble_hs_cfg.sync_cb=sync;ble_hs_cfg.reset_cb=reset;
  // Never evict unrelated bonds when the system bond store is full.
  ble_hs_cfg.store_status_cb=[](ble_store_status_event*,void*){return BLE_HS_ENOMEM;};
  // This board has a display and a touch confirmation button. Do not advertise
  // NoInputNoOutput or auto-accept numeric comparison requests.
  ble_hs_cfg.sm_io_cap=BLE_HS_IO_DISPLAY_YESNO;ble_hs_cfg.sm_bonding=1;ble_hs_cfg.sm_mitm=1;ble_hs_cfg.sm_sc=1;
  ble_hs_cfg.sm_our_key_dist=BLE_SM_PAIR_KEY_DIST_ENC|BLE_SM_PAIR_KEY_DIST_ID;
  ble_hs_cfg.sm_their_key_dist=BLE_SM_PAIR_KEY_DIST_ENC|BLE_SM_PAIR_KEY_DIST_ID;
  ble_store_config_init();const int mtuRc=ble_att_set_preferred_mtu(517);
  if(mtuRc){message("Could not configure requested MTU",mtuRc);publish();return false;}
  ble_npl_callout_init(&timer,nimble_port_get_dflt_eventq(),tick,nullptr);
  nimble_port_freertos_init(host);publish();return true;
}
bool request(Command command,uint8_t value,const Camera::Plan& plan){if(!commands)return false;const Input in{command,value,plan};return xQueueSend(commands,&in,0)==pdTRUE;}
bool confirmPairing(bool accept,uint32_t token){if(!commands || !token)return false;const Input in{Command::PairingReply,uint8_t(accept),Camera::Plan(),token};return xQueueSend(commands,&in,0)==pdTRUE;}
void stop(){stopWanted.store(true);}
Snapshot snapshot(){portENTER_CRITICAL(&mux);const auto result=published;portEXIT_CRITICAL(&mux);return result;}
const char* linkName(Link s){static const char* names[]={"STARTING","IDLE","SCANNING","CONNECTING","PAIRING","DISCOVERING","READY","DISCONNECT","FAILED","RECONNECT"};return names[uint8_t(s)];}
const char* profileName(Profile p){static const char* names[]={"AUTO","LEGACY","MODERN"};return names[uint8_t(p)];}
const char* stageName(Stage s){static const char* names[]={"IDLE","SCAN","CONNECT","MTU","SECURITY","SETTLING","DISCOVERY","READY"};return names[uint8_t(s)];}
const char* eventName(Event e){static const char* names[]={"SCAN","CONNECT_START","CONNECT_RESULT","LINK_ESTABLISHED","SECURITY_START","SECURITY_RESULT","MTU","ERROR","DISCONNECT","READY","MTU_START","PAIRING_ACTION","PAIRING_REPLY","DISCOVERY_START","PAIRING_COMPLETE"};return names[uint8_t(e)];}
}
