#include "EditorWidget.hpp"
#include "MainWindow.hpp"
#include <QStandardItemModel>
#include <QHBoxLayout>

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

FieldSlider::FieldSlider(QWidget* parent)
: QWidget(parent), m_slider(Qt::Horizontal)
{
    setFixedHeight(22);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QHBoxLayout* layout = new QHBoxLayout;
    layout->addWidget(&m_slider);
    layout->addWidget(&m_value);
    layout->setContentsMargins(QMargins());
    setLayout(layout);
    m_value.setFixedWidth(42);
    connect(&m_slider, SIGNAL(valueChanged(int)), this, SLOT(doValueChanged(int)));
    m_slider.setValue(0);
}

void FieldSlider::doValueChanged(int value)
{
    m_value.setText(QString::number(value));
    emit valueChanged(value);
}

FieldDoubleSlider::FieldDoubleSlider(QWidget* parent)
: QWidget(parent), m_slider(Qt::Horizontal)
{
    setFixedHeight(22);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QHBoxLayout* layout = new QHBoxLayout;
    layout->addWidget(&m_slider);
    layout->addWidget(&m_value);
    layout->setContentsMargins(QMargins());
    setLayout(layout);
    m_value.setFixedWidth(42);
    connect(&m_slider, SIGNAL(valueChanged(int)), this, SLOT(doValueChanged(int)));
    m_slider.setRange(0, 1000);
    m_slider.setValue(0);
}

double FieldDoubleSlider::value() const
{
    double t = m_slider.value() / 1000.0;
    return t * (m_max - m_min) + m_min;
}
void FieldDoubleSlider::setValue(double value)
{
    if (value < m_min)
        value = m_min;
    else if (value > m_max)
        value = m_max;

    double t = (value - m_min) / (m_max - m_min);
    m_slider.setValue(int(t * 1000.0));
    doValueChanged(int(t * 1000.0));
}
void FieldDoubleSlider::setRange(double min, double max)
{
    m_min = min; m_max = max;
    double curValue = value();
    if (curValue < min)
        setValue(min);
    else if (curValue > max)
        setValue(max);
}

void FieldDoubleSlider::doValueChanged(int value)
{
    double t = value / 1000.0;
    t = t * (m_max - m_min) + m_min;
    m_value.setText(QString::number(t, 'g', 2));
    emit valueChanged(t);
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

static QIcon ListingDeleteIcon;
static QIcon ListingDeleteHoveredIcon;

void ListingDeleteButton::enterEvent(QEvent* event)
{
    setIcon(ListingDeleteHoveredIcon);
}

void ListingDeleteButton::leaveEvent(QEvent* event)
{
    setIcon(ListingDeleteIcon);
}

ListingDeleteButton::ListingDeleteButton(QWidget* parent)
: QPushButton(parent)
{
    if (ListingDeleteIcon.isNull())
        ListingDeleteIcon = QIcon(QStringLiteral(":/icons/IconSoundMacroDelete.svg"));
    if (ListingDeleteHoveredIcon.isNull())
        ListingDeleteHoveredIcon = QIcon(QStringLiteral(":/icons/IconSoundMacroDeleteHovered.svg"));

    setVisible(false);
    setFixedSize(21, 21);
    setFlat(true);
    setToolTip(tr("Delete this SoundMacro"));
    setIcon(ListingDeleteIcon);
}
