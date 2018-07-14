#include "Common.hpp"
#include <QMessageBox>
#include <QObject>

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

bool MkPath(const QString& path, QWidget* parent)
{
    QFileInfo fInfo(path);
    return MkPath(fInfo.dir(), fInfo.fileName(), parent);
}

bool MkPath(const QDir& dir, const QString& file, QWidget* parent)
{
    if (!dir.mkpath(file))
    {
        QString msg = QString(parent->tr("A directory at '%1/%2' could not be created.")).arg(dir.path()).arg(file);
        QMessageBox::critical(parent, parent->tr("Unable to create directory"), msg);
        return false;
    }
    return true;
}
