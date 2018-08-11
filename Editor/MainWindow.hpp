#ifndef AMUSE_MAINWINDOW_HPP
#define AMUSE_MAINWINDOW_HPP

#include <QMainWindow>
#include <QUndoStack>
#include <QProgressDialog>
#include <QThread>
#include <QStyledItemDelegate>
#include <QSortFilterProxyModel>
#include "ui_MainWindow.h"
#include "amuse/Engine.hpp"
#include "amuse/BooBackend.hpp"
#include "boo/audiodev/IAudioVoiceEngine.hpp"
#include "ProjectModel.hpp"
#include "EditorWidget.hpp"
#include "MIDIReader.hpp"

#define MaxRecentFiles 4

namespace Ui {
class MainWindow;
}

class MainWindow;
class SongGroupEditor;
class SoundGroupEditor;
class SoundMacroEditor;
class ADSREditor;
class CurveEditor;
class KeymapEditor;
class LayersEditor;
class SampleEditor;

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

class TreeDelegate : public QStyledItemDelegate
{
    Q_OBJECT
    MainWindow& m_window;
public:
    explicit TreeDelegate(MainWindow& window, QObject* parent = Q_NULLPTR)
    : QStyledItemDelegate(parent), m_window(window) {}

    bool editorEvent(QEvent *event,
                     QAbstractItemModel *model,
                     const QStyleOptionViewItem &option,
                     const QModelIndex &index);
};

class MainWindow : public QMainWindow
{
    friend class MIDIReader;
    Q_OBJECT
    Ui::MainWindow m_ui;
    QAction* m_clearRecentFileAct;
    QAction* m_recentFileActs[MaxRecentFiles];
    TreeDelegate m_treeDelegate;
    UIMessenger m_mainMessenger;
    ProjectModel* m_projectModel = nullptr;
    QSortFilterProxyModel m_filterProjectModel;
    QWidget* m_faceSvg;
    SongGroupEditor* m_songGroupEditor = nullptr;
    SoundGroupEditor* m_soundGroupEditor = nullptr;
    SoundMacroEditor* m_soundMacroEditor = nullptr;
    ADSREditor* m_adsrEditor = nullptr;
    CurveEditor* m_curveEditor = nullptr;
    KeymapEditor* m_keymapEditor = nullptr;
    LayersEditor* m_layersEditor = nullptr;
    SampleEditor* m_sampleEditor = nullptr;

    std::unique_ptr<boo::IAudioVoiceEngine> m_voxEngine;
    std::unique_ptr<VoiceAllocator> m_voxAllocator;
    std::unique_ptr<amuse::Engine> m_engine;
    amuse::ObjToken<amuse::Voice> m_lastSound;
    int m_velocity = 90;
    float m_pitch = 0.f;
    int8_t m_ctrlVals[128] = {};
    bool m_uiDisabled = false;

    QUndoStack* m_undoStack;

    QMetaObject::Connection m_cutConn;
    QMetaObject::Connection m_copyConn;
    QMetaObject::Connection m_pasteConn;
    QMetaObject::Connection m_deleteConn;
    QMetaObject::Connection m_canEditConn;

    BackgroundTask* m_backgroundTask = nullptr;
    QProgressDialog* m_backgroundDialog = nullptr;
    QThread m_backgroundThread;

    void connectMessenger(UIMessenger* messenger, Qt::ConnectionType type);

    void updateWindowTitle();
    void updateRecentFileActions();
    bool setProjectPath(const QString& path);
    void refreshAudioIO();
    void refreshMIDIIO();
    void timerEvent(QTimerEvent* ev);
    void setSustain(bool sustain);
    void keyPressEvent(QKeyEvent* ev);
    void keyReleaseEvent(QKeyEvent* ev);

    void startBackgroundTask(const QString& windowTitle, const QString& label,
                             std::function<void(BackgroundTask&)>&& task);

    bool _setEditor(EditorWidget* widget);

public:
    explicit MainWindow(QWidget* parent = Q_NULLPTR);
    ~MainWindow();

    bool openProject(const QString& path);

    bool openEditor(ProjectModel::SongGroupNode* node);
    bool openEditor(ProjectModel::SoundGroupNode* node);
    bool openEditor(ProjectModel::SoundMacroNode* node);
    bool openEditor(ProjectModel::ADSRNode* node);
    bool openEditor(ProjectModel::CurveNode* node);
    bool openEditor(ProjectModel::KeymapNode* node);
    bool openEditor(ProjectModel::LayersNode* node);
    bool openEditor(ProjectModel::SampleNode* node);
    bool openEditor(ProjectModel::INode* node);
    void closeEditor();

    ProjectModel::INode* getEditorNode() const;
    EditorWidget* getEditorWidget() const;
    amuse::ObjToken<amuse::Voice> startEditorVoice(uint8_t key, uint8_t vel);
    amuse::ObjToken<amuse::Voice> startSFX(amuse::GroupId groupId, amuse::SFXId sfxId);
    amuse::ObjToken<amuse::Sequencer> startSong(amuse::GroupId groupId, amuse::SongId songId,
                                                const unsigned char* arrData);
    void pushUndoCommand(QUndoCommand* cmd);
    void updateFocus();
    void aboutToDeleteNode(ProjectModel::INode* node);

    QString getGroupName(ProjectModel::GroupNode* group) const;
    ProjectModel::GroupNode* getSelectedGroupNode() const;
    QString getSelectedGroupName() const;
    void _recursiveExpandOutline(const QModelIndex& filterIndex) const;
    void recursiveExpandAndSelectOutline(const QModelIndex& index) const;

    ProjectModel* projectModel() const { return m_projectModel; }
    UIMessenger& uiMessenger() { return m_mainMessenger; }

public slots:
    void newAction();
    void openAction();
    void openRecentFileAction();
    void clearRecentFilesAction();
    void saveAction();
    void revertAction();
    void reloadSampleDataAction();
    void importAction();
    void importSongsAction();
    void exportAction();

    void newSubprojectAction();
    void newSFXGroupAction();
    void newSongGroupAction();
    void newSoundMacroAction();
    void newADSRAction();
    void newCurveAction();
    void newKeymapAction();
    void newLayersAction();

    void aboutToShowAudioIOMenu();
    void aboutToShowMIDIIOMenu();

    void setAudioIO();
    void setMIDIIO();

    void notePressed(int key);
    void noteReleased();
    void velocityChanged(int vel);
    void modulationChanged(int mod);
    void pitchChanged(int pitch);
    void killSounds();

    void outlineCutAction();
    void outlineCopyAction();
    void outlinePasteAction();
    void outlineDeleteAction();

    void onFocusChanged(QWidget* old, QWidget* now);
    void outlineItemActivated(const QModelIndex& index);
    void setItemEditEnabled(bool enabled);
    void setItemNewEnabled(bool enabled);
    bool canEditOutline();
    void onOutlineSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
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
