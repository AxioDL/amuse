#ifndef AMUSE_EDITOR_WIDGET_HPP
#define AMUSE_EDITOR_WIDGET_HPP

#include <QWidget>
#include <QUndoCommand>
#include <QApplication>
#include "ProjectModel.hpp"

class EditorWidget : public QWidget
{
    Q_OBJECT
public:
    explicit EditorWidget(QWidget* parent = Q_NULLPTR);
    virtual bool valid() const { return true; }
    virtual void unloadData() {}
};

class EditorUndoCommand : public QUndoCommand
{
protected:
    std::shared_ptr<ProjectModel::INode> m_node;
    enum class Id
    {
        SMChangeVal,
    };
public:
    EditorUndoCommand(std::shared_ptr<ProjectModel::INode> node,
                      const QString& text, QUndoCommand* parent = nullptr)
    : QUndoCommand(text, parent), m_node(node) {}
    void undo();
    void redo();
};

#endif //AMUSE_EDITOR_WIDGET_HPP
