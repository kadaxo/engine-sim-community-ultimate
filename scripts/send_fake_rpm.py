#!/usr/bin/env python3
import argparse
import math
import socket
import time


def rpm_at(t: float, duration: float, idle: float, peak: float) -> float:
    phase = min(max(t / duration, 0.0), 1.0)
    # Smooth accelerate then decelerate over one cycle.
    return idle + (peak - idle) * math.sin(math.pi * phase)


def main() -> None:
    parser = argparse.ArgumentParser(description="Send fake RPM values to engine-sim over UDP.")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--duration", type=float, default=20.0)
    parser.add_argument("--rate", type=float, default=120.0)
    parser.add_argument("--idle", type=float, default=900.0)
    parser.add_argument("--peak", type=float, default=6500.0)
    args = parser.parse_args()

    interval = 1.0 / args.rate
    address = (args.host, args.port)

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        start = time.perf_counter()
        next_send = start
        while True:
            now = time.perf_counter()
            elapsed = now - start
            if elapsed > args.duration:
                break

            rpm = rpm_at(elapsed, args.duration, args.idle, args.peak)
            sock.sendto(f"{rpm:.1f}\n".encode("ascii"), address)

            next_send += interval
            sleep_for = next_send - time.perf_counter()
            if sleep_for > 0:
                time.sleep(sleep_for)

        sock.sendto(f"{args.idle:.1f}\n".encode("ascii"), address)


if __name__ == "__main__":
    main()
