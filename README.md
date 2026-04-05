# Arduino UNO R4 WiFi Spin Coater Controller

![Dashboard Screenshot](screenshot.png?v=2)

A complete, production-quality firmware for a DIY Spin Coater based on the **Arduino UNO R4 WiFi**. This system provides a modern, responsive Web UI to control a brushless motor with precision RPM feedback, allowing for complex spin coating recipes.

## 🚀 Capabilities

*   **Web-Based Interface:** No app required. Works on mobile and desktop via a responsive Single Page Application (SPA) hosted entirely on the Arduino.
*   **WiFi Connectivity:**
    *   **Access Point Mode:** Creates a hotspot (`SpinCoater-XXXX`) for initial setup.
    *   **Station Mode:** Connects to your local WiFi network.
    *   **Captive Portal:** Easy credential configuration.
*   **Advanced Motor Control:**
    *   **Closed-Loop PID:** Maintains precise RPM regardless of load.
    *   **Auto-Tuning:** Built-in PID auto-tuner using the relay method.
    *   **Ramp Profiles:** Supports Linear, Exponential, and S-Curve acceleration profiles.
    *   **Manual Mode:** Direct slider control for testing.
*   **Profile Management:**
    *   Create, Edit, Duplicate, and Delete multi-step spin recipes.
    *   Store up to 10 profiles in non-volatile memory (EEPROM).
*   **Real-Time Telemetry:**
    *   High-speed WebSocket connection.
    *   Live Analog Gauge and RPM vs. Target Chart.
    *   Status monitoring (Current Step, Time Remaining).
*   **Safety & Maintenance:**
    *   **Stall Detection:** Stops motor if no RPM is detected while powered.
    *   **Overspeed Protection:** Emergency stop if RPM exceeds target by 15%.
    *   **Absolute Max RPM:** User-configurable hard limit.


## 🧩3D Printed Enclosure

