#ifndef AMUSE_EDITOR_WIDGET_HPP
#define AMUSE_EDITOR_WIDGET_HPP

#include <QWidget>
#include "ProjectModel.hpp"

class EditorWidget : public QWidget
{
Q_OBJECT
public:
    explicit EditorWidget(QWidget* parent = Q_NULLPTR);
    virtual bool valid() const { return true; }
};


#endif //AMUSE_EDITOR_WIDGET_HPP
