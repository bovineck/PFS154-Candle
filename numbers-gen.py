import sys
import argparse
import struct
import wave
import numpy as np
import matplotlib as mpl
import matplotlib.pyplot as plt
import matplotlib.animation as animation

# Try importing sounddevice for live audio playback
try:
    import sounddevice as sd
    HAS_SOUNDDEVICE = True
except ImportError:
    HAS_SOUNDDEVICE = False

# Fix Matplotlib rendering overflow for multi-million point path drawing
mpl.rcParams['agg.path.chunksize'] = 20000

# =============================================================================
# Command-Line Argument Parser
# =============================================================================
def parse_arguments():
    parser = argparse.ArgumentParser(
        description="PFS154 Supercapacitor Candle Simulation with Time-Synced Audio-Visual Dashboard",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )

    parser.add_argument(
        "-s", "--samples", 
        type=int, 
        default=None, 
        help="Total flicker steps to simulate (Auto-calculated from target time if omitted)"
    )
    parser.add_argument(
        "-t", "--start", 
        type=float, 
        default=1000.0, 
        help="Start time for plot capture in seconds (e.g., 1000 = 16.7 min)"
    )
    parser.add_argument(
        "-w", "--window", 
        type=float, 
        default=120.0, 
        help="Window duration to plot in seconds (e.g., 120 = 2 min)"
    )
    parser.add_argument(
        "-a", "--avg-window", 
        type=int, 
        default=200, 
        help="Moving average window size (steps)"
    )
    parser.add_argument(
        "--seed", 
        type=int, 
        default=29012026, 
        help="32-bit PRNG initial seed"
    )
    parser.add_argument(
        "-o", "--output", 
        type=str, 
        default=None, 
        help="Output PNG filename snapshot"
    )
    parser.add_argument(
        "--no-audio", 
        action="store_true", 
        help="Skip generating WAV audio export"
    )

    if len(sys.argv) == 1:
        parser.print_help()
        sys.exit(0)

    return parser.parse_args()


