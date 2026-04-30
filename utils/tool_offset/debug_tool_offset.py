#!/usr/bin/env python3
"""
Standalone G426/G427 tool offset debug script.

Captures G426/G427 debug output from the printer and displays C++ analysis results.

Usage:
    # Single probe from printer, save raw data
    python debug_tool_offset.py single --save capture.json

    # Replay saved data with plots
    python debug_tool_offset.py single --load capture.json --show

    # Repeatability: 10 probes, save all
    python debug_tool_offset.py repeat -N 10 --save runs.json

    # Replay saved repeatability data
    python debug_tool_offset.py repeat --load runs.json --show

    # G427 full calibration: all mapped tools
    python debug_tool_offset.py all_tools --save calib.json

    # Replay G427 calibration data
    python debug_tool_offset.py all_tools --load calib.json --show
"""
from __future__ import annotations

import functools
import json
import re
import statistics
from contextlib import contextmanager
from dataclasses import dataclass, field, fields, MISSING
from pathlib import Path
from typing import Any, Dict, List, Optional

import click
import serial.tools.list_ports
from serial import Serial

import plotly.graph_objects as go
from plotly.subplots import make_subplots

PRUSA_VID = 0x2C99

_JSON_ALIASES = {"pass_num": "pass"}


def _from_json(cls, data: Dict[str, Any]):
    """Generic dataclass constructor that coerces JSON values to field types."""
    kwargs = {}
    for f in fields(cls):
        json_key = _JSON_ALIASES.get(f.name, f.name)

        if f.default is not MISSING:
            fallback = f.default
        elif f.default_factory is not MISSING:
            fallback = None
        else:
            fallback = None

        raw = data.get(json_key, fallback)

        # With __future__.annotations, f.type is a string
        if f.type in ("float", ):
            kwargs[f.name] = float(raw or 0)
        elif f.type in ("int", ):
            kwargs[f.name] = int(raw or 0)
        elif f.type in ("str", ):
            kwargs[f.name] = str(raw or "")
        elif f.type == "List[float]":
            if raw is None and f.default_factory is not MISSING:
                kwargs[f.name] = f.default_factory()
            else:
                kwargs[f.name] = [float(x) for x in (raw or [])]
        elif f.type == "List[int]":
            if raw is None and f.default_factory is not MISSING:
                kwargs[f.name] = f.default_factory()
            else:
                kwargs[f.name] = [int(x) for x in (raw or [])]
        else:
            kwargs[f.name] = raw

    return cls(**kwargs)


class Machine:

    def __init__(self, port: Serial) -> None:
        self._port = port

    @contextmanager
    def _preserveTimeout(self):
        orig = self._port.timeout
        try:
            yield
        finally:
            self._port.timeout = orig

    def waitForBoot(self) -> None:
        self.command("G")

    def command(self, command: str, timeout: float = 100) -> List[str]:
        if not command.endswith("\n"):
            command += "\n"
        with self._preserveTimeout():
            self._port.timeout = None
            self._port.read_all()
            self._port.write(command.encode("utf-8"))
            response: List[str] = []
            if timeout != 0:
                self._port.timeout = timeout
            else:
                self._port.timeout = 3
            while True:
                line = self._port.readline().decode("utf-8").strip()
                if line == "":
                    if timeout != 0:
                        raise TimeoutError(
                            f"No response on command {command.strip()}")
                    else:
                        return response
                if line.endswith("ok"):
                    rest = line[:-2].strip()
                    if rest:
                        response.append(rest)
                    return response
                response.append(line)


def getPrusaPort() -> Optional[str]:
    for port in serial.tools.list_ports.comports():
        if port.vid == PRUSA_VID:
            return port.device
    return None


@contextmanager
def enabledMachineConnection(port: str = None):
    if port is None:
        port = getPrusaPort()
    if port is None:
        raise RuntimeError("No Prusa printer found. Specify port with --port")
    with Serial(port) as s:
        m = Machine(s)
        m.waitForBoot()
        m.command("M17")
        try:
            yield m
        finally:
            m.command("M18")


@dataclass
class LineSamples:
    label: str
    samples: List[float]
    sampling_freq_hz: float


@dataclass
class MotionProfile:
    label: str
    speed1: float
    speed2: float
    rest_time: float
    time_s: List[float] = field(default_factory=list)
    position_mm: List[float] = field(default_factory=list)
    velocity_mm_s: List[float] = field(default_factory=list)

    @staticmethod
    def from_json(data: Dict[str, Any]) -> MotionProfile:
        time_s, pos, vel = [], [], []
        for row in data.get("samples", []):
            if len(row) >= 3:
                time_s.append(float(row[0]))
                pos.append(float(row[1]))
                vel.append(float(row[2]))
        return MotionProfile(
            label=data.get("label", ""),
            speed1=float(data.get("speed1", 0)),
            speed2=float(data.get("speed2", 0)),
            rest_time=float(data.get("rest_time", 0)),
            time_s=time_s,
            position_mm=pos,
            velocity_mm_s=vel,
        )


@dataclass
class MotionTiming:
    label: str
    convert_ms: float
    steps_ms: float
    expected_ms: float
    actual_ms: float
    sensor_offset_ms: float


@dataclass
class CppPeaks:
    label: str
    t1: float
    t2: float
    t3: float
    t4: float
    confidence: float
    name: str = ""


@dataclass
class CppEstimate:
    pos: float
    tau: float
    residual: float


@dataclass
class CppEstimates:
    label: str
    all: CppEstimate
    forward: CppEstimate
    backward: CppEstimate
    name: str = ""

    @staticmethod
    def from_json(data: Dict[str, Any]) -> CppEstimates:
        return CppEstimates(
            label=data.get("label", ""),
            all=_from_json(CppEstimate, data.get("all", {})),
            forward=_from_json(CppEstimate, data.get("forward", {})),
            backward=_from_json(CppEstimate, data.get("backward", {})),
            name=data.get("name", ""),
        )


@dataclass
class CppDeltas:
    label: str
    delta_12_obs: float
    delta_34_obs: float
    delta_12_model: float
    delta_34_model: float
    delta_13_obs: float
    delta_24_obs: float
    name: str = ""


@dataclass
class CppSymmetry:
    label: str
    pass_num: int
    peak_time: float
    confidence: float
    correlation_peak: float
    # First-pass (untrimmed) correlation score; defaults to correlation_peak
    # when older firmware did not emit it separately.
    correlation_peak_full: float = 0.0
    name: str = ""


@dataclass
class FinalOffset:
    x_mm: Optional[float] = None
    y_mm: Optional[float] = None
    z_mm: Optional[float] = None
    x_confidence: Optional[float] = None
    y_confidence: Optional[float] = None


@dataclass
class PassPreprocessed:
    label: str
    pass_num: int
    chunk_start: int
    chunk_size: int
    dt: float
    samples: List[float]


@dataclass
class PassDerivative:
    label: str
    pass_num: int
    samples: List[float]


@dataclass
class PassCorrelation:
    label: str
    pass_num: int
    fine_min: int
    fine_max: int
    best_lag_value: int
    best_lag_deriv: int
    best_lag_combined: int
    value_scores: List[float]
    deriv_scores: List[float]
    combined_scores: List[float]

    @staticmethod
    def from_json(data: Dict[str, Any]) -> PassCorrelation:
        if "value_scores" in data:
            return PassCorrelation(
                label=data.get("label", ""),
                pass_num=int(data.get("pass", 0)),
                fine_min=int(data.get("fine_min", 0)),
                fine_max=int(data.get("fine_max", 0)),
                best_lag_value=int(data.get("best_lag_value", 0)),
                best_lag_deriv=int(data.get("best_lag_deriv", 0)),
                best_lag_combined=int(data.get("best_lag_combined", 0)),
                value_scores=[float(s) for s in data.get("value_scores", [])],
                deriv_scores=[float(s) for s in data.get("deriv_scores", [])],
                combined_scores=[
                    float(s) for s in data.get("combined_scores", [])
                ],
            )
        # Backward compat: old format with single "scores" and "best_lag"
        bl = int(data.get("best_lag", 0))
        scores = [float(s) for s in data.get("scores", [])]
        return PassCorrelation(
            label=data.get("label", ""),
            pass_num=int(data.get("pass", 0)),
            fine_min=int(data.get("fine_min", 0)),
            fine_max=int(data.get("fine_max", 0)),
            best_lag_value=bl,
            best_lag_deriv=bl,
            best_lag_combined=bl,
            value_scores=[],
            deriv_scores=[],
            combined_scores=scores,
        )


