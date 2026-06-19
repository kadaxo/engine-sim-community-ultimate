# RPM Sync Notes

This branch adds a low-latency local RPM input path for driving engine-sim from a Python-generated RPM signal.

## Branch

```text
rpm-sync
```

This branch was created on top of `audio-fix-5-commit-silence-fill`.

## Commits

### `146998a Add UDP RPM sync input`

Adds the main RPM sync path.

- Adds `RpmSync`, a non-blocking UDP receiver bound to `127.0.0.1:8765`.
- Accepts plain text RPM values, one number per UDP packet.
- Keeps only the newest RPM value to minimize latency.
- Treats a target as active for 250 ms after the last packet.
- Applies active targets through existing dyno hold control.
- Sets throttle/speed control to full while external RPM sync is active.
- Adds `scripts/send_fake_rpm.py` to generate a synthetic 20-second RPM sweep.

Default receiver input format:

```text
2500.0\n
```

Default Python sender:

```sh
./scripts/send_fake_rpm.py
```

### `fd1faae Allow low idle RPM sync targets`

Fixes low-RPM behavior and improves the synthetic RPM source.

- Allows external RPM sync targets below the dyno hold UI minimum.
- Fixes the shared dyno clamp so RPM sync can target `0 rpm` through redline.
- Prevents received idle targets such as `750-950 rpm` from being raised to `1000 rpm`.
- Changes the fake RPM sender to hold unstable idle for 5 seconds at the start.
- Uses a 10-second acceleration/deceleration section before the stop ramp.
- Adds larger idle wobble so the idle region is visibly/audibly unstable.
- Adds `--log` to record sent RPM values as CSV.
- Smoothly ramps to `0 rpm` from 15s to 17s, then holds `0 rpm` until 20s.

Example with logging:

```sh
./scripts/send_fake_rpm.py --log /tmp/rpm-sync-sent.csv
```

## Default Synthetic RPM Shape

The default `send_fake_rpm.py` profile is 20 seconds total:

- `0-5s`: unstable idle around `900 rpm`
- `5-10s`: smooth acceleration toward `6500 rpm`
- `10-15s`: smooth deceleration back to idle
- `15-17s`: smooth stop ramp from idle to `0 rpm`
- `17-20s`: hold `0 rpm`

Idle wobble intentionally sends values below `1000 rpm` so the app can be checked for low-idle tracking. The final 3 seconds hold `0 rpm` to test shutdown/zero-speed behavior.

## Zero RPM Test

The app was tested with repeated `0 rpm` UDP packets:

```sh
python3 - <<'PY'
import socket
import time
with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
    end = time.perf_counter() + 2.0
    while time.perf_counter() < end:
        sock.sendto(b'0\n', ('127.0.0.1', 8765))
        time.sleep(1 / 120)
PY
```

Result: the app stayed running after the zero-RPM input.

## Notes

The current implementation uses dyno hold as the actuator. It follows the external RPM target by setting the dyno target speed rather than directly overwriting engine angular velocity. This keeps the engine simulation more natural than hard-setting RPM every frame, while still keeping latency low because the input path is UDP and non-blocking.
