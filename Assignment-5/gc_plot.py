import matplotlib.pyplot as plt
import pandas as pd
import numpy as np

max_memory = 10

fd = pd.read_csv('gc_data.csv')
gc_data = fd.loc[:, "gc"].to_numpy()
non_gc_data = fd.loc[:, "non_gc"]

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
plt.xlabel('Instruction')
plt.ylabel('Memory usage')
plt.plot(non_gc_data, 'bo-', label='Non-GC')
plt.axhline(y=max_memory, color='g', linestyle='-',label='Max memory')
plt.legend()
plt.savefig('gc_plot.png')