@dataclass
class PassTrimRefine:
    label: str
    pass_num: int
    n_full: int
    n_kept: int
    trim_start: int
    pass1_lag: int
    pass1_score: float
    pass1_refined: float
    pass2_lag: int
    pass2_score: float
    pass2_refined: float

    @staticmethod
    def from_json(data: Dict[str, Any]) -> PassTrimRefine:
        return PassTrimRefine(
            label=data.get("label", ""),
            pass_num=int(data.get("pass", 0)),
            n_full=int(data.get("n_full", 0)),
            n_kept=int(data.get("n_kept", 0)),
            trim_start=int(data.get("trim_start", 0)),
            pass1_lag=int(data.get("pass1_lag", 0)),
            pass1_score=float(data.get("pass1_score", 0.0)),
            pass1_refined=float(data.get("pass1_refined", 0.0)),
            pass2_lag=int(data.get("pass2_lag", 0)),
            pass2_score=float(data.get("pass2_score", 0.0)),
            pass2_refined=float(data.get("pass2_refined", 0.0)),
        )


@dataclass
class PassRawChunk:
    label: str
    pass_num: int
    chunk_start: int
    chunk_size: int
    samples: List[float]


@dataclass
class RoughAlignEnergy:
    label: str
    decimation: int
    dt_dec: float
    threshold: float
    offset: int
    regions: List[List[int]]
    energy: List[float]

    @staticmethod
    def from_json(data: Dict[str, Any]) -> RoughAlignEnergy:
        return RoughAlignEnergy(
            label=data.get("label", ""),
            decimation=int(data.get("decimation", 1)),
            dt_dec=float(data.get("dt_dec", 0)),
            threshold=float(data.get("threshold", 0)),
            offset=int(data.get("offset", 0)),
            regions=[list(r) for r in data.get("regions", [])],
            energy=[float(e) for e in data.get("energy", [])],
        )


@dataclass
class RoughAlignScore:
    label: str
    dt_dec: float
    k_min: int
    k_max: int
    k_best: int
    score_over_baseline: float
    score_curve: List[float]

    @staticmethod
    def from_json(data: Dict[str, Any]) -> RoughAlignScore:
        return RoughAlignScore(
            label=data.get("label", ""),
            dt_dec=float(data.get("dt_dec", 0)),
            k_min=int(data.get("k_min", 0)),
            k_max=int(data.get("k_max", 0)),
            k_best=int(data.get("k_best", 0)),
            score_over_baseline=float(data.get("score_over_baseline", 0)),
            score_curve=[float(s) for s in data.get("score_curve", [])],
        )


OFFSET_RE = re.compile(
    r"([XYZ]) offset:\s*([-\d.]+)\s*mm(?:\s*\(confidence:\s*([-\d.]+)\))?")

_PARSERS = [
    ("# line_samples", "line_samples", lambda d: _from_json(LineSamples, d)),
    ("# motion_profile", "motion_profile", MotionProfile.from_json),
    ("# motion_timing", "motion_timing",
     lambda d: _from_json(MotionTiming, d)),
    ("# twospeed_peaks", "peaks", lambda d: _from_json(CppPeaks, d)),
    ("# twospeed_estimates", "estimates", CppEstimates.from_json),
    ("# twospeed_deltas", "deltas", lambda d: _from_json(CppDeltas, d)),
    ("# twospeed_symmetry", "symmetry", lambda d: _from_json(CppSymmetry, d)),
    ("# pass_preprocessed", "pass_preprocessed",
     lambda d: _from_json(PassPreprocessed, d)),
    ("# pass_derivative", "pass_derivative",
     lambda d: _from_json(PassDerivative, d)),
    ("# pass_correlation", "pass_correlation", PassCorrelation.from_json),
    ("# pass_trim_refine", "pass_trim_refine", PassTrimRefine.from_json),
    ("# pass_raw_chunk", "pass_raw_chunk",
     lambda d: _from_json(PassRawChunk, d)),
    ("# rough_align_energy", "rough_align_energy", RoughAlignEnergy.from_json),
    ("# rough_align_score", "rough_align_score", RoughAlignScore.from_json),
]

_NOISE_RE = re.compile(r'echo:busy: processing'
                       r'|^T:\d'
                       r'|^X:\d'
                       r'|^echo:Unknown command')


def _reassemble_lines(response: List[str]) -> List[str]:
    """Reassemble debug lines fragmented by auto-report interleaving.

    The streaming line_samples reporter writes samples one-by-one.
    Auto-report messages (echo:busy, temperature) can get spliced in
    mid-line, breaking it into multiple readline() results.  Detect
    incomplete debug lines and stitch them back together.
    """
    out: List[str] = []
    pending: Optional[str] = None

    for line in response:
        if pending is not None:
            # Strip noise injected into the middle of a debug line
            cleaned = _NOISE_RE.sub('', line).strip()
            if cleaned:
                pending += cleaned
            # Check if the JSON is now complete (ends with })
            if pending.rstrip().endswith('}'):
                out.append(pending)
                pending = None
            continue

        if line.startswith('#'):
            # Check if this debug line looks incomplete (no closing brace)
            if not line.rstrip().endswith('}'):
                pending = line
                # Strip trailing noise from the first fragment
                pending = _NOISE_RE.sub('', pending)
                continue

        out.append(line)

    if pending is not None:
        out.append(pending)  # flush incomplete line as-is

    return out


def parse_response(response: List[str]) -> Dict[str, Any]:
    response = _reassemble_lines(response)
    data: Dict[str, list] = {key: [] for _, key, _ in _PARSERS}

    final = FinalOffset()

    for line in response:
        for prefix, key, factory in _PARSERS:
            if line.startswith(prefix):
                try:
                    json_str = line.split(" ", maxsplit=2)[2]
                    # Firmware may emit -inf/inf/nan which aren't valid JSON
                    json_str = re.sub(r'(?<=[\s:,\[])(-inf|inf|nan)\b',
                                      r'"\1"', json_str)
                    data[key].append(factory(json.loads(json_str)))
                except (IndexError, json.JSONDecodeError) as e:
                    print(f"  Warning: parse error on '{prefix}': {e}")
                break

        # Final offset lines: "X offset: 0.44 mm (confidence: 0.61)"
        for m in OFFSET_RE.finditer(line):
            axis, val, conf = m.group(1), float(m.group(2)), m.group(3)
            conf_f = float(conf) if conf else None
            if axis == "X":
                final.x_mm, final.x_confidence = val, conf_f
            elif axis == "Y":
                final.y_mm, final.y_confidence = val, conf_f
            elif axis == "Z":
                final.z_mm = val

    # Extract offsets from scan_result debug lines when G426-style
    # "X offset: ..." lines are absent (e.g. G427).
    # Prefer nozzle-offset results; fall back to center-detection.
    if final.x_mm is None or final.y_mm is None:
        scan_results: Dict[str, tuple] = {}
        for line in response:
            if line.startswith("# scan_result"):
                try:
                    sr = json.loads(line.split(" ", maxsplit=2)[2])
                    scan_results[sr["label"]] = (
                        float(sr.get("position_mm", 0)),
                        float(sr.get("confidence", 0)),
                    )
                except (IndexError, json.JSONDecodeError, KeyError):
                    pass
        for axis, preferred, fallback in [
            ("x", "nozzle-offset-x", "center-detection-x"),
            ("y", "nozzle-offset-y", "center-detection-y"),
        ]:
            sr = scan_results.get(preferred) or scan_results.get(fallback)
            if sr:
                val, conf = sr
                if axis == "x" and final.x_mm is None:
                    final.x_mm, final.x_confidence = val, conf
                elif axis == "y" and final.y_mm is None:
                    final.y_mm, final.y_confidence = val, conf

    data["final"] = final
    return data


