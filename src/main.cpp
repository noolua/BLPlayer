#ifndef __MAIN_CPP__
#define __MAIN_CPP__

#include <Arduino.h>
#include <ezButton.h>
#include <WiFi.h>
#include <nvs_flash.h>
#include <esp_log.h>

#include "version.h"
#include "wifi_config.h"
#include "AudioFileSourcePROGMEM.h"
#include "AudioFileSourceHTTPStreamV2.h"
#include "AudioFileSourceBufferEx.h"
#include "AudioFileDigitalTokens.h"
#include "AudioGeneratorAAC.h"
#include "AudioGeneratorMP3.h"
#include "improv/improv_serial.h"

// #define HAVE_USE_MCUMON
#include "mcumon.h"
#include "UDPMessageController.h"

#undef millis
#define millis XNetController::millis

typedef AudioFileSourceBufferEx       MyAudioFileSourceBuffer;
typedef AudioFileSourceHTTPStreamV2   MyHttpFileSource;


McuMonitor __monitor;

#define NV_BUNDLE     "BLPLAYER"
#define NV_BD_SSID    "BL.SSID"
#define NV_BD_PWD     "BL.PWD"
#define NV_BD_DIGITAL "BL.DIGI"
#define NV_BD_ERASE   "BL.ERASE"
#define NV_BD_SSID_DEF    WIFI_SSID_DEFAULT
#define NV_BD_PWD_DEF     WIFI_PWD_DEFAULT
#define NV_BD_DIGITAL_DEF 0
#define NV_BD_ERASE_DEF   0

class NVSetting{
  static char ssid[32];
  static char password[32];
  static int32_t erase_on_boot;
  static int32_t fresh_digital;
public:
  static bool need_commit;
  static void load_setting(){
    nvs_handle handle;
    size_t str_length = 0;
    esp_err_t err;
    memset(ssid, 0, sizeof(ssid));
    memset(password, 0, sizeof(password));

    strcpy(ssid, NV_BD_SSID_DEF);
    strcpy(password, NV_BD_PWD_DEF);
    erase_on_boot = NV_BD_ERASE_DEF;
    fresh_digital = NV_BD_DIGITAL_DEF;
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      Serial.println("nvs_init failed, erase it.");
      ESP_ERROR_CHECK(nvs_flash_erase());
      err = nvs_flash_init();
    }
    if(nvs_open(NV_BUNDLE, NVS_READONLY, &handle) == ESP_OK){
      Serial.println("load setting");
      str_length = sizeof(ssid);
      nvs_get_str(handle, NV_BD_SSID, ssid, &str_length);
      str_length = sizeof(password);
      nvs_get_str(handle, NV_BD_PWD, password, &str_length);
      nvs_get_i32(handle, NV_BD_ERASE, &erase_on_boot);
      nvs_get_i32(handle, NV_BD_DIGITAL, &fresh_digital);
      nvs_close(handle);
    }
  }

  static int save_setting(){
    nvs_handle handle;
    esp_err_t err;
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      Serial.println("nvs_init failed, erase it.");
      ESP_ERROR_CHECK(nvs_flash_erase());
      err = nvs_flash_init();
    }
    if(nvs_open(NV_BUNDLE, NVS_READWRITE, &handle) == ESP_OK){
      Serial.println("save setting");
      nvs_set_str(handle, NV_BD_SSID, ssid);
      nvs_set_str(handle, NV_BD_PWD, password);
      nvs_set_i32(handle, NV_BD_DIGITAL, fresh_digital);
      nvs_set_i32(handle, NV_BD_ERASE, erase_on_boot);
      err = nvs_commit(handle);
      nvs_close(handle);
    }
    return err == ESP_OK ? 0 : -1;
  }

  static int32_t get_erase_on_boot(){
    return erase_on_boot;
  }

  static int32_t set_erase_on_boot(int32_t v){
    erase_on_boot = v;
    save_setting();
    return v;
  }

  static int32_t get_fresh_digital(){
    return fresh_digital;
  }

  static int32_t set_fresh_digital(int32_t v){
    fresh_digital = v;
    save_setting();
    return v;
  }

  static const char *get_ssid(){
    return ssid;
  }

  static const char *get_password(){
    return password;
  }

  static int set_ssid_password(const char *ssid_, const char *password_){
    strncpy(ssid, ssid_, sizeof(ssid));
    strncpy(password, password_, sizeof(password));
    need_commit = true;
    return 0;
  }
};

