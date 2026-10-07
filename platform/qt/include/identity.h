// SPDX-FileCopyrightText: Thomas Lothian
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef EWAF_IDENTITY_H
#define EWAF_IDENTITY_H

#include <QString>

namespace EWAF {

class Identity {
public:
    static bool isInternal() {
#if defined(INTERNAL_REFERENCE)
        return true;
#elif defined(Q_OS_MACOS)
        // Defense in depth: even a development Qt build on macOS cannot join production updates.
        return true;
#else
        return false;
#endif
    }

    static QString title() {
        return isInternal() ? QStringLiteral("EWAF \u2014 INTERNAL Qt Reference")
                            : QStringLiteral("EWAF \u2014 Every Week a Folder");
    }

    static QString applicationId() {
        return isInternal() ? QStringLiteral("com.tlolabs.ewaf.qt-internal")
                            : QStringLiteral("com.tlolabs.ewaf");
    }
};

} // namespace EWAF

#endif // EWAF_IDENTITY_H
