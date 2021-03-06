#!/bin/bash
echo "BASH ===================================================================="
echo "BASH ===Running Bash Script for LLFS Assignment ========================="
echo "BASH ===================================================================="
echo "BASH ===building libraries and running unit tests ======================="
echo "BASH ===================================================================="

make run_unittest

echo "BASH ===================================================================="
echo "BASH ===running showcase tests [test0x.c] ==============================="
echo "BASH ===================================================================="
make test01
./test01
echo "BASH ===removing existing vdisk for test2================================"
rm ./vdisk 
make test02
./test02