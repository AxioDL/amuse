#ifndef AMUSE_MAINWINDOW_HPP
#define AMUSE_MAINWINDOW_HPP

#include <QMainWindow>
#include <QUndoStack>
#include <QProgressDialog>
#include <QThread>
#include <QStyledItemDelegate>
#include <QSortFilterProxyModel>
#include <QLinkedList>
#include "ui_MainWindow.h"
#include "amuse/Engine.hpp"
#include "amuse/BooBackend.hpp"
#include "boo/audiodev/IAudioVoiceEngine.hpp"
#include "ProjectModel.hpp"
#include "EditorWidget.hpp"
#include "MIDIReader.hpp"
#include "StudioSetupWidget.hpp"

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

enum BackgroundTaskId
{
    TaskOpen,
    TaskImport,
    TaskExport,
    TaskReloadSamples
};

class BackgroundTask : public QObject
{
    Q_OBJECT
    int m_id;
    std::function<void(BackgroundTask&)> m_task;
    UIMessenger m_threadMessenger;
    bool m_cancelled = false;
public:
    explicit BackgroundTask(int id, std::function<void(BackgroundTask&)>&& task)
    : m_id(id), m_task(std::move(task)), m_threadMessenger(this) {}
    bool isCanceled() const { QCoreApplication::processEvents(); return m_cancelled; }
    UIMessenger& uiMessenger() { return m_threadMessenger; }

signals:
    void setMinimum(int minimum);
    void setMaximum(int maximum);
    void setValue(int value);
    void setLabelText(const QString& text);
    void finished(int id);

public slots:
    void run() { m_task(*this); emit finished(m_id); }
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
public slots:
    void doExportGroup();
    void doFindUsages();
    void doCut();
    void doCopy();
    void doPaste();
    void doDuplicate();
    void doDelete();
    void doRename();
};

class MainWindow : public QMainWindow
{
    friend class MIDIReader;
    friend class ProjectModel;
    friend class GroupNodeUndoCommand;
    friend class TreeDelegate;
    Q_OBJECT
    Ui::MainWindow m_ui;
    QAction* m_goBack;
    QAction* m_goForward;
    QLinkedList<ProjectModel::INode*> m_navList;
    QLinkedList<ProjectModel::INode*>::iterator m_navIt;
    QAction* m_clearRecentFileAct;
    QAction* m_recentFileActs[MaxRecentFiles];
    TreeDelegate m_treeDelegate;
    UIMessenger m_mainMessenger;
    ProjectModel* m_projectModel = nullptr;
    QWidget* m_faceSvg;
    SongGroupEditor* m_songGroupEditor = nullptr;
    SoundGroupEditor* m_soundGroupEditor = nullptr;
    SoundMacroEditor* m_soundMacroEditor = nullptr;
    ADSREditor* m_adsrEditor = nullptr;
    CurveEditor* m_curveEditor = nullptr;
    KeymapEditor* m_keymapEditor = nullptr;
    LayersEditor* m_layersEditor = nullptr;
    SampleEditor* m_sampleEditor = nullptr;
    StudioSetupWidget* m_studioSetup = nullptr;

    std::unique_ptr<boo::IAudioVoiceEngine> m_voxEngine;
    std::unique_ptr<VoiceAllocator> m_voxAllocator;
    std::unique_ptr<amuse::Engine> m_engine;
    amuse::ObjToken<amuse::Voice> m_lastSound;
    amuse::ObjToken<amuse::Sequencer> m_interactiveSeq;
    int m_velocity = 90;
    float m_pitch = 0.f;
    int8_t m_ctrlVals[128] = {};
    float m_auxAVol = 0.f;
    float m_auxBVol = 0.f;
    bool m_uiDisabled = false;
    bool m_clipboardAmuseData = false;

    QUndoStack* m_undoStack;

    QMetaObject::Connection m_cutConn;
    QMetaObject::Connection m_copyConn;
    QMetaObject::Connection m_pasteConn;
    QMetaObject::Connection m_deleteConn;
    QMetaObject::Connection m_canEditConn;

    BackgroundTask* m_backgroundTask = nullptr;
    QProgressDialog* m_backgroundDialog = nullptr;
    QThread m_backgroundThread;

    uint64_t m_timerFireCount = 0;

    void connectMessenger(UIMessenger* messenger, Qt::ConnectionType type);

    void updateWindowTitle();
    void updateRecentFileActions();
    void updateNavigationButtons();
    bool setProjectPath(const QString& path);
    void refreshAudioIO();
    void refreshMIDIIO();
    void timerEvent(QTimerEvent* ev);
    void setSustain(bool sustain);
    void keyPressEvent(QKeyEvent* ev);
    void keyReleaseEvent(QKeyEvent* ev);

