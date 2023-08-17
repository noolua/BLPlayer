#ifndef __udp_message_controller_h__
#define __udp_message_controller_h__

#include <stdint.h>

/**
 * @brief 使用轮询的方式请求服务端，该设备是否有新的指令, 间隔2秒一次
 * 
 */

typedef struct xnet_message_t{
  uint16_t _opcode;         // message_key
  uint16_t _uparam;         // u参数
  uint16_t _wparam;         // w参数
  uint16_t _payload_sz;     // payload的字节大小，绝大多数时候都是0
  uint8_t  _payload[0];     // payload数据
}xnet_message_t;


#define XNET_REQ_TIMESTAMP          (0x0000)  // 请求授时
#define XNET_REQ_COMMAND            (0x0001)  // 请求是否有指令
#define XNET_REQ_NEW_MUSIC          (0x0002)  // 设备请求新的music, uparam为0表示当前频道的曲目，uparam为1表示新频道的曲目
#define XNET_REQ_UPDATE_STATE       (0x0003)  // 主动更新PLAYER状态到服务端, 用户使用按钮后的播放器状态应该主动推送给服务端保存, 为播放器重启后可以继续, 音量uparam(0-100), 暂停状态wparam,0播放,1暂停
#define XNET_REQ_DIGITAL_ACCESS     (0x0004)  // 临时数字码, 数字码是一个带有效期n分钟的访问凭证，通过该数字码可以在浏览器页面上临时管理该设备，数字码不同于访问码，数字码只能物理按键触发，数字码的相关操作被视作所有者的行为。

#define XNET_ACK_TIMESTAMP          (0x8000)  // 返回世界时间UTC, _payload中存放(uint64_t)类型的timestamp
#define XNET_ACK_MUSIC_NEXT         (0x8001)  // 播放下一首歌, _payload中存放music'url
#define XNET_ACK_PLAYER_STATE       (0x8002)  // 音量uparam(0-100), 暂停/播放状态wparam, 0播放, 1暂停
#define XNET_ACK_DIGITAL_ACCESS     (0x8003)  // 临时数字码, 以':'为前缀的6位数字，例如':873423', 此处的':'代表起始符，存放在_playload中

typedef int (*xnet_message_handler_cb_t)(const xnet_message_t *msg);

class XNetController{
  XNetController(){}
  ~XNetController(){}
  public:
  
  // 返回设备boot后的毫秒数
  static uint64_t millis();
  // 返回毫秒级的utc时间戳
  static uint64_t utc();
  // 初始化
  static void setup(xnet_message_handler_cb_t handler);
  // 循环逻辑
  static void loop();
  
  // 请求同步时钟
  static void req_utc();
  // 请求新音乐
  static void req_new_music(bool new_channel=false);
  // 请求更新播放器
  static void req_update_state(uint16_t volume, bool pause);
  // 请求临时性的数字码
  static void req_digital_access();
};


#endif // __udp_message_controller_h__
