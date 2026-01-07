import numpy as np
import matplotlib.pyplot as plt
import sys

# This script plots the results of the AGC (Automatic Gain Control) filter
# applied to a test signal. The signal is generated as CSV by agc_test.cpp
# accepted on stdin.
#
# The test signal has varying amplitude to demonstrate the AGC's ability to
# normalize the signal level.

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
target_rms = metadata['TARGET_RMS']
attack_time_sec = metadata['ATTACK_TIME_SEC']
release_time_sec = metadata['RELEASE_TIME_SEC']

# Read CSV data
labels = np.loadtxt(sys.stdin, delimiter=',', max_rows=1, dtype=str)
data = np.loadtxt(sys.stdin, delimiter=',')

# Extract columns
sample_idx = data[:, 0]
input_signal = data[:, 1]
output_signal = data[:, 2]
gain = data[:, 3]
current_rms = data[:, 4]

# Create time axis in seconds
time = sample_idx / sampling_freq

# Calculate RMS of output signal in a sliding window for verification
window_size = int(sampling_freq / 10)  # 100ms window
output_rms_windowed = np.zeros_like(output_signal)
for i in range(len(output_signal)):
    start_idx = max(0, i - window_size)
    end_idx = i + 1
    output_rms_windowed[i] = np.sqrt(
        np.mean(output_signal[start_idx:end_idx]**2))

# Create subplots
fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(14, 10))

# Plot 1: Input vs Output signals
ax1.plot(time, input_signal, label='Input', alpha=0.7, linewidth=0.8)
ax1.plot(time, output_signal, label='Output (AGC)', alpha=0.7, linewidth=0.8)
ax1.set_xlabel('Time (s)')
ax1.set_ylabel('Amplitude')
ax1.set_title('AGC Filter - Input vs Output Signals')
ax1.legend()
ax1.grid(alpha=0.3)

# Plot 2: Gain over time
ax2.plot(time, gain, label='Gain', color='green', linewidth=1.5)
ax2.set_xlabel('Time (s)')
ax2.set_ylabel('Gain')
ax2.set_title('AGC Gain Adjustment Over Time')
ax2.legend()
ax2.grid(alpha=0.3)

# Plot 3: RMS tracking
ax3.plot(time,
         current_rms,
         label='Current RMS (estimated)',
         color='blue',
         linewidth=1.5)
ax3.plot(time,
         output_rms_windowed,
         label='Output RMS (windowed)',
         color='orange',
         alpha=0.7,
         linewidth=1.0,
         linestyle='--')
ax3.axhline(y=target_rms,
            color='red',
            linestyle='--',
            linewidth=2,
            alpha=0.7,
            label=f'Target RMS: {target_rms}')
ax3.set_xlabel('Time (s)')
ax3.set_ylabel('RMS')
ax3.set_title('RMS Tracking (Attack: {:.1f}s, Release: {:.1f}s)'.format(
    attack_time_sec, release_time_sec))
ax3.legend()
ax3.grid(alpha=0.3)

# Plot 4: Input and Output envelopes (absolute values with smoothing)
input_envelope_window = int(sampling_freq / 50)  # 20ms window
input_envelope = np.convolve(np.abs(input_signal),
                             np.ones(input_envelope_window) /
                             input_envelope_window,
                             mode='same')
output_envelope = np.convolve(np.abs(output_signal),
                              np.ones(input_envelope_window) /
                              input_envelope_window,
                              mode='same')

ax4.plot(time, input_envelope, label='Input Envelope', color='blue', alpha=0.7)
ax4.plot(time,
         output_envelope,
         label='Output Envelope',
         color='orange',
         alpha=0.7)
# Target RMS line (multiplied by sqrt(2) to get approximate peak for sine wave)
target_peak = target_rms * np.sqrt(2)
ax4.axhline(y=target_peak,
            color='red',
            linestyle='--',
            linewidth=2,
            alpha=0.7,
            label=f'Target Peak: {target_peak:.3f}')
ax4.set_xlabel('Time (s)')
ax4.set_ylabel('Amplitude')
ax4.set_title('Signal Envelopes')
ax4.legend()
ax4.grid(alpha=0.3)

plt.tight_layout()
plt.show()
