#include "Common.hpp"
#include "MainWindow.hpp"
#include <QMessageBox>
#include <QObject>
#include <QProcess>

boo::SystemString QStringToSysString(const QString& str)
{
#ifdef _WIN32
    return (wchar_t*)str.utf16();
#else
    return str.toUtf8().toStdString();
#endif
}

QString SysStringToQString(const boo::SystemString& str)
{
#ifdef _WIN32
    return QString::fromStdWString(str);
#else
    return QString::fromStdString(str);
#endif
}

bool MkPath(const QString& path, UIMessenger& messenger)
{
    QFileInfo fInfo(path);
    return MkPath(fInfo.dir(), fInfo.fileName(), messenger);
}

bool MkPath(const QDir& dir, const QString& file, UIMessenger& messenger)
{
    if (!dir.mkpath(file))
    {
        QString msg = QString(MainWindow::tr("A directory at '%1/%2' could not be created.")).arg(dir.path()).arg(file);
        messenger.critical(MainWindow::tr("Unable to create directory"), msg);
        return false;
    }
    return true;
}

void ShowInGraphicalShell(QWidget* parent, const QString& pathIn)
{
    const QFileInfo fileInfo(pathIn);
    // Mac, Windows support folder or file.
#if defined(Q_OS_WIN)
    const FileName explorer = Environment::systemEnvironment().searchInPath(QLatin1String("explorer.exe"));
    if (explorer.isEmpty()) {
        QMessageBox::warning(parent,
                             MainWindow::tr("Launching Windows Explorer Failed"),
                             MainWindow::tr("Could not find explorer.exe in path to launch Windows Explorer."));
        return;
    }
    QStringList param;
    if (!fileInfo.isDir())
        param += QLatin1String("/select,");
    param += QDir::toNativeSeparators(fileInfo.canonicalFilePath());
    QProcess::startDetached(explorer.toString(), param);
#elif defined(Q_OS_MAC)
    QStringList scriptArgs;
    scriptArgs << QLatin1String("-e")
               << QString::fromLatin1("tell application \"Finder\" to reveal POSIX file \"%1\"")
                   .arg(fileInfo.canonicalFilePath());
    QProcess::execute(QLatin1String("/usr/bin/osascript"), scriptArgs);
    scriptArgs.clear();
    scriptArgs << QLatin1String("-e")
               << QLatin1String("tell application \"Finder\" to activate");
    QProcess::execute(QLatin1String("/usr/bin/osascript"), scriptArgs);
#else
    // we cannot select a file here, because no file browser really supports it...
    const QString folder = fileInfo.isDir() ? fileInfo.absoluteFilePath() : fileInfo.filePath();
    QProcess browserProc;
    const QString browserArgs = QStringLiteral("xdg-open \"%1\"").arg(QFileInfo(folder).path());
    browserProc.startDetached(browserArgs);
#endif
}

QString ShowInGraphicalShellString()
{
#if defined(Q_OS_WIN)
    return MainWindow::tr("Show in Explorer");
#elif defined(Q_OS_MAC)
    return MainWindow::tr("Show in Finder");
#else
    return MainWindow::tr("Show in Browser");
#endif
}

QTransform RectToRect(const QRectF& from, const QRectF& to)
{
    QPolygonF orig(from);
    orig.pop_back();
    QPolygonF resize(to);
    resize.pop_back();
    QTransform ret;
    QTransform::quadToQuad(orig, resize, ret);
    return ret;
}
