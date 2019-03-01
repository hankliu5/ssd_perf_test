#!/bin/bash
THREAD_NUM=$1
PATH_TO_FILE=$2

make
for i in {1..10}
do
        free && sync && echo 3 > /proc/sys/vm/drop_caches && free
        ./speed_test_multi $THREAD_NUM $PATH_TO_FILE
done
make clean
