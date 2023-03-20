#pragma once

#include "Editor.hpp"
#include "EditorForm.hpp"

class EditorProjectile : public EditorForm
{
private:
	void OnFieldLabelClick(EditorInput* field) override;
public:
	EditorProjectile(std::string windowTitleBase, uint32_t t_id, Editor* editor, EditorWindowBase* baseWindow);
};