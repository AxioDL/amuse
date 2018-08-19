#include "MainWindow.hpp"
#include <QFileDialog>
#include <QMessageBox>
#include <QLineEdit>
#include <QInputDialog>
#include <QProgressDialog>
#include <QMouseEvent>
#include <QtSvg/QtSvg>
#include "amuse/ContainerRegistry.hpp"
#include "Common.hpp"
#include "SongGroupEditor.hpp"
#include "SoundGroupEditor.hpp"
#include "SoundGroupEditor.hpp"
#include "SoundMacroEditor.hpp"
#include "ADSREditor.hpp"
#include "CurveEditor.hpp"
#include "KeymapEditor.hpp"
#include "LayersEditor.hpp"
#include "SampleEditor.hpp"
#include "NewSoundMacroDialog.hpp"

MainWindow::MainWindow(QWidget* parent)
: QMainWindow(parent),
  m_treeDelegate(*this, this),
  m_mainMessenger(this),
  m_filterProjectModel(this),
  m_undoStack(new QUndoStack(this)),
  m_backgroundThread(this)
{
    m_backgroundThread.start();

    m_ui.setupUi(this);
    m_ui.splitter->setCollapsible(1, false);
    QPalette palette = m_ui.projectOutlineFilter->palette();
    palette.setColor(QPalette::Base, palette.color(QPalette::Background));
    m_ui.projectOutlineFilter->setPalette(palette);
    connect(m_ui.projectOutlineFilter, SIGNAL(textChanged(const QString&)),
            &m_filterProjectModel, SLOT(setFilterRegExp(const QString&)));
    m_filterProjectModel.setRecursiveFilteringEnabled(true);
    m_filterProjectModel.setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_ui.projectOutline->setItemDelegate(&m_treeDelegate);
    connect(m_ui.projectOutline, SIGNAL(activated(const QModelIndex&)),
            this, SLOT(outlineItemActivated(const QModelIndex&)));
    connectMessenger(&m_mainMessenger, Qt::DirectConnection);

    m_ui.statusbar->connectKillClicked(this, SLOT(killSounds()));
    m_ui.statusbar->connectFXPressed(this, SLOT(fxPressed()));
    m_ui.statusbar->setVolumeValue(70);
    m_ui.statusbar->connectVolumeSlider(this, SLOT(volumeChanged(int)));
    m_ui.statusbar->connectASlider(this, SLOT(auxAChanged(int)));
    m_ui.statusbar->connectBSlider(this, SLOT(auxBChanged(int)));

    m_ui.keyboardContents->setStatusFocus(new StatusBarFocus(m_ui.statusbar));
    m_ui.velocitySlider->setStatusFocus(new StatusBarFocus(m_ui.statusbar));
    m_ui.modulationSlider->setStatusFocus(new StatusBarFocus(m_ui.statusbar));
    m_ui.pitchSlider->setStatusFocus(new StatusBarFocus(m_ui.statusbar));
    connect(m_ui.keyboardContents, SIGNAL(notePressed(int)), this, SLOT(notePressed(int)));
    connect(m_ui.keyboardContents, SIGNAL(noteReleased()), this, SLOT(noteReleased()));
    connect(m_ui.velocitySlider, SIGNAL(valueChanged(int)), this, SLOT(velocityChanged(int)));
    connect(m_ui.modulationSlider, SIGNAL(valueChanged(int)), this, SLOT(modulationChanged(int)));
    connect(m_ui.pitchSlider, SIGNAL(valueChanged(int)), this, SLOT(pitchChanged(int)));

    m_ui.actionNew_Project->setShortcut(QKeySequence::New);
    connect(m_ui.actionNew_Project, SIGNAL(triggered()), this, SLOT(newAction()));
    m_ui.actionOpen_Project->setShortcut(QKeySequence::Open);
    connect(m_ui.actionOpen_Project, SIGNAL(triggered()), this, SLOT(openAction()));
    m_ui.actionSave_Project->setShortcut(QKeySequence::Save);
    connect(m_ui.actionSave_Project, SIGNAL(triggered()), this, SLOT(saveAction()));
    connect(m_ui.actionRevert_Project, SIGNAL(triggered()), this, SLOT(revertAction()));
    connect(m_ui.actionReload_Sample_Data, SIGNAL(triggered()), this, SLOT(reloadSampleDataAction()));
    connect(m_ui.actionImport_Groups, SIGNAL(triggered()), this, SLOT(importAction()));
    connect(m_ui.actionImport_Songs, SIGNAL(triggered()), this, SLOT(importSongsAction()));
    connect(m_ui.actionExport_GameCube_Groups, SIGNAL(triggered()), this, SLOT(exportAction()));

    for (int i = 0; i < MaxRecentFiles; ++i)
    {
        m_recentFileActs[i] = new QAction(this);
        m_recentFileActs[i]->setVisible(false);
        m_ui.menuRecent_Projects->addAction(m_recentFileActs[i]);
        connect(m_recentFileActs[i], SIGNAL(triggered()), this, SLOT(openRecentFileAction()));
    }
    m_ui.menuRecent_Projects->addSeparator();
    m_clearRecentFileAct = new QAction(tr("Clear Recent Projects"), this);
    connect(m_clearRecentFileAct, SIGNAL(triggered()), this, SLOT(clearRecentFilesAction()));
    m_ui.menuRecent_Projects->addAction(m_clearRecentFileAct);

#ifndef __APPLE__
    m_ui.menuFile->addSeparator();
    QAction* quitAction = m_ui.menuFile->addAction(tr("Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
#endif

    updateRecentFileActions();

    connect(m_undoStack, SIGNAL(cleanChanged(bool)), this, SLOT(cleanChanged(bool)));
    QAction* undoAction = m_undoStack->createUndoAction(this);
    undoAction->setShortcut(QKeySequence::Undo);
    m_ui.menuEdit->insertAction(m_ui.actionCut, undoAction);
    QAction* redoAction = m_undoStack->createRedoAction(this);
    redoAction->setShortcut(QKeySequence::Redo);
    m_ui.menuEdit->insertAction(m_ui.actionCut, redoAction);
    m_ui.menuEdit->insertSeparator(m_ui.actionCut);
    m_ui.actionCut->setShortcut(QKeySequence::Cut);
    m_ui.actionCopy->setShortcut(QKeySequence::Copy);
    m_ui.actionPaste->setShortcut(QKeySequence::Paste);
    m_ui.actionDelete->setShortcut(QKeySequence::Delete);
    onFocusChanged(nullptr, this);

    QGridLayout* faceLayout = new QGridLayout;
    QSvgWidget* faceSvg = new QSvgWidget(QStringLiteral(":/bg/FaceGrey.svg"));
    faceSvg->setGeometry(0, 0, 256, 256);
    faceSvg->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    faceLayout->addWidget(faceSvg);
    m_faceSvg = new QWidget;
    m_faceSvg->setLayout(faceLayout);
    m_ui.editorContents->addWidget(m_faceSvg);
    m_songGroupEditor = new SongGroupEditor;
    m_ui.editorContents->addWidget(m_songGroupEditor);
    m_soundGroupEditor = new SoundGroupEditor;
    m_ui.editorContents->addWidget(m_soundGroupEditor);
    m_soundMacroEditor = new SoundMacroEditor;
    m_ui.editorContents->addWidget(m_soundMacroEditor);
    m_adsrEditor = new ADSREditor;
    m_ui.editorContents->addWidget(m_adsrEditor);
    m_curveEditor = new CurveEditor;
    m_ui.editorContents->addWidget(m_curveEditor);
    m_keymapEditor = new KeymapEditor;
    m_ui.editorContents->addWidget(m_keymapEditor);
    m_layersEditor = new LayersEditor;
    m_ui.editorContents->addWidget(m_layersEditor);
    m_sampleEditor = new SampleEditor;
    m_ui.editorContents->addWidget(m_sampleEditor);
    m_ui.editorContents->setCurrentWidget(m_faceSvg);

    m_studioSetup = new StudioSetupWidget(this);
    connect(m_studioSetup, SIGNAL(hidden()), this, SLOT(studioSetupHidden()));
    connect(m_studioSetup, SIGNAL(shown()), this, SLOT(studioSetupShown()));

    connect(m_ui.actionNew_Subproject, SIGNAL(triggered()), this, SLOT(newSubprojectAction()));
    connect(m_ui.actionNew_SFX_Group, SIGNAL(triggered()), this, SLOT(newSFXGroupAction()));
    connect(m_ui.actionNew_Song_Group, SIGNAL(triggered()), this, SLOT(newSongGroupAction()));
    connect(m_ui.actionNew_Sound_Macro, SIGNAL(triggered()), this, SLOT(newSoundMacroAction()));
    connect(m_ui.actionNew_ADSR, SIGNAL(triggered()), this, SLOT(newADSRAction()));
    connect(m_ui.actionNew_Curve, SIGNAL(triggered()), this, SLOT(newCurveAction()));
    connect(m_ui.actionNew_Keymap, SIGNAL(triggered()), this, SLOT(newKeymapAction()));
    connect(m_ui.actionNew_Layers, SIGNAL(triggered()), this, SLOT(newLayersAction()));

    connect(m_ui.menuAudio, SIGNAL(aboutToShow()), this, SLOT(aboutToShowAudioIOMenu()));
    connect(m_ui.menuMIDI, SIGNAL(aboutToShow()), this, SLOT(aboutToShowMIDIIOMenu()));

    connect(qApp, SIGNAL(focusChanged(QWidget*,QWidget*)), this, SLOT(onFocusChanged(QWidget*,QWidget*)));

    m_voxEngine = boo::NewAudioVoiceEngine();
    m_voxAllocator = std::make_unique<VoiceAllocator>(*m_voxEngine);
    m_engine = std::make_unique<amuse::Engine>(*m_voxAllocator);
    m_voxEngine->setVolume(0.7f);
    m_studioSetup->loadData(m_engine->getDefaultStudio().get());

    m_ctrlVals[7] = 127;
    m_ctrlVals[10] = 64;

    startTimer(16);
}

MainWindow::~MainWindow()
{
    m_backgroundThread.quit();
    m_backgroundThread.wait();
    printf("IM DYING\n");
}

void MainWindow::connectMessenger(UIMessenger* messenger, Qt::ConnectionType type)
{
    connect(messenger, SIGNAL(information(const QString&,
                                  const QString&, QMessageBox::StandardButtons,
                                  QMessageBox::StandardButton)),
            this, SLOT(msgInformation(const QString&,
                           const QString&, QMessageBox::StandardButtons,
                           QMessageBox::StandardButton)), type);
    connect(messenger, SIGNAL(question(const QString&,
                                  const QString&, QMessageBox::StandardButtons,
                                  QMessageBox::StandardButton)),
            this, SLOT(msgQuestion(const QString&,
                           const QString&, QMessageBox::StandardButtons,
                           QMessageBox::StandardButton)), type);
    connect(messenger, SIGNAL(warning(const QString&,
                                  const QString&, QMessageBox::StandardButtons,
                                  QMessageBox::StandardButton)),
            this, SLOT(msgWarning(const QString&,
                           const QString&, QMessageBox::StandardButtons,
                           QMessageBox::StandardButton)), type);
    connect(messenger, SIGNAL(critical(const QString&,
                                  const QString&, QMessageBox::StandardButtons,
                                  QMessageBox::StandardButton)),
            this, SLOT(msgCritical(const QString&,
                           const QString&, QMessageBox::StandardButtons,
                           QMessageBox::StandardButton)), type);
}

void MainWindow::updateWindowTitle()
{
    if (!m_projectModel)
    {
        setWindowTitle(tr("Amuse[*]"));
        return;
    }

    QDir dir(m_projectModel->path());
    if (EditorWidget* w = getEditorWidget())
    {
        ProjectModel::BasePoolObjectNode* objNode = static_cast<ProjectModel::BasePoolObjectNode*>(w->currentNode());
        setWindowTitle(tr("%1/%2/%3[*] - Amuse").arg(dir.dirName()).arg(
            m_projectModel->getGroupNode(objNode)->text()).arg(objNode->text()));
        return;
    }

    setWindowTitle(tr("%1[*] - Amuse").arg(dir.dirName()));
}

void MainWindow::updateRecentFileActions()
{
    QSettings settings;
    QStringList files = settings.value("recentFileList").toStringList();

    int numRecentFiles = std::min(files.size(), int(MaxRecentFiles));

    for (int i = 0; i < numRecentFiles; ++i)
    {
        QString text = QStringLiteral("&%1 %2").arg(i + 1).arg(QDir(files[i]).dirName());
        m_recentFileActs[i]->setText(text);
        m_recentFileActs[i]->setData(files[i]);
        m_recentFileActs[i]->setVisible(true);
    }
    for (int j = numRecentFiles; j < MaxRecentFiles; ++j)
        m_recentFileActs[j]->setVisible(false);

    m_ui.menuRecent_Projects->setEnabled(numRecentFiles > 0);
}

bool MainWindow::setProjectPath(const QString& path)
{
    QDir dir(path);
    if (dir.path().isEmpty() || dir.path() == QStringLiteral(".") || dir.path() == QStringLiteral(".."))
    {
        QString msg = QString(tr("The directory at '%1' must not be empty.")).arg(path);
        QMessageBox::critical(this, tr("Directory empty"), msg);
        return false;
    }
    else if (!dir.exists())
    {
        QString msg = QString(tr("The directory at '%1' must exist for the Amuse editor.")).arg(path);
        QMessageBox::critical(this, tr("Directory does not exist"), msg);
        return false;
    }
    QString testWritePath = dir.filePath(tr("__amuse_test__"));
    QFile testWriteFile(testWritePath);
    if (!testWriteFile.open(QFile::ReadWrite))
    {

        QString msg = QString(tr("The directory at '%1' must be writable for the Amuse editor: %2")).arg(path).
            arg(testWriteFile.errorString());
        QMessageBox::critical(this, tr("Unable to write to directory"), msg);
        return false;
    }
    testWriteFile.remove();

    closeEditor();
    if (m_projectModel)
        m_projectModel->deleteLater();
    m_projectModel = new ProjectModel(path, this);
    m_filterProjectModel.setSourceModel(m_projectModel);
    m_ui.projectOutline->setModel(&m_filterProjectModel);
    connect(m_ui.projectOutline->selectionModel(),
            SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)),
            this, SLOT(onOutlineSelectionChanged(const QItemSelection&, const QItemSelection&)));
    m_ui.actionSave_Project->setEnabled(true);
    m_ui.actionRevert_Project->setEnabled(true);
    m_ui.actionReload_Sample_Data->setEnabled(true);
    m_ui.actionImport_Songs->setEnabled(true);
    m_ui.actionExport_GameCube_Groups->setEnabled(true);
    m_ui.actionNew_Subproject->setEnabled(true);
    setWindowFilePath(path);
    updateWindowTitle();
    updateFocus();
    m_undoStack->clear();

    QSettings settings;
    QStringList files = settings.value("recentFileList").toStringList();
    files.removeAll(dir.path());
    files.prepend(dir.path());
    while (files.size() > MaxRecentFiles)
        files.removeLast();
    settings.setValue("recentFileList", files);

    updateRecentFileActions();

    return true;
}

