#pragma once

// Who made the program and where to find it: the single source for the About page, `--help` and, through
// tools/check_release_metadata.py, for the files that cannot include a header (README, man page,
// AppStream and Debian copyright files).
namespace ProjectInfo {

inline constexpr const char *kAuthor = "Zbara";
inline constexpr const char *kWebsite = "https://zbara.dev/";
inline constexpr const char *kProjectUrl = "https://github.com/Zbara/LinuxHardwareMonitor";
inline constexpr const char *kLicense = "GPL-3.0-or-later";

} // namespace ProjectInfo
