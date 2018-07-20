#ifndef AMUSE_STATUSBAR_WIDGET_HPP
#define AMUSE_STATUSBAR_WIDGET_HPP

#include <QStatusBar>
#include <QLabel>

class StatusBarFocus;

class StatusBarWidget : public QStatusBar
{
    friend class StatusBarFocus;
    Q_OBJECT
    QLabel* m_normalMessage;
    StatusBarFocus* m_curFocus = nullptr;
public:
    explicit StatusBarWidget(QWidget* parent = Q_NULLPTR) : QStatusBar(parent)
    {
        m_normalMessage = new QLabel(this);
        addWidget(m_normalMessage);
    }
    void setNormalMessage(const QString& message) { m_normalMessage->setText(message); }
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
