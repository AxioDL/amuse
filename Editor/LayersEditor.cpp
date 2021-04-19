#include "LayersEditor.hpp"

#include <algorithm>

#include <QMimeData>
#include <QScrollBar>
#include <QVBoxLayout>

#include "MainWindow.hpp"


class LayerDataChangeUndoCommand : public EditorUndoCommand {
  QModelIndex m_index;
  int m_undoVal, m_redoVal;
  bool m_undid = false;

public:
  explicit LayerDataChangeUndoCommand(ProjectModel::LayersNode* node, const QString& text, QModelIndex index,
                                      int redoVal)
  : EditorUndoCommand(node, text), m_index(index), m_redoVal(redoVal) {}
  void undo() override {
    m_undid = true;
    amuse::LayerMapping& layer = (*static_cast<ProjectModel::LayersNode*>(m_node.get())->m_obj)[m_index.row()];

    switch (m_index.column()) {
    case 0:
      layer.macro.id = m_undoVal;
      break;
    case 1:
      layer.keyLo = m_undoVal;
      break;
    case 2:
      layer.keyHi = m_undoVal;
      break;
    case 3:
      layer.transpose = m_undoVal;
      break;
    case 4:
      layer.volume = m_undoVal;
      break;
    case 5:
      layer.prioOffset = m_undoVal;
      break;
    case 6:
      layer.span = m_undoVal;
      break;
    case 7:
      layer.pan = m_undoVal;
      break;
    default:
      break;
    }

    EditorUndoCommand::undo();
  }
  void redo() override {
    amuse::LayerMapping& layer = (*static_cast<ProjectModel::LayersNode*>(m_node.get())->m_obj)[m_index.row()];

    switch (m_index.column()) {
    case 0:
      m_undoVal = layer.macro.id.id;
      layer.macro.id = m_redoVal;
      break;
    case 1:
      m_undoVal = layer.keyLo;
      layer.keyLo = m_redoVal;
      break;
    case 2:
      m_undoVal = layer.keyHi;
      layer.keyHi = m_redoVal;
      break;
    case 3:
      m_undoVal = layer.transpose;
      layer.transpose = m_redoVal;
      break;
    case 4:
      m_undoVal = layer.volume;
      layer.volume = m_redoVal;
      break;
    case 5:
      m_undoVal = layer.prioOffset;
      layer.prioOffset = m_redoVal;
      break;
    case 6:
      m_undoVal = layer.span;
      layer.span = m_redoVal;
      break;
    case 7:
      m_undoVal = layer.pan;
      layer.pan = m_redoVal;
      break;
    default:
      break;
    }

    if (m_undid)
      EditorUndoCommand::redo();
  }
};

SoundMacroDelegate::SoundMacroDelegate(QObject* parent) : BaseObjectDelegate(parent) {}

SoundMacroDelegate::~SoundMacroDelegate() = default;

ProjectModel::INode* SoundMacroDelegate::getNode(const QAbstractItemModel* __model, const QModelIndex& index) const {
  const LayersModel* model = static_cast<const LayersModel*>(__model);
  const amuse::LayerMapping& layer = (*model->m_node->m_obj)[index.row()];
  ProjectModel::GroupNode* group = g_MainWindow->projectModel()->getGroupNode(model->m_node.get());
  ProjectModel::CollectionNode* smColl = group->getCollectionOfType(ProjectModel::INode::Type::SoundMacro);
  return smColl->nodeOfId(layer.macro.id);
}

QWidget* SoundMacroDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                                          const QModelIndex& index) const {
  const LayersModel* model = static_cast<const LayersModel*>(index.model());
  ProjectModel::GroupNode* group = g_MainWindow->projectModel()->getGroupNode(model->m_node.get());
  EditorFieldProjectNode* cb =
      new EditorFieldProjectNode(group->getCollectionOfType(ProjectModel::INode::Type::SoundMacro), parent);
  connect(cb, qOverload<int>(&EditorFieldProjectNode::currentIndexChanged), this, &SoundMacroDelegate::smIndexChanged);
  return cb;
}

void SoundMacroDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
  const LayersModel* model = static_cast<const LayersModel*>(index.model());
  const amuse::LayerMapping& layer = (*model->m_node->m_obj)[index.row()];
  ProjectModel::GroupNode* group = g_MainWindow->projectModel()->getGroupNode(model->m_node.get());
  ProjectModel::CollectionNode* smColl = group->getCollectionOfType(ProjectModel::INode::Type::SoundMacro);
  static_cast<EditorFieldProjectNode*>(editor)->setCurrentIndex(smColl->indexOfId(layer.macro.id) + 1);
  if (static_cast<EditorFieldProjectNode*>(editor)->shouldPopupOpen())
    QApplication::postEvent(editor, new QEvent(QEvent::User));
}

