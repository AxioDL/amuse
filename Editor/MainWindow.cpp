#include "MainWindow.hpp"
#include <QFileDialog>
#include <QMessageBox>
#include <QLineEdit>
#include <QInputDialog>
#include <QProgressDialog>
#include <QMouseEvent>
#include <QClipboard>
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
  m_navIt(m_navList.begin()),
  m_treeDelegate(*this, this),
  m_mainMessenger(this),
  m_fileDialog(this),
  m_undoStack(new QUndoStack(this)),
  m_backgroundThread(this)
{
    m_backgroundThread.start();

    m_ui.setupUi(this);
    m_ui.splitter->setCollapsible(1, false);
    QPalette palette = m_ui.projectOutlineFilter->palette();
    palette.setColor(QPalette::Base, palette.color(QPalette::Background));
    m_ui.projectOutlineFilter->setPalette(palette);
    m_ui.projectOutline->setItemDelegate(&m_treeDelegate);
    connect(m_ui.projectOutline, SIGNAL(activated(const QModelIndex&)),
            this, SLOT(outlineItemActivated(const QModelIndex&)));
    m_goBack = new QAction(m_ui.backButton->icon(), tr("Go Back"), this);
    m_goBack->setEnabled(false);
    connect(m_goBack, SIGNAL(triggered()), this, SLOT(goBack()));
    m_goBack->setShortcut(QKeySequence::Back);
    m_ui.backButton->setDefaultAction(m_goBack);
    m_goForward = new QAction(m_ui.forwardButton->icon(), tr("Go Forward"), this);
    m_goForward->setEnabled(false);
    connect(m_goForward, SIGNAL(triggered()), this, SLOT(goForward()));
    m_goForward->setShortcut(QKeySequence::Forward);
    m_ui.forwardButton->setDefaultAction(m_goForward);

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
    connect(m_ui.actionImport_C_Headers, SIGNAL(triggered()), this, SLOT(importHeadersAction()));
    connect(m_ui.actionExport_C_Headers, SIGNAL(triggered()), this, SLOT(exportHeadersAction()));
    m_ui.actionQuit->setShortcut(QKeySequence::Quit);
    connect(m_ui.actionQuit, SIGNAL(triggered()), this, SLOT(close()));

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

    updateRecentFileActions();

    connect(m_undoStack, SIGNAL(cleanChanged(bool)), this, SLOT(cleanChanged(bool)));
    QAction* undoAction = m_undoStack->createUndoAction(this);
    undoAction->setShortcut(QKeySequence::Undo);
    undoAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-undo")));
    m_ui.menuEdit->insertAction(m_ui.actionCut, undoAction);
    QAction* redoAction = m_undoStack->createRedoAction(this);
    redoAction->setShortcut(QKeySequence::Redo);
    redoAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-redo")));
    m_ui.menuEdit->insertAction(m_ui.actionCut, redoAction);
    m_ui.menuEdit->insertSeparator(m_ui.actionCut);
    m_ui.actionCut->setShortcut(QKeySequence::Cut);
    m_ui.actionCopy->setShortcut(QKeySequence::Copy);
    m_ui.actionPaste->setShortcut(QKeySequence::Paste);
    m_ui.actionDelete->setShortcut(QKeySequence::Delete);
    onFocusChanged(nullptr, this);

    connect(m_ui.actionAbout_Amuse, SIGNAL(triggered()), this, SLOT(aboutAmuseAction()));
    connect(m_ui.actionAbout_Qt, SIGNAL(triggered()), this, SLOT(aboutQtAction()));

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
    connect(QGuiApplication::clipboard(), SIGNAL(dataChanged()), this, SLOT(onClipboardChanged()));

    m_voxEngine = boo::NewAudioVoiceEngine();
    m_voxAllocator = std::make_unique<VoiceAllocator>(*m_voxEngine);
    m_engine = std::make_unique<amuse::Engine>(*m_voxAllocator);
    m_engine->setVolume(0.7f);
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
                              const QString&,
                              const QString&,
                              const QString&,
                              const QString&,
                              int,
                              int)),
            this, SLOT(msgQuestion(const QString&,
                             const QString&,
                             const QString&,
                             const QString&,
                             const QString&,
                             int,
                             int)), type);
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
        m_recentFileActs[i]->setToolTip(files[i]);
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
        m_mainMessenger.critical(tr("Directory empty"), msg);
        return false;
    }
    else if (!dir.exists())
    {
        QString msg = QString(tr("The directory at '%1' must exist for the Amuse editor.")).arg(path);
        m_mainMessenger.critical(tr("Directory does not exist"), msg);
        return false;
    }
    QString testWritePath = dir.filePath(tr("__amuse_test__"));
    QFile testWriteFile(testWritePath);
    if (!testWriteFile.open(QFile::ReadWrite))
    {

        QString msg = QString(tr("The directory at '%1' must be writable for the Amuse editor: %2")).arg(path).
            arg(testWriteFile.errorString());
        m_mainMessenger.critical(tr("Unable to write to directory"), msg);
        return false;
    }
    testWriteFile.remove();

    closeEditor();
    if (m_projectModel)
        m_projectModel->deleteLater();
    m_projectModel = new ProjectModel(path, this);
    m_ui.projectOutlineFilter->clear();
    connect(m_ui.projectOutlineFilter, SIGNAL(textChanged(const QString&)),
            m_projectModel->getOutlineProxy(), SLOT(setFilterRegExp(const QString&)));
    m_ui.projectOutline->setModel(m_projectModel->getOutlineProxy());
    connect(m_ui.projectOutline->selectionModel(),
            SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)),
            this, SLOT(onOutlineSelectionChanged(const QItemSelection&, const QItemSelection&)));
    m_fileDialog.setDirectory(path);
    m_ui.actionSave_Project->setEnabled(true);
    m_ui.actionRevert_Project->setEnabled(true);
    m_ui.actionReload_Sample_Data->setEnabled(true);
    m_ui.actionImport_Songs->setEnabled(true);
    m_ui.actionExport_GameCube_Groups->setEnabled(true);
    m_ui.actionImport_C_Headers->setEnabled(true);
    m_ui.actionExport_C_Headers->setEnabled(true);
    m_ui.actionNew_Subproject->setEnabled(true);
    setWindowFilePath(path);
    updateWindowTitle();
    updateFocus();
    m_undoStack->clear();
    m_navList.clear();
    m_navIt = m_navList.begin();

    updateNavigationButtons();

    QSettings settings;
    QStringList files = settings.value("recentFileList").toStringList();
    files.removeAll(dir.path());
    files.prepend(dir.path());
    while (files.size() > MaxRecentFiles)
        files.removeLast();
    settings.setValue("recentFileList", files);
    settings.sync();

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

        m_peakVoices = std::max(int(m_engine->getNumTotalActiveVoices()), m_peakVoices);
        if (!(m_timerFireCount % 10))
        {
            /* Rate limit voice counter */
            m_ui.statusbar->setVoiceCount(m_peakVoices);
            m_peakVoices = 0;
        }
        if (m_engine->getActiveVoices().empty() && m_uiDisabled)
        {
            m_ui.projectOutline->setEnabled(true);
            m_ui.backButton->setEnabled(true);
            m_ui.forwardButton->setEnabled(true);
            if (EditorWidget* w = getEditorWidget())
                w->setEditorEnabled(true);
            m_ui.menubar->setEnabled(true);
            m_uiDisabled = false;
        }
        else if (!m_engine->getActiveVoices().empty() && !m_uiDisabled)
        {
            m_ui.projectOutline->setEnabled(false);
            m_ui.backButton->setEnabled(false);
            m_ui.forwardButton->setEnabled(false);
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
                if (player->sequencer() && player->sequencer()->state() != amuse::SequencerState::Playing)
                    player->stopped();

        QTableView* sfxTable = m_soundGroupEditor->getSFXListView();
        for (int i = 0; i < sfxTable->model()->rowCount(); ++i)
            if (SFXPlayerWidget* player = qobject_cast<SFXPlayerWidget*>(
                sfxTable->indexWidget(sfxTable->model()->index(i, 1))))
                if (player->voice() && player->voice()->state() != amuse::VoiceState::Playing)
                    player->stopped();

        ++m_timerFireCount;
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

bool MainWindow::_setEditor(EditorWidget* editor, bool appendNav)
{
    m_interactiveSeq.reset();
    if (editor != m_ui.editorContents->currentWidget() &&
        m_ui.editorContents->currentWidget() != m_faceSvg)
        getEditorWidget()->unloadData();
    if (appendNav && m_navIt != m_navList.end())
        m_navList.erase(m_navIt + 1, m_navList.end());
    if (!editor || !editor->valid())
    {
        m_ui.editorContents->setCurrentWidget(m_faceSvg);
        updateWindowTitle();
        if (appendNav)
        {
            m_navIt = m_navList.end();
            updateNavigationButtons();
        }
        return false;
    }
    m_ui.editorContents->setCurrentWidget(editor);
    m_ui.editorContents->update();
    updateWindowTitle();
    ProjectModel::INode* currentNode = editor->currentNode();
    if (appendNav)
    {
        if (m_navIt == m_navList.end() || *m_navIt != currentNode)
            m_navIt = m_navList.insert(m_navList.end(), currentNode);
        updateNavigationButtons();
    }
    recursiveExpandAndSelectOutline(m_projectModel->index(currentNode));
    return true;
}

bool MainWindow::openEditor(ProjectModel::SongGroupNode* node, bool appendNav)
{
    bool ret = _setEditor(m_songGroupEditor->loadData(node) ? m_songGroupEditor : nullptr, appendNav);
    if (ProjectModel::INode* n = getEditorNode())
    {
        if (n->type() == ProjectModel::INode::Type::SongGroup)
        {
            ProjectModel::SongGroupNode* cn = static_cast<ProjectModel::SongGroupNode*>(n);
            ProjectModel::GroupNode* gn = m_projectModel->getGroupNode(n);
            m_interactiveSeq = m_engine->seqPlay(gn->getAudioGroup(), cn->m_id, {}, nullptr);
        }
    }
    return ret;
}

bool MainWindow::openEditor(ProjectModel::SoundGroupNode* node, bool appendNav)
{
    bool ret = _setEditor(m_soundGroupEditor->loadData(node) ? m_soundGroupEditor : nullptr, appendNav);
    if (ProjectModel::INode* n = getEditorNode())
    {
        if (n->type() == ProjectModel::INode::Type::SoundGroup)
        {
            ProjectModel::SoundGroupNode* cn = static_cast<ProjectModel::SoundGroupNode*>(n);
            ProjectModel::GroupNode* gn = m_projectModel->getGroupNode(n);
            m_interactiveSeq = m_engine->seqPlay(gn->getAudioGroup(), cn->m_id, {}, nullptr);
        }
    }
    return ret;
}

bool MainWindow::openEditor(ProjectModel::SoundMacroNode* node, bool appendNav)
{
    return _setEditor(m_soundMacroEditor->loadData(node) ? m_soundMacroEditor : nullptr, appendNav);
}

bool MainWindow::openEditor(ProjectModel::ADSRNode* node, bool appendNav)
{
    return _setEditor(m_adsrEditor->loadData(node) ? m_adsrEditor : nullptr, appendNav);
}

bool MainWindow::openEditor(ProjectModel::CurveNode* node, bool appendNav)
{
    return _setEditor(m_curveEditor->loadData(node) ? m_curveEditor : nullptr, appendNav);
}

bool MainWindow::openEditor(ProjectModel::KeymapNode* node, bool appendNav)
{
    return _setEditor(m_keymapEditor->loadData(node) ? m_keymapEditor : nullptr, appendNav);
}

bool MainWindow::openEditor(ProjectModel::LayersNode* node, bool appendNav)
{
    return _setEditor(m_layersEditor->loadData(node) ? m_layersEditor : nullptr, appendNav);
}

bool MainWindow::openEditor(ProjectModel::SampleNode* node, bool appendNav)
{
    return _setEditor(m_sampleEditor->loadData(node) ? m_sampleEditor : nullptr, appendNav);
}

bool MainWindow::openEditor(ProjectModel::INode* node, bool appendNav)
{
    switch (node->type())
    {
    case ProjectModel::INode::Type::SongGroup:
        return openEditor(static_cast<ProjectModel::SongGroupNode*>(node), appendNav);
    case ProjectModel::INode::Type::SoundGroup:
        return openEditor(static_cast<ProjectModel::SoundGroupNode*>(node), appendNav);
    case ProjectModel::INode::Type::SoundMacro:
        return openEditor(static_cast<ProjectModel::SoundMacroNode*>(node), appendNav);
    case ProjectModel::INode::Type::ADSR:
        return openEditor(static_cast<ProjectModel::ADSRNode*>(node), appendNav);
    case ProjectModel::INode::Type::Curve:
        return openEditor(static_cast<ProjectModel::CurveNode*>(node), appendNav);
    case ProjectModel::INode::Type::Keymap:
        return openEditor(static_cast<ProjectModel::KeymapNode*>(node), appendNav);
    case ProjectModel::INode::Type::Layer:
        return openEditor(static_cast<ProjectModel::LayersNode*>(node), appendNav);
    case ProjectModel::INode::Type::Sample:
        return openEditor(static_cast<ProjectModel::SampleNode*>(node), appendNav);
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

void MainWindow::pushUndoCommand(EditorUndoCommand* cmd)
{
    m_undoStack->push(cmd);
}

void MainWindow::updateFocus()
{
    onFocusChanged(nullptr, focusWidget());
}

void MainWindow::aboutToDeleteNode(ProjectModel::INode* node)
{
    if (*m_navIt == node && m_navIt != m_navList.end())
    {
        m_navList.erase(m_navIt, m_navList.end());
        m_navIt = m_navList.end();
    }
    for (auto it = m_navList.begin(); it != m_navList.end();)
    {
        if (*it == node)
        {
            it = m_navList.erase(it);
            continue;
        }
        ++it;
    }
    updateNavigationButtons();
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
        int result = m_mainMessenger.question(tr("Unsaved Changes"), tr("Save Changes in %1?").arg(dir.dirName()),
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
    m_fileDialog.setWindowTitle(tr("New Project"));
    m_fileDialog.setAcceptMode(QFileDialog::AcceptSave);
    m_fileDialog.setFileMode(QFileDialog::AnyFile);
    m_fileDialog.setOption(QFileDialog::ShowDirsOnly, false);
    m_fileDialog.open(this, SLOT(_newAction(const QString&)));
}

void MainWindow::_newAction(const QString& path)
{
    m_fileDialog.close();
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
        m_mainMessenger.critical(tr("Directory empty"), msg);
        return false;
    }
    else if (!dir.exists())
    {
        QString msg = QString(tr("The directory at '%1' does not exist.")).arg(path);
        m_mainMessenger.critical(tr("Bad Directory"), msg);
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
    m_fileDialog.setWindowTitle(tr("Open Project"));
    m_fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
    m_fileDialog.setFileMode(QFileDialog::Directory);
    m_fileDialog.setOption(QFileDialog::ShowDirsOnly, true);
    m_fileDialog.open(this, SLOT(_openAction(const QString&)));
}

void MainWindow::_openAction(const QString& path)
{
    m_fileDialog.close();
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
            settings.sync();
            updateRecentFileActions();
        }
}

void MainWindow::clearRecentFilesAction()
{
    QSettings settings;
    settings.setValue("recentFileList", QStringList());
    settings.sync();
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
    m_navList.clear();
    m_navIt = m_navList.begin();
    updateNavigationButtons();
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
    m_fileDialog.setWindowTitle(tr("Import Project"));
    m_fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
    m_fileDialog.setFileMode(QFileDialog::ExistingFile);
    m_fileDialog.setOption(QFileDialog::ShowDirsOnly, false);
    m_fileDialog.open(this, SLOT(_importAction(const QString&)));
}

void MainWindow::_importAction(const QString& path)
{
    m_fileDialog.close();
    if (path.isEmpty())
        return;

    /* Validate input file */
    amuse::ContainerRegistry::Type tp =
        amuse::ContainerRegistry::DetectContainerType(QStringToSysString(path).c_str());
    if (tp == amuse::ContainerRegistry::Type::Invalid)
    {
        QString msg = QString(tr("The file at '%1' could not be interpreted as a MusyX container.")).arg(path);
        m_mainMessenger.critical(tr("Unsupported MusyX Container"), msg);
        return;
    }

    /* Ask user about sample conversion */
    int impMode = m_mainMessenger.question(tr("Sample Import Mode"),
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
        int scanMode = m_mainMessenger.question(tr("Raw Import Mode"),
                                                tr("Would you like to scan for all MusyX group files in this directory?"),
                                                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
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
                model->openSongsData();
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
        model->openSongsData();
    });
}

void MainWindow::importSongsAction()
{
    if (!m_projectModel)
        return;

    m_fileDialog.setWindowTitle(tr("Import Songs"));
    m_fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
    m_fileDialog.setFileMode(QFileDialog::ExistingFile);
    m_fileDialog.setOption(QFileDialog::ShowDirsOnly, false);
    m_fileDialog.open(this, SLOT(_importSongsAction(const QString&)));
}

void MainWindow::_importSongsAction(const QString& path)
{
    m_fileDialog.close();
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

void MainWindow::importHeadersAction()
{
    if (!m_projectModel)
        return;

    int confirm = m_mainMessenger.warning(tr("Import C Headers"),
        tr("<p>Importing names from C headers depends on up-to-date, "
           "consistent names relative to the sound group data.</p>"
           "<p>Headers are imported on a per-subproject basis from "
           "a single directory. Headers must be named with the form "
           "<code>&lt;subproject&gt;.h</code>.</p>"
           "<p>Group, Song and SFX definitions are matched according to the following forms:"
           "<pre>#define GRP&lt;name&gt; &lt;id&gt;\n#define SNG&lt;name&gt; &lt;id&gt;\n"
           "#define SFX&lt;name> &lt;id&gt;</pre></p>"
           "<p><strong>This operation cannot be undone! It is recommended to "
           "make a backup of the project directory before proceeding.</strong></p>"
           "<p>Continue?</p>"), QMessageBox::Yes | QMessageBox::No);
    if (confirm == QMessageBox::No)
        return;

    m_fileDialog.setWindowTitle(tr("Import C Headers"));
    m_fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
    m_fileDialog.setFileMode(QFileDialog::Directory);
    m_fileDialog.setOption(QFileDialog::ShowDirsOnly, true);
    m_fileDialog.open(this, SLOT(_importHeadersAction(const QString&)));
}

void MainWindow::_importHeadersAction(const QString& path)
{
    m_fileDialog.close();
    if (path.isEmpty())
        return;

    closeEditor();

    for (QString group : m_projectModel->getGroupList())
        m_projectModel->importHeader(path, group, m_mainMessenger);
    m_projectModel->updateNodeNames();

    /* Samples may have been renamed.. stay consistent! */
    saveAction();
}

void MainWindow::exportHeadersAction()
{
    if (!m_projectModel)
        return;

    m_fileDialog.setWindowTitle(tr("Export C Headers"));
    m_fileDialog.setAcceptMode(QFileDialog::AcceptOpen);
    m_fileDialog.setFileMode(QFileDialog::Directory);
    m_fileDialog.setOption(QFileDialog::ShowDirsOnly, true);
    m_fileDialog.open(this, SLOT(_exportHeadersAction(const QString&)));
}

void MainWindow::_exportHeadersAction(const QString& path)
{
    m_fileDialog.close();
    if (path.isEmpty())
        return;

    bool yesToAll = false;
    for (QString group : m_projectModel->getGroupList())
        if (!m_projectModel->exportHeader(path, group, yesToAll, m_mainMessenger))
            return;
}

bool TreeDelegate::editorEvent(QEvent* event,
                               QAbstractItemModel* __model,
                               const QStyleOptionViewItem& option,
                               const QModelIndex& index)
{
    QSortFilterProxyModel* _model = static_cast<QSortFilterProxyModel*>(__model);
    ProjectModel* model = static_cast<ProjectModel*>(_model->sourceModel());
    ProjectModel::INode* node = model->node(_model->mapToSource(index));
    if (!node || !(node->flags() & Qt::ItemIsEditable))
        return false;
    AmuseItemEditFlags flags = node->editFlags();

    if (event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent* ev = static_cast<QMouseEvent*>(event);
        if (ev->button() == Qt::RightButton)
        {
            ContextMenu* menu = new ContextMenu;

            if (node->type() == ProjectModel::INode::Type::Group)
            {
                QAction* exportGroupAction = new QAction(tr("Export GameCube Group"), menu);
                exportGroupAction->setData(QVariant::fromValue((void*)node));
                connect(exportGroupAction, SIGNAL(triggered()), this, SLOT(doExportGroup()));
                menu->addAction(exportGroupAction);

                menu->addSeparator();
            }

            QAction* findUsagesAction = new QAction(tr("Find Usages"), menu);
            findUsagesAction->setData(QVariant::fromValue((void*)node));
            findUsagesAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-find")));
            connect(findUsagesAction, SIGNAL(triggered()), this, SLOT(doFindUsages()));
            menu->addAction(findUsagesAction);

            menu->addSeparator();

            QAction* cutAction = new QAction(tr("Cut"), menu);
            cutAction->setData(index);
            cutAction->setEnabled(flags & AmuseItemCut);
            cutAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-cut")));
            cutAction->setShortcut(QKeySequence::Cut);
            connect(cutAction, SIGNAL(triggered()), this, SLOT(doCut()));
            menu->addAction(cutAction);

            QAction* copyAction = new QAction(tr("Copy"), menu);
            copyAction->setData(index);
            copyAction->setEnabled(flags & AmuseItemCopy);
            copyAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
            copyAction->setShortcut(QKeySequence::Copy);
            connect(copyAction, SIGNAL(triggered()), this, SLOT(doCopy()));
            menu->addAction(copyAction);

            QAction* pasteAction = new QAction(tr("Paste"), menu);
            pasteAction->setData(index);
            pasteAction->setEnabled(g_MainWindow->m_clipboardAmuseData && flags & AmuseItemPaste);
            pasteAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-paste")));
            pasteAction->setShortcut(QKeySequence::Paste);
            connect(pasteAction, SIGNAL(triggered()), this, SLOT(doPaste()));
            menu->addAction(pasteAction);

            QAction* dupeAction = new QAction(tr("Duplicate"), menu);
            dupeAction->setData(index);
            connect(dupeAction, SIGNAL(triggered()), this, SLOT(doDuplicate()));
            menu->addAction(dupeAction);

            QAction* deleteAction = new QAction(tr("Delete"), menu);
            deleteAction->setData(index);
            deleteAction->setEnabled(flags & AmuseItemDelete);
            deleteAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
            deleteAction->setShortcut(QKeySequence::Delete);
            connect(deleteAction, SIGNAL(triggered()), this, SLOT(doDelete()));
            menu->addAction(deleteAction);

            QAction* renameAction = new QAction(tr("Rename"), menu);
            renameAction->setData(index);
            renameAction->setShortcut(Qt::Key_F2);
            connect(renameAction, SIGNAL(triggered()), this, SLOT(doRename()));
            menu->addAction(renameAction);

            menu->popup(ev->globalPos());
        }
    }

    return false;
}

void TreeDelegate::doExportGroup()
{
    if (!m_window.m_projectModel)
        return;
    QAction* act = qobject_cast<QAction*>(sender());
    if (ProjectModel::INode* node = reinterpret_cast<ProjectModel::INode*>(act->data().value<void*>()))
    {
        if (node->type() != ProjectModel::INode::Type::Group)
            return;
        QString groupName = static_cast<ProjectModel::GroupNode*>(node)->name();
        ProjectModel* model = m_window.m_projectModel;

        QFileInfo dirInfo(model->dir(), QStringLiteral("out"));
        if (!MkPath(dirInfo.filePath(), m_window.m_mainMessenger))
            return;

        QDir dir(dirInfo.filePath());

        m_window.startBackgroundTask(BackgroundTaskId::TaskExport, tr("Exporting"), tr("Exporting %1").arg(groupName),
        [model, dir, groupName](BackgroundTask& task)
        {
            model->exportGroup(dir.path(), groupName, task.uiMessenger());
        });
    }
}

void TreeDelegate::doFindUsages()
{
    QAction* act = qobject_cast<QAction*>(sender());
    if (ProjectModel::INode* node = reinterpret_cast<ProjectModel::INode*>(act->data().value<void*>()))
        m_window.findUsages(node);
}

void TreeDelegate::doCut()
{
    QAction* act = qobject_cast<QAction*>(sender());
    if (!m_window.m_projectModel)
        return;
    m_window.m_projectModel->cut(m_window.m_projectModel->getOutlineProxy()->mapToSource(act->data().toModelIndex()));
}

void TreeDelegate::doCopy()
{
    QAction* act = qobject_cast<QAction*>(sender());
    if (!m_window.m_projectModel)
        return;
    m_window.m_projectModel->copy(m_window.m_projectModel->getOutlineProxy()->mapToSource(act->data().toModelIndex()));
}

void TreeDelegate::doPaste()
{
    QAction* act = qobject_cast<QAction*>(sender());
    if (!m_window.m_projectModel)
        return;
    m_window.m_projectModel->paste(m_window.m_projectModel->getOutlineProxy()->mapToSource(act->data().toModelIndex()));
}

void TreeDelegate::doDuplicate()
{
    QAction* act = qobject_cast<QAction*>(sender());
    if (!m_window.m_projectModel)
        return;
    QModelIndex newIdx =
        m_window.m_projectModel->duplicate(m_window.m_projectModel->getOutlineProxy()->mapToSource(act->data().toModelIndex()));
    if (newIdx.isValid())
    {
        newIdx = m_window.m_projectModel->getOutlineProxy()->mapFromSource(newIdx);
        m_window.m_ui.projectOutline->edit(newIdx);
    }
}

void TreeDelegate::doDelete()
{
    QAction* act = qobject_cast<QAction*>(sender());
    if (!m_window.m_projectModel)
        return;
    m_window.m_projectModel->del(m_window.m_projectModel->getOutlineProxy()->mapToSource(act->data().toModelIndex()));
}

void TreeDelegate::doRename()
{
    QAction* act = qobject_cast<QAction*>(sender());
    m_window.m_ui.projectOutline->edit(act->data().toModelIndex());
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
    if (!m_ui.projectOutline->currentIndex().isValid())
        return nullptr;
    return m_projectModel->getGroupNode(m_projectModel->node(
        m_projectModel->getOutlineProxy()->mapToSource(m_ui.projectOutline->currentIndex())));
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
    QModelIndex filterIndex = m_projectModel->getOutlineProxy()->mapFromSource(index);
    _recursiveExpandOutline(filterIndex);
    if (filterIndex.isValid())
        m_ui.projectOutline->setCurrentIndex(filterIndex);
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
        dialog.setWindowModality(Qt::WindowModal);
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

void MainWindow::updateNavigationButtons()
{
    m_goForward->setDisabled(m_navIt == m_navList.end() || m_navIt + 1 == m_navList.end());
    m_goBack->setDisabled(m_navIt == m_navList.begin());
}

void MainWindow::goForward()
{
    if (m_navIt == m_navList.end() || m_navIt + 1 == m_navList.end())
        return;
    ++m_navIt;
    openEditor(*m_navIt, false);
    updateNavigationButtons();
}

void MainWindow::goBack()
{
    if (m_navIt == m_navList.begin())
        return;
    --m_navIt;
    openEditor(*m_navIt, false);
    updateNavigationButtons();
}

void MainWindow::findUsages(ProjectModel::INode* node)
{
    m_ui.projectOutlineFilter->setText(QStringLiteral("usages:") + node->name());
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

void MainWindow::aboutAmuseAction()
{
#ifdef Q_OS_MAC
    static QPointer<QMessageBox> oldMsgBox;

    if (oldMsgBox) {
        oldMsgBox->show();
        oldMsgBox->raise();
        oldMsgBox->activateWindow();
        return;
    }
#endif

    QString translatedTextAboutAmuseCaption;
    translatedTextAboutAmuseCaption = QMessageBox::tr(
        "<h3>About Amuse</h3>"
        );
    QString translatedTextAboutAmuseText;
    translatedTextAboutAmuseText = QMessageBox::tr(
        "<p>Amuse is an alternate editor and runtime library for MusyX sound groups.</p>"
        "<p>MusyX originally served as a widely-deployed audio system for "
        "developing games on the Nintendo 64, GameCube, and GameBoy Advance.</p>"
        "<p>Amuse is available under the MIT license.<br>"
        "Please see <a href=\"https://gitlab.axiodl.com/AxioDL/amuse/blob/master/LICENSE\">"
        "https://gitlab.axiodl.com/AxioDL/amuse/blob/master/LICENSE</a> for futher information.</p>"
        "<p>Copyright (C) 2015-2018 Antidote / Jackoalan.</p>"
        "<p>MusyX is a trademark of Factor 5, LLC.</p>"
        "<p>Nintendo 64, GameCube, and GameBoy Advance are trademarks of Nintendo Co., Ltd.</p>"
        );
    QMessageBox *msgBox = new QMessageBox(this);
    msgBox->setAttribute(Qt::WA_DeleteOnClose);
    msgBox->setWindowTitle(tr("About Amuse"));
    msgBox->setText(translatedTextAboutAmuseCaption);
    msgBox->setInformativeText(translatedTextAboutAmuseText);
    msgBox->setIconPixmap(windowIcon().pixmap(64, 64));

    // should perhaps be a style hint
#ifdef Q_OS_MAC
    oldMsgBox = msgBox;
#if 0
    // ### doesn't work until close button is enabled in title bar
    //msgBox->d_func()->autoAddOkButton = false;
#else
    //msgBox->d_func()->buttonBox->setCenterButtons(true);
#endif
    msgBox->show();
#else
    msgBox->exec();
#endif
}

void MainWindow::aboutQtAction()
{
    QMessageBox::aboutQt(this);
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
    if (!m_projectModel)
        return;
    m_projectModel->cut(m_projectModel->getOutlineProxy()->mapToSource(m_ui.projectOutline->currentIndex()));
}

void MainWindow::outlineCopyAction()
{
    if (!m_projectModel)
        return;
    m_projectModel->copy(m_projectModel->getOutlineProxy()->mapToSource(m_ui.projectOutline->currentIndex()));
}

void MainWindow::outlinePasteAction()
{
    if (!m_projectModel)
        return;
    m_projectModel->paste(m_projectModel->getOutlineProxy()->mapToSource(m_ui.projectOutline->currentIndex()));
}

void MainWindow::outlineDeleteAction()
{
    if (!m_projectModel)
        return;
    m_projectModel->del(m_projectModel->getOutlineProxy()->mapToSource(m_ui.projectOutline->currentIndex()));
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
        AmuseItemEditFlags editFlags = outlineEditFlags();
        if (!m_clipboardAmuseData)
            editFlags = AmuseItemEditFlags(editFlags & ~AmuseItemPaste);
        setItemEditFlags(editFlags);
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
        if (EditorWidget* editor = getEditorWidget())
        {
            setItemEditFlags(editor->itemEditFlags());
            m_cutConn = connect(m_ui.actionCut, SIGNAL(triggered()), editor, SLOT(itemCutAction()));
            m_copyConn = connect(m_ui.actionCopy, SIGNAL(triggered()), editor, SLOT(itemCopyAction()));
            m_pasteConn = connect(m_ui.actionPaste, SIGNAL(triggered()), editor, SLOT(itemPasteAction()));
            m_deleteConn = connect(m_ui.actionDelete, SIGNAL(triggered()), editor, SLOT(itemDeleteAction()));
        }
        else
        {
            setItemEditFlags(AmuseItemNone);
        }
    }
}

void MainWindow::onClipboardChanged()
{
    if (QClipboard* cb = qobject_cast<QClipboard*>(sender()))
    {
        if (const QMimeData* md = cb->mimeData())
        {
            for (const QString& str : md->formats())
            {
                if (str.startsWith(QStringLiteral("application/x-amuse-")))
                {
                    m_clipboardAmuseData = true;
                    updateFocus();
                    return;
                }
            }
        }
    }
    m_clipboardAmuseData = false;
    updateFocus();
}

void MainWindow::outlineItemActivated(const QModelIndex& index)
{
    ProjectModel::INode* node = m_projectModel->node(m_projectModel->getOutlineProxy()->mapToSource(index));
    if (!node)
        return;
    openEditor(node);
}

void MainWindow::setItemEditFlags(AmuseItemEditFlags flags)
{
    m_ui.actionCut->setEnabled(flags & AmuseItemCut);
    m_ui.actionCopy->setEnabled(flags & AmuseItemCopy);
    m_ui.actionPaste->setEnabled(flags & AmuseItemPaste);
    m_ui.actionDelete->setEnabled(flags & AmuseItemDelete);
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

AmuseItemEditFlags MainWindow::outlineEditFlags()
{
    if (!m_projectModel)
        return AmuseItemNone;
    QModelIndex curIndex = m_ui.projectOutline->currentIndex();
    if (!curIndex.isValid())
        return AmuseItemNone;
    return m_projectModel->editFlags(m_projectModel->getOutlineProxy()->mapToSource(curIndex));
}

void MainWindow::onOutlineSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    if (!m_projectModel)
        return;
    setItemNewEnabled(m_ui.projectOutline->currentIndex().isValid());
    if (selected.indexes().empty())
        setItemEditFlags(AmuseItemNone);
    else
        setItemEditFlags(m_projectModel->editFlags(m_projectModel->getOutlineProxy()->mapToSource(selected.indexes().front())));
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
    m_backgroundDialog->close();
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
            ShowInGraphicalShell(this,
                                 entryList.empty() ? dirInfo.filePath() : QFileInfo(dir, entryList.first()).filePath());
        }
    } else
    {
        bool hasGroups = m_projectModel->ensureModelData();
        m_ui.actionImport_Groups->setDisabled(hasGroups);
        m_ui.actionImport_Songs->setEnabled(hasGroups);
    }

    setEnabled(true);
}

static int ShowOldMessageBox(QWidget *parent, QMessageBox::Icon icon,
                             const QString &title, const QString &text,
                             const QString &button0Text,
                             const QString &button1Text,
                             const QString &button2Text,
                             int defaultButtonNumber,
                             int escapeButtonNumber)
{
    QMessageBox messageBox(icon, title, text, QMessageBox::NoButton, parent);
    messageBox.setWindowModality(Qt::WindowModal);
    QString myButton0Text = button0Text;
    if (myButton0Text.isEmpty())
        myButton0Text = QDialogButtonBox::tr("OK");
    messageBox.addButton(myButton0Text, QMessageBox::ActionRole);
    if (!button1Text.isEmpty())
        messageBox.addButton(button1Text, QMessageBox::ActionRole);
    if (!button2Text.isEmpty())
        messageBox.addButton(button2Text, QMessageBox::ActionRole);

    const QList<QAbstractButton *> &buttonList = messageBox.buttons();
    messageBox.setDefaultButton(static_cast<QPushButton *>(buttonList.value(defaultButtonNumber)));
    messageBox.setEscapeButton(buttonList.value(escapeButtonNumber));

    return messageBox.exec();
}

static QMessageBox::StandardButton ShowNewMessageBox(QWidget *parent,
       QMessageBox::Icon icon, const QString& title, const QString& text,
       QMessageBox::StandardButtons buttons, QMessageBox::StandardButton defaultButton)
{
    QMessageBox msgBox(icon, title, text, QMessageBox::NoButton, parent);
    msgBox.setWindowModality(Qt::WindowModal);
    QDialogButtonBox *buttonBox = msgBox.findChild<QDialogButtonBox*>();
    Q_ASSERT(buttonBox != 0);

    uint mask = QMessageBox::FirstButton;
    while (mask <= QMessageBox::LastButton) {
        uint sb = buttons & mask;
        mask <<= 1;
        if (!sb)
            continue;
        QPushButton *button = msgBox.addButton((QMessageBox::StandardButton)sb);
        // Choose the first accept role as the default
        if (msgBox.defaultButton())
            continue;
        if ((defaultButton == QMessageBox::NoButton && buttonBox->buttonRole(button) == QDialogButtonBox::AcceptRole)
            || (defaultButton != QMessageBox::NoButton && sb == uint(defaultButton)))
            msgBox.setDefaultButton(button);
    }
    if (msgBox.exec() == -1)
        return QMessageBox::Cancel;
    return msgBox.standardButton(msgBox.clickedButton());
}

QMessageBox::StandardButton MainWindow::msgInformation(const QString &title,
    const QString &text, QMessageBox::StandardButtons buttons,
    QMessageBox::StandardButton defaultButton)
{
    return ShowNewMessageBox(this, QMessageBox::Information, title, text, buttons, defaultButton);
}

int MainWindow::msgQuestion(const QString &title,
                            const QString& text,
                            const QString& button0Text,
                            const QString& button1Text,
                            const QString& button2Text,
                            int defaultButtonNumber,
                            int escapeButtonNumber)
{
    return ShowOldMessageBox(this, QMessageBox::Question, title, text,
                             button0Text, button1Text, button2Text,
                             defaultButtonNumber, escapeButtonNumber);
}

QMessageBox::StandardButton MainWindow::msgQuestion(const QString &title,
    const QString &text, QMessageBox::StandardButtons buttons,
    QMessageBox::StandardButton defaultButton)
{
    return ShowNewMessageBox(this, QMessageBox::Question, title, text, buttons, defaultButton);
}

QMessageBox::StandardButton MainWindow::msgWarning(const QString &title,
    const QString &text, QMessageBox::StandardButtons buttons,
    QMessageBox::StandardButton defaultButton)
{
    return ShowNewMessageBox(this, QMessageBox::Warning, title, text, buttons, defaultButton);
}

QMessageBox::StandardButton MainWindow::msgCritical(const QString &title,
    const QString &text, QMessageBox::StandardButtons buttons,
    QMessageBox::StandardButton defaultButton)
{
    return ShowNewMessageBox(this, QMessageBox::Critical, title, text, buttons, defaultButton);
}
