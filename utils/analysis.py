from pathlib import Path
import sys
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

p = Path(sys.argv[1])
file_list = [(f, f.name) for f in p.rglob('*.npy')]
file_list.sort(key=lambda tup: tup[1])
table = {}
best_among_threads = {}
best_among_scheduler = {}

# for example: 1SSD_device_none_7.npy
# <num of SSD>_<platform>_<scheduler>_<num of thread>.npy
for path, filename in file_list:
    arr = np.load(path)
    tmp_ls = filename.split('.')[0].split('_')
    num_of_SSD, platform, scheduler, num_of_thread = tmp_ls[0], tmp_ls[1], '_'.join(
        tmp_ls[2:-1]), tmp_ls[-1]
    if num_of_SSD not in table:
        table[num_of_SSD] = {}
        best_among_threads[num_of_SSD] = {}
        best_among_scheduler[num_of_SSD] = {}
    if platform not in table[num_of_SSD]:
        table[num_of_SSD][platform] = {}
        best_among_threads[num_of_SSD][platform] = {}
        best_among_scheduler[num_of_SSD][platform] = {
            'name': None, 'speed': 0, 'arr': None}
    if scheduler not in table[num_of_SSD][platform]:
        table[num_of_SSD][platform][scheduler] = {}
        best_among_threads[num_of_SSD][platform][scheduler] = {
            'num_of_thread': 0, 'speed': 0, 'arr': None}
    table[num_of_SSD][platform][scheduler][num_of_thread] = arr

# first, compare the same num_of_SSD/platform/scheduler with different thread_num
for num_of_SSD, num_of_SSD_results in table.items():
    print('='*20+'{}'.format(num_of_SSD)+'='*20)
    for platform, platform_results in num_of_SSD_results.items():
        for scheduler, scheduler_results in platform_results.items():
            m = {}
            print('type: {}_{}'.format(platform, scheduler))
            print(
                'thread_num\tmin (MB/s)\tavg (MB/s)\tmedian (MB/s)\tmax (MB/s)\tstd (MB/s)\t')
            for thread_num, thread_num_results in scheduler_results.items():
                avg_bandwidth = np.average(thread_num_results)
                print('{}\t{}\t{}\t{}\t{}\t{}'.format(
                    thread_num,
                    np.min(thread_num_results),
                    avg_bandwidth,
                    np.median(thread_num_results),
                    np.max(thread_num_results),
                    np.std(thread_num_results)
                ))
                m[thread_num] = thread_num_results
                if avg_bandwidth > best_among_scheduler[num_of_SSD][platform]['speed']:
                    best_among_scheduler[num_of_SSD][platform]['name'] = '{}_{}'.format(
                        scheduler, thread_num)
                    best_among_scheduler[num_of_SSD][platform]['speed'] = avg_bandwidth
                    best_among_scheduler[num_of_SSD][platform]['arr'] = thread_num_results
                if avg_bandwidth > best_among_threads[num_of_SSD][platform][scheduler]['speed']:
                    best_among_threads[num_of_SSD][platform][scheduler]['num_of_thread'] = thread_num
                    best_among_threads[num_of_SSD][platform][scheduler]['speed'] = avg_bandwidth
                    best_among_threads[num_of_SSD][platform][scheduler]['arr'] = thread_num_results

            fig = plt.figure()
            fig.suptitle('{}_{}'.format(platform, scheduler),
                         fontsize=14, fontweight='bold')
            ax = fig.add_subplot(111)
            m = pd.DataFrame.from_dict(m)
            ax = m.boxplot(ax=ax)
            ax.set_xlabel('thread_num')
            ax.set_ylabel('throughput (MB/s)')
            plt.savefig(
                sys.argv[2]+'/{}_{}_{}.png'.format(num_of_SSD, platform, scheduler))
            plt.close(fig)


# second, pick up the fastest num_of_threads for each scheduler and platform
for num_of_SSD, num_of_SSD_results in best_among_threads.items():
    for platform, platform_results in num_of_SSD_results.items():
        m = {}
        for scheduler, scheduler_results in platform_results.items():
            k = '{}_{}'.format(scheduler, scheduler_results['num_of_thread'])
            m[k] = scheduler_results['arr']
        fig = plt.figure()
        fig.suptitle('{} Comparison with different scheduler'.format(platform),
                     fontsize=14, fontweight='bold')
        ax = fig.add_subplot(111)
        m = pd.DataFrame.from_dict(m)
        ax = m.boxplot(ax=ax)
        ax.set_xlabel('scheduler')
        ax.set_ylabel('throughput (MB/s)')
        plt.savefig(
            sys.argv[2]+'/{}_{}_scheduler_compare.png'.format(num_of_SSD, platform))
        plt.close(fig)


# Third, compare host/device throughput differences
# Pick up the maximum bandwidth among schedulers in each platform
m = {}
for num_of_SSD, num_of_SSD_results in best_among_scheduler.items():
    for platform, platform_results in num_of_SSD_results.items():
        k = '{}_{}_{}'.format(num_of_SSD, platform, platform_results['name'])
        m[k] = platform_results['arr']
fig = plt.figure()
fig.suptitle('Platform Comparison', fontsize=14, fontweight='bold')
ax = fig.add_subplot(111)
m = pd.DataFrame.from_dict(m)
ax = m.boxplot(ax=ax)
ax.set_xlabel('scheduler')
ax.set_ylabel('throughput (MB/s)')
plt.savefig(sys.argv[2]+'/best_speed_compare.png')
plt.close(fig)
