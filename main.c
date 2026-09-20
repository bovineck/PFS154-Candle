/*
  PFS154 based Solar/SuperCap "Candle"

  Pseudo-random flickering to simulate a candle. Output is through
  3xPWM channels, variables can be changed to alter the simulation.

  Features:
  - 32 bit PRNG 3-LED (3D) candle flickering
  - Day/Night sensing through internal comparator with hysteresis
  - Deep sleep mode for power conservation when not in operation
  - Low-voltage protection of LIC super capacitor

  Tue 15 Sep 2026 16:15:47 AEST

  DEVICE = PFS154
  F_CPU = 50000 Hz
  TARGET_VDD = 3.0 V
                     _________
                    /         |
                1--|VCC    GND|--8
                2--|PA7    PA0|--7 (LED3 Slow PWM)
                3--|PA6    PA4|--6 (LED1 Fast PWM)
(LED2 Med PWM)  4--|PA5    PA3|--5 (Comparator Input/Solar Panel)
                   |__________| 

                 output           off             on              pullup    
                 ------           ---             --              ------
  pin 2 PA7 PAC = 0b10000000, PA = 0b10000000, PA = 0b00000000, PAPH = 0b10000000
  pin 3 PA6 PAC = 0b01000000, PA = 0b01000000, PA = 0b00000000, PAPH = 0b01000000
  pin 4 PA5 PAC = 0b00100000, PA = 0b00100000, PA = 0b00000000, PAPH = 0b00100000 (LED2 - Med PWM / PWMG1)
  pin 5 PA3 PAC = 0b00001000, PA = 0b00001000, PA = 0b00000000, PAPH = 0b00001000 (Comparator IN-Solar Panel)
  pin 6 PA4 PAC = 0b00010000, PA = 0b00010000, PA = 0b00000000, PAPH = 0b00010000 (LED1 - Fast PWM / PWMG0)
  pin 7 PA0 PAC = 0b00000001, PA = 0b00000001, PA = 0b00000000, PAPH = 0b00000001 (LED3 - Slow PWM / PWMG2)

*/

// library includes
#include <stdint.h>
#include <pdk/device.h>
#include "auto_sysclock.h"
#include "startup.h"
#include "delay.h"
#include <stdbool.h>
#include <stdlib.h>

// --------------------------------------
// Global Variables & Simulation Settings
// --------------------------------------

// Seed for pseudorandom flicker generation (32-bit Xorshift state)
uint32_t myrand = 29012026; // 32-bit seed (must be non-zero)

// Initialise variables for the 3 LED channels
uint8_t slowcounter = 0;
uint8_t medcounter  = 0;
uint8_t fastcounter = 0;

uint8_t slowstart = 0;
uint8_t slowend   = 0;
uint8_t medstart  = 0;
uint8_t medend    = 0;
uint8_t faststart = 0;
uint8_t fastend   = 0;

uint8_t faster  = 0; 
uint8_t medstep = 0;

// Delay durations (in microseconds) per step for each flicker intensity mode
uint8_t flickdelay              = 40; // initial working delay

// Mood delay constants: Lower values = faster/more frantic, Higher values = slower/smoother
const uint8_t flickdelaysputter = 14; // e.g. range: 10 to 30   (Erratic, sputtering flare-ups)
const uint8_t flickdelaynormal  = 55; // e.g. range: 40 to 80   (Standard natural candle flicker)
const uint8_t flickdelaycalm    = 120;// e.g. range: 90 to 200  (Gentle, slow glowing transitions)

// Initial countdown steps before first mode evaluation
uint8_t delaycounter = 50;

// Duration multiplier for flicker modes (Sputter/Normal/Calm).
// Scales gimmerand(1,100) step count; higher values make the
// mood phases last longer (e.g. 20 = ~1s to 60s).
uint8_t delaydelay   = 20;

// Dusk Sensitivity Settings (Internal Resistor Ladder Step 0 to 15)
const uint8_t Sensitivity = 1; 

// Hysteresis gap step (raises threshold by ~0.23V when candle is ON)
const uint8_t Hysteresis = 1; 

// System state tracking: true = Daylight (Sleep), false = Night (Flicker)
bool sunshine = false; 

// Flat array (9 rows * 4 columns = 36 bytes) storing wave limit boundaries
// can adjust to change flickering max and min limits (0-255)
uint8_t waves[36] = {
  /* Row 0: Sputter slow */  4,  6,  40,  50,
  /* Row 1: Sputter med  */  6,  8,  50,  80,
  /* Row 2: Sputter fast */  8, 10, 110, 130,
  /* Row 3: Normal slow  */ 15, 25,  60, 100,
  /* Row 4: Normal med   */ 10, 25, 110, 140,
  /* Row 5: Normal fast  */ 20, 25, 100, 120,
  /* Row 6: Calm slow    */ 40, 60, 100, 140,
  /* Row 7: Calm med     */ 50, 70, 120, 160,
  /* Row 8: Calm fast    */ 70, 80, 140, 180
};

