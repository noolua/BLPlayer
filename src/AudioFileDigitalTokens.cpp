/*
  AudioFileDigitalTokens
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

#include "AudioFileDigitalTokens.h"
#include "assets/access_token.h"

AudioFileDigitalTokens::AudioFileDigitalTokens(const char *tokens)
{
  _opened = false;
  _node_count = 0;
  _size = 0;
  _pos = 0;
  memset(_nodes, 0, sizeof(_nodes));
  open(tokens);
}

AudioFileDigitalTokens::~AudioFileDigitalTokens()
{
}

bool AudioFileDigitalTokens::open(const char *tokens) {
  if (!tokens) return false;

  const char *ch = tokens;
  _opened = true;
  _node_count = 0;
  _size = 0;
  _pos = 0;
  while(*ch){
    token_node_t *n = &_nodes[_node_count];
    switch(*ch){
      case ':': { n->_data = prefix_aac; n->_data_sz = sizeof(prefix_aac);  _size += n->_data_sz; } break;
      case '0': { n->_data = d0_aac; n->_data_sz = sizeof(d0_aac);  _size += n->_data_sz; } break;
      case '1': { n->_data = d1_aac; n->_data_sz = sizeof(d1_aac);  _size += n->_data_sz; } break;
      case '2': { n->_data = d2_aac; n->_data_sz = sizeof(d2_aac);  _size += n->_data_sz; } break;
      case '3': { n->_data = d3_aac; n->_data_sz = sizeof(d3_aac);  _size += n->_data_sz; } break;
      case '4': { n->_data = d4_aac; n->_data_sz = sizeof(d4_aac);  _size += n->_data_sz; } break;
      case '5': { n->_data = d5_aac; n->_data_sz = sizeof(d5_aac);  _size += n->_data_sz; } break;
      case '6': { n->_data = d6_aac; n->_data_sz = sizeof(d6_aac);  _size += n->_data_sz; } break;
      case '7': { n->_data = d7_aac; n->_data_sz = sizeof(d7_aac);  _size += n->_data_sz; } break;
      case '8': { n->_data = d8_aac; n->_data_sz = sizeof(d8_aac);  _size += n->_data_sz; } break;
      case '9': { n->_data = d9_aac; n->_data_sz = sizeof(d9_aac);  _size += n->_data_sz; } break;
      case 'p': { n->_data = pop_aac; n->_data_sz = sizeof(pop_aac);  _size += n->_data_sz; } break;
      case 's': { n->_data = success_aac; n->_data_sz = sizeof(success_aac);  _size += n->_data_sz; } break;
      case 'x':
      default: { n->_data = dd_aac; n->_data_sz = sizeof(dd_aac);  _size += n->_data_sz; } break;
    }
    // Serial.printf("node %d, data_sz %u\n", _node_count, n->_data_sz);
    _node_count++;
    ch++;
  }
  // Serial.printf("tokens: %s, count: %d, size: %u, pos: %u\n", tokens, _node_count, _size, _pos);
  return true;
}

uint32_t AudioFileDigitalTokens::getSize()
{
  if (!_opened) return 0;
  return _size;
}

bool AudioFileDigitalTokens::isOpen()
{
  return _opened;
}

bool AudioFileDigitalTokens::close()
{
  _opened = false;
  _size = 0;
  _pos = 0;
  _node_count = 0;
  return true;
}  

bool AudioFileDigitalTokens::seek(int32_t pos, int dir)
{
  if (!_opened) return false;
  uint32_t newPtr;
  switch (dir) {
    case SEEK_SET: newPtr = pos; break;
    case SEEK_CUR: newPtr = _pos + pos; break;
    case SEEK_END: newPtr = _size - pos; break;
    default: return false;
  }
  if (newPtr > _size) return false;
  _pos = newPtr;
  return true;
}

uint32_t AudioFileDigitalTokens::read(void *data, uint32_t len) {
  if (!_opened) return 0;
  if (_pos >= _size) return 0;
  uint8_t *buffer = (uint8_t *)data;

  uint32_t toRead = _size - _pos;
  if (toRead > len) toRead = len;

  uint32_t seek_pos = 0;
  int32_t node_found = -1;
  for(int i=0; i<_node_count;i++){
    token_node_t *n = &_nodes[i];
    if(seek_pos + n->_data_sz <= _pos){
      seek_pos += n->_data_sz;
    }else{
      node_found = i;
      break;
    }
  }

  uint32_t copyed = 0;
  // Serial.printf("begin read len: %u, real: %u, ssseek: %u, pos: %u\n", len, toRead, seek_pos, _pos);
  while(node_found < _node_count && copyed < toRead) {
    token_node_t *n = &_nodes[node_found];
    uint32_t skiped_head = _pos - seek_pos;
    uint32_t rest_part = (n->_data_sz - skiped_head);
    if (rest_part >= (toRead - copyed)) rest_part = (toRead - copyed);
    // Serial.printf("read, _seek_pos: %u, _pos:%u, _copyed: %u, part: %u\n", seek_pos, _pos, copyed, rest_part);
    memcpy_P(buffer + copyed, reinterpret_cast<const uint8_t*>(n->_data) + skiped_head, rest_part);
    copyed += rest_part;
    _pos += rest_part;
    seek_pos = _pos;
    node_found++;
  }

  return copyed;
}


