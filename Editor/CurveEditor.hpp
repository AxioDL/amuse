#ifndef AMUSE_CURVE_EDITOR_HPP
#define AMUSE_CURVE_EDITOR_HPP

#include "EditorWidget.hpp"

class CurveEditor : public EditorWidget
{
Q_OBJECT
public:
    explicit CurveEditor(QWidget* parent = Q_NULLPTR);
    bool loadData(ProjectModel::CurveNode* node);
};


#endif //AMUSE_CURVE_EDITOR_HPP
