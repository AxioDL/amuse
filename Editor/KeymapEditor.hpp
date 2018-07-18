#ifndef AMUSE_KEYMAP_EDITOR_HPP
#define AMUSE_KEYMAP_EDITOR_HPP

#include "EditorWidget.hpp"

class KeymapEditor : public EditorWidget
{
    Q_OBJECT
public:
    explicit KeymapEditor(ProjectModel::KeymapNode* node, QWidget* parent = Q_NULLPTR);
};


#endif //AMUSE_KEYMAP_EDITOR_HPP