void SoundMacroDelegate::setModelData(QWidget* editor, QAbstractItemModel* m, const QModelIndex& index) const {
  const LayersModel* model = static_cast<const LayersModel*>(m);
  amuse::LayerMapping& layer = (*model->m_node->m_obj)[index.row()];
  ProjectModel::GroupNode* group = g_MainWindow->projectModel()->getGroupNode(model->m_node.get());
  ProjectModel::CollectionNode* smColl = group->getCollectionOfType(ProjectModel::INode::Type::SoundMacro);
  int idx = static_cast<EditorFieldProjectNode*>(editor)->currentIndex();
  amuse::SoundMacroId id;
  if (idx != 0)
    id = smColl->idOfIndex(idx - 1);
  if (layer.macro.id == id)
    return;
  g_MainWindow->pushUndoCommand(new LayerDataChangeUndoCommand(
      model->m_node.get(), tr("Change %1").arg(m->headerData(0, Qt::Horizontal).toString()), index, id.id));
  emit m->dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
}

void SoundMacroDelegate::smIndexChanged() { emit commitData(static_cast<QWidget*>(sender())); }

void LayersModel::loadData(ProjectModel::LayersNode* node) {
  beginResetModel();
  m_node = node;
  endResetModel();
}

void LayersModel::unloadData() {
  beginResetModel();
  m_node.reset();
  endResetModel();
}

int LayersModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid())
    return 0;
  if (!m_node)
    return 0;
  return int(m_node->m_obj->size()) + 1;
}

int LayersModel::columnCount(const QModelIndex& parent) const {
  if (parent.isValid())
    return 0;
  return 8;
}

QVariant LayersModel::data(const QModelIndex& index, int role) const {
  if (!m_node)
    return QVariant();
  if (index.row() == m_node->m_obj->size())
    return QVariant();
  const amuse::LayerMapping& layer = (*m_node->m_obj)[index.row()];

  if (role == Qt::DisplayRole || role == Qt::EditRole) {
    switch (index.column()) {
    case 0: {
      ProjectModel::GroupNode* group = g_MainWindow->projectModel()->getGroupNode(m_node.get());
      ProjectModel::CollectionNode* smColl = group->getCollectionOfType(ProjectModel::INode::Type::SoundMacro);
      if (ProjectModel::BasePoolObjectNode* node = smColl->nodeOfId(layer.macro.id))
        return node->text();
      return QVariant();
    }
    case 1:
      return layer.keyLo;
    case 2:
      return layer.keyHi;
    case 3:
      return layer.transpose;
    case 4:
      return layer.volume;
    case 5:
      return layer.prioOffset;
    case 6:
      return layer.span;
    case 7:
      return layer.pan;
    default:
      break;
    }
  }

  return QVariant();
}

bool LayersModel::setData(const QModelIndex& index, const QVariant& value, int role) {
  if (!m_node || role != Qt::EditRole)
    return false;
  const amuse::LayerMapping& layer = (*m_node->m_obj)[index.row()];

  switch (index.column()) {
  case 0:
    if (layer.macro.id == value.toInt())
      return false;
    break;
  case 1:
    if (layer.keyLo == value.toInt())
      return false;
    break;
  case 2:
    if (layer.keyHi == value.toInt())
      return false;
    break;
  case 3:
    if (layer.transpose == value.toInt())
      return false;
    break;
  case 4:
    if (layer.volume == value.toInt())
      return false;
    break;
  case 5:
    if (layer.prioOffset == value.toInt())
      return false;
    break;
  case 6:
    if (layer.span == value.toInt())
      return false;
    break;
  case 7:
    if (layer.pan == value.toInt())
      return false;
    break;
  default:
    return false;
  }

  g_MainWindow->pushUndoCommand(new LayerDataChangeUndoCommand(
      m_node.get(), tr("Change %1").arg(headerData(index.column(), Qt::Horizontal).toString()), index, value.toInt()));
  emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});

  return false;
}

