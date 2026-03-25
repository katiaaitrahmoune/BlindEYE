<div align="center">

# 👁️ BlindEYE
### *Redonner la vue autrement*

**An AI-powered wearable assistive device for the visually impaired.**

![ESP32](https://img.shields.io/badge/Hardware-ESP32--S3-E7352C?style=flat-square&logo=espressif)
![AI](https://img.shields.io/badge/AI-On--Device_Inference-FF6F00?style=flat-square&logo=tensorflow)
![I2S](https://img.shields.io/badge/Audio-I2S_INMP441-0078D4?style=flat-square)
![Camera](https://img.shields.io/badge/Vision-OV2640_Camera-47A248?style=flat-square)
![Status](https://img.shields.io/badge/Status-Prototype-lightgrey?style=flat-square)

**Ait Rahmoune Katia · Daho Bachir Ghada · Meghraoui Fatima Zohra**

</div>

---

## 🌍 Problem Statement

According to the WHO, nearly **2.2 billion people** worldwide live with a visual impairment or blindness — of whom approximately **39 million are completely blind**. For these individuals, everyday tasks like navigating a space, identifying objects, or handling money represent constant challenges.

**BlindEYE** was built to address this reality: a lightweight, head-worn intelligent device that integrates advanced AI to help visually impaired users perceive and interact with their environment independently.

---

## 🎯 Objectives

1. **Facilitate mobility** — help users perceive their surroundings through a smart wearable device.
2. **Identify objects, colors, text, and currency** — via simple voice commands.
3. **Ensure real-time safety** — detect nearby obstacles and emit immediate audio alerts.
4. **Deliver a comfortable, accessible experience** — through a lightweight, ergonomic, hands-free design.
5. **Leverage AI continuously** — improve visual recognition accuracy over time.

---

## ⚙️ Functional Features

### What BlindEYE allows the user to do

| Feature | Description |
|---|---|
| 🌐 **Environment perception** | Integrated camera analyzes the scene in real time |
| 🔔 **Obstacle alerts** | Audio warning triggered when an obstacle is detected nearby |
| 🗣️ **Object & text recognition** | Identifies objects, colors, and text via voice feedback |
| 💰 **Currency recognition** | Detects and counts banknotes to assist with daily transactions |
| 🎙️ **Voice commands** | Activate or deactivate features with simple spoken instructions |
| 🔊 **Directional audio guidance** | Helps the user navigate using spatial audio feedback |

### What BlindEYE must integrate

| Component | Role |
|---|---|
| Front-facing camera | Image capture and real-time scene analysis |
| Sensitive microphone (I2S) | Voice command capture |
| Directional speaker | Clear audio feedback without isolating the user from the environment |
| Embedded microcontroller | On-device AI processing and sensor management |
| Bluetooth / Wi-Fi | Wireless communication with the mobile app |
| Rechargeable battery | Several hours of autonomous operation |

### Non-functional requirements

- ⚡ Fast response time between detection and audio output
- 🙌 Fully hands-free and intuitive operation
- 🏋️ Lightweight and ergonomic form factor
- 🌧️ Resistant to dust, light humidity, and minor shocks

---

## 🔩 Hardware Components

| Component | Qty | Specs | Role | Unit Cost |
|---|---|---|---|---|
| **ESP32-S3-CAM Board** | 1 | Wi-Fi + Bluetooth MCU | Main controller, AI processing | ~€20 |
| **Camera OV2640 / OV5640** | 1 | 2MP / 5MP, ESP32-compatible | Real-time image capture | ~€8 |
| **Microphone INMP441 (I2S)** | 1 | Low-noise digital mic | Voice commands & ambient sound | ~€6 |
| **Amplifier MAX98357A + 8Ω Speaker** | 1 | 3W audio output, I2S | Audio feedback to user | ~€7 |
| **Mini Power Bank** | 1 | 5V / 2000–5000 mAh | Portable power supply | ~€10 |
| **SPST Slide Switch** | 1 | Miniature slide switch | Power on/off | ~€2 |
| **FTDI Adapter** | 1 | USB to UART | Programming & data transfer | ~€5 |
| | | | **Total** | **~€58** |

---

## 🏗️ System Architecture

### Operating Flow

```
Power ON
    │
    ▼
Initialize sensors
    │
    ▼
Wait for event
    │
    ├── Voice command detected?
    │       ├── YES → Audio analysis (INMP441)
    │       │           → AI processing on ESP32-S3
    │       │           → Send data to backend
    │       │           → Receive result → Voice feedback
    │       │
    │       └── NO → Obstacle detected?
    │                   ├── YES → On-device AI analysis
    │                   │         → Distance estimation
    │                   │         → Instant audio alert
    │                   │
    │                   └── NO → Idle / wait for next event
    │
    ▼
Return to standby mode
```

---

## 🖨️ 3D Device Model

> Head-worn ergonomic form factor — lightweight, discrete, and designed for everyday use.

![BlindEYE 3D Model](https://private-user-images.githubusercontent.com/199088353/518175333-470e7404-1e24-4580-bba9-e2e46fb34207.png)

---

## 🚀 Getting Started

### Prerequisites

- PlatformIO or Arduino IDE with ESP32-S3 board support
- Python 3.8+ (for backend/mobile AI inference)

### Flash the ESP32-S3

```bash
git clone https://github.com/katiaaitrahmoune/BlindEYE.git
cd BlindEYE
# Open in PlatformIO or Arduino IDE
# Select board: ESP32-S3-DevKitC-1
# Flash firmware
```

### Configure Wi-Fi / Backend

In `config.h`:

```cpp
#define WIFI_SSID     "your_ssid"
#define WIFI_PASSWORD "your_password"
#define BACKEND_URL   "http://your-backend-ip:PORT"
```

---

## 🔮 Roadmap

- [x] Hardware selection and BOM
- [x] System architecture design
- [x] 3D device modeling
- [x] Functional specification (cahier des charges)
- [ ] Firmware: voice command detection (INMP441 I2S)
- [ ] Firmware: obstacle detection with camera
- [ ] On-device AI model integration (object/text/color recognition)
- [ ] Currency recognition module
- [ ] Mobile companion app
- [ ] Field testing with visually impaired users

---

## 👥 Team

| Name | Role |
|---|---|
| Ait Rahmoune Katia | Embedded systems, firmware |
| Daho Bachir Ghada | AI / ML integration |
| Meghraoui Fatima Zohra | Hardware & system design |

---

## 💬 Conclusion

BlindEYE is more than an assistive device — it is a bridge between humans and technology, designed to restore confidence and independence to visually impaired users. This prototype lays a solid foundation for future improvements and potential large-scale production.

---

<div align="center">
  <i>"Not just a device — a path back to independence."</i>
</div>
