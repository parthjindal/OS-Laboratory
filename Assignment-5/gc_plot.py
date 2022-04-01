import matplotlib.pyplot as plt
import pandas as pd
import numpy as np

# max_memory = 350000 / 256.0

fd = pd.read_csv('gc.csv')
gc_data = fd.loc[:, "gc"].to_numpy()

fd = pd.read_csv('non_gc.csv')
non_gc_data = fd.loc[:, "non_gc"].to_numpy()

# in KB
gc_data = gc_data
non_gc_data = non_gc_data

_temp = []
prev = -1
j = -1
for i in gc_data:
    if i > prev:
        j += 1
        _temp.append(non_gc_data[j])
    else:
        _temp.append(non_gc_data[j])
    prev = i

non_gc_data = np.array(_temp)
# non_gc_data[-1] = 0.00
gc_avg = np.mean(gc_data)
gc_std_dev = np.std(gc_data)
gc_max = np.max(gc_data)

non_gc_avg = np.mean(non_gc_data)
non_gc_std_dev = np.std(non_gc_data)
non_gc_max = np.max(non_gc_data)

print('GC: mean = {}, std_dev = {}, max = {}'.format(gc_avg, gc_std_dev, gc_max))
print('Non-GC: mean = {}, std_dev = {}, max = {}'.format(non_gc_avg,
      non_gc_std_dev, non_gc_max))

plt.plot(gc_data, 'ro-', label='GC')
plt.xlabel('Instruction flow')
plt.ylabel('Memory usage (in Bytes)')
plt.plot(non_gc_data, 'bo-', label='Non-GC')
# plt.axhline(y=max_memory, color='g', linestyle='-', label='Max memory')
plt.legend()
plt.tight_layout()
plt.savefig('gc_plot.png')
plt.show()