QVariant LayersModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    switch (section) {
    case 0:
      return tr("SoundMacro");
    case 1:
      return tr("Key Lo");
    case 2:
      return tr("Key Hi");
    case 3:
      return tr("Transpose");
    case 4:
      return tr("Volume");
    case 5:
      return tr("Prio Off");
    case 6:
      return tr("Span");
    case 7:
      return tr("Pan");
    default:
      break;
    }
  }
  return QVariant();
}

Qt::ItemFlags LayersModel::flags(const QModelIndex& index) const {
  if (!index.isValid())
    return Qt::NoItemFlags;
  if (index.row() == m_node->m_obj->size())
    return Qt::NoItemFlags;
  return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
}

Qt::DropActions LayersModel::supportedDropActions() const { return Qt::MoveAction; }

Qt::DropActions LayersModel::supportedDragActions() const { return Qt::MoveAction; }

class LayerRowMoveCommand : public EditorUndoCommand {
  LayersTableView* m_view;
  int m_undoPos, m_redoPos, m_count;
  bool m_undid = false;

public:
  explicit LayerRowMoveCommand(ProjectModel::LayersNode* node, const QString& text, LayersTableView* view, int undoPos,
                               int redoPos, int count)
  : EditorUndoCommand(node, text), m_view(view), m_undoPos(undoPos), m_redoPos(redoPos), m_count(count) {}
  void undo() override {
    m_undid = true;
    EditorUndoCommand::undo();
    if (m_redoPos > m_undoPos)
      m_view->model()->moveRows(QModelIndex(), m_redoPos - 1, m_count, QModelIndex(), m_undoPos);
    else
      m_view->model()->moveRows(QModelIndex(), m_redoPos, m_count, QModelIndex(), m_undoPos + 1);
  }
  void redo() override {
    if (m_undid)
      EditorUndoCommand::redo();
    m_view->model()->moveRows(QModelIndex(), m_undoPos, m_count, QModelIndex(), m_redoPos);
  }
};

bool LayersModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column,
                               const QModelIndex& parent) {
  // check if the action is supported
  if (!data || action != Qt::MoveAction)
    return false;
  // check if the format is supported
  QStringList types = mimeTypes();
  if (types.isEmpty())
    return false;
  QString format = types.at(0);
  if (!data->hasFormat(format))
    return false;

  // decode and insert
  QByteArray encoded = data->data(format);
  QDataStream stream(&encoded, QIODevice::ReadOnly);

  std::unordered_set<int> rows;
  int lastRow = -1;

  while (!stream.atEnd()) {
    int r, c;
    QMap<int, QVariant> v;
    stream >> r >> c >> v;
    rows.insert(r);
    lastRow = r;
  }

  if (lastRow == -1)
    return false;

  int start = lastRow;
  while (rows.find(start - 1) != rows.cend())
    start -= 1;
  int count = lastRow - start + 1;
  while (rows.find(start + count) != rows.cend())
    count += 1;

  int dest = parent.row();
  if (dest >= start) {
    if (dest - start < count)
      return false;
    dest += 1;
  }

  g_MainWindow->pushUndoCommand(new LayerRowMoveCommand(m_node.get(), count > 1 ? tr("Move Layers") : tr("Move Layer"),
                                                        &static_cast<LayersEditor*>(QObject::parent())->m_tableView,
                                                        start, dest, count));

  return true;
}

class LayerRowUndoCommand : public EditorUndoCommand {
protected:
  LayersTableView* m_view;
  std::vector<std::pair<amuse::LayerMapping, int>> m_data;
  bool m_undid = false;
  void add() {
    m_view->selectionModel()->clearSelection();
    for (const auto& p : m_data) {
      static_cast<LayersModel*>(m_view->model())->_insertRow(p.second, p.first);
      m_view->setCurrentIndex(m_view->model()->index(p.second, 0));
    }
  }
  void del() {
    for (auto it = m_data.rbegin(); it != m_data.rend(); ++it) {
      it->first = static_cast<LayersModel*>(m_view->model())->_removeRow(it->second);
    }
  }
  void undo() override {
    m_undid = true;
    EditorUndoCommand::undo();
  }
  void redo() override {
    if (m_undid)
      EditorUndoCommand::redo();
  }

public:
  explicit LayerRowUndoCommand(ProjectModel::LayersNode* node, const QString& text, LayersTableView* view,
                               std::vector<std::pair<amuse::LayerMapping, int>>&& data)
  : EditorUndoCommand(node, text), m_view(view), m_data(std::move(data)) {}
};