char NVSetting::ssid[32];
char NVSetting::password[32];
int32_t NVSetting::erase_on_boot = 0;
int32_t NVSetting::fresh_digital = 0;
bool NVSetting::need_commit = false;

#ifndef ESP32_A2DP_SOURCE
#include "AudioOutputI2S.h"

#ifdef CONFIG_IDF_TARGET_ESP32C3
#define BOOT_PIN        (9)
#ifdef ESPGJOYMODEL
  #define I2S_BCK         12
  #define I2S_LRCLK       13
  #define I2S_SDA         1
  #define TFT_RST         10
#else
  #define I2S_BCK         0
  #define I2S_LRCLK       1
  #define I2S_SDA         12
#endif

#endif // CONFIG_IDF_TARGET_ESP32C3

#ifdef CONFIG_IDF_TARGET_ESP32
#define BOOT_PIN        (23)
#define I2S_BCK         15
#define I2S_LRCLK       2
#define I2S_SDA         4
#endif // CONFIG_IDF_TARGET_ESP32

#ifdef CONFIG_IDF_TARGET_ESP32S3
#define BOOT_PIN        (0)
#define I2S_BCK         39
#define I2S_LRCLK       40
#define I2S_SDA         41
#endif // CONFIG_IDF_TARGET_ESP32S3

#ifdef ESPGJOYMODEL
#define OUTPUT_SETPIN(x) do{x.SetPinout(I2S_BCK, I2S_LRCLK, I2S_SDA); pinMode(TFT_RST, OUTPUT); delay(100); digitalWrite(TFT_RST, HIGH);}while(0)
#else
#define OUTPUT_SETPIN(x) do{x.SetPinout(I2S_BCK, I2S_LRCLK, I2S_SDA); x.SetLsbJustified(true);}while(0)
#endif

class MyAudioOutput: public AudioOutputI2S{
  public:
    MyAudioOutput(){
      _paused = false;
    }
    ~MyAudioOutput(){}
    bool SetPause(bool enable){
      _paused = enable;
      return _paused;
    }
    void GetGainAndPause(uint16_t *volume, bool *pause){
      uint16_t v = (gainF2P6 >> 6) * 100 + (100.0f/64.0f) * (gainF2P6 & 0b00111111);
      if (v > 100) v = 100;
      *volume = v;
      *pause = _paused;
    }
    virtual bool ConsumeSample(int16_t sample[2]){
      if(_paused){
        int16_t zsample[2] = {0, 0};
        AudioOutputI2S::ConsumeSample(zsample);
        return false;
      }
      return AudioOutputI2S::ConsumeSample(sample);
    }
  protected:
    bool _paused;    
};

MyAudioOutput output;
static int auto_refresh_digital_token = 0;
#endif // ESP32_A2DP_SOURCE

#ifdef ESP32_A2DP_SOURCE
#define BOOT_PIN    (0)

#include "BluetoothA2DPSource.h"

