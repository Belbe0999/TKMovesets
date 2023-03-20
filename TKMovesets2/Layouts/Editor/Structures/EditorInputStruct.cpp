#include <ImGui.h>
#include <format>
#include <string>

#include "imgui_extras.hpp"
#include "EditorInputStruct.hpp"
#include "Localization.hpp"

// -- Public methods -- //

EditorInputStruct::EditorInputStruct(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow, int listSize)
{
	windowType = EditorWindowType_Input;
	m_baseWindow = baseWindow;
	m_listSize = listSize;
	InitForm(windowTitleBase, t_id, editor);
}

// -- Private methods-- //

void EditorInputStruct::OnUpdate(int listIdx, EditorInput* field)
{
	BuildItemDetails(listIdx);
}

void EditorInputStruct::BuildItemDetails(int listIdx)
{
	auto& identifierMap = m_items[listIdx]->identifierMaps;

	m_items[listIdx]->itemLabel = m_editor->GetCommandStr(identifierMap["direction"]->buffer, identifierMap["button"]->buffer);
}

void EditorInputStruct::OnResize(int sizeChange, int oldSize)
{
	m_baseWindow->IssueFieldUpdate("inputs", sizeChange, id, id + oldSize);
}

void EditorInputStruct::RequestFieldUpdate(std::string fieldName, int valueChange, int listStart, int listEnd)
{
	if (fieldName == "inputs") {
		// If a struct was created before this one, we must shfit our own ID
		if (MUST_SHIFT_ID(id, valueChange, listStart, listEnd)) {
			// Same shifting logic as in ListCreations
			id += valueChange;
			ApplyWindowName();
		}
	}
}