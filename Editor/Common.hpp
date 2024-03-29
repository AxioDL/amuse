#pragma once

#include <functional>

#include <QMessageBox>
#include <QString>

#include <boo/System.hpp>

class MainWindow;
extern MainWindow* g_MainWindow;

class QDir;
class QRectF;
class QTransform;

class UIMessenger : public QObject {
  Q_OBJECT
public:
  using QObject::QObject;
signals:
  QMessageBox::StandardButton information(const QString& title, const QString& text,
                                          QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                                          QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);
  int question(const QString& title, const QString& text, const QString& button0Text,
               const QString& button1Text = QString(), const QString& button2Text = QString(),
               int defaultButtonNumber = 0, int escapeButtonNumber = -1);
  QMessageBox::StandardButton
  question(const QString& title, const QString& text,
           QMessageBox::StandardButtons buttons = QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No),
           QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);
  QMessageBox::StandardButton warning(const QString& title, const QString& text,
                                      QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                                      QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);
  QMessageBox::StandardButton critical(const QString& title, const QString& text,
                                       QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                                       QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);
};

std::string QStringToUTF8(const QString& str);
QString UTF8ToQString(const std::string& str);

bool MkPath(const QString& path, UIMessenger& messenger);
bool MkPath(const QDir& dir, const QString& file, UIMessenger& messenger);

void ShowInGraphicalShell(QWidget* parent, const QString& pathIn);
QString ShowInGraphicalShellString();

/* Used for generating transform matrices to map SVG coordinate space */
QTransform RectToRect(const QRectF& from, const QRectF& to);