class A2DSourceOutput : public AudioOutput
{
  enum{FRAME_SZ = 1536};
  public:
    A2DSourceOutput(){
      _begined = false;
      _pushed = 0;
      _poped = 0;
      __monitor.register_monitor_message(this, (obj_monitor_message_cb)&A2DSourceOutput::monitor_message);
    }
    virtual bool begin(){
      _pushed = 0;
      _poped  = 0;
      _ts_poped = 0;
      _ts_begin = millis();
      _ts_wait_full = _ts_begin + 1000;
      _paused = false;
      _begined = true;
      return true;
    }
    bool SetPause(bool enable){
      _paused = enable;
      return _paused;
    }
    bool SetLsbJustified(bool lsb){
      return true;
    }
    void GetGainAndPause(uint16_t *volume, bool *pause){
      uint16_t v = (gainF2P6 >> 6) * 100 + (100.0f/64.0f) * (gainF2P6 & 0b00111111);
      if (v > 100) v = 100;
      *volume = v;
      *pause = _paused;
    }
    virtual bool ConsumeSample(int16_t sample[2]){
      if(_pushed < _poped + FRAME_SZ){
        int idx = _pushed % FRAME_SZ;
        _frames[idx].channel1 = Amplify(sample[0]);
        _frames[idx].channel2 = Amplify(sample[1]);
        _pushed++;
        return true;
      }
      return false;
    }
    int monitor_message(char *message, int message_len){
      uint32_t ts_timeout = millis() - _ts_begin;
      if(ts_timeout > 60000){
        if(_paused == false && _poped - _ts_poped == 0 && _begined == true){
          Serial.println("one minitue, no samples consume, reboot ...");
          ESP.restart();
        }
        ts_timeout = 1;
        _ts_poped = _poped;
        _ts_begin = millis();
      }
      float audio_freq = (_poped-_ts_poped) / (ts_timeout/1000.0f);
      if(audio_freq < 32000 && _pushed - _poped < 512 && _begined == true){
        _ts_wait_full = millis() + 1000;
        Serial.printf(" _ts_wait_fill_samples 1second ... ");
      }
      return snprintf(message, message_len, ", audio: (%5.0fhz, %4d)", audio_freq, _pushed - _poped);
    }
    int32_t PopFrames(Frame *frame, int32_t frame_sz){
      int32_t can_poped_sz = _pushed - _poped;
      if(can_poped_sz >= frame_sz && _ts_wait_full < millis() && _paused == false){
        int32_t idx = _poped % FRAME_SZ;
        if(idx + frame_sz < FRAME_SZ){
          memcpy(&frame[0], &_frames[idx], sizeof(Frame) * frame_sz);
        }else{
          int32_t frame2end_sz = FRAME_SZ - idx;
          memcpy(&frame[0], &_frames[idx], sizeof(Frame) * frame2end_sz);
          memcpy(&frame[frame2end_sz], &_frames[0], sizeof(Frame) * (frame_sz - frame2end_sz));
        }
        _poped += frame_sz;
      }
      return frame_sz;
    }
    virtual bool stop(){
      _poped = 0;
      _pushed = 0;
      _begined = false;
      return true;
    }
  protected:
    Frame _frames[FRAME_SZ];
    int32_t _pushed, _poped;
    uint64_t _ts_begin, _ts_poped, _ts_wait_full;
    bool _paused, _begined;
};

typedef A2DSourceOutput  MyAudioOutput;

BluetoothA2DPSource a2dp_source;
MyAudioOutput output;
#define OUTPUT_SETPIN(x) NULL


int32_t a2dp_fill_samples_cb(Frame *frames, int32_t frame_sz){
  output.PopFrames(frames, frame_sz);
  return frame_sz;
}

#define DEF_RRSI  (-999)
typedef struct a2dp_match_ctx_t{
  int32_t _max_db;
  uint32_t _timeout;
  esp_bd_addr_t _max_db_device;
}a2dp_match_ctx_t;

#define ESP32_SOURCE_NAME   ("ESP32_A2DP_SRC")
static a2dp_match_ctx_t _a2dp_match;
static int auto_refresh_digital_token = 0;

static void a2dp_match_best_reset(){
  memset(&_a2dp_match, 0, sizeof(a2dp_match_ctx_t));
  _a2dp_match._max_db = DEF_RRSI;
}
/**
 * @brief 
 * 寻找指定时间内（3000毫秒）的信号最好的蓝牙音箱
 * @param name 设备名称
 * @param address 设备地址
 * @param rrsi 信号强度
 * @return true 
 * @return false 
 */
