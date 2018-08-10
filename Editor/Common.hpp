#ifndef AMUSE_COMMON_HPP
#define AMUSE_COMMON_HPP

#include "boo/System.hpp"
#include <QString>
#include <QDir>
#include <QMessageBox>

class MainWindow;
extern MainWindow* g_MainWindow;

class UIMessenger : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
signals:
    QMessageBox::StandardButton information(const QString &title,
            const QString &text, QMessageBox::StandardButtons buttons = QMessageBox::Ok,
            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);
    QMessageBox::StandardButton question(const QString &title,
            const QString &text, QMessageBox::StandardButtons buttons =
                QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No),
            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);
    QMessageBox::StandardButton warning(const QString &title,
            const QString &text, QMessageBox::StandardButtons buttons = QMessageBox::Ok,
            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);
    QMessageBox::StandardButton critical(const QString &title,
            const QString &text, QMessageBox::StandardButtons buttons = QMessageBox::Ok,
            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);
};

boo::SystemString QStringToSysString(const QString& str);
QString SysStringToQString(const boo::SystemString& str);

bool MkPath(const QString& path, UIMessenger& messenger);
bool MkPath(const QDir& dir, const QString& file, UIMessenger& messenger);

void ShowInGraphicalShell(QWidget* parent, const QString& pathIn);
QString ShowInGraphicalShellString();

static QLatin1String StringViewToQString(std::string_view sv)
{
    return QLatin1String(sv.data(), int(sv.size()));
}

/* Used for generating transform matrices to map SVG coordinate space */
QTransform RectToRect(const QRectF& from, const QRectF& to);

namespace std
{
template<> struct hash<QString>
{
    std::size_t operator()(const QString& s) const noexcept
    {
        return qHash(s);
    }
};
}

#endif //AMUSE_COMMON_HPP
