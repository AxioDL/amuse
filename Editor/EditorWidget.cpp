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

AddRemoveButtons::AddRemoveButtons(QWidget* parent)
: QWidget(parent), m_addAction(tr("Add Row")), m_addButton(this),
  m_removeAction(tr("Remove Row")), m_removeButton(this)
{
    setFixedSize(64, 32);

    m_addAction.setIcon(QIcon(QStringLiteral(":/icons/IconAdd.svg")));
    m_addButton.setDefaultAction(&m_addAction);
    m_addButton.setFixedSize(32, 32);
    m_addButton.move(0, 0);

    m_removeAction.setIcon(QIcon(QStringLiteral(":/icons/IconRemove.svg")));
    m_removeButton.setDefaultAction(&m_removeAction);
    m_removeButton.setFixedSize(32, 32);
    m_removeButton.move(32, 0);
    m_removeAction.setEnabled(false);
}
