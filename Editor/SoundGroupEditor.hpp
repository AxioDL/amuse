#ifndef AMUSE_SOUND_GROUP_EDITOR_HPP
#define AMUSE_SOUND_GROUP_EDITOR_HPP

#include "EditorWidget.hpp"

class SoundGroupEditor : public EditorWidget
{
    Q_OBJECT
public:
    explicit SoundGroupEditor(QWidget* parent = Q_NULLPTR);
    bool loadData(ProjectModel::SoundGroupNode* node);
};


#endif //AMUSE_SOUND_GROUP_EDITOR_HPP