def match_by_label(items: list, label_fragment: str):
    for item in items:
        if label_fragment in item.label:
            return item
    return None


def save_raw(path: str, response: List[str]) -> None:
    with open(path, "w") as f:
        json.dump({"raw_response": response}, f, indent=2)
    print(f"Saved raw output to {path}")


def load_raw(path: str) -> List[str]:
    with open(path) as f:
        return json.load(f)["raw_response"]


def save_raw_multi(path: str, responses: List[List[str]]) -> None:
    with open(path, "w") as f:
        json.dump({"runs": [{
            "raw_response": r
        } for r in responses]},
                  f,
                  indent=2)
    print(f"Saved {len(responses)} runs to {path}")


def load_raw_multi(path: str) -> List[List[str]]:
    with open(path) as f:
        data = json.load(f)
    # Support both multi-run format and single-run format
    if "runs" in data:
        return [run["raw_response"] for run in data["runs"]]
    if "raw_response" in data:
        return [data["raw_response"]]
    raise ValueError(f"Unrecognized JSON format in {path}")


def build_g426(speed1, speed2, diameter, zheight) -> str:
    cmd = "G426"
    for letter, value in [("F", speed1), ("R", speed2), ("D", diameter),
                          ("Z", zheight)]:
        if value is not None:
            cmd += f" {letter}{value}"
    return cmd


def build_g427(r_param=None, probe_count=None) -> str:
    cmd = "G427"
    for letter, value in [("R", r_param), ("P", probe_count)]:
        if value is not None:
            cmd += f" {letter}{value}"
    return cmd


def split_response_by_tool(response: List[str]) -> List[List[str]]:
    """Split a G427 multi-tool response into per-tool segments.

    Each tool's XY calibration starts with a center-detection-x line_samples
    entry. Non-debug lines before the first tool are kept in the first segment.
    """
    BOUNDARY = '# line_samples {"label": "center-detection-x"'
    segments: List[List[str]] = []
    current: List[str] = []
    found_first = False

    for line in response:
        if line.startswith(BOUNDARY):
            if found_first and current:
                segments.append(current)
                current = []
            found_first = True
        current.append(line)

    if current:
        segments.append(current)

    return segments if segments else [response]


def print_axis_results(
    axis: str,
    peaks: Optional[CppPeaks],
    est: Optional[CppEstimates],
    deltas: Optional[CppDeltas],
    symmetry: List[CppSymmetry],
    timing: Optional[MotionTiming],
    samples: Optional[LineSamples],
    profile: Optional[MotionProfile],
    diameter: float,
    trim_refines: Optional[List[PassTrimRefine]] = None,
):
    center = diameter / 2.0
    title = _scan_title(axis, peaks)
    print(f"\n{'=' * 70}")
    print(f"  {title}")
    print(f"{'=' * 70}")

    if samples:
        print(
            f"  Samples: {len(samples.samples)} @ {samples.sampling_freq_hz:.1f} Hz"
        )
    if timing:
        print(
            f"  Timing: sensor_offset={timing.sensor_offset_ms:.3f} ms, "
            f"actual={timing.actual_ms:.0f} ms (expected {timing.expected_ms:.0f} ms)"
        )

    pass_labels = _make_pass_labels(profile)
    if peaks:
        print(f"\n  {'Pass':<24} {'Peak time (s)':<16}")
        print(f"  {'-' * 40}")
        for t, lbl in zip(
            [peaks.t1, peaks.t2, peaks.t3, peaks.t4],
                pass_labels,
        ):
            print(f"  {lbl:<24} {t:<16.6f}")
        print(f"  Confidence: {peaks.confidence:.3f}")

    if est:
        print(
            f"\n  {'Estimate':<16} {'Position (mm)':<16} {'Offset (mm)':<14} {'tau (ms)':<12} {'Residual'}"
        )
        print(f"  {'-' * 70}")
        for lbl, e in [("All 4 peaks", est.all), ("Forward", est.forward),
                       ("Backward", est.backward)]:
            print(
                f"  {lbl:<16} {e.pos:<16.4f} {e.pos - center:>+12.4f}   {e.tau * 1000:<12.3f} {e.residual:.6f}"
            )

    if deltas:
        print(f"\n  {'Delta':<10} {'Observed (ms)':<16} {'Model (ms)'}")
        print(f"  {'-' * 42}")
        print(
            f"  {'d12':<10} {deltas.delta_12_obs * 1000:<16.3f} {deltas.delta_12_model * 1000:.3f}"
        )
        print(
            f"  {'d34':<10} {deltas.delta_34_obs * 1000:<16.3f} {deltas.delta_34_model * 1000:.3f}"
        )
        print(f"  {'d13':<10} {deltas.delta_13_obs * 1000:<16.3f}")
        print(f"  {'d24':<10} {deltas.delta_24_obs * 1000:<16.3f}")

    if symmetry:
        print(
            f"\n  {'Pass':<8} {'Peak time (s)':<16} {'Confidence':<14} {'Corr full':<12} {'Corr trimmed':<14} {'Δ'}"
        )
        print(f"  {'-' * 70}")
        for s in symmetry:

            def _fmt(v: float) -> str:
                return f"{v:.4f}" if v != float("-inf") else "-inf"

            full_str = _fmt(s.correlation_peak_full)
            trim_str = _fmt(s.correlation_peak)
            delta_str = (f"{s.correlation_peak - s.correlation_peak_full:+.4f}"
                         if s.correlation_peak_full != 0.0
                         and s.correlation_peak != float("-inf") else "—")
            print(
                f"  {s.pass_num:<8} {s.peak_time:<16.6f} {s.confidence:<14.3f} "
                f"{full_str:<12} {trim_str:<14} {delta_str}")

    if trim_refines:
        print(
            f"\n  {'Pass':<6} {'n_full':<8} {'n_kept':<8} {'lag1':<8} {'lag1_ref':<10} "
            f"{'lag2':<8} {'lag2_ref':<10} {'Δlag':<10}")
        print(f"  {'-' * 72}")
        for tr in trim_refines:
            d_lag = tr.pass2_refined - tr.pass1_refined
            print(
                f"  {tr.pass_num:<6} {tr.n_full:<8} {tr.n_kept:<8} "
                f"{tr.pass1_lag:<8} {tr.pass1_refined:<10.3f} "
                f"{tr.pass2_lag:<8} {tr.pass2_refined:<10.3f} {d_lag:<+10.3f}")


def print_final_offset(final: FinalOffset) -> None:
    print(f"\n{'=' * 70}")
    print("  FINAL OFFSET")
    print(f"{'=' * 70}")
    parts = []
    for name, val, conf in [
        ("X", final.x_mm, final.x_confidence),
        ("Y", final.y_mm, final.y_confidence),
        ("Z", final.z_mm, None),
    ]:
        if val is not None:
            c = f" (conf: {conf:.2f})" if conf is not None else ""
            parts.append(f"  {name}: {val:+.4f} mm{c}")
    print("\n".join(parts) if parts else "  No offset data found")
    print()