class LayerRowAddUndoCommand : public LayerRowUndoCommand {
  using base = LayerRowUndoCommand;

public:
  explicit LayerRowAddUndoCommand(ProjectModel::LayersNode* node, const QString& text, LayersTableView* view,
                                  std::vector<std::pair<amuse::LayerMapping, int>>&& data)
  : LayerRowUndoCommand(node, text, view, std::move(data)) {}
  void undo() override {
    base::undo();
    base::del();
  }
  void redo() override {
    base::redo();
    base::add();
  }
};

class LayerRowDelUndoCommand : public LayerRowUndoCommand {
  using base = LayerRowUndoCommand;

public:
  explicit LayerRowDelUndoCommand(ProjectModel::LayersNode* node, const QString& text, LayersTableView* view,
                                  std::vector<std::pair<amuse::LayerMapping, int>>&& data)
  : LayerRowUndoCommand(node, text, view, std::move(data)) {}
  void undo() override {
    base::undo();
    base::add();
  }
  void redo() override {
    base::redo();
    base::del();
  }
};

bool LayersModel::insertRows(int row, int count, const QModelIndex& parent) {
  if (!m_node)
    return false;
  beginInsertRows(parent, row, row + count - 1);
  std::vector<amuse::LayerMapping>& layers = *m_node->m_obj;
  layers.insert(layers.begin() + row, count, amuse::LayerMapping());
  endInsertRows();
  return true;
}

bool LayersModel::moveRows(const QModelIndex& sourceParent, int sourceRow, int count,
                           const QModelIndex& destinationParent, int destinationChild) {
  if (!m_node) {
    return false;
  }

  const bool moving =
      beginMoveRows(sourceParent, sourceRow, sourceRow + count - 1, destinationParent, destinationChild);
  std::vector<amuse::LayerMapping>& layers = *m_node->m_obj;

  const auto pivot = layers.begin() + sourceRow;
  const auto begin = layers.begin() + destinationChild;
  const auto end = pivot + count;

  if (destinationChild < sourceRow) {
    std::rotate(begin, pivot, end);
  } else if (destinationChild > sourceRow) {
    std::rotate(pivot, end, begin);
  }

  if (moving) {
    endMoveRows();
  }
  return true;
}

bool LayersModel::removeRows(int row, int count, const QModelIndex& parent) {
  if (!m_node)
    return false;
  beginRemoveRows(parent, row, row + count - 1);
  std::vector<amuse::LayerMapping>& layers = *m_node->m_obj;
  layers.erase(layers.begin() + row, layers.begin() + row + count);
  endRemoveRows();
  return true;
}

void LayersModel::_insertRow(int row, const amuse::LayerMapping& data) {
  if (!m_node)
    return;
  beginInsertRows(QModelIndex(), row, row);
  std::vector<amuse::LayerMapping>& layers = *m_node->m_obj;
  layers.insert(layers.begin() + row, data);
  endInsertRows();
}

amuse::LayerMapping LayersModel::_removeRow(int row) {
  if (!m_node)
    return {};
  beginRemoveRows(QModelIndex(), row, row);
  std::vector<amuse::LayerMapping>& layers = *m_node->m_obj;
  amuse::LayerMapping ret = layers[row];
  layers.erase(layers.begin() + row);
  endRemoveRows();
  return ret;
}

LayersModel::LayersModel(QObject* parent) : QAbstractTableModel(parent) {}

LayersModel::~LayersModel() = default;

