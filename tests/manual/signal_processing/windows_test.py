import numpy as np
import matplotlib.pyplot as plt
import sys

# Simple CSV reader: first line is header, first column is index, rest are windows
header = sys.stdin.readline()
if not header:
    raise SystemExit('No input received on stdin')

labels = [s.strip() for s in header.strip().split(',')]

# Load remaining CSV data
data = np.loadtxt(sys.stdin, delimiter=',')
if data.ndim == 1:
    data = data[np.newaxis, :]

# x axis is the first column (index)
x = data[:, 0]
if labels and labels[0]:
    x_label = labels[0]
else:
    x_label = 'Index'

# Plot all other columns using their header names
fig, ax = plt.subplots(1, 1, figsize=(10, 5))
for i in range(1, len(labels)):
    if i >= data.shape[1]:
        continue
    ax.plot(x, data[:, i], label=labels[i])

ax.set_title('Window Functions - Time Domain')
ax.set_xlabel(x_label)
ax.set_ylabel('Amplitude')
ax.legend()
ax.grid()

plt.tight_layout()
plt.show()
