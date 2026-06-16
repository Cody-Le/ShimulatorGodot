"""
HASP thermal PID gain optimizer.

Runs differential evolution (scipy) to minimize the RMS temperature error from
37 C, averaged across multiple turbulence seeds so the optimizer sees a stable
signal rather than one lucky/unlucky noise realization.

Why differential evolution:
    The objective is non-smooth (relay switching quantization produces discrete
    jumps), so gradient methods fail. DE is a population-based global optimizer
    that handles this well and finds a globally good solution, not just a local
    minimum.

Usage:
    pip install numpy scipy matplotlib
    python tuner.py                         # default settings, single core
    python tuner.py --seeds 3 --iters 50   # faster, less accurate
    python tuner.py --jobs -1              # all CPU cores via multiprocessing

Output:
    Prints optimal Kp / Ki / Kd and saves pid_tuner_results.png comparison plot.
"""
import argparse
import sys
import time
from functools import partial

import numpy as np
from scipy.optimize import differential_evolution

from simulator import simulate, SimResult, SETPOINT_C

# Search bounds: (min, max) for Kp, Ki, Kd
# Kp — sets error sensitivity for relay flip threshold.
#       Too high → chattering on 0.5 C sensor quantization steps.
#       Too low  → slow response, integral dominates entirely.
# Ki — eliminates steady-state bias from sensor offset and asymmetric environment.
# Kd — anticipates sign change, suppresses overshoot at relay flip.
#       High Kd amplifies quantization noise; keep modest.
PARAM_BOUNDS = [
    (0.05, 50.0),   # Kp [u/C]
    (0.00,  2.0),   # Ki [u/(C·s)]
    (0.00, 10.0),   # Kd [u/(C/s)]
]

DEFAULT_GAINS = (2.0, 0.1, 0.5)   # thermal.hpp:20-22


# ---------------------------------------------------------------------------
# Objective — must be a module-level function (picklable for multiprocessing)
# ---------------------------------------------------------------------------

def _objective(params: np.ndarray, n_seeds: int) -> float:
    """
    Mean RMSE across n_seeds independent turbulence realizations.
    Each seed draws a different Ornstein-Uhlenbeck atmospheric noise sequence,
    so averaging smooths out lucky/unlucky turbulence hits.
    """
    Kp, Ki, Kd = float(params[0]), float(params[1]), float(params[2])
    total = sum(simulate(Kp, Ki, Kd, seed=s).rmse for s in range(n_seeds))
    return total / n_seeds


# ---------------------------------------------------------------------------
# Validation & reporting
# ---------------------------------------------------------------------------

def _validate(Kp: float, Ki: float, Kd: float) -> SimResult:
    """Long run (2 h simulated) used for final comparison and plotting."""
    return simulate(
        Kp, Ki, Kd,
        sim_duration_s=7200.0,
        warmup_s=600.0,
        seed=99,
        record=True,
    )


def _fmt(res: SimResult) -> str:
    return (
        f"RMSE={res.rmse:.4f} C  "
        f"bias={res.mean_error:+.4f} C  "
        f"max_dev={res.max_deviation:.4f} C  "
        f"switches={res.switch_count}"
    )


