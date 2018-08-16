#ifndef AMUSE_EDITOR_WIDGET_HPP
#define AMUSE_EDITOR_WIDGET_HPP

#include <QWidget>
#include <QUndoCommand>
#include <QApplication>
#include <QSpinBox>
#include <QComboBox>
#include <QWheelEvent>
#include <QItemEditorFactory>
#include <QToolButton>
#include <QAction>
#include <QPushButton>
#include <QLabel>
#include "ProjectModel.hpp"

class EditorWidget : public QWidget
{
    Q_OBJECT
public:
    explicit EditorWidget(QWidget* parent = Q_NULLPTR);
    virtual bool valid() const { return true; }
    virtual void unloadData() {}
    virtual ProjectModel::INode* currentNode() const { return nullptr; }
    virtual void setEditorEnabled(bool en) { setEnabled(en); }
    virtual bool isItemEditEnabled() const { return false; }
public slots:
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

class FieldSlider : public QWidget
{
Q_OBJECT
    QSlider m_slider;
    QLabel m_value;
public:
    explicit FieldSlider(QWidget* parent = Q_NULLPTR);

    /* Don't scroll */
    void wheelEvent(QWheelEvent* event) { event->ignore(); }

    int value() const { return m_slider.value(); }
    void setValue(int value) { m_slider.setValue(value); doValueChanged(value); }
    void setRange(int min, int max) { m_slider.setRange(min, max); }

private slots:
    void doValueChanged(int value);
signals:
    void valueChanged(int value);
};

class FieldDoubleSlider : public QWidget
{
    Q_OBJECT
    QSlider m_slider;
    QLabel m_value;
    double m_min = 0.0;
    double m_max = 1.0;
public:
    explicit FieldDoubleSlider(QWidget* parent = Q_NULLPTR);

    /* Don't scroll */
    void wheelEvent(QWheelEvent* event) { event->ignore(); }

    double value() const;
    void setValue(double value);
    void setRange(double min, double max);

private slots:
    void doValueChanged(int value);
signals:
    void valueChanged(double value);
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

class FieldPageObjectNode : public FieldComboBox
{
    Q_OBJECT
    ProjectModel::GroupNode* m_group;
public:
    explicit FieldPageObjectNode(ProjectModel::GroupNode* group = Q_NULLPTR, QWidget* parent = Q_NULLPTR);
    void setGroup(ProjectModel::GroupNode* group);
    ProjectModel::GroupNode* group() const { return m_group; }
};

template <class T>
class EditorFieldNode : public T
{
    bool m_deferPopupOpen = true;
public:
    using T::T;
    bool shouldPopupOpen()
    {
        bool ret = m_deferPopupOpen;
        m_deferPopupOpen = false;
        return ret;
    }
};

using EditorFieldProjectNode = EditorFieldNode<FieldProjectNode>;
using EditorFieldPageObjectNode = EditorFieldNode<FieldPageObjectNode>;

template <int MIN, int MAX>
class RangedValueFactory : public QItemEditorFactory
{
public:
    QWidget* createEditor(int userType, QWidget *parent) const
    {
        QSpinBox* sb = new QSpinBox(parent);
        sb->setFrame(false);
        sb->setMinimum(MIN);
        sb->setMaximum(MAX);
        return sb;
    }
};

class AddRemoveButtons : public QWidget
{
Q_OBJECT
    QAction m_addAction;
    QToolButton m_addButton;
    QAction m_removeAction;
    QToolButton m_removeButton;
public:
    explicit AddRemoveButtons(QWidget* parent = Q_NULLPTR);
    QAction* addAction() { return &m_addAction; }
    QAction* removeAction() { return &m_removeAction; }
};

class ListingDeleteButton : public QPushButton
{
Q_OBJECT
public:
    explicit ListingDeleteButton(QWidget* parent = Q_NULLPTR);
    void enterEvent(QEvent* event);
    void leaveEvent(QEvent* event);
};

#endif //AMUSE_EDITOR_WIDGET_HPP
