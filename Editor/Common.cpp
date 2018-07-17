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

bool MkPath(const QString& path, UIMessenger& messenger)
{
    QFileInfo fInfo(path);
    return MkPath(fInfo.dir(), fInfo.fileName(), messenger);
}

bool MkPath(const QDir& dir, const QString& file, UIMessenger& messenger)
{
    if (!dir.mkpath(file))
    {
        QString msg = QString(QObject::tr("A directory at '%1/%2' could not be created.")).arg(dir.path()).arg(file);
        messenger.critical(QObject::tr("Unable to create directory"), msg);
        return false;
    }
    return true;
}
