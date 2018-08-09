#include "SongGroupEditor.hpp"
#include "MainWindow.hpp"

PageObjectDelegate::PageObjectDelegate(QObject* parent)
: QStyledItemDelegate(parent) {}

QWidget* PageObjectDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const PageModel* model = static_cast<const PageModel*>(index.model());
    ProjectModel::GroupNode* group = g_MainWindow->projectModel()->getGroupNode(model->m_node.get());
    EditorFieldPageObjectNode* cb = new EditorFieldPageObjectNode(group, parent);
    connect(cb, SIGNAL(currentIndexChanged(int)), this, SLOT(objIndexChanged()));
    return cb;
}

void PageObjectDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    const PageModel* model = static_cast<const PageModel*>(index.model());
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

void PageObjectDelegate::setModelData(QWidget* editor, QAbstractItemModel* m, const QModelIndex& index) const
{
    const PageModel* model = static_cast<const PageModel*>(m);
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

void PageObjectDelegate::objIndexChanged()
{
    emit commitData(static_cast<QWidget*>(sender()));
}

void MIDIFileFieldWidget::buttonPressed()
{
    m_dialog.setDirectory(QFileInfo(g_MainWindow->projectModel()->dir().absoluteFilePath(m_le.text())).path());
    m_dialog.open(this, SLOT(fileDialogOpened(const QString&)));
}

void MIDIFileFieldWidget::fileDialogOpened(const QString& path)
{
    m_le.setText(g_MainWindow->projectModel()->dir().relativeFilePath(path));
    emit pathChanged();
}

MIDIFileFieldWidget::MIDIFileFieldWidget(QWidget* parent)
: QWidget(parent), m_button(tr("Browse")),
  m_dialog(this, tr("Open Song File"), {}, QStringLiteral("Songs(*.mid *.son *.sng)"))
{
    QHBoxLayout* layout = new QHBoxLayout;
    layout->addWidget(&m_le);
    layout->addWidget(&m_button);
    layout->setContentsMargins(QMargins());
    layout->setSpacing(0);
    setLayout(layout);

    connect(&m_le, SIGNAL(returnPressed()), this, SIGNAL(pathChanged()));
    connect(&m_button, SIGNAL(clicked(bool)), this, SLOT(buttonPressed()));

    m_dialog.setFileMode(QFileDialog::ExistingFile);
}

QWidget* MIDIFileDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    MIDIFileFieldWidget* field = new MIDIFileFieldWidget(parent);
    connect(field, SIGNAL(pathChanged()), this, SLOT(pathChanged()));
    return field;
}

void MIDIFileDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    MIDIFileFieldWidget* widget = static_cast<MIDIFileFieldWidget*>(editor);
    const SetupListModel* model = static_cast<const SetupListModel*>(index.model());
    auto entry = model->m_sorted[index.row()];
    widget->setPath(g_MainWindow->projectModel()->getMIDIPathOfSong(entry->first));
}

void MIDIFileDelegate::setModelData(QWidget* editor, QAbstractItemModel* m, const QModelIndex& index) const
{
    MIDIFileFieldWidget* widget = static_cast<MIDIFileFieldWidget*>(editor);
    const SetupListModel* model = static_cast<const SetupListModel*>(index.model());
    auto entry = model->m_sorted[index.row()];
    g_MainWindow->projectModel()->setMIDIPathOfSong(entry->first, widget->path());
    emit m->dataChanged(index, index);
}

void MIDIFileDelegate::pathChanged()
{
    emit commitData(static_cast<MIDIFileFieldWidget*>(sender()));
}

MIDIFileDelegate::MIDIFileDelegate(QObject* parent)
: QStyledItemDelegate(parent) {}

std::unordered_map<uint8_t, amuse::SongGroupIndex::PageEntry>& PageModel::_getMap() const
{
    return m_drum ? m_node->m_index->m_drumPages : m_node->m_index->m_normPages;
}

