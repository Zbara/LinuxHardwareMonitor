# Linux Hardware Monitor

A native Qt 6 / QML system monitor for Linux that combines a hardware sensor viewer with a Task Manager–style
interface. No Electron, no web view, no root: everything comes from `/sys`, `/proc`, D-Bus and (optionally) NVIDIA NVML.

## Features

- **Performance** – CPU (overall and per-core load, frequency, temperature, caches, topology), memory
  (composition, swap, slab), every disk (activity, throughput, usage, serial/firmware, since-boot totals),
  every network adapter (throughput, totals, packets, driver, IP addresses; Wi-Fi SSID/channel/security through
  NetworkManager), GPUs of any mix of vendors - NVIDIA (load, VRAM, clocks, power, encoder/decoder, driver/CUDA/PCI IDs), AMD (load, VRAM, temperature, fan, power, clocks) and Intel (clocks, power) and the laptop battery
  (charge, health, power, time remaining).
- **Processes** – applications grouped by name with expandable children, heat-map columns (CPU, memory, disk),
  sorting, search, End task / Force kill, Suspend / Resume, process priority (nice), processor affinity, Properties dialog.
- **Services** – systemd services over D-Bus: filter, search, start / stop / restart (asks for your password through polkit).
- **Startup apps** – XDG autostart entries with enable/disable (system entries are overridden in your own
  `~/.config/autostart`, never edited in `/etc`).
- **Sensors / Overview** – every hwmon sensor with min/max and history (search by name, filter by type, star your favourites; click a column header to sort inside each device group; click a sensor for a large chart with min/max/average and a per-sensor CSV export), plus a summary dashboard.
- **System tray** – optional icon with the program's logo (or, as a setting, the CPU temperature on a blue / amber / red tile) and a load/RAM tooltip; closing the window can keep the app running there (needs a desktop with tray support, e.g. the AppIndicator extension on GNOME).
- **History across restarts** – the last ~15 minutes of every sensor are saved on exit (and every 5 minutes) to `~/.local/share/LinuxHardwareMonitor/history.bin`; live charts start fresh after a restart, while the sensor detail window (click a sensor on the Sensors page) shows the restored history with the downtime as a gap. Not used in simulation or fixture mode.
- **Overheat notifications** – optional desktop notification (freedesktop `Notify` over D-Bus) when a temperature sensor reaches a threshold you choose; off by default. A temperature sensor can have a threshold of its own (in its detail window), which replaces the global one for that sensor.
- **Keyboard** – `Ctrl+1`…`Ctrl+7` switch pages, `Ctrl+F` focuses the search field of the Processes, Services and Sensors pages.
- **Temperature unit** – Celsius or Fahrenheit (Settings); only what is shown changes, thresholds and data stay in Celsius.
- **System information** – Settings > About copies a short description of the hardware, versions and renderer for a bug report. It never contains serial numbers, MAC/IP addresses or Wi-Fi names.
- **Settings** – update interval, history length, dark/light theme, language, start with the system, CSV export of the history.
- **Languages** – English, Русский, Українська, Deutsch, Español (switchable at runtime, follows the system by default).

Anything that cannot be read without privileges (DIMM details, SMART data, RAPL energy counters) is shown as
"N/A" or left out; the app never invents values.

## Dependencies (Ubuntu / Debian)

```
sudo apt install build-essential cmake ninja-build python3 \
    qt6-base-dev qt6-declarative-dev \
    qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
    qml6-module-qtquick-window qml6-module-qtquick-templates
```

Optional at runtime: the proprietary NVIDIA driver (`libnvidia-ml.so.1`) for GPU metrics, NetworkManager for Wi-Fi
details, systemd for the Services page. Without them the corresponding parts are simply empty.

## Build, test, run

```
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./build/LinuxHardwareMonitor
```

`LinuxHardwareMonitor --help` lists every option. Useful flags for screenshots and smoke tests: `--page <n>`, `--resource <n>`, `--size <w> <h>`,
`--screenshot <file.png>` (e.g. `QT_QPA_PLATFORM=offscreen ./build/LinuxHardwareMonitor --page 0 --screenshot out.png`).

## Resource usage

Pages are created only while they are visible, static CPU facts are read once, and Qt Quick uses the software
renderer by default (about 150-190 MB RSS and 2-5 % of one core on a 32-thread machine, versus ~330 MB with OpenGL).
Set `QT_QUICK_BACKEND=rhi` (or `QSG_RHI_BACKEND=opengl`) to use the GPU renderer instead.

## Simulation and test data

Try other hardware without owning it: `./build/LinuxHardwareMonitor --mock mocks/amd-desktop.json` shows a simulated
Ryzen + Radeon system (profiles for AMD, Intel, NVIDIA, a minimal box and a stress test with overheating, GPU loss,
sensor dropout and fan changes). `--hwmon-root <dir>` runs the real hwmon parser on prepared files
(`tests/fixtures/`). Real hardware is always the default; simulation is opt-in and always announced by a bar at the top
of the window. Details and the profile format: `mocks/README.md`.

## Install and package

```
cmake --install build --prefix /usr/local          # binary, .desktop entry and icon
cd build && cpack -G DEB                           # linux-hardware-monitor_<version>_amd64.deb
sudo apt install ./linux-hardware-monitor_0.1_amd64.deb
```

The default build type is `Release`. `packaging/` holds the desktop entry (localized) and the SVG icon. The maintainer
address of the package is set in `CMakeLists.txt` (`CPACK_PACKAGE_CONTACT`).

## Project layout

| Path | Contents |
|------|----------|
| `src/core` | `SensorManager` (one polling thread, history, min/max), process scanner/monitor/control, systemd and NetworkManager clients, hardware facts, localization |
| `src/providers` | `ISensorProvider` implementations: hwmon, NVIDIA NVML, system (CPU/RAM), I/O (network, disks), battery |
| `src/models` | Qt list/proxy models for sensors, processes, services, startup entries |
| `src/ui` | `AppController` – the single object exposed to QML |
| `qml` | Pages, reusable components (charts, tiles, cards) and the theme |
| `translations` | One JSON file per language (source-text keyed) |
| `tests` | Qt Test suites; hardware access is injectable, so they run against fake `/sys` and `/proc` trees |
| `tools` | Translation table and checker (`tools/translations_table.py`, `tools/check_translations.py`) |

## Adding or changing UI text

Wrap the string in `qsTr()` (QML) or `tr()` (C++), add a row with all languages to `tools/translations_table.py`, then run
`python3 tools/translations_table.py`. The `translations_complete` test fails when a string is missing, stale or has
mismatching `%1` placeholders.