static bool a2dp_match_best_signal_device_cb(const char *name, esp_bd_addr_t address, int rrsi){
  bool found = false;
  a2dp_match_ctx_t *ctx = &_a2dp_match;
  uint64_t ts_now = millis();
  
  if(ctx->_max_db < rrsi && strncmp(ESP32_SOURCE_NAME, name, strlen(ESP32_SOURCE_NAME)) != 0){
    if(ctx->_timeout == 0){
      ctx->_timeout = ts_now + 3000;
    }
    ctx->_max_db = rrsi;
    memcpy(ctx->_max_db_device, address, sizeof(esp_bd_addr_t));
  }
  if(ctx->_max_db != DEF_RRSI && ctx->_timeout < ts_now){
    if(memcmp(address, ctx->_max_db_device, sizeof(esp_bd_addr_t)) == 0){
      found = true;
    }
  }  
  Serial.printf("a2dp_match_best_signal_device_cb: %s, db: %d, found: %d\n", name, rrsi, found);
  return found;
}

#endif // ESP32_A2DP_SOURCE

typedef AudioGeneratorAAC MyAudioGenerator;
static ezButton button(BOOT_PIN);
static AudioGenerator *audio_aac;
static AudioFileSource *file;
static AudioFileSource *srcbuffer;
static const char *audio_url = "";
static char audio_from_server[512] = {0};
static char audio_from_bakup[512] = {0};
static uint64_t audio_url_expired = 0, _last_udp_msg_timestamp = 0;
static uint32_t digital_token_sz = 0;
static char digital_token[32] = {0};

static int main_xnet_message_handler(const xnet_message_t *msg){
  switch(msg->_opcode){
    case XNET_ACK_MUSIC_NEXT:{
      if(msg->_wparam == 0){
        memset(audio_from_bakup, 0, sizeof(audio_from_bakup));
        strncpy(audio_from_bakup, (const char*)msg->_payload, msg->_payload_sz);
        // Serial.printf("bakup: %s\n", audio_from_bakup);
      }
      memset(audio_from_server, 0, sizeof(audio_from_server));
      strncpy(audio_from_server, (const char*)msg->_payload, msg->_payload_sz);
      // Serial.printf("recv url: %s\n", audio_url);
      audio_url = audio_from_server;
      audio_url_expired = millis() + 3000;
      if(audio_aac) audio_aac->stop();
    }
    break;
    case XNET_ACK_PLAYER_STATE:{
      float gain = msg->_uparam / 100.0f;
      output.SetGain(gain);
      output.SetPause(msg->_wparam ? true : false);
      // Serial.printf("set gain %.1f, pause: %d by xnet\n", gain, msg->_wparam);
    }
    break;
    case XNET_ACK_DIGITAL_ACCESS:{
      // should tts to speaker
      NVSetting::set_fresh_digital(0);
      memset(digital_token, 0, sizeof(digital_token));
      memcpy(digital_token, msg->_payload, msg->_payload_sz);
      digital_token[sizeof(digital_token)-1] = 0;
      digital_token_sz = msg->_payload_sz;
      Serial.printf("digital access token: %s\n", digital_token);
      if(audio_aac) audio_aac->stop();
    }
    break;
  }
  _last_udp_msg_timestamp = millis();
  return 0;
}

static uint64_t _ts_last_loop_tick = 0;
static void IRAM_ATTR app_health_checker(){
  uint64_t ticks = millis();
  if(ticks - _ts_last_loop_tick > 60000 || ticks - _last_udp_msg_timestamp > 120000){
    ESP.restart();
  }
}

static void yield_wait(uint32_t ms){
  uint64_t expired = millis() + ms;
  while(expired > millis()){
    yield();
  }
  return;
}

static const char *file_ext(const char *filename){
  if(strstr(filename, ".bilivideo.")) 
    return "bba";
  const char *dot = strrchr(filename, '.');
  if(!dot || dot == filename) return NULL;
  return dot + 1;  
}

static uint64_t wait_improv_timeout = 0;

static int improv_wifi_begin(const char *ssid, const char *passwd) {
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, passwd);
  return NVSetting::set_ssid_password(ssid, passwd);
}

static int imporv_wifi_isconnected() {
  return WiFi.status() == WL_CONNECTED ? 0 : -1;
}

static void improv_device_info(char firmware[16], char chip[16], char name[16], char version[16]) {
  sprintf(firmware, "%s", "Firmware");
  sprintf(chip, "%s", "ESP32");
  sprintf(name, "%s", "BLPlayer");
  sprintf(version,"%d", 20230710);
}

