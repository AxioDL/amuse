#include "SoundGroupEditor.hpp"
#include "MainWindow.hpp"

class SFXDataChangeUndoCommand : public EditorUndoCommand
{
    amuse::SFXId m_sfx;
    int m_column;
    int m_undoVal, m_redoVal;
    bool m_undid = false;
public:
    explicit SFXDataChangeUndoCommand(ProjectModel::SoundGroupNode* node, const QString& text,
                                      amuse::SFXId sfx, int column, int redoVal)
    : EditorUndoCommand(node, text), m_sfx(sfx), m_column(column), m_redoVal(redoVal) {}
    void undo()
    {
        m_undid = true;
        amuse::SFXGroupIndex& index = *static_cast<ProjectModel::SoundGroupNode*>(m_node.get())->m_index;
        auto& map = index.m_sfxEntries;
        amuse::SFXGroupIndex::SFXEntry& entry = map[m_sfx];

        switch (m_column)
        {
        case 1:
            entry.objId.id = m_undoVal;
            break;
        case 2:
            entry.priority = m_undoVal;
            break;
        case 3:
            entry.maxVoices = m_undoVal;
            break;
        case 4:
            entry.defVel = m_undoVal;
            break;
        case 5:
            entry.panning = m_undoVal;
            break;
        case 6:
            entry.defKey = m_undoVal;
            break;
        default:
            break;
        }

        EditorUndoCommand::undo();
    }
    void redo()
    {
        amuse::SFXGroupIndex& index = *static_cast<ProjectModel::SoundGroupNode*>(m_node.get())->m_index;
        auto& map = index.m_sfxEntries;
        amuse::SFXGroupIndex::SFXEntry& entry = map[m_sfx];

        switch (m_column)
        {
        case 1:
            m_undoVal = entry.objId.id;
            entry.objId.id = m_redoVal;
            break;
        case 2:
            m_undoVal = entry.priority;
            entry.priority = m_redoVal;
            break;
        case 3:
            m_undoVal = entry.maxVoices;
            entry.maxVoices = m_redoVal;
            break;
        case 4:
            m_undoVal = entry.defVel;
            entry.defVel = m_redoVal;
            break;
        case 5:
            m_undoVal = entry.panning;
            entry.panning = m_redoVal;
            break;
        case 6:
            m_undoVal = entry.defKey;
            entry.defKey = m_redoVal;
            break;
        default:
            break;
        }

        if (m_undid)
            EditorUndoCommand::redo();
    }
};

class SFXNameChangeUndoCommand : public EditorUndoCommand
{
    amuse::SFXId m_sfx;
    std::string m_undoVal, m_redoVal;
    bool m_undid = false;
public:
    explicit SFXNameChangeUndoCommand(ProjectModel::SoundGroupNode* node, const QString& text,
                                      amuse::SFXId sfx, std::string_view redoVal)
    : EditorUndoCommand(node, text), m_sfx(sfx), m_redoVal(redoVal) {}
    void undo()
    {
        m_undid = true;
        g_MainWindow->projectModel()->setIdDatabases(m_node.get());
        amuse::SFXGroupIndex& index = *static_cast<ProjectModel::SoundGroupNode*>(m_node.get())->m_index;
        auto& map = index.m_sfxEntries;

        amuse::SFXId::CurNameDB->rename(m_sfx, m_undoVal);

        EditorUndoCommand::undo();
    }
    void redo()
    {
        g_MainWindow->projectModel()->setIdDatabases(m_node.get());
        amuse::SFXGroupIndex& index = *static_cast<ProjectModel::SoundGroupNode*>(m_node.get())->m_index;
        auto& map = index.m_sfxEntries;

        m_undoVal = amuse::SFXId::CurNameDB->resolveNameFromId(m_sfx);
        amuse::SFXId::CurNameDB->rename(m_sfx, m_redoVal);

        if (m_undid)
            EditorUndoCommand::redo();
    }
};

SFXObjectDelegate::SFXObjectDelegate(QObject* parent)
: QStyledItemDelegate(parent) {}

