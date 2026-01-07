import numpy as np
import matplotlib.pyplot as plt
import sys

# This script plots the results of signal generators (like LinearChirp)
# The signal is generated as CSV by signal_generator_test.cpp accepted on stdin.

# Read metadata lines
metadata = {}
for line_num in range(5):
    line = sys.stdin.readline()
    if line.startswith('#'):
        key_value = line[1:].strip().split('=')
        if len(key_value) == 2:
            key = key_value[0].strip()
            value = float(key_value[1].strip())
            metadata[key] = value

sampling_freq = metadata['SAMPLING_FREQ']
duration = metadata['DURATION']
start_freq = metadata['START_FREQ']
end_freq = metadata['END_FREQ']
amplitude = metadata['AMPLITUDE']

# Read CSV data
labels = np.loadtxt(sys.stdin, delimiter=',', max_rows=1, dtype=str)
data = np.loadtxt(sys.stdin, delimiter=',')

# Extract columns
sample_idx = data[:, 0]
time = data[:, 1]
linear_chirp = data[:, 2]
quadratic_chirp = data[:, 3]

# Create subplots
fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(14, 10))

# Plot 1: Time domain - full signal comparison
ax1.plot(time,
         linear_chirp,
         label='Linear',
         color='blue',
         alpha=0.7,
         linewidth=0.5)
ax1.plot(time,
         quadratic_chirp,
         label='Quadratic',
         color='red',
         alpha=0.7,
         linewidth=0.5)
ax1.set_xlabel('Time (s)')
ax1.set_ylabel('Amplitude')
ax1.set_title(f'Chirp Comparison: {start_freq:.1f} Hz → {end_freq:.1f} Hz')
ax1.legend()
ax1.grid(alpha=0.3)

# Plot 2: Time domain - zoomed in to show frequency progression
zoom_start = 0.0
zoom_end = 0.5  # First 500ms
zoom_mask = (time >= zoom_start) & (time <= zoom_end)
ax2.plot(time[zoom_mask],
         linear_chirp[zoom_mask],
         label='Linear',
         color='blue',
         linewidth=1.0)
ax2.plot(time[zoom_mask],
         quadratic_chirp[zoom_mask],
         label='Quadratic',
         color='red',
         linewidth=1.0)
ax2.set_xlabel('Time (s)')
ax2.set_ylabel('Amplitude')
ax2.set_title(f'Zoomed View: {zoom_start}s - {zoom_end}s')
ax2.legend()
ax2.grid(alpha=0.3)

# Plot 3: Spectrogram of linear chirp
window_size = int(sampling_freq * 0.1)  # 100ms window
overlap = int(window_size * 0.9)  # 90% overlap
ax3.specgram(linear_chirp,
             Fs=sampling_freq,
             NFFT=window_size,
             noverlap=overlap,
             cmap='viridis',
             vmin=-60,
             vmax=0)
ax3.set_xlabel('Time (s)')
ax3.set_ylabel('Frequency (Hz)')
ax3.set_title('Spectrogram: Linear Chirp')
ax3.set_ylim([0, min(end_freq * 1.2, sampling_freq / 2)])

# Add expected linear frequency line
expected_time = np.linspace(0, duration, 100)
linear_freq = start_freq + (end_freq - start_freq) * (expected_time / duration)
ax3.plot(expected_time,
         linear_freq,
         'r--',
         linewidth=2,
         label='Linear f(t)',
         alpha=0.7)
ax3.legend(loc='upper left')

# Plot 4: Spectrogram of quadratic chirp
ax4.specgram(quadratic_chirp,
             Fs=sampling_freq,
             NFFT=window_size,
             noverlap=overlap,
             cmap='viridis',
             vmin=-60,
             vmax=0)
ax4.set_xlabel('Time (s)')
ax4.set_ylabel('Frequency (Hz)')
ax4.set_title('Spectrogram: Quadratic Chirp')
ax4.set_ylim([0, min(end_freq * 1.2, sampling_freq / 2)])

# Add expected quadratic frequency line
quadratic_freq = start_freq + (end_freq - start_freq) * (expected_time /
                                                         duration)**2
ax4.plot(expected_time,
         quadratic_freq,
         'r--',
         linewidth=2,
         label='Quadratic f(t)',
         alpha=0.7)
ax4.legend(loc='upper left')

plt.tight_layout()
plt.show()
