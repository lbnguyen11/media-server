#!/bin/bash

# Define debug trap
debug() {
  # print a '+' for every element in BASH_LINENO, similar to PS4's behavior
  printf '%s' "${BASH_LINENO[@]/*/+}"
  # Then print the current command, colored
  printf ' \e[36m%s\e[0m\n' "$BASH_COMMAND"
}
trap debug DEBUG
shopt -s extdebug # necessary for the DEBUG trap to carry into functions

# CYAN=$(tput setaf 6)
# RESET=$(tput sgr0)
# exec 3> >(exec sed -E -u "s@^[+]+.*\$@${CYAN}&${RESET}@")
# exec 1>&3
# BASH_XTRACEFD=3
# set -x

#run Debug build:
#echo "[INFO] Start Debug build!"
#cmake -B build/Debug -DCMAKE_BUILD_TYPE=Debug
#cmake --build build/Debug

#run Release build:
RELEASE_DIR=build/Release/bin
RELEASE_BIN=file_server
echo "[INFO](BEG) Run Release build!"
cd ${RELEASE_DIR} || exit
./${RELEASE_BIN} 2>&1
echo "[INFO](END) Run Release build!"