// initialise array start (12 = Row 3 "Normal" mode)
uint8_t choosearray = 12; 

// starting ramp directions (true = up, false = down)
bool fastup = true;
bool slowup = true;
bool medup  = true;

// -------------------
// Function Prototypes
// -------------------
// Declarations to give the compiler notice of functions coming further down the code, declaring them here 
// to keep the code organised and allow functions to be ordered logically.
void mydelay(uint8_t counter);
uint16_t gimmerand(uint16_t small, uint16_t big);
void getnewslow(uint8_t base);
void getnewmed(uint8_t base);
void getnewfast(uint8_t base);
void Interrupt(void);
bool checksolar(void);
bool checkbattery(void);
void candlingon(void);
void candlingoff(void);
void sleepnow(void);

// ------------------------
// Function Implementations
// ------------------------

// custom delay function - needed for the PFS154 to handle time better when the
// timing is called dynamically
void mydelay(uint8_t counter) {
  for (uint8_t thiscount = 0; thiscount <= counter; thiscount++) {
    _delay_us(1);
  }
}

// Xorshift32 PRNG: Bumps sequence length to 4,294,967,295 steps (~2 years)
// More about this here - https://www.alanzucconi.com/2026/08/15/xorshift-generators/
uint16_t gimmerand(uint16_t small, uint16_t big) {
  myrand ^= (myrand << 13);
  myrand ^= (myrand >> 17);
  myrand ^= (myrand << 5);

  uint16_t range = big - small + 1;
  return small + (uint16_t)(myrand % range);
}

//  generate lower/upper boundaries for the Slow channel (PA0)
void getnewslow(uint8_t base) {
  slowstart = gimmerand(waves[base],     waves[base + 1]);
  slowend   = gimmerand(waves[base + 2], waves[base + 3]);
}

//  generate lower/upper boundaries and random step speed for Medium channel (PA5)
void getnewmed(uint8_t base) {
  uint8_t idx = base + 4;
  medstart = gimmerand(waves[idx],     waves[idx + 1]);
  medend   = gimmerand(waves[idx + 2], waves[idx + 3]);
  medstep  = gimmerand(2, 5); // Step speed: 2 to 5 units per iteration
}

//  generate lower/upper boundaries and random step speed for Fast channel (PA4)
void getnewfast(uint8_t base) {
  uint8_t idx = base + 8;
  faststart = gimmerand(waves[idx],     waves[idx + 1]);
  fastend   = gimmerand(waves[idx + 2], waves[idx + 3]);
  faster    = gimmerand(2, 6); // Step speed: 2 to 6 units per iteration
}

// Interrupt Service Routine: see datasheet
void Interrupt(void) {
  __disgint();       // Disable global interrupts during ISR execution
  INTEN = 0;          // Turn off all interrupt sources
  INTRQ = 0;          // Clear pending interrupt flags
  sunshine = false;  // Set state to night mode
  __engint();        // Re-enable global interrupts
}

//  Checks the solar input pin (PA3) against internal comparator voltage.
//  Also reseeds the randomiser using hardware timer 2 (TM2CT)
bool checksolar(void) {

  // Determine the comparator threshold. 
  // If it's daytime, use baseline sensitivity. If night time, add hysteresis 
  // to raise the threshold and prevent light flickering/chattering.
  uint8_t current_gpcs;
  
  if (sunshine) {
      // Day: Base sensitivity, masked to 4 bits (0-15) for the GPCS register
      current_gpcs = Sensitivity & 0x0F;
  } else {
      // Night: Add hysteresis buffer, masked to stay within valid 4-bit range
      current_gpcs = (Sensitivity + Hysteresis) & 0x0F; 
  }

  // Re-establish solar input comparator routing
  GPCS = current_gpcs;
  GPCC = 0b10010000;

  sunshine = (GPCC & 0b01000000) != 0; // GPCC Bit 6 (COMP_OUT) is '1' when Solar > Int. ref.

  // Harvest live hardware noise from Timer 2 and the Comparator to "salt" the PRNG.
  // This mixes real-world timing jitter into our random sequence.
  uint32_t timer_noise_high = (uint32_t)TM2CT << 24; // Place timer bits in the top byte
  uint32_t timer_noise_mid  = (uint32_t)TM2CT << 8;  // Place timer bits in the middle byte
  uint32_t comp_status      = (uint32_t)GPCC;        // Grab comparator flags for the bottom byte

  // XOR (mix) all the hardware noise directly into our 32-bit random seed
  myrand ^= (timer_noise_high | timer_noise_mid | comp_status);

  if (sunshine) {
    // Daylight detected: Set GPCS to base Sensitivity threshold.
    GPCS = Sensitivity & 0x0F;
  } else {
    // Nighttime detected: Raise GPCS by Hysteresis gap step.
    GPCS = (Sensitivity + Hysteresis) & 0x0F;
  }

  return sunshine;
}

