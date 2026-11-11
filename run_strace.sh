#!/bin/bash
set -eu
cd "$(dirname "$0")"
strace -f -tt -yy -s 160 -o strace.log ./build/parent < input.txt
printf 'Trace saved to strace.log\n'
