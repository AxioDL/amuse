#include "EditorWidget.hpp"
#include "MainWindow.hpp"

EditorWidget::EditorWidget(QWidget* parent)
: QWidget(parent)
{

}

void EditorUndoCommand::undo()
{
    g_MainWindow->openEditor(m_node.get());
}

void EditorUndoCommand::redo()
{
    g_MainWindow->openEditor(m_node.get());
}
