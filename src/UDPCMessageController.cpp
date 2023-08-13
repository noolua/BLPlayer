#include <Arduino.h>
#include <WiFiUdp.h>
#include <lwip/netdb.h>
#include <rom/crc.h>
#include "UDPMessageController.h"

#define SIGN_DEFAULT      0xB0B2BEEF  // bob love beef
#define FRAME_MAGIX       (0x3728)    // 帧的Magic
#define MAX_XNET_FRAME    (1024)      // 最大帧的字节数
#define TS_NEXT_REQ       (2000)      // 轮询的时间间隔毫秒
#define FRAME_EXPIRED     (15)        // 数据帧的有效时间15秒
#define SVR_ADDRESS       IPAddress(10, 1, 2, 247) // FOR TEST

#define SVR_PORT          (60499)

typedef struct xnet_header_t{
  uint16_t _frame_sz;     // 整个数据帧的大小，包含头部4字节
  uint16_t _magic;        // 数据帧的特征码
  uint32_t _signature;    // 数据帧的签名值
  uint32_t _timestamp;    // 数据帧的发出时间
  uint8_t  _unique_id[8]; // 设备id
}xnet_header_t;

typedef struct xnet_entity_t{
  xnet_header_t _header;
  xnet_message_t _message;
  xnet_entity_t(u_int16_t opcode){
    build_header(opcode);
  }
  void build_header(uint16_t opcode, uint16_t uparam=0, uint16_t wparam=0, uint16_t payload_sz=0){
    _header._frame_sz = sizeof(struct xnet_entity_t) + payload_sz;
    _header._magic = FRAME_MAGIX;
    _header._signature = 0U;
    _header._timestamp = 0U;
    _message._opcode = opcode;
    _message._uparam = uparam;
    _message._wparam = wparam;
    _message._payload_sz = payload_sz;
    memset(_header._unique_id, 0, sizeof(_header._unique_id));
    esp_efuse_mac_get_default(_header._unique_id);    
  }
  void sign(){
    _header._timestamp = XNetController::utc() / 1000;
    _header._signature = SIGN_DEFAULT;
    uint32_t crc32 = crc32_le(0, (const uint8_t *)&_header._frame_sz, _header._frame_sz);
    _header._signature = crc32;
    _header._timestamp ^= _header._signature;
  }
  /**
   * @brief 校验时间和签名是否有效，签名是对整个数据帧进行crc32校验
   * 
   * @return true 
   * @return false 
   */
  bool verify() {
    uint32_t crc32 = _header._signature, hash;
    uint32_t ts_now = XNetController::utc() / 1000;
    _header._timestamp ^= _header._signature;
    _header._signature = SIGN_DEFAULT;
    hash = crc32_le(0, (const uint8_t *)&_header._frame_sz, _header._frame_sz);
    
    if(_message._opcode == XNET_ACK_TIMESTAMP){
      return hash == crc32 
      && _header._magic == FRAME_MAGIX 
      && _header._frame_sz == sizeof(struct xnet_entity_t) + _message._payload_sz;
    }
    return hash == crc32 
    && _header._magic == FRAME_MAGIX 
    && _header._timestamp > ts_now ? (_header._timestamp - ts_now) < FRAME_EXPIRED : (ts_now - _header._timestamp) < FRAME_EXPIRED
    && _header._frame_sz == sizeof(struct xnet_entity_t) + _message._payload_sz;
  }
}xnet_entity_t;

typedef struct xnet_runtime_t{
  uint8_t _buffer[MAX_XNET_FRAME];
  WiFiUDP _svr_udp;
  IPAddress _svr_addr;
  uint16_t _svr_port;
  xnet_message_handler_cb_t _handler;
  uint32_t _ts_next_query;
  uint64_t _utc_timestamp_ms;            // 毫秒精度的utc
}xnet_runtime_t;

static xnet_runtime_t _xnet;

static void _xnet_send_frame(xnet_entity_t *frame){
  if(!frame) return;
  frame->sign();
  _xnet._svr_udp.beginPacket(_xnet._svr_addr, _xnet._svr_port);
  _xnet._svr_udp.write((const uint8_t*)&frame->_header, frame->_header._frame_sz);
  _xnet._svr_udp.endPacket();
}

void XNetController::setup(xnet_message_handler_cb_t handler){
  _xnet._handler = handler;
  _xnet._ts_next_query = 0;
  _xnet._svr_addr = SVR_ADDRESS;
  _xnet._svr_port = SVR_PORT;
  struct hostent* he = gethostbyname("iot.fadai8.cn");
  if(he){
    struct in_addr retAddr = *(struct in_addr*) (he->h_addr_list[0]);
    _xnet._svr_addr = IPAddress(retAddr.s_addr);
  }
}

void XNetController::loop(){
  uint32_t ts_now = millis();
  int cb = _xnet._svr_udp.parsePacket();
  if(cb > 0){
    cb = _xnet._svr_udp.read(_xnet._buffer, MAX_XNET_FRAME);
    if(cb >= sizeof(xnet_entity_t)){
      xnet_entity_t * entity = (xnet_entity_t *)_xnet._buffer;
      if(entity->verify()){
        if(_xnet._handler){
          _xnet._handler(&entity->_message);
        }
        if(entity->_message._opcode == XNET_ACK_TIMESTAMP && entity->_message._payload_sz == sizeof(uint64_t)){
          _xnet._utc_timestamp_ms = *(uint64_t*)entity->_message._payload - millis();
          // uint32_t l32, h32;
          // l32 = *(uint32_t*)entity->_message._payload;
          // h32 = *(uint32_t*)(entity->_message._payload+4);
          // Serial.printf("update _utc_timestamp_ms %08x, %08x\n", l32, h32);
          // _xnet._utc_timestamp_ms =  (uint64_t)l32 | (uint64_t)h32 << 32;
          // _xnet._utc_timestamp_ms -= millis();
          
        }
      }else{
        Serial.println("ERROR entity->verify");
      }
    }
  }
  if(_xnet._ts_next_query < ts_now){
    _xnet._ts_next_query = ts_now + TS_NEXT_REQ;
    xnet_entity_t frame(XNET_REQ_COMMAND);
    _xnet_send_frame(&frame);
  }
}

void XNetController::req_utc(){
  xnet_entity_t frame(XNET_REQ_TIMESTAMP);
  _xnet_send_frame(&frame);
}

void XNetController::req_new_music(bool new_channel){
  xnet_entity_t frame(XNET_REQ_NEW_MUSIC);
  frame._message._uparam = new_channel == false ? 0 : 1;
  _xnet_send_frame(&frame);
}

void XNetController::req_update_state(uint16_t volume, bool pause){
  xnet_entity_t frame(XNET_REQ_UPDATE_STATE);
  frame._message._uparam = volume > 100 ? 100 : volume;
  frame._message._wparam = pause == false ? 0 : 1;
  _xnet_send_frame(&frame);
}

void XNetController::req_digital_access(){
  xnet_entity_t frame(XNET_REQ_DIGITAL_ACCESS);
  _xnet_send_frame(&frame);
}

uint64_t XNetController::utc(){
  return millis() + _xnet._utc_timestamp_ms;
}
