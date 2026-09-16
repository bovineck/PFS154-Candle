# PFS154 Solar/SuperCap Candle

**An ultra-low-power, solar-harvesting electronic candle powered by a Padauk PFS154 8-bit microcontroller.**

It features authentic multi-channel pseudo-random flickering, automatic day/night sensing using the internal comparator with hysteresis, and smart low-voltage protection for Lithium-Ion Capacitors (LICs).

Use https://github.com/free-pdk/free-pdk-examples for latest (working) includes etc

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

## 6. Python script and output

The script numbers-gen.py is a nice check on the code running in the IC as it can simulate a time frame and produce a graphical output of the LEDs ramping up and down, as well as an audio file that can be played to check for repeats or other side effects. 

The audio file candle_flicker_audio.wav and the png file graph.png are provided as an example using the command:

    "python3 numbers-gen.py -t 1200 -w 15 -a 200 -o graph.png"

The python script accepts the following arguments:

usage: numbers-gen.py [-h] [-s SAMPLES] [-t START] [-w WINDOW] [-a AVG_WINDOW] [--seed SEED] [-o OUTPUT] [--no-audio]

options:
  -h, --help            show this help message and exit

  -s SAMPLES, --samples SAMPLES
                        Total flicker steps to simulate (Auto-calculated from target time if omitted) (default: None)

  -t START, --start START
                        Start time for plot capture in seconds (e.g., 1000 = 16.7 min) (default: 1000.0)

  -w WINDOW, --window WINDOW
                        Window duration to plot in seconds (e.g., 120 = 2 min) (default: 120.0)

  -a AVG_WINDOW, --avg-window AVG_WINDOW
                        Moving average window size (steps) (default: 200)

  --seed SEED           32-bit PRNG initial seed (default: 29012026)

  -o OUTPUT, --output OUTPUT
                        Output PNG filename snapshot (default: None)
                        
  --no-audio            Skip generating WAV audio export (default: False)


The code also contains all the other variables the main.c file contains for you to tinker with, for example:

    flickdelay = 55
    flickdelaysputter = 14
    flickdelaynormal = 55
    flickdelaycalm = 120

Therefore you can see (and hear!) the effect of changing these variables on the simulation.

Enjoy!

OneCircuit

## About OneCircuit
Created by OneCircuit and Gemini for the maker community. This project focuses on professional-grade code for hobbyist hardware.

* **YouTube:** [OneCircuit YouTube Channel](https://www.youtube.com/@onecircuit-as)
* **Blog:** [OneCircuit Blog](https://onecircuit.blogspot.com/)
* **GitHub:** [OneCircuit Repositories](https://github.com/bovineck/)

---

