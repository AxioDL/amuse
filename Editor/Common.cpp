#include "Common.hpp"
#include "MainWindow.hpp"

#include <QDir>
#include <QMessageBox>
#include <QObject>
#include <QProcess>
#include <QTransform>

std::string QStringToUTF8(const QString& str) {
  return str.toUtf8().toStdString();
}

QString UTF8ToQString(const std::string& str) {
  return QString::fromStdString(str);
}

bool MkPath(const QString& path, UIMessenger& messenger) {
  QFileInfo fInfo(path);
  return MkPath(fInfo.dir(), fInfo.fileName(), messenger);
}

bool MkPath(const QDir& dir, const QString& file, UIMessenger& messenger) {
  if (!dir.mkpath(file)) {
    QString msg = QString(MainWindow::tr("A directory at '%1/%2' could not be created.")).arg(dir.path()).arg(file);
    messenger.critical(MainWindow::tr("Unable to create directory"), msg);
    return false;
  }
  return true;
}

void ShowInGraphicalShell(QWidget* parent, const QString& pathIn) {
  const QFileInfo fileInfo(pathIn);
  // Mac, Windows support folder or file.
#if defined(Q_OS_WIN)
  QString paths = QProcessEnvironment::systemEnvironment().value(QStringLiteral("Path"));
  QString explorer;
  for (QString path : paths.split(QStringLiteral(";"))) {
    QFileInfo finfo(QDir(path), QStringLiteral("explorer.exe"));
    if (finfo.exists()) {
      explorer = finfo.filePath();
      break;
    }
  }
  if (explorer.isEmpty()) {
    QMessageBox::warning(parent, MainWindow::tr("Launching Windows Explorer Failed"),
                         MainWindow::tr("Could not find explorer.exe in path to launch Windows Explorer."));
    return;
  }
  QStringList param;
  if (!fileInfo.isDir())
    param += QLatin1String("/select,");
  param += QDir::toNativeSeparators(fileInfo.canonicalFilePath());
  QProcess::startDetached(explorer, param);
#elif defined(Q_OS_MAC)
  QStringList scriptArgs;
  scriptArgs << QLatin1String("-e")
             << QString::fromLatin1("tell application \"Finder\" to reveal POSIX file \"%1\"")
                    .arg(fileInfo.canonicalFilePath());
  QProcess::execute(QLatin1String("/usr/bin/osascript"), scriptArgs);
  scriptArgs.clear();
  scriptArgs << QLatin1String("-e") << QLatin1String("tell application \"Finder\" to activate");
  QProcess::execute(QLatin1String("/usr/bin/osascript"), scriptArgs);
#else
  // we cannot select a file here, because no file browser really supports it...
  const QString folder = fileInfo.isDir() ? fileInfo.absoluteFilePath() : fileInfo.filePath();
  QProcess browserProc;
  const QStringList browserArgs = QStringList() << QStringLiteral("%1").arg(QFileInfo(folder).path());
  browserProc.startDetached(QStringLiteral("xdg-open"), browserArgs);
#endif
}

QString ShowInGraphicalShellString() {
#if defined(Q_OS_WIN)
  return MainWindow::tr("Show in Explorer");
#elif defined(Q_OS_MAC)
  return MainWindow::tr("Show in Finder");
#else
  return MainWindow::tr("Show in Browser");
#endif
}

QTransform RectToRect(const QRectF& from, const QRectF& to) {
  QPolygonF orig(from);
  orig.pop_back();
  QPolygonF resize(to);
  resize.pop_back();
  QTransform ret;
  QTransform::quadToQuad(orig, resize, ret);
  return ret;
}