void MainWindow::refreshAudioIO()
{
    QList<QAction*> audioActions = m_ui.menuAudio->actions();
    if (audioActions.size() > 2)
        for (auto it = audioActions.begin() + 2 ; it != audioActions.end() ; ++it)
            m_ui.menuAudio->removeAction(*it);

    bool addedDev = false;
    if (m_voxEngine)
    {
        std::string curOut = m_voxEngine->getCurrentAudioOutput();
        for (const auto& dev : m_voxEngine->enumerateAudioOutputs())
        {
            QAction* act = m_ui.menuAudio->addAction(QString::fromStdString(dev.second));
            act->setCheckable(true);
            act->setData(QString::fromStdString(dev.first));
            if (curOut == dev.first)
                act->setChecked(true);
            connect(act, SIGNAL(triggered()), this, SLOT(setAudioIO()));
            addedDev = true;
        }
    }

    if (!addedDev)
        m_ui.menuAudio->addAction(tr("No Audio Devices Found"))->setEnabled(false);
}

void MainWindow::refreshMIDIIO()
{
    QList<QAction*> midiActions = m_ui.menuMIDI->actions();
    if (midiActions.size() > 2)
        for (auto it = midiActions.begin() + 2 ; it != midiActions.end() ; ++it)
            m_ui.menuMIDI->removeAction(*it);

    bool addedDev = false;
    if (m_voxEngine)
    {
        if (m_voxEngine->supportsVirtualMIDIIn())
        {
            QAction* act = m_ui.menuMIDI->addAction(tr("Virtual MIDI-In"));
            act->setCheckable(true);
            act->setData(QStringLiteral("<amuse-virtual-in>"));
            act->setChecked(static_cast<MIDIReader*>(m_engine->getMIDIReader())->hasVirtualIn());
            connect(act, SIGNAL(triggered(bool)), this, SLOT(setMIDIIO(bool)));
            addedDev = true;
        }
        for (const auto& dev : m_voxEngine->enumerateMIDIInputs())
        {
            QAction* act = m_ui.menuMIDI->addAction(QString::fromStdString(dev.second));
            act->setCheckable(true);
            act->setData(QString::fromStdString(dev.first));
            act->setChecked(static_cast<MIDIReader*>(m_engine->getMIDIReader())->hasMIDIIn(dev.first.c_str()));
            connect(act, SIGNAL(triggered(bool)), this, SLOT(setMIDIIO(bool)));
            addedDev = true;
        }
    }

    if (!addedDev)
        m_ui.menuMIDI->addAction(tr("No MIDI Devices Found"))->setEnabled(false);
}

