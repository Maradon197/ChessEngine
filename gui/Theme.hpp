#ifndef THEME_HPP
#define THEME_HPP

#include "GuiTypes.hpp"

//Application look: light/dark palette and the default color of every ColorRole.
//Used by the Doorman only; widgets receive their colors through their setters.
namespace theme {

//resolves SYSTEM to the current system setting
bool isDark(ThemeMode mode);

//sets the application style and palette, call from the GUI thread
void applyMode(ThemeMode mode);

ColorValue getDefaultColor(ColorRole role, bool dark);

} // namespace theme

#endif
