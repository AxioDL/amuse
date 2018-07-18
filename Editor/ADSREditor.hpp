#ifndef AMUSE_ADSR_EDITOR_HPP
#define AMUSE_ADSR_EDITOR_HPP

#include "EditorWidget.hpp"

class ADSREditor : public EditorWidget
{
Q_OBJECT
public:
    explicit ADSREditor(ProjectModel::ADSRNode* node, QWidget* parent = Q_NULLPTR);
};


#endif //AMUSE_ADSR_EDITOR_HPP
