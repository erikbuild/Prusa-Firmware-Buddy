import numpy as np
import matplotlib.pyplot as plt
import sys

try:
    from scipy import signal as sp_signal
except ImportError:
    sp_signal = None


def parse_metadata(lines):
    meta = {}
    for line in lines:
        if not line.startswith('#'):
            break
        payload = line[1:].strip()
        if '=' not in payload:
            continue
        key, value = payload.split('=', 1)
        meta[key.strip()] = float(value.strip())
    return meta


def read_input():
    lines = sys.stdin.readlines()
    if not lines:
        raise SystemExit('No input received on stdin')

    meta_lines = []
    samples_header = None
    samples_lines = []
    tf_header = None
    tf_lines = []

    section = 'meta'
    for line in lines:
        if line.startswith('#'):
            meta_lines.append(line)
            if line.strip() == '# END_SAMPLES':
                section = 'tf'
            continue

        if section == 'meta':
            if samples_header is None:
                samples_header = line
            else:
                samples_lines.append(line)
        else:
            if tf_header is None:
                tf_header = line
            else:
                tf_lines.append(line)

    if samples_header is None or tf_header is None:
        raise SystemExit('Missing CSV header')

    samples_labels = [s.strip() for s in samples_header.strip().split(',')]
    tf_labels = [s.strip() for s in tf_header.strip().split(',')]

    samples = np.loadtxt(samples_lines, delimiter=',')
    if samples.ndim == 1:
        samples = samples[np.newaxis, :]
    tf_data = np.loadtxt(tf_lines, delimiter=',')
    if tf_data.ndim == 1:
        tf_data = tf_data[np.newaxis, :]

    return parse_metadata(
        meta_lines), samples_labels, samples, tf_labels, tf_data


def biquad_response(freqs, fs, b0, b1, b2, a1, a2):
    w = 2.0 * np.pi * freqs / fs
    z = np.exp(-1j * w)
    num = b0 + b1 * z + b2 * z * z
    den = 1.0 + a1 * z + a2 * z * z
    return num / den


def h1_from_samples(x, y, fs, window_size, overlap):
    if sp_signal is None:
        raise RuntimeError('scipy is required for H1 computation')
    noverlap = int(window_size * overlap)
    _, syx = sp_signal.csd(x,
                           y,
                           fs=fs,
                           window='hann',
                           nperseg=window_size,
                           noverlap=noverlap)
    _, sxx = sp_signal.welch(x,
                             fs=fs,
                             window='hann',
                             nperseg=window_size,
                             noverlap=noverlap)
    h1 = np.zeros_like(syx)
    mask = sxx > 1e-12
    h1[mask] = syx[mask] / sxx[mask]
    return h1


def main():
    meta, samples_labels, samples, tf_labels, tf_data = read_input()
    fs = meta['SAMPLING_FREQ']
    window_size = int(meta['WINDOW_SIZE'])
    overlap = meta['OVERLAP']
    b0 = meta['B0']
    b1 = meta['B1']
    b2 = meta['B2']
    a1 = meta['A1']
    a2 = meta['A2']

    freqs = tf_data[:, 0]
    noise_cpp = tf_data[:, 1] + 1j * tf_data[:, 2]
    chirp_cpp = tf_data[:, 3] + 1j * tf_data[:, 4]
    analytic = biquad_response(freqs, fs, b0, b1, b2, a1, a2)

    noise_in = samples[:, 1]
    noise_out = samples[:, 2]
    chirp_in = samples[:, 3]
    chirp_out = samples[:, 4]

    noise_py = h1_from_samples(noise_in, noise_out, fs, window_size, overlap)
    chirp_py = h1_from_samples(chirp_in, chirp_out, fs, window_size, overlap)

    fig, (ax_mag, ax_phase) = plt.subplots(2, 1, figsize=(10, 8))

    eps = 1e-12
    ax_mag.plot(freqs, 20 * np.log10(np.abs(analytic) + eps), label='Analytic')
    ax_mag.plot(freqs,
                20 * np.log10(np.abs(noise_cpp) + eps),
                label='C++ Noise H1')
    ax_mag.plot(freqs,
                20 * np.log10(np.abs(chirp_cpp) + eps),
                label='C++ Chirp H1')
    ax_mag.plot(freqs,
                20 * np.log10(np.abs(noise_py) + eps),
                '--',
                label='Py Noise H1')
    ax_mag.plot(freqs,
                20 * np.log10(np.abs(chirp_py) + eps),
                '--',
                label='Py Chirp H1')
    ax_mag.set_title('Transfer Function Magnitude')
    ax_mag.set_xlabel('Frequency (Hz)')
    ax_mag.set_ylabel('Magnitude (dB)')
    ax_mag.grid()
    ax_mag.legend()

    ax_phase.plot(freqs, np.unwrap(np.angle(analytic)), label='Analytic')
    ax_phase.plot(freqs, np.unwrap(np.angle(noise_cpp)), label='C++ Noise H1')
    ax_phase.plot(freqs, np.unwrap(np.angle(chirp_cpp)), label='C++ Chirp H1')
    ax_phase.plot(freqs,
                  np.unwrap(np.angle(noise_py)),
                  '--',
                  label='Py Noise H1')
    ax_phase.plot(freqs,
                  np.unwrap(np.angle(chirp_py)),
                  '--',
                  label='Py Chirp H1')
    ax_phase.set_title('Transfer Function Phase')
    ax_phase.set_xlabel('Frequency (Hz)')
    ax_phase.set_ylabel('Phase (rad)')
    ax_phase.grid()
    ax_phase.legend()

    plt.tight_layout()
    plt.show()


if __name__ == '__main__':
    main()
