#include "EditorWidget.hpp"
#include "MainWindow.hpp"
#include <QStandardItemModel>

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

FieldProjectNode::FieldProjectNode(ProjectModel::CollectionNode* collection, QWidget* parent)
: FieldComboBox(parent)
{
    setCollection(collection);
}

void FieldProjectNode::setCollection(ProjectModel::CollectionNode* collection)
{
    m_collection = collection;

    if (!collection)
    {
        setModel(new QStandardItemModel(0, 1, this));
        return;
    }

    ProjectModel* model = g_MainWindow->projectModel();
    setModel(model->getNullProxy());
    setRootModelIndex(model->getNullProxy()->mapFromSource(model->index(collection)));
}

FieldPageObjectNode::FieldPageObjectNode(ProjectModel::GroupNode* group, QWidget* parent)
: FieldComboBox(parent)
{
    setGroup(group);
}

void FieldPageObjectNode::setGroup(ProjectModel::GroupNode* group)
{
    m_group = group;

    if (!group)
    {
        setModel(new QStandardItemModel(0, 1, this));
        return;
    }

    ProjectModel* model = g_MainWindow->projectModel();
    setModel(model->getPageObjectProxy());
    setRootModelIndex(model->getPageObjectProxy()->mapFromSource(model->index(group)));
}
