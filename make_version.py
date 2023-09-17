Import("env")
import os
VERSIONFILE="./src/version.h"
print("checking version.h")
DEF_VERSION='''#ifndef __version_h__

#define BUILD_VERSION   "0.0.0"
#define BUILD_TIME      "2023-01-01T00:00:00Z"

#endif // __version_h__
'''

# 检查version.h文件是否存在
if not os.path.isfile(VERSIONFILE):
  # 如果不存在，创建version.h并写入指定内容
  with open(VERSIONFILE, 'w') as file:
    file.write(DEF_VERSION)
    print('version.h created with default version number.')
else:
  print('version.h already exists.')