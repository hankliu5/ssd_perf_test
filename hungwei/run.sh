#!/bin/bash
THREAD_NUM=$1
PATH_TO_READ=$2
MODE=$3

FILE_SIZE=4
mount -o relatime -o remount $PATH_TO_READ
# do relatime (default)
for thread_num in `seq 1 ${THREAD_NUM}`
do
        cd libnvmed
        make clean && make NUM_OF_THREADS=${thread_num} && make install
        cd ..
        make
        touch log.txt
        echo ${FILE_SIZE} >> log.txt
        for i in {1..128}
        do
                free &>/dev/null && sync && echo 3 > /proc/sys/vm/drop_caches && free &>/dev/null
                ./speed_test_hungwei ${PATH_TO_READ}/input_${FILE_SIZE}.txt >> log.txt
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
        cd libnvmed
        make clean && make NUM_OF_THREADS=${thread_num} && make install
        cd ..
        make
        touch log.txt
        echo ${FILE_SIZE} >> log.txt
        for i in {1..128}
        do
                free &>/dev/null && sync && echo 3 > /proc/sys/vm/drop_caches && free &>/dev/null
                ./speed_test_hungwei ${PATH_TO_READ}/input_${FILE_SIZE}.txt >> log.txt
        done
        echo "finish num_thread ${thread_num}"
        python3 extract.py log.txt ${MODE}_${thread_num}
        rm log.txt
done
mv *.npy out
cd libnvmed
make clean
cd ..
make clean
