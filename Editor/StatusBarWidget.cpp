#include "StatusBarWidget.hpp"

FXButton::FXButton(QWidget* parent)
: QPushButton(parent)
{
    setIcon(QIcon(QStringLiteral(":/icons/IconFX.svg")));
    setToolTip(tr("Access studio setup window for experimenting with audio effects"));
}

StatusBarWidget::StatusBarWidget(QWidget* parent)
: QStatusBar(parent), m_volumeSlider(Qt::Horizontal)
{
    addWidget(&m_normalMessage);
    m_killButton.setIcon(QIcon(QStringLiteral(":/icons/IconKill.svg")));
    m_killButton.setVisible(false);
    m_killButton.setToolTip(tr("Immediately kill active voices"));
    m_voiceCount.setVisible(false);
    m_volumeIcons[0] = QIcon(QStringLiteral(":/icons/IconVolume0"));
    m_volumeIcons[1] = QIcon(QStringLiteral(":/icons/IconVolume1"));
    m_volumeIcons[2] = QIcon(QStringLiteral(":/icons/IconVolume2"));
    m_volumeIcons[3] = QIcon(QStringLiteral(":/icons/IconVolume3"));
    m_volumeIcon.setFixedSize(16, 16);
    m_volumeIcon.setPixmap(m_volumeIcons[0].pixmap(16, 16));
    connect(&m_volumeSlider, SIGNAL(valueChanged(int)), this, SLOT(volumeChanged(int)));
    m_volumeSlider.setRange(0, 100);
    m_volumeSlider.setFixedWidth(100);
    addPermanentWidget(&m_voiceCount);
    addPermanentWidget(&m_killButton);
    addPermanentWidget(&m_fxButton);
    addPermanentWidget(&m_volumeIcon);
    addPermanentWidget(&m_volumeSlider);
}

void StatusBarWidget::setVoiceCount(int voices)
{
    if (voices != m_cachedVoiceCount)
    {
        m_voiceCount.setText(QString::number(voices));
        m_cachedVoiceCount = voices;
        setKillVisible(voices != 0);
    }
}

void StatusBarWidget::volumeChanged(int vol)
{
    int idx = int(std::round(vol * (3.f / 100.f)));
    if (idx != m_lastVolIdx)
    {
        m_lastVolIdx = idx;
        m_volumeIcon.setPixmap(m_volumeIcons[idx].pixmap(16, 16));
    }
}

void StatusBarFocus::setMessage(const QString& message)
{
    m_message = message;
    if (StatusBarWidget* widget = qobject_cast<StatusBarWidget*>(parent()))
    {
        if (widget->m_curFocus == this)
        {
            if (m_message.isEmpty())
                widget->clearMessage();
            else
                widget->showMessage(m_message);
        }
    }
}

void StatusBarFocus::enter()
{
    if (StatusBarWidget* widget = qobject_cast<StatusBarWidget*>(parent()))
    {
        widget->m_curFocus = this;
        if (m_message.isEmpty())
            widget->clearMessage();
        else
            widget->showMessage(m_message);
    }
}

void StatusBarFocus::exit()
{
    if (StatusBarWidget* widget = qobject_cast<StatusBarWidget*>(parent()))
    {
        if (widget->m_curFocus == this)
        {
            widget->clearMessage();
            widget->m_curFocus = nullptr;
        }
    }
}
