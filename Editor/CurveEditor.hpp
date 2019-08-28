#pragma once

#include <array>

#include <QFrame>
#include <QJSEngine>
#include <QStaticText>

#include "EditorWidget.hpp"
#include "ProjectModel.hpp"

#include <amuse/Common.hpp>

class CurveEditor;

class QLabel;
class QLineEdit;
class QPushButton;

class CurveView : public QWidget {
  Q_OBJECT
  friend class CurveControls;
  amuse::ObjToken<ProjectModel::CurveNode> m_node;
  QFont m_gridFont;
  std::array<QStaticText, 11> m_percentTexts;
  std::array<QStaticText, 11> m_percentTextsCenter;
  CurveEditor* getEditor() const;

public:
  explicit CurveView(QWidget* parent = Q_NULLPTR);
  ~CurveView() override;

  void loadData(ProjectModel::CurveNode* node);
  void unloadData();
  ProjectModel::INode* currentNode() const;

  void paintEvent(QPaintEvent* ev) override;
  void mousePressEvent(QMouseEvent* ev) override;
  void mouseMoveEvent(QMouseEvent* ev) override;
};

class CurveControls : public QFrame {
  Q_OBJECT
  friend class CurveView;
  QLineEdit* m_lineEdit;
  QLabel* m_errLabel;
  QPushButton* m_setExpr;
  QJSEngine m_engine;
  CurveEditor* getEditor() const;

public:
  explicit CurveControls(QWidget* parent = Q_NULLPTR);
  ~CurveControls() override;

  void loadData();
  void unloadData();
  void resizeEvent(QResizeEvent* ev) override;

public slots:
  void exprCommit();
};

class CurveEditor : public EditorWidget {
  Q_OBJECT
  friend class CurveView;
  friend class CurveControls;
  CurveView* m_curveView;
  CurveControls* m_controls;

public:
  explicit CurveEditor(QWidget* parent = Q_NULLPTR);
  ~CurveEditor() override;

  bool loadData(ProjectModel::CurveNode* node);
  void unloadData() override;
  ProjectModel::INode* currentNode() const override;
};
