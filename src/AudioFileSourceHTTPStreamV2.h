/*
  AudioFileSourceHTTPStreamV2
*/

#if defined(ESP32) || defined(ESP8266)
#pragma once

#include <Arduino.h>
#include "AudioFileSource.h"
#include <esp_http_client.h>

class AudioFileSourceHTTPStreamV2 : public AudioFileSource {
  public:
    AudioFileSourceHTTPStreamV2(const char *url);
    virtual ~AudioFileSourceHTTPStreamV2() override;

    virtual bool open(const char *url) override;
    virtual uint32_t read(void *data, uint32_t len) override;
    virtual uint32_t readNonBlock(void *data, uint32_t len) override;
    virtual bool seek(int32_t pos, int dir) override;
    virtual bool close() override;
    virtual bool isOpen() override;
    virtual uint32_t getSize() override;
    virtual uint32_t getPos() override;
  private:
    esp_http_client_handle_t _client;
    uint32_t _pos;
};

#endif