void MainWindow::timerEvent(QTimerEvent* ev)
{
    if (m_voxEngine && m_engine)
    {
        m_voxEngine->pumpAndMixVoices();
        m_ui.statusbar->setVoiceCount(int(m_engine->getNumTotalActiveVoices()));
        if (m_engine->getActiveVoices().empty() && m_uiDisabled)
        {
            m_ui.projectOutline->setEnabled(true);
            if (EditorWidget* w = getEditorWidget())
                w->setEditorEnabled(true);
            m_ui.menubar->setEnabled(true);
            m_uiDisabled = false;
        }
        else if (!m_engine->getActiveVoices().empty() && !m_uiDisabled)
        {
            m_ui.projectOutline->setEnabled(false);
            if (EditorWidget* w = getEditorWidget())
                w->setEditorEnabled(false);
            m_ui.menubar->setEnabled(false);
            m_uiDisabled = true;
        }

        if (SampleEditor* sampleEditor = qobject_cast<SampleEditor*>(m_ui.editorContents->currentWidget()))
        {
            const auto& activeVoxs = m_engine->getActiveVoices();
            if (activeVoxs.size() && activeVoxs.back()->state() != amuse::VoiceState::Dead)
                sampleEditor->setSamplePos(activeVoxs.back()->getSamplePos());
            else
                sampleEditor->setSamplePos(-1);
        }

        QTableView* songTable = m_songGroupEditor->getSetupListView();
        for (int i = 0; i < songTable->model()->rowCount(); ++i)
            if (MIDIPlayerWidget* player = qobject_cast<MIDIPlayerWidget*>(
                songTable->indexWidget(songTable->model()->index(i, 1))))
                for (auto& p : m_engine->getActiveSequencers())
                    if (p.get() == player->sequencer() && p->state() != amuse::SequencerState::Playing)
                        player->stopped();

        QTableView* sfxTable = m_soundGroupEditor->getSFXListView();
        for (int i = 0; i < sfxTable->model()->rowCount(); ++i)
            if (SFXPlayerWidget* player = qobject_cast<SFXPlayerWidget*>(
                sfxTable->indexWidget(sfxTable->model()->index(i, 1))))
                if (player->voice() && player->voice()->state() != amuse::VoiceState::Playing)
                    player->stopped();
    }
}

void MainWindow::setSustain(bool sustain)
{
    if (sustain && m_ctrlVals[64] < 0x40)
    {
        m_ui.statusbar->setNormalMessage(tr("SUSTAIN"));
        for (auto& v : m_engine->getActiveVoices())
            v->setPedal(true);
        m_ctrlVals[64] = 127;
    }
    else if (!sustain && m_ctrlVals[64] >= 0x40)
    {
        m_ui.statusbar->setNormalMessage({});
        for (auto& v : m_engine->getActiveVoices())
            v->setPedal(false);
        m_ctrlVals[64] = 0;
    }
}

void MainWindow::keyPressEvent(QKeyEvent* ev)
{
    if (ev->key() == Qt::Key_Shift)
        setSustain(true);
    else if (ev->key() == Qt::Key_Escape)
        killSounds();
}

void MainWindow::keyReleaseEvent(QKeyEvent* ev)
{
    if (ev->key() == Qt::Key_Shift)
        setSustain(false);
}

void MainWindow::startBackgroundTask(int id, const QString& windowTitle, const QString& label,
                                     std::function<void(BackgroundTask&)>&& task)
{
    assert(m_backgroundTask == nullptr && "existing background process");
    setEnabled(false);

    m_backgroundTask = new BackgroundTask(id, std::move(task));
    m_backgroundTask->moveToThread(&m_backgroundThread);

    m_backgroundDialog = new QProgressDialog(this);
    m_backgroundDialog->setWindowTitle(windowTitle);
    m_backgroundDialog->setLabelText(label);
    m_backgroundDialog->setMinimumWidth(300);
    m_backgroundDialog->setAutoClose(false);
    m_backgroundDialog->setAutoReset(false);
    m_backgroundDialog->setMinimumDuration(0);
    m_backgroundDialog->setMinimum(0);
    m_backgroundDialog->setMaximum(0);
    m_backgroundDialog->setValue(0);

    connect(m_backgroundTask, SIGNAL(setMinimum(int)),
            m_backgroundDialog, SLOT(setMinimum(int)), Qt::QueuedConnection);
    connect(m_backgroundTask, SIGNAL(setMaximum(int)),
            m_backgroundDialog, SLOT(setMaximum(int)), Qt::QueuedConnection);
    connect(m_backgroundTask, SIGNAL(setValue(int)),
            m_backgroundDialog, SLOT(setValue(int)), Qt::QueuedConnection);
    connect(m_backgroundTask, SIGNAL(setLabelText(const QString&)),
            m_backgroundDialog, SLOT(setLabelText(const QString&)), Qt::QueuedConnection);
    connect(m_backgroundTask, SIGNAL(finished(int)),
            this, SLOT(onBackgroundTaskFinished(int)), Qt::QueuedConnection);
    m_backgroundDialog->open(m_backgroundTask, SLOT(cancel()));

    connectMessenger(&m_backgroundTask->uiMessenger(), Qt::BlockingQueuedConnection);

    QMetaObject::invokeMethod(m_backgroundTask, "run", Qt::QueuedConnection);
}

