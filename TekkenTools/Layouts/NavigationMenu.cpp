#include <ImGui.h>

#include "NavigationMenu.h"
#include "Localization.h"
#include "imgui_extras.h"

const NavMenuBtn const moveset_btns[] = {
	{NAV__MENU_EXTRACT, "navmenu.extraction"},
	{NAV__MENU_IMPORT, "navmenu.import"},
	{NAV__MENU_ONLINE_PLAY, "navmenu.online"},
};

const NavMenuBtn const tools_btns[] = {
	{NAV__MENU_EDITION, "navmenu.moveset_edit"},
	{NAV__MENU_CAMERA, "navmenu.camera"},
};

const NavMenuBtn const other_btns[] = {
	{NAV__MENU_DOCUMENTATION, "navmenu.documentation"},
	{NAV__MENU_ABOUT, "navmenu.about"},
};

NavigationMenu::NavigationMenu()
{
	// Init
	menuId = NAV__MENU_DEFAULT;
}

// Layout //

void NavigationMenu::RenderBtnList(const NavMenuBtn* BTNS, unsigned int SIZE)
{
	for (unsigned int i = 0; i < SIZE; ++i)
	{
		ImGui::Spacing();
		NavMenuBtn navBtn = BTNS[i];
		if (ImGuiExtra::RenderButtonEnabled(_(navBtn.NAME), menuId == navBtn.ID, ImVec2(215, 39))) {
			menuId = navBtn.ID;
		}
	}
}

void NavigationMenu::Render()
{
	ImGui::SeparatorText(_("navmenu.category_moveset"));
	RenderBtnList(moveset_btns, sizeof(moveset_btns) / sizeof(moveset_btns[0]));

	ImGui::SeparatorText(_("navmenu.category_tools"));
	RenderBtnList(tools_btns, sizeof(tools_btns) / sizeof(tools_btns[0]));

	ImGui::SeparatorText(_("navmenu.category_other"));
	RenderBtnList(other_btns, sizeof(other_btns) / sizeof(other_btns[0]));
}