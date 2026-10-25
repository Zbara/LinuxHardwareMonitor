#pragma once

#include <QString>

// What this binary is: version, the commit it was built from and the Qt it runs on. The one source for
// `--version`, the About page and the bug-report text.
namespace BuildInfo {

QString version();        // "0.1.0"
// The commit the binary was built from, "v0.1.0-4-gf462ac8 (2026-10-25)"; empty when it was built from
// a source archive without git. Taken when CMake configured the project.
QString build();
QString qtRuntime();      // the Qt library in use
QString qtBuiltWith();    // the Qt headers it was compiled against

} // namespace BuildInfo
