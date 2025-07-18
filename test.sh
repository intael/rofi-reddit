#!/bin/bash
set -e


meson setup build --reconfigure

cp build/compile_commands.json .

ninja -C build test
