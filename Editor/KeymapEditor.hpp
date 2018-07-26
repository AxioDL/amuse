#ifndef AMUSE_KEYMAP_EDITOR_HPP
#define AMUSE_KEYMAP_EDITOR_HPP

#include "EditorWidget.hpp"

class KeymapEditor : public EditorWidget
{
    Q_OBJECT
public:
    explicit KeymapEditor(QWidget* parent = Q_NULLPTR);
    bool loadData(ProjectModel::KeymapNode* node);
};


#endif //AMUSE_KEYMAP_EDITOR_HPP