def _plot(
    default_res: SimResult,
    optimal_res: SimResult,
    Kp_opt: float,
    Ki_opt: float,
    Kd_opt: float,
) -> None:
    import matplotlib.pyplot as plt

    fig, axes = plt.subplots(2, 1, figsize=(14, 8))
    fig.suptitle("HASP Thermal PID — Default vs Optimized Gains", fontsize=13)

    pairs = [
        (
            axes[0], default_res, "Default",
            f"Kp={DEFAULT_GAINS[0]}  Ki={DEFAULT_GAINS[1]}  Kd={DEFAULT_GAINS[2]}",
        ),
        (
            axes[1], optimal_res, "Optimized",
            f"Kp={Kp_opt:.4f}  Ki={Ki_opt:.4f}  Kd={Kd_opt:.4f}",
        ),
    ]

    for ax, res, label, gains_str in pairs:
        t_min = [t / 60.0 for t in res.t_log]
        ax.plot(t_min, res.T_log, linewidth=0.6, color="steelblue", label="Chamber T (C)")
        ax.axhline(SETPOINT_C, color="red", linewidth=1.2, linestyle="--", label="Setpoint 37 C")
        ax.fill_between(
            t_min,
            SETPOINT_C - res.rmse,
            SETPOINT_C + res.rmse,
            alpha=0.18,
            color="orange",
            label=f"+/-RMSE band ({res.rmse:.3f} C)",
        )
        ax.set_title(
            f"{label}  |  {gains_str}  |  "
            f"RMSE={res.rmse:.4f} C  switches={res.switch_count}"
        )
        ax.set_ylabel("Temperature (C)")
        ax.set_xlabel("Time (min)")
        ax.legend(fontsize=9, loc="upper right")
        ax.grid(True, alpha=0.3)

    plt.tight_layout()
    out = "pid_tuner_results.png"
    plt.savefig(out, dpi=150, bbox_inches="tight")
    print(f"Plot saved -> {out}")
    plt.close()


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(description="HASP thermal PID optimizer")
    parser.add_argument(
        "--seeds", type=int, default=5,
        help="Random turbulence seeds averaged per objective call (default: 5)",
    )
    parser.add_argument(
        "--iters", type=int, default=80,
        help="Differential evolution max iterations (default: 80)",
    )
    parser.add_argument(
        "--jobs", type=int, default=1,
        help=(
            "Parallel workers (default: 1=single core). "
            "Use -1 for all CPUs — requires multiprocessing-safe environment."
        ),
    )
    parser.add_argument("--no-plot", action="store_true", help="Skip saving the comparison plot")
    args = parser.parse_args()

    print("=" * 65)
    print("  HASP Thermal PID Optimizer")
    print(f"  seeds={args.seeds}  maxiter={args.iters}  workers={args.jobs}")
    print(f"  Kp in {PARAM_BOUNDS[0]}  Ki in {PARAM_BOUNDS[1]}  Kd in {PARAM_BOUNDS[2]}")
    print(f"  Default: Kp={DEFAULT_GAINS[0]}  Ki={DEFAULT_GAINS[1]}  Kd={DEFAULT_GAINS[2]}")
    print("=" * 65)

    t0 = time.perf_counter()
    iter_n = [0]

    def callback(xk, convergence):
        iter_n[0] += 1
        Kp, Ki, Kd = xk
        elapsed = time.perf_counter() - t0
        print(
            f"  iter {iter_n[0]:3d}  [{elapsed:6.1f}s]  "
            f"Kp={Kp:.4f}  Ki={Ki:.4f}  Kd={Kd:.4f}  "
            f"conv={convergence:.5f}",
            flush=True,
        )

    obj = partial(_objective, n_seeds=args.seeds)

    result = differential_evolution(
        obj,
        bounds=PARAM_BOUNDS,
        maxiter=args.iters,
        seed=42,
        workers=args.jobs,
        updating="deferred" if args.jobs != 1 else "immediate",
        callback=callback,
        tol=1e-5,
        polish=True,   # L-BFGS-B polish step after DE converges
    )

    Kp_opt, Ki_opt, Kd_opt = float(result.x[0]), float(result.x[1]), float(result.x[2])
    elapsed_total = time.perf_counter() - t0

    print()
    print("=" * 65)
    print(f"  Optimization finished in {elapsed_total:.1f} s")
    print(f"  Default  : Kp={DEFAULT_GAINS[0]}  Ki={DEFAULT_GAINS[1]}  Kd={DEFAULT_GAINS[2]}")
    print(f"  Optimal  : Kp={Kp_opt:.4f}  Ki={Ki_opt:.4f}  Kd={Kd_opt:.4f}")
    print(f"  Best mean RMSE (over {args.seeds} seeds): {result.fun:.6f} C")
    print("=" * 65)

    print("\nRunning 7200 s validation episodes for both gain sets...")
    default_res = _validate(*DEFAULT_GAINS)
    optimal_res = _validate(Kp_opt, Ki_opt, Kd_opt)

    print(f"  Default  : {_fmt(default_res)}")
    print(f"  Optimal  : {_fmt(optimal_res)}")

    improvement = (default_res.rmse - optimal_res.rmse) / default_res.rmse * 100
    print(f"\n  RMSE improvement: {improvement:+.1f}%")

    if not args.no_plot:
        try:
            _plot(default_res, optimal_res, Kp_opt, Ki_opt, Kd_opt)
        except ImportError:
            print("matplotlib not installed - skipping plot (pip install matplotlib)")

    print(f"\n  --> Use Kp={Kp_opt:.4f}  Ki={Ki_opt:.4f}  Kd={Kd_opt:.4f}")
    print(
        "      Update thermal.hpp lines 20-22 and recompile FSW to apply.\n"
    )


if __name__ == "__main__":
    main()
