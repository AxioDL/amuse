#ifndef AMUSE_CURVE_EDITOR_HPP
#define AMUSE_CURVE_EDITOR_HPP

#include "EditorWidget.hpp"
#include <QFrame>
#include <QLabel>
#include <QStaticText>
#include <QLineEdit>
#include <QPushButton>
#include <QScriptEngine>

class CurveEditor;

class CurveView : public QWidget
{
Q_OBJECT
    friend class CurveControls;
    amuse::ObjToken<ProjectModel::CurveNode> m_node;
    QFont m_gridFont;
    QStaticText m_percentTexts[11];
    QStaticText m_percentTextsCenter[11];
    CurveEditor* getEditor() const;
public:
    explicit CurveView(QWidget* parent = Q_NULLPTR);
    void loadData(ProjectModel::CurveNode* node);
    void unloadData();
    ProjectModel::INode* currentNode() const;

    void paintEvent(QPaintEvent* ev);
    void mousePressEvent(QMouseEvent* ev);
    void mouseMoveEvent(QMouseEvent* ev);
};

class CurveControls : public QFrame
{
Q_OBJECT
    friend class CurveView;
    QLineEdit* m_lineEdit;
    QLabel* m_errLabel;
    QPushButton* m_setExpr;
    QScriptEngine m_engine;
    CurveEditor* getEditor() const;
public:
    explicit CurveControls(QWidget* parent = Q_NULLPTR);
    void loadData();
    void unloadData();
    void resizeEvent(QResizeEvent* ev);
public slots:
    void exprCommit();
};

class CurveEditor : public EditorWidget
{
Q_OBJECT
    friend class CurveView;
    friend class CurveControls;
    CurveView* m_curveView;
    CurveControls* m_controls;
public:
    explicit CurveEditor(QWidget* parent = Q_NULLPTR);
    bool loadData(ProjectModel::CurveNode* node);
    void unloadData();
    ProjectModel::INode* currentNode() const;
};

#endif //AMUSE_CURVE_EDITOR_HPP
