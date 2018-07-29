#ifndef AMUSE_SAMPLE_EDITOR_HPP
#define AMUSE_SAMPLE_EDITOR_HPP

#include "EditorWidget.hpp"

class SampleEditor : public EditorWidget
{
    Q_OBJECT
public:
    explicit SampleEditor(QWidget* parent = Q_NULLPTR);
    bool loadData(ProjectModel::SampleNode* node);
};


#endif //AMUSE_SAMPLE_EDITOR_HPP