bool MainWindow::_setEditor(EditorWidget* editor)
{
    if (editor != m_ui.editorContents->currentWidget() &&
        m_ui.editorContents->currentWidget() != m_faceSvg)
        getEditorWidget()->unloadData();
    if (!editor || !editor->valid())
    {
        m_ui.editorContents->setCurrentWidget(m_faceSvg);
        updateWindowTitle();
        return false;
    }
    m_ui.editorContents->setCurrentWidget(editor);
    m_ui.editorContents->update();
    updateWindowTitle();
    return true;
}

bool MainWindow::openEditor(ProjectModel::SongGroupNode* node)
{
    return _setEditor(m_songGroupEditor->loadData(node) ? m_songGroupEditor : nullptr);
}

bool MainWindow::openEditor(ProjectModel::SoundGroupNode* node)
{
    return _setEditor(m_soundGroupEditor->loadData(node) ? m_soundGroupEditor : nullptr);
}

bool MainWindow::openEditor(ProjectModel::SoundMacroNode* node)
{
    return _setEditor(m_soundMacroEditor->loadData(node) ? m_soundMacroEditor : nullptr);
}

bool MainWindow::openEditor(ProjectModel::ADSRNode* node)
{
    return _setEditor(m_adsrEditor->loadData(node) ? m_adsrEditor : nullptr);
}

bool MainWindow::openEditor(ProjectModel::CurveNode* node)
{
    return _setEditor(m_curveEditor->loadData(node) ? m_curveEditor : nullptr);
}

bool MainWindow::openEditor(ProjectModel::KeymapNode* node)
{
    return _setEditor(m_keymapEditor->loadData(node) ? m_keymapEditor : nullptr);
}

bool MainWindow::openEditor(ProjectModel::LayersNode* node)
{
    return _setEditor(m_layersEditor->loadData(node) ? m_layersEditor : nullptr);
}

bool MainWindow::openEditor(ProjectModel::SampleNode* node)
{
    return _setEditor(m_sampleEditor->loadData(node) ? m_sampleEditor : nullptr);
}

bool MainWindow::openEditor(ProjectModel::INode* node)
{
    switch (node->type())
    {
    case ProjectModel::INode::Type::SongGroup:
        return openEditor(static_cast<ProjectModel::SongGroupNode*>(node));
    case ProjectModel::INode::Type::SoundGroup:
        return openEditor(static_cast<ProjectModel::SoundGroupNode*>(node));
    case ProjectModel::INode::Type::SoundMacro:
        return openEditor(static_cast<ProjectModel::SoundMacroNode*>(node));
    case ProjectModel::INode::Type::ADSR:
        return openEditor(static_cast<ProjectModel::ADSRNode*>(node));
    case ProjectModel::INode::Type::Curve:
        return openEditor(static_cast<ProjectModel::CurveNode*>(node));
    case ProjectModel::INode::Type::Keymap:
        return openEditor(static_cast<ProjectModel::KeymapNode*>(node));
    case ProjectModel::INode::Type::Layer:
        return openEditor(static_cast<ProjectModel::LayersNode*>(node));
    case ProjectModel::INode::Type::Sample:
        return openEditor(static_cast<ProjectModel::SampleNode*>(node));
    default:
        return false;
    }
}

void MainWindow::closeEditor()
{
    _setEditor(nullptr);
}

ProjectModel::INode* MainWindow::getEditorNode() const
{
    if (EditorWidget* w = getEditorWidget())
        return w->currentNode();
    return nullptr;
}

EditorWidget* MainWindow::getEditorWidget() const
{
    if (m_ui.editorContents->currentWidget() != m_faceSvg)
        return static_cast<EditorWidget*>(m_ui.editorContents->currentWidget());
    return nullptr;
}

amuse::ObjToken<amuse::Voice> MainWindow::startEditorVoice(uint8_t key, uint8_t velocity)
{
    amuse::ObjToken<amuse::Voice> vox;
    if (ProjectModel::INode* node = getEditorNode())
    {
        amuse::AudioGroupDatabase* group = projectModel()->getGroupNode(node)->getAudioGroup();
        if (node->type() == ProjectModel::INode::Type::SoundMacro)
        {
            ProjectModel::SoundMacroNode* cNode = static_cast<ProjectModel::SoundMacroNode*>(node);
            vox = m_engine->macroStart(group, cNode->id(), key, velocity, m_ctrlVals[1]);
        }
        else if (node->type() == ProjectModel::INode::Type::Keymap ||
                 node->type() == ProjectModel::INode::Type::Layer)
        {
            ProjectModel::BasePoolObjectNode* cNode = static_cast<ProjectModel::BasePoolObjectNode*>(node);
            vox = m_engine->pageObjectStart(group, cNode->id(), key, velocity, m_ctrlVals[1]);
        }
        else if (node->type() == ProjectModel::INode::Type::Sample)
        {
            SampleEditor* editor = static_cast<SampleEditor*>(m_ui.editorContents->currentWidget());
            vox = m_engine->macroStart(group, editor->soundMacro(), key, velocity, m_ctrlVals[1]);
        }
        if (vox)
        {
            vox->setPedal(m_ctrlVals[64] >= 0x40);
            vox->setPitchWheel(m_pitch);
            vox->installCtrlValues(m_ctrlVals);
            vox->setReverbVol(m_auxAVol);
            vox->setAuxBVol(m_auxBVol);
        }
    }
    return vox;
}

amuse::ObjToken<amuse::Voice> MainWindow::startSFX(amuse::GroupId groupId, amuse::SFXId sfxId)
{
    if (ProjectModel::INode* node = getEditorNode())
    {
        amuse::AudioGroupDatabase* group = projectModel()->getGroupNode(node)->getAudioGroup();
        auto ret = m_engine->fxStart(group, groupId, sfxId, 1.f, 0.f);
        ret->setReverbVol(m_auxAVol);
        ret->setAuxBVol(m_auxBVol);
        return ret;
    }
    return {};
}

amuse::ObjToken<amuse::Sequencer> MainWindow::startSong(amuse::GroupId groupId, amuse::SongId songId,
                                                        const unsigned char* arrData)
{
    if (ProjectModel::INode* node = getEditorNode())
    {
        amuse::AudioGroupDatabase* group = projectModel()->getGroupNode(node)->getAudioGroup();
        auto ret = m_engine->seqPlay(group, groupId, songId, arrData);
        for (uint8_t i = 0; i < 16; ++i)
        {
            ret->setCtrlValue(i, 0x5b, int8_t(m_auxAVol * 127.f));
            ret->setCtrlValue(i, 0x5d, int8_t(m_auxBVol * 127.f));
        }
        return ret;
    }
    return {};
}

