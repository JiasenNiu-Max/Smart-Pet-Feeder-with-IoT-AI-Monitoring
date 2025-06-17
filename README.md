# Smart Pet Feeder with IoT & AI Monitoring

An intelligent pet feeder system using IoT and embedded computer vision. It automatically dispenses food, monitors food weight, and detects pet presence through a camera and AI model.

## 🐾 Features

- 📷 AI-based pet detection using onboard camera
- ⚖️ Real-time weight monitoring with load cell sensor
- 🕹 Servo motor control for automatic food dispensing
- 📡 ESP32-based IoT connectivity
- 💻 Serial and web-based monitoring (optional expansion)

## 🧰 Hardware Used

- XIAO ESP32S3 Sense (with onboard camera)
- FS5109M Servo Motor
- 1kg Load Cell + HX711 (Makerverse load cell amplifier)
- Breadboard + Jumper Wires
- 5V Power Supply
- Resistors, Connectors, etc.

## 📐 System Architecture

```

\[Pet Detected via Camera] --> \[ESP32 AI Model] --> \[Trigger Servo Motor]
|
\--> \[Read Weight via Load Cell]
|
\--> \[Send Data via Serial/WiFi]

```

## 🚀 How to Set Up

1. **Hardware Setup**
   - Connect the servo to control the food door
   - Wire the load cell to HX711 and connect to ESP32
   - Ensure camera is functional on ESP32S3 Sense

2. **Software Setup**
   - Use PlatformIO or Arduino IDE
   - Install necessary libraries:
     - `ESP32Camera`
     - `HX711`
     - `Servo`
   - Flash the firmware via USB

3. **Run**
   - Pet approaches feeder → camera detects pet → motor activates → food dispensed
   - Weight sensor confirms amount dispensed

## 📊 Future Improvements

- WiFi upload of food logs
- Camera feed via web interface
- Pet recognition via deep learning
- MQTT integration for smart home

## 🧑‍💻 Authors

- [Your Name]
- University of Western Australia, 2025
- CITS5506 – Internet of Things

## 📄 License

This project is for educational use only.
