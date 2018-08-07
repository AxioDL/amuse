#ifndef AMUSE_EDITOR_WIDGET_HPP
#define AMUSE_EDITOR_WIDGET_HPP

#include <QWidget>
#include <QUndoCommand>
#include <QApplication>
#include <QSpinBox>
#include <QComboBox>
#include <QWheelEvent>
#include "ProjectModel.hpp"

class EditorWidget : public QWidget
{
    Q_OBJECT
public:
    explicit EditorWidget(QWidget* parent = Q_NULLPTR);
    virtual bool valid() const { return true; }
    virtual void unloadData() {}
    virtual ProjectModel::INode* currentNode() const { return nullptr; }
public slots:
    virtual bool isItemEditEnabled() const { return false; }
    virtual void itemCutAction() {}
    virtual void itemCopyAction() {}
    virtual void itemPasteAction() {}
    virtual void itemDeleteAction() {}
};

class EditorUndoCommand : public QUndoCommand
{
protected:
    amuse::ObjToken<ProjectModel::INode> m_node;
    enum class Id
    {
        SMChangeVal,
        SampLoop,
        SampPitch,
        ADSRAttack,
        ADSRDecay,
        ADSRSustain,
        ADSRAttackAndDecay,
        ADSRDecayAndSustain,
        ADSRRelease,
        ADSRDLS,
        ADSRVelToAttack,
        ADSRKeyToDecay,
        CurveEdit
    };
public:
    EditorUndoCommand(amuse::ObjToken<ProjectModel::INode> node,
                      const QString& text, QUndoCommand* parent = nullptr)
    : QUndoCommand(text, parent), m_node(node) {}
    void undo();
    void redo();
};

class FieldSpinBox : public QSpinBox
{
    Q_OBJECT
public:
    explicit FieldSpinBox(QWidget* parent = Q_NULLPTR)
    : QSpinBox(parent) {}

    /* Don't scroll */
    void wheelEvent(QWheelEvent* event) { event->ignore(); }
};

class FieldComboBox : public QComboBox
{
    Q_OBJECT
public:
    explicit FieldComboBox(QWidget* parent = Q_NULLPTR)
    : QComboBox(parent) {}

    /* Don't scroll */
    void wheelEvent(QWheelEvent* event) { event->ignore(); }
};

class FieldProjectNode : public FieldComboBox
{
    Q_OBJECT
    ProjectModel::CollectionNode* m_collection;
public:
    explicit FieldProjectNode(ProjectModel::CollectionNode* collection = Q_NULLPTR, QWidget* parent = Q_NULLPTR);
    void setCollection(ProjectModel::CollectionNode* collection);
    ProjectModel::CollectionNode* collection() const { return m_collection; }
};

#endif //AMUSE_EDITOR_WIDGET_HPP
