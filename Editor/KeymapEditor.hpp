#pragma once

#include "EditorWidget.hpp"

#include <array>
#include <bitset>

#include <QCheckBox>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QStaticText>
#include <QSpinBox>
#include <QSvgRenderer>

class KeymapEditor;

class PaintButton : public QPushButton {
  Q_OBJECT
public:
  explicit PaintButton(QWidget* parent = Q_NULLPTR);
  void mouseReleaseEvent(QMouseEvent* event) override { event->ignore(); }
  void mouseMoveEvent(QMouseEvent* event) override { event->ignore(); }
  void focusOutEvent(QFocusEvent* event) override { event->ignore(); }
  void keyPressEvent(QKeyEvent* event) override { event->ignore(); }
};

using PaintPalette = std::array<QColor, 129>;

class KeymapView : public QWidget {
  Q_OBJECT
  friend class KeymapControls;
  friend class KeymapEditor;
  amuse::ObjToken<ProjectModel::KeymapNode> m_node;
  QSvgRenderer m_octaveRenderer;
  QSvgRenderer m_lastOctaveRenderer;
  std::array<QRectF, 7> m_natural;
  std::array<QRectF, 5> m_sharp;
  QTransform m_widgetToSvg;
  QFont m_keyFont;
  std::array<QStaticText, 128> m_keyTexts;
  std::array<int, 128> m_keyPalettes;
  KeymapEditor* getEditor() const;
  int getKey(const QPoint& localPos) const;
  void drawKey(QPainter& painter, const QRect& octaveRect, qreal penWidth, const PaintPalette& keyPalette, int o,
               int k) const;

public:
  explicit KeymapView(QWidget* parent = Q_NULLPTR);
  void loadData(ProjectModel::KeymapNode* node);
  void unloadData();
  ProjectModel::INode* currentNode() const;

  void paintEvent(QPaintEvent* ev) override;
  void mousePressEvent(QMouseEvent* ev) override;
  void mouseMoveEvent(QMouseEvent* ev) override;
  void wheelEvent(QWheelEvent* event) override;
};

class KeymapControls : public QFrame {
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

class KeymapEditor : public EditorWidget {
  Q_OBJECT
  friend class KeymapView;
  friend class KeymapControls;
  QScrollArea* m_scrollArea;
  KeymapView* m_kmView;
  KeymapControls* m_controls;
  PaintPalette m_paintPalette;
  amuse::Keymap m_controlKeymap;
  std::unordered_map<uint64_t, std::pair<int, int>> m_configToIdx;
  std::bitset<129> m_idxBitmap;
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
  void unloadData() override;
  ProjectModel::INode* currentNode() const override;
  void keyPressEvent(QKeyEvent* event) override;
};
