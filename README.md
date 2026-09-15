# PFS154 Solar/SuperCap Candle

**An ultra-low-power, solar-harvesting electronic candle powered by a Padauk PFS154 8-bit microcontroller.**

It features authentic multi-channel pseudo-random flickering, automatic day/night sensing using the internal comparator with hysteresis, and smart low-voltage protection for Lithium-Ion Capacitors (LICs).

## 1. Features

- **32-bit Xorshift PRNG Flame Simulation:** Drives 3 independent LED channels with dynamic wave limits for organic, realistic candle motion (Sputter, Normal, and Calm phases).
- **Day/Night Sensing:** Uses the internal analog comparator paired with the solar panel (PA3) and software hysteresis to prevent threshold chattering at dusk/dawn.
- **Deep Sleep Mode:** Automatically goes into ultra-low-power sleep during daylight to charge the supercapacitor, disabling internal pull-ups to eliminate leakage.
- **Low-Voltage Protection:** Protects the LIC supercapacitor by cutting off LED operation before deep discharge, leaving a safe reserve for morning solar recovery.
- **Lean Codebase:** Compiles down to ~1.5 KB with minimal RAM usage, optimized using SDCC for Padauk microcontrollers.

## 2. Hardware Pinout (PFS154 SOP8)

Plaintext

```
                     _________
                    /         |
                1--|VCC    GND|--8
                2--|PA7    PA0|--7 (LED3 Slow PWM)
                3--|PA6    PA4|--6 (LED1 Fast PWM)
(LED2 Med PWM)  4--|PA5    PA3|--5 (Comparator Input/Solar Panel)
                   |__________| 

```

| **Pin** | **Peripheral** |  **Function**                         |
|   ---   |   ----------   |  --------                             |
| **PA0** |    PWMG2       | LED 3 (Slow channel)                  |
| **PA3** |    COMP/ADC    | Solar Panel input (Dusk/Dawn sensing) |
| **PA4** |    PWMG0       | LED 1 (Fast channel)                  |
| **PA5** |    PWMG1       | LED 2 (Medium channel)                |

## 3. Repository Structure

- main.c: Core application logic, PRNG seeding, flicker state machine, and power management.
- Makefile: Build and programming automation using sdcc and easypdkprog.

## 4. Building and Programming

### Prerequisites

You will need:

- **SDCC** (Small Device C Compiler) with Padauk support (-mpdk14).
- **easypdkprog** (Programming tool for Padauk microcontrollers).

### Makefile Commands

- **Build the binary:** make
- **Program the PFS154 chip:** make program
- **Clean build artifacts:** make clean

## 5. Customization and Tuning

### A. Supercapacitor Low-Voltage Cutoff

The battery protection routine (checkbattery()) uses the internal resistor ladder (GPCS) against a 1.2V bandgap reference. You can adjust the cutoff threshold in checkbattery():

| **GPCS Value** | **Ladder Ratio** | **Real-World Cutoff (3.8V LIC)** | **Notes**                         |
|   ----------   |   ------------   |   ----------------------------   |   -----                           |
|     **4**      |      13/32       |           ~3.10V                 |   high                            |
|     **5**      |      14/32       |        **\~2.88V**               | **\[Recommended Starting Point]** |
|     **6**      |      15/32       |           ~2.68V                 |   Deeper discharge                |

### B. Dusk Sensitivity and Hysteresis

Adjust Sensitivity and Hysteresis at the top of main.c to match your specific solar panel characteristics and ambient lighting environment.

```
const uint8_t Sensitivity = 1;  
const uint8_t Hysteresis = 1;   
```