void PageModel::_buildSortedList()
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

QModelIndex PageModel::_indexOfProgram(uint8_t prog) const
{
    auto search = std::lower_bound(m_sorted.cbegin(), m_sorted.cend(), prog);
    if (search == m_sorted.cend() || search->m_it->first != prog)
        return QModelIndex();
    else
        return createIndex(search - m_sorted.begin(), 0);
}

int PageModel::_hypotheticalIndexOfProgram(uint8_t prog) const
{
    auto search = std::lower_bound(m_sorted.cbegin(), m_sorted.cend(), prog);
    return search - m_sorted.begin();
}

void PageModel::loadData(ProjectModel::SongGroupNode* node)
{
    beginResetModel();
    m_node = node;
    _buildSortedList();
    endResetModel();
}

void PageModel::unloadData()
{
    beginResetModel();
    m_node.reset();
    m_sorted.clear();
    endResetModel();
}

int PageModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    if (!m_node)
        return 0;
    return int(m_sorted.size()) + 1;
}

int PageModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return 4;
}

QVariant PageModel::data(const QModelIndex& index, int role) const
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
            return entry->first;
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
        default:
            break;
        }
    }

    return QVariant();
}

bool PageModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!m_node || role != Qt::EditRole)
        return false;

    auto& map = _getMap();
    auto entry = m_sorted[index.row()];

    switch (index.column())
    {
    case 0:
    {
        if (value.toInt() == entry->first)
            return false;
        if (map.find(value.toInt()) != map.cend())
        {
            QMessageBox::critical(g_MainWindow, tr("Program Conflict"),
                tr("Program %1 is already defined in table").arg(value.toInt()));
            return false;
        }
        emit layoutAboutToBeChanged();
        auto nh = map.extract(entry->first);
        nh.key() = value.toInt();
        map.insert(std::move(nh));
        _buildSortedList();
        QModelIndex newIndex = _indexOfProgram(value.toInt());
        changePersistentIndex(index, newIndex);
        emit layoutChanged();
        emit dataChanged(newIndex, newIndex);
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
    default:
        break;
    }

    return false;
}

QVariant PageModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        switch (section)
        {
        case 0:
            return tr("Program");
        case 1:
            return tr("Object");
        case 2:
            return tr("Priority");
        case 3:
            return tr("Max Voices");
        default:
            break;
        }
    }
    return QVariant();
}

Qt::ItemFlags PageModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    if (index.row() == m_sorted.size())
        return Qt::NoItemFlags;
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
}

bool PageModel::insertRows(int row, int count, const QModelIndex& parent)
{
    if (!m_node)
        return false;
    if (m_sorted.size() >= 128)
        return false;
    auto& map = _getMap();
    if (m_sorted.empty())
    {
        beginInsertRows(parent, 0, count - 1);
        for (int i = 0; i < count; ++i)
            map.emplace(std::make_pair(i, amuse::SongGroupIndex::PageEntry{}));
        _buildSortedList();
        endInsertRows();
        return true;
    }
    for (int i = 0; i < count; ++i)
    {
        int prog = -1;
        if (row < m_sorted.size())
        {
            prog = m_sorted[row].m_it->first;
            while (prog >= 0 && _indexOfProgram(prog).isValid())
                --prog;
        }
        if (prog == -1)
        {
            prog = 0;
            while (prog < 128 && _indexOfProgram(prog).isValid())
                ++prog;
        }
        if (prog == 128)
            return true;
        int insertIdx = _hypotheticalIndexOfProgram(prog);
        beginInsertRows(parent, insertIdx, insertIdx);
        map.emplace(std::make_pair(prog, amuse::SongGroupIndex::PageEntry{}));
        _buildSortedList();
        endInsertRows();
        ++row;
    }
    return true;
}