QWidget* SFXObjectDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const SFXModel* model = static_cast<const SFXModel*>(index.model());
    ProjectModel::GroupNode* group = g_MainWindow->projectModel()->getGroupNode(model->m_node.get());
    EditorFieldPageObjectNode* cb = new EditorFieldPageObjectNode(group, parent);
    connect(cb, SIGNAL(currentIndexChanged(int)), this, SLOT(objIndexChanged()));
    return cb;
}

void SFXObjectDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    const SFXModel* model = static_cast<const SFXModel*>(index.model());
    auto entry = model->m_sorted[index.row()];
    ProjectModel::GroupNode* group = g_MainWindow->projectModel()->getGroupNode(model->m_node.get());
    ProjectModel::BasePoolObjectNode* node = group->pageObjectNodeOfId(entry->second.objId.id);
    int idx = 0;
    if (node)
        idx = g_MainWindow->projectModel()->getPageObjectProxy()->mapFromSource(g_MainWindow->projectModel()->index(node)).row();
    static_cast<EditorFieldPageObjectNode*>(editor)->setCurrentIndex(idx);
    if (static_cast<EditorFieldPageObjectNode*>(editor)->shouldPopupOpen())
        QApplication::postEvent(editor, new QEvent(QEvent::User));
}

void SFXObjectDelegate::setModelData(QWidget* editor, QAbstractItemModel* m, const QModelIndex& index) const
{
    const SFXModel* model = static_cast<const SFXModel*>(m);
    auto entry = model->m_sorted[index.row()];
    ProjectModel::BasePoolObjectNode* node = static_cast<EditorFieldPageObjectNode*>(editor)->currentNode();
    amuse::ObjectId id;
    if (node)
        id = node->id();
    if (id == entry->second.objId.id)
    {
        emit m->dataChanged(index, index);
        return;
    }
    g_MainWindow->pushUndoCommand(new SFXDataChangeUndoCommand(model->m_node.get(),
        tr("Change %1").arg(m->headerData(1, Qt::Horizontal).toString()), entry->first, 1, id.id));
    emit m->dataChanged(index, index);
}

void SFXObjectDelegate::objIndexChanged()
{
    emit commitData(static_cast<QWidget*>(sender()));
}

std::unordered_map<amuse::SFXId, amuse::SFXGroupIndex::SFXEntry>& SFXModel::_getMap() const
{
    return m_node->m_index->m_sfxEntries;
}

void SFXModel::_buildSortedList()
{
    m_sorted.clear();
    if (!m_node)
        return;
    auto& map = _getMap();
    m_sorted.reserve(map.size());
    for (auto it = map.begin() ; it != map.end() ; ++it)
        m_sorted.emplace_back(it);
    std::sort(m_sorted.begin(), m_sorted.end());
}

QModelIndex SFXModel::_indexOfSFX(amuse::SFXId sfx) const
{
    auto search = std::lower_bound(m_sorted.cbegin(), m_sorted.cend(), sfx);
    if (search == m_sorted.cend() || search->m_it->first != sfx)
        return QModelIndex();
    else
        return createIndex(search - m_sorted.begin(), 0);
}

int SFXModel::_hypotheticalIndexOfSFX(const std::string& sfxName) const
{
    auto search = std::lower_bound(m_sorted.cbegin(), m_sorted.cend(), sfxName);
    return search - m_sorted.begin();
}

void SFXModel::loadData(ProjectModel::SoundGroupNode* node)
{
    beginResetModel();
    m_node = node;
    _buildSortedList();
    endResetModel();
}

void SFXModel::unloadData()
{
    beginResetModel();
    m_node.reset();
    m_sorted.clear();
    endResetModel();
}

int SFXModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    if (!m_node)
        return 0;
    return int(m_sorted.size()) + 1;
}

int SFXModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return 7;
}

