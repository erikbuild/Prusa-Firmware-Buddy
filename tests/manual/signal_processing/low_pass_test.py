import numpy as np
import matplotlib.pyplot as plt
import sys

# This script plots the results of different low-pass filters applied to a test
# signal. The signal is generated as CSV by low_pass_test.cpp accepted on stdin.
#
# The first row contains metadata (sampling frequency).
# The second row contains labels of signals. The first column is the input signal.

# Read sampling frequency from metadata line
metadata_line = sys.stdin.readline()
assert metadata_line.startswith('# SAMPLING_FREQ=')
sampling_freq = int(metadata_line.split('=')[1])

metadata_line = sys.stdin.readline()
assert metadata_line.startswith('# CUTOFF_FREQ=')
cutoff_freq = int(metadata_line.split('=')[1])

labels = np.loadtxt(sys.stdin, delimiter=',', max_rows=1, dtype=str)
data = np.loadtxt(sys.stdin, delimiter=',')

# Create subplots: time domain, magnitude, and phase
fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(10, 12))

# Get number of samples
n_samples = data.shape[0]

# Plot time domain (raw values)
for i, label in enumerate(labels):
    series = data[:, i]
    ax1.plot(series, label=label)

ax1.set_xlabel('Sample')
ax1.set_ylabel('Amplitude')
ax1.set_title('Low-Pass Filter Comparison - Time Domain')
ax1.legend()
ax1.grid()

freqs = np.fft.fftfreq(n_samples, d=1.0 / sampling_freq)
pos_mask = freqs > 0.1  # Exclude very low frequencies for better log scale visualization
freqs_pos = freqs[pos_mask]

input_signal = data[:, 0]
input_fft = np.fft.fft(input_signal)
input_fft_pos = input_fft[pos_mask]

for i, label in enumerate(labels):
    series = data[:, i]

    fft_result = np.fft.fft(series)
    fft_pos = fft_result[pos_mask]

    transfer_function = fft_pos / (
        input_fft_pos + 1e-10)  # Add small value to avoid division by zero

    magnitude_db = 20 * np.log10(np.abs(transfer_function) + 1e-10)
    ax2.plot(freqs_pos, magnitude_db, label=label)

    phase_deg = np.angle(transfer_function, deg=True)
    ax3.plot(freqs_pos, phase_deg, label=label)

ax2.set_xlabel('Frequency (Hz)')
ax2.set_ylabel('Magnitude (dB)')
ax2.set_title('Bode Plot - Magnitude')
ax2.set_xscale('log')

# Plot vertical line at cutoff frequency
ax2.axvline(x=cutoff_freq,
            color='red',
            linestyle='--',
            linewidth=2,
            alpha=0.7,
            label=f'Cutoff: {cutoff_freq} Hz')
ax2.legend()
ax2.grid(which='both', alpha=0.3)

ax3.set_xlabel('Frequency (Hz)')
ax3.set_ylabel('Phase (degrees)')
ax3.set_title('Bode Plot - Phase')
ax3.set_xscale('log')

# Plot vertical line at cutoff frequency
ax3.axvline(x=cutoff_freq,
            color='red',
            linestyle='--',
            linewidth=2,
            alpha=0.7,
            label=f'Cutoff: {cutoff_freq} Hz')
ax3.legend()
ax3.grid(which='both', alpha=0.3)

plt.tight_layout()
plt.show()
