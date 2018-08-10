#include "SoundGroupEditor.hpp"
#include "MainWindow.hpp"

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
        static_cast<EditorFieldPageObjectNode*>(editor)->showPopup();
}

void SFXObjectDelegate::setModelData(QWidget* editor, QAbstractItemModel* m, const QModelIndex& index) const
{
    const SFXModel* model = static_cast<const SFXModel*>(m);
    auto entry = model->m_sorted[index.row()];
    int idx = static_cast<EditorFieldPageObjectNode*>(editor)->currentIndex();
    if (idx == 0)
    {
        entry->second.objId.id = amuse::ObjectId();
    }
    else
    {
        ProjectModel::BasePoolObjectNode* node = static_cast<ProjectModel::BasePoolObjectNode*>(
            g_MainWindow->projectModel()->node(
                g_MainWindow->projectModel()->getPageObjectProxy()->mapToSource(
                    g_MainWindow->projectModel()->getPageObjectProxy()->index(idx, 0,
                        static_cast<EditorFieldPageObjectNode*>(editor)->rootModelIndex()))));
        entry->second.objId.id = node->id();
    }
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
            g_MainWindow->projectModel()->getGroupNode(m_node.get())->getAudioGroup()->setIdDatabases();
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
        g_MainWindow->projectModel()->getGroupNode(m_node.get())->getAudioGroup()->setIdDatabases();
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
        amuse::SFXId::CurNameDB->rename(entry.m_it->first, utf8key.data());
        _buildSortedList();
        QModelIndex newIndex = _indexOfSFX(entry.m_it->first);
        changePersistentIndex(index, newIndex);
        emit layoutChanged();
        emit dataChanged(newIndex, newIndex, {Qt::DisplayRole, Qt::EditRole});
        return true;
    }
    case 2:
        entry->second.priority = value.toInt();
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
        return true;
    case 3:
        entry->second.maxVoices = value.toInt();
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
        return true;
    case 4:
        entry->second.defVel = value.toInt();
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
        return true;
    case 5:
        entry->second.panning = value.toInt();
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
        return true;
    case 6:
        entry->second.defKey = value.toInt();
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
        return true;
    default:
        break;
    }

    return false;
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

bool SFXModel::insertRows(int row, int count, const QModelIndex& parent)
{
    if (!m_node)
        return false;
    auto& map = _getMap();
    g_MainWindow->projectModel()->getGroupNode(m_node.get())->getAudioGroup()->setIdDatabases();
    for (int i = 0; i < count; ++i)
    {
        amuse::ObjectId sfxId = amuse::SFXId::CurNameDB->generateId(amuse::NameDB::Type::SFX);
        std::string sfxName = amuse::SFXId::CurNameDB->generateName(sfxId, amuse::NameDB::Type::SFX);
        int insertIdx = _hypotheticalIndexOfSFX(sfxName);
        beginInsertRows(parent, insertIdx, insertIdx);
        amuse::SFXId::CurNameDB->registerPair(sfxName, sfxId);
        map.emplace(std::make_pair(sfxId, amuse::SFXGroupIndex::SFXEntry{}));
        _buildSortedList();
        endInsertRows();
        ++row;
    }
    return true;
}

bool SFXModel::removeRows(int row, int count, const QModelIndex& parent)
{
    if (!m_node)
        return false;
    auto& map = _getMap();
    beginRemoveRows(parent, row, row + count - 1);
    std::vector<amuse::SFXId> removeSFXs;
    removeSFXs.reserve(count);
    for (int i = 0; i < count; ++i)
        removeSFXs.push_back(m_sorted[row+i].m_it->first);
    for (amuse::SFXId sfx : removeSFXs)
        map.erase(sfx);
    _buildSortedList();
    endRemoveRows();
    return true;
}

SFXModel::SFXModel(QObject* parent)
: QAbstractTableModel(parent)
{}

void SFXTableView::deleteSelection()
{
    QModelIndexList list;
    while (!(list = selectionModel()->selectedRows()).isEmpty())
        model()->removeRow(list.back().row());
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

bool SoundGroupEditor::isItemEditEnabled() const
{
    return m_sfxTable->hasFocus() && !m_sfxTable->selectionModel()->selectedRows().isEmpty();
}

void SoundGroupEditor::doAdd()
{
    QModelIndex idx = m_sfxTable->selectionModel()->currentIndex();
    if (!idx.isValid())
        m_sfxTable->model()->insertRow(m_sfxTable->model()->rowCount() - 1);
    else
        m_sfxTable->model()->insertRow(idx.row());
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
        if (!w || w->sfxId() != p.m_it->first)
        {
            SFXPlayerWidget* newW = new SFXPlayerWidget(index, m_sfxs.m_node->m_id, p.m_it->first);
            m_sfxTable->setIndexWidget(index, newW);
        }
        ++idx;
    }
}

void SoundGroupEditor::itemCutAction()
{

}

void SoundGroupEditor::itemCopyAction()
{

}

void SoundGroupEditor::itemPasteAction()
{

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
