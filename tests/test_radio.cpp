#define CAMERA_HOST_TEST 1
#include "../CameraCompanion/CameraRadio.cpp"
#include <cassert>
using namespace CameraRadio;
void clean(){live=Snapshot();published=Snapshot();capture=Camera::Capture();h=Handles();clearOp();
  conn=BLE_HS_CONN_HANDLE_NONE;wanted=connecting=wasReady=watching=watchFound=disconnectWanted=false;retries=0;
  retryAt=0;generation=10;testMs=10000;lastSimSample=UINT64_MAX;wire=HostState();stopWanted=false;xQueueReset(commands);}
void ready(){clean();conn=42;live.link=Link::Ready;live.encrypted=live.remote=live.location=true;h.control=10;h.loc=20;}
void ack(int status=0){ble_gatt_error e{};e.status=status;wrote(conn,&e,nullptr,context());}
void mtuAck(int status=0){ble_gatt_error e{};e.status=status;mtuDone(conn,&e,128,context());}
int main(){assert(begin());assert(ble_hs_cfg.sm_bonding==1 && ble_hs_cfg.sm_mitm==1 && ble_hs_cfg.sm_sc==1 && ble_hs_cfg.sm_io_cap==BLE_HS_IO_DISPLAY_YESNO);assert(ble_hs_cfg.store_status_cb(nullptr,nullptr)==BLE_HS_ENOMEM);
  clean();live.link=Link::Scanning;
  const uint8_t raw[]={15,255,0x2d,1,3,0,0x64,0,'A','1',0x22,0xc2,0,0x21,0x40,0,10,9,'I','L','C','E','-','7','M','4','X'};
  ble_gap_disc_desc desc;desc.data=raw;desc.length_data=sizeof raw;desc.addr.val[0]=7;desc.addr.type=1;scanResult(desc);
  assert(live.count==1 && live.peers[0].protocol==0x64 && live.peers[0].type==1);
  scanResult(desc);assert(live.count==1);const uint8_t bad[]={5,255,0,0,0,0};desc.addr.val[0]=8;desc.data=bad;desc.length_data=sizeof bad;scanResult(desc);assert(live.count==1);
  Peer p{};p.protocol=0x64;connectPeer(p);assert(wire.timeout==30000 && connecting);
  ble_gap_event event;event.type=BLE_GAP_EVENT_CONNECT;event.connect.conn_handle=42;gap(&event,context());assert(conn==42 && wire.security==0 && wire.mtuRequests==1 && live.stage==Stage::Mtu);
  mtuAck();assert(wire.security==1 && live.stage==Stage::Security && live.mtu==128);
  event.type=BLE_GAP_EVENT_ENC_CHANGE;event.enc_change.conn_handle=42;wire.encrypted=false;gap(&event,context());assert(wire.terminate==1 && !live.encrypted);
  ready();live.protocol=0x64;setupStep=4;setup();assert(live.location && setupStep==6 && wire.writes.empty());
  ready();live.protocol=0x65;h.lock=21;h.enable=22;setupStep=4;setup();assert(op==Op::Lock && wire.writes.back().attr==21);ack();assert(setupStep==5);setup();assert(op==Op::Enable);ack();assert(live.location);
  ready();op=Op::Services;ble_gatt_error done;done.status=BLE_HS_EDONE;services(conn,&done,nullptr,reinterpret_cast<void*>(uintptr_t(generation-1)));assert(op==Op::Services);
  ready();live.sim=true;Camera::Plan plan;plan.count=1;assert(capture.start(testMs,plan));tick(nullptr);
  assert(op==Op::Remote && wire.writes.back().data[1]==9);ack();testMs+=30;tick(nullptr);assert(wire.writes.size()==1);
  for(int i=0;i<12;++i)assert(request(Command::Single));assert(!request(Command::Single));stop();tick(nullptr);
  assert(commands->data.empty() && wire.writes.back().data[1]==8);ack();tick(nullptr);assert(wire.writes.back().data[1]==6);ack();
  assert(capture.phase==Camera::Phase::Idle && !capture.full && !capture.half);
  ready();wanted=true;stop();tick(nullptr);assert(wanted);ack();tick(nullptr);ack();
  ready();live.sim=true;nextGps=0;tick(nullptr);assert(op==Op::Location && wire.writes.back().longWrite && wire.writes.back().data.size()==95);ack();assert(live.gpsWrites==1);
  ready();startOp(Op::Remote);opDeadline=testMs+1;testMs+=2;tick(nullptr);assert(wire.terminate==1 && live.error==-30 && !wanted);
  ready();assert(capture.start(testMs,plan));tick(nullptr);handle({Command::Disconnect,0,plan});assert(wire.terminate==0 && disconnectWanted);ack();tick(nullptr);ack();tick(nullptr);ack();tick(nullptr);assert(wire.terminate==1);
  ready();wanted=wasReady=true;target.address[0]=7;target.type=1;
  event=ble_gap_event();event.type=BLE_GAP_EVENT_DISCONNECT;event.disconnect.conn.conn_handle=42;gap(&event,context());testMs+=100;tick(nullptr);
  assert(watching && live.link==Link::Reconnecting && wire.scanParams.window==80);
  std::vector<uint8_t> asleep(raw,raw+sizeof raw);asleep[14]=0;
  event=ble_gap_event();event.type=BLE_GAP_EVENT_DISC;event.disc.data=asleep.data();event.disc.length_data=uint8_t(asleep.size());event.disc.addr.val[0]=7;event.disc.addr.type=1;gap(&event,nullptr);tick(nullptr);assert(wire.connect==0);
  testMs+=60001;tick(nullptr);testMs+=20;tick(nullptr);assert(watching && wire.scanParams.window==32 && wire.scanParams.itvl==320);
  event=ble_gap_event();event.type=BLE_GAP_EVENT_DISC;event.disc.data=raw;event.disc.length_data=sizeof raw;event.disc.addr.val[0]=7;event.disc.addr.type=1;gap(&event,nullptr);tick(nullptr);
  assert(wire.connect==1 && capture.phase==Camera::Phase::Idle);
  stop();tick(nullptr);assert(wire.cancel==1 && !wanted);
  clean();live.saved=true;saved.peer.address[0]=9;handle({Command::Forget,0,plan});assert(wire.deleted==1 && !live.saved);
  ready();plan.count=30;plan.intervalMs=15000;handle({Command::Start,0,plan});publish();assert(published.captureKind==CaptureKind::Interval && published.activePlan.count==30 && published.activePlan.intervalMs==15000);
  handle({Command::Single,0,plan});publish();assert(published.captureKind==CaptureKind::Interval && published.activePlan.count==30); // Rejected start must not replace the running plan.
  ready();handle({Command::Focus,0,plan});publish();assert(published.captureKind==CaptureKind::Focus && published.activePlan.focusOnly);
  clean();connectPeer(p);testMs+=35001;tick(nullptr);assert(wire.cancel==1 && !wanted && live.error==-31);
  tick(nullptr);assert(wire.cancel==1); // The expired timer does not repeatedly cancel an in-flight cancellation.
  clean();connectPeer(p);event=ble_gap_event();event.type=BLE_GAP_EVENT_CONNECT;event.connect.conn_handle=42;gap(&event,context());
  mtuAck();
  assert(live.stage==Stage::Security && setupDeadline==testMs+60000);
  testMs+=21000;tick(nullptr);assert(wire.terminate==0 && live.link==Link::Pairing); // Time to confirm on the camera.
  event=ble_gap_event();event.type=BLE_GAP_EVENT_ENC_CHANGE;event.enc_change.conn_handle=42;event.enc_change.status=BLE_HS_ENOTCONN;gap(&event,context());
  assert(live.securityCode==7 && wire.terminate==0 && !wanted); // SM's broken-connection code is not a reason to issue another terminate.
  event=ble_gap_event();event.type=BLE_GAP_EVENT_DISCONNECT;event.disconnect.conn.conn_handle=42;event.disconnect.reason=0x23e;gap(&event,context());
  assert(live.disconnectReason==0x23e && live.error==0x23e && live.lastFailure==0x23e && live.failureStage==Stage::Security);
  assert(live.trace[live.traceCount-2].event==Event::Disconnected && published.disconnectReason==0x23e);
  message("Unrelated successful operation");assert(live.error==0 && live.lastFailure==0x23e && live.disconnectReason==0x23e);
  const auto before=live.traceCount;event=ble_gap_event();event.type=BLE_GAP_EVENT_LINK_ESTAB;event.link_estab.conn_handle=42;gap(&event,reinterpret_cast<void*>(uintptr_t(generation-1)));assert(live.traceCount==before);
  for(int i=0;i<20;++i)trace(Event::ConnectStart,i);assert(live.traceCount==12 && live.trace[0].code==8 && live.trace[11].code==19);
  clean();connectPeer(p);event=ble_gap_event();event.type=BLE_GAP_EVENT_CONNECT;event.connect.conn_handle=42;gap(&event,context());
  mtuAck();
  abortLink("Pair/setup timeout",-31);event=ble_gap_event();event.type=BLE_GAP_EVENT_ENC_CHANGE;event.enc_change.conn_handle=42;event.enc_change.status=7;gap(&event,context());
  assert(live.error==-31 && live.lastFailure==-31 && wire.terminate==1);
  event=ble_gap_event();event.type=BLE_GAP_EVENT_DISCONNECT;event.disconnect.conn.conn_handle=42;event.disconnect.reason=0x216;gap(&event,context());assert(live.error==-31 && live.disconnectReason==0x216);
  clean();connectPeer(p);event=ble_gap_event();event.type=BLE_GAP_EVENT_CONNECT;event.connect.conn_handle=42;gap(&event,context());
  mtuAck();
  handle({Command::Disconnect,0,plan});event=ble_gap_event();event.type=BLE_GAP_EVENT_ENC_CHANGE;event.enc_change.conn_handle=42;event.enc_change.status=7;gap(&event,context());assert(live.error==0);
  // App mode 1: ATT MTU completes before security; discovery waits 500ms.
  clean();connectPeer(p);event=ble_gap_event();event.type=BLE_GAP_EVENT_CONNECT;event.connect.conn_handle=42;gap(&event,context());
  tick(nullptr);assert(wire.security==0 && wire.serviceRequests==0);mtuAck();
  event=ble_gap_event();event.type=BLE_GAP_EVENT_ENC_CHANGE;event.enc_change.conn_handle=42;gap(&event,context());
  assert(live.encrypted && live.stage==Stage::Settling && wire.mtuRequests==1);
  testMs+=499;tick(nullptr);assert(wire.serviceRequests==0);++testMs;tick(nullptr);assert(wire.serviceRequests==1 && live.stage==Stage::Discovery);
  clean();connectPeer(p);event=ble_gap_event();event.type=BLE_GAP_EVENT_CONNECT;event.connect.conn_handle=42;gap(&event,context());
  mtuAck(5);assert(wire.security==1 && live.mtu==23); // App's callback proceeds even on MTU failure.
  clean();connectPeer(p);event=ble_gap_event();event.type=BLE_GAP_EVENT_CONNECT;event.connect.conn_handle=42;gap(&event,context());
  testMs+=4001;tick(nullptr);assert(wire.security==0 && wire.terminate==1 && live.failureStage==Stage::Mtu);
  mtuAck();assert(wire.security==0); // A late callback cannot restart a cancelled/timed-out pairing.
  clean();connectPeer(p);wire.nextError=5;event=ble_gap_event();event.type=BLE_GAP_EVENT_CONNECT;event.connect.conn_handle=42;gap(&event,context());assert(wire.security==1 && live.stage==Stage::Security && live.mtu==23);
  // The camera may start encryption before MTU callback; discovery still serializes.
  clean();connectPeer(p);event=ble_gap_event();event.type=BLE_GAP_EVENT_CONNECT;event.connect.conn_handle=42;gap(&event,context());
  event.type=BLE_GAP_EVENT_ENC_CHANGE;event.enc_change.conn_handle=42;gap(&event,context());testMs+=600;tick(nullptr);assert(wire.serviceRequests==0);
  mtuAck();testMs+=500;tick(nullptr);assert(wire.serviceRequests==1 && wire.security==0);
  // No auto-confirmation. A UI request must match the outstanding challenge token.
  clean();conn=42;wanted=true;live.link=Link::Pairing;live.stage=Stage::Security;setupDeadline=testMs+60000;
  event=ble_gap_event();event.type=BLE_GAP_EVENT_PASSKEY_ACTION;event.passkey.conn_handle=42;event.passkey.params.action=BLE_SM_IOACT_NUMCMP;event.passkey.params.numcmp=12345;gap(&event,context());
  const auto token=live.pairingToken;assert(token && live.pairingPrompt==PairingPrompt::Compare && live.pairingCode==12345 && wire.ioRequests==0);
  assert(confirmPairing(true,token+1));tick(nullptr);assert(wire.ioRequests==0);
  assert(confirmPairing(true,token));tick(nullptr);assert(wire.ioRequests==1 && wire.io.action==BLE_SM_IOACT_NUMCMP && wire.io.numcmp_accept==1 && live.pairingPrompt==PairingPrompt::None);
  assert(confirmPairing(true,token));tick(nullptr);assert(wire.ioRequests==1);
  gap(&event,context());assert(live.pairingToken!=token);assert(confirmPairing(false,live.pairingToken));tick(nullptr);assert(wire.terminate==1 && !wanted && live.pairingPrompt==PairingPrompt::None && !wire.io.numcmp_accept);
  clean();conn=42;wanted=true;live.link=Link::Pairing;live.stage=Stage::Security;setupDeadline=testMs+60000;
  event.passkey.params.action=BLE_SM_IOACT_DISP;gap(&event,context());assert(live.pairingPrompt==PairingPrompt::Display && live.pairingCode==234567 && wire.io.passkey==234567);
  const auto displayToken=live.pairingToken;assert(confirmPairing(true,displayToken));tick(nullptr);assert(wire.terminate==0);
  assert(confirmPairing(false,displayToken));tick(nullptr);assert(wire.terminate==1 && live.pairingPrompt==PairingPrompt::None);
  clean();conn=42;wanted=true;live.link=Link::Pairing;live.stage=Stage::Security;setupDeadline=testMs+60000;
  event.passkey.params.action=BLE_SM_IOACT_NUMCMP;gap(&event,context());const auto lostToken=live.pairingToken;
  event=ble_gap_event();event.type=BLE_GAP_EVENT_DISCONNECT;event.disconnect.conn.conn_handle=42;event.disconnect.reason=0x213;gap(&event,context());assert(live.pairingPrompt==PairingPrompt::None && !live.pairingCode);
  assert(confirmPairing(true,lostToken));tick(nullptr);assert(wire.ioRequests==0);
  clean();conn=42;wanted=true;live.link=Link::Pairing;live.stage=Stage::Security;setupDeadline=testMs+60000;
  event=ble_gap_event();event.type=BLE_GAP_EVENT_PARING_COMPLETE;event.pairing_complete.conn_handle=42;event.pairing_complete.status=3;gap(&event,context());assert(live.smpReason==3 && live.trace[0].event==Event::PairingComplete);
  event.type=BLE_GAP_EVENT_ENC_CHANGE;event.enc_change.conn_handle=42;event.enc_change.status=0x403;gap(&event,context());assert(live.error==0x403 && live.smpReason==3 && wire.terminate==1);
  // A local rejection must survive the SM's subsequent rejection notification.
  event.enc_change.status=0x30c;gap(&event,context());assert(live.error==0x403 && wire.terminate==1);
  // UUID bytes copied from the pinned App's Sony service / Bluetooth-base
  // characteristic constants (little endian on NimBLE), independent of uuid().
  ble_uuid128_t remoteService{{BLE_UUID_TYPE_128},{0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x00,0xff,0x00,0xff,0x00,0x80}};
  ble_uuid128_t locationService{{BLE_UUID_TYPE_128},{0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x00,0xdd,0x00,0xdd,0x00,0x80}};
  ble_uuid128_t remoteControl{{BLE_UUID_TYPE_128},{0xfb,0x34,0x9b,0x5f,0x80,0x00,0x00,0x80,0x00,0x10,0x00,0x00,0x01,0xff,0x00,0x00}};
  assert(uuid(&remoteService.u,0xff00,0xff00) && uuid(&locationService.u,0xdd00,0xdd00) && uuid(&remoteControl.u,0xff01,0xff00));
  assert(!uuid(&remoteService.u,0xdd00,0xdd00) && !uuid(&remoteControl.u,0xff02,0xff00));
  std::puts("PASS: actual radio source: App mode 1 MTU/security/500ms ordering, pairing UI replies/stale tokens/cancel, diagnostics, capture/GPS/stop/reconnect");
}
