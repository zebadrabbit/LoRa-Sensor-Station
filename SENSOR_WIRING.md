# Sensor Wiring Guide

## Heltec WiFi LoRa 32 V3 - Sensor Node Configuration

This guide covers wiring for all supported sensors on the Heltec WiFi LoRa 32 V3 sensor nodes.

---

## Table of Contents

1. [Pin Reference](#pin-reference)
2. [Thermistor Sensors (10kΩ NTC)](#thermistor-sensors-10kω-ntc)
3. [I2C Sensors](#i2c-sensors)
4. [I2C Sensor Specifications](#i2c-sensor-specifications)
5. [Power Considerations](#power-considerations)
6. [Troubleshooting](#troubleshooting)

---

## Pin Reference

### Heltec WiFi LoRa 32 V3 GPIO Pinout

**Power Pins:**

- **3.3V** - Regulated 3.3V output (max 500mA)
- **5V** - USB/Battery voltage (use with caution)
- **GND** - Ground

**I2C Bus (JST Connector):**

- **GPIO 41** - SDA (I2C Data)
- **GPIO 42** - SCL (I2C Clock)
- **3.3V** - Power supply for I2C devices
- **GND** - Ground

**ADC Pins (for Thermistors):**

- **GPIO 1-10** - ADC capable (3.3V max input)
- Default configuration uses GPIO 1, 2, 3, 4

**Reserved Pins (Do Not Use):**

- **GPIO 18** - LoRa SCK
- **GPIO 14** - LoRa MISO
- **GPIO 13** - LoRa MOSI
- **GPIO 8** - LoRa CS
- **GPIO 12** - LoRa RST
- **GPIO 14** - LoRa DIO1
- **GPIO 37** - Internal Display SCL
- **GPIO 38** - Internal Display SDA
- **GPIO 48** - Internal Display RST

---

## Thermistor Sensors (10kΩ NTC)

### What You Need

- **10kΩ NTC thermistor** (Negative Temperature Coefficient)
- **10kΩ resistor** (1% tolerance recommended)
- Wire (22-24 AWG)
- Optional: Heat shrink tubing for connections

### Voltage Divider Circuit

The thermistor is configured as a voltage divider:

```
3.3V ----[10kΩ Resistor]----+----[Thermistor]---- GND
                             |
                          GPIO Pin
                        (ADC Input)
```

### Wiring Instructions

**For 1-Meter Cable Thermistors:**

1. **Identify Thermistor Leads:**

   - Thermistors are non-polarized (either lead can be used)
   - If your thermistor has color-coded wires, note which is which

2. **Connect Fixed Resistor:**

   - Solder a 10kΩ resistor between:
     - **3.3V pin** on Heltec board
     - One lead of the thermistor

3. **Connect Thermistor to Ground:**

   - Connect the other thermistor lead to **GND**

4. **Connect ADC Sense Wire:**

   - Connect GPIO pin to the junction between resistor and thermistor
   - Use GPIO 1, 2, 3, or 4 (default configuration)

5. **Insulate Connections:**
   - Use heat shrink tubing over solder joints
   - Ensure no shorts between connections

### Recommended GPIO Assignment

| Sensor # | GPIO Pin | Function       |
| -------- | -------- | -------------- |
| Sensor 1 | GPIO 1   | Thermistor ADC |
| Sensor 2 | GPIO 2   | Thermistor ADC |
| Sensor 3 | GPIO 3   | Thermistor ADC |
| Sensor 4 | GPIO 4   | Thermistor ADC |

### Example Wiring Diagram (Single Thermistor)

```
Heltec Board                    Thermistor
                                (1m cable)
┌─────────────┐
│             │
│  3.3V   ────┼────[10kΩ]────┬────[ Thermistor ]───┐
│             │               │                     │
│  GPIO 1 ────┼───────────────┘                     │
│             │                                     │
│  GND    ────┼─────────────────────────────────────┘
│             │
└─────────────┘
```

### Multiple Thermistor Setup

For multiple thermistors, repeat the circuit for each sensor:

```
3.3V ----[10kΩ]----+----[Therm1]---- GND
                   └── GPIO 1

3.3V ----[10kΩ]----+----[Therm2]---- GND
                   └── GPIO 2

3.3V ----[10kΩ]----+----[Therm3]---- GND
                   └── GPIO 3

3.3V ----[10kΩ]----+----[Therm4]---- GND
                   └── GPIO 4
```

### Calibration

The firmware uses the **Steinhart-Hart equation** for temperature calculation:

- **Beta coefficient (B)**: 3950K (typical for 10kΩ NTC)
- **Reference resistance**: 10kΩ @ 25°C

If your thermistors have different specifications, update in the code:

- File: `include/sensors/thermistor_sensor.h`
- Constants: `THERMISTOR_NOMINAL`, `BETA_COEFFICIENT`, `TEMPERATURE_NOMINAL`

---

## I2C Sensors

### JST Connector Pinout

The Heltec WiFi LoRa 32 V3 has a built-in JST connector for I2C devices:

```
JST Connector (4-pin, 1.25mm pitch)
┌─────────────────┐
│ 1  2  3  4      │  View from front
└─────────────────┘

Pin 1: GND    (Black)
Pin 2: 3.3V   (Red)
Pin 3: SDA    (Blue/White)
Pin 4: SCL    (Yellow/White)
```

### Standard I2C Wiring

**If using JST connector:**

- Simply plug in compatible I2C sensors
- Power and data are provided automatically

**If wiring directly:**

| Heltec Pin | I2C Sensor Pin | Wire Color (Typical) |
| ---------- | -------------- | -------------------- |
| GND        | GND            | Black                |
| 3.3V       | VCC/VIN        | Red                  |
| GPIO 41    | SDA            | Blue/White           |
| GPIO 42    | SCL            | Yellow/White         |

### I2C Bus Topology

Multiple I2C sensors can share the same bus:

```
Heltec Board
┌──────────────┐
│ GPIO 41 (SDA)├───┬───[BME680]
│              │   │
│ GPIO 42 (SCL)├───┼───[BH1750]
│              │   │
│ 3.3V         ├───┼───[INA219]
│              │   │
│ GND          ├───┴───[Common Ground]
└──────────────┘
```

**Important:**

- All sensors share the **same SDA and SCL lines**
- Each sensor needs its **own power and ground connection**
- Maximum cable length: **1 meter** (for reliable communication)
- Pull-up resistors: **Built into Heltec board** (no external resistors needed)

---

## I2C Sensor Specifications

### BME680 - Environmental Sensor

**Measures:** Temperature, Humidity, Pressure, Gas Resistance

**Pinout:**
| BME680 Pin | Connect To |
|------------|------------|
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO 41 |
| SCL | GPIO 42 |
| CS | (Leave unconnected for I2C mode) |
| SDO | GND (for 0x76) or 3.3V (for 0x77) |

**I2C Addresses:**

- **0x76** - SDO pin connected to GND (default)
- **0x77** - SDO pin connected to 3.3V

**Typical Values:**

- Temperature: -40°C to +85°C
- Humidity: 0% to 100% RH
- Pressure: 300 to 1100 hPa
- Gas Resistance: 0 to 200 kΩ

---

### BH1750 - Light Intensity Sensor

**Measures:** Ambient Light (Lux)

**Pinout:**
| BH1750 Pin | Connect To |
|------------|------------|
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO 41 |
| SCL | GPIO 42 |
| ADDR | GND (for 0x23) or 3.3V (for 0x5C) |

**I2C Addresses:**

- **0x23** - ADDR pin connected to GND (default)
- **0x5C** - ADDR pin connected to 3.3V

**Measurement Range:**

- 1 to 65,535 lux
- Resolution: 0.5 lux (high-resolution mode)

**Modes:**

- Continuous High-Resolution Mode (default)
- One-Time Measurement Mode
- Low/High Resolution options

---

### INA219 - Power Monitor

**Measures:** Voltage, Current, Power

**Pinout:**
| INA219 Pin | Connect To |
|------------|------------|
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO 41 |
| SCL | GPIO 42 |
| VIN+ | Positive power supply to monitor |
| VIN- | Negative power supply to monitor |

**I2C Address:**

- **0x40** - Default address

**Measurement Range:**

- Bus Voltage: 0 to 26V
- Shunt Voltage: ±320mV
- Current: ±3.2A (with 0.1Ω shunt resistor)
- Power: Up to 83.2W

**Typical Use Case:**

- Monitor battery voltage and current draw
- Measure solar panel power output
- Track power consumption of devices

---

## I2C Address Table

| Sensor | Default Address | Alternate Address | Address Selection    |
| ------ | --------------- | ----------------- | -------------------- |
| BME680 | 0x76            | 0x77              | SDO pin (GND/3.3V)   |
| BH1750 | 0x23            | 0x5C              | ADDR pin (GND/3.3V)  |
| INA219 | 0x40            | 0x41, 0x44, 0x45  | A0/A1 solder jumpers |

**Reserved Addresses (Do Not Use):**

- **0x3C** - Internal OLED display
- **0x3D** - Alternate OLED address

**Auto-Detection:**
The firmware automatically scans these addresses on boot:

- 0x23, 0x40, 0x5C, 0x76, 0x77

---

## Power Considerations

### Current Draw

**Typical Current Consumption:**
| Device | Active | Sleep |
|-----------------|---------|---------|
| Heltec Board | 80mA | 10µA |
| BME680 | 12mA | 0.15µA |
| BH1750 | 0.12mA | 1µA |
| INA219 | 1mA | N/A |
| Thermistor | 0.33mA | 0.33mA |

**Total (All Sensors):**

- Active: ~95mA
- Sleep: ~11µA (without thermistors)

### Power Supply Notes

1. **3.3V Rail:**

   - Maximum current: 500mA
   - Safe for all sensors simultaneously

2. **Battery Operation:**

   - Recommended: 3.7V LiPo (1000mAh or larger)
   - Expected runtime: 10-12 hours (continuous operation)
   - Sleep mode: Several days to weeks

3. **USB Power:**
   - Always available during development
   - 5V from USB, regulated to 3.3V on board

---

## Wiring Best Practices

### General Guidelines

1. **Cable Length:**

   - Thermistors: Up to 10 meters (minimal signal degradation)
   - I2C sensors: Maximum 1 meter (reliable communication)
   - Use shielded cable for longer I2C runs

2. **Wire Gauge:**

   - Power lines: 22-24 AWG
   - Signal lines: 24-28 AWG
   - I2C bus: 24-26 AWG (twisted pair recommended)

3. **Strain Relief:**

   - Use hot glue or cable ties at connection points
   - Prevent stress on solder joints
   - JST connector provides built-in strain relief

4. **Environmental Protection:**
   - Thermistor probe: Waterproof with heat shrink or epoxy
   - I2C sensors: Use IP-rated enclosures if outdoor
   - Board: Conformal coating for humid environments

### Connection Checklist

Before powering on:

- [ ] Verify all ground connections are common
- [ ] Check for shorts between power and ground
- [ ] Confirm correct voltage (3.3V, not 5V)
- [ ] Verify I2C pull-ups present (internal to board)
- [ ] Test continuity of all connections
- [ ] Ensure proper polarity for sensors
- [ ] Check no loose wires or exposed conductors

---

## Troubleshooting

### Thermistor Issues

**Problem: Reading shows -127°C or invalid temperature**

- **Cause:** Open circuit or poor connection
- **Fix:** Check solder joints, verify continuity with multimeter

**Problem: Temperature reading doesn't change**

- **Cause:** Shorted thermistor or wrong resistor value
- **Fix:** Measure thermistor resistance (should be ~10kΩ at 25°C)

**Problem: Reading is stable but inaccurate**

- **Cause:** Wrong beta coefficient or reference resistance
- **Fix:** Update calibration values in firmware

**Resistance Check:**

- At 25°C (77°F): Should read ~10kΩ
- At 0°C (32°F): Should read ~32kΩ
- At 50°C (122°F): Should read ~3.6kΩ

### I2C Sensor Issues

**Problem: Sensor not detected**

- **Cause:** Wrong I2C address, loose connection, or power issue
- **Fix:**
  1. Run I2C scanner (included in firmware)
  2. Check power supply (should be 3.3V)
  3. Verify SDA/SCL connections
  4. Test with shorter cable
  5. Check address selection pins (SDO/ADDR)

**Problem: Intermittent readings**

- **Cause:** Long cable, electrical noise, or weak pull-ups
- **Fix:**
  1. Shorten I2C cable to under 1 meter
  2. Use twisted pair or shielded cable
  3. Add external pull-up resistors (4.7kΩ to 10kΩ)
  4. Move away from sources of EMI

**Problem: Multiple sensors on bus not working**

- **Cause:** I2C address conflict
- **Fix:**
  1. Verify each sensor has unique address
  2. Configure address selection pins
  3. Check reserved addresses (0x3C, 0x3D)

### Testing I2C Bus

Use serial monitor to view I2C scan results:

```
I2C Bus Scan:
Found device at 0x23 (BH1750)
Found device at 0x40 (INA219)
Found device at 0x76 (BME680)
Scan complete.
```

### Multimeter Testing

**Voltage Checks:**

- 3.3V rail: Should measure 3.25V to 3.35V
- GPIO pins: 0V (low) or 3.3V (high)
- I2C lines (idle): Should be pulled to 3.3V

**Resistance Checks:**

- I2C pull-up: 4kΩ to 10kΩ to 3.3V
- Thermistor: ~10kΩ at room temperature
- Power to ground: Should be infinite (no short)

---

## Example Multi-Sensor Setup

### Outdoor Weather Station Configuration

**Sensors:**

1. **BME680** (0x76) - Temperature, Humidity, Pressure, Air Quality
2. **BH1750** (0x23) - Light intensity
3. **Thermistor** (GPIO 1) - Soil temperature
4. **Thermistor** (GPIO 2) - Water temperature

**Wiring:**

```
Heltec Board
├── JST Connector
│   ├── BME680 (0x76) - Ambient conditions
│   └── BH1750 (0x23) - Light sensor
├── GPIO 1 + 10kΩ resistor
│   └── Thermistor - Soil probe (1m cable)
└── GPIO 2 + 10kΩ resistor
    └── Thermistor - Water probe (1m cable)
```

**Result:**

- 7 total sensor values transmitted
- Single packet format (MultiSensorPacket)
- Update every 60 seconds
- LoRa transmission to base station

---

## Safety Notes

⚠️ **Important Safety Information:**

1. **Voltage Limits:**

   - Never apply more than 3.3V to GPIO pins
   - ESP32-S3 is **NOT** 5V tolerant
   - Overvoltage can permanently damage the board

2. **Current Limits:**

   - GPIO pins: Maximum 40mA per pin
   - 3.3V rail: Maximum 500mA total
   - Exceeding limits may damage voltage regulator

3. **Thermistor Safety:**

   - NTC thermistors can self-heat at high current
   - Keep current below 1mA (achieved with 10kΩ resistor)
   - Do not exceed thermistor temperature rating (usually 125°C)

4. **Static Discharge:**

   - ESP32 is sensitive to ESD
   - Ground yourself before handling board
   - Use ESD-safe work surface if possible

5. **Polarity:**
   - Reversing power polarity can destroy the board
   - Always double-check connections before power-on
   - Use keyed connectors when possible

---

## Firmware Configuration

### Default Sensor Configuration

**File:** `src/main.cpp`

Default GPIO assignments:

```cpp
// Thermistor sensor pins
#define THERMISTOR_PIN_1  1
#define THERMISTOR_PIN_2  2
#define THERMISTOR_PIN_3  3
#define THERMISTOR_PIN_4  4

// I2C pins (hardware defined)
#define SDA_PIN  41
#define SCL_PIN  42
```

### Adding More Thermistors

To add additional thermistors:

1. Choose available GPIO pins (avoid reserved pins)
2. Update sensor configuration in `src/main.cpp`
3. Add voltage divider circuit with 10kΩ resistor
4. Firmware will auto-detect and configure

### Modifying I2C Sensors

The firmware auto-detects I2C sensors on these addresses:

- 0x23, 0x40, 0x5C, 0x76, 0x77

No code changes needed for standard sensors!

---

## Additional Resources

### Datasheets

- **BME680:** [Bosch Sensortec](https://www.bosch-sensortec.com/products/environmental-sensors/gas-sensors/bme680/)
- **BH1750:** [ROHM Semiconductor](https://www.mouser.com/datasheet/2/348/bh1750fvi-e-186247.pdf)
- **INA219:** [Texas Instruments](https://www.ti.com/product/INA219)
- **ESP32-S3:** [Espressif Systems](https://www.espressif.com/en/products/socs/esp32-s3)

### Community Support

- GitHub Issues: [Project Repository]
- Discord: [Your Server]
- Documentation: [FEATURES.md](FEATURES.md)

---

## Revision History

**v2.15.0 (December 14, 2025)**

- Added I2C sensor support (BME680, BH1750, INA219)
- Added thermistor wiring documentation
- Auto-detection for I2C sensors
- Multi-sensor packet format

---

_Last Updated: December 14, 2025_
