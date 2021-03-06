#include "KeyboardWidget.hpp"

#include <QApplication>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSvgRenderer>

#include "Common.hpp"
#include "StatusBarWidget.hpp"

const std::array<QString, 7> NaturalKeyNames{
    QStringLiteral("C"), QStringLiteral("D"), QStringLiteral("E"), QStringLiteral("F"),
    QStringLiteral("G"), QStringLiteral("A"), QStringLiteral("B"),
};

const std::array<QString, 5> SharpKeyNames{
    QStringLiteral("Cs"), QStringLiteral("Ds"), QStringLiteral("Fs"), QStringLiteral("Gs"), QStringLiteral("As"),
};

const std::array<QString, 12> KeyStrings{
    QStringLiteral("C"),  QStringLiteral("C#"), QStringLiteral("D"),  QStringLiteral("D#"),
    QStringLiteral("E"),  QStringLiteral("F"),  QStringLiteral("F#"), QStringLiteral("G"),
    QStringLiteral("G#"), QStringLiteral("A"),  QStringLiteral("A#"), QStringLiteral("B"),
};

const std::array<int, 7> NaturalKeyNumbers{
    0, 2, 4, 5, 7, 9, 11,
};

const std::array<int, 5> SharpKeyNumbers{
    1, 3, 6, 8, 10,
};

KeyboardOctave::KeyboardOctave(int octave, const QString& svgPath, QWidget* parent)
: QSvgWidget(svgPath, parent), m_octave(octave) {
  for (size_t i = 0; i < NaturalKeyNames.size(); ++i) {
    const auto& naturalKeyName = NaturalKeyNames[i];

    if (renderer()->elementExists(naturalKeyName)) {
      m_natural[i] = renderer()->transformForElement(naturalKeyName).mapRect(renderer()->boundsOnElement(naturalKeyName));
    }
  }

  for (size_t i = 0; i < SharpKeyNames.size(); ++i) {
    const auto& sharpKeyName = SharpKeyNames[i];

    if (renderer()->elementExists(sharpKeyName)) {
      m_sharp[i] = renderer()->transformForElement(sharpKeyName).mapRect(renderer()->boundsOnElement(sharpKeyName));
    }
  }

  /* The parent keyboard manages all mouse events */
  setAttribute(Qt::WA_TransparentForMouseEvents);
}

int KeyboardOctave::getKey(const QPoint& localPos) const {
  const QPointF localPoint = m_widgetToSvg.map(localPos);

  for (size_t i = 0; i < m_sharp.size(); ++i) {
    if (m_sharp[i].contains(localPoint)) {
      return SharpKeyNumbers[i];
    }
  }

  for (size_t i = 0; i < m_natural.size(); ++i) {
    if (m_natural[i].contains(localPoint)) {
      return NaturalKeyNumbers[i];
    }
  }

  return -1;
}

void KeyboardOctave::resizeEvent(QResizeEvent* event) { m_widgetToSvg = RectToRect(rect(), renderer()->viewBoxF()); }

KeyboardWidget::KeyboardWidget(QWidget* parent) : QWidget(parent) {
  QHBoxLayout* layout = new QHBoxLayout(this);
  layout->setContentsMargins(QMargins());
  layout->setSpacing(0);

  for (size_t i = 0; i < m_widgets.size() - 1; ++i) {
    m_widgets[i] = new KeyboardOctave(int(i), QStringLiteral(":/bg/keyboard.svg"), this);
    m_widgets[i]->setGeometry(QRect(0, 0, 141, 50));
    m_widgets[i]->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred));
    layout->addWidget(m_widgets[i]);
  }

  m_widgets[10] = new KeyboardOctave(10, QStringLiteral(":/bg/keyboard_last.svg"), this);
  m_widgets[10]->setGeometry(QRect(0, 0, 101, 50));
  m_widgets[10]->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred));
  layout->addWidget(m_widgets[10]);

  setLayout(layout);
  setMouseTracking(true);
}

KeyboardWidget::~KeyboardWidget() = default;

std::pair<int, int> KeyboardWidget::_getOctaveAndKey(QMouseEvent* event) const {
  for (KeyboardOctave* oct : m_widgets) {
    QPoint localPos = oct->mapFromParent(event->pos());
    if (oct->rect().contains(localPos))
      return {oct->getOctave(), oct->getKey(localPos)};
  }
  return {-1, -1};
}

