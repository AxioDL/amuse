#pragma once

#include <QWidget>
#include <QSvgWidget>
#include <QSlider>
#include <QWheelEvent>
#include "StatusBarWidget.hpp"
#include "Common.hpp"

extern const QString NaturalKeyNames[7];
extern const QString SharpKeyNames[5];
extern const QString KeyStrings[12];
extern const int NaturalKeyNumbers[7];
extern const int SharpKeyNumbers[5];

class KeyboardWidget;

class KeyboardOctave : public QSvgWidget {
  Q_OBJECT
  int m_octave;
  QRectF m_natural[7];
  QRectF m_sharp[5];
  QTransform m_widgetToSvg;

public:
  explicit KeyboardOctave(int octave, const QString& svgPath, QWidget* parent = Q_NULLPTR);
  int getOctave() const { return m_octave; }
  int getKey(const QPoint& localPos) const;
  void resizeEvent(QResizeEvent* event);
};

class KeyboardWidget : public QWidget {
  Q_OBJECT
  KeyboardOctave* m_widgets[11];
  StatusBarFocus* m_statusFocus = nullptr;
  int m_lastOctave = -1;
  int m_lastKey = -1;
  bool m_holding = false;

  std::pair<int, int> _getOctaveAndKey(QMouseEvent* event) const;
  void _startKey(int octave, int key);
  void _stopKey();
  void _moveOnKey(int octave, int key);
  void _pressOnKey(int octave, int key);

public:
  explicit KeyboardWidget(QWidget* parent = Q_NULLPTR);
  void setStatusFocus(StatusBarFocus* statusFocus) { m_statusFocus = statusFocus; }

  void mouseMoveEvent(QMouseEvent* event);
  void mousePressEvent(QMouseEvent* event);
  void mouseReleaseEvent(QMouseEvent* event);
  void enterEvent(QEvent* event);
  void leaveEvent(QEvent* event);
  void wheelEvent(QWheelEvent* event);
  void showEvent(QShowEvent* event);

signals:
  void notePressed(int key);
  void noteReleased();
};

class KeyboardSlider : public QSlider {
  Q_OBJECT
protected:
  StatusBarFocus* m_statusFocus = nullptr;
  virtual QString stringOfValue(int value) const = 0;

public:
  explicit KeyboardSlider(QWidget* parent = Q_NULLPTR);
  void enterEvent(QEvent* event);
  void leaveEvent(QEvent* event);
  void setStatusFocus(StatusBarFocus* statusFocus);
  void sliderChange(SliderChange change);
};

class VelocitySlider : public KeyboardSlider {
  Q_OBJECT
  QString stringOfValue(int value) const;

public:
  explicit VelocitySlider(QWidget* parent = Q_NULLPTR);
};

class ModulationSlider : public KeyboardSlider {
  Q_OBJECT
  QString stringOfValue(int value) const;

public:
  explicit ModulationSlider(QWidget* parent = Q_NULLPTR);
};

class PitchSlider : public KeyboardSlider {
  Q_OBJECT
  QString stringOfValue(int value) const;

public:
  explicit PitchSlider(QWidget* parent = Q_NULLPTR);
  void mouseReleaseEvent(QMouseEvent* ev);
  void wheelEvent(QWheelEvent* ev) { ev->ignore(); }
};
