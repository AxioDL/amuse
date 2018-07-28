#ifndef AMUSE_STATUSBAR_WIDGET_HPP
#define AMUSE_STATUSBAR_WIDGET_HPP

#include <QStatusBar>
#include <QLabel>
#include <QPushButton>

class StatusBarFocus;

class StatusBarWidget : public QStatusBar
{
    friend class StatusBarFocus;
    Q_OBJECT
    QLabel m_normalMessage;
    QPushButton m_killButton;
    QLabel m_voiceCount;
    int m_cachedVoiceCount = -1;
    StatusBarFocus* m_curFocus = nullptr;
    void setKillVisible(bool vis) { m_killButton.setVisible(vis); m_voiceCount.setVisible(vis); }
public:
    explicit StatusBarWidget(QWidget* parent = Q_NULLPTR) : QStatusBar(parent)
    {
        addWidget(&m_normalMessage);
        m_killButton.setIcon(QIcon(QStringLiteral(":/icons/IconKill.svg")));
        m_killButton.setVisible(false);
        m_killButton.setToolTip(tr("Immediately kill active voices"));
        m_voiceCount.setVisible(false);
        addPermanentWidget(&m_voiceCount);
        addPermanentWidget(&m_killButton);
    }
    void setNormalMessage(const QString& message) { m_normalMessage.setText(message); }
    void setVoiceCount(int voices)
    {
        if (voices != m_cachedVoiceCount)
        {
            m_voiceCount.setText(QString::number(voices));
            m_cachedVoiceCount = voices;
            setKillVisible(voices != 0);
        }
    }
    void connectKillClicked(const QObject* receiver, const char* method)
    { connect(&m_killButton, SIGNAL(clicked(bool)), receiver, method); }
};

class StatusBarFocus : public QObject
{
Q_OBJECT
    QString m_message;
public:
    explicit StatusBarFocus(StatusBarWidget* statusWidget)
    : QObject(statusWidget) {}
    ~StatusBarFocus() { exit(); }
    void setMessage(const QString& message)
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
    void enter()
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
    void exit()
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
};

#endif //AMUSE_STATUSBAR_WIDGET_HPP
