Name:           linux-hardware-monitor
Version:        0.2.2
Release:        1%{?dist}
Summary:        Hardware sensors, performance graphs and a task manager
License:        GPL-3.0-or-later
URL:            https://github.com/Zbara/LinuxHardwareMonitor
Source0:        %{url}/releases/download/v%{version}/%{name}-%{version}.tar.gz

BuildRequires:  gcc-c++
BuildRequires:  cmake >= 3.21
BuildRequires:  ninja-build
BuildRequires:  gzip
BuildRequires:  python3
BuildRequires:  cmake(Qt6Core)
BuildRequires:  cmake(Qt6Gui)
BuildRequires:  cmake(Qt6Widgets)
BuildRequires:  cmake(Qt6DBus)
BuildRequires:  cmake(Qt6Network)
BuildRequires:  cmake(Qt6Qml)
BuildRequires:  cmake(Qt6Quick)
BuildRequires:  cmake(Qt6QuickControls2)
BuildRequires:  cmake(Qt6Test)
BuildRequires:  desktop-file-utils
BuildRequires:  appstream
# For the tests in %%check
BuildRequires:  dbus-daemon
BuildRequires:  dbus-tools
BuildRequires:  dejavu-sans-fonts

# The QML modules of the window (QtQuick, Controls, Layouts, Window, Templates, WorkerScript) are in this package.
Requires:       qt6-qtdeclarative%{?_isa}
# Wi-Fi details come from NetworkManager; disk health from udisks2; RAM module details (only on a click) need
# polkit's pkexec and dmidecode.
Recommends:     NetworkManager
Suggests:       udisks2
Suggests:       polkit
Suggests:       dmidecode

%description
A native Qt 6 system monitor for Linux. It shows temperatures, fans, loads and
clocks of the CPU, memory, disks, network adapters, graphics cards and the
battery, next to a process manager, background services and startup
applications.

It runs without root rights and shows only what the system exposes; a value
that cannot be read is displayed as N/A, never invented.

%prep
%autosetup

%build
# The Debian-only files (copyright in DEP-5 form, changelog, lintian overrides) are not installed. A source archive
# has no git, so the build line of --version is given here.
%cmake -G Ninja \
    -DHWMON_DEBIAN_PACKAGING=OFF \
    -DHWMON_GIT_DESCRIBE=v%{version}
%cmake_build

%install
%cmake_install

%check
desktop-file-validate %{buildroot}%{_datadir}/applications/linux-hardware-monitor.desktop
appstreamcli validate --no-net %{buildroot}%{_metainfodir}/io.github.zbara.LinuxHardwareMonitor.metainfo.xml
# The tests start the real program (without a screen) for every page and need a session bus.
export QT_QPA_PLATFORM=offscreen
dbus-run-session -- ctest --test-dir %{__cmake_builddir} --output-on-failure --timeout 120

%files
%license LICENSE
%doc README.md
%{_bindir}/LinuxHardwareMonitor
%{_datadir}/applications/linux-hardware-monitor.desktop
%{_datadir}/icons/hicolor/scalable/apps/linux-hardware-monitor.svg
%{_metainfodir}/io.github.zbara.LinuxHardwareMonitor.metainfo.xml
%{_mandir}/man1/LinuxHardwareMonitor.1*
%{_datadir}/linux-hardware-monitor/

%changelog
* Sun Oct 25 2026 Zbara <zbara.dev@gmail.com> - 0.2.2-1
- Packages for Fedora (RPM) and Arch Linux (PKGBUILD), built and tested in CI.
- The source archive is published with every release.

* Sun Oct 25 2026 Zbara <zbara.dev@gmail.com> - 0.2.1-1
- Packages for Ubuntu 22.04 and 24.04; the program builds with Qt 6.2 and newer.
- The packages know the commit they were built from (--version, About page).
- Builds without warnings on the newest toolchains (Fedora 44, Arch Linux).
- A test starts the application for every page and fails on a QML error.
- First RPM package.