bool PageModel::removeRows(int row, int count, const QModelIndex& parent)
{
    if (!m_node)
        return false;
    auto& map = _getMap();
    beginRemoveRows(parent, row, row + count - 1);
    std::vector<uint8_t> removeProgs;
    removeProgs.reserve(count);
    for (int i = 0; i < count; ++i)
        removeProgs.push_back(m_sorted[row+i].m_it->first);
    for (uint8_t prog : removeProgs)
        map.erase(prog);
    _buildSortedList();
    endRemoveRows();
    return true;
}

PageModel::PageModel(bool drum, QObject* parent)
: QAbstractTableModel(parent), m_drum(drum)
{}

std::unordered_map<amuse::SongId, std::array<amuse::SongGroupIndex::MIDISetup, 16>>& SetupListModel::_getMap() const
{
    return m_node->m_index->m_midiSetups;
}

void SetupListModel::_buildSortedList()
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

QModelIndex SetupListModel::_indexOfSong(amuse::SongId id) const
{
    auto search = std::lower_bound(m_sorted.cbegin(), m_sorted.cend(), id);
    if (search == m_sorted.cend() || search->m_it->first != id)
        return QModelIndex();
    else
        return createIndex(search - m_sorted.begin(), 0);
}

int SetupListModel::_hypotheticalIndexOfSong(const std::string& songName) const
{
    auto search = std::lower_bound(m_sorted.cbegin(), m_sorted.cend(), songName);
    return search - m_sorted.begin();
}

void SetupListModel::loadData(ProjectModel::SongGroupNode* node)
{
    beginResetModel();
    m_node = node;
    g_MainWindow->projectModel()->getGroupNode(m_node.get())->getAudioGroup()->setIdDatabases();
    _buildSortedList();
    endResetModel();
}

void SetupListModel::unloadData()
{
    beginResetModel();
    m_node = nullptr;
    m_sorted.clear();
    endResetModel();
}

int SetupListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    if (!m_node)
        return 0;
    return int(m_sorted.size()) + 1;
}

int SetupListModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return 2;
}

QVariant SetupListModel::data(const QModelIndex& index, int role) const
{
    if (!m_node)
        return QVariant();
    if (index.row() == m_sorted.size())
        return QVariant();

    auto entry = m_sorted[index.row()];

    if (role == Qt::DisplayRole || role == Qt::EditRole)
    {
        if (index.column() == 0)
        {
            g_MainWindow->projectModel()->getGroupNode(m_node.get())->getAudioGroup()->setIdDatabases();
            return amuse::SongId::CurNameDB->resolveNameFromId(entry->first.id).data();
        }
        else if (index.column() == 1)
        {
            return g_MainWindow->projectModel()->getMIDIPathOfSong(entry.m_it->first);
        }
    }

    return QVariant();
}

bool SetupListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!m_node || role != Qt::EditRole || index.column() != 0)
        return false;

    auto& map = _getMap();
    auto entry = m_sorted[index.row()];

    g_MainWindow->projectModel()->getGroupNode(m_node.get())->getAudioGroup()->setIdDatabases();
    auto utf8key = value.toString().toUtf8();
    std::unordered_map<std::string, amuse::ObjectId>::iterator idIt;
    if ((idIt = amuse::SongId::CurNameDB->m_stringToId.find(utf8key.data())) != amuse::SongId::CurNameDB->m_stringToId.cend())
    {
        if (idIt->second == entry->first)
            return false;
        QMessageBox::critical(g_MainWindow, tr("Song Conflict"),
                              tr("Song %1 is already defined in project").arg(value.toString()));
        return false;
    }
    emit layoutAboutToBeChanged();
    amuse::SongId::CurNameDB->rename(entry.m_it->first, utf8key.data());
    _buildSortedList();
    QModelIndex newIndex = _indexOfSong(entry.m_it->first);
    changePersistentIndex(index, newIndex);
    emit layoutChanged();
    emit dataChanged(newIndex, newIndex, {Qt::DisplayRole, Qt::EditRole});

    return true;
}