void KeyboardWidget::_startKey(int octave, int key) { emit notePressed(octave * 12 + key); }

void KeyboardWidget::_stopKey() { emit noteReleased(); }

void KeyboardWidget::_moveOnKey(int octave, int key) {
  if (m_lastOctave == octave && m_lastKey == key) {
    return;
  }

  m_lastOctave = octave;
  m_lastKey = key;

  if (m_statusFocus) {
    m_statusFocus->setMessage(QStringLiteral("%1%2 (%3)").arg(KeyStrings[key]).arg(octave - 1).arg(octave * 12 + key));
  }

  if (m_holding) {
    _startKey(octave, key);
  }
}

void KeyboardWidget::_pressOnKey(int octave, int key) {
  _moveOnKey(octave, key);
  m_holding = true;
  _startKey(octave, key);
}

void KeyboardWidget::mouseMoveEvent(QMouseEvent* event) {
  std::pair<int, int> ok = _getOctaveAndKey(event);
  if (ok.first != -1 && ok.second != -1)
    _moveOnKey(ok.first, ok.second);
}

void KeyboardWidget::mousePressEvent(QMouseEvent* event) {
  std::pair<int, int> ok = _getOctaveAndKey(event);
  if (ok.first != -1 && ok.second != -1)
    _pressOnKey(ok.first, ok.second);
}

void KeyboardWidget::mouseReleaseEvent(QMouseEvent* event) {
  _stopKey();
  m_holding = false;
}

void KeyboardWidget::enterEvent(QEnterEvent* event) {
  if (m_statusFocus)
    m_statusFocus->enter();
}

void KeyboardWidget::leaveEvent(QEvent* event) {
  if (m_statusFocus)
    m_statusFocus->exit();
}

void KeyboardWidget::wheelEvent(QWheelEvent* event) {
  if (QScrollArea* scroll = qobject_cast<QScrollArea*>(parentWidget()->parentWidget())) {
    /* Send wheel event directly to the scroll bar */
    QApplication::sendEvent(scroll->horizontalScrollBar(), event);
  }
}

void KeyboardWidget::showEvent(QShowEvent* event) {
  if (QScrollArea* scroll = qobject_cast<QScrollArea*>(parentWidget()->parentWidget())) {
    /* Scroll to C1 */
    scroll->ensureVisible(141 * 2 + scroll->width(), 0, 0, 0);
  }
}

KeyboardSlider::KeyboardSlider(QWidget* parent) : QSlider(parent) {}

KeyboardSlider::~KeyboardSlider() = default;

void KeyboardSlider::enterEvent(QEnterEvent* event) {
  if (m_statusFocus)
    m_statusFocus->enter();
}

void KeyboardSlider::leaveEvent(QEvent* event) {
  if (m_statusFocus)
    m_statusFocus->exit();
}

void KeyboardSlider::setStatusFocus(StatusBarFocus* statusFocus) {
  m_statusFocus = statusFocus;
  QString str = stringOfValue(value());
  m_statusFocus->setMessage(str);
  setToolTip(str);
}

void KeyboardSlider::sliderChange(SliderChange change) {
  QSlider::sliderChange(change);
  if (m_statusFocus && change == QAbstractSlider::SliderValueChange) {
    QString str = stringOfValue(value());
    m_statusFocus->setMessage(str);
    setToolTip(str);
  }
}

VelocitySlider::VelocitySlider(QWidget* parent) : KeyboardSlider(parent) {}

QString VelocitySlider::stringOfValue(int value) const { return tr("Velocity: %1").arg(value); }

ModulationSlider::ModulationSlider(QWidget* parent) : KeyboardSlider(parent) {}

QString ModulationSlider::stringOfValue(int value) const { return tr("Modulation: %1").arg(value); }

PitchSlider::PitchSlider(QWidget* parent) : KeyboardSlider(parent) {}

QString PitchSlider::stringOfValue(int value) const { return tr("Pitch: %1").arg(value / 2048.0, 0, 'g', 2); }

void PitchSlider::mouseReleaseEvent(QMouseEvent* ev) {
  KeyboardSlider::mouseReleaseEvent(ev);
  setValue(0);
}