bool checkbattery(void) {

  // Checks if LIC supercapacitor voltage (VDD) is above cutoff threshold.
  // Uses internal 1.2V Bandgap on COMP IN- (bits 3:1 = 010) and GPCS ladder on COMP IN+.
  // Returns true if VDD > threshold (OK to run), false if VDD <= threshold (Brownout Cutoff).
  //
  // Example values - do your own thing!
  // -------------------------------------------------------------------------
  // GPCS | Ladder Ratio     | Formula Cutoff | Real-World Measured Cutoff
  // -------------------------------------------------------------------------
  //  4   | 13/32 (0.406)    | ~2.95V         | ~3.10V (perhaps too conservative)
  //  5   | 14/32 (0.438)    | ~2.74V         | ~2.88V [RECOMMENDED START]
  //  6   | 15/32 (0.469)    | ~2.56V         | ~2.68V
  //  7   | 16/32 (0.500)    | ~2.40V         | ~2.52V
  //  8   | 17/32 (0.531)    | ~2.26V         | ~2.38V (Deep discharge - too low?)
  // -------------------------------------------------------------------------

  // e.g. GPCS = 5 selects VinternalR = (5 + 9) / 32 * VDD = 14/32 * VDD (0.438 * VDD)
  GPCS = 5;

  // GPCC Register Config:
  // Bit 7   = 1   : Comparator enabled
  // Bit 6   = R/O : Comparator result (COMP_OUT)
  // Bit 5   = 0   : Do not sample through Timer2
  // Bit 4   = 0   : Non-inverted output polarity
  // Bits 3:1 = 010 : Internal 1.20V Bandgap connected to Inverting Input (-)
  // Bit 0   = 0   : VinternalR (GPCS ladder) connected to Non-Inverting Input (+)

  GPCC = 0b10000100;

  // Allow bandgap and internal resistor ladder to settle
  _delay_ms(2);

  // Result = 1 when VinternalR > 1.20V (VDD > ~2.88V for GPCS = 5)
  bool batt_ok = (GPCC & 0b01000000) != 0;

  // Restore comparator to Solar sensing mode (PA3 vs GPCS)
  checksolar();

  return batt_ok;
}

/*
  Main flickering loop: Configures hardware PWM generators and continuously
  ramps the 3 LED channels independently until daylight is detected or battery drains below cutoff.
*/
void candlingon(void) {
  // Set PA0, PA4, PA5 as outputs
  PAC = 0b00110001;
  PA  = 0b00000000;

  // Initialize Hardware PWM Generator 0 (PA4 / LED1 Fast)
  PWMG0DTL = 0x00; PWMG0DTH = 0x00;
  PWMG0CUBL = 0xff; PWMG0CUBH = 0xff;
  PWMG0C = 0b10100111; PWMG0S = 0b00000000;

  // Initialize Hardware PWM Generator 1 (PA5 / LED2 Med)
  PWMG1DTL = 0x00; PWMG1DTH = 0x00;
  PWMG1CUBL = 0xff; PWMG1CUBH = 0xff;
  PWMG1C = 0b10100111; PWMG1S = 0b00000000;

  // Initialize Hardware PWM Generator 2 (PA0 / LED3 Slow)
  PWMG2DTL = 0x00; PWMG2DTH = 0x00;
  PWMG2CUBL = 0xff; PWMG2CUBH = 0xff;
  PWMG2C = 0b10101011; PWMG2S = 0b00000000;

  // retrieve wave limits
  getnewfast(choosearray);
  getnewslow(choosearray);
  getnewmed(choosearray);
  
  slowcounter = slowstart;
  fastcounter = faststart;
  medcounter  = (medstart + medend) >> 1; 

  while (!sunshine) {
    // Ramp slow channel
    if (slowup) {
      slowcounter++;
      if (slowcounter >= slowend) slowup = !slowup;
    } else {
      if (slowcounter > slowstart) {
        slowcounter--;
      } else {
        slowup = !slowup;
        getnewslow(choosearray);
      }
    }

    // Ramp medium channel
    if (medup) {
      medcounter += medstep;
      if (medcounter >= medend) medup = !medup;
    } else {
      if (medcounter > (medstart + medstep)) {
        medcounter -= medstep;
      } else {
        medcounter = medstart;
        medup = !medup;
        getnewmed(choosearray);
      }
    }

    // Ramp fast channel
    if (fastup) {
      fastcounter += faster;
      if (fastcounter >= fastend) fastup = !fastup;
    } else {
      if (fastcounter > (faststart + faster)) {
        fastcounter -= faster;
      } else {
        fastcounter = faststart;
        fastup = !fastup;
        getnewfast(choosearray);
      }
    }

    mydelay(flickdelay + faster);
    delaycounter--;

    // Check for Sputter / Normal / Calm
    if (delaycounter == 0) {
      delaycounter = gimmerand(1, 100);
      
      // Dynamically pick a threshold for Calm (70% to 85%) and Sputter (5% to 20%)
      uint8_t dynamic_calm    = gimmerand(70, 85);
      uint8_t dynamic_sputter = gimmerand(5, 20);

      if (delaycounter > dynamic_calm) {
        flickdelay = flickdelaycalm;
        choosearray = 24; // Row 6 (Calm Mode)
        delaycounter = 100 - delaycounter;
      } else if (delaycounter > dynamic_sputter) {
        flickdelay = flickdelaynormal;
        choosearray = 12; // Row 3 (Normal Mode)
      } else {
        flickdelay = flickdelaysputter;
        choosearray = 0;  // Row 0 (Sputter Mode)
      }
      
      // Perform solar check and salt PRNG
      sunshine = checksolar();

      // If still nighttime, perform low-voltage health check
      if (!sunshine && !checkbattery()) {
        break; // Break out of flicker loop if supercap voltage drops below cutoff
      }

      delaycounter = delaycounter * delaydelay;
    }

    // Update hardware PWM duty cycle registers
    PWMG2DTL = slowcounter & 255;
    PWMG2DTH = slowcounter;
    PWMG0DTL = fastcounter & 255;
    PWMG0DTH = fastcounter;
    PWMG1DTL = medcounter & 255;
    PWMG1DTH = medcounter; 
  }
}

