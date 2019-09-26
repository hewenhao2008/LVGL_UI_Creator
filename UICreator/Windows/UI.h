#pragma once
#include <string>
#include <vector>
#include "lvgl/lvgl.h"
#include "PropertyWindow.h"
#include "ToolTray.h"
#include "SimWindow.h"
#include "ToolBar.h"
#include "../Fonts/FTFont.h"
#include "Themes.h"
#include "../Widgets/ColorPicker.h"

class UI
{
public:
	UI();

private:
	lv_theme_t *activeTheme;
	std::vector<lv_theme_t *> themes;
	void initializeThemes(uint16_t hue);

	PropertyWindow *propertyWindow;
	ToolTray *toolTray;
	SimWindow *simWindow;
	ToolBar *toolBar;
};