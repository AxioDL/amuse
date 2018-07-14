#ifndef AMUSE_COMMON_HPP
#define AMUSE_COMMON_HPP

#include "boo/System.hpp"
#include <QString>
#include <QDir>

boo::SystemString QStringToSysString(const QString& str);
QString SysStringToQString(const boo::SystemString& str);

bool MkPath(const QString& path, QWidget* parent);
bool MkPath(const QDir& dir, const QString& file, QWidget* parent);

#endif //AMUSE_COMMON_HPP
