# Smart Hook: An Intelligent Hostel Security & Card Management System
[Browse in HackMD for best experience 😎](https://hackmd.io/@HC8294/By0vtWmTZe)

## Problem Statement Analysis
As an exchange student living in the SUTD hostel, I face a daily challenge with the dormitory's electronic locking system. The rooms are equipped with auto-locking electric locks that require a physical key card for access.

Being a forgetful person, I frequently encounter a frustrating scenario: leaving my key card inside the room while stepping out for a brief moment. Because the door locks automatically, I often find myself locked out, leading to significant inconvenience and the need for security assistance. 

To solve this, I envisioned a device that doesn't just hold my keys, but also ensures my card is not left and room are always secure.

![OIP](https://hackmd.io/_uploads/Hy8oCIQ6We.jpg)

## Project Overview
Smart Hook is an add-on device mounted directly onto the hostel door. Built on the ESP32-C6 platform and powered by ESP RainMaker, it acts as an intelligent supervisor for both the door's physical state and the user's key card presence.

Unlike traditional smart locks that replace existing hardware, Smart Hook is designed to be retrofitted onto any standard door, making it ideal for student housing where permanent modifications are prohibited.
![IMG_8696 Small](https://hackmd.io/_uploads/By1cZRm6-x.jpg)


### Core Objectives:
- Real-time Monitoring: 
Tracks whether the key card is inserted in its holder and whether the door is properly closed.

- Cloud Connectivity: 
Provides instant status updates and push notifications to the user's smartphone via Wi-Fi.

- Active Prevention (Safety Lock): 
Automatically engages a physical barrier hook (servo-controlled) to prevent accidental lockouts.

### Key Features:
- Dual-Sensor Detection: 
Uses an IR Barrier Sensor for door position and a GY-30 (BH1750) light sensor to detect card insertion via luminosity changes.

- Smart Logic Engine:
If the door is detected as OPEN while the card is still IN THE HOLDER (which means YOU FORGOT YOUR CARD!!!), the system triggers the Anti-Lock Servo to move to the "ON" position.

- Cross-Platform Control: 
Users can manually override the lock or check environmental light levels through the ESP RainMaker mobile app.

- Integrated Alert System: 
Instead of cluttering the user with multiple notifications, the system bundles all telemetry data into a single, easy-to-read "System Status" string.

## Supplies

| Eletronic Component | Purpose |
| -------- | -------- |
| ESP32-C6 | The main MCU with Wi-Fi 6 and Matter support | 
| MG996R Servo Motor | High-torque motor used for the physical blocking mechanism |
| GY-30 (BH1750) | Digital Light Sensor to detect the presence of the key card |
| IR Obstacle Avoidance Sensor | Infrared sensor to monitor the door's open/closed state. |
| Breadboard | circuit assembly |
| Jumper Wires | M-M, M-F wires for interconnecting components |
|Power Bank with Usb-C| power supply |

| Mechanical & Mounting | Purpose |
| -------- | -------- |
| 3D Printed Housing | Custom-designed enclosure to hold the eletronic components and the card slot |
| 3D Printed Hook |  Attached to the servo to act as the physical barrier |
| 3D Printed block | Obstacle for the IR sensor |
| Nano Tape | For secure, non-destructive attachment to the hostel door |

| Software & Development Tools | |
| --- | --- |
| Framework|ESP-IDF v5.5.4 (Official development framework) |
| IoT Platform|ESP RainMaker (Cloud backend and Mobile App) |
| Development Environment|VS Code with ESP-IDF Extension |
| Design Tools| Fusion 360 (for the external casing design).|

## Let's Start!
### Step 1: Hardware Setup
📌Please assemble the circuit using the pinout table and circuit diagram below.
📌Please note that the circuit uses two different power supplies: +3.3V and +5V.

#### Circuit Diagram
![image](https://hackmd.io/_uploads/SJObWu7abe.png)

#### Pin Connection Table

| Component (Role) | Signal | ESP32-C6 GPIO | 
| :--- | :--- | :---: | 
| **MG996R Servo** | PWM (Orange) | GPIO 1 |
|  | +VCC (Red) | **5V** |
| | GND (Brown) | GND |
| **GY-302 (BH1750)** | SDA | GPIO 2 | 
|  | SCL | GPIO 3 | - |
| | VCC (3.3V) | **3.3V** | - |
| | GND | GND |
| **IR Sensor** | OUT | GPIO 4 |
|  | VCC (3.3V) | **3.3V**|
| | GND | GND |

---

### Step 2: Software Development & Programming

The software for Smart Hook is built using the ESP-IDF (Espressif IoT Development Framework) and ESP RainMaker.
[Download in GitHub](https://github.com/HC8294/ESP32C6_Smart_Hook)![Screenshot 2026-04-20 at 9.09.37 PM](https://hackmd.io/_uploads/Sy1oBiQpWg.png)

#### 2.1 Environment Setup
Install ESP-IDF: We used v5.5.4 (don't use v6.0) for ESP32-C6 support. [Installation Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/get-started/index.html)

Clone RainMaker (bash):
```
git clone --recursive https://github.com/espressif/esp-rainmaker.git
```

Set Target: Open your terminal in the project folder and run:
```
idf.py set-target esp32c6
```
---

#### 2.2 Core System Logic
The intelligence of the Smart Hook is built on four functional pillars that transform basic sensors into an active security system.

![image](https://hackmd.io/_uploads/rk_O8oXpZg.png)


1. Safety Interlock (The "Soul")
The device features an automated physical barrier rather than just a manual switch:
    * Trigger: If Door = OPEN AND Card = PRESENT (Lux < 15).
    * Action: The MG996R Servo is forced to the LOCKED position.
    * Goal: Prevents the user from accidentally leaving the room without retrieving their key card.

2. Integrated Status Reporting
We implemented Multi-Dimensional Data Aggregation to simplify the user experience:
    * Dashboard Efficiency: Users can monitor the entire environment at a single glance on the RainMaker app.
    * Unified String: Instead of separate toggles, three physical states are merged into one string: 
Door: [Status] ; Card: [Status] ; Lock: [Status]


3. Data Optimization & Traffic Control
To manage cloud resources and prevent "Notification Fatigue":
    * Threshold Filtering: Updates are only sent if light intensity changes by > 5 Lux.
    * Budget Management: This prevents the device from exceeding the MQTT Budget, ensuring a stable cloud connection even with high-frequency sensing.

---

#### 2.3 Compilation & Flashing
To upload the code to your ESP32-C6:

Build & Flash (Bash):
```
idf.py build flash monitor
```
or click the icon in the tool bar (Wrench/Lightning/Monitor)

![Screenshot 2026-04-20 at 8.24.07 PM](https://hackmd.io/_uploads/BJ_wsqmpZx.png)

📌When the QR code appears in the terminal, scan it using the ESP RainMaker Mobile App (available on iOS/Android).

Follow the prompts to **connect the device to your hostel Wi-Fi**.

---

### Step 3: Mechanical Assembly & Non-Invasive Mounting

Our design focuses on fit for any door and adjustable: it attaches to the door without screws or drills, adhering strictly to hostel housing rules.

--- 

#### 3.1 3D Printed Housing & Components
To securely hold the sensors and the servo, and to provide a dedicated card slot, we designed a custom 3D-printed housing. 
[Here to download the STL & Fusion file and Print it out !](https://grabcad.com/library/smart_hook-1)

The components include:
* The Main Body: With enough space and proper hole for the GY-30, IR sensor, and ESP32-C6 DevKit.
* The Card Case Slot: Ensuring the card sits directly over the light sensor.
![Screenshot 2026-04-20 at 8.01.19 PM](https://hackmd.io/_uploads/B1iCHpQpWl.png =50%x)
![Screenshot 2026-04-20 at 8.01.40 PM](https://hackmd.io/_uploads/SJiRSTQa-l.png =50%x)

* The Anti-Lock Hook: Attached to the MG996R servo horn to act as the physical barrier. **You can adjust the shape or size to fit your door.**
![Screenshot 2026-04-20 at 11.25.48 PM](https://hackmd.io/_uploads/H1CyU6Q6bg.png =50%x)

* The blocker for the IR obstacle sensor: Stick on the door frame, let the door-state sensor work.

    ![Screenshot 2026-04-20 at 11.26.06 PM](https://hackmd.io/_uploads/S1DlITQpbg.png =50%x)


#### 3.2 Physical Assembly 

1. Insert the IR Sensor into the side port, ensuring its transmitter and receiver are facing outwards to detect the door frame.
![IMG_8713 Small](https://hackmd.io/_uploads/rJh3Z0Qp-e.jpg)

2. Carefully fit the ESP32-C6 DevKit into the housing. Make sure the jumper wires still attach on bread board. 

    ![IMG_8712 Small](https://hackmd.io/_uploads/rk_WGRmp-l.jpg)

3. Place the GY-30 Light Sensor directly inside the base of the Card Case Slot.

    ![IMG_8716 Small](https://hackmd.io/_uploads/ByIEMCmpWl.jpg)

5. Attaching all the parts to the hostel door. We achieve this using nano tape.
![IMG_8715 Small](https://hackmd.io/_uploads/H1AIfA7T-e.jpg)
![IMG_8701 Small](https://hackmd.io/_uploads/HJCLzR76-g.jpg)
![IMG_8696 Small](https://hackmd.io/_uploads/HJ0UGCmabl.jpg)
![IMG_8704 Small](https://hackmd.io/_uploads/Hk2Pz07aZg.jpg)

6. WELL DONE!
![image](https://hackmd.io/_uploads/HyJF3omaWl.png)
