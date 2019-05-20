#!/bin/bash

MSVC_ROOT="/cygdrive/y/Programs/Microsoft Visual Studio/2017/Community/VC/Tools/MSVC/14.13.26128/bin/HostX86/x86/"
BIN_ROOT="/cygdrive/y/Games/Steam/Windows/steamapps/common/Reassembly/win32"

DUMPBIN="$MSVC_ROOT/dumpbin"
LIB="$MSVC_ROOT/lib"

make_def_file() {
	local target="$1"
	local filename=$(basename -- "$target")
	
	echo "NAME $filename"
	echo "EXPORTS"
	"$DUMPBIN" /exports "$(cygpath -w "$target")" | grep "^ \+[0-9]\+ [0-9A-F ]\+[^ ]*$" | sed "s/.* \+//g" | sort 
}
compile_def_file() {
	"$LIB" "/def:$1" /machine:x86 "/out:$2"
}
compile_lib_file() {
	local target="$1"
	local filename=$(basename -- "$target")
	local filename="${filename%.*}"

	make_def_file "$target" > "$filename.def"
	compile_def_file "$filename.def" "$filename.lib"
}

rm *.def *.exp *.lib
compile_lib_file "$BIN_ROOT/ReassemblyRelease.exe" 
# Dependencies
compile_lib_file "$BIN_ROOT/glew32.dll"
compile_lib_file "$BIN_ROOT/SDL2.dll"
compile_lib_file "$BIN_ROOT/SDL2_image.dll"
compile_lib_file "$BIN_ROOT/SDL2_ttf.dll"
compile_lib_file "$BIN_ROOT/zlib1.dll"
rm *.def *.exp
