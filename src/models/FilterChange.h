#pragma once

#include <QtGlobal>

// QSortFilterProxyModel::invalidateFilter() is deprecated from Qt 6.13 (it still works, and Qt 7 drops it) in
// favour of beginFilterChange()/endFilterChange(), which exist from Qt 6.9. A proxy model wraps the change of
// whatever its filter depends on:
//     HWMON_BEGIN_FILTER_CHANGE();
//     m_filter = text;
//     HWMON_END_FILTER_CHANGE();
// Older Qt versions simply invalidate at the end.
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
#define HWMON_BEGIN_FILTER_CHANGE() beginFilterChange()
#define HWMON_END_FILTER_CHANGE() endFilterChange()
#else
#define HWMON_BEGIN_FILTER_CHANGE() ((void)0)
#define HWMON_END_FILTER_CHANGE() invalidateFilter()
#endif
