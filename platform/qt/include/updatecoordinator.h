// SPDX-FileCopyrightText: Thomas Lothian
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef EWAF_UPDATECOORDINATOR_H
#define EWAF_UPDATECOORDINATOR_H

namespace EWAF {

class MainWindow;

class UpdateCoordinator {
public:
    static void check(MainWindow *window, bool manual);
};

} // namespace EWAF

#endif // EWAF_UPDATECOORDINATOR_H