    void startBackgroundTask(int id, const QString& windowTitle, const QString& label,
                             std::function<void(BackgroundTask&)>&& task);

    bool _setEditor(EditorWidget* widget, bool appendNav = true);

public:
    explicit MainWindow(QWidget* parent = Q_NULLPTR);
    ~MainWindow();

    bool openProject(const QString& path);

    bool openEditor(ProjectModel::SongGroupNode* node, bool appendNav = true);
    bool openEditor(ProjectModel::SoundGroupNode* node, bool appendNav = true);
    bool openEditor(ProjectModel::SoundMacroNode* node, bool appendNav = true);
    bool openEditor(ProjectModel::ADSRNode* node, bool appendNav = true);
    bool openEditor(ProjectModel::CurveNode* node, bool appendNav = true);
    bool openEditor(ProjectModel::KeymapNode* node, bool appendNav = true);
    bool openEditor(ProjectModel::LayersNode* node, bool appendNav = true);
    bool openEditor(ProjectModel::SampleNode* node, bool appendNav = true);
    bool openEditor(ProjectModel::INode* node, bool appendNav = true);
    void closeEditor();

    ProjectModel::INode* getEditorNode() const;
    EditorWidget* getEditorWidget() const;
    amuse::ObjToken<amuse::Voice> startEditorVoice(uint8_t key, uint8_t vel);
    amuse::ObjToken<amuse::Voice> startSFX(amuse::GroupId groupId, amuse::SFXId sfxId);
    amuse::ObjToken<amuse::Sequencer> startSong(amuse::GroupId groupId, amuse::SongId songId,
                                                const unsigned char* arrData);
    void pushUndoCommand(EditorUndoCommand* cmd);
    void updateFocus();
    void aboutToDeleteNode(ProjectModel::INode* node);
    void closeEvent(QCloseEvent* ev);
    void showEvent(QShowEvent* ev);

    QString getGroupName(ProjectModel::GroupNode* group) const;
    ProjectModel::GroupNode* getSelectedGroupNode() const;
    QString getSelectedGroupName() const;
    void _recursiveExpandOutline(const QModelIndex& filterIndex) const;
    void recursiveExpandAndSelectOutline(const QModelIndex& index) const;

    ProjectModel* projectModel() const { return m_projectModel; }
    UIMessenger& uiMessenger() { return m_mainMessenger; }

    void setItemEditFlags(AmuseItemEditFlags flags);
    void setItemNewEnabled(bool enabled);
    AmuseItemEditFlags outlineEditFlags();
    bool isUiDisabled() const { return m_uiDisabled; }
    void findUsages(ProjectModel::INode* node);

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
    void importHeadersAction();
    void exportHeadersAction();

    void newSubprojectAction();
    void newSFXGroupAction();
    void newSongGroupAction();
    void newSoundMacroAction();
    void newADSRAction();
    void newCurveAction();
    void newKeymapAction();
    void newLayersAction();

    void goForward();
    void goBack();

    void aboutToShowAudioIOMenu();
    void aboutToShowMIDIIOMenu();

    void setAudioIO();
    void setMIDIIO(bool checked);

    void aboutAmuseAction();
    void aboutQtAction();

    void notePressed(int key);
    void noteReleased();
    void velocityChanged(int vel);
    void modulationChanged(int mod);
    void pitchChanged(int pitch);
    void killSounds();
    void fxPressed();
    void volumeChanged(int vol);
    void auxAChanged(int vol);
    void auxBChanged(int vol);

    void outlineCutAction();
    void outlineCopyAction();
    void outlinePasteAction();
    void outlineDeleteAction();

    void onFocusChanged(QWidget* old, QWidget* now);
    void onClipboardChanged();
    void outlineItemActivated(const QModelIndex& index);
    void onOutlineSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
    void onTextSelect();
    void onTextDelete();
    void cleanChanged(bool clean);

    void studioSetupHidden();
    void studioSetupShown();

    void onBackgroundTaskFinished(int id);

    QMessageBox::StandardButton msgInformation(const QString &title,
        const QString &text, QMessageBox::StandardButtons buttons = QMessageBox::Ok,
        QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);
    int msgQuestion(const QString &title,
                    const QString& text,
                    const QString& button0Text,
                    const QString& button1Text = QString(),
                    const QString& button2Text = QString(),
                    int defaultButtonNumber = 0,
                    int escapeButtonNumber = -1);
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
