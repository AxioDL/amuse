#ifndef AMUSE_SONG_GROUP_EDITOR_HPP
#define AMUSE_SONG_GROUP_EDITOR_HPP

#include "EditorWidget.hpp"

class SongGroupEditor : public EditorWidget
{
    Q_OBJECT
public:
    explicit SongGroupEditor(ProjectModel::SongGroupNode* node, QWidget* parent = Q_NULLPTR);
};


#endif //AMUSE_SONG_GROUP_EDITOR_HPP
