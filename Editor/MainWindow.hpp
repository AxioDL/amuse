#ifndef AMUSE_MAINWINDOW_HPP
#define AMUSE_MAINWINDOW_HPP

#include <QMainWindow>
#include <QUndoStack>
#include <QProgressDialog>
#include <QThread>
#include "ui_MainWindow.h"
#include "amuse/Engine.hpp"
#include "amuse/BooBackend.hpp"
#include "boo/audiodev/IAudioVoiceEngine.hpp"
#include "ProjectModel.hpp"

namespace Ui {
class MainWindow;
}

class AudioGroupModel;

class BackgroundTask : public QObject
{
    Q_OBJECT
    std::function<void(BackgroundTask&)> m_task;
    UIMessenger m_threadMessenger;
    bool m_cancelled = false;
public:
    explicit BackgroundTask(std::function<void(BackgroundTask&)>&& task)
    : m_task(std::move(task)), m_threadMessenger(this) {}
    bool isCanceled() const { QCoreApplication::processEvents(); return m_cancelled; }
    UIMessenger& uiMessenger() { return m_threadMessenger; }

signals:
    void setMinimum(int minimum);
    void setMaximum(int maximum);
    void setValue(int value);
    void setLabelText(const QString& text);
    void finished();

public slots:
    void run() { m_task(*this); emit finished(); }
    void cancel() { m_cancelled = true; }
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
    Ui::MainWindow m_ui;
    UIMessenger m_mainMessenger;
    ProjectModel* m_projectModel = nullptr;
    AudioGroupModel* m_focusAudioGroup = nullptr;

    std::unique_ptr<boo::IAudioVoiceEngine> m_voxEngine;
    std::unique_ptr<amuse::BooBackendVoiceAllocator> m_voxAllocator;
    std::unique_ptr<amuse::Engine> m_engine;

    QUndoStack* m_undoStack;

    QMetaObject::Connection m_undoConn;
    QMetaObject::Connection m_canUndoConn;
    QMetaObject::Connection m_redoConn;
    QMetaObject::Connection m_canRedoConn;
    QMetaObject::Connection m_cutConn;
    QMetaObject::Connection m_copyConn;
    QMetaObject::Connection m_pasteConn;
    QMetaObject::Connection m_deleteConn;
    QMetaObject::Connection m_canEditConn;

    BackgroundTask* m_backgroundTask = nullptr;
    QProgressDialog* m_backgroundDialog = nullptr;
    QThread m_backgroundThread;

    void connectMessenger(UIMessenger* messenger, Qt::ConnectionType type);

    bool setProjectPath(const QString& path);
    void setFocusAudioGroup(AudioGroupModel* group);
    void refreshAudioIO();
    void refreshMIDIIO();

    void startBackgroundTask(const QString& windowTitle, const QString& label,
                             std::function<void(BackgroundTask&)>&& task);

public:
    explicit MainWindow(QWidget* parent = Q_NULLPTR);
    ~MainWindow();

public slots:
    void newAction();
    void openAction();
    void importAction();
    void exportAction();

    void newSFXGroupAction();
    void newSongGroupAction();
    void newSoundMacroAction();
    void newKeymapAction();
    void newLayersAction();

    void aboutToShowAudioIOMenu();
    void aboutToShowMIDIIOMenu();

    void setAudioIO();
    void setMIDIIO();

    void onFocusChanged(QWidget* old, QWidget* now);
    void onTextEdited();
    void onTextSelect();
    void onTextDelete();

    void onBackgroundTaskFinished();

    QMessageBox::StandardButton msgInformation(const QString &title,
        const QString &text, QMessageBox::StandardButtons buttons = QMessageBox::Ok,
        QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);
    QMessageBox::StandardButton msgQuestion(const QString &title,
        const QString &text, QMessageBox::StandardButtons buttons =
    QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No),
        QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);
    QMessageBox::StandardButton msgWarning(const QString &title,
        const QString &text, QMessageBox::StandardButtons buttons = QMessageBox::Ok,
        QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);
    QMessageBox::StandardButton msgCritical(const QString &title,
        const QString &text, QMessageBox::StandardButtons buttons = QMessageBox::Ok,
        QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

};


#endif //AMUSE_MAINWINDOW_HPP
