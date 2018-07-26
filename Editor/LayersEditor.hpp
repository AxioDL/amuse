#ifndef AMUSE_LAYERS_EDITOR_HPP
#define AMUSE_LAYERS_EDITOR_HPP

#include "EditorWidget.hpp"

class LayersEditor : public EditorWidget
{
    Q_OBJECT
public:
    explicit LayersEditor(QWidget* parent = Q_NULLPTR);
    bool loadData(ProjectModel::LayersNode* node);
};


#endif //AMUSE_LAYERS_EDITOR_HPP
