"""
Thermal plant models ported from the Godot HASP simulation scripts.

  ThermalEnvironment  — hasp_environment.gd
  ThermalChamber      — hasp_chamber.gd
  DS18B20Sensor       — temp_probe.gd (Temp1 / PID probe defaults)

All time units are real seconds (not Godot sim-rate-scaled).
"""
import math
import random


class ThermalEnvironment:
    """
    US Standard Atmosphere 1976 at a fixed altitude, plus a Fort Sumner
    seasonal bias and an Ornstein-Uhlenbeck turbulent perturbation.
    Matches hasp_environment.gd exactly.
    """

    SEASONAL_OFFSET_C = 7.0    # Fort Sumner August warm bias
    TURBULENCE_SIGMA_C = 1.5   # steady-state std-dev of turbulent fluctuation
    TURBULENCE_TAU_S = 120.0   # correlation time of O-U process

    def __init__(self, altitude_m: float = 36_000.0, rng: random.Random | None = None):
        self.altitude_m = altitude_m
        self._turb = 0.0
        self._rng = rng or random.Random()

    def _standard_atmo_c(self) -> float:
        h = self.altitude_m / 1000.0
        if h < 11.0:
            return 15.0 - 6.5 * h
        elif h < 20.0:
            return -56.5
        elif h < 32.0:
            return -56.5 + 1.0 * (h - 20.0)
        else:
            return -44.5 + 2.8 * (h - 32.0)   # HASP float range

    def step(self, dt_s: float) -> None:
        """Advance the Ornstein-Uhlenbeck turbulence process by dt_s seconds."""
        if dt_s <= 0.0:
            return
        a = math.exp(-dt_s / self.TURBULENCE_TAU_S)
        step_std = self.TURBULENCE_SIGMA_C * math.sqrt(1.0 - a * a)
        self._turb = a * self._turb + self._rng.gauss(0.0, step_std)

    @property
    def temp_c(self) -> float:
        return self._standard_atmo_c() + self.SEASONAL_OFFSET_C + self._turb


class ThermalChamber:
    """
    Lumped one-node thermal plant.

        dT/dt = (Q_heater - (T - T_env) / R) / C

    Parameters from hasp_chamber.gd:
        C = 120 J/K  (stainless vessel, ~0.24 kg × 500 J/kg·K)
        R = 10  K/W  (Mylar insulation — uncertain, tune from bench)
        Q = 11.2 W   (2 × 5.6 W heaters at 0.2 A × 28 V)

    Steady states at 36 km float altitude (T_env ≈ −26 °C):
        heater ON  → ~86 °C
        heater OFF → ~−26 °C
    """

    C = 120.0        # J/K
    R = 10.0         # K/W
    Q_HEATER = 11.2  # W

    def __init__(self, T_init: float = 20.0):
        self.T = T_init
        self.heater_on = False

    def step(self, dt_s: float, T_env: float) -> None:
        Q = self.Q_HEATER if self.heater_on else 0.0
        self.T += ((Q - (self.T - T_env) / self.R) / self.C) * dt_s


class DS18B20Sensor:
    """
    First-order thermal lag toward chamber temperature, plus quantization
    and per-sensor systematic bias — matching temp_probe.gd.

    Temp1 (the PID-control probe, sensor_index=0):
        tau_s=18s, 9-bit resolution (0.5 °C LSB), +0.1 °C bias.

    tau_s is a real-time constant (seconds), independent of Godot sim_rate.
    """

    def __init__(self, tau_s: float = 18.0, resolution_bits: int = 9, bias_c: float = 0.1):
        self.tau_s = tau_s
        self.resolution_bits = resolution_bits
        self.bias_c = bias_c
        self._t_internal: float | None = None

    def lsb_c(self) -> float:
        return 0.5 / (2 ** (self.resolution_bits - 9))

    def step(self, dt_s: float, T_chamber: float) -> None:
        if self._t_internal is None:
            self._t_internal = T_chamber
            return
        alpha = min(dt_s / self.tau_s, 1.0)
        self._t_internal += alpha * (T_chamber - self._t_internal)

    def read_c(self) -> float:
        """Quantized reading (°C) the FSW sees via /sys/bus/w1/devices/.../temperature."""
        if self._t_internal is None:
            raise RuntimeError("sensor not initialized — call step() at least once first")
        lsb = self.lsb_c()
        return round((self._t_internal + self.bias_c) / lsb) * lsb
