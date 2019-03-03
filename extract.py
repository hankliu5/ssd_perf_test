import re
import numpy as np
import sys

TIME_UNIT = 1e-06
FILE_SIZE = 1024
PATTERN = '[1-9][0-9]+'

with open(sys.argv[1], 'r') as f:
    # first line is the thread number
    s = f.readline()
    thread_num = int(s)
    
    # every thread_num lines we find a maximum second
    count = 0
    total_arr = []
    tmp_ls = []
    for line in f:
        tmp_ls.append(line)
        count += 1
        if count == thread_num:
            s = ' '.join(tmp_ls)
            total_arr.append(max(list(map(int, re.findall(PATTERN, s)))))
            
            # reset the counter
            tmp_ls = []
            count = 0

    # calculate throughput for this thread_num
    time_arr = np.asarray(total_arr, dtype=np.float32) * TIME_UNIT
    result_arr = np.ones(time_arr.size) * FILE_SIZE * thread_num
    result_arr = result_arr / time_arr
    np.save(sys.argv[2], result_arr)