QVariant SetupListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        switch (section)
        {
        case 0:
            return tr("Song");
        case 1:
            return tr("MIDI File");
        default:
            break;
        }
    }
    return QVariant();
}

Qt::ItemFlags SetupListModel::flags(const QModelIndex& index) const
{
    if (index.row() == m_sorted.size())
        return Qt::NoItemFlags;
    return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
}

bool SetupListModel::insertRows(int row, int count, const QModelIndex& parent)
{
    if (!m_node)
        return false;
    auto& map = _getMap();
    g_MainWindow->projectModel()->getGroupNode(m_node.get())->getAudioGroup()->setIdDatabases();
    for (int i = 0; i < count; ++i)
    {
        amuse::ObjectId songId = amuse::SongId::CurNameDB->generateId(amuse::NameDB::Type::Song);
        std::string songName = amuse::SongId::CurNameDB->generateName(songId, amuse::NameDB::Type::Song);
        int insertIdx = _hypotheticalIndexOfSong(songName);
        beginInsertRows(parent, insertIdx, insertIdx);
        amuse::SongId::CurNameDB->registerPair(songName, songId);
        map.emplace(std::make_pair(songId, std::array<amuse::SongGroupIndex::MIDISetup, 16>{}));
        _buildSortedList();
        endInsertRows();
        ++row;
    }
    return true;
}

bool SetupListModel::removeRows(int row, int count, const QModelIndex& parent)
{
    if (!m_node)
        return false;
    auto& map = _getMap();
    g_MainWindow->projectModel()->getGroupNode(m_node.get())->getAudioGroup()->setIdDatabases();
    beginRemoveRows(parent, row, row + count - 1);
    std::vector<amuse::SongId> removeSongs;
    removeSongs.reserve(count);
    for (int i = 0; i < count; ++i)
        removeSongs.push_back(m_sorted[row+i].m_it->first);
    for (amuse::SongId song : removeSongs)
    {
        amuse::SongId::CurNameDB->remove(song);
        map.erase(song);
    }
    _buildSortedList();
    endRemoveRows();
    return true;
}

SetupListModel::SetupListModel(QObject* parent)
: QAbstractTableModel(parent)
{}

void SetupModel::loadData(std::pair<const amuse::SongId, std::array<amuse::SongGroupIndex::MIDISetup, 16>>* data)
{
    beginResetModel();
    m_data = data;
    endResetModel();
}

void SetupModel::unloadData()
{
    beginResetModel();
    m_data = nullptr;
    endResetModel();
}

int SetupModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    if (!m_data)
        return 0;
    return 16;
}

int SetupModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return 5;
}

QVariant SetupModel::data(const QModelIndex& index, int role) const
{
    if (!m_data)
        return QVariant();

    auto& entry = m_data->second[index.row()];

    if (role == Qt::DisplayRole || role == Qt::EditRole)
    {
        switch (index.column())
        {
        case 0:
            return entry.programNo;
        case 1:
            return entry.volume;
        case 2:
            return entry.panning;
        case 3:
            return entry.reverb;
        case 4:
            return entry.chorus;
        default:
            break;
        }
    }

    return QVariant();
}

bool SetupModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!m_data || role != Qt::EditRole)
        return false;

    auto& entry = m_data->second[index.row()];

    switch (index.column())
    {
    case 0:
        entry.programNo = value.toInt();
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
        return true;
    case 1:
        entry.volume = value.toInt();
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
        return true;
    case 2:
        entry.panning = value.toInt();
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
        return true;
    case 3:
        entry.reverb = value.toInt();
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
        return true;
    case 4:
        entry.chorus = value.toInt();
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
        return true;
    default:
        break;
    }

    return false;
}

