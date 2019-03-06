#!/bin/bash
PATH_TO_READ=$1
MODE=$2
THREAD_NUM=$3
make

mount -o relatime -o remount $PATH_TO_READ
# do relatime (default)
for thread_num in `seq 1 ${THREAD_NUM}`
do
        touch log.txt
        echo ${thread_num} >> log.txt
        for i in {1..128}
        do
                free &>/dev/null && sync && echo 3 > /proc/sys/vm/drop_caches && free &>/dev/null
                ./speed_test_multi ${thread_num} ${PATH_TO_READ} >> log.txt
        done
        echo "finish num_thread ${thread_num}"
        python3 extract.py log.txt ${MODE}_${thread_num}
        rm log.txt
done

# do noatime
mount -o noatime -o remount $PATH_TO_READ
MODE+="_noatime"
echo "working on noatime"

for thread_num in `seq 1 ${THREAD_NUM}`
do
        touch log.txt
        echo ${thread_num} >> log.txt
        for i in {1..128}
        do
                free &>/dev/null && sync && echo 3 > /proc/sys/vm/drop_caches && free &>/dev/null
                ./speed_test_multi ${thread_num} ${PATH_TO_READ} >> log.txt
        done
        echo "finish num_thread ${thread_num}"
        python3 extract.py log.txt ${MODE}_${thread_num}
        rm log.txt
done
mv *.npy out
make clean