QVariant SFXModel::data(const QModelIndex& index, int role) const
{
    if (!m_node)
        return QVariant();
    if (index.row() == m_sorted.size())
        return QVariant();

    auto entry = m_sorted[index.row()];

    if (role == Qt::DisplayRole || role == Qt::EditRole)
    {
        switch (index.column())
        {
        case 0:
        {
            g_MainWindow->projectModel()->setIdDatabases(m_node.get());
            return amuse::SFXId::CurNameDB->resolveNameFromId(entry->first.id).data();
        }
        case 1:
        {
            ProjectModel::GroupNode* group = g_MainWindow->projectModel()->getGroupNode(m_node.get());
            if (ProjectModel::BasePoolObjectNode* node = group->pageObjectNodeOfId(entry->second.objId.id))
                return node->text();
            return QVariant();
        }
        case 2:
            return entry->second.priority;
        case 3:
            return entry->second.maxVoices;
        case 4:
            return entry->second.defVel;
        case 5:
            return entry->second.panning;
        case 6:
            return entry->second.defKey;
        default:
            break;
        }
    }

    return QVariant();
}

bool SFXModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!m_node || role != Qt::EditRole)
        return false;

    auto& map = _getMap();
    auto entry = m_sorted[index.row()];

    switch (index.column())
    {
    case 0:
    {
        g_MainWindow->projectModel()->setIdDatabases(m_node.get());
        auto utf8key = value.toString().toUtf8();
        std::unordered_map<std::string, amuse::ObjectId>::iterator idIt;
        if ((idIt = amuse::SFXId::CurNameDB->m_stringToId.find(utf8key.data())) != amuse::SFXId::CurNameDB->m_stringToId.cend())
        {
            if (idIt->second == entry->first)
                return false;
            QMessageBox::critical(g_MainWindow, tr("SFX Conflict"),
                                  tr("SFX %1 is already defined in project").arg(value.toString()));
            return false;
        }
        emit layoutAboutToBeChanged();
        g_MainWindow->pushUndoCommand(new SFXNameChangeUndoCommand(m_node.get(),
            tr("Change SFX Name"), entry->first, utf8key.data()));
        _buildSortedList();
        QModelIndex newIndex = _indexOfSFX(entry->first);
        changePersistentIndex(index, newIndex);
        emit layoutChanged();
        emit dataChanged(newIndex, newIndex, {Qt::DisplayRole, Qt::EditRole});
        return true;
    }
    case 2:
        if (entry->second.priority == value.toInt())
            return false;
        break;
    case 3:
        if (entry->second.maxVoices == value.toInt())
            return false;
        break;
    case 4:
        if (entry->second.defVel == value.toInt())
            return false;
        break;
    case 5:
        if (entry->second.panning == value.toInt())
            return false;
        break;
    case 6:
        if (entry->second.defKey == value.toInt())
            return false;
        break;
    default:
        return false;
    }

    g_MainWindow->pushUndoCommand(new SFXDataChangeUndoCommand(m_node.get(),
        tr("Change %1").arg(headerData(index.column(), Qt::Horizontal).toString()),
        entry->first, index.column(), value.toInt()));
    emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});

    return true;
}

QVariant SFXModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        switch (section)
        {
        case 0:
            return tr("SFX");
        case 1:
            return tr("Object");
        case 2:
            return tr("Priority");
        case 3:
            return tr("Max Voices");
        case 4:
            return tr("Velocity");
        case 5:
            return tr("Panning");
        case 6:
            return tr("Key");
        default:
            break;
        }
    }
    return QVariant();
}

Qt::ItemFlags SFXModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    if (index.row() == m_sorted.size())
        return Qt::NoItemFlags;
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
}

