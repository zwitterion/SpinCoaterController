# SpinCoaterController Architecture

This document provides a technical overview of the firmware architecture for the Arduino UNO R4 WiFi Spin Coater Controller.

## 1. System Design Philosophy
The system is built using a **Modular Object-Oriented** approach. Each hardware component or logical responsibility is encapsulated in its own class, ensuring that the `main.ino` remains clean and primarily handles high-level orchestration.

## 2. Core Components

### Hardware Abstraction Layer (HAL)
*   **RPMReader:** Manages the TCRT5000 IR sensor. It uses hardware interrupts (Pin D8/D2) and software debouncing to calculate motor frequency. It includes a timeout mechanism to set RPM to zero when the motor stops.
*   **R4ESC / ESCController:** Wraps the PWM generation for the Electronic Speed Controller. `ESCController` handles the logic of mapping target RPM to PWM pulse widths (1000us - 2000us) and implements the **PID Control Loop**.
*   **WiFiManager:** Handles the dual-mode WiFi logic (Access Point for configuration, Station for operation). It interacts with the UNO R4 LED Matrix to provide visual status updates (IP addresses, icons).

### Logic & Execution
*   **ExecutionEngine:** The system's "brain." It contains the main state machine and handles the progression of multi-step profiles. It calculates ramp trajectories (Linear, Exponential, S-Curve) in real-time.
*   **ProfileManager:** Logic for creating, retrieving, and deleting spin profiles. It acts as an intermediary between the Web API and the storage layer. Handles the PWM-to-RPM mapping regression analysis.
*   **SafetyManager:** Monitors system health. It triggers an `EMERGENCY_STOP` if it detects conditions like a motor stall (throttle applied but no RPM) or overspeed (>15% above target).

### Data & Connectivity
*   **EEPROMStorage:** Manages non-volatile memory. It stores WiFi credentials, PID constants, ESC calibration, and up to 10 user profiles. It uses a Magic Number and CRC check for data integrity.
*   **WebServer & SimpleWebSocket:** Provides the interface. The `WebServer` serves a compressed SPA (Single Page Application) from `web_assets.h`. `SimpleWebSocket` provides a lightweight, custom implementation for high-speed telemetry (RPM, state, errors) to the browser.

## 3. State Machine (ExecutionEngine)

The system operates based on the following states:
| State | Description |
| :--- | :--- |
| `IDLE` | Motor stopped, system waiting for command. |
| `RUNNING` | Actively executing a multi-step profile. |
| `PAUSED` | Maintaining current RPM but halting the timer. |
| `STOPPING` | Controlled deceleration to 0 RPM. |
| `MANUAL` | Direct RPM control via the Web UI slider. |
| `TUNING` | Running the PID Auto-tune (Relay method) algorithm. |
| `MAPPING` | Automated PWM-to-RPM characterization using linear regression. |
| `CALIBRATING` | ESC Throttle range calibration mode. |
| `ERROR / EMERGENCY_STOP` | Critical failure detected; motor cut immediately. |

## 4. Control Theory

### PID Control
The system uses a closed-loop PID controller:
*   **Anti-Windup:** Integral accumulation is limited to a specific range and disabled when the output is saturated.
*   **Low Pass Filter:** A configurable `Alpha` filter is applied to the raw RPM sensor data to prevent PID jitter.
*   **Auto-Tuning:** Uses the **Ziegler-Nichols Relay Method**, oscillating the motor briefly to calculate ideal `Kp`, `Ki`, and `Kd` values.

### Empirical Open-Loop Mode (Feed-Forward)
The system has transitioned from a theoretical KV-based formula to a data-driven **Empirical Model**. This removes dependency on battery voltage or motor specs in favor of observed performance.

1.  **Data Collection:** The system sweeps the PWM range, waiting 5 seconds at each step for the motor to stabilize before recording the RPM.
2.  **Linear Regression:** Using the **Least Squares Method**, the system calculates a line of best fit: $RPM = (Slope \cdot PWM) + Intercept$.
3.  **Inference:** The system infers the `mapStartPWM` (the theoretical pulse width where $RPM = 0$) and uses the calculated `mapSlope` for control.
4.  **Control Formula:**
    `PulseWidth = mapStartPWM + (TargetRPM / mapSlope)`

## 5. Data Flow

1.  **Sensors:** Interrupt -> `RPMReader` -> `ExecutionEngine`.
2.  **Logic:** `ExecutionEngine` -> `ESCController` -> `PWM Output`.
3.  **Mapping:** `ExecutionEngine` -> `Linear Regression` -> `EEPROMStorage`.
4.  **User Input:** Browser -> `WebServer` (REST/WS) -> `ExecutionEngine` / `ProfileManager`.
4.  **Telemetry:** `ExecutionEngine` -> `WebServer` (WebSocket) -> Browser Gauge/Chart.

## 6. Pin Mapping (Default)

| Function | Pin | Note |
| :--- | :--- | :--- |
| ESC Signal | D9 | Servo PWM |
| RPM Sensor | D8 | Interrupt Pin |
| Sensor LED | D6 | Power for IR LED |
| I2C / Matrix | Internal | UNO R4 Matrix |

## 7. Storage Mapping (EEPROM)

*   **0x00:** Magic Number (0xCAFEBABE)
*   **0x04:** `SystemSettings` Struct (PID, WiFi, Calibration)
*   **0x1FE:** RPM Check Enable (bool)
*   **0x200+:** `SpinProfile` Array (10 slots)

## 8. Build Tools
*   **convert.py:** A Python script that minifies `index.html` (CSS/JS included) and Gzips it into a C-style header file. This allows the Arduino to serve a modern UI while using minimal Flash space.