#if defined(ESP32) || defined(ESP8266)

#include "AudioFileSourceHTTPStreamV2.h"

AudioFileSourceHTTPStreamV2::AudioFileSourceHTTPStreamV2(const char *url, bool fill_body){
  _pos = 0;
  _fill_body = fill_body;
  _body = NULL;
  _body_sz = 0;
  open(url);
}

AudioFileSourceHTTPStreamV2::~AudioFileSourceHTTPStreamV2(){
  if(_client){
    esp_http_client_close(_client);
    esp_http_client_cleanup(_client);
    _client = NULL;
  }
  if(_body){
    free(_body);
  }
}

bool AudioFileSourceHTTPStreamV2::open(const char *url){
  esp_http_client_config_t config = {
    .url = url,
    .timeout_ms = 25000,            // connect timeout 25 seconds
    .buffer_size = RX_BUFFER_SZ,    // recv buffer > mtu ~ 1500 bytes
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
    if(_fill_body && err > 0){
      return _try_fill_body();
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
  
  if(_fill_body && _body && _body_sz > 0){
    read = min(len, _body_sz - _pos);
    memcpy(data, _body + _pos, read);
    _pos += read;
    return read;
  }
  
  if(_client){
    esp_http_client_set_timeout_ms(_client, 5000);
    if (len > RX_BUFFER_SZ)
      len = RX_BUFFER_SZ;
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

  if(_fill_body && _body && _body_sz > 0){
    read = min(len, _body_sz - _pos);
    memcpy(data, _body + _pos, read);
    _pos += read;
    return read;
  }

  if(_client){
    esp_http_client_set_timeout_ms(_client, 1);
    if (len > RX_BUFFER_SZ)
      len = RX_BUFFER_SZ;
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
  if(_fill_body && _body && _body_sz > 0){
    bool ok = false;
    switch(dir){
      case SEEK_CUR:{
        if(0 <= _pos + pos && _pos + pos < _body_sz){
          _pos += pos;
          ok = true;
        }
      }break;
      case SEEK_SET:{
        if(0 <= pos && pos < _body_sz){
          _pos = pos;
          ok = true;
        }
      }break;
      case SEEK_END:{
        if(0 <= _body_sz - pos && _body_sz - pos < _body_sz){
          _pos = _body_sz - pos;
          ok = true;
        }
      }break;
    }
    // Serial.printf("SEEK: %d, dir:%d, ok: %d\n", pos, dir, ok);
    return ok;
  }

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

bool AudioFileSourceHTTPStreamV2::_try_fill_body(){
  int wrote_length = 0;
  int content_length = esp_http_client_get_content_length(_client);
  _body = (uint8_t*)malloc(content_length);
  if(!_body){
    Serial.println("esp_http_client_download_body error");
    return false;
  }
  while(wrote_length < content_length) {
    int data_read = esp_http_client_read(_client, (char*)(_body+wrote_length), content_length - wrote_length);
    if(data_read == 0){
      bool is_recv_complete = esp_http_client_is_complete_data_received(_client);
      if ((errno == ENOTCONN || errno == ECONNRESET || errno == ECONNABORTED) && !is_recv_complete) {
        goto ERR_CLEANUP;
      }
      if(!is_recv_complete){
        vTaskDelay(1/portTICK_PERIOD_MS);
        continue;
      }else{
        break;
      }
    }
    else if(data_read > 0){
      wrote_length += data_read;
    }else{
      goto ERR_CLEANUP;
    }
  }
ERR_CLEANUP:
  if(content_length > 0 && content_length == wrote_length){
    _body_sz = content_length;
    Serial.printf("http client body size: %u\n", _body_sz);
    return true;
  }
  return false;
}

#endif