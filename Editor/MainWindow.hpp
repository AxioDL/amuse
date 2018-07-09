#ifndef AMUSE_MAINWINDOW_HPP
#define AMUSE_MAINWINDOW_HPP

#include <QMainWindow>
namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
    Ui::MainWindow* m_ui;
public:
    explicit MainWindow(QWidget* parent = Q_NULLPTR);
};


#endif //AMUSE_MAINWINDOW_HPP
