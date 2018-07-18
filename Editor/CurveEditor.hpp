#ifndef AMUSE_CURVE_EDITOR_HPP
#define AMUSE_CURVE_EDITOR_HPP

#include "EditorWidget.hpp"

class CurveEditor : public EditorWidget
{
Q_OBJECT
public:
    explicit CurveEditor(ProjectModel::CurveNode* node, QWidget* parent = Q_NULLPTR);
};


#endif //AMUSE_CURVE_EDITOR_HPP
