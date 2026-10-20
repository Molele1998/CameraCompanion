#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <deque>
#include <map>
#include <string>
#include <algorithm>
inline uint64_t testMs=0;
inline int64_t esp_timer_get_time(){return int64_t(testMs*1000);}
inline uint32_t esp_random(){return 234567;}
using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) (void)(x)
#define portEXIT_CRITICAL(x) (void)(x)
constexpr int pdTRUE=1;
struct HostQueue {size_t capacity,size;std::deque<std::vector<uint8_t>> data;};
using QueueHandle_t=HostQueue*;
inline QueueHandle_t xQueueCreate(size_t n,size_t size){return new HostQueue{n,size,{}};}
inline int xQueueSend(HostQueue* q,const void* p,int){if(q->data.size()==q->capacity)return 0;const auto* b=static_cast<const uint8_t*>(p);q->data.emplace_back(b,b+q->size);return 1;}
inline int xQueueReceive(HostQueue* q,void* out,int){if(q->data.empty())return 0;memcpy(out,q->data.front().data(),q->size);q->data.pop_front();return 1;}
inline void xQueueReset(HostQueue* q){q->data.clear();}
class Preferences {
 public:
  std::map<std::string,std::vector<uint8_t>> data;
  bool begin(const char*,bool){return true;}
  size_t getBytesLength(const char* key){return data[key].size();}
  size_t getBytes(const char* key,void* out,size_t n){n=std::min(n,data[key].size());memcpy(out,data[key].data(),n);return n;}
  size_t putBytes(const char* key,const void* p,size_t n){auto* b=static_cast<const uint8_t*>(p);data[key]={b,b+n};return n;}
  bool remove(const char* key){data.erase(key);return true;}
  uint8_t getUChar(const char* key,uint8_t def){return data[key].empty()?def:data[key][0];}
  size_t putUChar(const char* key,uint8_t v){data[key]={v};return 1;}
};
constexpr uint16_t BLE_HS_CONN_HANDLE_NONE=0xffff;
constexpr int BLE_HS_EDONE=14,BLE_HS_EALREADY=2,BLE_HS_ENOMEM=6,BLE_HS_ENOENT=5,BLE_HS_ENOTCONN=7;
constexpr int BLE_HS_FOREVER=0x7fffffff,BLE_ERR_REM_USER_CONN_TERM=19;
constexpr int BLE_UUID_TYPE_16=16,BLE_UUID_TYPE_128=128;
constexpr int BLE_GATT_CHR_PROP_WRITE=8,BLE_GATT_CHR_PROP_NOTIFY=16;
constexpr int BLE_GAP_EVENT_DISC=1,BLE_GAP_EVENT_DISC_COMPLETE=2,BLE_GAP_EVENT_CONNECT=3,BLE_GAP_EVENT_ENC_CHANGE=4;
constexpr int BLE_GAP_EVENT_DISCONNECT=5,BLE_GAP_EVENT_NOTIFY_RX=6,BLE_GAP_EVENT_REPEAT_PAIRING=7,BLE_GAP_EVENT_PASSKEY_ACTION=8;
#define BLE_GAP_EVENT_LINK_ESTAB 38
#define BLE_GAP_EVENT_PARING_COMPLETE 27
constexpr int BLE_GAP_REPEAT_PAIRING_IGNORE=0,BLE_HCI_ADV_RPT_EVTYPE_ADV_IND=0,BLE_HCI_ADV_RPT_EVTYPE_SCAN_RSP=4;
constexpr int BLE_HS_IO_NO_INPUT_OUTPUT=3,BLE_SM_PAIR_KEY_DIST_ENC=1,BLE_SM_PAIR_KEY_DIST_ID=2;
constexpr int BLE_HS_IO_DISPLAY_YESNO=1;
constexpr int BLE_SM_IOACT_NONE=0,BLE_SM_IOACT_DISP=3,BLE_SM_IOACT_NUMCMP=4;
struct ble_sm_io {uint8_t action=0;union {uint32_t passkey;uint8_t numcmp_accept;};};
struct ble_uuid_t {uint8_t type;};
struct ble_uuid16_t {ble_uuid_t u;uint16_t value;};
struct ble_uuid128_t {ble_uuid_t u;uint8_t value[16];};
union ble_uuid_any_t {ble_uuid_t u;ble_uuid16_t u16;ble_uuid128_t u128;};
struct ble_addr_t {uint8_t type=0;uint8_t val[6]{};};
struct os_mbuf {std::vector<uint8_t> bytes;};
#define OS_MBUF_PKTLEN(om) ((om)->bytes.size())
inline int os_mbuf_copydata(os_mbuf* om,size_t offset,size_t n,void* out){if(offset+n>om->bytes.size())return 1;memcpy(out,om->bytes.data()+offset,n);return 0;}
inline os_mbuf* ble_hs_mbuf_from_flat(const void* p,uint16_t n){auto* b=static_cast<const uint8_t*>(p);return new os_mbuf{{b,b+n}};}
struct ble_gatt_error {int status=0;};
struct ble_gatt_svc {uint16_t start_handle=0,end_handle=0;ble_uuid_any_t uuid{};};
struct ble_gatt_chr {uint16_t def_handle=0,val_handle=0;uint8_t properties=0;ble_uuid_any_t uuid{};};
struct ble_gatt_dsc {uint16_t handle=0;ble_uuid_any_t uuid{};};
struct ble_gatt_attr {os_mbuf* om=nullptr;};
struct ble_gap_conn_desc {uint16_t conn_handle=0;ble_addr_t peer_id_addr{};struct {bool encrypted=false,bonded=false,authenticated=false;} sec_state;};
struct ble_gap_disc_desc {ble_addr_t addr{};int8_t rssi=-50;uint8_t event_type=0;const uint8_t* data=nullptr;uint8_t length_data=0;};
struct ble_gap_event {
  int type=0;ble_gap_disc_desc disc;
  struct {int status=0;uint16_t conn_handle=0;} connect,enc_change,link_estab,pairing_complete;
  struct {ble_gap_conn_desc conn;int reason=0;} disconnect;
  struct {uint16_t conn_handle=0,attr_handle=0;os_mbuf* om=nullptr;} notify_rx;
  struct {uint16_t conn_handle=0;struct {uint8_t action=0;uint32_t numcmp=0;} params;} passkey;
};
struct ble_hs_adv_fields {const uint8_t* mfg_data=nullptr;uint8_t mfg_data_len=0;const uint8_t* name=nullptr;uint8_t name_len=0;};
inline int ble_hs_adv_parse_fields(ble_hs_adv_fields* f,const uint8_t* p,uint8_t n){
  for(size_t i=0;i<n;){const size_t len=p[i++];if(!len)break;if(i+len>n)return 1;const uint8_t t=p[i++];
    if(t==255){f->mfg_data=p+i;f->mfg_data_len=uint8_t(len-1);}if(t==9){f->name=p+i;f->name_len=uint8_t(len-1);}i+=len-1;}
  return 0;
}
struct ble_gap_disc_params {uint16_t itvl=0,window=0;uint8_t passive=0,filter_duplicates=0;};
struct ble_npl_event{};struct ble_npl_callout{};struct ble_store_status_event{};
struct HostCfg {void(*sync_cb)()=nullptr;void(*reset_cb)(int)=nullptr;int(*store_status_cb)(ble_store_status_event*,void*)=nullptr;
  int sm_io_cap=0,sm_bonding=0,sm_mitm=0,sm_sc=0,sm_our_key_dist=0,sm_their_key_dist=0;};