class SFXRowUndoCommand : public EditorUndoCommand
{
protected:
    SFXTableView* m_view;
    std::vector<std::tuple<amuse::SFXId, std::string, amuse::SFXGroupIndex::SFXEntry>> m_data;
    bool m_undid = false;
    void add()
    {
        m_view->selectionModel()->clearSelection();
        for (auto& p : m_data)
        {
            int row = static_cast<SFXModel*>(m_view->model())->_insertRow(p);
            m_view->selectionModel()->select(QItemSelection(
                m_view->model()->index(row, 0), m_view->model()->index(row, 6)),
                                               QItemSelectionModel::SelectCurrent);
        }
    }
    void del()
    {
        for (auto it = m_data.rbegin(); it != m_data.rend(); ++it)
        {
            *it = static_cast<SFXModel*>(m_view->model())->_removeRow(std::get<0>(*it));
        }
    }
    void undo()
    {
        m_undid = true;
        EditorUndoCommand::undo();
    }
    void redo()
    {
        if (m_undid)
            EditorUndoCommand::redo();
    }
public:
    explicit SFXRowUndoCommand(ProjectModel::SoundGroupNode* node, const QString& text, SFXTableView* view,
                               std::vector<std::tuple<amuse::SFXId, std::string, amuse::SFXGroupIndex::SFXEntry>>&& data)
        : EditorUndoCommand(node, text), m_view(view), m_data(std::move(data)) {}
};

class SFXRowAddUndoCommand : public SFXRowUndoCommand
{
    using base = SFXRowUndoCommand;
public:
    explicit SFXRowAddUndoCommand(ProjectModel::SoundGroupNode* node, const QString& text, SFXTableView* view,
                                  std::vector<std::tuple<amuse::SFXId, std::string, amuse::SFXGroupIndex::SFXEntry>>&& data)
    : SFXRowUndoCommand(node, text, view, std::move(data)) {}
    void undo() { base::undo(); base::del(); }
    void redo() { base::redo(); base::add(); }
};

class SFXRowDelUndoCommand : public SFXRowUndoCommand
{
    using base = SFXRowUndoCommand;
public:
    explicit SFXRowDelUndoCommand(ProjectModel::SoundGroupNode* node, const QString& text, SFXTableView* view,
                                  std::vector<std::tuple<amuse::SFXId, std::string, amuse::SFXGroupIndex::SFXEntry>>&& data)
    : SFXRowUndoCommand(node, text, view, std::move(data)) {}
    void undo() { base::undo(); base::add(); }
    void redo() { base::redo(); base::del(); }
};

int SFXModel::_insertRow(const std::tuple<amuse::SFXId, std::string, amuse::SFXGroupIndex::SFXEntry>& data)
{
    if (!m_node)
        return 0;
    auto& map = _getMap();
    g_MainWindow->projectModel()->setIdDatabases(m_node.get());
    amuse::SFXId::CurNameDB->registerPair(std::get<1>(data), std::get<0>(data));
    int idx = _hypotheticalIndexOfSFX(std::get<1>(data));
    beginInsertRows(QModelIndex(), idx, idx);
    map.emplace(std::make_pair(std::get<0>(data), std::get<2>(data)));
    _buildSortedList();
    endInsertRows();
    return idx;
}

std::tuple<amuse::SFXId, std::string, amuse::SFXGroupIndex::SFXEntry> SFXModel::_removeRow(amuse::SFXId sfx)
{
    std::tuple<amuse::SFXId, std::string, amuse::SFXGroupIndex::SFXEntry> ret;
    if (!m_node)
        return ret;
    auto& map = _getMap();
    g_MainWindow->projectModel()->setIdDatabases(m_node.get());
    int idx = _indexOfSFX(sfx).row();
    beginRemoveRows(QModelIndex(), idx, idx);
    std::get<0>(ret) = sfx;
    std::get<1>(ret) = amuse::SFXId::CurNameDB->resolveNameFromId(sfx);
    auto search = map.find(sfx);
    if (search != map.cend())
    {
        std::get<2>(ret) = search->second;
        amuse::SFXId::CurNameDB->remove(sfx);
        map.erase(search);
    }
    _buildSortedList();
    endRemoveRows();
    return ret;
}

SFXModel::SFXModel(QObject* parent)
: QAbstractTableModel(parent)
{}

