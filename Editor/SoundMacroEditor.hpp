#pragma once

#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QStaticText>
#include <QTreeWidget>

#include "EditorWidget.hpp"
#include "ProjectModel.hpp"

#include <amuse/AudioGroupPool.hpp>
#include <amuse/Common.hpp>

class CatalogueItem;
class SoundMacroEditor;
class SoundMacroListing;

class QLayoutItem;
class QPropertyAnimation;
class QSplitter;
class QVBoxLayout;

class TargetButton : public QPushButton {
  Q_OBJECT
public:
  explicit TargetButton(QWidget* parent = Q_NULLPTR);
  void mouseReleaseEvent(QMouseEvent* event) override { event->ignore(); }
  void mouseMoveEvent(QMouseEvent* event) override { event->ignore(); }
  void focusOutEvent(QFocusEvent* event) override { event->ignore(); }
  void keyPressEvent(QKeyEvent* event) override { event->ignore(); }
};

class FieldSoundMacroStep : public QWidget {
  friend class CommandWidget;
  Q_OBJECT
  FieldProjectNode* m_macroField;
  FieldSpinBox m_spinBox;
  TargetButton m_targetButton;
  SoundMacroEditor* getEditor() const;
  SoundMacroListing* getListing() const;
signals:
  void valueChanged(int);
public slots:
  void targetPressed();
  void updateMacroField();

public:
  explicit FieldSoundMacroStep(FieldProjectNode* macroField = Q_NULLPTR, QWidget* parent = Q_NULLPTR);
  ~FieldSoundMacroStep() override;
  void setIndex(int index);
  void cancel();
};

class CommandWidget : public QWidget {
  Q_OBJECT
  friend class SoundMacroListing;
  QFont m_numberFont;
  QLabel m_titleLabel;
  ListingDeleteButton m_deleteButton;
  QStaticText m_numberText;
  int m_index = -1;
  amuse::SoundMacro::ICmd* m_cmd;
  const amuse::SoundMacro::CmdIntrospection* m_introspection;
  FieldSoundMacroStep* m_stepField = nullptr;
  void setIndex(int index);
  SoundMacroListing* getParent() const;
private slots:
  void boolChanged(int);
  void numChanged(int);
  void nodeChanged(int);
  void deleteClicked();

private:
  CommandWidget(QWidget* parent, amuse::SoundMacro::ICmd* cmd, amuse::SoundMacro::CmdOp op, SoundMacroListing* listing);

public:
  CommandWidget(QWidget* parent, amuse::SoundMacro::ICmd* cmd, SoundMacroListing* listing);
  CommandWidget(QWidget* parent, amuse::SoundMacro::CmdOp op, SoundMacroListing* listing);
  ~CommandWidget() override;

  void paintEvent(QPaintEvent* event) override;
  QString getText() const { return m_titleLabel.text(); }
};

class CommandWidgetContainer : public QWidget {
  Q_OBJECT
  friend class SoundMacroListing;
  CommandWidget* m_commandWidget;
  QPropertyAnimation* m_animation = nullptr;
  void animateOpen();
  void animateClosed();
  void snapOpen();
  void snapClosed();

private slots:
  void animationDestroyed();

public:
  template <class... _Args>
  CommandWidgetContainer(QWidget* parent, _Args&&... args);
  ~CommandWidgetContainer() override;
};

class SoundMacroListing : public QWidget {
  Q_OBJECT
  friend class CommandWidget;
  friend class SoundMacroEditor;
  amuse::ObjToken<ProjectModel::SoundMacroNode> m_node;
  QVBoxLayout* m_layout;
  QLayoutItem* m_dragItem = nullptr;
  int m_origIdx = -1;
  int m_dragOpenIdx = -1;
  CommandWidgetContainer* m_prevDragOpen = nullptr;
  int m_autoscrollTimer = -1;
  int m_autoscrollDelta = 0;
  QWidget* m_autoscrollSource = nullptr;
  QMouseEvent* m_autoscrollEvent = nullptr;
  void startAutoscroll(QWidget* source, QMouseEvent* event, int delta);
  void stopAutoscroll();
  bool beginDrag(CommandWidget* widget);
  void endDrag();
  void cancelDrag();
  void _moveDrag(int hoverIdx, const QPoint& pt, QWidget* source, QMouseEvent* event);
  void moveDrag(CommandWidget* widget, const QPoint& pt, QWidget* source, QMouseEvent* event);
  int moveInsertDrag(const QPoint& pt, QWidget* source, QMouseEvent* event);
  void insertDragout();
  void insert(amuse::SoundMacro::CmdOp op, const QString& text);
  void deleteCommand(int index);
  void reindex();
  void clear();

public:
  explicit SoundMacroListing(QWidget* parent = Q_NULLPTR);
  ~SoundMacroListing() override;

  bool loadData(ProjectModel::SoundMacroNode* node);
  void unloadData();
  ProjectModel::INode* currentNode() const;
  void timerEvent(QTimerEvent* event) override;
};

class CatalogueItem : public QWidget {
  Q_OBJECT
  amuse::SoundMacro::CmdOp m_op;
  QLabel m_iconLab;
  QLabel m_label;

public:
  explicit CatalogueItem(amuse::SoundMacro::CmdOp op, const QString& name, const QString& doc,
                         QWidget* parent = Q_NULLPTR);
  explicit CatalogueItem(const CatalogueItem& other, QWidget* parent = Q_NULLPTR);
  ~CatalogueItem() override;

  amuse::SoundMacro::CmdOp getCmdOp() const { return m_op; }
  QString getText() const { return m_label.text(); }
};

class SoundMacroCatalogue : public QTreeWidget {
  Q_OBJECT
public:
  explicit SoundMacroCatalogue(QWidget* parent = Q_NULLPTR);
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
};

class SoundMacroEditor : public EditorWidget {
  Q_OBJECT
  friend class SoundMacroCatalogue;
  friend class FieldSoundMacroStep;
  QSplitter* m_splitter;
  SoundMacroListing* m_listing;
  SoundMacroCatalogue* m_catalogue;
  CommandWidget* m_draggedCmd = nullptr;
  CatalogueItem* m_draggedItem = nullptr;
  FieldSoundMacroStep* m_targetField = nullptr;
  QPoint m_draggedPt;
  int m_dragInsertIdx = -1;
  void beginCommandDrag(CommandWidget* widget, const QPoint& eventPt, const QPoint& pt);
  void beginCatalogueDrag(CatalogueItem* item, const QPoint& eventPt, const QPoint& pt);
  void beginStepTarget(FieldSoundMacroStep* stepField);
  void endStepTarget();

public:
  explicit SoundMacroEditor(QWidget* parent = Q_NULLPTR);
  ~SoundMacroEditor() override;

  bool loadData(ProjectModel::SoundMacroNode* node);
  void unloadData() override;
  ProjectModel::INode* currentNode() const override;

  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

public slots:
  void catalogueDoubleClicked(QTreeWidgetItem* item, int column);
};
