# Simulation profiles

Profiles for `--mock`. **Every value is made up** to exercise the UI; none of it is a real measurement.

```
./build/LinuxHardwareMonitor --mock mocks/amd-desktop.json
./build/LinuxHardwareMonitor --mock mocks/stress-test.json --mock-speed 10   # scenarios 10x faster
```

A bar at the top of the window always says that the hardware data is simulated (processes, services and startup
apps stay real). An invalid profile aborts with a message and exit code 2; the app never falls back to real data
silently.

| Profile | Hardware |
|---------|----------|
| `amd-desktop.json` | Ryzen 9 7950X (16C/32T), Radeon RX 7900 XTX, NVMe + HDD, 2.5 GbE, 3 fans |
| `amd-laptop.json` | Ryzen 7 7840HS, Radeon 780M (no fan), NVMe, Wi-Fi, battery |
| `intel-desktop.json` | Core i9-14900K (24C/32T), Arc A770, NVMe, GbE |
| `nvidia-laptop.json` | Core i7-12700H + RTX 3060 Laptop, NVMe, Wi-Fi, charging battery |
| `hybrid-amd-nvidia-laptop.json` | Ryzen 9 7940HS with Radeon 780M iGPU **and** RTX 4060 (two GPUs, two vendors), Wi-Fi, battery |
| `intel-amd-desktop.json` | Core i5-13600K with UHD 770 iGPU **and** Radeon RX 6700 XT |
| `minimal-system.json` | 2 cores, no GPU, no fans, one SATA disk, 100 Mbit Ethernet |
| `stress-test.json` | the AMD desktop plus scenarios (below) |

## Format

```jsonc
{
  "name": "AMD Test System",           // shown in the simulation bar
  "seed": 7,                           // same seed = same sequence of values
  "cpu": {
    "vendor": "AuthenticAMD",          // AuthenticAMD -> "Tctl", otherwise "Package id 0"
    "model": "AMD Ryzen 9 7950X", "cores": 16, "threads": 32,
    "base_mhz": 4500, "max_mhz": 5700, "l1_kb": 1024, "l2_kb": 16384, "l3_kb": 65536, "power_limit_w": 170,
    "sensors": {                       // or put these keys directly under "cpu"
      "temperature":   { "base": 65, "min": 35, "max": 95, "variation": 8 },
      "usage":         { "base": 25, "min": 0,  "max": 100 },
      "frequency_mhz": { "base": 4200, "min": 500, "max": 5700 }
    }
  },
  "memory":   { "total_gb": 64, "swap_gb": 8, "usage": { "base": 40, "min": 15, "max": 85 } },
  "gpu":      { "vendor": "AMD", "model": "...", "vram_mb": 24576, "has_fan": true,
                "sensors": { "temperature": {...}, "usage": {...}, "power_w": {...}, "clock_mhz": {...},
                             "vram_usage": {...}, "fan_percent": {...} } },       // "gpus": [...] for several
  "disks":    [ { "name": "nvme0n1", "model": "...", "size_gb": 2000, "used_fraction": 0.45,
                  "temperature": {...}, "read_mbps": {...}, "write_mbps": {...}, "busy": {...} } ],
  "networks": [ { "name": "wlp1s0", "kind": "Wi-Fi", "ssid": "...", "speed_mbps": 1200,
                  "down_mbps": {...}, "up_mbps": {...}, "signal_dbm": {...} } ],   // kind: Ethernet | Wi-Fi | Virtual (VPN/tunnel)
  "fans":     [ { "name": "CPU Fan", "rpm": { "base": 1400, "min": 500, "max": 2400 } } ],
  "battery":  { "design_wh": 75, "full_wh": 68, "cycles": 112, "status": "Discharging", "charge": {...}, "power_w": {...} },
  "scenarios": [ ... ]
}
```

Which GPU sensors exist depends on `vendor`, mirroring the real drivers: **NVIDIA** reports everything (fan in %), **AMD**
has no encoder/memory-controller/limit sensors and reports the fan in RPM, **Intel** (iGPU) reports only clocks and power -
no load, temperature or VRAM, exactly what sysfs offers without extra privileges. Use `"gpus": [...]` for several cards
and `"vram_mb": 0` for a GPU without dedicated memory.

Each signal is a mean-reverting random walk around `base`, clamped to `[min, max]`; `variation` is the typical step.
Omitted signals use sensible defaults; omitted sections (no `gpu`, no `fans`, no `battery`) simply do not exist.

## Scenarios

Times are seconds of simulated time (`--mock-speed` scales them).

| `type` | `target` | Effect |
|--------|----------|--------|
| `overheat` | `cpu`, `gpuN` | temperature ramps to `peak` over a third of `duration_s`, holds, ramps back (may exceed the normal `max`) |
| `gpu_loss` | `gpuN` | every sensor of that GPU disappears, as if it fell off the bus, then returns |
| `sensor_dropout` | `cpu`, `gpuN`, `fan:<name>`, `disk:<name>` | that one sensor becomes unavailable (N/A) |
| `fan_change` | `fan:<name>` | the fan jumps to `rpm` for the duration |

All scenarios take `start_s` and `duration_s`.

## Triggering scenarios from the window

In simulation mode the bar at the top has a **Scenarios** button. It lists what makes sense for the profile's hardware
(overheat the CPU or a GPU, disconnect a GPU, lose a temperature sensor, stop or spin up a fan, ...). A triggered
scenario starts immediately, lasts 20-30 seconds and then everything returns to normal; "Cancel all triggered
scenarios" ends them early. Scenarios from the profile file keep running independently.

## Fixture mode

To test the real parsers on prepared directory trees instead:

```
./build/LinuxHardwareMonitor --hwmon-root tests/fixtures/amd/hwmon
```

Also available: `--proc-root`, `--cpu-root`, `--power-supply-root`, `--drm-root`. Fixture mode skips the NVIDIA driver and shows a
"Test data" bar. `--mock` and the root options cannot be combined. A fixture checks discovery and parsing of the
Linux hwmon files, not the behaviour of the real AMD/Intel drivers.
