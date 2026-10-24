#pragma once

#include "MockProfile.h"
#include "core/DmiMemory.h"
#include "core/SystemInfo.h"

#include <QVariantMap>

// The static hardware facts (model names, capacities, addresses) the real app reads from
// sysfs, produced from a simulation profile instead. Addresses use documentation ranges
// (RFC 5737 / locally administered MACs) so they can never be mistaken for real ones.
namespace MockFacts {

SystemInfo systemInfo(const MockProfile &profile, double memUtilPercent, double elapsedSeconds);

// Keys match what AppController::fillHardwareLists produces for real disks.
QVariantMap disk(const MockProfile &profile, const QString &name);
// Keys match the real network entries, including a "wifi" map for Wi-Fi adapters.
QVariantMap net(const MockProfile &profile, const QString &name);

// What `dmidecode -t memory` would report for the simulated machine: two equal modules in four slots
// (SODIMM when the profile has a battery, DIMM otherwise); DDR5 for the AMD/Intel profiles.
MemoryInfo memory(const MockProfile &profile);

const MockDisk *findDisk(const MockProfile &profile, const QString &name);
const MockNet *findNet(const MockProfile &profile, const QString &name);

} // namespace MockFacts
