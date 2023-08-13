/*
  AudioFileSourceHTTPStreamEx
  Connect to a HTTP based streaming service
  
  Copyright (C) 2017  Earle F. Philhower, III

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#if defined(ESP32) || defined(ESP8266)
#pragma once

#include <Arduino.h>
#ifdef ESP32
  #include <WiFiClientSecure.h>
  #include <HTTPClient.h>
#else
  #include <ESP8266HTTPClient.h>
#endif
#include "AudioFileSource.h"

class AudioFileSourceHTTPStreamEx : public AudioFileSource
{
  enum{CHUNK_SIZE, CHUNK_PAYLOAD, CHUNK_COMPLETE};
  friend class AudioFileSourceICYStream;

  public:
    AudioFileSourceHTTPStreamEx();
    AudioFileSourceHTTPStreamEx(const char *url);
    virtual ~AudioFileSourceHTTPStreamEx() override;
    
    virtual bool open(const char *url) override;
    virtual uint32_t read(void *data, uint32_t len) override;
    virtual uint32_t readNonBlock(void *data, uint32_t len) override;
    virtual bool seek(int32_t pos, int dir) override;
    virtual bool close() override;
    virtual bool isOpen() override;
    virtual uint32_t getSize() override;
    virtual uint32_t getPos() override;
    bool SetReconnect(int tries, int delayms) { reconnectTries = tries; reconnectDelayMs = delayms; return true; }
    void useHTTP10 () { http.useHTTP10(true); }

    enum { STATUS_HTTPFAIL=2, STATUS_DISCONNECTED, STATUS_RECONNECTING, STATUS_RECONNECTED, STATUS_NODATA };

  private:
    virtual uint32_t readInternal(void *data, uint32_t len, bool nonBlock);
    WiFiClient client;
    HTTPClient http;
    bool chunked_content;
    int  chunk_size, chunk_status, chunk_filled;
    int pos;
    int size;
    int reconnectTries;
    int reconnectDelayMs;
    char saveURL[1024];
};

#endif