void MainWindow::pushUndoCommand(QUndoCommand* cmd)
{
    m_undoStack->push(cmd);
}

void MainWindow::updateFocus()
{
    onFocusChanged(nullptr, focusWidget());
}

void MainWindow::aboutToDeleteNode(ProjectModel::INode* node)
{
    if (getEditorNode() == node)
        closeEditor();
}

void MainWindow::closeEvent(QCloseEvent* ev)
{
    if (!m_projectModel)
    {
        ev->accept();
        return;
    }

    if (!m_undoStack->isClean())
    {
        QDir dir(m_projectModel->path());
        int result = QMessageBox::question(this, tr("Unsaved Changes"), tr("Save Changes in %1?").arg(dir.dirName()),
                     QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
        if (result == QMessageBox::Save)
        {
            saveAction();
            ev->accept();
        }
        else if (result == QMessageBox::Discard)
        {
            ev->accept();
        }
        else
        {
            ev->ignore();
        }
    }
    else
    {
        ev->accept();
    }
}

void MainWindow::showEvent(QShowEvent* ev)
{
    m_studioSetup->updateWindowPosition();
}

void MainWindow::newAction()
{
    QString path = QFileDialog::getSaveFileName(this, tr("New Project"));
    if (path.isEmpty())
        return;
    if (!MkPath(path, m_mainMessenger))
        return;
    if (!setProjectPath(path))
        return;

    m_projectModel->clearProjectData();
    bool hasGroups = m_projectModel->ensureModelData();
    m_ui.actionImport_Groups->setDisabled(hasGroups);
    m_ui.actionImport_Songs->setEnabled(hasGroups);
}

bool MainWindow::openProject(const QString& path)
{
    QDir dir(path);
    if (dir.path().isEmpty() || dir.path() == QStringLiteral(".") || dir.path() == QStringLiteral(".."))
    {
        QString msg = QString(tr("The directory at '%1' must not be empty.")).arg(path);
        QMessageBox::critical(this, tr("Directory empty"), msg);
        return false;
    }
    else if (!dir.exists())
    {
        QString msg = QString(tr("The directory at '%1' does not exist.")).arg(path);
        QMessageBox::critical(this, tr("Bad Directory"), msg);
        return false;
    }

    if (QFileInfo(dir, QStringLiteral("!project.yaml")).exists() &&
        QFileInfo(dir, QStringLiteral("!pool.yaml")).exists())
        dir.cdUp();

    if (m_projectModel && m_projectModel->path() == dir.path())
        return true;

    if (!setProjectPath(dir.path()))
        return false;

    ProjectModel* model = m_projectModel;
    startBackgroundTask(TaskOpen, tr("Opening"), tr("Scanning Project"),
    [dir, model](BackgroundTask& task)
    {
        QStringList childDirs = dir.entryList(QDir::Dirs);
        for (const auto& chDir : childDirs)
        {
            if (task.isCanceled())
                return;
            if (chDir == QStringLiteral("out"))
                continue;
            QString chPath = dir.filePath(chDir);
            if (QFileInfo(chPath, QStringLiteral("!project.yaml")).exists() &&
                QFileInfo(chPath, QStringLiteral("!pool.yaml")).exists())
            {
                task.setLabelText(tr("Opening %1").arg(chDir));
                if (!model->openGroupData(chDir, task.uiMessenger()))
                    return;
            }
        }
        model->openSongsData();
    });

    return true;
}

void MainWindow::openAction()
{
    QString path = QFileDialog::getExistingDirectory(this, tr("Open Project"));
    if (path.isEmpty())
        return;
    openProject(path);
}

void MainWindow::openRecentFileAction()
{
    if (QAction *action = qobject_cast<QAction *>(sender()))
        if (!openProject(action->data().toString()))
        {
            QString path = action->data().toString();
            QSettings settings;
            QStringList files = settings.value("recentFileList").toStringList();
            files.removeAll(path);
            settings.setValue("recentFileList", files);
            updateRecentFileActions();
        }
}

void MainWindow::clearRecentFilesAction()
{
    QSettings settings;
    settings.setValue("recentFileList", QStringList());
    updateRecentFileActions();
}

void MainWindow::saveAction()
{
    if (m_projectModel)
    {
        m_projectModel->saveToFile(m_mainMessenger);
        m_undoStack->setClean();
    }
}

void MainWindow::revertAction()
{
    QString path = m_projectModel->path();
    closeEditor();
    m_undoStack->clear();
    delete m_projectModel;
    m_projectModel = nullptr;
    openProject(path);
}

void MainWindow::reloadSampleDataAction()
{
    closeEditor();

    ProjectModel* model = m_projectModel;
    if (!m_projectModel)
        return;

    QDir dir(m_projectModel->path());
    if (!dir.exists())
        return;

    startBackgroundTask(TaskReloadSamples, tr("Reloading Samples"), tr("Scanning Project"),
    [dir, model](BackgroundTask& task)
    {
        QStringList childDirs = dir.entryList(QDir::Dirs);
        for (const auto& chDir : childDirs)
        {
            if (task.isCanceled())
                return;
            if (chDir == QStringLiteral("out"))
                continue;
            QString chPath = dir.filePath(chDir);
            if (QFileInfo(chPath, QStringLiteral("!project.yaml")).exists() &&
                QFileInfo(chPath, QStringLiteral("!pool.yaml")).exists())
            {
                task.setLabelText(tr("Scanning %1").arg(chDir));
                if (!model->reloadSampleData(chDir, task.uiMessenger()))
                    return;
            }
        }
    });
}

void MainWindow::importAction()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Import Project"),
        m_projectModel ? m_projectModel->dir().path() : QString());
    if (path.isEmpty())
        return;

    /* Validate input file */
    amuse::ContainerRegistry::Type tp =
        amuse::ContainerRegistry::DetectContainerType(QStringToSysString(path).c_str());
    if (tp == amuse::ContainerRegistry::Type::Invalid)
    {
        QString msg = QString(tr("The file at '%1' could not be interpreted as a MusyX container.")).arg(path);
        QMessageBox::critical(this, tr("Unsupported MusyX Container"), msg);
        return;
    }

    /* Ask user about sample conversion */
    int impMode = QMessageBox::question(this, tr("Sample Import Mode"),
        tr("Amuse can import samples as WAV files for ease of editing, "
           "import original compressed data for lossless repacking, or both. "
           "Exporting the project will prefer whichever version was modified "
           "most recently."),
        tr("Import Compressed"), tr("Import WAVs"), tr("Import Both"));

    switch (impMode)
    {
    case 0:
    case 1:
    case 2:
        break;
    default:
        return;
    }
    ProjectModel::ImportMode importMode = ProjectModel::ImportMode(impMode);

    /* Special handling for raw groups - gather sibling groups in filesystem */
    if (tp == amuse::ContainerRegistry::Type::Raw4)
    {
        int scanMode = QMessageBox::question(this, tr("Raw Import Mode"),
                                             tr("Would you like to scan for all MusyX group files in this directory?"),
                                             QMessageBox::Yes, QMessageBox::No);
        if (scanMode == QMessageBox::Yes)
        {
            /* Auto-create project */
            if (m_projectModel == nullptr)
            {
                QString newName;
                bool ok = true;
                while (ok && newName.isEmpty())
                    newName = QInputDialog::getText(this, tr("Project Name"), tr("What should this project be named?"),
                                                    QLineEdit::Normal, QString(), &ok);
                if (!ok)
                    return;

                QFileInfo fInfo(path);
                QString newPath = QFileInfo(fInfo.dir(), newName).filePath();
                if (!MkPath(fInfo.dir(), newName, m_mainMessenger))
                    return;
                if (!setProjectPath(newPath))
                    return;
            }

            ProjectModel* model = m_projectModel;
            startBackgroundTask(TaskImport, tr("Importing"), tr("Scanning Project"),
            [model, path, importMode](BackgroundTask& task)
            {
                QDir dir = QFileInfo(path).dir();
                QStringList filters;
                filters << "*.proj" << "*.pro";
                QStringList files = dir.entryList(filters, QDir::Files);
                for (const QString& fPath : files)
                {
                    auto data = amuse::ContainerRegistry::LoadContainer(QStringToSysString(dir.filePath(fPath)).c_str());
                    for (auto& p : data)
                    {
                        task.setLabelText(tr("Importing %1").arg(SysStringToQString(p.first)));
                        if (task.isCanceled())
                            return;
                        if (!model->importGroupData(SysStringToQString(p.first), p.second,
                                                    importMode, task.uiMessenger()))
                            return;
                    }
                }
            });


            return;
        }
        else if (scanMode == QMessageBox::No)
        {}
        else
        {
            return;
        }
    }

    /* Auto-create project */
    if (m_projectModel == nullptr)
    {
        QFileInfo fInfo(path);
        QString newPath = QFileInfo(fInfo.dir(), fInfo.completeBaseName()).filePath();
        if (!MkPath(fInfo.dir(), fInfo.completeBaseName(), m_mainMessenger))
            return;
        if (!setProjectPath(newPath))
            return;
    }

    ProjectModel* model = m_projectModel;
    startBackgroundTask(TaskImport, tr("Importing"), tr("Scanning Project"),
    [model, path, importMode](BackgroundTask& task)
    {
        /* Handle single container */
        auto data = amuse::ContainerRegistry::LoadContainer(QStringToSysString(path).c_str());
        task.setMaximum(int(data.size()));
        int curVal = 0;
        for (auto& p : data)
        {
            task.setLabelText(tr("Importing %1").arg(SysStringToQString(p.first)));
            if (task.isCanceled())
                return;
            if (!model->importGroupData(SysStringToQString(p.first), p.second,
                                        importMode, task.uiMessenger()))
                return;
            task.setValue(++curVal);
        }
    });
}