def median_filter(data: List[float], kernel: int = 5) -> List[float]:
    half = kernel // 2
    n = len(data)
    out = []
    for i in range(n):
        lo = max(0, i - half)
        hi = min(n, i + half + 1)
        out.append(sorted(data[lo:hi])[len(data[lo:hi]) // 2])
    return out


PASS_COLORS = ["orange", "purple", "green", "magenta"]
CORR_COLORS = {"value": "blue", "deriv": "red", "combined": "black"}
PASS_BG_COLORS = [
    "rgba(255,165,0,0.08)",
    "rgba(128,0,128,0.08)",
    "rgba(0,128,0,0.08)",
    "rgba(255,0,255,0.08)",
]
PASS_DIRECTIONS = ["fwd", "bwd", "fwd", "bwd"]


def _make_pass_labels(profile: Optional[MotionProfile]) -> List[str]:
    """Build per-pass labels from motion profile speeds, or use generic fallback."""
    if profile and profile.speed1 and profile.speed2:
        speeds = [
            profile.speed1, profile.speed1, profile.speed2, profile.speed2
        ]
        return [
            f"P{i+1} {s:.0f} mm/s {d}"
            for i, (s, d) in enumerate(zip(speeds, PASS_DIRECTIONS))
        ]
    return [
        f"P{i+1} v{'1' if i < 2 else '2'} {d}"
        for i, d in enumerate(PASS_DIRECTIONS)
    ]


def _scan_title(axis: str, peaks: Optional[CppPeaks]) -> str:
    """Build a plot title prefix including the scan name if available."""
    name = peaks.name if peaks and peaks.name else ""
    if name:
        return f"{axis}-Axis [{name}]"
    return f"{axis}-Axis"


def _interp(x_data: List[float], y_data: List[float], x: float) -> float:
    """Linear interpolation in sorted x_data."""
    if x <= x_data[0]:
        return y_data[0]
    if x >= x_data[-1]:
        return y_data[-1]
    lo, hi = 0, len(x_data) - 1
    while hi - lo > 1:
        mid = (lo + hi) // 2
        if x_data[mid] <= x:
            lo = mid
        else:
            hi = mid
    frac = (x - x_data[lo]) / (x_data[hi] -
                               x_data[lo]) if x_data[hi] != x_data[lo] else 0
    return y_data[lo] + frac * (y_data[hi] - y_data[lo])


def _find_pass_boundaries(profile: MotionProfile) -> List[tuple]:
    """Find (start_t, end_t) for each motion pass from velocity sign changes."""
    vel = profile.velocity_mm_s
    time_s = profile.time_s
    if not vel:
        return []
    passes = []
    in_pass = False
    start_t = 0.0
    for i in range(len(vel)):
        moving = abs(vel[i]) > 0.1
        if not in_pass and moving:
            in_pass = True
            start_t = time_s[i]
        elif in_pass and not moving:
            in_pass = False
            passes.append((start_t, time_s[i]))
    if in_pass:
        passes.append((start_t, time_s[-1]))
    return passes


def _normalize(values: List[float]) -> List[float]:
    """Zero-mean, unit-range normalization."""
    vmin = min(values)
    vmax = max(values)
    r = vmax - vmin if vmax != vmin else 1.0
    mid = (vmin + vmax) / 2.0
    return [(v - mid) / (r / 2.0) for v in values]


def _add_symmetry_overlay(fig, row, col, data, score_sets, sign, fwd_name,
                          mirror_prefix, legendgroup, show_legend):
    N = len(data)
    x = list(range(N))
    fig.add_trace(go.Scatter(
        x=x,
        y=list(data),
        name=fwd_name,
        mode="lines",
        line=dict(color="gray", width=1.5),
        legendgroup=legendgroup,
        showlegend=show_legend,
    ),
                  row=row,
                  col=col)
    for name, scores, best in score_sets:
        if not scores:
            continue
        y_mirror = [
            sign * data[(N - 1 - i) - best] if 0 <=
            (N - 1 - i) - best < N else None for i in range(N)
        ]
        fig.add_trace(go.Scatter(
            x=x,
            y=y_mirror,
            name=f"{mirror_prefix}@{best} ({name})",
            mode="lines",
            line=dict(color=CORR_COLORS[name], width=1, dash="dash"),
            legendgroup=legendgroup,
            showlegend=show_legend,
        ),
                      row=row,
                      col=col)
    fig.update_xaxes(title_text="Sample", row=row, col=col)


def plot_sweep_analysis(
    axis: str,
    samples: Optional[LineSamples],
    profile: Optional[MotionProfile],
    peaks: Optional[CppPeaks],
    est: Optional[CppEstimates],
    diameter: float,
    symmetry: Optional[List[CppSymmetry]] = None,
    preprocessed: Optional[List[PassPreprocessed]] = None,
    derivatives: Optional[List[PassDerivative]] = None,
    correlations: Optional[List[PassCorrelation]] = None,
    raw_chunks: Optional[List[PassRawChunk]] = None,
    energy_data: Optional[RoughAlignEnergy] = None,
    score_data: Optional[RoughAlignScore] = None,
    tool_label: Optional[str] = None,
) -> Optional[go.Figure]:
    """Single complex figure per sweep with all analysis panels."""
    if not samples or not samples.samples or not profile or not profile.time_s:
        return None
    if not peaks:
        return None

    dt = 1.0 / samples.sampling_freq_hz
    n = len(samples.samples)
    filtered = median_filter(samples.samples)
    norm = _normalize(filtered)

    passes = _find_pass_boundaries(profile)
    if len(passes) < 4:
        return None

    peak_times = [peaks.t1, peaks.t2, peaks.t3, peaks.t4]
    profile_t_max = profile.time_s[-1] if profile.time_s else 0
    center = diameter / 2.0
    pass_labels = _make_pass_labels(profile)
    title_prefix = _scan_title(axis, peaks)
    tau = est.all.tau if est else 0.0

    subtitle = ""
    if est:
        offset = est.all.pos - center
        subtitle = (f"Offset: {offset:+.3f} mm "
                    f"(pos={est.all.pos:.3f}, "
                    f"\u03c4={est.all.tau * 1000:.2f}ms, "
                    f"conf={peaks.confidence:.3f})")

    has_energy = energy_data is not None and bool(energy_data.energy)
    has_score = score_data is not None and bool(score_data.score_curve)
    has_preprocessed = bool(preprocessed)
    has_detail = bool(correlations) and bool(raw_chunks)
    pp_by_pass = {
        pp.pass_num: pp
        for pp in preprocessed
    } if preprocessed else {}
    deriv_by_pass = {d.pass_num: d for d in derivatives} if derivatives else {}

    # Build panel layout: 4-column grid, full-width rows use colspan=4
    COLS = 4
    full_row = [{"colspan": COLS}, None, None, None]
    quad_row = [{}, {}, {}, {}]
    panels = []  # [(spec_row, titles, height), ...]

    def _add_full(title, height):
        panels.append((list(full_row), [title], height))

    def _add_quad(titles, height):
        panels.append((list(quad_row), list(titles), height))

    _add_full("Time Domain + Velocity", 0.25)
    if has_score:
        _add_full("Rough-Align Matched-Filter Score", 0.15)
    if has_preprocessed:
        _add_full("Per-Pass Preprocessed (overlaid)", 0.20)
    if has_detail:
        _add_quad([f"P{i+1} Correlation" for i in range(4)], 0.15)
        _add_quad([f"P{i+1} Symmetry Overlay" for i in range(4)], 0.15)
        _add_quad([f"P{i+1} Derivative Overlay" for i in range(4)], 0.15)
        _add_quad([f"P{i+1} Preproc Overlay" for i in range(4)], 0.15)
        _add_quad([f"P{i+1} Preproc Deriv Overlay" for i in range(4)], 0.15)
    _add_full("Spatial Domain (overlay)", 0.25)

    specs = [p[0] for p in panels]
    row_heights = [p[2] for p in panels]
    subplot_titles = [t for p in panels for t in p[1]]

    n_rows = len(specs)
    total = sum(row_heights)
    row_heights = [h / total for h in row_heights]

    fig = make_subplots(
        rows=n_rows,
        cols=COLS,
        subplot_titles=subplot_titles,
        specs=specs,
        vertical_spacing=0.04,
        horizontal_spacing=0.04,
        row_heights=row_heights,
    )

    cur_row = 1

    def _axis_ref(row, col=1):
        """Return (xref, yref) for add_shape targeting a subplot at (row, col)."""
        # _grid_ref[row-1][col-1] is a list of (axis_type, axis_id) tuples
        refs = fig._grid_ref[row - 1][col - 1]
        if not refs:
            return "x", "y domain"
        # refs is like [('xy', 'x2y2')] — extract axis numbers
        xaxis = refs[0].layout_keys[0].replace("axis", "")  # "x" or "x2"
        yaxis = refs[0].layout_keys[1].replace("axis", "")  # "y" or "y2"
        return xaxis, f"{yaxis} domain"

    def _add_bg_rect(row, x0, x1, fillcolor, col=1):
        xref, yref = _axis_ref(row, col)
        fig.add_shape(type="rect",
                      x0=x0,
                      x1=x1,
                      y0=0,
                      y1=1,
                      xref=xref,
                      yref=yref,
                      fillcolor=fillcolor,
                      line_width=0,
                      layer="below")

    def _add_vline_shape(row, x, col=1, **kwargs):
        xref, yref = _axis_ref(row, col)
        dash = kwargs.get("line_dash", "dot")
        color = kwargs.get("line_color", "gray")
        width = kwargs.get("line_width", 1)
        fig.add_shape(type="line",
                      x0=x,
                      x1=x,
                      y0=0,
                      y1=1,
                      xref=xref,
                      yref=yref,
                      line=dict(dash=dash, color=color, width=width),
                      layer="below")

    time_ms = [i * dt * 1000 for i in range(n)]

    if pp_by_pass:
        for i in range(4):
            pp = pp_by_pass.get(i + 1)
            if not pp:
                continue
            t0 = pp.chunk_start * dt * 1000
            t1 = (pp.chunk_start + pp.chunk_size) * dt * 1000
            _add_bg_rect(cur_row, t0, t1, PASS_BG_COLORS[i])
            _add_vline_shape(cur_row, t0)
        last_pp = pp_by_pass.get(4)
        if last_pp:
            _add_vline_shape(cur_row,
                             (last_pp.chunk_start + last_pp.chunk_size) * dt *
                             1000)
    else:
        for i in range(min(4, len(passes))):
            t_start, t_end = passes[i]
            _add_bg_rect(cur_row, t_start * 1000, t_end * 1000,
                         PASS_BG_COLORS[i])

    fig.add_trace(go.Scatter(x=time_ms,
                             y=norm,
                             name="Sensor (normalized)",
                             mode="lines",
                             line=dict(color="gray", width=1),
                             legendgroup="time",
                             showlegend=True),
                  row=cur_row,
                  col=1)

    if profile.velocity_mm_s:
        vel_max = max(abs(v) for v in profile.velocity_mm_s) or 1.0
        vel_scaled = [v / vel_max * 0.9 for v in profile.velocity_mm_s]
        vel_time_ms = [(t + tau) * 1000 for t in profile.time_s]
        fig.add_trace(go.Scatter(x=vel_time_ms,
                                 y=vel_scaled,
                                 name="Velocity (scaled)",
                                 mode="lines",
                                 line=dict(color="red", width=1.5,
                                           dash="dash"),
                                 opacity=0.6,
                                 legendgroup="time",
                                 showlegend=True),
                      row=cur_row,
                      col=1)

    sym_by_pass = {}
    if symmetry:
        for s in symmetry:
            sym_by_pass[s.pass_num] = s

    for i in range(4):
        t = peak_times[i]
        idx = max(0, min(int(t / dt), n - 1))
        sym = sym_by_pass.get(i + 1)
        conf = sym.confidence if sym else 0.0
        marker_size = 8 + conf * 8

        fig.add_trace(go.Scatter(x=[t * 1000],
                                 y=[norm[idx]],
                                 mode="markers",
                                 name=pass_labels[i],
                                 marker=dict(color=PASS_COLORS[i],
                                             size=marker_size,
                                             symbol="star",
                                             line=dict(width=1,
                                                       color="black")),
                                 legendgroup="passes",
                                 showlegend=True),
                      row=cur_row,
                      col=1)
        _add_vline_shape(cur_row,
                         t * 1000,
                         line_dash="dot",
                         line_color=PASS_COLORS[i])

        if sym:
            corr_str = f"{sym.correlation_peak:.2f}" if sym.correlation_peak != float(
                "-inf") else "-inf"
            fig.add_annotation(
                x=t * 1000,
                y=norm[idx],
                text=f"conf={sym.confidence:.2f}<br>corr={corr_str}",
                showarrow=True,
                arrowhead=0,
                arrowcolor=PASS_COLORS[i],
                ax=0,
                ay=-30,
                font=dict(size=9, color=PASS_COLORS[i]),
                row=cur_row,
                col=1)

    if has_energy:
        ed = energy_data
        ed_n = len(ed.energy)
        ed_time_ms = [i * ed.dt_dec * 1000 for i in range(ed_n)]
        ed_norm = _normalize(ed.energy)

        fig.add_trace(go.Scatter(x=ed_time_ms,
                                 y=ed_norm,
                                 name="Rough align energy",
                                 mode="lines",
                                 line=dict(color="darkblue",
                                           width=1.5,
                                           dash="dashdot"),
                                 opacity=0.5,
                                 legendgroup="time",
                                 showlegend=True),
                      row=cur_row,
                      col=1)

        # The four template windows the matched filter aligned with.
        for region in ed.regions:
            r_start_ms = region[0] * ed.dt_dec * 1000
            r_end_ms = region[1] * ed.dt_dec * 1000
            _add_vline_shape(cur_row,
                             r_start_ms,
                             line_dash="dash",
                             line_color="darkblue",
                             line_width=1)
            _add_vline_shape(cur_row,
                             r_end_ms,
                             line_dash="dash",
                             line_color="darkblue",
                             line_width=1)

    fig.update_xaxes(title_text="Time (ms)", row=cur_row, col=1)
    fig.update_yaxes(title_text="Normalized", row=cur_row, col=1)
    cur_row += 1

    if has_score:
        sc = score_data
        ks = list(range(sc.k_min, sc.k_max + 1))
        ks_ms = [k * sc.dt_dec * 1000 for k in ks]
        fig.add_trace(go.Scatter(x=ks_ms,
                                 y=sc.score_curve,
                                 name="Matched-filter score",
                                 mode="lines+markers",
                                 line=dict(color="black", width=1),
                                 marker=dict(size=3),
                                 showlegend=False),
                      row=cur_row,
                      col=1)
        k_best_ms = sc.k_best * sc.dt_dec * 1000
        fig.add_vline(
            x=k_best_ms,
            line_color="red",
            line_width=2,
            annotation_text=(f"k_best={sc.k_best} "
                             f"({k_best_ms:+.0f}ms), "
                             f"score/baseline={sc.score_over_baseline:.2f}"),
            annotation_position="top",
            annotation_font_size=10,
            annotation_font_color="red",
            row=cur_row,
            col=1)
        fig.update_xaxes(title_text="Shift τ (ms)", row=cur_row, col=1)
        fig.update_yaxes(title_text="Σ energy at 4 taps", row=cur_row, col=1)
        cur_row += 1

    if has_preprocessed:
        for i in range(4):
            pp = pp_by_pass.get(i + 1)
            if not pp or not pp.samples:
                continue

            pp_dt = pp.dt
            n_pp = len(pp.samples)
            peak_t = peak_times[i] if i < len(peak_times) else 0
            peak_in_chunk = (peak_t / pp_dt) - pp.chunk_start
            rel_time_ms = [(j - peak_in_chunk) * pp_dt * 1000
                           for j in range(n_pp)]

            vals = list(pp.samples)
            if i in (1, 3):
                vals = vals[::-1]
                rel_time_ms = rel_time_ms[::-1]

            fig.add_trace(go.Scatter(x=rel_time_ms,
                                     y=vals,
                                     name=f"P{i+1} signal",
                                     mode="lines",
                                     line=dict(color=PASS_COLORS[i],
                                               width=1.5),
                                     legendgroup="preprocessed",
                                     showlegend=True),
                          row=cur_row,
                          col=1)

            deriv = deriv_by_pass.get(i + 1)
            if deriv and deriv.samples:
                n_d = len(deriv.samples)
                d_peak_in_chunk = peak_in_chunk - 0.5  # derivative is shifted by 0.5
                d_rel_time_ms = [(j - d_peak_in_chunk) * pp_dt * 1000
                                 for j in range(n_d)]
                d_vals = list(deriv.samples)
                if i in (1, 3):
                    d_vals = d_vals[::-1]
                    d_rel_time_ms = d_rel_time_ms[::-1]

                fig.add_trace(go.Scatter(x=d_rel_time_ms,
                                         y=d_vals,
                                         name=f"P{i+1} deriv",
                                         mode="lines",
                                         line=dict(color=PASS_COLORS[i],
                                                   width=1,
                                                   dash="dot"),
                                         opacity=0.5,
                                         legendgroup="preprocessed",
                                         showlegend=False),
                              row=cur_row,
                              col=1)

        _add_vline_shape(cur_row, 0, line_dash="solid", line_color="black")
        fig.update_xaxes(title_text="Time relative to peak (ms)",
                         row=cur_row,
                         col=1)
        fig.update_yaxes(title_text="Preprocessed", row=cur_row, col=1)
        cur_row += 1

    if has_detail:
        corr_by_pass = {c.pass_num: c for c in correlations}
        raw_by_pass = {r.pass_num: r for r in raw_chunks}
        corr_row = cur_row
        overlay_row = cur_row + 1
        deriv_row = cur_row + 2
        pp_overlay_row = cur_row + 3
        pp_deriv_row = cur_row + 4

        for i in range(4):
            col = i + 1
            corr = corr_by_pass.get(i + 1)
            if not corr:
                continue
            show_legend = (i == 0)

            score_sets = [
                ("value", corr.value_scores, corr.best_lag_value),
                ("deriv", corr.deriv_scores, corr.best_lag_deriv),
                ("combined", corr.combined_scores, corr.best_lag_combined),
            ]
            for name, scores, best in score_sets:
                if not scores:
                    continue
                lags = list(range(corr.fine_min, corr.fine_min + len(scores)))
                color = CORR_COLORS[name]
                dash = "dash" if name == "deriv" else "solid"
                width = 2 if name == "combined" else 1.2

                fig.add_trace(go.Scatter(
                    x=lags,
                    y=scores,
                    name=name.capitalize(),
                    mode="lines",
                    line=dict(color=color, width=width, dash=dash),
                    legendgroup="corr",
                    showlegend=show_legend,
                ),
                              row=corr_row,
                              col=col)

                best_idx = best - corr.fine_min
                if 0 <= best_idx < len(scores):
                    fig.add_trace(go.Scatter(
                        x=[best],
                        y=[scores[best_idx]],
                        mode="markers",
                        name=f"{name} best={best}",
                        marker=dict(color=color,
                                    size=8,
                                    symbol="star",
                                    line=dict(width=1, color="black")),
                        legendgroup="corr",
                        showlegend=False,
                    ),
                                  row=corr_row,
                                  col=col)

            fig.update_xaxes(title_text="Lag", row=corr_row, col=col)

            raw = raw_by_pass.get(i + 1)
            if not raw or not raw.samples:
                continue

            _add_symmetry_overlay(fig, overlay_row, col, raw.samples,
                                  score_sets, 1, "Forward", "Mirror",
                                  "overlay", show_legend)

            # Anti-symmetric mirror: deriv[i] vs -deriv[N-2-i-lag]
            raw_deriv = [
                raw.samples[j + 1] - raw.samples[j]
                for j in range(len(raw.samples) - 1)
            ]
            _add_symmetry_overlay(fig, deriv_row, col, raw_deriv, score_sets,
                                  -1, "d/dt Forward", "d/dt Mirror",
                                  "deriv_ov", show_legend)

            pp = pp_by_pass.get(i + 1)
            if pp and pp.samples:
                _add_symmetry_overlay(fig, pp_overlay_row, col, pp.samples,
                                      score_sets, 1, "Preproc fwd",
                                      "Preproc mirror", "pp_ov", show_legend)

            dv = deriv_by_pass.get(i + 1)
            if dv and dv.samples:
                _add_symmetry_overlay(fig, pp_deriv_row, col, dv.samples,
                                      score_sets, -1, "Preproc d/dt fwd",
                                      "Preproc d/dt mirror", "ppd_ov",
                                      show_legend)

        cur_row += 5

    for i in range(4):
        t_start, t_end = passes[i]
        peak_t = peak_times[i]
        if peak_t < 0 or peak_t > n * dt:
            continue

        margin = 0.02
        i_start = max(0, int((t_start + tau - margin) / dt))
        i_end = min(n, int((t_end + tau + margin) / dt))

        pass_pos = []
        pass_val = []

        for j in range(i_start, i_end):
            aligned_t = j * dt - tau
            if aligned_t > profile_t_max or aligned_t < 0:
                continue
            pos = _interp(profile.time_s, profile.position_mm, aligned_t)
            pass_pos.append(pos)
            pass_val.append(norm[j])

        if not pass_pos:
            continue

        if i in (1, 3):
            pass_pos = pass_pos[::-1]
            pass_val = pass_val[::-1]

        fig.add_trace(go.Scatter(x=pass_pos,
                                 y=pass_val,
                                 name=pass_labels[i],
                                 mode="lines",
                                 line=dict(color=PASS_COLORS[i], width=1.5),
                                 legendgroup="spatial",
                                 showlegend=True),
                      row=cur_row,
                      col=1)

    if est:
        pos = est.all.pos
        offset = pos - center
        _add_vline_shape(cur_row,
                         pos,
                         line_dash="solid",
                         line_color="black",
                         line_width=2)
        xref_s, yref_s = _axis_ref(cur_row)
        fig.add_annotation(x=pos,
                           y=1,
                           text=f"{pos:.3f}mm ({offset:+.3f})",
                           xref=xref_s,
                           yref=yref_s,
                           showarrow=False,
                           font=dict(size=11),
                           xanchor="left",
                           yanchor="top")

    fig.update_xaxes(title_text="Position (mm)", row=cur_row, col=1)
    fig.update_yaxes(title_text="Sensor response", row=cur_row, col=1)

    main_title = (f"{tool_label} \u2014 {title_prefix}: Sweep Analysis"
                  if tool_label else f"{title_prefix}: Sweep Analysis")
    fig.update_layout(
        title=dict(text=(f"{main_title}<br><sup>{subtitle}</sup>"
                         if subtitle else main_title)),
        autosize=True,
        height=450 * n_rows,
        showlegend=True,
        legend=dict(orientation="h",
                    yanchor="bottom",
                    y=-0.03,
                    xanchor="center",
                    x=0.5,
                    font=dict(size=9),
                    tracegroupgap=10),
        margin=dict(b=120),
    )
    return fig


def plot_repeat_scatter(x_vals: List[float], y_vals: List[float]) -> go.Figure:
    import numpy as np

    x_arr = np.array(x_vals)
    y_arr = np.array(y_vals)
    x_mean = float(np.mean(x_arr))
    y_mean = float(np.mean(y_arr))
    x_centered = x_arr - x_mean
    y_centered = y_arr - y_mean
    half_range = 0.025
    neighborhood = 0.001

    n_eval = 400
    x_eval = np.linspace(-half_range, half_range, n_eval)
    y_eval = np.linspace(-half_range, half_range, n_eval)
    x_density = np.array([
        int(np.sum(np.abs(x_centered - xp) <= neighborhood)) for xp in x_eval
    ])
    y_density = np.array([
        int(np.sum(np.abs(y_centered - yp) <= neighborhood)) for yp in y_eval
    ])

    fig = make_subplots(
        rows=2,
        cols=2,
        column_widths=[0.92, 0.08],
        row_heights=[0.08, 0.92],
        horizontal_spacing=0.02,
        vertical_spacing=0.02,
        shared_xaxes=True,
        shared_yaxes=True,
    )

    # X density strip (top) — bar chart sharing x-axis with scatter
    bin_w = float(x_eval[1] - x_eval[0])
    fig.add_trace(go.Bar(
        x=x_eval.tolist(),
        y=x_density.tolist(),
        width=bin_w,
        marker_color="black",
        opacity=0.6,
        hovertemplate="X=%{x:.4f} mm<br>count=%{y}<extra></extra>",
    ),
                  row=1,
                  col=1)

    # Y density strip (right) — horizontal bar sharing y-axis with scatter
    bin_h = float(y_eval[1] - y_eval[0])
    fig.add_trace(go.Bar(
        y=y_eval.tolist(),
        x=y_density.tolist(),
        width=bin_h,
        marker_color="black",
        opacity=0.6,
        orientation="h",
        hovertemplate="Y=%{y:.4f} mm<br>count=%{x}<extra></extra>",
    ),
                  row=2,
                  col=2)

    fig.add_trace(go.Scatter(
        x=x_centered.tolist(),
        y=y_centered.tolist(),
        mode="markers+text",
        text=[str(i + 1) for i in range(len(x_vals))],
        textposition="top center",
        textfont=dict(size=8),
        marker=dict(size=8,
                    color=list(range(len(x_vals))),
                    colorscale="Viridis",
                    showscale=True,
                    colorbar=dict(title="Run #")),
    ),
                  row=2,
                  col=1)

    ax_range = [-half_range, half_range]
    fig.update_xaxes(range=ax_range,
                     row=2,
                     col=1,
                     title_text=f"X offset \u2212 mean ({x_mean:+.4f}) mm")
    fig.update_yaxes(range=ax_range,
                     row=2,
                     col=1,
                     title_text=f"Y offset \u2212 mean ({y_mean:+.4f}) mm")
    fig.update_xaxes(range=ax_range, showticklabels=False, row=1, col=1)
    fig.update_yaxes(showticklabels=False, row=1, col=1)
    fig.update_xaxes(showticklabels=False, row=2, col=2)
    fig.update_yaxes(range=ax_range, showticklabels=False, row=2, col=2)

    # Lock aspect ratio to square
    side = 800
    fig.update_layout(
        title="Repeatability: X vs Y offset",
        width=side,
        height=side,
        showlegend=False,
        bargap=0,
    )
    return fig


def plot_repeat_histograms(x_vals: List[float],
                           y_vals: List[float]) -> go.Figure:
    fig = make_subplots(rows=1,
                        cols=2,
                        subplot_titles=("X offset (mm)", "Y offset (mm)"))
    fig.add_trace(go.Histogram(x=x_vals, name="X", marker_color="steelblue"),
                  row=1,
                  col=1)
    fig.add_trace(go.Histogram(x=y_vals, name="Y", marker_color="coral"),
                  row=1,
                  col=2)
    fig.update_layout(title="Repeatability: offset distributions",
                      showlegend=False,
                      autosize=True)
    return fig


def plot_repeat_timeseries(
    x_vals: List[float],
    y_vals: List[float],
    x_conf: List[Optional[float]],
    y_conf: List[Optional[float]],
) -> go.Figure:
    runs = list(range(1, len(x_vals) + 1))

    fig = make_subplots(rows=2,
                        cols=1,
                        subplot_titles=("X offset vs run", "Y offset vs run"),
                        vertical_spacing=0.12)

    for row, vals, confs, name, color in [
        (1, x_vals, x_conf, "X", "steelblue"),
        (2, y_vals, y_conf, "Y", "coral"),
    ]:
        mean = statistics.mean(vals)
        fig.add_trace(go.Scatter(x=runs,
                                 y=vals,
                                 name=name,
                                 mode="markers+lines",
                                 marker=dict(size=8, color=color),
                                 line=dict(color=color, width=1)),
                      row=row,
                      col=1)
        fig.add_hline(y=mean,
                      line_dash="dash",
                      line_color="gray",
                      annotation_text=f"mean={mean:.4f}",
                      row=row,
                      col=1)
        if len(vals) >= 2:
            std = statistics.stdev(vals)
            fig.add_hrect(y0=mean - std,
                          y1=mean + std,
                          fillcolor=color,
                          opacity=0.1,
                          line_width=0,
                          row=row,
                          col=1)

        valid_confs = [c for c in confs if c is not None]
        if valid_confs:
            fig.add_trace(go.Scatter(
                x=runs,
                y=vals,
                name=f"{name} conf",
                mode="markers",
                showlegend=False,
                marker=dict(size=[max(4, (c or 0) * 20) for c in confs],
                            color=color,
                            opacity=0.3)),
                          row=row,
                          col=1)

    fig.update_xaxes(title_text="Run #", row=2, col=1)
    fig.update_yaxes(title_text="mm", row=1, col=1)
    fig.update_yaxes(title_text="mm", row=2, col=1)
    fig.update_layout(title="Repeatability: offset over time", autosize=True)
    return fig


def run_single_probe(machine: Machine, gcode: str) -> List[str]:
    print(f"  Sending: {gcode}")
    return machine.command(gcode, timeout=120)


_TWO_PASS_SCANS = [
    ("X", "center-detection-x", "Pass 1: Center Detection"),
    ("Y", "center-detection-y", "Pass 1: Center Detection"),
    ("X", "nozzle-offset-x", "Pass 2: Nozzle Offset"),
    ("Y", "nozzle-offset-y", "Pass 2: Nozzle Offset"),
]

_LEGACY_SCANS = [
    ("X", "scan_x", "Single Pass"),
    ("Y", "scan_y", "Single Pass"),
]


def _get_scan_configs(data: Dict[str, Any]):
    """Return two-pass configs if available, else legacy single-pass."""
    if match_by_label(data["line_samples"], "center-detection"):
        return _TWO_PASS_SCANS
    return _LEGACY_SCANS


def _gather_scan_data(data: Dict[str, Any], frag: str) -> Dict[str, Any]:
    """Extract all data matching a scan label fragment."""

    def _filter(key):
        return [item for item in data[key] if frag in item.label]

    return {
        "samples":
        match_by_label(data["line_samples"], frag),
        "profile":
        match_by_label(data["motion_profile"], frag),
        "timing":
        match_by_label(data["motion_timing"], frag),
        "peaks":
        match_by_label(data["peaks"], frag),
        "est":
        match_by_label(data["estimates"], frag),
        "deltas":
        match_by_label(data["deltas"], frag),
        "symmetry":
        _filter("symmetry"),
        "preprocessed":
        _filter("pass_preprocessed"),
        "derivatives":
        _filter("pass_derivative"),
        "correlations":
        _filter("pass_correlation"),
        "trim_refines":
        _filter("pass_trim_refine"),
        "raw_chunks":
        _filter("pass_raw_chunk"),
        "energy_data":
        next((e for e in data["rough_align_energy"] if frag in e.label), None),
        "score_data":
        next((s for s in data["rough_align_score"] if frag in s.label), None),
    }


def process_single(response: List[str],
                   diameter: float,
                   tool_label: Optional[str] = None) -> tuple:
    """Process a single probe. Returns (data, figures) where figures is a list of go.Figure."""
    data = parse_response(response)
    figures: List[go.Figure] = []

    print(f"\nParsed: {len(data['line_samples'])} line_samples, "
          f"{len(data['motion_profile'])} profiles, "
          f"{len(data['peaks'])} peak sets, "
          f"{len(data['pass_preprocessed'])} preprocessed, "
          f"{len(data['pass_correlation'])} correlations, "
          f"{len(data['pass_trim_refine'])} trim_refines, "
          f"{len(data['rough_align_energy'])} energy, "
          f"{len(data['rough_align_score'])} score")

    for axis, frag, pass_name in _get_scan_configs(data):
        sd = _gather_scan_data(data, frag)

        print(f"\n--- {pass_name} / {axis}-Axis [{frag}] ---")
        print_axis_results(axis,
                           sd["peaks"],
                           sd["est"],
                           sd["deltas"],
                           sd["symmetry"],
                           sd["timing"],
                           sd["samples"],
                           sd["profile"],
                           diameter,
                           trim_refines=sd["trim_refines"])

        fig = plot_sweep_analysis(f"{pass_name} / {axis}",
                                  sd["samples"],
                                  sd["profile"],
                                  sd["peaks"],
                                  sd["est"],
                                  diameter,
                                  symmetry=sd["symmetry"],
                                  preprocessed=sd["preprocessed"],
                                  derivatives=sd["derivatives"],
                                  correlations=sd["correlations"],
                                  raw_chunks=sd["raw_chunks"],
                                  energy_data=sd["energy_data"],
                                  score_data=sd["score_data"],
                                  tool_label=tool_label)
        if fig:
            figures.append(fig)

    print_final_offset(data["final"])
    return data, figures


def common_options(f):
    f = click.option("--port",
                     default=None,
                     help="Serial port (auto-detect Prusa VID)")(f)
    f = click.option("--speed1",
                     "-F",
                     type=float,
                     default=None,
                     help="First speed v1 (mm/s)")(f)
    f = click.option("--speed2",
                     "-R",
                     type=float,
                     default=None,
                     help="Second speed v2 (mm/s)")(f)
    f = click.option("--diameter",
                     "-D",
                     type=float,
                     default=None,
                     help="Scan diameter (mm)")(f)
    f = click.option("--zheight",
                     "-Z",
                     type=float,
                     default=None,
                     help="Sensing Z height (mm)")(f)
    return f


def output_options(f):
    f = click.option("--save",
                     type=click.Path(),
                     default=None,
                     help="Save raw JSON response")(f)
    f = click.option("--load",
                     type=click.Path(exists=True),
                     default=None,
                     help="Load from saved JSON")(f)
    f = click.option("--show",
                     is_flag=True,
                     default=False,
                     help="Show plots in browser")(f)
    f = click.option("--plot",
                     type=click.Path(),
                     default=None,
                     help="Save plots to HTML file")(f)

    @functools.wraps(f)
    def wrapper(*args, save=None, load=None, show=False, plot=None, **kwargs):
        if not save and not show and not plot and not load:
            raise click.UsageError(
                "At least one of --save, --show, --plot, or --load is required"
            )
        return f(*args, save=save, load=load, show=show, plot=plot, **kwargs)

    return wrapper


def _emit_figures(figures: List[go.Figure], show: bool, plot: Optional[str]):
    if show:
        for fig in figures:
            fig.show()
    if plot:
        if len(figures) == 1:
            figures[0].write_html(plot)
            print(f"Plot saved to {plot}")
        else:
            base = Path(plot)
            stem, suffix = base.stem, base.suffix or ".html"
            for i, fig in enumerate(figures):
                p = base.parent / f"{stem}_{i + 1}{suffix}"
                fig.write_html(str(p))
                print(f"Plot saved to {p}")


def _print_run_result(label: str, final: FinalOffset) -> None:
    x, y = final.x_mm, final.y_mm
    xc, yc = final.x_confidence, final.y_confidence
    if x is not None and y is not None:
        conf_str = ""
        if xc is not None and yc is not None:
            conf_str = f"  conf: X={xc:.2f} Y={yc:.2f}"
        print(f"  {label}: X={x:+.4f} mm, Y={y:+.4f} mm{conf_str}")
    else:
        print(f"  {label}: incomplete data")


@click.group()
def cli():
    """G426/G427 tool offset debug and repeatability tool."""
    pass


@cli.command()
@common_options
@output_options
def single(port, speed1, speed2, diameter, zheight, save, load, show, plot):
    """Single probe: detailed C++ results and plots."""
    d = diameter or 10.0

    if load:
        print(f"Loading from {load}...")
        response = load_raw(load)
    else:
        gcode = build_g426(speed1, speed2, diameter, zheight)
        with enabledMachineConnection(port) as machine:
            response = run_single_probe(machine, gcode)

    if save:
        save_raw(save, response)

    data, figures = process_single(response, d)
    _emit_figures(figures, show, plot)


@cli.command()
@common_options
@output_options
@click.option("--count",
              "-N",
              type=int,
              default=None,
              help="Number of repetitions")
def repeat(port, speed1, speed2, diameter, zheight, save, load, show, plot,
           count):
    """Repeatability: run N probes, show statistics and plots."""
    d = diameter or 10.0
    all_finals: List[FinalOffset] = []
    all_responses: List[List[str]] = []

    if load:
        print(f"Loading from {load}...")
        all_responses = load_raw_multi(load)
        print(f"Loaded {len(all_responses)} runs")
        for i, response in enumerate(all_responses):
            data = parse_response(response)
            all_finals.append(data["final"])
            _print_run_result(f"Run {i + 1}", data["final"])
    else:
        if count is None or count < 1:
            raise click.UsageError("Specify --count/-N or --load")

        gcode = build_g426(speed1, speed2, diameter, zheight)
        try:
            with enabledMachineConnection(port) as machine:
                for i in range(count):
                    print(f"\n--- Run {i + 1}/{count} ---")
                    response = run_single_probe(machine, gcode)
                    all_responses.append(response)

                    data = parse_response(response)
                    final = data["final"]
                    all_finals.append(final)
                    _print_run_result("Result", final)
        except KeyboardInterrupt:
            print(f"\nInterrupted after {len(all_responses)} runs")

    if save and not load and all_responses:
        save_raw_multi(save, all_responses)

    x_vals = [f.x_mm for f in all_finals if f.x_mm is not None]
    y_vals = [f.y_mm for f in all_finals if f.y_mm is not None]
    x_conf = [f.x_confidence for f in all_finals]
    y_conf = [f.y_confidence for f in all_finals]

    if len(x_vals) < 2 or len(y_vals) < 2:
        print("\nNot enough valid results for statistics.")
        return

    print(f"\n{'=' * 70}")
    print(f"  REPEATABILITY STATISTICS ({len(x_vals)} runs)")
    print(f"{'=' * 70}")
    for name, vals in [("X", x_vals), ("Y", y_vals)]:
        mn = statistics.mean(vals)
        sd = statistics.stdev(vals)
        print(f"  {name}: mean={mn:+.4f}  std={sd:.4f}  "
              f"min={min(vals):+.4f}  max={max(vals):+.4f}  "
              f"range={max(vals) - min(vals):.4f} mm")
    print()

    figures = [
        plot_repeat_scatter(x_vals, y_vals),
        plot_repeat_histograms(x_vals, y_vals),
        plot_repeat_timeseries(x_vals, y_vals, x_conf, y_conf),
    ]
    _emit_figures(figures, show, plot)


@cli.command(name="all_tools")
@click.option("--port",
              default=None,
              help="Serial port (auto-detect Prusa VID)")
@click.option("--r-param",
              "-R",
              type=int,
              default=None,
              help="Random jitter mm for Z probing")
@click.option("--probe-count",
              "-P",
              type=int,
              default=None,
              help="Z probe repetitions per point")
@click.option("--diameter",
              "-D",
              type=float,
              default=None,
              help="Scan diameter for analysis (mm)")
@output_options
def all_tools(port, r_param, probe_count, diameter, save, load, show, plot):
    """G427: Full tool offset calibration for all enabled tools."""
    d = diameter or 10.0

    if load:
        print(f"Loading from {load}...")
        response = load_raw(load)
    else:
        gcode = build_g427(r_param, probe_count)
        with enabledMachineConnection(port) as machine:
            print(f"  Sending: {gcode}")
            response = machine.command(gcode, timeout=600)

    if save:
        save_raw(save, response)

    tool_responses = split_response_by_tool(response)
    print(f"\nDetected {len(tool_responses)} tool(s) in response")

    all_figures: List[go.Figure] = []
    for i, tool_resp in enumerate(tool_responses):
        tool_label = f"Tool {i}"
        print(f"\n{'#' * 70}")
        print(f"  {tool_label.upper()}")
        print(f"{'#' * 70}")
        data, figures = process_single(tool_resp, d, tool_label=tool_label)
        all_figures.extend(figures)

    _emit_figures(all_figures, show, plot)


if __name__ == "__main__":
    cli()
