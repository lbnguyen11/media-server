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

for i in $(seq 1 20);
do
  firefox --private http://0.0.0.0:8080/openning.mp4 > /dev/null 2>&1 &
  # sleep 1
done