# =============================================================================
# Bare-Metal C-Code Emulation
# =============================================================================
def run_simulation(args):
    # Switch to dark mode for a workbench oscilloscope feel
    plt.style.use('dark_background')
    
    # Hide the GUI window toolbar to prevent directory confusion on manual saves
    plt.rcParams['toolbar'] = 'none'

    myrand = np.uint32(args.seed)

    slowcounter = 0
    medcounter = 0
    fastcounter = 0

    slowstart = 0
    slowend = 0
    medstart = 0
    medend = 0
    faststart = 0
    fastend = 0

    faster = 0
    medstep = 1

    flickdelay = 55
    flickdelaysputter = 14
    flickdelaynormal = 55
    flickdelaycalm = 120

    choosearray = 12

    delaycounter = 50
    delaydelay = 20

    slowup = True
    medup = True
    fastup = True

    tm2ct_sim = 0

    waves = [
        # Row 0-2: Sputter (slow, med, fast)
        4, 6, 40, 50,
        6, 8, 50, 80,
        8, 10, 110, 130,
        # Row 3-5: Normal (slow, med, fast)
        15, 25, 60, 100,
        10, 25, 110, 140,
        20, 25, 100, 120,
        # Row 6-8: Calm (slow, med, fast)
        40, 60, 100, 140,
        50, 70, 120, 160,
        70, 80, 140, 180,
    ]

    def gimmerand(small, big):
        nonlocal myrand
        myrand = (myrand ^ ((myrand << np.uint32(13)) & np.uint32(0xFFFFFFFF))) & np.uint32(0xFFFFFFFF)
        myrand = (myrand ^ (myrand >> np.uint32(17))) & np.uint32(0xFFFFFFFF)
        myrand = (myrand ^ ((myrand << np.uint32(5)) & np.uint32(0xFFFFFFFF))) & np.uint32(0xFFFFFFFF)

        rng = big - small + 1
        return small + int(myrand % rng)

    def getnewslow(base):
        nonlocal slowstart, slowend
        slowstart = gimmerand(waves[base], waves[base + 1])
        slowend = gimmerand(waves[base + 2], waves[base + 3])

    def getnewmed(base):
        nonlocal medstart, medend, medstep
        idx = base + 4
        medstart = gimmerand(waves[idx], waves[idx + 1])
        medend = gimmerand(waves[idx + 2], waves[idx + 3])
        medstep = gimmerand(2, 5)

    def getnewfast(base):
        nonlocal faststart, fastend, faster
        idx = base + 8
        faststart = gimmerand(waves[idx], waves[idx + 1])
        fastend = gimmerand(waves[idx + 2], waves[idx + 3])
        faster = gimmerand(2, 6)

    def checksolar_sim():
        nonlocal myrand, tm2ct_sim
        tm2ct_sim = (tm2ct_sim + 17) & 0xFF
        gpcc_sim = 0x10
        salt = (((tm2ct_sim << 8) | gpcc_sim) | (tm2ct_sim << 24)) & 0xFFFFFFFF
        myrand ^= np.uint32(salt)

    end_time_sec = args.start + args.window

    # Auto-calculate sample limit if omitted (~35,000 steps per second)
    if args.samples is None:
        max_samples = int((end_time_sec + 5) * 35000)
    else:
        max_samples = args.samples

    # Target slice storage
    sliced_time = []
    sliced_slow = []
    sliced_med = []
    sliced_fast = []

    accumulated_time_us = 0.0

    getnewfast(choosearray)
    getnewslow(choosearray)
    getnewmed(choosearray)

    slowcounter = slowstart
    fastcounter = faststart
    medcounter = (medstart + medend) // 2

    print(f"Simulating up to {max_samples:,} steps...")
    print(f"Plot Target Window: {args.start:.1f}s ({args.start/60:.1f}m) to {end_time_sec:.1f}s ({end_time_sec/60:.1f}m)")

    for i in range(max_samples):
        if slowup:
            slowcounter += 1
            if slowcounter >= slowend:
                slowup = not slowup
        else:
            if slowcounter > slowstart:
                slowcounter -= 1
            else:
                slowup = not slowup
                getnewslow(choosearray)

        if medup:
            medcounter += medstep
            if medcounter >= medend:
                medup = not medup
        else:
            if medcounter > (medstart + medstep):
                medcounter -= medstep
            else:
                medcounter = medstart
                medup = not medup
                getnewmed(choosearray)

        if fastup:
            fastcounter += faster
            if fastcounter >= fastend:
                fastup = not fastup
        else:
            if fastcounter > (faststart + faster):
                fastcounter -= faster
            else:
                fastcounter = faststart
                fastup = not fastup
                getnewfast(choosearray)

        delaycounter -= 1

        if delaycounter <= 0:
            delaycounter = gimmerand(1, 100)
            dynamic_calm = gimmerand(70, 85)
            dynamic_sputter = gimmerand(5, 20)

            if delaycounter > dynamic_calm:
                flickdelay = flickdelaycalm
                choosearray = 24
                delaycounter = 100 - delaycounter
            elif delaycounter > dynamic_sputter:
                flickdelay = flickdelaynormal
                choosearray = 12
            else:
                flickdelay = flickdelaysputter
                choosearray = 0

            checksolar_sim()
            delaycounter = delaycounter * delaydelay

        step_delay_us = flickdelay + faster
        accumulated_time_us += step_delay_us
        current_time_sec = accumulated_time_us / 1_000_000.0

        if args.start <= current_time_sec <= end_time_sec:
            sliced_time.append(current_time_sec)
            sliced_slow.append(slowcounter)
            sliced_med.append(medcounter)
            sliced_fast.append(fastcounter)

        if current_time_sec > end_time_sec:
            print(f"Target window completed at step {i:,} ({current_time_sec:.2f}s total simulated time).")
            break

    if len(sliced_time) == 0:
        print(f"\n[Error] Captured 0 steps in the target window ({args.start}s - {end_time_sec}s).")
        print(f"The simulation only reached {current_time_sec:.2f}s total execution time.")
        print(f"To reach {args.start}s, increase step count `-s` or remove `-s` to let auto-scaling run.\n")
        sys.exit(1)

    print(f"Captured {len(sliced_time):,} data points in target time window.")

    time_sec = np.array(sliced_time)
    ch_slow = np.array(sliced_slow, dtype=np.float32)
    ch_med = np.array(sliced_med, dtype=np.float32)
    ch_fast = np.array(sliced_fast, dtype=np.float32)

    ch_combined = ch_slow + ch_med + ch_fast

    kernel = np.ones(args.avg_window) / args.avg_window
    ch_moving_avg = np.convolve(ch_combined, kernel, mode="same")

    # Time-Synced Animation Configuration (Targeting steady 30 FPS)
    target_fps = 30
    target_frames = int(max(30, args.window * target_fps))
    anim_stride = max(1, len(time_sec) // target_frames)
    
    a_time = time_sec[::anim_stride]
    a_slow = ch_slow[::anim_stride]
    a_med = ch_med[::anim_stride]
    a_fast = ch_fast[::anim_stride]
    a_avg = ch_moving_avg[::anim_stride]
    a_comb = ch_combined[::anim_stride]

    anim_interval = int(1000.0 / target_fps)

    # Prepare Audio Buffers for Export & Live Playback (Clean original logic)
    sample_rate = 22050
    audio_time_grid = np.arange(args.start, end_time_sec, 1.0 / sample_rate)
    
    a_s_arr = np.interp(audio_time_grid, time_sec, ch_slow)
    a_m_arr = np.interp(audio_time_grid, time_sec, ch_med)
    a_f_arr = np.interp(audio_time_grid, time_sec, ch_fast)

    left_audio = (a_s_arr * 100.0) + (a_f_arr * 40.0)
    right_audio = (a_m_arr * 100.0) + (a_f_arr * 40.0)

    max_l = np.max(np.abs(left_audio))
    max_r = np.max(np.abs(right_audio))

    left_pcm = np.int16((left_audio / max_l * 32767) if max_l > 0 else left_audio)
    right_pcm = np.int16((right_audio / max_r * 32767) if max_r > 0 else right_audio)

    # Save WAV file
    if not args.no_audio and len(time_sec) > 1:
        wav_filename = "candle_flicker_audio.wav"
        with wave.open(wav_filename, "w") as wav_file:
            wav_file.setnchannels(2)
            wav_file.setsampwidth(2)
            wav_file.setframerate(sample_rate)
            stereo_data_int16 = np.empty((left_pcm.size + right_pcm.size,), dtype=np.int16)
            stereo_data_int16[0::2] = left_pcm
            stereo_data_int16[1::2] = right_pcm
            wav_file.writeframes(stereo_data_int16.tobytes())
        print(f"Saved exact {args.window:.1f}s audio file to '{wav_filename}'.")

    # Prepare stereo float buffer for live playback via sounddevice
    stereo_float = None
    if not args.no_audio and HAS_SOUNDDEVICE and len(time_sec) > 1:
        stereo_float = np.empty((left_pcm.size, 2), dtype=np.float32)
        stereo_float[:, 0] = left_pcm.astype(np.float32) / 32767.0
        stereo_float[:, 1] = right_pcm.astype(np.float32) / 32767.0

    # Build compact 3-row figure layout
    fig, (ax_leds, ax1, ax2) = plt.subplots(
        3, 1, figsize=(14, 10), sharex=False, 
        gridspec_kw={"height_ratios": [0.6, 2, 1], "hspace": 0.2}
    )

    ax2.sharex(ax1)

    # Configure LED Panel Top Axis with square limits (0 to 3) for perfect circles
    ax_leds.set_xlim(0, 3)
    ax_leds.set_ylim(0, 3)
    ax_leds.set_aspect('equal')
    ax_leds.axis('off')
    ax_leds.set_title(f"Hardware LED Simulation Panel (Window: {args.window:.1f}s)", fontsize=12, fontweight="bold", pad=8)

    led_color = "#ffcc00"
    led_slow = plt.Circle((0.9, 1.5), 0.90, color=led_color, alpha=0.15, zorder=3)
    led_med  = plt.Circle((1.5, 1.5), 0.90, color=led_color, alpha=0.15, zorder=3)
    led_fast = plt.Circle((2.1, 1.5), 0.90, color=led_color, alpha=0.15, zorder=3)

    ax_leds.add_patch(led_slow)
    ax_leds.add_patch(led_med)
    ax_leds.add_patch(led_fast)

    # Set up static axis ranges for the dark oscilloscope plots
    ax1.set_xlim(a_time[0], a_time[-1])
    ax1.set_ylim(np.min([a_slow, a_med, a_fast]) - 5, np.max([a_slow, a_med, a_fast]) + 10)
    ax1.set_ylabel("PWM Duty (0-255)", fontsize=10)
    ax1.set_title(f"Live Dark-Theme Oscilloscope: {args.start:.1f}s to {end_time_sec:.1f}s", fontsize=11)
    ax1.grid(True, linestyle="--", alpha=0.3)

    ax2.set_xlim(a_time[0], a_time[-1])
    ax2.set_ylim(np.min(a_comb) - 20, np.max(a_comb) + 20)
    ax2.set_xlabel("Simulation Time (Seconds)", fontsize=10)
    ax2.set_ylabel("Total Sum", fontsize=10)
    ax2.grid(True, linestyle="--", alpha=0.3)

    # Initialize live-drawing graph lines
    line_slow, = ax1.plot([], [], label="Slow Channel (PA0 / LED3)", color="#ff9933", alpha=0.9, linewidth=1.0)
    line_med,  = ax1.plot([], [], label="Med Channel (PA5 / LED2)", color="#ffcc00", alpha=0.9, linewidth=1.0)
    line_fast, = ax1.plot([], [], label="Fast Channel (PA4 / LED1)", color="#ff4444", alpha=0.9, linewidth=1.0)
    ax1.legend(loc="upper right", fontsize=9, framealpha=0.5)

    line_avg,  = ax2.plot([], [], label=f"{args.avg_window}-Step Rolling Average", color="#cc66ff", alpha=0.9, linewidth=1.5)
    line_comb, = ax2.plot([], [], label="Combined Intensity (Sum)", color="#ff7700", alpha=0.6, linewidth=0.7)
    ax2.legend(loc="upper right", fontsize=9, framealpha=0.5)

    # Populate lines temporarily to save the complete static snapshot
    line_slow.set_data(a_time, a_slow)
    line_med.set_data(a_time, a_med)
    line_fast.set_data(a_time, a_fast)
    line_avg.set_data(a_time, a_avg)
    line_comb.set_data(a_time, a_comb)

    out_file = args.output if args.output else f"flicker_slice_{int(args.start)}s_to_{int(end_time_sec)}s.png"
    fig.savefig(out_file, dpi=300)
    print(f"Saved complete static plot snapshot to '{out_file}'.")

    # Reset lines back to empty for the live animation
    line_slow.set_data([], [])
    line_med.set_data([], [])
    line_fast.set_data([], [])
    line_avg.set_data([], [])
    line_comb.set_data([], [])

    # Track audio playback state to trigger it precisely on Frame 0
    audio_played = False

    # Real-time animation update function
    def update_frame(frame_idx):
        nonlocal audio_played
        if frame_idx == 0 and stereo_float is not None and not audio_played:
            sd.play(stereo_float, sample_rate)
            audio_played = True

        idx = frame_idx + 1
        line_slow.set_data(a_time[:idx], a_slow[:idx])
        line_med.set_data(a_time[:idx], a_med[:idx])
        line_fast.set_data(a_time[:idx], a_fast[:idx])
        line_avg.set_data(a_time[:idx], a_avg[:idx])
        line_comb.set_data(a_time[:idx], a_comb[:idx])

        s_val = float(a_slow[frame_idx]) / 255.0
        m_val = float(a_med[frame_idx]) / 255.0
        f_val = float(a_fast[frame_idx]) / 255.0

        led_slow.set_alpha(max(0.15, s_val))
        led_med.set_alpha(max(0.15, m_val))
        led_fast.set_alpha(max(0.15, f_val))

        return led_slow, led_med, led_fast, line_slow, line_med, line_fast, line_avg, line_comb

    ani = animation.FuncAnimation(
        fig, update_frame, frames=len(a_time), interval=anim_interval, blit=False, repeat=False
    )

    print(f"Launching time-synced audio-visual dashboard ({args.window:.1f}s)...")
    plt.show()


if __name__ == "__main__":
    args = parse_arguments()
    run_simulation(args)
