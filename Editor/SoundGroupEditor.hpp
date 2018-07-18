#ifndef AMUSE_SOUND_GROUP_EDITOR_HPP
#define AMUSE_SOUND_GROUP_EDITOR_HPP

#include "EditorWidget.hpp"

class SoundGroupEditor : public EditorWidget
{
    Q_OBJECT
public:
    explicit SoundGroupEditor(ProjectModel::SoundGroupNode* node, QWidget* parent = Q_NULLPTR);
};


#endif //AMUSE_SOUND_GROUP_EDITOR_HPP
