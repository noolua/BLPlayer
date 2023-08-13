/*
  AUDIOFILEDIGITALTOKENS
  Store a "file" as a PROGMEM array and use it as audio source data
  
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

#ifndef _AUDIOFILEDIGITALTOKENS_H
#define _AUDIOFILEDIGITALTOKENS_H

#include "AudioFileSource.h"

typedef struct token_node_s{
  const uint8_t *_data;
  int32_t _data_sz;
}token_node_t;

class AudioFileDigitalTokens : public AudioFileSource
{
  public:
    AudioFileDigitalTokens(const char *tokens);
    virtual ~AudioFileDigitalTokens() override;
    virtual bool open(const char *tokens) override;
    virtual uint32_t read(void *data, uint32_t len) override;
    virtual bool seek(int32_t pos, int dir) override;
    virtual bool close() override;
    virtual bool isOpen() override;
    virtual uint32_t getSize() override;
    virtual uint32_t getPos() override { if (!_opened) return 0; else return _pos; };

  private:
    bool _opened;
    token_node_t _nodes[64];
    int32_t _node_count;
    uint32_t _size, _pos;
};

#endif

