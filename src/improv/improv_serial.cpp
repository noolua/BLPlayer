
#include <Arduino.h>
#include "improv/improv_serial.h"
#include "improv/improv.h"

/*
  https://www.improv-wifi.com/serial/
*/

#define WP_TIMEOUT  20000
#define MAX_NETWORKS  32

typedef struct wifi_network_s{
  char  _ssid[64];
  int   _rssi;
  bool  _auth;
}wifi_network_t;

typedef struct improv_model_s {
  bool _running;
  uint32_t _wifi_provisioning_timeout, _net_count;
  improv_callbacks_t _improv_cb;
  wifi_network_t *_networks;
}improv_model_t;

typedef struct imporv_header_s{
  uint8_t _magic[6];                  // {'I', 'M', 'P', 'R', 'O', 'V'}
  uint8_t _version;                   // IMPROV_SERIAL_VERSION -> 0x01
  uint8_t _type;                      // ImprovSerialType
  uint8_t _payload_sz;                // payload size
  uint8_t _payload[0];                // payload
}imporv_header_t;

typedef struct improv_command_s{
  improv::Command _command;           // improv::Command
  char   _ssid[32];                   // for improv::WIFI_SETTINGS
  char   _passwd[32];                 // for improv::WIFI_SETTINGS
}improv_command_t;

typedef void (*improv_err_cb)(improv::Error);
typedef bool (*improv_command_cb)(const improv_command_t *cmd);

static improv_model_t __model = {0};

static int _build_header(imporv_header_t *header, improv::ImprovSerialType ty){
  header->_magic[0] = 'I';
  header->_magic[1] = 'M';
  header->_magic[2] = 'P';
  header->_magic[3] = 'R';
  header->_magic[4] = 'O';
  header->_magic[5] = 'V';
  header->_version = improv::IMPROV_SERIAL_VERSION;
  header->_type = ty;
  header->_payload_sz = 0; // payload_offset
  return sizeof(imporv_header_t);
}

static void _serial_write_frame(uint8_t *frame){
  imporv_header_t *header = (imporv_header_t*)frame;
  uint8_t checksum = 0x00;
  int frame_sz = header->_payload_sz + sizeof(imporv_header_t);
  for(int i = 0; i < frame_sz; i++){
    checksum += frame[i];
  }
  frame[frame_sz] = checksum; // append checksum
  frame[frame_sz + 1] = '\n'; // append enter
  frame_sz += 2;
  Serial.write(frame, frame_sz);
}

static void _build_error_response(improv::Error err){
  uint8_t frame[32];
  uint32_t pos = 0;
  imporv_header_t *header = (imporv_header_t*)frame;
  // fill header
  pos = _build_header(header, improv::TYPE_ERROR_STATE);

  // fill payload
  frame[pos] = err; // pos = 9 is err
  pos += 1;
  // set payload size
  header->_payload_sz = (uint8_t)(pos - sizeof(imporv_header_t));
  
  _serial_write_frame(frame);
}

static void _build_state_response(improv::State state){
  uint8_t frame[32];
  uint32_t pos = 0;
  imporv_header_t *header = (imporv_header_t*)frame;
  // fill header
  pos = _build_header(header, improv::TYPE_CURRENT_STATE);

  // fill payload
  frame[pos] = state; // pos = 9 is err
  pos += 1;
  // set payload size
  header->_payload_sz = (uint8_t)(pos - sizeof(imporv_header_t));
  
  _serial_write_frame(frame);
}


static void _build_rpc_response(improv::Command cmd, const char **sstrings, const int string_num) {
  uint8_t frame[300];
  uint32_t pos = 0;
  imporv_header_t *header = (imporv_header_t*)frame;
  // fill header
  pos = _build_header(header, improv::TYPE_RPC_RESPONSE);
  // fill payload
  frame[pos] = cmd; // pos = 9 is command
  pos += 1;
  frame[pos] = 0;   // pos = 10 is command_result_sz
  pos += 1;

  for(int i=0; i < string_num && sstrings; i++){
    if(sstrings[i]){
      char str_len = strlen(sstrings[i]);
      if(str_len > 0) {
        frame[pos++] = str_len;
        memcpy(&frame[pos], sstrings[i], str_len);
        pos += str_len;
      }
    }
  }
  // set cmd_sz
  frame[sizeof(imporv_header_t) + 1] = (uint8_t)(pos - (sizeof(imporv_header_t) + 2));

  // set payload size
  header->_payload_sz = (uint8_t)(pos - sizeof(imporv_header_t));
  
  _serial_write_frame(frame);
}

