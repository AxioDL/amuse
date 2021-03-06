#include "StatusBarWidget.hpp"

#include <cmath>

FXButton::FXButton(QWidget* parent) : QPushButton(parent) {
  setIcon(QIcon(QStringLiteral(":/icons/IconFX.svg")));
  setToolTip(tr("Access studio setup window for experimenting with audio effects"));
}

StatusBarWidget::StatusBarWidget(QWidget* parent)
: QStatusBar(parent), m_volumeSlider(Qt::Horizontal), m_aSlider(Qt::Horizontal), m_bSlider(Qt::Horizontal) {
  addWidget(&m_normalMessage);
  m_killButton.setIcon(QIcon(QStringLiteral(":/icons/IconKill.svg")));
  m_killButton.setVisible(false);
  m_killButton.setToolTip(tr("Immediately kill active voices"));
  m_voiceCount.setVisible(false);
  for (size_t i = 0; i < m_volumeIcons.size(); i++) {
    m_volumeIcons[i] = QIcon(QStringLiteral(":/icons/IconVolume%1.svg").arg(i));
  }
  m_aIcon.setFixedSize(16, 16);
  m_aIcon.setPixmap(QIcon(QStringLiteral(":/icons/IconA.svg")).pixmap(16, 16));
  QString aTip = tr("Aux A send level for all voices");
  m_aIcon.setToolTip(aTip);
  m_aSlider.setRange(0, 100);
  m_aSlider.setFixedWidth(100);
  m_aSlider.setToolTip(aTip);
  m_bIcon.setFixedSize(16, 16);
  m_bIcon.setPixmap(QIcon(QStringLiteral(":/icons/IconB.svg")).pixmap(16, 16));
  QString bTip = tr("Aux B send level for all voices");
  m_bIcon.setToolTip(bTip);
  m_bSlider.setRange(0, 100);
  m_bSlider.setFixedWidth(100);
  m_bSlider.setToolTip(bTip);
  m_volumeIcon.setFixedSize(16, 16);
  m_volumeIcon.setPixmap(m_volumeIcons[0].pixmap(16, 16));
  QString volTip = tr("Master volume level");
  m_volumeIcon.setToolTip(volTip);
  connect(&m_volumeSlider, qOverload<int>(&QSlider::valueChanged), this, &StatusBarWidget::volumeChanged);
  m_volumeSlider.setRange(0, 100);
  m_volumeSlider.setFixedWidth(100);
  m_volumeSlider.setToolTip(volTip);
  addPermanentWidget(&m_voiceCount);
  addPermanentWidget(&m_killButton);
  addPermanentWidget(&m_fxButton);
  addPermanentWidget(&m_aIcon);
  addPermanentWidget(&m_aSlider);
  addPermanentWidget(&m_bIcon);
  addPermanentWidget(&m_bSlider);
  addPermanentWidget(&m_volumeIcon);
  addPermanentWidget(&m_volumeSlider);
}

StatusBarWidget::~StatusBarWidget() = default;

void StatusBarWidget::setVoiceCount(int voices) {
  if (voices != m_cachedVoiceCount) {
    m_voiceCount.setText(QString::number(voices));
    m_cachedVoiceCount = voices;
    setKillVisible(voices != 0);
  }
}

void StatusBarWidget::volumeChanged(int vol) {
  const int idx = int(std::round(vol * (3.f / 100.f)));

  if (idx == m_lastVolIdx) {
    return;
  }

  m_lastVolIdx = idx;
  m_volumeIcon.setPixmap(m_volumeIcons[idx].pixmap(16, 16));
}

void StatusBarFocus::setMessage(const QString& message) {
  m_message = message;
  if (StatusBarWidget* widget = qobject_cast<StatusBarWidget*>(parent())) {
    if (widget->m_curFocus == this) {
      if (m_message.isEmpty())
        widget->clearMessage();
      else
        widget->showMessage(m_message);
    }
  }
}

void StatusBarFocus::enter() {
  if (StatusBarWidget* widget = qobject_cast<StatusBarWidget*>(parent())) {
    widget->m_curFocus = this;
    if (m_message.isEmpty())
      widget->clearMessage();
    else
      widget->showMessage(m_message);
  }
}

void StatusBarFocus::exit() {
  if (StatusBarWidget* widget = qobject_cast<StatusBarWidget*>(parent())) {
    if (widget->m_curFocus == this) {
      widget->clearMessage();
      widget->m_curFocus = nullptr;
    }
  }
}