QVariant SetupModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole)
    {
        if (orientation == Qt::Horizontal)
        {
            switch (section)
            {
            case 0:
                return tr("Program");
            case 1:
                return tr("Volume");
            case 2:
                return tr("Panning");
            case 3:
                return tr("Reverb");
            case 4:
                return tr("Chorus");
            default:
                break;
            }
        }
        else
        {
            return section + 1;
        }
    }
    else if (role == Qt::BackgroundColorRole && orientation == Qt::Vertical)
    {
        if (section == 9)
            return QColor(64, 0, 0);
        return QColor(0, 64, 0);
    }
    return QVariant();
}

Qt::ItemFlags SetupModel::flags(const QModelIndex& index) const
{
    return Qt::ItemIsEnabled | Qt::ItemIsEditable;
}

SetupModel::SetupModel(QObject* parent)
: QAbstractTableModel(parent)
{}

void PageTableView::deleteSelection()
{
    QModelIndexList list;
    while (!(list = selectionModel()->selectedRows()).isEmpty())
        model()->removeRow(list.back().row());
}

void PageTableView::setModel(QAbstractItemModel* model)
{
    QTableView::setModel(model);
    horizontalHeader()->setMinimumSectionSize(75);
    horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    horizontalHeader()->resizeSection(0, 75);
    horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    horizontalHeader()->resizeSection(2, 75);
    horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    horizontalHeader()->resizeSection(3, 100);
}

PageTableView::PageTableView(QWidget* parent)
: QTableView(parent)
{
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setGridStyle(Qt::NoPen);

    m_127Delegate.setItemEditorFactory(&m_127Factory);
    m_255Delegate.setItemEditorFactory(&m_255Factory);

    setItemDelegateForColumn(0, &m_127Delegate);
    setItemDelegateForColumn(1, &m_poDelegate);
    setItemDelegateForColumn(2, &m_255Delegate);
    setItemDelegateForColumn(3, &m_255Delegate);
}

void SetupTableView::setModel(QAbstractItemModel* list, QAbstractItemModel* table)
{
    {
        m_listView.setModel(list);
        auto hheader = m_listView.horizontalHeader();
        hheader->setMinimumSectionSize(200);
        hheader->resizeSection(0, 200);
        hheader->setSectionResizeMode(1, QHeaderView::Stretch);
    }
    {
        m_tableView.setModel(table);
        auto hheader = m_tableView.horizontalHeader();
        hheader->setSectionResizeMode(QHeaderView::Stretch);
    }
}

void SetupTableView::deleteSelection()
{
    QModelIndexList list;
    while (!(list = m_listView.selectionModel()->selectedRows()).isEmpty())
        m_listView.model()->removeRow(list.back().row());
}

void SetupTableView::showEvent(QShowEvent* event)
{
    setSizes({width() - 375, 375});
}

SetupTableView::SetupTableView(QWidget* parent)
: QSplitter(parent), m_listView(this), m_tableView(this)
{
    setChildrenCollapsible(false);
    setStretchFactor(0, 1);
    setStretchFactor(1, 0);

    addWidget(&m_listView);
    addWidget(&m_tableView);

    m_listView.setSelectionBehavior(QAbstractItemView::SelectRows);
    m_listView.setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_listView.setGridStyle(Qt::NoPen);
    m_listView.setItemDelegateForColumn(1, &m_midiDelegate);

    m_tableView.setSelectionMode(QAbstractItemView::NoSelection);
    m_tableView.setGridStyle(Qt::NoPen);

    m_127Delegate.setItemEditorFactory(&m_127Factory);

    m_tableView.setItemDelegateForColumn(0, &m_127Delegate);
    m_tableView.setItemDelegateForColumn(1, &m_127Delegate);
    m_tableView.setItemDelegateForColumn(2, &m_127Delegate);
    m_tableView.setItemDelegateForColumn(3, &m_127Delegate);
    m_tableView.setItemDelegateForColumn(4, &m_127Delegate);
}

