#define CAMERA_HOST_TEST 1
#include "../CameraCompanion/CameraRadio.cpp"
#include <cassert>
using namespace CameraRadio;
void clean(){live=Snapshot();published=Snapshot();capture=Camera::Capture();h=Handles();clearOp();
  clearControls();holdExpires=0;
  conn=BLE_HS_CONN_HANDLE_NONE;wanted=connecting=wasReady=watching=watchFound=disconnectWanted=false;retries=0;
  retryAt=0;generation=10;testMs=10000;lastSimSample=UINT64_MAX;wire=HostState();stopWanted=false;xQueueReset(commands);}
void ready(){clean();conn=42;live.session=generation;live.link=Link::Ready;live.encrypted=live.remote=live.location=live.notifications=true;h.control=10;h.status=11;h.loc=20;publish();}
void ack(int status=0){ble_gatt_error e{};e.status=status;wrote(conn,&e,nullptr,context());}
void mtuAck(int status=0){ble_gatt_error e{};e.status=status;mtuDone(conn,&e,128,context());}
void notify(std::initializer_list<uint8_t> bytes,uint16_t attr=11,uintptr_t gen=0){os_mbuf om{{bytes}};ble_gap_event e;e.type=BLE_GAP_EVENT_NOTIFY_RX;e.notify_rx={conn,attr,&om};gap(&e,gen?reinterpret_cast<void*>(gen):context());}
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
  // A write ACK never creates a movie state. A genuine current-link FF02
  // notification can arrive before the button's UP ACK and must be retained.
  ready();assert(requestRecord());tick(nullptr);assert(op==Op::Button && wire.writes.back().data==std::vector<uint8_t>({1,0x0f}));
  ack();assert(live.video==Camera::Video::Unknown && live.recordPending);
  notify({2,0xd5,0x20},99);notify({2,0xd5,0x20},11,generation-1);notify({2,0xd5,0x21});notify({2,0xd5});notify({1,0xd5,0x20});assert(live.video==Camera::Video::Unknown);
  notify({2,0xd5,0x20});const auto observed=live.videoSince;assert(live.video==Camera::Video::Recording && live.recordPending && observed==testMs);
  testMs+=121;tick(nullptr);assert(wire.writes.back().data==std::vector<uint8_t>({1,0x0e}));ack();tick(nullptr);assert(!live.recordPending && live.video==Camera::Video::Recording);
  notify({2,0xd5,0x20});assert(live.videoSince==observed);notify({2,0xd5,0});assert(live.video==Camera::Video::Stopped && live.videoSince==0);
  // Physical REC updates are visible even with no pending board command.
  publish();assert(published.video==Camera::Video::Stopped && published.lastVideoNotify==testMs);
  assert(requestRecord());notify({2,0xd5,0x20});tick(nullptr);assert(wire.writes.size()==2 && live.error==-62); // State changed while queued, no accidental stop.
  ready();notify({2,0xd5,0});publish();live.sim=true;nextGps=0;tick(nullptr);assert(op==Op::Location);
  assert(requestRecord());tick(nullptr);assert(live.recordPending && !remotePress.downIssued);notify({2,0xd5,0x20});ack();tick(nullptr);
  assert(wire.writes.size()==1 && !live.recordPending && live.video==Camera::Video::Recording && live.error==-62); // State changed while ATT was busy, still no toggle.
  // No notification -> unknown, no automatic retry or assumed success.
  ready();assert(requestRecord());tick(nullptr);ack();testMs+=121;tick(nullptr);ack();testMs+=6000;tick(nullptr);
  assert(!live.recordPending && live.video==Camera::Video::Unknown && live.error==-61 && wire.writes.size()==2);
  testMs+=10000;tick(nullptr);assert(wire.writes.size()==2);notify({2,0xd5,0x20});assert(live.video==Camera::Video::Recording);
  ready();notify({2,0xd5,0});publish();assert(requestRecord());tick(nullptr);ack();notify({2,0xd5,0});testMs+=121;tick(nullptr);ack();tick(nullptr);assert(live.recordPending); // Unchanged status is not a start confirmation.
  // A failed write may have reached the peer: release it; never toggle again.
  ready();assert(requestRecord());tick(nullptr);ack(5);tick(nullptr);assert(wire.writes.size()==2 && wire.writes.back().data[1]==0x0e && !live.recordPending);ack();tick(nullptr);assert(live.video==Camera::Video::Unknown);
  ready();assert(pressRemote(Camera::RemoteKey::C1));tick(nullptr);ack();testMs+=121;tick(nullptr);ack(5);assert(wire.terminate==1 && !live.session);
  // Every custom/lens key uses the same sequencer and its own UP bytes.
  for(auto key:{Camera::RemoteKey::AfOn,Camera::RemoteKey::C1,Camera::RemoteKey::FocusNear,Camera::RemoteKey::FocusFar,Camera::RemoteKey::ZoomWide,Camera::RemoteKey::ZoomTele}){
    ready();assert(pressRemote(key));tick(nullptr);assert(remotePress.key==key);ack();testMs+=121;tick(nullptr);uint8_t bytes[3]{};const size_t n=Camera::remotePayload(key,false,bytes);
    assert(wire.writes.back().data==std::vector<uint8_t>(bytes,bytes+n));ack();tick(nullptr);assert(!remotePress.busy());}
  ready();const auto hold=holdRemote(Camera::RemoteKey::FocusNear);assert(hold);tick(nullptr);ack();testMs+=200;keepRemoteHeld(hold);tick(nullptr);assert(wire.writes.size()==1);
  for(int i=0;i<12;++i)assert(request(Command::SetSim));assert(!request(Command::SetSim));releaseRemote(hold);tick(nullptr);
  assert(op==Op::Button && wire.writes.back().data==std::vector<uint8_t>({2,0x6a,0}));ack();tick(nullptr);assert(!remotePress.busy());
  // Release before the host gets to the queue, or an expired lease, must not
  // briefly run the lens later. Lost renewals stop an active hold as well.
  ready();auto tokenHold=holdRemote(Camera::RemoteKey::ZoomTele);releaseRemote(tokenHold);tick(nullptr);assert(wire.writes.empty());
  ready();tokenHold=holdRemote(Camera::RemoteKey::ZoomTele);testMs+=401;tick(nullptr);assert(wire.writes.empty());
  ready();tokenHold=holdRemote(Camera::RemoteKey::ZoomTele);tick(nullptr);ack();testMs+=401;tick(nullptr);assert(wire.writes.back().data==std::vector<uint8_t>({2,0x44,0}));ack();
  ready();tokenHold=holdRemote(Camera::RemoteKey::AfOn);tick(nullptr);ack();
  for(int i=0;i<50;++i){testMs+=200;keepRemoteHeld(tokenHold);tick(nullptr);}assert(wire.writes.size()==2 && remotePress.pending==Camera::KeyStep::Up);ack();
  // Stop has a path outside the ordinary queue, including an outstanding DOWN.
  ready();tokenHold=holdRemote(Camera::RemoteKey::C1);tick(nullptr);stop();tick(nullptr);assert(op==Op::Button);ack();tick(nullptr);assert(wire.writes.back().data==std::vector<uint8_t>({1,0x20}));ack();
  // Capture and new control are mutually exclusive; lens keys remain usable
  // while recording. Location writes wait for release, then resume normally.
  ready();capture.start(testMs,plan);assert(requestRecord());tick(nullptr);assert(!live.recordPending && remotePress.key==Camera::RemoteKey::None);
  ready();capture.phase=Camera::Phase::Paused;handle({Command::Single,0,plan});assert(capture.phase==Camera::Phase::Paused);
  ready();assert(pressRemote(Camera::RemoteKey::C1));tick(nullptr);handle({Command::Single,0,plan});assert(!captureOccupied());ack();
  ready();notify({2,0xd5,0x20});handle({Command::Start,0,plan});assert(!captureOccupied());publish();assert(pressRemote(Camera::RemoteKey::FocusFar));tick(nullptr);assert(remotePress.busy());
  ready();plan.count=10;capture.start(testMs,plan);notify({2,0xd5,0x20});assert(capture.phase==Camera::Phase::Stopping);tick(nullptr);assert(wire.writes.back().data[1]==8);ack();notify({2,0xd5,0x20});tick(nullptr);assert(wire.writes.back().data[1]==6);ack();assert(!capture.running());
  ready();live.sim=true;nextGps=0;tokenHold=holdRemote(Camera::RemoteKey::ZoomWide);tick(nullptr);ack();testMs+=200;keepRemoteHeld(tokenHold);tick(nullptr);assert(wire.writes.size()==1);
  releaseRemote(tokenHold);tick(nullptr);ack();tick(nullptr);assert(op==Op::Location && wire.writes.back().attr==20);
  // Connection generations protect both queued actions and real status. New
  // connections start UNKNOWN, and reconnect never replays an old button.
  ready();assert(requestRecord());assert(pressRemote(Camera::RemoteKey::C1));tokenHold=holdRemote(Camera::RemoteKey::FocusFar);
  event=ble_gap_event();event.type=BLE_GAP_EVENT_DISCONNECT;event.disconnect.conn.conn_handle=42;gap(&event,context());
  conn=42;live.session=generation;live.link=Link::Ready;live.remote=true;tick(nullptr);assert(wire.writes.empty() && live.video==Camera::Video::Unknown && !remotePress.busy());
  ready();tokenHold=holdRemote(Camera::RemoteKey::ZoomWide);tick(nullptr);ack();handle({Command::Disconnect,0,plan});tick(nullptr);assert(wire.terminate==0 && wire.writes.back().data[1]==0x46);ack();tick(nullptr);ack();tick(nullptr);ack();tick(nullptr);assert(wire.terminate==1);
  std::puts("PASS: actual radio source: mode 1 pairing + movie notifications/ACK distinction/timeout/stale link/state + all remote keys/lease/release/queue/stop/GPS/capture/reconnect");
}
