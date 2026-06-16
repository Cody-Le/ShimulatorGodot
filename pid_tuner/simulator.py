"""
Simulates one HASP thermal control episode and returns performance metrics.

Tick timing:
    ~150 ms mean period — 94 ms DS18B20 9-bit conversion + ~57 ms blocking
    pressure reads (3× MS5837, telemetry-only but still gate loop restart).
    Jitter of ±20 ms models I/O variance. (thermal.cpp, driver_thermal.cpp)

Evaluation window:
    Metrics are computed over [warmup_s, sim_duration_s].
    T_log / t_log (populated when record=True) start from t=0 so the
    heat-up transient is visible in plots.
"""
import math
import random
from dataclasses import dataclass, field

from plant import ThermalEnvironment, ThermalChamber, DS18B20Sensor
from pid_controller import PIDController

SETPOINT_C = 37.0
DT_MEAN_S = 0.150   # mean FSW tick period (blocking sensor I/O)
DT_SIGMA_S = 0.020  # tick-to-tick standard deviation


@dataclass
class SimResult:
    rmse: float           # RMS temperature error from 37 °C (post-warmup)
    mean_error: float     # signed mean error — non-zero = steady-state bias
    max_deviation: float  # worst-case |T − 37| post-warmup
    switch_count: int     # heater state transitions (relay switching events)
    T_log: list[float] = field(default_factory=list)
    t_log: list[float] = field(default_factory=list)


def simulate(
    Kp: float,
    Ki: float,
    Kd: float,
    sim_duration_s: float = 3600.0,
    warmup_s: float = 600.0,
    altitude_m: float = 36_000.0,
    seed: int | None = None,
    record: bool = False,
) -> SimResult:
    rng = random.Random(seed)
    env = ThermalEnvironment(altitude_m=altitude_m, rng=rng)
    chamber = ThermalChamber(T_init=20.0)
    sensor = DS18B20Sensor(tau_s=18.0, resolution_bits=9, bias_c=0.1)
    pid = PIDController(Kp, Ki, Kd, setpoint=SETPOINT_C)

    sensor.step(0.0, chamber.T)  # seed sensor internal state to T_init

    t = 0.0
    errors: list[float] = []
    T_log: list[float] = []
    t_log: list[float] = []
    switch_count = 0
    prev_heater = False

    while t < sim_duration_s:
        # Tick period with jitter, floor at 1 ms to avoid divide-by-zero in PID
        dt = max(0.001, rng.gauss(DT_MEAN_S, DT_SIGMA_S))

        # Read current (lagged, quantized) temperature → PID decision → heater
        T_measured = sensor.read_c()
        control = pid.compute(T_measured, dt)
        heater_on = control > 0.0

        if heater_on != prev_heater:
            switch_count += 1
        prev_heater = heater_on
        chamber.heater_on = heater_on

        # Advance plant by dt
        env.step(dt)
        chamber.step(dt, env.temp_c)
        sensor.step(dt, chamber.T)
        t += dt

        if t >= warmup_s:
            errors.append(chamber.T - SETPOINT_C)
        if record:
            T_log.append(chamber.T)
            t_log.append(t)

    if not errors:
        return SimResult(
            rmse=1e6, mean_error=1e6, max_deviation=1e6,
            switch_count=switch_count, T_log=T_log, t_log=t_log,
        )

    n = len(errors)
    rmse = math.sqrt(sum(e * e for e in errors) / n)
    mean_error = sum(errors) / n
    max_deviation = max(abs(e) for e in errors)

    return SimResult(
        rmse=rmse,
        mean_error=mean_error,
        max_deviation=max_deviation,
        switch_count=switch_count,
        T_log=T_log,
        t_log=t_log,
    )