void MainWindow::importSongsAction()
{
    if (!m_projectModel)
        return;

    QString path = QFileDialog::getOpenFileName(this, tr("Import Songs"), m_projectModel->dir().path());
    if (path.isEmpty())
        return;

    closeEditor();
    m_projectModel->importSongsData(path);
}

void MainWindow::exportAction()
{
    if (!m_projectModel)
        return;

    QFileInfo dirInfo(m_projectModel->dir(), QStringLiteral("out"));
    if (!MkPath(dirInfo.filePath(), m_mainMessenger))
        return;

    QDir dir(dirInfo.filePath());

    ProjectModel* model = m_projectModel;
    startBackgroundTask(BackgroundTaskId::TaskExport, tr("Exporting"), tr("Scanning Project"),
    [model, dir](BackgroundTask& task)
    {
        QStringList groupList = model->getGroupList();
        task.setMaximum(groupList.size());
        int curVal = 0;
        for (QString group : groupList)
        {
            task.setLabelText(tr("Exporting %1").arg(group));
            if (task.isCanceled())
                return;
            if (!model->exportGroup(dir.path(), group, task.uiMessenger()))
                return;
            task.setValue(++curVal);
        }
    });
}

bool TreeDelegate::editorEvent(QEvent* event,
                               QAbstractItemModel* _model,
                               const QStyleOptionViewItem& option,
                               const QModelIndex& index)
{
    ProjectModel* model = static_cast<ProjectModel*>(_model);
    ProjectModel::INode* node = model->node(index);
    if (!node)
        return false;

#if 0
    if ((event->type() == QEvent::MouseButtonDblClick &&
         static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton))
    {
        // Open in editor
        return m_window.openEditor(node);
    }
#endif

    return false;
}

QString MainWindow::getGroupName(ProjectModel::GroupNode* group) const
{
    if (group)
        return group->text();
    return {};
}

ProjectModel::GroupNode* MainWindow::getSelectedGroupNode() const
{
    if (!m_projectModel)
        return nullptr;
    if (!m_ui.projectOutline->selectionModel()->currentIndex().isValid())
        return nullptr;
    return m_projectModel->getGroupNode(m_projectModel->node(m_ui.projectOutline->selectionModel()->currentIndex()));
}

QString MainWindow::getSelectedGroupName() const
{
    return getGroupName(getSelectedGroupNode());
}

void MainWindow::_recursiveExpandOutline(const QModelIndex& filterIndex) const
{
    if (filterIndex.isValid())
    {
        _recursiveExpandOutline(filterIndex.parent());
        m_ui.projectOutline->expand(filterIndex);
    }
}

void MainWindow::recursiveExpandAndSelectOutline(const QModelIndex& index) const
{
    QModelIndex filterIndex = m_filterProjectModel.mapFromSource(index);
    _recursiveExpandOutline(filterIndex);
    if (filterIndex.isValid())
    {
        m_ui.projectOutline->selectionModel()->setCurrentIndex(filterIndex, QItemSelectionModel::ClearAndSelect);
        m_ui.projectOutline->selectionModel()->setCurrentIndex(filterIndex, QItemSelectionModel::Current);
    }
}

void MainWindow::newSubprojectAction()
{
    QString newName;
    bool ok = true;
    while (ok && newName.isEmpty())
        newName = QInputDialog::getText(this, tr("New Subproject"),
            tr("What should this subproject be named?"), QLineEdit::Normal, QString(), &ok);
    if (!ok)
        return;

    ProjectModel::GroupNode* node = m_projectModel->newSubproject(newName);
}

void MainWindow::newSFXGroupAction()
{
    ProjectModel::GroupNode* group = getSelectedGroupNode();
    m_projectModel->setIdDatabases(group);
    QString groupName = getGroupName(group);
    QString newName;
    bool ok = true;
    while (ok && newName.isEmpty())
        newName = QInputDialog::getText(this, tr("New SFX Group"),
            tr("What should the new SFX group in %1 be named?").arg(groupName), QLineEdit::Normal, QString::fromStdString(
                amuse::GroupId::CurNameDB->generateDefaultName(amuse::NameDB::Type::Group)), &ok);
    if (!ok)
        return;

    ProjectModel::SoundGroupNode* node = m_projectModel->newSoundGroup(group, newName);
    if (node)
        openEditor(node);
}

void MainWindow::newSongGroupAction()
{
    ProjectModel::GroupNode* group = getSelectedGroupNode();
    m_projectModel->setIdDatabases(group);
    QString groupName = getGroupName(group);
    QString newName;
    bool ok = true;
    while (ok && newName.isEmpty())
        newName = QInputDialog::getText(this, tr("New Song Group"),
            tr("What should the new Song group in %1 be named?").arg(groupName), QLineEdit::Normal, QString::fromStdString(
                amuse::GroupId::CurNameDB->generateDefaultName(amuse::NameDB::Type::Group)), &ok);
    if (!ok)
        return;

    ProjectModel::SongGroupNode* node = m_projectModel->newSongGroup(group, newName);
    if (node)
        openEditor(node);
}

