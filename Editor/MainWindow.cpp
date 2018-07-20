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

MainWindow::MainWindow(QWidget* parent)
: QMainWindow(parent),
  m_treeDelegate(*this, this),
  m_mainMessenger(this),
  m_undoStack(new QUndoStack(this)),
  m_backgroundThread(this)
{
    m_backgroundThread.start();

    m_ui.setupUi(this);
    m_ui.projectOutline->setItemDelegate(&m_treeDelegate);
    connectMessenger(&m_mainMessenger, Qt::DirectConnection);

    m_ui.keyboardContents->setStatusFocus(new StatusBarFocus(m_ui.statusbar));

    m_ui.actionNew_Project->setShortcut(QKeySequence::New);
    connect(m_ui.actionNew_Project, SIGNAL(triggered()), this, SLOT(newAction()));
    m_ui.actionOpen_Project->setShortcut(QKeySequence::Open);
    connect(m_ui.actionOpen_Project, SIGNAL(triggered()), this, SLOT(openAction()));
    connect(m_ui.actionImport, SIGNAL(triggered()), this, SLOT(importAction()));
    connect(m_ui.actionExport_GameCube_Groups, SIGNAL(triggered()), this, SLOT(exportAction()));
#ifndef __APPLE__
    m_ui.menuFile->addSeparator();
    QAction* quitAction = m_ui.menuFile->addAction(tr("Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
#endif

    m_ui.actionUndo->setShortcut(QKeySequence::Undo);
    m_ui.actionRedo->setShortcut(QKeySequence::Redo);
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

    setFocusAudioGroup(nullptr);

    m_voxEngine = boo::NewAudioVoiceEngine();
    m_voxAllocator = std::make_unique<amuse::BooBackendVoiceAllocator>(*m_voxEngine);
    m_engine = std::make_unique<amuse::Engine>(*m_voxAllocator);
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

bool MainWindow::setProjectPath(const QString& path)
{
    if (m_projectModel && m_projectModel->path() == path)
        return true;

    QDir dir(path);
    if (!dir.exists())
    {
        QString msg = QString(tr("The directory at '%1' must exist for the Amuse editor.")).arg(path);
        QMessageBox::critical(this, tr("Directory does not exist"), msg);
        return false;
    }
    QString testWritePath = dir.filePath(tr("test"));
    QFile testWriteFile(testWritePath);
    if (!testWriteFile.open(QFile::ReadWrite))
    {
        QString msg = QString(tr("The directory at '%1' must be writable for the Amuse editor.")).arg(path);
        QMessageBox::critical(this, tr("Unable to write to directory"), msg);
        return false;
    }
    testWriteFile.remove();

    if (m_projectModel)
        m_projectModel->deleteLater();
    m_projectModel = new ProjectModel(path, this);
    m_ui.projectOutline->setModel(m_projectModel);
    m_ui.actionExport_GameCube_Groups->setEnabled(true);
    setWindowFilePath(path);
    setWindowTitle(QString("Amuse [%1]").arg(dir.dirName()));
    setFocusAudioGroup(nullptr);
    onFocusChanged(nullptr, focusWidget());

    return true;
}

void MainWindow::setFocusAudioGroup(AudioGroupModel* group)
{
    m_focusAudioGroup = group;
    bool active = m_focusAudioGroup != nullptr;
    m_ui.actionNew_Sound_Macro->setEnabled(active);
    m_ui.actionNew_Keymap->setEnabled(active);
    m_ui.actionNew_Layers->setEnabled(active);
}

void MainWindow::refreshAudioIO()
{
    QList<QAction*> audioActions = m_ui.menuAudio->actions();
    if (audioActions.size() > 3)
        for (auto it = audioActions.begin() + 3 ; it != audioActions.end() ; ++it)
            m_ui.menuAudio->removeAction(*it);

    bool addedDev = false;
    // TODO: Do

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
        for (const auto& dev : m_voxEngine->enumerateMIDIDevices())
        {
            QAction* act = m_ui.menuMIDI->addAction(QString::fromStdString(dev.second));
            act->setData(QString::fromStdString(dev.first));
            connect(act, SIGNAL(triggered()), this, SLOT(setMIDIIO()));
            addedDev = true;
        }
    }

    if (!addedDev)
        m_ui.menuMIDI->addAction(tr("No MIDI Devices Found"))->setEnabled(false);
}

void MainWindow::startBackgroundTask(const QString& windowTitle, const QString& label,
                                     std::function<void(BackgroundTask&)>&& task)
{
    assert(m_backgroundTask == nullptr && "existing background process");
    setEnabled(false);

    m_backgroundTask = new BackgroundTask(std::move(task));
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
    connect(m_backgroundTask, SIGNAL(finished()),
            this, SLOT(onBackgroundTaskFinished()), Qt::QueuedConnection);
    m_backgroundDialog->open(m_backgroundTask, SLOT(cancel()));

    connectMessenger(&m_backgroundTask->uiMessenger(), Qt::BlockingQueuedConnection);

    QMetaObject::invokeMethod(m_backgroundTask, "run", Qt::QueuedConnection);
}

bool MainWindow::_setEditor(EditorWidget* editor)
{
    while (m_ui.editorContents->currentWidget() != m_faceSvg)
    {
        m_ui.editorContents->currentWidget()->deleteLater();
        m_ui.editorContents->removeWidget(m_ui.editorContents->currentWidget());
    }
    if (!editor)
        return false;
    if (!editor->valid())
    {
        editor->deleteLater();
        return false;
    }
    m_ui.editorContents->addWidget(editor);
    m_ui.editorContents->setCurrentWidget(editor);
    return true;
}

bool MainWindow::openEditor(ProjectModel::SongGroupNode* node)
{
    return _setEditor(new SongGroupEditor(node));
}

bool MainWindow::openEditor(ProjectModel::SoundGroupNode* node)
{
    return _setEditor(new SoundGroupEditor(node));
}

bool MainWindow::openEditor(ProjectModel::SoundMacroNode* node)
{
    return _setEditor(new SoundMacroEditor(node));
}

bool MainWindow::openEditor(ProjectModel::ADSRNode* node)
{
    return _setEditor(new ADSREditor(node));
}

bool MainWindow::openEditor(ProjectModel::CurveNode* node)
{
    return _setEditor(new CurveEditor(node));
}

bool MainWindow::openEditor(ProjectModel::KeymapNode* node)
{
    return _setEditor(new KeymapEditor(node));
}

bool MainWindow::openEditor(ProjectModel::LayersNode* node)
{
    return _setEditor(new LayersEditor(node));
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
    default:
        return false;
    }
}

void MainWindow::closeEditor()
{
    _setEditor(nullptr);
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
    m_projectModel->ensureModelData();
}

void MainWindow::openAction()
{
    QString path = QFileDialog::getExistingDirectory(this, tr("Open Project"));
    if (path.isEmpty())
        return;

    QDir dir(path);
    if (!dir.exists())
    {
        QString msg = QString(tr("The directory at '%1' does not exist.")).arg(path);
        QMessageBox::critical(this, tr("Bad Directory"), msg);
        return;
    }

    if (QFileInfo(dir, QStringLiteral("!project.yaml")).exists() &&
        QFileInfo(dir, QStringLiteral("!pool.yaml")).exists())
        dir.cdUp();

    if (!setProjectPath(dir.path()))
        return;

    ProjectModel* model = m_projectModel;
    startBackgroundTask(tr("Opening"), tr("Scanning Project"),
    [dir, model](BackgroundTask& task)
    {
        QStringList childDirs = dir.entryList(QDir::Dirs);
        for (const auto& chDir : childDirs)
        {
            if (task.isCanceled())
                return;
            QString chPath = dir.filePath(chDir);
            if (QFileInfo(chPath, QStringLiteral("!project.yaml")).exists() &&
                QFileInfo(chPath, QStringLiteral("!pool.yaml")).exists())
            {
                task.setLabelText(tr("Opening %1").arg(chDir));
                if (!model->openGroupData(chDir, task.uiMessenger()))
                    return;
            }
        }
    });
}

void MainWindow::importAction()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Import Project"));
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
            startBackgroundTask(tr("Importing"), tr("Scanning Project"),
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
    startBackgroundTask(tr("Importing"), tr("Scanning Project"),
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

void MainWindow::exportAction()
{

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

    if ((event->type() == QEvent::MouseButtonDblClick &&
         static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton))
    {
        // Open in editor
        return m_window.openEditor(node);
    }

    return false;
}

void MainWindow::newSubprojectAction()
{

}

void MainWindow::newSFXGroupAction()
{

}

void MainWindow::newSongGroupAction()
{

}

void MainWindow::newSoundMacroAction()
{

}

void MainWindow::newADSRAction()
{

}

void MainWindow::newCurveAction()
{

}

void MainWindow::newKeymapAction()
{

}

void MainWindow::newLayersAction()
{

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
    // TODO: Do
}

void MainWindow::setMIDIIO()
{
    // TODO: Do
    //qobject_cast<QAction*>(sender())->data().toString().toUtf8().constData();
}

void MainWindow::onFocusChanged(QWidget* old, QWidget* now)
{
    disconnect(m_undoConn);
    disconnect(m_canUndoConn);
    disconnect(m_redoConn);
    disconnect(m_canRedoConn);
    disconnect(m_cutConn);
    disconnect(m_copyConn);
    disconnect(m_pasteConn);
    disconnect(m_deleteConn);
    disconnect(m_canEditConn);

    if (QLineEdit* le = qobject_cast<QLineEdit*>(now))
    {
        m_undoConn = connect(m_ui.actionUndo, SIGNAL(triggered()), le, SLOT(undo()));
        m_canUndoConn = connect(le, SIGNAL(textChanged(const QString&)), this, SLOT(onTextEdited()));
        m_ui.actionUndo->setEnabled(le->isUndoAvailable());
        m_redoConn = connect(m_ui.actionRedo, SIGNAL(triggered()), le, SLOT(redo()));
        m_ui.actionRedo->setEnabled(le->isRedoAvailable());
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

    m_undoConn = connect(m_ui.actionUndo, SIGNAL(triggered()), m_undoStack, SLOT(undo()));
    m_canUndoConn = connect(m_undoStack, SIGNAL(canUndoChanged(bool)), m_ui.actionUndo, SLOT(setEnabled(bool)));
    m_ui.actionUndo->setEnabled(m_undoStack->canUndo());
    m_redoConn = connect(m_ui.actionRedo, SIGNAL(triggered()), m_undoStack, SLOT(redo()));
    m_canRedoConn = connect(m_undoStack, SIGNAL(canRedoChanged(bool)), m_ui.actionRedo, SLOT(setEnabled(bool)));
    m_ui.actionRedo->setEnabled(m_undoStack->canRedo());

    if (now == m_ui.projectOutline || m_ui.projectOutline->isAncestorOf(now))
    {
        m_ui.actionCut->setEnabled(false);
        m_ui.actionCopy->setEnabled(false);
        m_ui.actionPaste->setEnabled(false);
        if (m_projectModel)
        {
            m_deleteConn = connect(m_ui.actionDelete, SIGNAL(triggered()), m_projectModel, SLOT(del()));
            m_ui.actionDelete->setEnabled(m_projectModel->canDelete());
            m_canEditConn = connect(m_projectModel, SIGNAL(canDeleteChanged(bool)),
                                    m_ui.actionDelete, SLOT(setEnabled(bool)));
        }
    }
    else if (now == m_ui.editorContents || m_ui.editorContents->isAncestorOf(now))
    {

    }

}

void MainWindow::onTextEdited()
{
    if (QLineEdit* le = qobject_cast<QLineEdit*>(sender()))
    {
        m_ui.actionUndo->setEnabled(le->isUndoAvailable());
        m_ui.actionRedo->setEnabled(le->isRedoAvailable());
    }
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

void MainWindow::onBackgroundTaskFinished()
{
    m_backgroundDialog->reset();
    m_backgroundDialog->deleteLater();
    m_backgroundDialog = nullptr;
    m_backgroundTask->deleteLater();
    m_backgroundTask = nullptr;
    m_projectModel->ensureModelData();
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