static void _on_imporv_error(improv::Error err){
  _build_error_response(err);
}

static bool _on_improv_command(const improv_command_t *cmd) {
  switch (cmd->_command){
  case improv::WIFI_SETTINGS: {
      // _build_state_response(improv::STATE_PROVISIONING);
      int ret = __model._improv_cb.wifi_begin(cmd->_ssid, cmd->_passwd);
      __model._wifi_provisioning_timeout = millis() + WP_TIMEOUT;
      _build_state_response(improv::STATE_PROVISIONING);
    }
    break;
  case improv::GET_CURRENT_STATE:{
      if(__model._improv_cb.wifi_isconnected() == 0)
        _build_state_response(improv::STATE_PROVISIONED);
      else if(__model._wifi_provisioning_timeout > millis())
        _build_state_response(improv::STATE_PROVISIONING);
      else
        _build_state_response(improv::STATE_AUTHORIZED);
    }
    break;
  case improv::GET_DEVICE_INFO:{
      char firmware[16], chip[16], version[16], name[16];
      __model._improv_cb.device_info(firmware, chip, name, version);
      const char *decvice_info[] = {
        firmware, version, chip, name
      };
      _build_rpc_response(improv::GET_DEVICE_INFO, decvice_info, 4);
    }
    break;
  case improv::GET_WIFI_NETWORKS: {
      if(__model._networks) {
        char rssi[64];
        for(int i = 0; i < __model._net_count; i++){
          wifi_network_t *net = &__model._networks[i];
          sprintf(rssi, "%d", net->_rssi);
          const char *networks[] = {
            net->_ssid, rssi, net->_auth ? "YES" : "NO"
          };
          _build_rpc_response(improv::GET_WIFI_NETWORKS, networks, 3);
        }
      }      
      _build_rpc_response(improv::GET_WIFI_NETWORKS, NULL, 0);
    }
    break;
  case improv::BAD_CHECKSUM: {
    _build_error_response(improv::ERROR_INVALID_RPC);
  }
    break;
  default:
    _build_error_response(improv::ERROR_UNKNOWN_RPC);
    break;
  }
  return true;
}

bool _parse_improv_command(improv_command_t *cmd, const uint8_t *data, size_t length, bool check_checksum) {
  improv::Command command = (improv::Command) data[0];
  uint8_t data_length = data[1];
  
  if(!cmd) return false;

  if (data_length != length - 2 - check_checksum) {
    cmd->_command = improv::UNKNOWN;
    return true;
  }

  if (check_checksum) {
    uint8_t checksum = data[length - 1];

    uint32_t calculated_checksum = 0;
    for (uint8_t i = 0; i < length - 1; i++) {
      calculated_checksum += data[i];
    }

    if ((uint8_t) calculated_checksum != checksum) {
      cmd->_command = improv::BAD_CHECKSUM;
      return true;
    }
  }

  if (command == improv::WIFI_SETTINGS) {
    uint8_t ssid_length = data[2];
    uint8_t ssid_start = 3;
    uint8_t ssid_end = ssid_start + ssid_length;
    uint8_t pass_length = data[ssid_end];
    uint8_t pass_start = ssid_end + 1;
    uint8_t max_ssid = sizeof(cmd->_ssid);
    uint8_t max_pass = sizeof(cmd->_passwd);

    memset(cmd->_ssid, 0, max_ssid);
    memset(cmd->_passwd, 0, max_pass);
    memcpy(cmd->_ssid, data + ssid_start, max_ssid < ssid_length ? max_ssid : ssid_length);
    cmd->_ssid[max_ssid - 1] = 0;
    memcpy(cmd->_passwd, data + pass_start, max_pass < pass_length ? max_pass : pass_length);
    cmd->_passwd[max_pass - 1] = 0;
    cmd->_command = command;
    return true;
  }

  cmd->_command = command;
  return true;
}