void SFXTableView::deleteSelection()
{
    QModelIndexList list = selectionModel()->selectedRows();
    if (list.isEmpty())
        return;
    std::sort(list.begin(), list.end(), [](const auto& a, const auto& b) { return a.row() < b.row(); });
    std::vector<std::tuple<amuse::SFXId, std::string, amuse::SFXGroupIndex::SFXEntry>> data;
    data.reserve(list.size());
    for (QModelIndex idx : list)
    {
        auto& entry = *static_cast<SFXModel*>(model())->m_sorted[idx.row()].m_it;
        data.push_back({entry.first, {}, {}});
    }
    g_MainWindow->pushUndoCommand(
        new SFXRowDelUndoCommand(static_cast<SFXModel*>(model())->m_node.get(),
        data.size() > 1 ? tr("Delete SFX Entries") : tr("Delete SFX Entry"), this, std::move(data)));
}

void SFXTableView::setModel(QAbstractItemModel* model)
{
    QTableView::setModel(model);
    horizontalHeader()->setMinimumSectionSize(75);
    horizontalHeader()->resizeSection(0, 200);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    horizontalHeader()->resizeSection(2, 75);
    horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    horizontalHeader()->resizeSection(3, 100);
    horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    horizontalHeader()->resizeSection(4, 75);
    horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    horizontalHeader()->resizeSection(5, 75);
    horizontalHeader()->setSectionResizeMode(6, QHeaderView::Fixed);
    horizontalHeader()->resizeSection(6, 75);
}

SFXTableView::SFXTableView(QWidget* parent)
: QTableView(parent)
{
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setGridStyle(Qt::NoPen);

    m_127Delegate.setItemEditorFactory(&m_127Factory);
    m_255Delegate.setItemEditorFactory(&m_255Factory);

    setItemDelegateForColumn(1, &m_sfxDelegate);
    setItemDelegateForColumn(2, &m_255Delegate);
    setItemDelegateForColumn(3, &m_255Delegate);
    setItemDelegateForColumn(4, &m_127Delegate);
    setItemDelegateForColumn(5, &m_127Delegate);
    setItemDelegateForColumn(6, &m_127Delegate);
}

void SFXPlayerWidget::clicked()
{
    if (!m_vox)
    {
        m_vox = g_MainWindow->startSFX(m_groupId, m_sfxId);
        if (m_vox)
        {
            m_playAction.setText(tr("Stop"));
            m_playAction.setIcon(QIcon(QStringLiteral(":/icons/IconStop.svg")));
        }
    }
    else
    {
        stopped();
    }
}

void SFXPlayerWidget::stopped()
{
    m_vox->keyOff();
    m_vox.reset();
    m_playAction.setText(tr("Play"));
    m_playAction.setIcon(QIcon(QStringLiteral(":/icons/IconSoundMacro.svg")));
}

void SFXPlayerWidget::resizeEvent(QResizeEvent* event)
{
    m_button.setGeometry(event->size().width() - event->size().height(), 0, event->size().height(), event->size().height());
}

void SFXPlayerWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    qobject_cast<QTableView*>(parentWidget()->parentWidget())->setIndexWidget(m_index, nullptr);
    event->ignore();
}

SFXPlayerWidget::~SFXPlayerWidget()
{
    if (m_vox)
        m_vox->keyOff();
}

SFXPlayerWidget::SFXPlayerWidget(QModelIndex index, amuse::GroupId gid, amuse::SFXId id, QWidget* parent)
: QWidget(parent), m_button(this), m_playAction(tr("Play")), m_index(index),
  m_groupId(gid), m_sfxId(id)
{
    m_playAction.setIcon(QIcon(QStringLiteral(":/icons/IconSoundMacro.svg")));
    m_button.setDefaultAction(&m_playAction);
    connect(&m_playAction, SIGNAL(triggered()), this, SLOT(clicked()));
}

bool SoundGroupEditor::loadData(ProjectModel::SoundGroupNode* node)
{
    m_sfxs.loadData(node);
    return true;
}

void SoundGroupEditor::unloadData()
{
    m_sfxs.unloadData();
}

