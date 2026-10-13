// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// This check must not create a device during global option construction.
inline bool metal4OSAvailable()
{
#if defined(USE_METAL)
    if (__builtin_available(macOS 26.0, *))
        return true;
#endif
    return false;
}

bool metal4Available();
