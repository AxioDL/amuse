#ifndef AMUSE_STATUSBAR_WIDGET_HPP
#define AMUSE_STATUSBAR_WIDGET_HPP

#include <QStatusBar>

class StatusBarWidget : public QStatusBar
{
    Q_OBJECT
public:
    explicit StatusBarWidget(QWidget* parent = Q_NULLPTR);
};


#endif //AMUSE_STATUSBAR_WIDGET_HPP
