#!/bin/bash

# Headers required:
# - chipmunk (statically linked)
# - glew 1.11.0 (glew32.dll)
# - glm 0.9.7.0 (statically linked)
# - outlaws-core (statically linked)
# - zlib 1.2.11 (zlib1.dll)

# While we could use submodules for much of this, Visual Studio behaves very poorly
# with random unused CMakeLists scattered across the directory structure.
#
# Plus, this helps with the user updating the core sources. Git is weird.

LIBS="$(pwd)"

CHIPMUNK_VERSION=9526bb67a0217293738d8a9ac1e274d563e73abd
GLEW_VERSION=1.11.0
GLM_VERSION=0.9.7.0
ZLIB_VERSION=1.2.11

for i in chipmunk core glew glm zlib; do
	rm -rvf $i || exit 1
	mkdir -v $i || exit 1	
done

cd /tmp
rm -rvf download_temp || exit 1
mkdir download_temp || exit 1
cd download_temp

# Download chipmunk sources
wget -O Chipmunk2D-$CHIPMUNK_VERSION.tgz https://github.com/Lymia/Chipmunk2D/archive/$CHIPMUNK_VERSION.tar.gz
tar xvf Chipmunk2D-$CHIPMUNK_VERSION.tgz || exit 1
cp -vr Chipmunk2D-$CHIPMUNK_VERSION/{LICENSE.txt,src,include} "$LIBS/chipmunk" || exit 1
rm -v "$LIBS/chipmunk/src/CMakeLists.txt" || exit 1

# Download core sources
git clone https://github.com/Lymia/outlaws-core || exit 1
cp -v outlaws-core/{LICENSE,*.h,{base64,md5}.c,{base64,Geometry,Nav,Particles,stl_ext,Str}.cpp} "$LIBS/core" || exit 1

# Download glew headers
wget http://downloads.sourceforge.net/project/glew/glew/$GLEW_VERSION/glew-$GLEW_VERSION-win32.zip || exit 1
unzip glew-$GLEW_VERSION-win32.zip || exit 1
cp -vr glew-$GLEW_VERSION/{LICENSE.txt,include/*} "$LIBS/glew" || exit 1

# Download glm sources
wget -O glm-$GLM_VERSION.tar.gz https://github.com/g-truc/glm/archive/$GLM_VERSION.tar.gz
tar xvf glm-$GLM_VERSION.tar.gz || exit 1
cp -vr glm-$GLM_VERSION/{copying.txt,glm} "$LIBS/glm" || exit 1
rm -v "$LIBS/glm/glm/CMakeLists.txt" || exit 1

# Download zlib headers
wget https://zlib.net/zlib-$ZLIB_VERSION.tar.xz || exit 1
tar xvf zlib-$ZLIB_VERSION.tar.xz || exit 1
cp -v zlib-$ZLIB_VERSION/{README,*.h} "$LIBS/zlib" || exit 1

cd ..
rm -rvf download_temp || exit 1
