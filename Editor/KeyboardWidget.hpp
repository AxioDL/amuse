#ifndef AMUSE_KEYBOARD_WIDGET_HPP
#define AMUSE_KEYBOARD_WIDGET_HPP

#include <QWidget>
#include <QSvgWidget>
#include "StatusBarWidget.hpp"

class KeyboardWidget;

class KeyboardOctave : public QSvgWidget
{
    Q_OBJECT
    int m_octave;
    QRectF m_natural[7];
    QRectF m_sharp[5];
    QTransform m_widgetToSvg;
public:
    explicit KeyboardOctave(int octave, const QString& svgPath, QWidget* parent = Q_NULLPTR);
    int getOctave() const { return m_octave; }
    int getKey(const QPoint& localPos) const;
    void resizeEvent(QResizeEvent *event);
};

class KeyboardWidget : public QWidget
{
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
    void showEvent(QShowEvent *event);
};


#endif //AMUSE_KEYBOARD_WIDGET_HPP
