#if defined(ESP32) || defined(ESP8266)

#include "AudioFileSourceHTTPStreamV2.h"

AudioFileSourceHTTPStreamV2::AudioFileSourceHTTPStreamV2(const char *url){
  _pos = 0;
  open(url);
}

AudioFileSourceHTTPStreamV2::~AudioFileSourceHTTPStreamV2(){
  if(_client){
    esp_http_client_close(_client);
    esp_http_client_cleanup(_client);
    _client = NULL;
  }
}

bool AudioFileSourceHTTPStreamV2::open(const char *url){
  esp_http_client_config_t config = {
    .url = url,
    .timeout_ms = 15000,      // connect timeout 15 seconds
    .buffer_size = 4096,      // recv buffer > mtu ~ 1500 bytes
    .is_async = true,
  };
  _client = esp_http_client_init(&config);
  if(_client){
    esp_err_t err = esp_http_client_open(_client, 0);
    if(err != ESP_OK){
      Serial.println("esp_http_client_open error");
      return false;
    }
    err = esp_http_client_fetch_headers(_client);
    if(err == -1){
      Serial.println("esp_http_client_fetch_headers error");
      return false;
    }
    /*
    Serial.printf("esp_http_client_init success: %d\n", esp_http_client_is_chunked_response(_client));
    if(esp_http_client_is_chunked_response(_client)){
      int len = 0;
      err = esp_http_client_get_chunk_length(_client, &len);
      Serial.printf("current chunked length: %d, %d\n", err, len);
    }
    */
    return true;
  }else{
    Serial.println("esp_http_client_init error");
  }
  return false;
}

uint32_t AudioFileSourceHTTPStreamV2::read(void *data, uint32_t len){
  int read = -1;
  if(_client){
    esp_http_client_set_timeout_ms(_client, 5000);
    read = esp_http_client_read(_client, (char *)data, (int)len);
  }
  if(read == -1){
    return 0;
  }
  _pos += read;
  // Serial.printf("read: %d, len: %u\n", read, len);
  return read;
}

uint32_t AudioFileSourceHTTPStreamV2::readNonBlock(void *data, uint32_t len){
  int read = -1;
  if(_client){
    esp_http_client_set_timeout_ms(_client, 1);
    read = esp_http_client_read(_client, (char *)data, (int)len);
  }
  if(read == -1){
    return 0;
  }
  _pos += read;
  // Serial.printf("read: %d, len: %u\n", read, len);
  return read;
}

bool AudioFileSourceHTTPStreamV2::seek(int32_t pos, int dir){
  return false;
}

bool AudioFileSourceHTTPStreamV2::close(){
  if(_client){
    esp_http_client_close(_client);
    esp_http_client_cleanup(_client);
    _client = NULL;
  }
  return true;
}

bool AudioFileSourceHTTPStreamV2::isOpen(){
  return _client ? true : false;
}

uint32_t AudioFileSourceHTTPStreamV2::getSize(){
  if(_client)
    return esp_http_client_is_chunked_response(_client) ? 0x7FFFFFFF : esp_http_client_get_content_length(_client);
  return 0;
}

uint32_t AudioFileSourceHTTPStreamV2::getPos(){
  return _pos;
}





#endif