inline HostCfg ble_hs_cfg;
struct Wire {uint16_t attr;std::vector<uint8_t> data;bool longWrite;};
struct HostState {int terminate=0,cancel=0,scan=0,connect=0,security=0,timeout=0,deleted=0,mtuRequests=0,serviceRequests=0,ioRequests=0;int nextError=0;
  ble_sm_io io{};bool encrypted=true,bonded=true;uint16_t mtu=23;ble_gap_disc_params scanParams{};std::vector<Wire> writes;};
inline HostState wire;
inline int errorOnce(){int e=wire.nextError;wire.nextError=0;return e;}
inline int ble_gap_terminate(uint16_t,int){++wire.terminate;return 0;}
inline int ble_gap_conn_cancel(){++wire.cancel;return 0;}
inline int ble_gap_disc_cancel(){return 0;}
using GapCb=int(*)(ble_gap_event*,void*);
inline int ble_gap_connect(uint8_t,const ble_addr_t*,int timeout,const void*,GapCb,void*){++wire.connect;wire.timeout=timeout;return errorOnce();}
inline int ble_gap_disc(uint8_t,int,const ble_gap_disc_params* p,GapCb,void*){++wire.scan;wire.scanParams=*p;return errorOnce();}
inline int ble_gap_security_initiate(uint16_t){++wire.security;return errorOnce();}
inline int ble_sm_inject_io(uint16_t,ble_sm_io* io){++wire.ioRequests;wire.io=*io;return errorOnce();}
inline int ble_gap_conn_find(uint16_t,ble_gap_conn_desc* d){d->sec_state.encrypted=wire.encrypted;d->sec_state.bonded=wire.bonded;return 0;}
inline int ble_store_util_delete_peer(const ble_addr_t*){++wire.deleted;return errorOnce();}
inline int ble_hs_util_ensure_addr(int){return 0;}inline int ble_hs_id_infer_auto(int,uint8_t* t){*t=0;return 0;}
using AttrCb=int(*)(uint16_t,const ble_gatt_error*,ble_gatt_attr*,void*);
using SvcCb=int(*)(uint16_t,const ble_gatt_error*,const ble_gatt_svc*,void*);
using ChrCb=int(*)(uint16_t,const ble_gatt_error*,const ble_gatt_chr*,void*);
using DscCb=int(*)(uint16_t,const ble_gatt_error*,uint16_t,const ble_gatt_dsc*,void*);
using MtuCb=int(*)(uint16_t,const ble_gatt_error*,uint16_t,void*);
inline int ble_gattc_exchange_mtu(uint16_t,MtuCb,void*){++wire.mtuRequests;return errorOnce();}
inline int ble_gattc_disc_all_svcs(uint16_t,SvcCb,void*){++wire.serviceRequests;return errorOnce();}
inline int ble_gattc_disc_all_chrs(uint16_t,uint16_t,uint16_t,ChrCb,void*){return errorOnce();}
inline int ble_gattc_disc_all_dscs(uint16_t,uint16_t,uint16_t,DscCb,void*){return errorOnce();}
inline int ble_gattc_read(uint16_t,uint16_t,AttrCb,void*){return errorOnce();}
inline uint16_t ble_att_mtu(uint16_t){return wire.mtu;}
inline int ble_gattc_write_flat(uint16_t,uint16_t attr,const void* p,uint16_t n,AttrCb,void*){const auto* b=static_cast<const uint8_t*>(p);wire.writes.push_back({attr,{b,b+n},false});return errorOnce();}
inline int ble_gattc_write_long(uint16_t,uint16_t attr,uint16_t,os_mbuf* om,AttrCb,void*){wire.writes.push_back({attr,om->bytes,true});delete om;return errorOnce();}
inline int nimble_port_init(){return 0;}inline void nimble_port_run(){}inline void nimble_port_freertos_deinit(){}
inline void nimble_port_freertos_init(void(*)(void*)){}inline void* nimble_port_get_dflt_eventq(){return nullptr;}
inline void ble_store_config_init(){}inline void ble_svc_gap_init(){}inline void ble_svc_gatt_init(){}
inline int ble_svc_gap_device_name_set(const char*){return 0;}inline int ble_att_set_preferred_mtu(uint16_t){return 0;}
inline int ble_npl_time_ms_to_ticks32(int ms){return ms;}
inline int ble_npl_callout_init(ble_npl_callout*,void*,void(*)(ble_npl_event*),void*){return 0;}
inline int ble_npl_callout_reset(ble_npl_callout*,int){return 0;}