void LayersTableView::deleteSelection() {
  const QModelIndexList list = selectionModel()->selectedRows();
  if (list.isEmpty()) {
    return;
  }

  std::vector<std::pair<amuse::LayerMapping, int>> data;
  data.reserve(list.size());
  for (const QModelIndex& idx : list) {
    data.emplace_back(amuse::LayerMapping{}, idx.row());
  }

  std::sort(data.begin(), data.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
  g_MainWindow->pushUndoCommand(new LayerRowDelUndoCommand(static_cast<LayersModel*>(model())->m_node.get(),
                                                           data.size() > 1 ? tr("Delete Layers") : tr("Delete Layer"),
                                                           this, std::move(data)));
}

void LayersTableView::setModel(QAbstractItemModel* model) {
  QTableView::setModel(model);
  horizontalHeader()->setMinimumSectionSize(75);
  horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
  horizontalHeader()->resizeSection(1, 75);
  horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
  horizontalHeader()->resizeSection(2, 75);
  horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
  horizontalHeader()->resizeSection(3, 75);
  horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
  horizontalHeader()->resizeSection(4, 75);
  horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
  horizontalHeader()->resizeSection(5, 75);
  horizontalHeader()->setSectionResizeMode(6, QHeaderView::Fixed);
  horizontalHeader()->resizeSection(6, 75);
  horizontalHeader()->setSectionResizeMode(7, QHeaderView::Fixed);
  horizontalHeader()->resizeSection(7, 75);
}

LayersTableView::LayersTableView(QWidget* parent) : QTableView(parent) {
  setSelectionBehavior(QAbstractItemView::SelectRows);
  setSelectionMode(QAbstractItemView::ExtendedSelection);
  setDragDropMode(QAbstractItemView::InternalMove);
  setDefaultDropAction(Qt::MoveAction);
  setDragEnabled(true);
  setGridStyle(Qt::NoPen);

  m_signedDelegate.setItemEditorFactory(&m_signedFactory);
  m_unsignedDelegate.setItemEditorFactory(&m_unsignedFactory);

  setItemDelegateForColumn(0, &m_smDelegate);
  setItemDelegateForColumn(1, &m_unsignedDelegate);
  setItemDelegateForColumn(2, &m_unsignedDelegate);
  setItemDelegateForColumn(3, &m_signedDelegate);
  setItemDelegateForColumn(4, &m_unsignedDelegate);
  setItemDelegateForColumn(5, &m_signedDelegate);
  setItemDelegateForColumn(6, &m_unsignedDelegate);
  setItemDelegateForColumn(7, &m_unsignedDelegate);
}

LayersTableView::~LayersTableView() = default;

bool LayersEditor::loadData(ProjectModel::LayersNode* node) {
  m_model.loadData(node);
  return true;
}

void LayersEditor::unloadData() { m_model.unloadData(); }

ProjectModel::INode* LayersEditor::currentNode() const { return m_model.m_node.get(); }

void LayersEditor::resizeEvent(QResizeEvent* ev) {
  m_tableView.setGeometry(QRect({}, ev->size()));
  m_addRemoveButtons.move(0, ev->size().height() - 32);
}

void LayersEditor::rowsInserted(const QModelIndex& parent, int first, int last) {
  m_tableView.scrollTo(m_tableView.model()->index(first, 0));
}

void LayersEditor::rowsMoved(const QModelIndex& parent, int start, int end, const QModelIndex& destination, int row) {
  m_tableView.scrollTo(m_tableView.model()->index(row, 0));
}

void LayersEditor::doAdd() {
  const QModelIndex idx = m_tableView.selectionModel()->currentIndex();
  std::vector<std::pair<amuse::LayerMapping, int>> data;

  if (idx.isValid()) {
    data.emplace_back(amuse::LayerMapping{}, idx.row());
  } else {
    data.emplace_back(amuse::LayerMapping{}, m_model.rowCount() - 1);
  }

  g_MainWindow->pushUndoCommand(
      new LayerRowAddUndoCommand(m_model.m_node.get(), tr("Add Layer"), &m_tableView, std::move(data)));
}

void LayersEditor::doSelectionChanged() {
  m_addRemoveButtons.removeAction()->setDisabled(m_tableView.selectionModel()->selectedRows().isEmpty());
  g_MainWindow->updateFocus();
}

AmuseItemEditFlags LayersEditor::itemEditFlags() const {
  return m_tableView.selectionModel()->selectedRows().isEmpty() ? AmuseItemNone : AmuseItemDelete;
}

void LayersEditor::itemDeleteAction() { m_tableView.deleteSelection(); }

LayersEditor::LayersEditor(QWidget* parent)
: EditorWidget(parent), m_model(this), m_tableView(this), m_addRemoveButtons(this) {
  m_tableView.setModel(&m_model);
  connect(m_tableView.selectionModel(), &QItemSelectionModel::selectionChanged, this,
          &LayersEditor::doSelectionChanged);
  connect(&m_model, &LayersModel::rowsInserted, this, &LayersEditor::rowsInserted);
  connect(&m_model, &LayersModel::rowsMoved, this, &LayersEditor::rowsMoved);

  m_addRemoveButtons.addAction()->setToolTip(tr("Add new layer mapping"));
  connect(m_addRemoveButtons.addAction(), &QAction::triggered, this, &LayersEditor::doAdd);
  m_addRemoveButtons.removeAction()->setToolTip(tr("Remove selected layer mappings"));
  connect(m_addRemoveButtons.removeAction(), &QAction::triggered, this, &LayersEditor::itemDeleteAction);
}

LayersEditor::~LayersEditor() = default;
