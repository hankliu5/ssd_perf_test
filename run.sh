#!/bin/bash
PATH_TO_READ=$1

make
for thread_num in {1..8}
do
        touch log.txt
        echo $thread_num >> log.txt
        for i in {1..128}
        do
                free &>/dev/null && sync && echo 3 > /proc/sys/vm/drop_caches && free &>/dev/null
                ./speed_test_multi $thread_num $PATH_TO_READ >> log.txt
        done
        echo "finish num_thread $thread_num"
        python3 extract.py log.txt $2_$thread_num
        rm log.txt
done
mv *.npy out
make clean
