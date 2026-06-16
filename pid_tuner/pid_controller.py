"""
Discrete PID controller matching thermal.cpp / thermal.hpp exactly.

Control law:
    u(t) = Kp·e(t) + Ki·∫e dτ + Kd·de/dt

Discretization:
    integral  — rectangular rule, clamped to ±INTEGRAL_LIMIT
    derivative — backward finite difference, skipped on first tick
    dt <= 0   — proportional only (thermal.cpp first-iteration guard)

Actuation:
    heater_on = u(t) > 0.0   (binary relay, not proportional output)
"""


class PIDController:

    INTEGRAL_LIMIT = 100.0

    def __init__(self, Kp: float, Ki: float, Kd: float, setpoint: float = 37.0):
        self.Kp = Kp
        self.Ki = Ki
        self.Kd = Kd
        self.setpoint = setpoint
        self._integral = 0.0
        self._prev_error: float | None = None

    def compute(self, temperature: float, dt: float) -> float:
        error = self.setpoint - temperature

        if dt <= 0.0 or self._prev_error is None:
            self._prev_error = error
            return self.Kp * error

        self._integral = max(
            -self.INTEGRAL_LIMIT,
            min(self.INTEGRAL_LIMIT, self._integral + error * dt),
        )
        derivative = (error - self._prev_error) / dt
        self._prev_error = error

        return self.Kp * error + self.Ki * self._integral + self.Kd * derivative

    def reset(self) -> None:
        self._integral = 0.0
        self._prev_error = None
