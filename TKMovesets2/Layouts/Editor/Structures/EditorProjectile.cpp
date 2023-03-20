#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorProjectile.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorProjectile::EditorProjectile(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow)
{
	windowType = EditorWindowType_Projectile;
	m_baseWindow = baseWindow;
	InitForm(windowTitleBase, t_id, editor);
}

void EditorProjectile::OnFieldLabelClick(EditorInput* field)
{
	int id = atoi(field->buffer);
	std::string& name = field->name;

	if (name == "cancel_addr") {
		m_baseWindow->OpenFormWindow(EditorWindowType_Cancel, id);
	}
	else if (name == "hit_condition_addr") {
		m_baseWindow->OpenFormWindow(EditorWindowType_HitCondition, id);
	}
}

void EditorProjectile::RequestFieldUpdate(std::string fieldName, int valueChange, int listStart, int listEnd)
{
	if (fieldName == "cancel_addr" || fieldName == "hit_condition_addr")
{
		EditorInput* field = m_fieldIdentifierMap[fieldName];

		if (field->errored) {
			return;
		}

		int value = atoi(field->buffer);
		if (MUST_SHIFT_ID(value, valueChange, listStart, listEnd)) {
			// Same shifting logic as in ListCreations
			// Might be a good idea to macro it
			sprintf(field->buffer, "%d", value + valueChange);
		}
	}
}