#ifndef AMUSE_MAINWINDOW_HPP
#define AMUSE_MAINWINDOW_HPP

#include <QMainWindow>
#include <QUndoStack>
#include "ui_MainWindow.h"
#include "amuse/Engine.hpp"
#include "amuse/BooBackend.hpp"
#include "boo/audiodev/IAudioVoiceEngine.hpp"
#include "ProjectModel.hpp"

namespace Ui {
class MainWindow;
}

class AudioGroupModel;

class MainWindow : public QMainWindow
{
    Q_OBJECT
    Ui::MainWindow m_ui;
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

    bool setProjectPath(const QString& path);
    void setFocusAudioGroup(AudioGroupModel* group);
    void refreshAudioIO();
    void refreshMIDIIO();

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

};


#endif //AMUSE_MAINWINDOW_HPP