void setup()
{
  improv_callbacks_t cb = {
    .wifi_begin = improv_wifi_begin, 
    .wifi_isconnected = imporv_wifi_isconnected, 
    .device_info = improv_device_info
  };

  uint64_t ts_expired = 0;
  Serial.begin(115200);
  delay(2000);
  Serial.printf("version: %s, build: %s\n", BUILD_VERSION, BUILD_TIME);
  NVSetting::load_setting();
  ImprovSerial::begin(&cb);

  __monitor.loop();
  Serial.println("Connecting to WiFi");
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  Serial.printf("ssid: %s, pwd: %s\n", NVSetting::get_ssid(), NVSetting::get_password());
  WiFi.begin(NVSetting::get_ssid(), NVSetting::get_password());

  // Try 60 seconds
  ts_expired = millis() + 60000;
  while (WiFi.status() != WL_CONNECTED && ts_expired > millis()) {
    Serial.println("... connecting to WiFi ...");
    __monitor.loop();
    if(NVSetting::need_commit){
      NVSetting::need_commit = false;
      NVSetting::set_fresh_digital(1);
    }
    yield_wait(1000);
  }
  if(WiFi.status() != WL_CONNECTED){
    Serial.println("REBOOT FOR WiFi");
    ESP.restart();
  }else{
    Serial.println("Connected");
  }

  XNetController::setup(main_xnet_message_handler);
 
  OUTPUT_SETPIN(output);
  output.SetGain(0.6);
  audioLogger = &Serial;
  
  
  #ifdef ESP32_A2DP_SOURCE
  a2dp_source.set_discoverability(ESP_BT_NON_DISCOVERABLE);
  a2dp_source.set_ssid_callback(a2dp_match_best_signal_device_cb);
  a2dp_source.start("tom and jerry", a2dp_fill_samples_cb);

  a2dp_match_best_reset();
  ts_expired = millis() + 20000;
  while(!a2dp_source.is_connected() && ts_expired > millis()){
    Serial.println("... connecting bluetooth speaker ...");
    __monitor.loop();
    yield_wait(1000);
    XNetController::req_utc();
    XNetController::loop();
  }
  #else
  WiFi.setSleep(false);
  int times = 3;
  while(times--){
    yield_wait(1000);
    XNetController::req_utc();
    XNetController::loop();
  }
  #endif // ESP32_A2DP_SOURCE

  // install app-check-timer
  hw_timer_t *timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &app_health_checker, true);
  timerAlarmWrite(timer, 1000000, true);
  timerAlarmEnable(timer);
  timerRestart(timer);
  _ts_last_loop_tick = millis();

  auto_refresh_digital_token = NVSetting::get_fresh_digital();
  Serial.printf("auto_refresh_digital_token:  %d\n", auto_refresh_digital_token);
  
  // reserve some time to change wifi, if need.
  wait_improv_timeout = millis() + 60000;
}

