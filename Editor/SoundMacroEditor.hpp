#ifndef AMUSE_SOUND_MACRO_EDITOR_HPP
#define AMUSE_SOUND_MACRO_EDITOR_HPP

#include "EditorWidget.hpp"

class SoundMacroEditor : public EditorWidget
{
    Q_OBJECT
public:
    explicit SoundMacroEditor(ProjectModel::SoundMacroNode* node, QWidget* parent = Q_NULLPTR);
};


#endif //AMUSE_SOUND_MACRO_EDITOR_HPP
