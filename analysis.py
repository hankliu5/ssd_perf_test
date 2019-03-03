import numpy as np
from pathlib import Path
import matplotlib.pyplot as plt
import pandas as pd

tick = 500
p = Path('out')
file_list = [str(x) for x in p.iterdir()]
file_list.sort()
table = {}
# host / device
# different scheduler
# different number
# for example: 'out/device_raid_kyber_1.npy'
for filename in file_list:
    arr = np.load(filename)
    filename = filename.split('/')[-1].split('.')[0]
    tmp_ls = filename.split('_')
    platform = tmp_ls[0]
    thread_num = tmp_ls[-1]
    scheduler = '_'.join(tmp_ls[1:-1])
    if platform not in table:
        table[platform] = {}
    if scheduler not in table[platform]:
        table[platform][scheduler] = {}
    table[platform][scheduler][thread_num] = arr

for platform, platform_results in table.items():
    for scheduler, scheduler_results in platform_results.items():
        # first, compare the same platform/scheduler with different thread_num
        ls = []
        print('type: {}_{}'.format(platform, scheduler))
        print('thread_num\tmin (MB/s)\tavg (MB/s)\tmax (MB/s)\tstd(MB/s)\t')
        y_top_limit = 0
        for thread_num, thread_num_results in scheduler_results.items():
            max_bandwidth = np.max(thread_num_results)
            print('{}\t{}\t{}\t{}\t{}'.format(
                thread_num,
                np.min(thread_num_results),
                np.average(thread_num_results),
                max_bandwidth,
                np.std(thread_num_results)
            ))
            y_top_limit = max(y_top_limit, max_bandwidth)
            ls.append(thread_num_results)

        fig = plt.figure()
        fig.suptitle('{}_{}'.format(platform, scheduler),
                     fontsize=14, fontweight='bold')
        y_top_limit = ((int(y_top_limit) // tick) + 1) * tick
        ax = fig.add_subplot(111)
        ax.boxplot(ls)
        ax.set_xlabel('thread_num')
        ax.set_ylabel('throughput (MB/s)')
        ax.set_ylim(bottom=0, top=y_top_limit)
        ax.set_yticks([t for t in range(0, y_top_limit+1, tick)])
        plt.savefig('plot/{}_{}.png'.format(platform, scheduler))


# for RAID 0, 2 SSDs we know that when thread_num is 3, we can get the best throughput.
# so we compare different scheduler here:
for platform, platform_results in table.items():
    m = {}
    y_top_limit = 0
    for scheduler, scheduler_results in platform_results.items():
        y_top_limit = max(y_top_limit, np.max(scheduler_results['3']))
        m[scheduler] = scheduler_results['3']

    fig = plt.figure()
    fig.suptitle('{}_{} Comparison with 3 Threads'.format(platform, scheduler),
                 fontsize=14, fontweight='bold')
    y_top_limit = ((int(y_top_limit) // tick) + 1) * tick
    ax = fig.add_subplot(111)
    m = pd.DataFrame.from_dict(m)
    ax = m.boxplot(ax=ax)
    ax.set_xlabel('scheduler')
    ax.set_ylabel('throughput (MB/s)')
    ax.set_ylim(bottom=0, top=y_top_limit)
    ax.set_yticks([t for t in range(0, y_top_limit+1, tick)])
    plt.savefig('plot/{}_scheduler_compare.png'.format(platform))


# Lastly, compare host/device throughput differences
for scheduler in table['host'].keys():
    print(scheduler)
    if scheduler in table['device']:
        m = {}
        y_top_limit = 0
        for platform in ('host', 'device'):
            results = table[platform][scheduler]['3']
            y_top_limit = max(y_top_limit, np.max(results))
            m[platform] = results

        fig = plt.figure()
        fig.suptitle('Platform Comparison in {} with 3 Threads'.format(scheduler),
                        fontsize=14, fontweight='bold')
        y_top_limit = ((int(y_top_limit) // tick) + 1) * tick
        ax = fig.add_subplot(111)
        m = pd.DataFrame.from_dict(m)
        ax = m.boxplot(ax=ax)
        ax.set_xlabel('scheduler')
        ax.set_ylabel('throughput (MB/s)')
        ax.set_ylim(bottom=0, top=y_top_limit)
        ax.set_yticks([t for t in range(0, y_top_limit+1, tick)])
        plt.savefig('plot/platform_{}_compare.png'.format(scheduler))