void loop() {
  static uint64_t ts_button = 0, ts_button_down = 0, ts_for_next_music = 0, ts_for_utc = 0, ts_wifi_check = 0;
  static uint64_t ts_auto_digital = 0;

  uint64_t ts_now = millis();
  _ts_last_loop_tick = ts_now;
  __monitor.loop();
  XNetController::loop();
  
  // stop improv
  if(wait_improv_timeout != 0 && wait_improv_timeout < ts_now){
    wait_improv_timeout = 0;
    ImprovSerial::stop();
  }

  if(ts_wifi_check < ts_now){
    ts_wifi_check = ts_now + 90000;
    if(WiFi.status() != WL_CONNECTED){
      Serial.println("RECNN WiFi ... ");
      WiFi.disconnect();
      WiFi.reconnect();
    }
  }

  if(ts_for_utc < ts_now){
    ts_for_utc = ts_now + 180000;
    XNetController::req_utc();
  }

  if(ts_button < ts_now){
    button.loop();
    ts_button = ts_now + 50;    
  }
  if(button.isPressed() && ts_button_down == 0){
    ts_button_down = ts_now;
  }
  if(button.isReleased() && ts_button_down != 0){
    uint32_t ts_hold = ts_now - ts_button_down;
    ts_button_down = 0;
    if(ts_hold < 200){
      Serial.println("nothing to do 0 ...");
    }else if(ts_hold < 1500){
      memset(digital_token, 0, sizeof(digital_token));
      strcpy(digital_token, "xs");
      digital_token[sizeof(digital_token)-1] = 0;
      digital_token_sz = strlen("xs");
      Serial.printf("local sound effect: %s\n", digital_token);
    }else if(ts_hold < 3000){
      Serial.println("nothing to do 1 ...");
    }else if(ts_hold < 10000){
      XNetController::req_digital_access();
      Serial.println("refresh access token ...");
    }else if(ts_hold > 20000){
      nvs_flash_erase();
      Serial.println("nvs_flash_erase on boot ... ");
      delay(1000);
      ESP.restart();
    }
  }
  if(ts_auto_digital == 0) 
    ts_auto_digital = ts_now + 10000;
  if(ts_auto_digital < ts_now && auto_refresh_digital_token){
    auto_refresh_digital_token = 0;
    digital_token_sz = 0;
    XNetController::req_digital_access();
  }
  if(audio_aac){
    if(!audio_aac->isRunning()){
      __monitor.unregister(srcbuffer);
      file->close();
      srcbuffer->close();
      output.stop();
      delete file;
      delete audio_aac;
      delete srcbuffer;
      file = NULL;
      srcbuffer = NULL;
      audio_aac = NULL;
      if(audio_url_expired < millis()){
        audio_url = "";
        Serial.printf("music done, try next ...\n");
      }else{
        Serial.printf("music change ... \n");
      }
      delay(100);
    }else{
      if (!audio_aac->loop()){
        audio_aac->stop();
        if(strcmp(audio_from_server, audio_from_bakup) != 0 && audio_from_bakup[0] != 0){
          memcpy(audio_from_server, audio_from_bakup, sizeof(audio_from_server));
          audio_url = audio_from_server;
          audio_url_expired = ts_now + 3000;
          // Serial.printf("restore: %s\n", audio_url);          
        }else{
          audio_from_server[0] = 0;
          audio_from_bakup[0] = 0;
        }
      }
    }
  }else{
    if(digital_token_sz > 0){
      digital_token_sz = 0;
      file = new AudioFileDigitalTokens(digital_token);
      srcbuffer =  new MyAudioFileSourceBuffer(file, 2048);
      __monitor.register_monitor_message(srcbuffer, (obj_monitor_message_cb)&MyAudioFileSourceBuffer::monitor_message);
      audio_aac = new MyAudioGenerator();
      audio_aac->begin(srcbuffer, &output);
    }else if(strncmp(audio_url, "http://", strlen("http://")) == 0 || strncmp(audio_url, "https://", strlen("https://")) == 0){
      Serial.printf("play music: %s\n", audio_url);
      const char *ext = file_ext(audio_url);
      if(ext){
        file = new MyHttpFileSource(audio_url);
#if ESP32_A2DP_SOURCE
        srcbuffer =  new MyAudioFileSourceBuffer(file, 1024*8);
#else
        srcbuffer =  new MyAudioFileSourceBuffer(file, 1024*96);
#endif
        __monitor.register_monitor_message(srcbuffer, (obj_monitor_message_cb)&MyAudioFileSourceBuffer::monitor_message);
        if(strcmp(ext, "mp3") == 0){
          audio_aac = new AudioGeneratorMP3();
        }else if(strcmp(ext, "aac") == 0){
          audio_aac = new MyAudioGenerator();
        }else if(strcmp(ext, "bba") == 0){
          audio_aac = new MyAudioGenerator();

          Serial.println("BLBL Audio");
        }
        audio_aac->begin(srcbuffer, &output);
      }
    }else{
      if(ts_for_next_music < ts_now){
        ts_for_next_music = ts_now + 6000;
        XNetController::req_new_music(false);
        Serial.println("req_new_music ... ");
      }
    }
  }
}
#endif //__MAIN_CPP__