/*
  Shuts off PWM generators and forces all LED pins LOW during daylight or low-power transitions
*/
void candlingoff(void) {
  _delay_ms(100);
  PWMG2DTL = 0; PWMG2DTH = 0;
  PWMG0DTL = 0; PWMG0DTH = 0;
  PWMG1DTL = 0; PWMG1DTH = 0;
  PWMG0C = 0b00100001; // Disable PWM output
  PWMG1C = 0b00100001;
  PWMG2C = 0b00100001;
  _delay_ms(100);
}

/*
  Puts MCU into deep sleep mode (__stopsys) while waiting for nightfall.
  Disables internal pull-ups on PA3 to eliminate supercapacitor leakage.
  Configures low-power mode on comparator to minimize quiescent current draw.
*/
void sleepnow(void) {
  __disgint();                         // Disable global interrupts before sleep setup
  MISC |= MISC_FAST_WAKEUP_ENABLE;     // Enable fast wake-up mode
  
  PAC = 0;                             // Set all pins as inputs
  PA  = 0;                             // Drive data outputs LOW
  PAPH = 0xF7;                         // Exclude PA3 (bit 3) from internal pull-ups to stop leakage
  PBDIER = 0;                          // Disable Port B digital inputs

  INTEN = 0b00010000;                  // Enable comparator interrupt wake-up

  if (checksolar()) {
    GPCC = 0b10000000;                 // Bit 4 = 0 sets low-power mode for comparator during sleep
    INTRQ = 0;                         // Clear pending flags RIGHT before sleeping!
    __engint();                        // Re-enable interrupts
    __stopsys();                       // Halt core clock & enter ultra-low-power sleep
  }
}

/*
  Main entry point: Configures comparator and handles main operating loop.
*/
void main(void) {
  GPCS = Sensitivity & 0x0F;   // Set comparator threshold
  GPCC = 0b10010000;          // Enable comparator hardware

  _delay_ms(100); 

  while (1) {
    if (!sunshine) {
      if (checkbattery()) {
        candlingon();         // Nighttime & Power OK: Run flicker
      } else {
        candlingoff();        // Low voltage: Shutdown LEDs
        
        // Loop sleep until daylight arrives to charge the supercap
        while (!checksolar()) {
          sleepnow();
        }
      }
    } else {
      candlingoff();          // Daylight: Shut down LEDs and sleep
      sleepnow();    
    }
  }
}

/*
  Auto-generated clock initialization required by SDCC PDK framework
*/
unsigned char STARTUP_FUNCTION(void) {
  AUTO_INIT_SYSCLOCK();
  AUTO_CALIBRATE_SYSCLOCK(TARGET_VDD_MV);
  return 0; 
}
