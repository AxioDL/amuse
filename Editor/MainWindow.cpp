#include "MainWindow.hpp"
#include "ui_MainWindow.h"

MainWindow::MainWindow(QWidget* parent)
: QMainWindow(parent),
  m_ui(new Ui::MainWindow)
{
    m_ui->setupUi(this);
}

