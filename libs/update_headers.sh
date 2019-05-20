#!/bin/bash

# Headers required:
# - chipmunk 6.2.2 (statically linked)
# - glew 1.11.0 (glew32.dll)
# - glm 0.9.7.0 (statically linked)
# - minizip 1.2.x (statically linked)
# - re2 2019-04-01 (statically linked)
# - SDL2 2.0.8 (SDL2.dll)
# - SDL2_image 2.0.4 (SDL2_image.dll)
# - SDL2_ttf 2.0.14 (SDL2_ttf.dll)
# - zlib 1.2.11 (zlib1.dll)

LIBS="$(pwd)"

CHIPMUNK_VERSION=6.2.2
GLEW_VERSION=1.11.0
GLM_VERSION=0.9.7.0
MINIZIP_VERSION=1.2
RE2_VERSION=2019-04-01
SDL2_VERSION=2.0.8
SDL2_IMAGE_VERSION=2.0.4
SDL2_TTF_VERSION=2.0.14
ZLIB_VERSION=1.2.11

for i in chipmunk glew glm minizip re2 sdl sdl_image sdl_ttf zlib; do
	rm -rvf $i || exit 1
	mkdir -v $i || exit 1	
done

cd /tmp
rm -rvf download_temp || exit 1
mkdir download_temp || exit 1
cd download_temp

# Download chipmunk sources
wget https://chipmunk-physics.net/release/Chipmunk-6.x/Chipmunk-$CHIPMUNK_VERSION.tgz
tar xvf Chipmunk-$CHIPMUNK_VERSION.tgz || exit 1
cp -vr Chipmunk-$CHIPMUNK_VERSION/{LICENSE.txt,src,include} "$LIBS/chipmunk" || exit 1
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

# Download minizip sources
wget -O minizip-$MINIZIP_VERSION.tar.gz https://github.com/nmoinvaz/minizip/archive/$MINIZIP_VERSION.tar.gz || exit 1
tar xvf minizip-$MINIZIP_VERSION.tar.gz || exit 1
cp -vr minizip-$MINIZIP_VERSION/{LICENSE,aes,*.c,*.h} "$LIBS/minizip" || exit 1

# Download re2 sources
wget -O re2-$RE2_VERSION.tar.gz https://github.com/google/re2/archive/$RE2_VERSION.tar.gz
tar xvf re2-$RE2_VERSION.tar.gz || exit 1
cp -vr re2-$RE2_VERSION/{LICENSE,util} "$LIBS/re2" || exit 1
mkdir -v "$LIBS/re2/re2" || exit 1
cp -v re2-$RE2_VERSION/re2/{*.cc,*.h} "$LIBS/re2/re2" || exit 1

# Download SDL2 headers
wget https://www.libsdl.org/release/SDL2-$SDL2_VERSION.tar.gz || exit 1
tar xvf SDL2-$SDL2_VERSION.tar.gz || exit 1
cp -v SDL2-$SDL2_VERSION/{COPYING.txt,include/*.h} "$LIBS/sdl" || exit 1

# Download SDL2_image headers
wget https://www.libsdl.org/projects/SDL_image/release/SDL2_image-$SDL2_IMAGE_VERSION.tar.gz || exit 1
tar xvf SDL2_image-$SDL2_IMAGE_VERSION.tar.gz || exit 1
cp -v SDL2_image-$SDL2_IMAGE_VERSION/{COPYING.txt,*.h} "$LIBS/sdl_image" || exit 1     

# Download SDL2_ttf headers
wget https://www.libsdl.org/projects/SDL_ttf/release/SDL2_ttf-$SDL2_TTF_VERSION.tar.gz || exit 1
tar xvf SDL2_ttf-$SDL2_TTF_VERSION.tar.gz || exit 1
cp -v SDL2_ttf-$SDL2_TTF_VERSION/{COPYING.txt,*.h} "$LIBS/sdl_ttf" || exit 1

# Download zlib headers
wget https://zlib.net/zlib-$ZLIB_VERSION.tar.xz || exit 1
tar xvf zlib-$ZLIB_VERSION.tar.xz || exit 1
cp -v zlib-$ZLIB_VERSION/{README,*.h} "$LIBS/zlib" || exit 1

cd ..
rm -rvf download_temp || exit 1
