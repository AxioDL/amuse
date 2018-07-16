#include "MainWindow.hpp"
#include <QFileDialog>
#include <QMessageBox>
#include <QLineEdit>
#include <QInputDialog>
#include <QtSvg/QtSvg>
#include "amuse/ContainerRegistry.hpp"
#include "Common.hpp"

MainWindow::MainWindow(QWidget* parent)
: QMainWindow(parent),
  m_undoStack(new QUndoStack(this))
{
    m_ui.setupUi(this);
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

    m_ui.editorSvg->load(QStringLiteral(":/bg/FaceGrey.svg"));

    connect(m_ui.actionNew_SFX_Group, SIGNAL(triggered()), this, SLOT(newSFXGroupAction()));
    connect(m_ui.actionNew_Song_Group, SIGNAL(triggered()), this, SLOT(newSongGroupAction()));
    connect(m_ui.actionNew_Sound_Macro, SIGNAL(triggered()), this, SLOT(newSoundMacroAction()));
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
    printf("IM DYING\n");
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

void MainWindow::newAction()
{
    QString path = QFileDialog::getSaveFileName(this, tr("New Project"));
    if (path.isEmpty())
        return;
    if (!MkPath(path, this))
        return;
    setProjectPath(path);
}

void MainWindow::openAction()
{
    QString path = QFileDialog::getExistingDirectory(this, tr("Open Project"));
    if (path.isEmpty())
        return;
    setProjectPath(path);
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
           "Exporting the project will prefer compressed files over WAVs, so "
           "be sure to delete the compressed version if you edit the WAV."),
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
                if (!MkPath(fInfo.dir(), newName, this))
                    return;
                if (!setProjectPath(newPath))
                    return;
            }

            QDir dir = QFileInfo(path).dir();
            QStringList filters;
            filters << "*.proj" << "*.pro";
            QStringList files = dir.entryList(filters, QDir::Files);
            for (const QString& fPath : files)
            {
                auto data = amuse::ContainerRegistry::LoadContainer(QStringToSysString(dir.filePath(fPath)).c_str());
                for (auto& p : data)
                    if (!m_projectModel->importGroupData(SysStringToQString(p.first), std::move(p.second)))
                        return;
            }
            m_projectModel->extractSamples(ProjectModel::ImportMode(impMode), this);
            m_projectModel->saveToFile(this);
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
        if (!MkPath(fInfo.dir(), fInfo.completeBaseName(), this))
            return;
        if (!setProjectPath(newPath))
            return;
    }

    /* Handle single container */
    auto data = amuse::ContainerRegistry::LoadContainer(QStringToSysString(path).c_str());
    for (auto& p : data)
        if (!m_projectModel->importGroupData(SysStringToQString(p.first), std::move(p.second)))
            return;

    m_projectModel->extractSamples(ProjectModel::ImportMode(impMode), this);
    m_projectModel->saveToFile(this);
}

void MainWindow::exportAction()
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
