#pragma once

#include <QAction>
#include <QComboBox>
#include <QItemEditorFactory>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QUndoCommand>
#include <QWheelEvent>
#include <QWidget>

#include "ProjectModel.hpp"

#include <amuse/Common.hpp>

class EditorWidget : public QWidget {
  Q_OBJECT
public:
  explicit EditorWidget(QWidget* parent = Q_NULLPTR);
  ~EditorWidget() override;

  virtual bool valid() const { return true; }
  virtual void unloadData() {}
  virtual ProjectModel::INode* currentNode() const { return nullptr; }
  virtual void setEditorEnabled(bool en) { setEnabled(en); }
  virtual AmuseItemEditFlags itemEditFlags() const { return AmuseItemNone; }

public slots:
  virtual void itemCutAction() {}
  virtual void itemCopyAction() {}
  virtual void itemPasteAction() {}
  virtual void itemDeleteAction() {}
};

class EditorUndoCommand : public QUndoCommand {
protected:
  amuse::ObjToken<ProjectModel::INode> m_node;
  enum class Id {
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
  EditorUndoCommand(amuse::ObjToken<ProjectModel::INode> node, const QString& text, QUndoCommand* parent = nullptr)
  : QUndoCommand(text, parent), m_node(node) {}
  void undo() override;
  void redo() override;
};

class FieldSpinBox : public QSpinBox {
  Q_OBJECT
public:
  explicit FieldSpinBox(QWidget* parent = Q_NULLPTR) : QSpinBox(parent) {}

  /* Don't scroll */
  void wheelEvent(QWheelEvent* event) override { event->ignore(); }
};

class FieldSlider : public QWidget {
  Q_OBJECT
  QSlider m_slider;
  QLabel m_value;

public:
  explicit FieldSlider(QWidget* parent = Q_NULLPTR);

  /* Don't scroll */
  void wheelEvent(QWheelEvent* event) override { event->ignore(); }

  int value() const { return m_slider.value(); }
  void setValue(int value) {
    m_slider.setValue(value);
    doValueChanged(value);
  }
  void setRange(int min, int max) { m_slider.setRange(min, max); }

private slots:
  void doValueChanged(int value);
signals:
  void valueChanged(int value);
};

class FieldDoubleSlider : public QWidget {
  Q_OBJECT
  QSlider m_slider;
  QLabel m_value;
  double m_min = 0.0;
  double m_max = 1.0;

public:
  explicit FieldDoubleSlider(QWidget* parent = Q_NULLPTR);

  /* Don't scroll */
  void wheelEvent(QWheelEvent* event) override { event->ignore(); }

  double value() const;
  void setValue(double value);
  void setRange(double min, double max);

private slots:
  void doValueChanged(int value);
signals:
  void valueChanged(double value);
};

class FieldComboBox : public QComboBox {
  Q_OBJECT
public:
  explicit FieldComboBox(QWidget* parent = Q_NULLPTR) : QComboBox(parent) {}

  /* Don't scroll */
  void wheelEvent(QWheelEvent* event) override { event->ignore(); }
};

class FieldProjectNode : public QWidget {
  Q_OBJECT
  ProjectModel::CollectionNode* m_collection;
  FieldComboBox m_comboBox;
  QPushButton m_button;

public:
  explicit FieldProjectNode(ProjectModel::CollectionNode* collection = Q_NULLPTR, QWidget* parent = Q_NULLPTR);
  ~FieldProjectNode() override;

  void setCollection(ProjectModel::CollectionNode* collection);
  ProjectModel::CollectionNode* collection() const { return m_collection; }
  int currentIndex() const { return m_comboBox.currentIndex(); }
  void setCurrentIndex(int index) { m_comboBox.setCurrentIndex(index); }
  void showPopup() { m_comboBox.showPopup(); }
  ProjectModel::BasePoolObjectNode* currentNode() const;
  bool event(QEvent* ev) override;

private slots:
  void _currentIndexChanged(int);

public slots:
  void openCurrent();

signals:
  void currentIndexChanged(int);
};

class FieldPageObjectNode : public QWidget {
  Q_OBJECT
  ProjectModel::GroupNode* m_group;
  FieldComboBox m_comboBox;
  QPushButton m_button;

public:
  explicit FieldPageObjectNode(ProjectModel::GroupNode* group = Q_NULLPTR, QWidget* parent = Q_NULLPTR);
  ~FieldPageObjectNode() override;

  void setGroup(ProjectModel::GroupNode* group);
  ProjectModel::GroupNode* group() const { return m_group; }
  int currentIndex() const { return m_comboBox.currentIndex(); }
  void setCurrentIndex(int index) { m_comboBox.setCurrentIndex(index); }
  QModelIndex rootModelIndex() const { return m_comboBox.rootModelIndex(); }
  void showPopup() { m_comboBox.showPopup(); }
  ProjectModel::BasePoolObjectNode* currentNode() const;
  bool event(QEvent* ev) override;

private slots:
  void _currentIndexChanged(int);

public slots:
  void openCurrent();

signals:
  void currentIndexChanged(int);
};

template <class T>
class EditorFieldNode : public T {
  bool m_deferPopupOpen = true;

public:
  using T::T;
  bool shouldPopupOpen() {
    bool ret = m_deferPopupOpen;
    m_deferPopupOpen = false;
    return ret;
  }
};

using EditorFieldProjectNode = EditorFieldNode<FieldProjectNode>;
using EditorFieldPageObjectNode = EditorFieldNode<FieldPageObjectNode>;

template <int MIN, int MAX>
class RangedValueFactory : public QItemEditorFactory {
public:
  QWidget* createEditor(int userType, QWidget* parent) const override {
    QSpinBox* sb = new QSpinBox(parent);
    sb->setFrame(false);
    sb->setMinimum(MIN);
    sb->setMaximum(MAX);
    return sb;
  }
};

class AddRemoveButtons : public QWidget {
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

class ListingDeleteButton : public QPushButton {
  Q_OBJECT
public:
  explicit ListingDeleteButton(QWidget* parent = Q_NULLPTR);
  void enterEvent(QEnterEvent* event) override;
  void leaveEvent(QEvent* event) override;
};

class ContextMenu : public QMenu {
public:
  void hideEvent(QHideEvent* ev) override {
    QMenu::hideEvent(ev);
    deleteLater();
  }
};

class BaseObjectDelegate : public QStyledItemDelegate {
  Q_OBJECT
protected:
  virtual ProjectModel::INode* getNode(const QAbstractItemModel* model, const QModelIndex& index) const = 0;

public:
  explicit BaseObjectDelegate(QObject* parent = Q_NULLPTR) : QStyledItemDelegate(parent) {}
  bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                   const QModelIndex& index) override;
private slots:
  void doOpenEditor();
  void doFindUsages();
};
