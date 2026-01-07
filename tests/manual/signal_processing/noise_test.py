import numpy as np
import matplotlib.pyplot as plt
import sys

# This script plots the results of the GaussianNoise generator
# The signal is generated as CSV by noise_test.cpp accepted on stdin.

# Read metadata lines
metadata = {}
for line_num in range(4):
    line = sys.stdin.readline()
    if line.startswith('#'):
        key_value = line[1:].strip().split('=')
        if len(key_value) == 2:
            key = key_value[0].strip()
            value = float(key_value[1].strip())
            metadata[key] = value

sampling_freq = metadata['SAMPLING_FREQ']
test_samples = int(metadata['TEST_SAMPLES'])
target_rms = metadata['TARGET_RMS']
seed = int(metadata['SEED'])

# Read CSV data
labels = np.loadtxt(sys.stdin, delimiter=',', max_rows=1, dtype=str)
data = np.loadtxt(sys.stdin, delimiter=',')

# Extract columns
sample_idx = data[:, 0]
time = data[:, 1]
noise = data[:, 2]
filtered = data[:, 3]

# Calculate actual RMS
actual_rms = np.sqrt(np.mean(noise**2))
filtered_rms = np.sqrt(np.mean(filtered**2))

# Create subplots
fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(14, 10))

# Plot 1: Time domain - raw noise
plot_samples = min(1000, len(time))  # Show first 1000 samples
ax1.plot(time[:plot_samples],
         noise[:plot_samples],
         color='blue',
         alpha=0.7,
         linewidth=0.5)
ax1.axhline(y=target_rms,
            color='red',
            linestyle='--',
            linewidth=1,
            alpha=0.7,
            label=f'Target RMS: {target_rms:.3f}')
ax1.axhline(y=-target_rms, color='red', linestyle='--', linewidth=1, alpha=0.7)
ax1.set_xlabel('Time (s)')
ax1.set_ylabel('Amplitude')
ax1.set_title(f'Gaussian White Noise (Seed: {seed})')
ax1.legend()
ax1.grid(alpha=0.3)

# Plot 2: Time domain - filtered vs raw
ax2.plot(time[:plot_samples],
         noise[:plot_samples],
         label='Raw',
         color='blue',
         alpha=0.5,
         linewidth=0.5)
ax2.plot(time[:plot_samples],
         filtered[:plot_samples],
         label='Filtered (50 Hz LPF)',
         color='red',
         alpha=0.7,
         linewidth=0.8)
ax2.set_xlabel('Time (s)')
ax2.set_ylabel('Amplitude')
ax2.set_title('Raw vs Filtered Noise')
ax2.legend()
ax2.grid(alpha=0.3)

# Plot 3: Power Spectral Density
freqs = np.fft.rfftfreq(len(noise), 1 / sampling_freq)
noise_fft = np.fft.rfft(noise)
noise_psd = np.abs(noise_fft)**2 / len(noise)
noise_psd_db = 10 * np.log10(noise_psd + 1e-10)

filtered_fft = np.fft.rfft(filtered)
filtered_psd = np.abs(filtered_fft)**2 / len(filtered)
filtered_psd_db = 10 * np.log10(filtered_psd + 1e-10)

ax3.plot(freqs,
         noise_psd_db,
         label='Raw Noise',
         color='blue',
         alpha=0.7,
         linewidth=1)
ax3.plot(freqs,
         filtered_psd_db,
         label='Filtered',
         color='red',
         alpha=0.7,
         linewidth=1)
ax3.set_xlabel('Frequency (Hz)')
ax3.set_ylabel('Power (dB)')
ax3.set_title('Power Spectral Density')
ax3.set_xlim([0, sampling_freq / 2])
ax3.legend()
ax3.grid(alpha=0.3)

# Plot 4: Histogram and Gaussian fit
ax4.hist(noise,
         bins=50,
         density=True,
         alpha=0.7,
         color='blue',
         label='Noise samples')

# Overlay theoretical Gaussian
x = np.linspace(noise.min(), noise.max(), 100)
gaussian = (1 /
            (target_rms * np.sqrt(2 * np.pi))) * np.exp(-x**2 /
                                                        (2 * target_rms**2))
ax4.plot(x,
         gaussian,
         'r-',
         linewidth=2,
         label=f'Gaussian (σ={target_rms:.3f})')

ax4.set_xlabel('Amplitude')
ax4.set_ylabel('Probability Density')
ax4.set_title(f'Distribution (Actual RMS: {actual_rms:.3f})')
ax4.legend()
ax4.grid(alpha=0.3)

# Add text with statistics
stats_text = f'Target RMS: {target_rms:.4f}\n'
stats_text += f'Actual RMS: {actual_rms:.4f}\n'
stats_text += f'Error: {abs(actual_rms - target_rms):.4f} ({abs(actual_rms - target_rms) / target_rms * 100:.2f}%)\n'
stats_text += f'Filtered RMS: {filtered_rms:.4f}'
fig.text(0.02,
         0.98,
         stats_text,
         transform=fig.transFigure,
         verticalalignment='top',
         fontfamily='monospace',
         bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))

plt.tight_layout()
plt.show()
