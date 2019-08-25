#pragma once

#include <array>

#include "EditorWidget.hpp"
#include <QFrame>
#include <QLabel>
#include <QStaticText>
#include <QLineEdit>
#include <QPushButton>
#include <QJSEngine>

class CurveEditor;

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
  bool loadData(ProjectModel::CurveNode* node);
  void unloadData() override;
  ProjectModel::INode* currentNode() const override;
};
