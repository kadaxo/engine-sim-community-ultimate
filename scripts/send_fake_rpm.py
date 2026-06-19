#!/usr/bin/env python3
import argparse
import math
import random
import socket
import time


def rpm_at(
    t: float,
    duration: float,
    idle: float,
    peak: float,
    idle_hold: float,
    stop_ramp: float,
    stop_hold: float,
    rng: random.Random,
) -> float:
    stop_start = max(0.0, duration - stop_hold - stop_ramp)
    if t >= duration - stop_hold:
        return 0.0

    active_duration = max(0.001, stop_start - idle_hold)
    active_t = min(max(t - idle_hold, 0.0), active_duration)
    phase = active_t / active_duration
    # Smooth accelerate then decelerate after the initial idle hold.
    base = idle + (peak - idle) * math.sin(math.pi * phase)

    idle_weight = 1.0 - min(max((base - idle) / 2000.0, 0.0), 1.0)
    slow_wobble = math.sin(t * 2.6) * 95.0 + math.sin(t * 7.3) * 35.0
    jitter = rng.uniform(-55.0, 55.0)

    rpm = max(0.0, base + idle_weight * (slow_wobble + jitter))
    if t >= stop_start:
        stop_phase = min(max((t - stop_start) / stop_ramp, 0.0), 1.0)
        smooth = stop_phase * stop_phase * (3.0 - 2.0 * stop_phase)
        rpm *= 1.0 - smooth

    return rpm


def main() -> None:
    parser = argparse.ArgumentParser(description="Send fake RPM values to engine-sim over UDP.")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--duration", type=float, default=20.0)
    parser.add_argument("--idle-hold", type=float, default=5.0)
    parser.add_argument("--stop-ramp", type=float, default=2.0)
    parser.add_argument("--stop-hold", type=float, default=3.0)
    parser.add_argument("--rate", type=float, default=120.0)
    parser.add_argument("--idle", type=float, default=900.0)
    parser.add_argument("--peak", type=float, default=6500.0)
    parser.add_argument("--seed", type=int, default=None)
    parser.add_argument("--log", default=None)
    args = parser.parse_args()

    interval = 1.0 / args.rate
    address = (args.host, args.port)
    rng = random.Random(args.seed)

    log_file = open(args.log, "w", encoding="utf-8") if args.log else None
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            start = time.perf_counter()
            next_send = start
            while True:
                now = time.perf_counter()
                elapsed = now - start
                if elapsed > args.duration:
                    break

                rpm = rpm_at(
                    elapsed,
                    args.duration,
                    args.idle,
                    args.peak,
                    args.idle_hold,
                    args.stop_ramp,
                    args.stop_hold,
                    rng,
                )
                sock.sendto(f"{rpm:.1f}\n".encode("ascii"), address)
                if log_file:
                    log_file.write(f"{elapsed:.6f},{rpm:.1f}\n")

                next_send += interval
                sleep_for = next_send - time.perf_counter()
                if sleep_for > 0:
                    time.sleep(sleep_for)

            sock.sendto(b"0.0\n", address)
            if log_file:
                log_file.write(f"{args.duration:.6f},0.0\n")
    finally:
        if log_file:
            log_file.close()


if __name__ == "__main__":
    main()
