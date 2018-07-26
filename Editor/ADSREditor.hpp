#ifndef AMUSE_ADSR_EDITOR_HPP
#define AMUSE_ADSR_EDITOR_HPP

#include "EditorWidget.hpp"

class ADSREditor : public EditorWidget
{
Q_OBJECT
public:
    explicit ADSREditor(QWidget* parent = Q_NULLPTR);
    bool loadData(ProjectModel::ADSRNode* node);
};


#endif //AMUSE_ADSR_EDITOR_HPP
