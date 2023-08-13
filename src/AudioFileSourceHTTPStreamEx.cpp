/*
  AudioFileSourceHTTPStreamEx
  Streaming HTTP source

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

#include "AudioFileSourceHTTPStreamEx.h"

AudioFileSourceHTTPStreamEx::AudioFileSourceHTTPStreamEx(){
  pos = 0;
  size = 0;
  reconnectTries = 0;
  saveURL[0] = 0;
  chunked_content = false;
  chunk_size = 0;
  chunk_status = CHUNK_SIZE;
}

AudioFileSourceHTTPStreamEx::AudioFileSourceHTTPStreamEx(const char *url){
  pos = 0;
  size = 0;
  saveURL[0] = 0;
  reconnectTries = 0;
  chunked_content = false;
  chunk_size = 0;
  chunk_status = CHUNK_SIZE;
  open(url);
}

bool AudioFileSourceHTTPStreamEx::open(const char *url)
{
  pos = 0; 
  http.begin(client, url);
  http.setReuse(true);
  http.setTimeout(10000);
#ifndef ESP32
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
#endif
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    cb.st(STATUS_HTTPFAIL, PSTR("Can't open HTTP request"));
    return false;
  }
  size = http.getSize();
  if(size == -1){
    Serial.println("HTTP CHUNKED CONTENT");
    chunked_content = true;
    size = 0x7FFFFFFF;
  }
  strncpy(saveURL, url, sizeof(saveURL));
  saveURL[sizeof(saveURL)-1] = 0;
  return true;
}

AudioFileSourceHTTPStreamEx::~AudioFileSourceHTTPStreamEx(){
  http.end();
}

uint32_t AudioFileSourceHTTPStreamEx::read(void *data, uint32_t len){
  if (data==NULL) {
    audioLogger->printf_P(PSTR("ERROR! AudioFileSourceHTTPStreamEx::read passed NULL data\n"));
    return 0;
  }
  return readInternal(data, len, false);
}

uint32_t AudioFileSourceHTTPStreamEx::readNonBlock(void *data, uint32_t len){
  if (data==NULL) {
    audioLogger->printf_P(PSTR("ERROR! AudioFileSourceHTTPStreamEx::readNonBlock passed NULL data\n"));
    return 0;
  }
  return readInternal(data, len, true);
}

uint32_t AudioFileSourceHTTPStreamEx::readInternal(void *data, uint32_t len, bool nonBlock){
retry:
  if (!http.connected()) {
    cb.st(STATUS_DISCONNECTED, PSTR("Stream disconnected"));
    http.end();
    for (int i = 0; i < reconnectTries; i++) {
      char buff[64];
      sprintf_P(buff, PSTR("Attempting to reconnect, try %d"), i);
      cb.st(STATUS_RECONNECTING, buff);
      delay(reconnectDelayMs);
      if (open(saveURL)) {
        cb.st(STATUS_RECONNECTED, PSTR("Stream reconnected"));
        break;
      }
    }
    if (!http.connected()) {
      cb.st(STATUS_DISCONNECTED, PSTR("Unable to reconnect"));
      return 0;
    }
  }
  if ((size > 0) && (pos >= size)) return 0;

  WiFiClient *stream = http.getStreamPtr();

  // Can't read past EOF...
  if ( (size > 0) && (len > (uint32_t)(pos - size)) ) len = pos - size;

  if (!nonBlock) {
    int start = millis();
    while ((stream->available() < (int)len) && (millis() - start < 500)) yield();
  }

  size_t avail = stream->available();
  if (!nonBlock && !avail) {
    cb.st(STATUS_NODATA, PSTR("No stream data available"));
    http.end();
    goto retry;
  }
  if (avail == 0) return 0;
  if (avail < len) len = avail;

  int read = 0;
  if(chunked_content == false){
    read = stream->read(reinterpret_cast<uint8_t*>(data), len);
    pos += read;
  }else{
    if(chunk_status == CHUNK_COMPLETE){
      return 0;
    }else if(chunk_status == CHUNK_SIZE){
      char buffer[32];
      int idx = 0;
      while(stream->read(reinterpret_cast<uint8_t*>(&buffer[idx]), 1)){
        if(buffer[idx] == '\n'){
          break;
        }
        idx++;
      }
      if(idx > 0){
        // Serial.printf("buffer: %s\n", buffer);
        buffer[idx-1] = 0;
        chunk_size = strtol(buffer, NULL, 16);
        chunk_filled = 0;
        chunk_status = CHUNK_PAYLOAD;
        if(chunk_size == 0){
          // Serial.println("CHUNKED 00000, END");
          chunk_status = CHUNK_COMPLETE;
          return 0;
        }else{
          // Serial.printf("CURR_CHUNKED : %u\n", chunk_size);
        }
      }
    }
    if(chunk_status == CHUNK_PAYLOAD){
      len = (chunk_size - chunk_filled) < len ? (chunk_size - chunk_filled) : len;
      if(len > 0){
        read = stream->read(reinterpret_cast<uint8_t*>(data), len);
        chunk_filled += read;
        pos += read;
      }
      // Serial.printf("read.chunk.real.data %u, chunk_size: %u, chunk_filed: %u\n", read, chunk_size, chunk_filled);
      if(chunk_size == chunk_filled){
        uint8_t endCRLF[4];
        stream->read(endCRLF, 2);
        chunk_status = CHUNK_SIZE;
      }
    }
  }
  return read;
}

bool AudioFileSourceHTTPStreamEx::seek(int32_t pos, int dir){
  audioLogger->printf_P(PSTR("ERROR! AudioFileSourceHTTPStreamEx::seek not implemented!"));
  (void) pos;
  (void) dir;
  return false;
}

bool AudioFileSourceHTTPStreamEx::close(){
  http.end();
  return true;
}

bool AudioFileSourceHTTPStreamEx::isOpen(){
  return http.connected();
}

uint32_t AudioFileSourceHTTPStreamEx::getSize(){
  return size;
}

uint32_t AudioFileSourceHTTPStreamEx::getPos(){
  return pos;
}

#endif