void ColoredTabBarStyle::drawControl(QStyle::ControlElement element, const QStyleOption *option,
                                     QPainter *painter, const QWidget *widget) const
{
    if (element == QStyle::CE_TabBarTab)
    {
        QStyleOptionTab optionTab = *static_cast<const QStyleOptionTab*>(option);
        switch (optionTab.position)
        {
        case QStyleOptionTab::Beginning:
            optionTab.palette.setColor(QPalette::Button, QColor(0, 64, 0));
            break;
        case QStyleOptionTab::Middle:
            optionTab.palette.setColor(QPalette::Button, QColor(64, 0, 0));
            break;
        default:
            break;
        }
        QProxyStyle::drawControl(element, &optionTab, painter, widget);
    }
    else
        QProxyStyle::drawControl(element, option, painter, widget);
}

ColoredTabBar::ColoredTabBar(QWidget* parent)
: QTabBar(parent), m_style(style())
{
    setDrawBase(false);
    setStyle(&m_style);
}

ColoredTabWidget::ColoredTabWidget(QWidget* parent)
: QTabWidget(parent)
{
    setTabBar(&m_tabBar);
}

bool SongGroupEditor::loadData(ProjectModel::SongGroupNode* node)
{
    m_normPages.loadData(node);
    m_drumPages.loadData(node);
    m_setupList.loadData(node);
    m_setup.unloadData();
    return true;
}

void SongGroupEditor::unloadData()
{
    m_normPages.unloadData();
    m_drumPages.unloadData();
    m_setupList.unloadData();
    m_setup.unloadData();
}

ProjectModel::INode* SongGroupEditor::currentNode() const
{
    return m_normPages.m_node.get();
}

void SongGroupEditor::resizeEvent(QResizeEvent* ev)
{
    m_tabs.setGeometry(QRect({}, ev->size()));
    m_addButton.move(0, ev->size().height() - 32);
    m_removeButton.move(32, ev->size().height() - 32);
}

void SongGroupEditor::doAdd()
{
    if (PageTableView* table = qobject_cast<PageTableView*>(m_tabs.currentWidget()))
    {
        QModelIndex idx = table->selectionModel()->currentIndex();
        if (!idx.isValid())
            table->model()->insertRow(table->model()->rowCount() - 1);
        else
            table->model()->insertRow(idx.row());
        if (PageTableView* ctable = qobject_cast<PageTableView*>(table))
            m_addAction.setDisabled(ctable->model()->rowCount() >= 128);
    }
    else if (SetupTableView* table = qobject_cast<SetupTableView*>(m_tabs.currentWidget()))
    {
        QModelIndex idx = table->m_listView.selectionModel()->currentIndex();
        if (!idx.isValid())
            table->m_listView.model()->insertRow(table->m_listView.model()->rowCount() - 1);
        else
            table->m_listView.model()->insertRow(idx.row());
    }
}

void SongGroupEditor::doSelectionChanged(const QItemSelection& selected)
{
    m_removeAction.setDisabled(selected.isEmpty());
    g_MainWindow->updateFocus();
}

void SongGroupEditor::doSetupSelectionChanged(const QItemSelection& selected)
{
    doSelectionChanged(selected);
    if (selected.indexes().isEmpty() || m_setupList.m_sorted.empty())
    {
        m_setup.unloadData();
    }
    else
    {
        auto entry = m_setupList.m_sorted[selected.indexes().last().row()];
        m_setup.loadData(&*entry.m_it);
    }
}

void SongGroupEditor::currentTabChanged(int idx)
{
    if (PageTableView* table = qobject_cast<PageTableView*>(m_tabs.currentWidget()))
    {
        m_addAction.setDisabled(table->model()->rowCount() >= 128);
        doSelectionChanged(table->selectionModel()->selection());
    }
    else if (SetupTableView* table = qobject_cast<SetupTableView*>(m_tabs.currentWidget()))
    {
        m_addAction.setDisabled(false);
        doSelectionChanged(table->m_listView.selectionModel()->selection());
    }
}

