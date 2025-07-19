#!/bin/sh

header_file=$(basename "$2")
header_name=${header_file%.*}
c_file="$header_name.c"
$4 ../subprojects/CMock/lib/cmock.rb -o$1 $2

if [ $? -eq 0 ]; then
	mocks_path="$MESON_BUILD_ROOT/mocks"
	if [ -n "$5" ]; then
		mocks_path="$mocks_path/$5"
	fi
	cp "$mocks_path/mock_${header_name}.c" $3
	cp "$mocks_path/mock_${header_name}.h" $3
	printf "%s %s" "$3/mock_$header_file" "$3/mock_$c_file"
fi