void MainWindow::newSoundMacroAction()
{
    ProjectModel::GroupNode* group = getSelectedGroupNode();
    m_projectModel->setIdDatabases(group);
    QString groupName = getGroupName(group);
    QString newName;
    const SoundMacroTemplateEntry* templ = nullptr;
    int result = QDialog::Accepted;
    while (result == QDialog::Accepted && newName.isEmpty())
    {
        NewSoundMacroDialog dialog(groupName, this);
        result = dialog.exec();
        newName = dialog.getName();
        templ = dialog.getSelectedTemplate();
    }
    if (result == QDialog::Rejected)
        return;

    ProjectModel::SoundMacroNode* node = m_projectModel->newSoundMacro(group, newName, templ);
    if (node)
        openEditor(node);
}

void MainWindow::newADSRAction()
{
    ProjectModel::GroupNode* group = getSelectedGroupNode();
    m_projectModel->setIdDatabases(group);
    QString groupName = getGroupName(group);
    QString newName;
    bool ok = true;
    while (ok && newName.isEmpty())
        newName = QInputDialog::getText(this, tr("New ADSR"),
            tr("What should the new ADSR in %1 be named?").arg(groupName), QLineEdit::Normal, QString::fromStdString(
                amuse::TableId::CurNameDB->generateDefaultName(amuse::NameDB::Type::Table)), &ok);
    if (!ok)
        return;

    ProjectModel::ADSRNode* node = m_projectModel->newADSR(group, newName);
    if (node)
        openEditor(node);
}

void MainWindow::newCurveAction()
{
    ProjectModel::GroupNode* group = getSelectedGroupNode();
    m_projectModel->setIdDatabases(group);
    QString groupName = getGroupName(group);
    QString newName;
    bool ok = true;
    while (ok && newName.isEmpty())
        newName = QInputDialog::getText(this, tr("New Curve"),
            tr("What should the new Curve in %1 be named?").arg(groupName), QLineEdit::Normal, QString::fromStdString(
                amuse::TableId::CurNameDB->generateDefaultName(amuse::NameDB::Type::Table)), &ok);
    if (!ok)
        return;

    ProjectModel::CurveNode* node = m_projectModel->newCurve(group, newName);
    if (node)
        openEditor(node);
}

void MainWindow::newKeymapAction()
{
    ProjectModel::GroupNode* group = getSelectedGroupNode();
    m_projectModel->setIdDatabases(group);
    QString groupName = getGroupName(group);
    QString newName;
    bool ok = true;
    while (ok && newName.isEmpty())
        newName = QInputDialog::getText(this, tr("New Keymap"),
            tr("What should the new Keymap in %1 be named?").arg(groupName), QLineEdit::Normal, QString::fromStdString(
                amuse::KeymapId::CurNameDB->generateDefaultName(amuse::NameDB::Type::Keymap)), &ok);
    if (!ok)
        return;

    ProjectModel::KeymapNode* node = m_projectModel->newKeymap(group, newName);
    if (node)
        openEditor(node);
}

void MainWindow::newLayersAction()
{
    ProjectModel::GroupNode* group = getSelectedGroupNode();
    m_projectModel->setIdDatabases(group);
    QString groupName = getGroupName(group);
    QString newName;
    bool ok = true;
    while (ok && newName.isEmpty())
        newName = QInputDialog::getText(this, tr("New Layers"),
            tr("What should the new Layers in %1 be named?").arg(groupName), QLineEdit::Normal, QString::fromStdString(
                amuse::LayersId::CurNameDB->generateDefaultName(amuse::NameDB::Type::Layer)), &ok);
    if (!ok)
        return;

    ProjectModel::LayersNode* node = m_projectModel->newLayers(group, newName);
    if (node)
        openEditor(node);
}

void MainWindow::aboutToShowAudioIOMenu()
{
    refreshAudioIO();
}

void MainWindow::aboutToShowMIDIIOMenu()
{
    refreshMIDIIO();
}

void MainWindow::setAudioIO()
{
    QByteArray devName = qobject_cast<QAction*>(sender())->data().toString().toUtf8();
    if (m_voxEngine)
        m_voxEngine->setCurrentAudioOutput(devName.data());
}

void MainWindow::setMIDIIO(bool checked)
{
    QAction* action = qobject_cast<QAction*>(sender());
    QByteArray devName = action->data().toString().toUtf8();
    if (m_voxEngine)
    {
        MIDIReader* mr = static_cast<MIDIReader*>(m_engine->getMIDIReader());
        if (devName == "<amuse-virtual-in>")
        {
            mr->setVirtualIn(checked);
            action->setChecked(mr->hasVirtualIn());
        }
        else
        {
            if (checked)
                mr->addMIDIIn(devName.data());
            else
                mr->removeMIDIIn(devName.data());
            action->setChecked(mr->hasMIDIIn(devName.data()));
        }
    }
}

void MainWindow::notePressed(int key)
{
    if (m_engine)
    {
        if (m_lastSound)
            m_lastSound->keyOff();
        m_lastSound = startEditorVoice(key, m_velocity);
    }
}

void MainWindow::noteReleased()
{
    if (m_lastSound)
    {
        m_lastSound->keyOff();
        m_lastSound.reset();
    }
}

void MainWindow::velocityChanged(int vel)
{
    m_velocity = vel;
}

void MainWindow::modulationChanged(int mod)
{
    m_ctrlVals[1] = int8_t(mod);
    for (auto& v : m_engine->getActiveVoices())
        v->setCtrlValue(1, m_ctrlVals[1]);
}

void MainWindow::pitchChanged(int pitch)
{
    m_pitch = pitch / 2048.f;
    for (auto& v : m_engine->getActiveVoices())
        v->setPitchWheel(m_pitch);
}

void MainWindow::killSounds()
{
    for (auto& v : m_engine->getActiveVoices())
        v->kill();
}

void MainWindow::fxPressed()
{
    if (m_studioSetup->isVisible())
        m_studioSetup->hide();
    else
        m_studioSetup->show();
}

void MainWindow::volumeChanged(int vol)
{
    m_engine->setVolume(vol / 100.f);
}

void MainWindow::auxAChanged(int vol)
{
    m_auxAVol = vol / 100.f;
    for (auto& vox : m_engine->getActiveVoices())
        vox->setReverbVol(m_auxAVol);
    for (auto& seq : m_engine->getActiveSequencers())
        for (uint8_t i = 0; i < 16; ++i)
            seq->setCtrlValue(i, 0x5b, int8_t(m_auxAVol * 127.f));
}

void MainWindow::auxBChanged(int vol)
{
    m_auxBVol = vol / 100.f;
    for (auto& vox : m_engine->getActiveVoices())
        vox->setAuxBVol(m_auxBVol);
    for (auto& seq : m_engine->getActiveSequencers())
        for (uint8_t i = 0; i < 16; ++i)
            seq->setCtrlValue(i, 0x5d, int8_t(m_auxBVol * 127.f));
}

void MainWindow::outlineCutAction()
{

}

void MainWindow::outlineCopyAction()
{

}

void MainWindow::outlinePasteAction()
{

}

void MainWindow::outlineDeleteAction()
{
    if (!m_projectModel)
        return;
    QModelIndexList indexes = m_ui.projectOutline->selectionModel()->selectedIndexes();
    if (indexes.empty())
        return;
    m_projectModel->del(m_filterProjectModel.mapToSource(indexes.front()));
}

