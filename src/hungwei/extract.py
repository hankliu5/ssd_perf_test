import re
import numpy as np
import sys

TIME_UNIT = 1e-06
FILE_SIZE = 1024
PATTERN = '[1-9][0-9]+'

with open(sys.argv[1], 'r') as f:
    # first line is the thread number
    s = f.readline()
    file_size = FILE_SIZE * int(s)
    
    # every thread_num lines we find a maximum second
    tmp = []
    for line in f:
        tmp.append(line)
    
    s = ' '.join(tmp)
    total_arr = list(map(int, re.findall(PATTERN, s)))

    # calculate throughput for this thread_num
    time_arr = np.asarray(total_arr, dtype=np.float32) * TIME_UNIT
    result_arr = np.ones(time_arr.size) * file_size
    result_arr = result_arr / time_arr
    print('min: {}, median: {}, max: {}, std: {}'.format(np.min(result_arr), np.median(result_arr), np.max(result_arr), np.std(result_arr)))
    np.save(sys.argv[2], result_arr)