void SongGroupEditor::rowsAboutToBeRemoved(const QModelIndex &parent, int first, int last)
{
    for (int i = first; i <= last; ++i)
    {
        auto entry = m_setupList.m_sorted[i];
        if (&*entry.m_it == m_setup.m_data)
        {
            m_setup.unloadData();
            return;
        }
    }
}

void SongGroupEditor::modelAboutToBeReset()
{
    m_setup.unloadData();
}

bool SongGroupEditor::isItemEditEnabled() const
{
    if (PageTableView* table = qobject_cast<PageTableView*>(m_tabs.currentWidget()))
        return table->hasFocus() && !table->selectionModel()->selectedRows().isEmpty();
    else if (SetupTableView* table = qobject_cast<SetupTableView*>(m_tabs.currentWidget()))
        return table->m_listView.hasFocus() && !table->m_listView.selectionModel()->selectedRows().isEmpty();
    return false;
}

void SongGroupEditor::itemCutAction()
{

}

void SongGroupEditor::itemCopyAction()
{

}

void SongGroupEditor::itemPasteAction()
{

}

void SongGroupEditor::itemDeleteAction()
{
    if (PageTableView* table = qobject_cast<PageTableView*>(m_tabs.currentWidget()))
        table->deleteSelection();
    else if (SetupTableView* table = qobject_cast<SetupTableView*>(m_tabs.currentWidget()))
        table->deleteSelection();
}

SongGroupEditor::SongGroupEditor(QWidget* parent)
: EditorWidget(parent), m_normPages(false, this), m_drumPages(true, this), m_setup(this),
  m_normTable(this), m_drumTable(this), m_setupTable(this), m_tabs(this),
  m_addAction(tr("Add Row")), m_addButton(this), m_removeAction(tr("Remove Row")), m_removeButton(this)
{
    m_tabs.addTab(&m_normTable, tr("Normal Pages"));
    m_tabs.addTab(&m_drumTable, tr("Drum Pages"));
    m_tabs.addTab(&m_setupTable, tr("MIDI Setups"));

    connect(&m_tabs, SIGNAL(currentChanged(int)), this, SLOT(currentTabChanged(int)));

    connect(&m_setupList, SIGNAL(rowsAboutToBeRemoved(const QModelIndex&, int, int)),
            this, SLOT(rowsAboutToBeRemoved(const QModelIndex&, int, int)));
    connect(&m_setupList, SIGNAL(modelAboutToBeReset()),
            this, SLOT(modelAboutToBeReset()));

    m_normTable.setModel(&m_normPages);
    m_drumTable.setModel(&m_drumPages);
    m_setupTable.setModel(&m_setupList, &m_setup);
    connect(m_normTable.selectionModel(), SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)),
            this, SLOT(doSelectionChanged(const QItemSelection&)));
    connect(m_drumTable.selectionModel(), SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)),
            this, SLOT(doSelectionChanged(const QItemSelection&)));
    connect(m_setupTable.m_listView.selectionModel(), SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)),
            this, SLOT(doSetupSelectionChanged(const QItemSelection&)));

    m_addAction.setIcon(QIcon(QStringLiteral(":/icons/IconAdd.svg")));
    m_addButton.setDefaultAction(&m_addAction);
    m_addAction.setToolTip(tr("Add new page entry"));
    m_addButton.setFixedSize(32, 32);
    connect(&m_addAction, SIGNAL(triggered(bool)), this, SLOT(doAdd()));

    m_removeAction.setIcon(QIcon(QStringLiteral(":/icons/IconRemove.svg")));
    m_removeButton.setDefaultAction(&m_removeAction);
    m_removeAction.setToolTip(tr("Remove selected page entries"));
    m_removeButton.setFixedSize(32, 32);
    connect(&m_removeAction, SIGNAL(triggered(bool)), this, SLOT(itemDeleteAction()));
    m_removeAction.setEnabled(false);

    m_tabs.setCurrentIndex(0);
}
