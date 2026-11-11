#!/bin/bash
set -eu
cd "$(dirname "$0")"
./build/parent < input.txt
printf '\n--- short.txt ---\n'
cat short.txt
printf '\n--- long.txt ---\n'
cat long.txt
cmp expected_short.txt short.txt
cmp expected_long.txt long.txt
