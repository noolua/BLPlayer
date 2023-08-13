#ifndef __improv_serial_h__
#define __improv_serial_h__



typedef struct improv_callbacks_s{
  // wifi sta connect
  int (*wifi_begin)(const char *ssid, const char *passwd);
  // wifi check connect status
  int (*wifi_isconnected)();  
  // get deviceinfo
  void (*device_info)(char firmware[16], char chip_family[16], char name[16], char version[16]);
}improv_callbacks_t;

class ImprovSerial {
public:
  static void begin(improv_callbacks_t *cb);
  static void store_one_wifi_network(const char *ssid, int rssi, const bool auth);
  static void stop();
};

#endif // __improv_serial_h__