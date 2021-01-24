#include "EditorWidget.hpp"
#include "MainWindow.hpp"
#include <QStandardItemModel>
#include <QHBoxLayout>

EditorWidget::EditorWidget(QWidget* parent) : QWidget(parent) {}

EditorWidget::~EditorWidget() = default;

void EditorUndoCommand::undo() { g_MainWindow->openEditor(m_node.get()); }

void EditorUndoCommand::redo() { g_MainWindow->openEditor(m_node.get()); }

FieldSlider::FieldSlider(QWidget* parent) : QWidget(parent), m_slider(Qt::Horizontal) {
  setFixedHeight(22);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  QHBoxLayout* layout = new QHBoxLayout;
  layout->addWidget(&m_slider);
  layout->addWidget(&m_value);
  layout->setContentsMargins(QMargins());
  setLayout(layout);
  m_value.setFixedWidth(42);
  connect(&m_slider, qOverload<int>(&QSlider::valueChanged), this, &FieldSlider::doValueChanged);
  m_slider.setValue(0);
}

void FieldSlider::doValueChanged(int value) {
  m_value.setText(QString::number(value));
  emit valueChanged(value);
}

FieldDoubleSlider::FieldDoubleSlider(QWidget* parent) : QWidget(parent), m_slider(Qt::Horizontal) {
  setFixedHeight(22);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  QHBoxLayout* layout = new QHBoxLayout;
  layout->addWidget(&m_slider);
  layout->addWidget(&m_value);
  layout->setContentsMargins(QMargins());
  setLayout(layout);
  m_value.setFixedWidth(42);
  connect(&m_slider, qOverload<int>(&QSlider::valueChanged), this, &FieldDoubleSlider::doValueChanged);
  m_slider.setRange(0, 1000);
  m_slider.setValue(0);
}

double FieldDoubleSlider::value() const {
  double t = m_slider.value() / 1000.0;
  return t * (m_max - m_min) + m_min;
}
void FieldDoubleSlider::setValue(double value) {
  if (value < m_min)
    value = m_min;
  else if (value > m_max)
    value = m_max;

  double t = (value - m_min) / (m_max - m_min);
  m_slider.setValue(int(t * 1000.0));
  doValueChanged(int(t * 1000.0));
}
void FieldDoubleSlider::setRange(double min, double max) {
  m_min = min;
  m_max = max;
  double curValue = value();
  if (curValue < min)
    setValue(min);
  else if (curValue > max)
    setValue(max);
}

void FieldDoubleSlider::doValueChanged(int value) {
  double t = value / 1000.0;
  t = t * (m_max - m_min) + m_min;
  m_value.setText(QString::number(t, 'g', 2));
  emit valueChanged(t);
}

FieldProjectNode::FieldProjectNode(ProjectModel::CollectionNode* collection, QWidget* parent)
: QWidget(parent), m_comboBox(this), m_button(this) {
  m_button.setDisabled(true);
  m_button.setFixedSize(30, 30);
  m_comboBox.setFixedHeight(30);
  connect(&m_comboBox, qOverload<int>(&FieldComboBox::currentIndexChanged), this,
          &FieldProjectNode::_currentIndexChanged);
  connect(&m_button, &QPushButton::clicked, this, &FieldProjectNode::openCurrent);
  QIcon icon(QStringLiteral(":/icons/IconForward.svg"));
  icon.addFile(QStringLiteral(":/icons/IconForwardDisabled.svg"), {}, QIcon::Disabled);
  m_button.setIcon(icon);
  QHBoxLayout* layout = new QHBoxLayout;
  layout->setContentsMargins({});
  layout->setSpacing(0);
  layout->addWidget(&m_comboBox);
  layout->addWidget(&m_button);
  setLayout(layout);
  setCollection(collection);
}

FieldProjectNode::~FieldProjectNode() = default;

void FieldProjectNode::setCollection(ProjectModel::CollectionNode* collection) {
  m_collection = collection;

  if (!collection) {
    m_comboBox.setModel(new QStandardItemModel(0, 1, this));
    m_button.setDisabled(true);
    return;
  }

  ProjectModel* model = g_MainWindow->projectModel();
  m_comboBox.setModel(model->getNullProxy());
  m_comboBox.setRootModelIndex(model->getNullProxy()->mapFromSource(model->index(collection)));
}

void FieldProjectNode::_currentIndexChanged(int index) {
  m_button.setEnabled(index != 0);
  emit currentIndexChanged(index);
}

ProjectModel::BasePoolObjectNode* FieldProjectNode::currentNode() const {
  int index = m_comboBox.currentIndex();
  if (index == 0)
    return nullptr;
  else
    return m_collection->nodeOfIndex(index - 1);
}

bool FieldProjectNode::event(QEvent* ev) {
  if (ev->type() == QEvent::User) {
    showPopup();
    return true;
  }
  return QWidget::event(ev);
}

void FieldProjectNode::openCurrent() {
  if (ProjectModel::BasePoolObjectNode* node = currentNode())
    if (!g_MainWindow->isUiDisabled())
      g_MainWindow->openEditor(node);
}