bool _parse_improv_serial_byte(size_t position, uint8_t byte, const uint8_t *buffer, improv_command_cb command_cb, improv_err_cb err_cb) {
  if (position == 0)
    return byte == 'I';
  if (position == 1)
    return byte == 'M';
  if (position == 2)
    return byte == 'P';
  if (position == 3)
    return byte == 'R';
  if (position == 4)
    return byte == 'O';
  if (position == 5)
    return byte == 'V';

  if (position == 6){
    return byte == improv::IMPROV_SERIAL_VERSION;
  }

  if (position <= 8)
    return true;

  uint8_t type = buffer[7];
  uint8_t data_len = buffer[8];

  if (position <= 8 + data_len)
    return true;

  if (position == 8 + data_len + 1) {
    uint8_t checksum = 0x00;
    for (size_t i = 0; i < position; i++)
      checksum += buffer[i];

    if (checksum != byte && err_cb) {
      err_cb(improv::ERROR_INVALID_RPC);
      return false;
    }

    if (type == improv::TYPE_RPC && command_cb) {
      improv_command_t cmd;
      if(_parse_improv_command(&cmd, &buffer[9], data_len, false))
        return command_cb(&cmd);
    }
  }

  return false;
}

static void _improv_serial_task(void *pvParameters){
  int32_t buff_used = 0;
  uint32_t ts_read = 0, ms_now = 0, ts_second = 0;
  uint8_t buffer[300];
  while(1) {
    volatile bool until = __model._running;
    if(!until) break;
    
    ms_now = millis();
    if(ts_second < ms_now) {
      ts_second = ms_now + 1000;
      if(__model._wifi_provisioning_timeout > ms_now){
        if(__model._improv_cb.wifi_isconnected() == 0) {
          // const char *urls[] = {"https://iot.fadai8.cn"};
          __model._wifi_provisioning_timeout = 0;
          _build_state_response(improv::STATE_PROVISIONED);
          _build_rpc_response(improv::WIFI_SETTINGS, NULL, 0);
        }
      }
      // timeout, failed wifi connect
      if(__model._wifi_provisioning_timeout && __model._wifi_provisioning_timeout < ms_now){
        __model._wifi_provisioning_timeout = 0;
        _build_error_response(improv::ERROR_UNABLE_TO_CONNECT);
        _build_state_response(improv::STATE_AUTHORIZED);
      }
    }

    if(!Serial.available()){
      vTaskDelay(10/portTICK_PERIOD_MS);
      continue;
    }    
    
    // time expired check, max bytes frame ->  1/115200.0 * 8 * 300 = 0.020833 (s) < 30 ms
    if(ts_read < ms_now || buff_used >= 300){
      buff_used = 0;
      ts_read = ms_now + 50;
    }

    buffer[buff_used++] = Serial.read();
    if(false == _parse_improv_serial_byte(buff_used - 1, buffer[buff_used - 1], buffer, _on_improv_command, _on_imporv_error))
      buff_used = 0;
  }

  __model._running = false;
  if(__model._networks){
    free(__model._networks);
    __model._networks = NULL;
    __model._net_count = 0;
  }
  vTaskDelete(NULL);
}

void ImprovSerial::begin(improv_callbacks_t *cb) {
  if(!cb) return;
  if(__model._running) return;

  if(!__model._networks) 
    __model._networks = (wifi_network_t *)malloc(sizeof(wifi_network_t) * MAX_NETWORKS);
  __model._net_count = 0;
  __model._running = true;
  __model._improv_cb.wifi_begin = cb->wifi_begin;
  __model._improv_cb.wifi_isconnected = cb->wifi_isconnected;
  __model._improv_cb.device_info = cb->device_info;
  xTaskCreatePinnedToCore(_improv_serial_task, "_improv_serial_task", 2048, NULL, 5, NULL, 0);
}

void ImprovSerial::store_one_wifi_network(const char *ssid, int rssi, const bool auth) {
  if(ssid && __model._running && __model._networks && __model._net_count < MAX_NETWORKS){
    wifi_network_t *net = &__model._networks[__model._net_count];
    sprintf(net->_ssid, "%s", ssid);
    net->_rssi = rssi;
    net->_auth = auth;
    __model._net_count++;
  }
}

void ImprovSerial::stop(){
  if(__model._running) {
    __model._running = false;
  }
  if(__model._networks){
    free(__model._networks);
    __model._networks = NULL;
    __model._net_count = 0;
  }
}