ProjectModel::INode* SoundGroupEditor::currentNode() const
{
    return m_sfxs.m_node.get();
}

void SoundGroupEditor::resizeEvent(QResizeEvent* ev)
{
    m_sfxTable->setGeometry(QRect({}, ev->size()));
    m_addRemoveButtons.move(0, ev->size().height() - 32);
}

AmuseItemEditFlags SoundGroupEditor::itemEditFlags() const
{
    return (m_sfxTable->hasFocus() && !m_sfxTable->selectionModel()->selectedRows().isEmpty()) ? AmuseItemDelete : AmuseItemNone;
}

void SoundGroupEditor::doAdd()
{
    g_MainWindow->projectModel()->setIdDatabases(m_sfxs.m_node.get());
    std::vector<std::tuple<amuse::SFXId, std::string, amuse::SFXGroupIndex::SFXEntry>> data;
    amuse::SFXId sfxId = amuse::SFXId::CurNameDB->generateId(amuse::NameDB::Type::SFX);
    std::string sfxName = amuse::SFXId::CurNameDB->generateName(sfxId, amuse::NameDB::Type::SFX);
    data.push_back(std::make_tuple(sfxId, sfxName, amuse::SFXGroupIndex::SFXEntry{}));
    g_MainWindow->pushUndoCommand(
        new SFXRowAddUndoCommand(m_sfxs.m_node.get(), tr("Add SFX Entry"), m_sfxTable, std::move(data)));
}

void SoundGroupEditor::doSelectionChanged()
{
    m_addRemoveButtons.removeAction()->setDisabled(m_sfxTable->selectionModel()->selectedRows().isEmpty());
    g_MainWindow->updateFocus();
}

void SoundGroupEditor::sfxDataChanged()
{
    int idx = 0;
    for (const auto& p : m_sfxs.m_sorted)
    {
        QModelIndex index = m_sfxs.index(idx, 1);
        SFXPlayerWidget* w = qobject_cast<SFXPlayerWidget*>(m_sfxTable->indexWidget(index));
        if (!w || w->sfxId() != p->first)
        {
            SFXPlayerWidget* newW = new SFXPlayerWidget(index, m_sfxs.m_node->m_id, p->first, m_sfxTable->viewport());
            m_sfxTable->setIndexWidget(index, newW);
        }
        ++idx;
    }
}

void SoundGroupEditor::itemDeleteAction()
{
    m_sfxTable->deleteSelection();
}

SoundGroupEditor::SoundGroupEditor(QWidget* parent)
: EditorWidget(parent), m_sfxs(this), m_sfxTable(new SFXTableView(this)), m_addRemoveButtons(this)
{
    m_sfxTable->setModel(&m_sfxs);
    connect(m_sfxTable->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)),
            this, SLOT(doSelectionChanged()));

    connect(&m_sfxs, SIGNAL(rowsInserted(const QModelIndex&, int, int)),
            this, SLOT(sfxDataChanged()));
    connect(&m_sfxs, SIGNAL(rowsRemoved(const QModelIndex&, int, int)),
            this, SLOT(sfxDataChanged()));
    connect(&m_sfxs, SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&, const QVector<int>&)),
            this, SLOT(sfxDataChanged()));
    connect(&m_sfxs, SIGNAL(layoutChanged(const QList<QPersistentModelIndex>&, QAbstractItemModel::LayoutChangeHint)),
            this, SLOT(sfxDataChanged()));
    connect(&m_sfxs, SIGNAL(modelReset()),
            this, SLOT(sfxDataChanged()));

    m_addRemoveButtons.addAction()->setToolTip(tr("Add new SFX entry"));
    connect(m_addRemoveButtons.addAction(), SIGNAL(triggered(bool)), this, SLOT(doAdd()));
    m_addRemoveButtons.removeAction()->setToolTip(tr("Remove selected SFX entries"));
    connect(m_addRemoveButtons.removeAction(), SIGNAL(triggered(bool)), this, SLOT(itemDeleteAction()));
}