FieldPageObjectNode::FieldPageObjectNode(ProjectModel::GroupNode* group, QWidget* parent)
: QWidget(parent), m_comboBox(this), m_button(this) {
  m_button.setDisabled(true);
  m_button.setFixedSize(30, 30);
  m_comboBox.setFixedHeight(30);
  connect(&m_comboBox, qOverload<int>(&FieldComboBox::currentIndexChanged), this,
          &FieldPageObjectNode::_currentIndexChanged);
  connect(&m_button, &QPushButton::clicked, this, &FieldPageObjectNode::openCurrent);
  QIcon icon(QStringLiteral(":/icons/IconForward.svg"));
  icon.addFile(QStringLiteral(":/icons/IconForwardDisabled.svg"), {}, QIcon::Disabled);
  m_button.setIcon(icon);
  QHBoxLayout* layout = new QHBoxLayout;
  layout->setContentsMargins({});
  layout->setSpacing(0);
  layout->addWidget(&m_comboBox);
  layout->addWidget(&m_button);
  setLayout(layout);
  setGroup(group);
}

FieldPageObjectNode::~FieldPageObjectNode() = default;

void FieldPageObjectNode::setGroup(ProjectModel::GroupNode* group) {
  m_group = group;

  if (!group) {
    m_comboBox.setModel(new QStandardItemModel(0, 1, this));
    m_button.setDisabled(true);
    return;
  }

  ProjectModel* model = g_MainWindow->projectModel();
  m_comboBox.setModel(model->getPageObjectProxy());
  m_comboBox.setRootModelIndex(model->getPageObjectProxy()->mapFromSource(model->index(group)));
}

void FieldPageObjectNode::_currentIndexChanged(int index) {
  m_button.setEnabled(index != 0);
  emit currentIndexChanged(index);
}

ProjectModel::BasePoolObjectNode* FieldPageObjectNode::currentNode() const {
  int index = m_comboBox.currentIndex();
  if (index == 0) {
    return nullptr;
  } else {
    ProjectModel* model = g_MainWindow->projectModel();
    return static_cast<ProjectModel::BasePoolObjectNode*>(model->node(model->getPageObjectProxy()->mapToSource(
        model->getPageObjectProxy()->index(index, 0, m_comboBox.rootModelIndex()))));
  }
}

bool FieldPageObjectNode::event(QEvent* ev) {
  if (ev->type() == QEvent::User) {
    showPopup();
    return true;
  }
  return QWidget::event(ev);
}

void FieldPageObjectNode::openCurrent() {
  if (ProjectModel::BasePoolObjectNode* node = currentNode())
    if (!g_MainWindow->isUiDisabled())
      g_MainWindow->openEditor(node);
}

AddRemoveButtons::AddRemoveButtons(QWidget* parent)
: QWidget(parent)
, m_addAction(tr("Add Row"))
, m_addButton(this)
, m_removeAction(tr("Remove Row"))
, m_removeButton(this) {
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

void ListingDeleteButton::enterEvent(QEnterEvent* event) { setIcon(ListingDeleteHoveredIcon); }

void ListingDeleteButton::leaveEvent(QEvent* event) { setIcon(ListingDeleteIcon); }

ListingDeleteButton::ListingDeleteButton(QWidget* parent) : QPushButton(parent) {
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

bool BaseObjectDelegate::editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                                     const QModelIndex& index) {
  if (event->type() == QEvent::MouseButtonPress) {
    QMouseEvent* ev = static_cast<QMouseEvent*>(event);
    if (ev->button() == Qt::RightButton) {
      ProjectModel::INode* node = getNode(model, index);

      ContextMenu* menu = new ContextMenu;

      QAction* openEditorAction = new QAction(tr("Open in Editor"), menu);
      openEditorAction->setData(QVariant::fromValue((void*)node));
      openEditorAction->setIcon(QIcon::fromTheme(QStringLiteral("document-edit")));
      connect(openEditorAction, &QAction::triggered, this, &BaseObjectDelegate::doOpenEditor);
      menu->addAction(openEditorAction);

      QAction* findUsagesAction = new QAction(tr("Find Usages"), menu);
      findUsagesAction->setData(QVariant::fromValue((void*)node));
      findUsagesAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-find")));
      connect(findUsagesAction, &QAction::triggered, this, &BaseObjectDelegate::doFindUsages);
      menu->addAction(findUsagesAction);

      menu->popup(ev->globalPosition().toPoint());
    }
  }
  return false;
}

void BaseObjectDelegate::doOpenEditor() {
  QAction* act = static_cast<QAction*>(sender());
  if (ProjectModel::INode* node = reinterpret_cast<ProjectModel::INode*>(act->data().value<void*>()))
    g_MainWindow->openEditor(node);
}

void BaseObjectDelegate::doFindUsages() {
  QAction* act = static_cast<QAction*>(sender());
  if (ProjectModel::INode* node = reinterpret_cast<ProjectModel::INode*>(act->data().value<void*>()))
    g_MainWindow->findUsages(node);
}
