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

The simulation script `numbers-gen.py` models the PFS154 Supercapacitor Candle, providing a quick sanity check on the C code running in the microcontroller. It can simulate specific timeframes to generate graphical plots of LED brightness, export audio representations of the flicker pattern, and output raw PRNG data streams for statistical randomness testing using `dieharder`.

### Basic Usage & Example

The visual snapshot (`graph.png`) and audio file (`candle_flicker_audio.wav`) in this repository were generated using:

```bash
python3 numbers-gen.py -t 1200 -w 15 -a 200 -o graph.png
```

### Complete list of Command-Line Arguments

```text
usage: numbers-gen.py [-h] [-s SAMPLES] [-t START] [-w WINDOW] [-a AVG_WINDOW] [--seed SEED] [-o OUTPUT] [--no-audio]
                      [--dieharder]

PFS154 Supercapacitor Candle Simulation with ASCII Dieharder PRNG Generator

options:
  -h, --help            show this help message and exit
  -s SAMPLES, --samples SAMPLES
                        Total steps to simulate (For --dieharder: total numbers to output, default=10,000,000)
  -t START, --start START
                        Start time for plot capture in seconds (e.g., 1000 = 16.7 min)
  -w WINDOW, --window WINDOW
                        Window duration to plot in seconds (e.g., 120 = 2 min)
  -a AVG_WINDOW, --avg-window AVG_WINDOW
                        Moving average window size (steps)
  --seed SEED           32-bit PRNG initial seed
  -o OUTPUT, --output OUTPUT
                        Output snapshot PNG file OR output text file when --dieharder is set
  --no-audio            Skip generating WAV audio export
  --dieharder           Export raw 32-bit ASCII PRNG numbers formatted for dieharder analysis
```

### Dieharder Randomness Testing

The script includes a `--dieharder` flag to output raw 32-bit ASCII values directly formatted for the `dieharder` test suite.

**Pipe directly into dieharder:**
```bash
python3 numbers-gen.py --dieharder -s 10000000 | dieharder -a -g 201
```

**Export to an ASCII file for analysis:**
```bash
python3 numbers-gen.py --dieharder -s 5000000 -o prng_stream.txt
dieharder -a -g 201 -f prng_stream.txt
```

### Customizing Simulation Variables

The script replicates key state variables present in `main.c`, allowing you to hear and see the immediate effects of changing flicker dynamics before flashing firmware:

```python
flickdelay = 55
flickdelaysputter = 14
flickdelaynormal = 55
flickdelaycalm = 120
```

Enjoy!

OneCircuit

## About OneCircuit
Created by OneCircuit and Gemini for the maker community.

* **YouTube:** [OneCircuit YouTube Channel](https://www.youtube.com/@onecircuit-as)
* **Blog:** [OneCircuit Blog](https://onecircuit.blogspot.com/)
* **GitHub:** [OneCircuit Repositories](https://github.com/bovineck/)

---

## 📚 References & Further Reading

For those interested in a deeper mathematical and engineering dive into the **Xorshift32 PRNG**, linear feedback shift registers, and statistical randomness testing suites, check out the following references:

### Academic Papers & Primary Sources

1. **[Xorshift RNGs (2003)](https://www.jstatsoft.org/article/view/v008i14)** — *George Marsaglia (Journal of Statistical Software)*  
   *The foundational paper introducing Xorshift generators. Provides matrix proofs for Galois Field $GF(2)$ shift triplets, proves maximal periods ($2^N - 1$), and lists primitive shift tuplets for 16-bit, 32-bit, and 64-bit integer states.*

2. **[An Experimental Comparison of Software Pseudo-Random Number Generators (2014)](https://arxiv.org/abs/1402.6246)** — *Sebastiano Vigna (arXiv)*  
   *Breaks down the linear properties of Xorshift algorithms, bit-flip mechanics, linear complexity, and state-space traversal.*

3. **[TestU01: A C Library for Empirical Testing of Random Number Generators (2007)](https://dl.acm.org/doi/10.1145/1268776.1268777)** — *Pierre L'Ecuyer & Richard Simard (ACM TOMS)*  
   *The definitive academic testing framework (SmallCrush/BigCrush), explaining why matrix-based shift PRNGs outperform traditional Linear Congruential Generators (LCGs).*

---

### Empirical Testing & Standards

4. **[Dieharder: A Random Number Test Suite Documentation](https://webhome.phy.duke.edu/~rgb/General/dieharder.php)** — *Robert G. Brown (Duke University Physics)*  
   *User manual for the exact test suite used to evaluate this project. Explains $p$-value derivation, uniform distribution testing, and stream analysis.*

5. **[NIST SP 800-22 Rev. 1a: Statistical Test Suite for PRNGs](https://csrc.nist.gov/publications/detail/sp/800-22/rev-1a/final)** — *National Institute of Standards and Technology (NIST)*  
   *The gold-standard specification detailing tests for bit-level independence, frequency, monobits, and runs.*

---

### Implementation & Optimization

6. **[Numerical Recipes: The Art of Scientific Computing (3rd Edition)](3rd Edition)** — *Press, Teukolsky, Vetterling, & Flannery*  
   *Section 7.1 evaluates Xorshift generators alongside classic LCGs and Mersenne Twister, highlighting zero-overhead microcontroller implementations.*

7. **[Fast Random Integer Generation in an Interval (2019)](https://arxiv.org/abs/1805.10941)** — *Daniel Lemire (ACM TOMS)*  
   *Explains range-mapping math, eliminating modulo bias, and optimizing performance on constrained microcontrollers.*

8. **[Xorshift Random Number Generators Architecture Breakdown](https://www.alanzucconi.com/2026/08/15/xorshift-generators/)** — *Alan Zucconi*  
   *An intuitive, visual breakdown of bitwise operations (`^`, `<<`, `>>`) demonstrating how three linear shifts diffuse bit state across a 32-bit integer.*