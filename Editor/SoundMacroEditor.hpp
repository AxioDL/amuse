#ifndef AMUSE_SOUND_MACRO_EDITOR_HPP
#define AMUSE_SOUND_MACRO_EDITOR_HPP

#include "EditorWidget.hpp"
#include <QStaticText>
#include <QVBoxLayout>
#include <QPropertyAnimation>
#include <QSplitter>
#include <QLabel>
#include <QMouseEvent>
#include <QSpinBox>
#include <QComboBox>
#include <QTreeWidget>
#include <QPushButton>

class SoundMacroListing;
class CatalogueItem;

class FieldSpinBox : public QSpinBox
{
    Q_OBJECT
public:
    FieldSpinBox(QWidget* parent = Q_NULLPTR)
    : QSpinBox(parent) {}

    /* Don't scroll */
    void wheelEvent(QWheelEvent* event) { event->ignore(); }
};

class FieldComboBox : public QComboBox
{
Q_OBJECT
public:
    FieldComboBox(QWidget* parent = Q_NULLPTR)
    : QComboBox(parent) {}

    /* Don't scroll */
    void wheelEvent(QWheelEvent* event) { event->ignore(); }
};

class CommandWidget : public QWidget
{
    Q_OBJECT
    friend class SoundMacroListing;
    QFont m_numberFont;
    QLabel m_titleLabel;
    QPushButton m_deleteButton;
    QStaticText m_numberText;
    int m_index = -1;
    amuse::SoundMacro::ICmd* m_cmd;
    const amuse::SoundMacro::CmdIntrospection* m_introspection;
    void setIndex(int index);
    SoundMacroListing* getParent() const;
private slots:
    void boolChanged(int);
    void numChanged(int);
    void choiceChanged(int);
    void deleteClicked();
private:
    CommandWidget(amuse::SoundMacro::ICmd* cmd, amuse::SoundMacro::CmdOp op, QWidget* parent = Q_NULLPTR);
public:
    CommandWidget(amuse::SoundMacro::ICmd* cmd, QWidget* parent = Q_NULLPTR);
    CommandWidget(amuse::SoundMacro::CmdOp op, QWidget* parent = Q_NULLPTR);
    void paintEvent(QPaintEvent* event);
    QString getText() const { return m_titleLabel.text(); }
};

class CommandWidgetContainer : public QWidget
{
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
    CommandWidgetContainer(CommandWidget* child, QWidget* parent = Q_NULLPTR);
};

class SoundMacroListing : public QWidget
{
    Q_OBJECT
    friend class CommandWidget;
    friend class SoundMacroEditor;
    std::shared_ptr<ProjectModel::SoundMacroNode> m_node;
    QVBoxLayout* m_layout;
    QLayoutItem* m_dragItem = nullptr;
    int m_origIdx = -1;
    int m_dragOpenIdx = -1;
    CommandWidgetContainer* m_prevDragOpen = nullptr;
    int m_autoscrollTimer = -1;
    int m_autoscrollDelta = 0;
    QWidget* m_autoscrollSource = nullptr;
    QMouseEvent m_autoscrollEvent = {{}, {}, {}, {}, {}};
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
    bool loadData(ProjectModel::SoundMacroNode* node);
    void unloadData();
    void timerEvent(QTimerEvent* event);
};

class CatalogueItem : public QWidget
{
    Q_OBJECT
    amuse::SoundMacro::CmdOp m_op;
    QLabel m_iconLab;
    QLabel m_label;
public:
    explicit CatalogueItem(amuse::SoundMacro::CmdOp op, const QString& name,
                           const QString& doc, QWidget* parent = Q_NULLPTR);
    explicit CatalogueItem(const CatalogueItem& other, QWidget* parent = Q_NULLPTR);
    amuse::SoundMacro::CmdOp getCmdOp() const { return m_op; }
    QString getText() const { return m_label.text(); }
};

class SoundMacroCatalogue : public QTreeWidget
{
    Q_OBJECT
public:
    explicit SoundMacroCatalogue(QWidget* parent = Q_NULLPTR);
    void mousePressEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
};

class SoundMacroEditor : public EditorWidget
{
    Q_OBJECT
    friend class SoundMacroCatalogue;
    QSplitter* m_splitter;
    SoundMacroListing* m_listing;
    SoundMacroCatalogue* m_catalogue;
    CommandWidget* m_draggedCmd = nullptr;
    CatalogueItem* m_draggedItem = nullptr;
    QPoint m_draggedPt;
    int m_dragInsertIdx = -1;
    void beginCommandDrag(CommandWidget* widget, const QPoint& eventPt, const QPoint& pt);
    void beginCatalogueDrag(CatalogueItem* item, const QPoint& eventPt, const QPoint& pt);
public:
    explicit SoundMacroEditor(QWidget* parent = Q_NULLPTR);
    bool loadData(ProjectModel::SoundMacroNode* node);
    void unloadData();

    void mousePressEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
    void keyPressEvent(QKeyEvent* event);

public slots:
    void catalogueDoubleClicked(QTreeWidgetItem* item, int column);
};


#endif //AMUSE_SOUND_MACRO_EDITOR_HPP
