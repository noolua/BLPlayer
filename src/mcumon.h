/**
 * @file mcumon.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2023-03-12
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include <stdint.h>
#include <Arduino.h>

#if ARDUINO_ARCH_RP2040
#define esp_get_free_heap_size()  rp2040.getFreeHeap()
#endif
#ifdef HAVE_USE_MCUMON
#define Serial_printf   Serial.printf
#define Serial_print    Serial.print
#else
#define Serial_printf   (void)
#define Serial_print    (void)
#endif

class Ticker{
protected:
  uint32_t _ts_interval, _tick_next, _count, _period, _period_next, _last_count;
public:
  Ticker(uint32_t period=1, uint32_t interval=50){
    set_ticker(period, interval);
  }
  void set_ticker(uint32_t period, uint32_t interval){
    _ts_interval = interval;
    _tick_next = 0;
    _count = 0;
    _period = period;
    _period_next = 0;
    _last_count = 0;
  }
  bool check_next(uint32_t ms_now){
    bool ok = false;
    if(_tick_next < ms_now){
      _count++;
      _tick_next = ms_now + _ts_interval;
      ok = true;
    }
    return ok;
  }
  bool check_period(uint32_t ms_now){
    if(_period_next < ms_now){
      _last_count = _count / _period;
      _count = 0;
      _period_next = ms_now + (_period * 1000);
      return true;
    }
    return false;
  }
  uint32_t freq(){
    return _last_count;
  }
};

class McuMonitorCallback{
public:
  virtual int monitor_message(char *msg, int msg_len) = 0;
};

typedef int (*monitor_message_cb_t)(void *ud, char *msg, int msg_len);
typedef int (McuMonitorCallback::*obj_monitor_message_cb)(char *msg, int msg_len);

typedef struct{
  int type;
  union{
    struct{void *user_data; monitor_message_cb_t cb;};
    struct{McuMonitorCallback *obj; obj_monitor_message_cb obj_cb;};
  }method;
}monitor_message_reg_t;

class McuMonitor{
  enum {MAX_REGISTER=8, NONE_CB=0, GLOBAL_CB=1, OBJ_CB=2};
private:
  /* data */
  Ticker tft_freq, cpu_freq, mem_freq;
  monitor_message_reg_t _registers[MAX_REGISTER];
public:
  McuMonitor(/* args */){
    tft_freq.set_ticker(3, 50);
    cpu_freq.set_ticker(3, 0);
    mem_freq.set_ticker(3, 1000);
    memset(_registers, 0, sizeof(_registers));
  }
  ~McuMonitor(){}
  bool register_monitor_message(void* obj, obj_monitor_message_cb cb){
    bool registerd = false;
    for(int i=0; i < MAX_REGISTER; i++){
      monitor_message_reg_t *reg = &_registers[i];
      if(reg->type == 0){
        reg->type = OBJ_CB;
        reg->method.obj = (McuMonitorCallback*)obj;
        reg->method.obj_cb = cb;
        registerd = true;
        break;
      }
    }
    return registerd;
  }
  bool unregister(void *obj){
    bool found = false;
    for(int i=0; i < MAX_REGISTER; i++){
      monitor_message_reg_t *reg = &_registers[i];
      if(reg->type == OBJ_CB){
        if(reg->method.obj == obj){
          reg->type = NONE_CB;
          found = true;
          break;
        }
      }
    }
    return found;
  }
  void loop(){
    bool ok = false;
    uint32_t ms_now = millis();
    tft_freq.check_next(ms_now);
    cpu_freq.check_next(ms_now);
    mem_freq.check_next(ms_now);

    ok = tft_freq.check_period(ms_now);
    cpu_freq.check_period(ms_now);
    mem_freq.check_period(ms_now);
    if(ok){
      uint32_t free_heap_size = esp_get_free_heap_size();
      char message[128];
      Serial_printf("T:%8.1f, F:%2dHz, U:%4.1f%%, M:%5.1fKB", ms_now/1000.0f, tft_freq.freq(), (1000 - cpu_freq.freq())/10.0, free_heap_size/1024.0);
      for(int i=0; i < MAX_REGISTER; i++){
        monitor_message_reg_t *reg = &_registers[i];
        if(reg->type == OBJ_CB){
          int formated = (reg->method.obj->*(reg->method.obj_cb))(message, sizeof(message));
          if(formated > 0) Serial_print(message);
        }else if(reg->type == GLOBAL_CB){
          int formated = reg->method.cb(reg->method.user_data, message, sizeof(message));
          if(formated > 0) Serial_print(message);
        }
      }
      Serial_print("\n");
    }
  }
};
