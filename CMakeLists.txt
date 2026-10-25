cmake_minimum_required(VERSION 3.21)
project(LinuxHardwareMonitor VERSION 0.1 LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE) # optimised unless told otherwise
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets DBus Network Qml Quick QuickControls2 Test)
qt_standard_project_setup(REQUIRES 6.5)

# Everything except the GUI entry point, shared by the app and the tests.
add_library(hwmon_core STATIC
    src/core/SensorTypes.h
    src/core/SensorHistory.h
    src/core/SensorHistory.cpp
    src/core/OverheatMonitor.h
    src/core/OverheatMonitor.cpp
    src/core/ProcessControl.h
    src/core/ProcessControl.cpp
    src/core/ProcessScanner.h
    src/core/ProcessScanner.cpp
    src/core/DmiMemory.h
    src/core/DmiMemory.cpp
    src/core/HardwareInfo.h
    src/core/HardwareInfo.cpp
    src/core/WifiInfoMonitor.h
    src/core/WifiInfoMonitor.cpp
    src/core/Localization.h
    src/core/Localization.cpp
    src/core/ServiceManager.h
    src/core/ServiceManager.cpp
    src/core/StartupManager.h
    src/core/StartupManager.cpp
    src/core/Autostart.h
    src/core/Autostart.cpp
    src/core/SystemInfo.h
    src/core/SystemInfo.cpp
    src/core/ProcessMonitor.h
    src/core/ProcessMonitor.cpp
    src/core/CommandLine.h
    src/core/CommandLine.cpp
    src/core/SystemReport.h
    src/core/SystemReport.cpp
    src/core/QuitSignals.h
    src/core/QuitSignals.cpp
    src/core/DiskHealth.h
    src/core/DiskHealth.cpp
    src/core/DiskHealthMonitor.h
    src/core/DiskHealthMonitor.cpp
    src/core/Notifier.h
    src/core/Notifier.cpp
    src/core/SensorManager.h
    src/core/SensorManager.cpp
    src/mock/MockActions.h
    src/mock/MockActions.cpp
    src/mock/MockFacts.h
    src/mock/MockFacts.cpp
    src/mock/MockProfile.h
    src/mock/MockProfile.cpp
    src/mock/MockProvider.h
    src/mock/MockProvider.cpp
    src/providers/ISensorProvider.h
    src/providers/HwmonProvider.h
    src/providers/HwmonProvider.cpp
    src/providers/DrmProvider.h
    src/providers/DrmProvider.cpp
    src/providers/BatteryProvider.h
    src/providers/BatteryProvider.cpp
    src/providers/IoProvider.h
    src/providers/IoProvider.cpp
    src/providers/SystemProvider.h
    src/providers/SystemProvider.cpp
    src/providers/NvidiaProvider.h
    src/providers/NvidiaProvider.cpp
    src/models/ServiceModel.h
    src/models/ServiceModel.cpp
    src/models/StartupModel.h
    src/models/StartupModel.cpp
    src/models/ProcessListModel.h
    src/models/ProcessListModel.cpp
    src/models/SensorListModel.h
    src/models/SensorListModel.cpp
    src/ui/TrayIcon.h
    src/ui/TrayIcon.cpp
    src/ui/AppController.h
    src/ui/AppController.cpp
)
target_include_directories(hwmon_core PUBLIC src)
target_compile_definitions(hwmon_core PUBLIC HWMON_VERSION="${PROJECT_VERSION}")
target_link_libraries(hwmon_core PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::DBus Qt6::Network Qt6::Qml)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_executable(LinuxHardwareMonitor src/main.cpp)
qt_add_qml_module(LinuxHardwareMonitor
    URI HwMonitor
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/pages/DashboardPage.qml
        qml/pages/PerformancePage.qml
        qml/pages/ProcessesPage.qml
        qml/pages/ServicesPage.qml
        qml/pages/StartupPage.qml
        qml/pages/SensorsPage.qml
        qml/pages/SettingsPage.qml
        qml/components/BarCard.qml
        qml/components/ChartCard.qml
        qml/components/SensorDetailDialog.qml
        qml/components/CoreGrid.qml
        qml/components/InfoCard.qml
        qml/components/NavItem.qml
        qml/components/PillTabs.qml
        qml/components/StatTile.qml
        qml/components/PageTitle.qml
        qml/components/Toggle.qml
        qml/components/SensorCard.qml
        qml/components/TemperatureChart.qml
        qml/components/DeviceCard.qml
)
# Embedded translations (JSON, see src/core/Localization.h).
file(GLOB HWMON_TRANSLATIONS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/translations/*.json)
qt_add_resources(LinuxHardwareMonitor i18n PREFIX "/i18n" BASE ${CMAKE_SOURCE_DIR}/translations FILES ${HWMON_TRANSLATIONS})

target_link_libraries(LinuxHardwareMonitor PRIVATE hwmon_core Qt6::Quick Qt6::QuickControls2)

enable_testing()
add_subdirectory(tests)

# ---- Installation and Debian package (cpack -G DEB) -------------------------------------
include(GNUInstallDirs)
install(TARGETS LinuxHardwareMonitor RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/linux-hardware-monitor.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(DIRECTORY mocks/ DESTINATION ${CMAKE_INSTALL_DATADIR}/linux-hardware-monitor/mocks FILES_MATCHING PATTERN "*.json")
install(FILES packaging/copyright DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/linux-hardware-monitor)
install(FILES packaging/linux-hardware-monitor.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

set(CPACK_PACKAGE_NAME "linux-hardware-monitor")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Hardware sensors, performance graphs and a task manager")
set(CPACK_PACKAGE_DESCRIPTION "Native Qt 6 system monitor: CPU, memory, disk, network, GPU and battery graphs, a process \
manager, systemd services and startup apps. Runs without root.")
set(CPACK_PACKAGE_CONTACT "Linux Hardware Monitor maintainers <zbara.dev@gmail.com>")
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_STRIP_FILES ON)
set(CPACK_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
# QML modules are loaded at run time, so shlibdeps cannot see them.
set(CPACK_DEBIAN_PACKAGE_DEPENDS "qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-layouts, qml6-module-qtquick-window, qml6-module-qtquick-templates")
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "network-manager, qt6-qpa-plugins")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
include(CPack)
