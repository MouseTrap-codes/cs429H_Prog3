#!/usr/bin/env bash

# be sure to run:
# chmod +x build.sh 
# to make build.sh executable
set -e
gcc main.c -o hw3 -lm
echo "Build complete. Executable created: hw3"