Please see printables.com  project ([Printables](https://www.printables.com/model/1666277-spin-coater)).

## 🛠️ Hardware Required

1.  **Microcontroller:** Arduino UNO R4 WiFi.
2.  **Motor:** Brushless DC Motor (e.g., BLDC Motor NEMA17 42BLF02). [ebay Link](https://www.ebay.com/itm/305752707379)
3.  **Controller:** BLDC-8015A Direct Drive Motor Controller. [ebay Link](https://www.ebay.com/itm/305752707379)
4.  **Sensor:** US1881 Hall Effect Sensor (for RPM feedback). [Amazon Link](https://amzn.to/4meDdPM)
5.  **Power Supply:** 24V DC supply for your motor/BLDC-8015A [Amazon Link](https://amzn.to/3PTm8ia)
6.  **Resistor:** 10k ohms resistor (pullup resistor for the sensor, coonected between +5V and sensor OUT).
7.  **Acrylic Disc:** Acrylic disc 1/8 thick, 4 inches in diameter for lid. [Amazon Link](https://amzn.to/3PTm8ia)
8.  **Female Barrel connector**  [Amazon Link](https://amzn.to/4sPiO6f)
9.  **Buck Converter** Converts 24 V to 12 V for powering the Arduino. Powering via USB provides a lower voltage (4.7V) than powering via the Vin pin. This is not enough for the controller. [Amazon Link](https://amzn.to/3NLN8zH)
10. **M2,M3,M4 Screws** A set with multiple size/lengths. [Amazon Link](https://amzn.to/4m8HzI2)
11. **M2, M3, M4 Threaded Inserts** Kadrick brand or another brand with same outer diameters. [Amazon Link](https://amzn.to/4m8H00R)

#### **Note**: Clicking these links helps me help others. As an Amazon Associate I earn from qualifying purchases, which is a fancy way of saying Amazon throws a few nickels my way if you buy something. I don't keep the change—it all goes to Doctors Without Borders to support their medical teams worldwide.

## 🔌 Wiring

| Component | Pin Name | Arduino Pin | Notes |
| :--- | :--- | :--- | :--- |
| **BLDC-8015A** | Signal (PWM) | **D9** | Servo-style PWM |
| **BLDC-8015A** | Ground | **GND** | **Common Ground required** |
| **Sensor** | Digital Out | **D8** | Interrupt Pin |
| **Sensor** | VCC | **5V** | Or 3.3V depending on module |
| **Sensor** | GND | **GND** | |
| **10K Resistor** | Digital Out | **D8** | Pullup resistor for hall sensor connects D8 to VCC|



> **⚠️ Important:** Ensure the Arduino and the BLDC-8015A share a common ground connection, or the signal will not work.

## 📦 Installation

1.  **Dependencies:** Install the following libraries via the Arduino Library Manager:
    *   `WiFiS3`
    *   `ArduinoJson`
    *   `Arduino_LED_Matrix`
    *   `Servo Version >= 1.3.0 **⚠️ IMPORTANT**` 
2.  **Web Assets:**
    *   The HTML/JS is compressed into `web_assets.h`.
    *   To modify the UI, edit `index.html` and run: `python convert.py --mode prod index.html web_assets.h`.
3.  **Upload:** Open `SpinCoaterController.ino` in Arduino IDE and upload to the UNO R4 WiFi.

## 📖 How to Use

### 1. First Boot & WiFi Setup
*   Power on the device. The LED Matrix will display an Access Point icon.
*   Connect your phone/PC to the WiFi network **`SpinCoater-XXXX`** (password is usually empty or not set).
*   A captive portal should open. If not, navigate to `http://192.168.4.1`.
*   Enter your local WiFi credentials in the **Settings** card and click **Save WiFi**.
*   The device will reboot and connect to your network. The LED Matrix will scroll the new IP address. You can now access the controller at **`http://spincoater.local`**.

### 2. PWM-to-RPM Mapping (Empirical Tuning)
*   Before using the system, you must characterize your motor's performance.
*   Go to the **RPM Tuning** card.
*   Set your **Start (us)** (usually 0), **End (us)** (usually 2000), and **Step** size (100).
*   Click **Start PWM Mapping**. 
*   The system will automatically ramp the motor and use **Linear Regression** to calculate a best-fit line (Slope and Intercept).
*   This step is mandatory for accurate speed control in **Open-Loop (KV)** mode and provides a baseline for PID feed-forward.

### 4. Creating a Profile
*   Go to the **Profile Editor** card.
*   Click **New**.
*   Enter a name (e.g., "Photoresist").
*   Click **+ Step** to add stages.
    *   **RPM:** Target speed.
    *   **Ramp:** Time (ms) to reach that speed.
    *   **Hold:** Time (ms) to stay at that speed.
    *   **Type:** Linear, Exponential, or S-Curve (smooth).
*   Click **Save**.

### 5. Running a Process
*   Select your profile from the dropdown in the **Dashboard** or **Profile Editor**.
*   Click **START**.
*   Monitor the live RPM gauge and chart.

### 6. PID Tuning
*   If the RPM oscillates or is slow to reach the target:
    *   Go to **PID Controller** settings.
    *   Click **Auto Tune PID**. The motor will spin up and oscillate briefly to calculate ideal P, I, and D values.
    *   Alternatively, adjust `Kp`, `Ki`, `Kd` manually.

## ⚠️ Safety Features

*   **Emergency Stop:** Click the red **STOP** button at any time to immediately cut throttle (0 RPM).
*   **Connection Loss:** If the WebSocket disconnects, the system continues the current profile but will stop if a safety fault occurs.
*   **Startup Protection:** The motor will not spin at boot until explicitly commanded.

## 🔍 Motor Health Diagnostics

The values generated during **PWM-to-RPM Mapping** provide a baseline for your hardware's health. Significant changes in these values during subsequent tunings can indicate maintenance needs:

*   **Calculated Slope (RPM/µs):** Represents motor efficiency. A **decrease** in slope over time suggests increased mechanical resistance (e.g., debris in the spindle), bearing degradation, or a weakening power supply/battery.
*   **Inferred Start PWM (µs):** Represents the "stiction" point.
    *   **Increasing values:** Indicate that the motor requires more power to overcome static friction, often a sign of old grease or tight bearings.
    *   **Values significantly far from 1500µs:** Suggest the BLDC-8015A's internal neutral point has drifted and a fresh **BLDC-8015A Calibration** is recommended.

## 🤖 AI-Assisted Development

This project was developed with the assistance of **Google Gemini**. The prompt.txt file contains the master prompt that defined the initial architecture and requirements, as well as the sequence of refinement prompts used to add features, debug code, and build the UI.

This repository serves as a case study in using advanced generative AI for complex, production-quality embedded systems development.

You can use this file as a template or reference for generating your own complex embedded systems with AI assistance.

---

*This project was co-created with Google Gemini.*
