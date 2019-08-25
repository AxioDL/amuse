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
  void mouseReleaseEvent(QMouseEvent* event) override { event->ignore(); }
  void mouseMoveEvent(QMouseEvent* event) override { event->ignore(); }
  void focusOutEvent(QFocusEvent* event) override { event->ignore(); }
  void keyPressEvent(QKeyEvent* event) override { event->ignore(); }
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
  void connectKillClicked(const QObject* receiver, const char* method) {
    connect(&m_killButton, SIGNAL(clicked(bool)), receiver, method);
  }
  void connectFXPressed(const QObject* receiver, const char* method) {
    connect(&m_fxButton, SIGNAL(pressed()), receiver, method);
  }
  void setFXDown(bool down) { m_fxButton.setDown(down); }
  void connectVolumeSlider(const QObject* receiver, const char* method) {
    connect(&m_volumeSlider, SIGNAL(valueChanged(int)), receiver, method);
  }
  void connectASlider(const QObject* receiver, const char* method) {
    connect(&m_aSlider, SIGNAL(valueChanged(int)), receiver, method);
  }
  void connectBSlider(const QObject* receiver, const char* method) {
    connect(&m_bSlider, SIGNAL(valueChanged(int)), receiver, method);
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
  ~StatusBarFocus() override { exit(); }
  void setMessage(const QString& message);
  void enter();
  void exit();
};
