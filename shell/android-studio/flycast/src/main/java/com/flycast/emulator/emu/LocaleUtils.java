/*
	Copyright 2025 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
package com.flycast.emulator.emu;

import java.time.LocalDateTime;
import java.time.ZoneOffset;
import java.time.format.DateTimeFormatter;
import java.time.format.FormatStyle;
import java.util.Locale;

public class LocaleUtils {
    // called by native code
    private static String formatShortDateTime(long t) {
        LocalDateTime date = LocalDateTime.ofEpochSecond(t, 0, ZoneOffset.UTC);
        String s = date.format(DateTimeFormatter.ofLocalizedDateTime(FormatStyle.SHORT));
        // 'Narrow No-Break Space' isn't defined in our imgui font. Replace with 'Thin Space'
        s = s.replace('\u202F', '\u2009');
        return s;
    }

    // called by native code
    private static String getCurrentLocale() {
        return Locale.getDefault().toString();
    }

}
