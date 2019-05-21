#!/bin/bash

# Headers required:
# - chipmunk (statically linked)
# - glew 1.11.0 (glew32.dll)
# - glm 0.9.7.0 (statically linked)
# - minizip 1.2.x (statically linked)
# - re2 2019-04-01 (statically linked)
# - zlib 1.2.11 (zlib1.dll)

LIBS="$(pwd)"

CHIPMUNK_VERSION=a596bdb71db33c61c52c1ef94074737cb104c99f
GLEW_VERSION=1.11.0
GLM_VERSION=0.9.7.0
MINIZIP_VERSION=1.2
RE2_VERSION=2019-04-01
ZLIB_VERSION=1.2.11

for i in chipmunk glew glm minizip re2 zlib; do
	rm -rvf $i || exit 1
	mkdir -v $i || exit 1	
done

cd /tmp
rm -rvf download_temp || exit 1
mkdir download_temp || exit 1
cd download_temp

# Download chipmunk sources
wget -O Chipmunk2D-$CHIPMUNK_VERSION.tgz https://github.com/manylegged/Chipmunk2D/archive/$CHIPMUNK_VERSION.tar.gz
tar xvf Chipmunk2D-$CHIPMUNK_VERSION.tgz || exit 1
cp -vr Chipmunk2D-$CHIPMUNK_VERSION/{LICENSE.txt,src,include} "$LIBS/chipmunk" || exit 1
rm -v "$LIBS/chipmunk/src/CMakeLists.txt" || exit 1

# Download glew headers
wget http://downloads.sourceforge.net/project/glew/glew/$GLEW_VERSION/glew-$GLEW_VERSION-win32.zip || exit 1
unzip glew-$GLEW_VERSION-win32.zip || exit 1
cp -vr glew-$GLEW_VERSION/{LICENSE.txt,include/*} "$LIBS/glew" || exit 1

# Download glm sources
wget -O glm-$GLM_VERSION.tar.gz https://github.com/g-truc/glm/archive/$GLM_VERSION.tar.gz
tar xvf glm-$GLM_VERSION.tar.gz || exit 1
cp -vr glm-$GLM_VERSION/{copying.txt,glm} "$LIBS/glm" || exit 1
rm -v "$LIBS/glm/glm/CMakeLists.txt" || exit 1

# Download minizip headers
wget -O minizip-$MINIZIP_VERSION.tar.gz https://github.com/nmoinvaz/minizip/archive/$MINIZIP_VERSION.tar.gz || exit 1
tar xvf minizip-$MINIZIP_VERSION.tar.gz || exit 1
cp -vr minizip-$MINIZIP_VERSION/{LICENSE,*.h} "$LIBS/minizip" || exit 1
mkdir -v "$LIBS/minizip/aes" || exit 1
cp -v minizip-$MINIZIP_VERSION/aes/*.h "$LIBS/minizip/aes" || exit 1

# Download re2 headers
wget -O re2-$RE2_VERSION.tar.gz https://github.com/google/re2/archive/$RE2_VERSION.tar.gz
tar xvf re2-$RE2_VERSION.tar.gz || exit 1
cp -vr re2-$RE2_VERSION/LICENSE "$LIBS/re2" || exit 1
mkdir -v "$LIBS/re2"/{re2,util} || exit 1
cp -v re2-$RE2_VERSION/re2/*.h "$LIBS/re2/re2" || exit 1
cp -v re2-$RE2_VERSION/util/*.h "$LIBS/re2/util" || exit 1

# Download zlib headers
wget https://zlib.net/zlib-$ZLIB_VERSION.tar.xz || exit 1
tar xvf zlib-$ZLIB_VERSION.tar.xz || exit 1
cp -v zlib-$ZLIB_VERSION/{README,*.h} "$LIBS/zlib" || exit 1

cd ..
rm -rvf download_temp || exit 1
