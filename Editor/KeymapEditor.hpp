#ifndef AMUSE_KEYMAP_EDITOR_HPP
#define AMUSE_KEYMAP_EDITOR_HPP

#include "EditorWidget.hpp"
#include <QFrame>
#include <QLabel>
#include <QStaticText>
#include <QSpinBox>
#include <QPushButton>
#include <QSvgRenderer>
#include <QScrollArea>
#include <QCheckBox>
#include <bitset>

class KeymapEditor;

class PaintButton : public QPushButton
{
Q_OBJECT
public:
    explicit PaintButton(QWidget* parent = Q_NULLPTR);
    void mouseReleaseEvent(QMouseEvent* event) { event->ignore(); }
    void mouseMoveEvent(QMouseEvent* event) { event->ignore(); }
    void focusOutEvent(QFocusEvent* event) { event->ignore(); }
    void keyPressEvent(QKeyEvent* event) { event->ignore(); }
};

class KeymapView : public QWidget
{
Q_OBJECT
    friend class KeymapControls;
    friend class KeymapEditor;
    amuse::ObjToken<ProjectModel::KeymapNode> m_node;
    QSvgRenderer m_octaveRenderer;
    QSvgRenderer m_lastOctaveRenderer;
    QRectF m_natural[7];
    QRectF m_sharp[5];
    QTransform m_widgetToSvg;
    QFont m_keyFont;
    QStaticText m_keyTexts[128];
    int m_keyPalettes[128];
    KeymapEditor* getEditor() const;
    int getKey(const QPoint& localPos) const;
    void drawKey(QPainter& painter, const QRect& octaveRect, qreal penWidth,
                 const QColor* keyPalette, int o, int k) const;
public:
    explicit KeymapView(QWidget* parent = Q_NULLPTR);
    void loadData(ProjectModel::KeymapNode* node);
    void unloadData();
    ProjectModel::INode* currentNode() const;

    void paintEvent(QPaintEvent* ev);
    void mousePressEvent(QMouseEvent* ev);
    void mouseMoveEvent(QMouseEvent* ev);
    void wheelEvent(QWheelEvent* event);
};

class KeymapControls : public QFrame
{
Q_OBJECT
    friend class KeymapView;
    friend class KeymapEditor;
    FieldProjectNode* m_macro;
    QSpinBox* m_transpose;
    QSpinBox* m_pan;
    QCheckBox* m_surround;
    QSpinBox* m_prioOffset;
    PaintButton* m_paintButton;
    bool m_enableUpdate = true;
    KeymapEditor* getEditor() const;
    void setPaintIdx(int idx);
    void setKeymap(const amuse::Keymap& km);
public:
    explicit KeymapControls(QWidget* parent = Q_NULLPTR);
    void loadData(ProjectModel::KeymapNode* node);
    void unloadData();
public slots:
    void controlChanged();
    void paintButtonPressed();
};

class KeymapEditor : public EditorWidget
{
Q_OBJECT
    friend class KeymapView;
    friend class KeymapControls;
    QScrollArea* m_scrollArea;
    KeymapView* m_kmView;
    KeymapControls* m_controls;
    QColor m_paintPalette[128];
    amuse::Keymap m_controlKeymap;
    std::unordered_map<uint64_t, std::pair<int, int>> m_configToIdx;
    std::bitset<128> m_idxBitmap;
    bool m_inPaint = false;
    void _touch();
    void touchKey(int key, bool bulk = false);
    void touchControl(const amuse::Keymap& km);
    int allocateConfigIdx(uint64_t key);
    void deallocateConfigIdx(uint64_t key);
    int getConfigIdx(uint64_t key) const;
public:
    explicit KeymapEditor(QWidget* parent = Q_NULLPTR);
    bool loadData(ProjectModel::KeymapNode* node);
    void unloadData();
    ProjectModel::INode* currentNode() const;
    void keyPressEvent(QKeyEvent* event);
};

#endif //AMUSE_KEYMAP_EDITOR_HPP
