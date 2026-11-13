#ifndef THEME_HPP
#define THEME_HPP

#include "GuiTypes.hpp"

namespace theme {

bool isDark(ThemeMode mode);

void applyMode(ThemeMode mode);

ColorValue getDefaultColor(ColorRole role, bool dark);

} // !namespace theme

#endif