void MainWindow::onFocusChanged(QWidget* old, QWidget* now)
{
    disconnect(m_cutConn);
    disconnect(m_copyConn);
    disconnect(m_pasteConn);
    disconnect(m_deleteConn);
    disconnect(m_canEditConn);

    if (QLineEdit* le = qobject_cast<QLineEdit*>(now))
    {
        m_cutConn = connect(m_ui.actionCut, SIGNAL(triggered()), le, SLOT(cut()));
        m_ui.actionCut->setEnabled(le->hasSelectedText());
        m_copyConn = connect(m_ui.actionCopy, SIGNAL(triggered()), le, SLOT(copy()));
        m_ui.actionCopy->setEnabled(le->hasSelectedText());
        m_pasteConn = connect(m_ui.actionPaste, SIGNAL(triggered()), le, SLOT(paste()));
        m_ui.actionPaste->setEnabled(true);
        m_deleteConn = connect(m_ui.actionDelete, SIGNAL(triggered()), this, SLOT(onTextDelete()));
        m_ui.actionDelete->setEnabled(true);
        m_canEditConn = connect(le, SIGNAL(selectionChanged()), this, SLOT(onTextSelect()));
        return;
    }

    if (now == m_ui.projectOutline || m_ui.projectOutline->isAncestorOf(now))
    {
        setItemEditEnabled(canEditOutline());
        if (m_projectModel)
        {
            m_cutConn = connect(m_ui.actionCut, SIGNAL(triggered()), this, SLOT(outlineCutAction()));
            m_copyConn = connect(m_ui.actionCopy, SIGNAL(triggered()), this, SLOT(outlineCopyAction()));
            m_pasteConn = connect(m_ui.actionPaste, SIGNAL(triggered()), this, SLOT(outlinePasteAction()));
            m_deleteConn = connect(m_ui.actionDelete, SIGNAL(triggered()), this, SLOT(outlineDeleteAction()));
        }
    }
    else if (now == m_ui.editorContents || m_ui.editorContents->isAncestorOf(now))
    {
        setItemEditEnabled(false);
        if (EditorWidget* editor = getEditorWidget())
        {
            if (editor->isItemEditEnabled())
            {
                setItemEditEnabled(true);
                m_cutConn = connect(m_ui.actionCut, SIGNAL(triggered()), editor, SLOT(itemCutAction()));
                m_copyConn = connect(m_ui.actionCopy, SIGNAL(triggered()), editor, SLOT(itemCopyAction()));
                m_pasteConn = connect(m_ui.actionPaste, SIGNAL(triggered()), editor, SLOT(itemPasteAction()));
                m_deleteConn = connect(m_ui.actionDelete, SIGNAL(triggered()), editor, SLOT(itemDeleteAction()));
            }
        }
    }

}

void MainWindow::outlineItemActivated(const QModelIndex& index)
{
    ProjectModel::INode* node = m_projectModel->node(m_filterProjectModel.mapToSource(index));
    if (!node)
        return;
    openEditor(node);
}

void MainWindow::setItemEditEnabled(bool enabled)
{
    m_ui.actionCut->setEnabled(enabled);
    m_ui.actionCopy->setEnabled(enabled);
    m_ui.actionPaste->setEnabled(enabled);
    m_ui.actionDelete->setEnabled(enabled);
}

void MainWindow::setItemNewEnabled(bool enabled)
{
    m_ui.actionNew_SFX_Group->setEnabled(enabled);
    m_ui.actionNew_Song_Group->setEnabled(enabled);
    m_ui.actionNew_Sound_Macro->setEnabled(enabled);
    m_ui.actionNew_ADSR->setEnabled(enabled);
    m_ui.actionNew_Curve->setEnabled(enabled);
    m_ui.actionNew_Keymap->setEnabled(enabled);
    m_ui.actionNew_Layers->setEnabled(enabled);
}

bool MainWindow::canEditOutline()
{
    if (!m_projectModel)
        return false;
    QModelIndex curIndex = m_ui.projectOutline->selectionModel()->currentIndex();
    if (!curIndex.isValid())
        return false;
    return m_projectModel->canEdit(m_filterProjectModel.mapToSource(curIndex));
}

void MainWindow::onOutlineSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    if (!m_projectModel)
        return;
    setItemNewEnabled(m_ui.projectOutline->selectionModel()->currentIndex().isValid());
    if (selected.indexes().empty())
        setItemEditEnabled(false);
    else
        setItemEditEnabled(m_projectModel->canEdit(m_filterProjectModel.mapToSource(selected.indexes().front())));
}

void MainWindow::onTextSelect()
{
    if (QLineEdit* le = qobject_cast<QLineEdit*>(sender()))
    {
        m_ui.actionCut->setEnabled(le->hasSelectedText());
        m_ui.actionCopy->setEnabled(le->hasSelectedText());
    }
}

void MainWindow::onTextDelete()
{
    if (QLineEdit* le = qobject_cast<QLineEdit*>(focusWidget()))
    {
        le->del();
    }
}

void MainWindow::cleanChanged(bool clean)
{
    setWindowModified(!clean);
}

void MainWindow::studioSetupHidden()
{
    m_ui.statusbar->setFXDown(false);
}

void MainWindow::studioSetupShown()
{
    m_ui.statusbar->setFXDown(true);
}

void MainWindow::onBackgroundTaskFinished(int id)
{
    m_backgroundDialog->reset();
    m_backgroundDialog->deleteLater();
    m_backgroundDialog = nullptr;
    m_backgroundTask->deleteLater();
    m_backgroundTask = nullptr;

    if (id == TaskExport)
    {
        if (m_mainMessenger.question(tr("Export Complete"), tr("%1?").
            arg(ShowInGraphicalShellString())) == QMessageBox::Yes)
        {
            QFileInfo dirInfo(m_projectModel->dir(), QStringLiteral("out"));
            QDir dir(dirInfo.filePath());
            QStringList entryList = dir.entryList(QDir::Files);
            ShowInGraphicalShell(this, entryList.empty() ? dirInfo.filePath() : QFileInfo(dir, entryList.first()).filePath());
        }
    }
    else
    {
        bool hasGroups = m_projectModel->ensureModelData();
        m_ui.actionImport_Groups->setDisabled(hasGroups);
        m_ui.actionImport_Songs->setEnabled(hasGroups);
    }

    setEnabled(true);
}

QMessageBox::StandardButton MainWindow::msgInformation(const QString &title,
    const QString &text, QMessageBox::StandardButtons buttons,
    QMessageBox::StandardButton defaultButton)
{
    return QMessageBox::information(this, title, text, buttons, defaultButton);
}

QMessageBox::StandardButton MainWindow::msgQuestion(const QString &title,
    const QString &text, QMessageBox::StandardButtons buttons,
    QMessageBox::StandardButton defaultButton)
{
    return QMessageBox::question(this, title, text, buttons, defaultButton);
}

QMessageBox::StandardButton MainWindow::msgWarning(const QString &title,
    const QString &text, QMessageBox::StandardButtons buttons,
    QMessageBox::StandardButton defaultButton)
{
    return QMessageBox::warning(this, title, text, buttons, defaultButton);
}

QMessageBox::StandardButton MainWindow::msgCritical(const QString &title,
    const QString &text, QMessageBox::StandardButtons buttons,
    QMessageBox::StandardButton defaultButton)
{
    return QMessageBox::critical(this, title, text, buttons, defaultButton);
}
