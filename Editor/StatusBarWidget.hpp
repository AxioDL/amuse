#pragma once

#include <QStatusBar>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QMouseEvent>
#include <cmath>

class StatusBarFocus;

class FXButton : public QPushButton {
  Q_OBJECT
public:
  explicit FXButton(QWidget* parent = Q_NULLPTR);
  void mouseReleaseEvent(QMouseEvent* event) { event->ignore(); }
  void mouseMoveEvent(QMouseEvent* event) { event->ignore(); }
  void focusOutEvent(QFocusEvent* event) { event->ignore(); }
  void keyPressEvent(QKeyEvent* event) { event->ignore(); }
};

class StatusBarWidget : public QStatusBar {
  friend class StatusBarFocus;
  Q_OBJECT
  QLabel m_normalMessage;
  QPushButton m_killButton;
  FXButton m_fxButton;
  QIcon m_volumeIcons[4];
  QLabel m_volumeIcon;
  QSlider m_volumeSlider;
  QLabel m_aIcon;
  QSlider m_aSlider;
  QLabel m_bIcon;
  QSlider m_bSlider;
  int m_lastVolIdx = 0;
  QLabel m_voiceCount;
  int m_cachedVoiceCount = -1;
  StatusBarFocus* m_curFocus = nullptr;
  void setKillVisible(bool vis) {
    m_killButton.setVisible(vis);
    m_voiceCount.setVisible(vis);
  }

public:
  explicit StatusBarWidget(QWidget* parent = Q_NULLPTR);
  void setNormalMessage(const QString& message) { m_normalMessage.setText(message); }
  void setVoiceCount(int voices);

  template <typename Receiver>
  void connectKillClicked(const Receiver* receiver, void (Receiver::*method)()) {
    connect(&m_killButton, &QPushButton::clicked, receiver, method);
  }
  template <typename Receiver>
  void connectFXPressed(const Receiver* receiver, void (Receiver::*method)()) {
    connect(&m_fxButton, &FXButton::pressed, receiver, method);
  }
  void setFXDown(bool down) { m_fxButton.setDown(down); }

  template <typename Receiver>
  void connectVolumeSlider(const Receiver* receiver, void (Receiver::*method)(int)) {
    connect(&m_volumeSlider, qOverload<int>(&QSlider::valueChanged), receiver, method);
  }
  template <typename Receiver>
  void connectASlider(const Receiver* receiver, void (Receiver::*method)(int)) {
    connect(&m_aSlider, qOverload<int>(&QSlider::valueChanged), receiver, method);
  }
  template <typename Receiver>
  void connectBSlider(const Receiver* receiver, void (Receiver::*method)(int)) {
    connect(&m_bSlider, qOverload<int>(&QSlider::valueChanged), receiver, method);
  }
  void setVolumeValue(int vol) { m_volumeSlider.setValue(vol); }

private slots:
  void volumeChanged(int vol);
};

class StatusBarFocus : public QObject {
  Q_OBJECT
  QString m_message;

public:
  explicit StatusBarFocus(StatusBarWidget* statusWidget) : QObject(statusWidget) {}
  ~StatusBarFocus() { exit(); }
  void setMessage(const QString& message);
  void enter();
  void exit();
};
