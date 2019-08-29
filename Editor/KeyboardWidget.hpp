#pragma once

#include <array>

#include <QSlider>
#include <QString>
#include <QSvgWidget>
#include <QWheelEvent>
#include <QWidget>

extern const std::array<QString, 7> NaturalKeyNames;
extern const std::array<QString, 5> SharpKeyNames;
extern const std::array<QString, 12> KeyStrings;
extern const std::array<int, 7> NaturalKeyNumbers;
extern const std::array<int, 5> SharpKeyNumbers;

class KeyboardWidget;
class StatusBarFocus;

class KeyboardOctave : public QSvgWidget {
  Q_OBJECT
  int m_octave;
  std::array<QRectF, 7> m_natural;
  std::array<QRectF, 5> m_sharp;
  QTransform m_widgetToSvg;

public:
  explicit KeyboardOctave(int octave, const QString& svgPath, QWidget* parent = Q_NULLPTR);
  int getOctave() const { return m_octave; }
  int getKey(const QPoint& localPos) const;
  void resizeEvent(QResizeEvent* event) override;
};

class KeyboardWidget : public QWidget {
  Q_OBJECT
  std::array<KeyboardOctave*, 11> m_widgets{};
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
  ~KeyboardWidget() override;

  void setStatusFocus(StatusBarFocus* statusFocus) { m_statusFocus = statusFocus; }

  void mouseMoveEvent(QMouseEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void enterEvent(QEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void showEvent(QShowEvent* event) override;

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
  ~KeyboardSlider() override;

  void enterEvent(QEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void setStatusFocus(StatusBarFocus* statusFocus);
  void sliderChange(SliderChange change) override;
};

class VelocitySlider : public KeyboardSlider {
  Q_OBJECT
  QString stringOfValue(int value) const override;

public:
  explicit VelocitySlider(QWidget* parent = Q_NULLPTR);
};

class ModulationSlider : public KeyboardSlider {
  Q_OBJECT
  QString stringOfValue(int value) const override;

public:
  explicit ModulationSlider(QWidget* parent = Q_NULLPTR);
};

class PitchSlider : public KeyboardSlider {
  Q_OBJECT
  QString stringOfValue(int value) const override;

public:
  explicit PitchSlider(QWidget* parent = Q_NULLPTR);
  void mouseReleaseEvent(QMouseEvent* ev) override;
  void wheelEvent(QWheelEvent* ev) override { ev->ignore(); }
};
