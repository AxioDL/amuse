#include <athena/FileWriter.hpp>
#include <athena/FileReader.hpp>
#include "ProjectModel.hpp"
#include "Common.hpp"
#include "athena/YAMLDocWriter.hpp"
#include "MainWindow.hpp"
#include <QUndoCommand>

QIcon ProjectModel::GroupNode::Icon;
QIcon ProjectModel::SongGroupNode::Icon;
QIcon ProjectModel::SoundGroupNode::Icon;

NullItemProxyModel::NullItemProxyModel(ProjectModel* source)
: QIdentityProxyModel(source)
{
    setSourceModel(source);
}

QModelIndex NullItemProxyModel::mapFromSource(const QModelIndex& sourceIndex) const
{
    if (!sourceIndex.isValid())
        return QModelIndex();
    if (sourceIndex.row() == sourceModel()->rowCount(sourceIndex.parent()))
        return createIndex(0, sourceIndex.column(), sourceIndex.internalPointer());
    return createIndex(sourceIndex.row() + 1, sourceIndex.column(), sourceIndex.internalPointer());
}

QModelIndex NullItemProxyModel::mapToSource(const QModelIndex& proxyIndex) const
{
    if (!proxyIndex.isValid())
        return QModelIndex();
    return static_cast<ProjectModel*>(sourceModel())->
        proxyCreateIndex(proxyIndex.row() - 1, proxyIndex.column(), proxyIndex.internalPointer());
}

int NullItemProxyModel::rowCount(const QModelIndex& parent) const
{
    return QIdentityProxyModel::rowCount(parent) + 1;
}

QModelIndex NullItemProxyModel::index(int row, int column, const QModelIndex& parent) const
{
    const QModelIndex sourceParent = mapToSource(parent);
    const QModelIndex sourceIndex = sourceModel()->index(row - 1, column, sourceParent);
    return mapFromSource(sourceIndex);
}

QVariant NullItemProxyModel::data(const QModelIndex& proxyIndex, int role) const
{
    if (!proxyIndex.isValid() || proxyIndex.row() == 0)
        return QVariant();
    return QIdentityProxyModel::data(proxyIndex, role);
}

PageObjectProxyModel::PageObjectProxyModel(ProjectModel* source)
: QIdentityProxyModel(source)
{
    setSourceModel(source);
}

QModelIndex PageObjectProxyModel::mapFromSource(const QModelIndex& sourceIndex) const
{
    if (!sourceIndex.isValid())
        return QModelIndex();
    ProjectModel::INode* node = static_cast<ProjectModel::INode*>(sourceIndex.internalPointer());
    auto tp = node->type();
    if ((tp != ProjectModel::INode::Type::SoundMacro &&
         tp != ProjectModel::INode::Type::Keymap &&
         tp != ProjectModel::INode::Type::Layer &&
         tp != ProjectModel::INode::Type::Null) ||
        (tp == ProjectModel::INode::Type::Null &&
         node->parent() == static_cast<ProjectModel*>(sourceModel())->rootNode()))
        return createIndex(sourceIndex.row(), sourceIndex.column(), node);
    ProjectModel::GroupNode* group = static_cast<ProjectModel*>(sourceModel())->getGroupNode(node);
    ProjectModel::CollectionNode* smCol = group->getCollectionOfType(ProjectModel::INode::Type::SoundMacro);
    ProjectModel::CollectionNode* kmCol = group->getCollectionOfType(ProjectModel::INode::Type::Keymap);
    ProjectModel::CollectionNode* layCol = group->getCollectionOfType(ProjectModel::INode::Type::Layer);
    switch (tp)
    {
    case ProjectModel::INode::Type::Null:
        if (node->parent() == group)
            return createIndex(0, sourceIndex.column(), sourceIndex.internalPointer());
        else if (node->parent() == smCol)
            return createIndex(1, sourceIndex.column(), sourceIndex.internalPointer());
        else if (node->parent() == kmCol)
            return createIndex(2 + smCol->childCount(), sourceIndex.column(), sourceIndex.internalPointer());
        else if (node->parent() == layCol)
            return createIndex(3 + smCol->childCount() + kmCol->childCount(), sourceIndex.column(), sourceIndex.internalPointer());
        break;
    case ProjectModel::INode::Type::SoundMacro:
        return createIndex(2 + node->row(), sourceIndex.column(), sourceIndex.internalPointer());
    case ProjectModel::INode::Type::Keymap:
        return createIndex(3 + smCol->childCount() + node->row(), sourceIndex.column(), sourceIndex.internalPointer());
    case ProjectModel::INode::Type::Layer:
        return createIndex(4 + smCol->childCount() + kmCol->childCount() + node->row(), sourceIndex.column(), sourceIndex.internalPointer());
    default:
        break;
    }
    return QModelIndex();
}

QModelIndex PageObjectProxyModel::mapToSource(const QModelIndex& proxyIndex) const
{
    if (!proxyIndex.isValid())
        return QModelIndex();
    ProjectModel::INode* node = static_cast<ProjectModel::INode*>(proxyIndex.internalPointer());
    auto tp = node->type();
    if ((tp != ProjectModel::INode::Type::SoundMacro &&
         tp != ProjectModel::INode::Type::Keymap &&
         tp != ProjectModel::INode::Type::Layer &&
         tp != ProjectModel::INode::Type::Null) ||
        (tp == ProjectModel::INode::Type::Null &&
         node->parent() == static_cast<ProjectModel*>(sourceModel())->rootNode()))
        return static_cast<ProjectModel*>(sourceModel())->
            proxyCreateIndex(proxyIndex.row(), proxyIndex.column(), proxyIndex.internalPointer());
    ProjectModel::GroupNode* group = static_cast<ProjectModel*>(sourceModel())->getGroupNode(node);
    ProjectModel::CollectionNode* smCol = group->getCollectionOfType(ProjectModel::INode::Type::SoundMacro);
    ProjectModel::CollectionNode* kmCol = group->getCollectionOfType(ProjectModel::INode::Type::Keymap);
    ProjectModel::CollectionNode* layCol = group->getCollectionOfType(ProjectModel::INode::Type::Layer);
    switch (tp)
    {
    case ProjectModel::INode::Type::Null:
        if (node->parent() == group)
            return static_cast<ProjectModel*>(sourceModel())->
                proxyCreateIndex(group->childCount(), proxyIndex.column(), proxyIndex.internalPointer());
        else if (node->parent() == smCol)
            return static_cast<ProjectModel*>(sourceModel())->
                proxyCreateIndex(smCol->childCount(), proxyIndex.column(), proxyIndex.internalPointer());
        else if (node->parent() == kmCol)
            return static_cast<ProjectModel*>(sourceModel())->
                proxyCreateIndex(kmCol->childCount(), proxyIndex.column(), proxyIndex.internalPointer());
        else if (node->parent() == layCol)
            return static_cast<ProjectModel*>(sourceModel())->
                proxyCreateIndex(layCol->childCount(), proxyIndex.column(), proxyIndex.internalPointer());
        break;
    case ProjectModel::INode::Type::SoundMacro:
        return static_cast<ProjectModel*>(sourceModel())->
            proxyCreateIndex(node->row() - 2, proxyIndex.column(), proxyIndex.internalPointer());
    case ProjectModel::INode::Type::Keymap:
        return static_cast<ProjectModel*>(sourceModel())->
            proxyCreateIndex(node->row() - smCol->childCount() - 3, proxyIndex.column(), proxyIndex.internalPointer());
    case ProjectModel::INode::Type::Layer:
        return static_cast<ProjectModel*>(sourceModel())->
            proxyCreateIndex(node->row() - kmCol->childCount() - smCol->childCount() - 4, proxyIndex.column(), proxyIndex.internalPointer());
    default:
        break;
    }
    return QModelIndex();
}

QModelIndex PageObjectProxyModel::parent(const QModelIndex& child) const
{
    if (!child.isValid())
        return QModelIndex();
    ProjectModel::INode* node = static_cast<ProjectModel::INode*>(child.internalPointer());
    auto tp = node->type();
    if ((tp != ProjectModel::INode::Type::SoundMacro &&
         tp != ProjectModel::INode::Type::Keymap &&
         tp != ProjectModel::INode::Type::Layer &&
         tp != ProjectModel::INode::Type::Null) ||
        (tp == ProjectModel::INode::Type::Null &&
         node->parent() == static_cast<ProjectModel*>(sourceModel())->rootNode()))
        return QIdentityProxyModel::parent(child);
    ProjectModel::INode* group = node->parent();
    if (group->type() == ProjectModel::INode::Type::Collection)
        group = group->parent();
    return createIndex(group->row(), 0, group);
}

int PageObjectProxyModel::rowCount(const QModelIndex& parent) const
{
    ProjectModel::INode* node = static_cast<ProjectModel*>(sourceModel())->node(parent);
    auto tp = node->type();
    if (tp != ProjectModel::INode::Type::Group)
        return static_cast<ProjectModel*>(sourceModel())->rowCount(parent);
    ProjectModel::GroupNode* group = static_cast<ProjectModel::GroupNode*>(node);
    ProjectModel::CollectionNode* smCol = group->getCollectionOfType(ProjectModel::INode::Type::SoundMacro);
    ProjectModel::CollectionNode* kmCol = group->getCollectionOfType(ProjectModel::INode::Type::Keymap);
    ProjectModel::CollectionNode* layCol = group->getCollectionOfType(ProjectModel::INode::Type::Layer);
    return 4 + smCol->childCount() + kmCol->childCount() + layCol->childCount();
}

QModelIndex PageObjectProxyModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!parent.isValid())
        return QIdentityProxyModel::index(row, column, parent);
    ProjectModel::INode* parentNode = static_cast<ProjectModel::INode*>(parent.internalPointer());
    auto ptp = parentNode->type();
    if (ptp != ProjectModel::INode::Type::Group)
        return QIdentityProxyModel::index(row, column, parent);
    ProjectModel::GroupNode* group = static_cast<ProjectModel::GroupNode*>(parentNode);
    ProjectModel::CollectionNode* smCol = group->getCollectionOfType(ProjectModel::INode::Type::SoundMacro);
    ProjectModel::CollectionNode* kmCol = group->getCollectionOfType(ProjectModel::INode::Type::Keymap);
    ProjectModel::CollectionNode* layCol = group->getCollectionOfType(ProjectModel::INode::Type::Layer);
    if (row == 0)
        return createIndex(row, column, group->nullChild());
    else if (row == 1)
        return createIndex(row, column, smCol->nullChild());
    else if (row < 2 + smCol->childCount())
        return createIndex(row, column, smCol->child(row - 2));
    else if (row == 2 + smCol->childCount())
        return createIndex(row, column, kmCol->nullChild());
    else if (row < 3 + smCol->childCount() + kmCol->childCount())
        return createIndex(row, column, kmCol->child(row - smCol->childCount() - 3));
    else if (row == 3 + smCol->childCount() + kmCol->childCount())
        return createIndex(row, column, layCol->nullChild());
    else if (row < 4 + smCol->childCount() + kmCol->childCount() + layCol->childCount())
        return createIndex(row, column, layCol->child(row - kmCol->childCount() - smCol->childCount() - 4));
    return QModelIndex();
}

QVariant PageObjectProxyModel::data(const QModelIndex& proxyIndex, int role) const
{
    if (role != Qt::DisplayRole || !proxyIndex.isValid() || proxyIndex.row() == 0)
        return QVariant();
    ProjectModel::INode* node = static_cast<ProjectModel::INode*>(proxyIndex.internalPointer());
    auto tp = node->type();
    if ((tp != ProjectModel::INode::Type::SoundMacro &&
         tp != ProjectModel::INode::Type::Keymap &&
         tp != ProjectModel::INode::Type::Layer &&
         tp != ProjectModel::INode::Type::Null) ||
        (tp == ProjectModel::INode::Type::Null &&
         node->parent() == static_cast<ProjectModel*>(sourceModel())->rootNode()))
        return QVariant();
    ProjectModel::GroupNode* group = static_cast<ProjectModel*>(sourceModel())->getGroupNode(node);
    ProjectModel::CollectionNode* smCol = group->getCollectionOfType(ProjectModel::INode::Type::SoundMacro);
    ProjectModel::CollectionNode* kmCol = group->getCollectionOfType(ProjectModel::INode::Type::Keymap);
    ProjectModel::CollectionNode* layCol = group->getCollectionOfType(ProjectModel::INode::Type::Layer);
    switch (tp)
    {
    case ProjectModel::INode::Type::Null:
        if (node->parent() == group)
            return QVariant();
        else if (node->parent() == smCol)
            return tr("SoundMacros:");
        else if (node->parent() == kmCol)
            return tr("Keymaps:");
        else if (node->parent() == layCol)
            return tr("Layers:");
        break;
    case ProjectModel::INode::Type::SoundMacro:
    case ProjectModel::INode::Type::Keymap:
    case ProjectModel::INode::Type::Layer:
        return node->text();
    default:
        break;
    }
    return QVariant();
}

Qt::ItemFlags PageObjectProxyModel::flags(const QModelIndex& proxyIndex) const
{
    if (!proxyIndex.isValid())
        return Qt::NoItemFlags;
    if (proxyIndex.row() == 0)
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    ProjectModel::INode* node = static_cast<ProjectModel::INode*>(proxyIndex.internalPointer());
    auto tp = node->type();
    if (tp == ProjectModel::INode::Type::Null)
        return Qt::NoItemFlags;
    if (tp != ProjectModel::INode::Type::SoundMacro &&
        tp != ProjectModel::INode::Type::Keymap &&
        tp != ProjectModel::INode::Type::Layer)
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

ProjectModel::INode::INode(INode* parent, int row) : m_parent(parent), m_row(row)
{
    auto nullNode = amuse::MakeObj<NullNode>(this);
    m_nullChild = nullNode.get();
}

ProjectModel::CollectionNode* ProjectModel::GroupNode::getCollectionOfType(Type tp) const
{
    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
    {
        if ((*it)->type() == Type::Collection)
        {
            CollectionNode* col = static_cast<CollectionNode*>(it->get());
            if (col->collectionType() == tp)
                return col;
        }
    }
    return nullptr;
}

ProjectModel::BasePoolObjectNode* ProjectModel::GroupNode::pageObjectNodeOfId(amuse::ObjectId id) const
{
    if (ProjectModel::BasePoolObjectNode* ret = getCollectionOfType(Type::SoundMacro)->nodeOfId(id))
        return ret;
    if (ProjectModel::BasePoolObjectNode* ret = getCollectionOfType(Type::Keymap)->nodeOfId(id))
        return ret;
    if (ProjectModel::BasePoolObjectNode* ret = getCollectionOfType(Type::Layer)->nodeOfId(id))
        return ret;
    return nullptr;
}

int ProjectModel::CollectionNode::indexOfId(amuse::ObjectId id) const
{
    int ret = 0;
    for (auto& n : m_children)
    {
        if (static_cast<BasePoolObjectNode*>(n.get())->id() == id)
            return ret;
        ++ret;
    }
    return -1;
}

amuse::ObjectId ProjectModel::CollectionNode::idOfIndex(int idx) const
{
    return static_cast<BasePoolObjectNode*>(m_children[idx].get())->id();
}

ProjectModel::BasePoolObjectNode* ProjectModel::CollectionNode::nodeOfIndex(int idx) const
{
    return static_cast<BasePoolObjectNode*>(m_children[idx].get());
}

ProjectModel::BasePoolObjectNode* ProjectModel::CollectionNode::nodeOfId(amuse::ObjectId id) const
{
    int idx = indexOfId(id);
    if (idx < 0)
        return nullptr;
    return nodeOfIndex(idx);
}

ProjectModel::ProjectModel(const QString& path, QObject* parent)
: QAbstractItemModel(parent), m_dir(path), m_nullProxy(this), m_pageObjectProxy(this)
{
    m_root = amuse::MakeObj<RootNode>();

    GroupNode::Icon = QIcon(":/icons/IconGroup.svg");
    SongGroupNode::Icon = QIcon(":/icons/IconSongGroup.svg");
    SoundGroupNode::Icon = QIcon(":/icons/IconSoundGroup.svg");
}

void ProjectModel::_buildSortedList()
{
    m_sorted.clear();
    m_sorted.reserve(m_groups.size());
    for (auto it = m_groups.begin() ; it != m_groups.end() ; ++it)
        m_sorted.emplace_back(it);
    std::sort(m_sorted.begin(), m_sorted.end());
}

QModelIndex ProjectModel::_indexOfGroup(const QString& groupName) const
{
    auto search = std::lower_bound(m_sorted.cbegin(), m_sorted.cend(), groupName);
    if (search == m_sorted.cend() || search->m_it->first != groupName)
        return QModelIndex();
    else
    {
        int idx = search - m_sorted.begin();
        return createIndex(idx, 0, m_root->child(idx));
    }
}

int ProjectModel::_hypotheticalIndexOfGroup(const QString& groupName) const
{
    auto search = std::lower_bound(m_sorted.cbegin(), m_sorted.cend(), groupName);
    return search - m_sorted.begin();
}

bool ProjectModel::clearProjectData()
{
    m_projectDatabase = amuse::ProjectDatabase();
    m_groups.clear();
    m_sorted.clear();
    m_midiFiles.clear();

    m_needsReset = true;
    return true;
}

bool ProjectModel::openGroupData(const QString& groupName, UIMessenger& messenger)
{
    m_projectDatabase.setIdDatabases();
    QString path = QFileInfo(m_dir, groupName).filePath();
    m_groups.insert(std::make_pair(groupName, QStringToSysString(path)));

    m_needsReset = true;
    return true;
}

bool ProjectModel::openSongsData()
{
    m_midiFiles.clear();
    QFileInfo songsFile(m_dir, QStringLiteral("!songs.yaml"));
    if (songsFile.exists())
    {
        athena::io::FileReader r(QStringToSysString(songsFile.path()));
        if (!r.hasError())
        {
            athena::io::YAMLDocReader dr;
            if (dr.parse(&r))
            {
                m_midiFiles.reserve(dr.getRootNode()->m_mapChildren.size());
                for (auto& p : dr.getRootNode()->m_mapChildren)
                {
                    char* endPtr;
                    amuse::SongId id = uint16_t(strtoul(p.first.c_str(), &endPtr, 0));
                    if (endPtr == p.first.c_str() || id.id == 0xffff)
                        continue;
                    m_midiFiles[id] = QString::fromStdString(p.second->m_scalarString);
                }
            }
        }
    }
    return true;
}

bool ProjectModel::reloadSampleData(const QString& groupName, UIMessenger& messenger)
{
    m_projectDatabase.setIdDatabases();
    QString path = QFileInfo(m_dir, groupName).filePath();
    auto search = m_groups.find(groupName);
    if (search != m_groups.end())
    {
        search->second.setIdDatabases();
        search->second.getSdir().reloadSampleData(QStringToSysString(path));
    }

    m_needsReset = true;
    return true;
}

bool ProjectModel::importGroupData(const QString& groupName, const amuse::AudioGroupData& data,
                                   ImportMode mode, UIMessenger& messenger)
{
    m_projectDatabase.setIdDatabases();

    amuse::AudioGroupDatabase& grp = m_groups.insert(std::make_pair(groupName, data)).first->second;

    if (!MkPath(m_dir.path(), messenger))
        return false;
    QDir dir(QFileInfo(m_dir, groupName).filePath());
    if (!MkPath(dir.path(), messenger))
        return false;

    amuse::SystemString sysDir = QStringToSysString(dir.path());
    grp.setGroupPath(sysDir);
    switch (mode)
    {
    case ImportMode::Original:
        grp.getSdir().extractAllCompressed(sysDir, data.getSamp());
        break;
    case ImportMode::WAVs:
        grp.getSdir().extractAllWAV(sysDir, data.getSamp());
        break;
    case ImportMode::Both:
        grp.getSdir().extractAllWAV(sysDir, data.getSamp());
        grp.getSdir().extractAllCompressed(sysDir, data.getSamp());
        break;
    default:
        break;
    }

    grp.getProj().toYAML(sysDir);
    grp.getPool().toYAML(sysDir);

    m_needsReset = true;
    return true;
}

bool ProjectModel::saveToFile(UIMessenger& messenger)
{
    m_projectDatabase.setIdDatabases();

    if (!MkPath(m_dir.path(), messenger))
        return false;

    for (auto& g : m_groups)
    {
        QDir dir(QFileInfo(m_dir, g.first).filePath());
        if (!MkPath(dir.path(), messenger))
            return false;

        g.second.setIdDatabases();
        amuse::SystemString groupPath = QStringToSysString(dir.path());
        g.second.getProj().toYAML(groupPath);
        g.second.getPool().toYAML(groupPath);
    }

    if (!m_midiFiles.empty())
    {
        QFileInfo songsFile(m_dir, QStringLiteral("!songs.yaml"));
        athena::io::YAMLDocWriter dw("amuse::Songs");
        for (auto& p : m_midiFiles)
        {
            char id[16];
            snprintf(id, 16, "%04X", p.first.id);
            dw.writeString(id, p.second.toUtf8().data());
        }
        athena::io::FileWriter w(QStringToSysString(songsFile.path()));
        if (!w.hasError())
            dw.finish(&w);
    }

    return true;
}

void ProjectModel::_buildGroupNode(GroupNode& gn)
{
    amuse::AudioGroup& group = gn.m_it->second;
    auto& songGroups = group.getProj().songGroups();
    auto& sfxGroups = group.getProj().sfxGroups();
    auto& soundMacros = group.getPool().soundMacros();
    auto& tables = group.getPool().tables();
    auto& keymaps = group.getPool().keymaps();
    auto& layers = group.getPool().layers();
    auto& samples = group.getSdir().sampleEntries();
    gn.reserve(songGroups.size() + sfxGroups.size() + 4);
    for (const auto& grp : SortUnorderedMap(songGroups))
        gn.makeChild<SongGroupNode>(grp.first, grp.second.get());
    for (const auto& grp : SortUnorderedMap(sfxGroups))
        gn.makeChild<SoundGroupNode>(grp.first, grp.second.get());
    {
        CollectionNode& col =
            gn.makeChild<CollectionNode>(tr("Sound Macros"), QIcon(":/icons/IconSoundMacro.svg"), INode::Type::SoundMacro);
        col.reserve(soundMacros.size());
        for (const auto& macro : SortUnorderedMap(soundMacros))
            col.makeChild<SoundMacroNode>(macro.first, macro.second.get());
    }
    {
        auto tablesSort = SortUnorderedMap(tables);
        size_t ADSRCount = 0;
        size_t curveCount = 0;
        for (auto& t : tablesSort)
        {
            amuse::ITable::Type tp = (*t.second.get())->Isa();
            if (tp == amuse::ITable::Type::ADSR || tp == amuse::ITable::Type::ADSRDLS)
                ADSRCount += 1;
            else if (tp == amuse::ITable::Type::Curve)
                curveCount += 1;
        }
        {
            CollectionNode& col =
                gn.makeChild<CollectionNode>(tr("ADSRs"), QIcon(":/icons/IconADSR.svg"), INode::Type::ADSR);
            col.reserve(ADSRCount);
            for (auto& t : tablesSort)
            {
                amuse::ITable::Type tp = (*t.second.get())->Isa();
                if (tp == amuse::ITable::Type::ADSR || tp == amuse::ITable::Type::ADSRDLS)
                    col.makeChild<ADSRNode>(t.first, t.second.get());
            }
        }
        {
            CollectionNode& col =
                gn.makeChild<CollectionNode>(tr("Curves"), QIcon(":/icons/IconCurve.svg"), INode::Type::Curve);
            col.reserve(curveCount);
            for (auto& t : tablesSort)
            {
                amuse::ITable::Type tp = (*t.second.get())->Isa();
                if (tp == amuse::ITable::Type::Curve)
                    col.makeChild<CurveNode>(t.first, t.second.get());
            }
        }
    }
    {
        CollectionNode& col =
            gn.makeChild<CollectionNode>(tr("Keymaps"), QIcon(":/icons/IconKeymap.svg"), INode::Type::Keymap);
        col.reserve(keymaps.size());
        for (auto& keymap : SortUnorderedMap(keymaps))
            col.makeChild<KeymapNode>(keymap.first, keymap.second.get());
    }
    {
        CollectionNode& col =
            gn.makeChild<CollectionNode>(tr("Layers"), QIcon(":/icons/IconLayers.svg"), INode::Type::Layer);
        col.reserve(layers.size());
        for (auto& keymap : SortUnorderedMap(layers))
            col.makeChild<LayersNode>(keymap.first, keymap.second.get());
    }
    {
        CollectionNode& col =
            gn.makeChild<CollectionNode>(tr("Samples"), QIcon(":/icons/IconSample.svg"), INode::Type::Sample);
        col.reserve(samples.size());
        for (auto& sample : SortUnorderedMap(samples))
            col.makeChild<SampleNode>(sample.first, sample.second.get());
    }
}

void ProjectModel::_resetModelData()
{
    beginResetModel();
    _buildSortedList();
    m_projectDatabase.setIdDatabases();
    m_root = amuse::MakeObj<RootNode>();
    m_root->reserve(m_sorted.size());
    for (auto it = m_sorted.begin() ; it != m_sorted.end() ; ++it)
    {
        it->m_it->second.setIdDatabases();
        GroupNode& gn = m_root->makeChild<GroupNode>(it->m_it);
        _buildGroupNode(gn);
    }
    endResetModel();
}

bool ProjectModel::ensureModelData()
{
    if (m_needsReset)
    {
        _resetModelData();
        m_needsReset = false;
    }
    return !m_groups.empty();
}

QModelIndex ProjectModel::proxyCreateIndex(int arow, int acolumn, void *adata) const
{
    if (arow < 0)
    {
        INode* childItem = static_cast<INode*>(adata);
        return createIndex(childItem->parent()->childCount(), acolumn, adata);
    }
    return createIndex(arow, acolumn, adata);
}

QModelIndex ProjectModel::index(int row, int column, const QModelIndex& parent) const
{
    if (row < 0)
    {
        INode* parentItem;
        if (!parent.isValid())
            parentItem = m_root.get();
        else
            parentItem = static_cast<INode*>(parent.internalPointer());

        INode* childItem = parentItem->nullChild();
        return createIndex(childItem->row(), column, childItem);
    }

    if (!hasIndex(row, column, parent))
        return QModelIndex();

    INode* parentItem;
    if (!parent.isValid())
        parentItem = m_root.get();
    else
        parentItem = static_cast<INode*>(parent.internalPointer());

    INode* childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);
    else
        return QModelIndex();
}

QModelIndex ProjectModel::index(INode* node) const
{
    if (node == m_root.get())
        return QModelIndex();
    return createIndex(node->row(), 0, node);
}

QModelIndex ProjectModel::parent(const QModelIndex& index) const
{
    if (!index.isValid())
        return QModelIndex();

    INode* childItem = static_cast<INode*>(index.internalPointer());
    INode* parentItem = childItem->parent();

    if (parentItem == m_root.get())
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

int ProjectModel::rowCount(const QModelIndex& parent) const
{
    INode* parentItem;

    if (!parent.isValid())
        parentItem = m_root.get();
    else
        parentItem = static_cast<INode*>(parent.internalPointer());

    return parentItem->childCount();
}

int ProjectModel::columnCount(const QModelIndex& parent) const
{
    return 1;
}

QVariant ProjectModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    INode* item = static_cast<INode*>(index.internalPointer());

    switch (role)
    {
    case Qt::DisplayRole:
        return item->text();
    case Qt::DecorationRole:
        return item->icon();
    default:
        return {};
    }
}

Qt::ItemFlags ProjectModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    return static_cast<INode*>(index.internalPointer())->flags();
}

ProjectModel::INode* ProjectModel::node(const QModelIndex& index) const
{
    if (!index.isValid())
        return m_root.get();
    return static_cast<INode*>(index.internalPointer());
}

ProjectModel::GroupNode* ProjectModel::getGroupNode(INode* node) const
{
    if (!node)
        return nullptr;
    if (node->type() == INode::Type::Group)
        return static_cast<GroupNode*>(node);
    return getGroupNode(node->parent());
}

bool ProjectModel::canEdit(const QModelIndex& index) const
{
    if (!index.isValid())
        return false;
    return (static_cast<INode*>(index.internalPointer())->flags() & Qt::ItemIsSelectable) != Qt::NoItemFlags;
}

class DeleteNodeUndoCommand : public QUndoCommand
{
    QModelIndex m_deleteIdx;
    amuse::ObjToken<ProjectModel::INode> m_node;
    ProjectModel::NameUndoRegistry m_nameReg;
public:
    DeleteNodeUndoCommand(const QModelIndex& index)
     : QUndoCommand(ProjectModel::tr("Delete %1").arg(index.data().toString())), m_deleteIdx(index) {}
    void undo()
    {
        g_MainWindow->projectModel()->_undoDel(m_deleteIdx, std::move(m_node), m_nameReg);
        m_node.reset();
        m_nameReg.clear();
    }
    void redo()
    {
        m_node = g_MainWindow->projectModel()->_redoDel(m_deleteIdx, m_nameReg);
    }
};

void ProjectModel::_undoDel(const QModelIndex& index, amuse::ObjToken<ProjectModel::INode> n, const NameUndoRegistry& nameReg)
{
    beginInsertRows(index.parent(), index.row(), index.row());
    node(index.parent())->insertChild(index.row(), n);
    setIdDatabases(n.get());
    n->depthTraverse([&nameReg](INode* node)
    {
        node->registerNames(nameReg);
        return true;
    });
    endInsertRows();
}

amuse::ObjToken<ProjectModel::INode> ProjectModel::_redoDel(const QModelIndex& index, NameUndoRegistry& nameReg)
{
    node(index)->depthTraverse([&nameReg](INode* node)
    {
        g_MainWindow->aboutToDeleteNode(node);
        node->unregisterNames(nameReg);
        return true;
    });
    beginRemoveRows(index.parent(), index.row(), index.row());
    amuse::ObjToken<ProjectModel::INode> ret = node(index.parent())->removeChild(index.row());
    endRemoveRows();
    return ret;
}

void ProjectModel::del(const QModelIndex& index)
{
    if (!index.isValid())
        return;
    g_MainWindow->pushUndoCommand(new DeleteNodeUndoCommand(index));
}

ProjectModel::GroupNode* ProjectModel::newSubproject(const QString& name, UIMessenger& messenger)
{
    if (m_groups.find(name) != m_groups.cend())
    {
        messenger.critical(tr("Subproject Conflict"), tr("The subproject %1 is already defined").arg(name));
        return nullptr;
    }
    QDir dir(QFileInfo(m_dir, name).filePath());
    if (!MkPath(dir.path(), messenger))
        return nullptr;
    QFile(QFileInfo(dir, QStringLiteral("!project.yaml")).filePath()).open(QFile::WriteOnly);
    QFile(QFileInfo(dir, QStringLiteral("!pool.yaml")).filePath()).open(QFile::WriteOnly);
    int idx = _hypotheticalIndexOfGroup(name);
    beginInsertRows(QModelIndex(), idx, idx);
    auto it = m_groups.emplace(std::make_pair(name, amuse::AudioGroupDatabase(QStringToSysString(dir.path())))).first;
    _buildSortedList();
    m_projectDatabase.setIdDatabases();
    it->second.setIdDatabases();
    GroupNode& gn = m_root->makeChildAtIdx<GroupNode>(idx, it);
    _buildGroupNode(gn);
    endInsertRows();
    return &gn;
}

ProjectModel::SoundGroupNode* ProjectModel::newSoundGroup(GroupNode* group, const QString& name, UIMessenger& messenger)
{
    setIdDatabases(group);
    auto nameKey = name.toUtf8();
    if (amuse::GroupId::CurNameDB->m_stringToId.find(nameKey.data()) != amuse::GroupId::CurNameDB->m_stringToId.cend())
    {
        messenger.critical(tr("Sound Group Conflict"), tr("The group %1 is already defined").arg(name));
        return nullptr;
    }
    beginInsertRows(index(group), 0, 0);
    amuse::GroupId newId = amuse::GroupId::CurNameDB->generateId(amuse::NameDB::Type::Group);
    amuse::GroupId::CurNameDB->registerPair(nameKey.data(), newId);
    auto node = amuse::MakeObj<amuse::SFXGroupIndex>();
    group->getAudioGroup()->getProj().sfxGroups()[newId] = node;
    SoundGroupNode& ret = group->makeChildAtIdx<SoundGroupNode>(0, newId, node);
    endInsertRows();
    return &ret;
}

ProjectModel::SongGroupNode* ProjectModel::newSongGroup(GroupNode* group, const QString& name, UIMessenger& messenger)
{
    setIdDatabases(group);
    auto nameKey = name.toUtf8();
    if (amuse::GroupId::CurNameDB->m_stringToId.find(nameKey.data()) != amuse::GroupId::CurNameDB->m_stringToId.cend())
    {
        messenger.critical(tr("Song Group Conflict"), tr("The group %1 is already defined").arg(name));
        return nullptr;
    }
    beginInsertRows(index(group), 0, 0);
    amuse::GroupId newId = amuse::GroupId::CurNameDB->generateId(amuse::NameDB::Type::Group);
    amuse::GroupId::CurNameDB->registerPair(nameKey.data(), newId);
    auto node = amuse::MakeObj<amuse::SongGroupIndex>();
    group->getAudioGroup()->getProj().songGroups()[newId] = node;
    SongGroupNode& ret = group->makeChildAtIdx<SongGroupNode>(0, newId, node);
    endInsertRows();
    return &ret;
}

ProjectModel::SoundMacroNode* ProjectModel::newSoundMacro(GroupNode* group, const QString& name, UIMessenger& messenger,
                                                          const SoundMacroTemplateEntry* templ)
{
    setIdDatabases(group);
    auto nameKey = name.toUtf8();
    if (amuse::SoundMacroId::CurNameDB->m_stringToId.find(nameKey.data()) != amuse::SoundMacroId::CurNameDB->m_stringToId.cend())
    {
        messenger.critical(tr("Sound Macro Conflict"), tr("The macro %1 is already defined").arg(name));
        return nullptr;
    }
    ProjectModel::CollectionNode* coll = group->getCollectionOfType(INode::Type::SoundMacro);
    QModelIndex parentIdx = index(coll);
    int insertIdx = rowCount(parentIdx);
    beginInsertRows(parentIdx, insertIdx, insertIdx);
    amuse::SoundMacroId newId = amuse::SoundMacroId::CurNameDB->generateId(amuse::NameDB::Type::SoundMacro);
    amuse::SoundMacroId::CurNameDB->registerPair(nameKey.data(), newId);
    auto node = amuse::MakeObj<amuse::SoundMacro>();
    if (templ)
    {
        athena::io::MemoryReader r(templ->m_data, templ->m_length);
        node->readCmds<athena::utility::NotSystemEndian>(r, templ->m_length);
    }
    group->getAudioGroup()->getPool().soundMacros()[newId] = node;
    SoundMacroNode& ret = coll->makeChildAtIdx<SoundMacroNode>(insertIdx, newId, node);
    endInsertRows();
    return &ret;
}

ProjectModel::ADSRNode* ProjectModel::newADSR(GroupNode* group, const QString& name, UIMessenger& messenger)
{
    setIdDatabases(group);
    auto nameKey = name.toUtf8();
    if (amuse::TableId::CurNameDB->m_stringToId.find(nameKey.data()) != amuse::TableId::CurNameDB->m_stringToId.cend())
    {
        messenger.critical(tr("ADSR Conflict"), tr("The ADSR %1 is already defined").arg(name));
        return nullptr;
    }
    ProjectModel::CollectionNode* coll = group->getCollectionOfType(INode::Type::ADSR);
    QModelIndex parentIdx = index(coll);
    int insertIdx = rowCount(parentIdx);
    beginInsertRows(parentIdx, insertIdx, insertIdx);
    amuse::TableId newId = amuse::TableId::CurNameDB->generateId(amuse::NameDB::Type::Table);
    amuse::TableId::CurNameDB->registerPair(nameKey.data(), newId);
    auto node = amuse::MakeObj<std::unique_ptr<amuse::ITable>>();
    *node = std::make_unique<amuse::ADSR>();
    group->getAudioGroup()->getPool().tables()[newId] = node;
    ADSRNode& ret = coll->makeChildAtIdx<ADSRNode>(insertIdx, newId, node);
    endInsertRows();
    return &ret;
}

ProjectModel::CurveNode* ProjectModel::newCurve(GroupNode* group, const QString& name, UIMessenger& messenger)
{
    setIdDatabases(group);
    auto nameKey = name.toUtf8();
    if (amuse::TableId::CurNameDB->m_stringToId.find(nameKey.data()) != amuse::TableId::CurNameDB->m_stringToId.cend())
    {
        messenger.critical(tr("Curve Conflict"), tr("The Curve %1 is already defined").arg(name));
        return nullptr;
    }
    ProjectModel::CollectionNode* coll = group->getCollectionOfType(INode::Type::Curve);
    QModelIndex parentIdx = index(coll);
    int insertIdx = rowCount(parentIdx);
    beginInsertRows(parentIdx, insertIdx, insertIdx);
    amuse::TableId newId = amuse::TableId::CurNameDB->generateId(amuse::NameDB::Type::Table);
    amuse::TableId::CurNameDB->registerPair(nameKey.data(), newId);
    auto node = amuse::MakeObj<std::unique_ptr<amuse::ITable>>();
    *node = std::make_unique<amuse::Curve>();
    group->getAudioGroup()->getPool().tables()[newId] = node;
    CurveNode& ret = coll->makeChildAtIdx<CurveNode>(insertIdx, newId, node);
    endInsertRows();
    return &ret;
}

ProjectModel::KeymapNode* ProjectModel::newKeymap(GroupNode* group, const QString& name, UIMessenger& messenger)
{
    setIdDatabases(group);
    auto nameKey = name.toUtf8();
    if (amuse::KeymapId::CurNameDB->m_stringToId.find(nameKey.data()) != amuse::KeymapId::CurNameDB->m_stringToId.cend())
    {
        messenger.critical(tr("Keymap Conflict"), tr("The Keymap %1 is already defined").arg(name));
        return nullptr;
    }
    ProjectModel::CollectionNode* coll = group->getCollectionOfType(INode::Type::Keymap);
    QModelIndex parentIdx = index(coll);
    int insertIdx = rowCount(parentIdx);
    beginInsertRows(parentIdx, insertIdx, insertIdx);
    amuse::KeymapId newId = amuse::KeymapId::CurNameDB->generateId(amuse::NameDB::Type::Keymap);
    amuse::KeymapId::CurNameDB->registerPair(nameKey.data(), newId);
    auto node = amuse::MakeObj<std::array<amuse::Keymap, 128>>();
    group->getAudioGroup()->getPool().keymaps()[newId] = node;
    KeymapNode& ret = coll->makeChildAtIdx<KeymapNode>(insertIdx, newId, node);
    endInsertRows();
    return &ret;
}

ProjectModel::LayersNode* ProjectModel::newLayers(GroupNode* group, const QString& name, UIMessenger& messenger)
{
    setIdDatabases(group);
    auto nameKey = name.toUtf8();
    if (amuse::LayersId::CurNameDB->m_stringToId.find(nameKey.data()) != amuse::LayersId::CurNameDB->m_stringToId.cend())
    {
        messenger.critical(tr("Layers Conflict"), tr("Layers %1 is already defined").arg(name));
        return nullptr;
    }
    ProjectModel::CollectionNode* coll = group->getCollectionOfType(INode::Type::Layer);
    QModelIndex parentIdx = index(coll);
    int insertIdx = rowCount(parentIdx);
    beginInsertRows(parentIdx, insertIdx, insertIdx);
    amuse::LayersId newId = amuse::LayersId::CurNameDB->generateId(amuse::NameDB::Type::Layer);
    amuse::LayersId::CurNameDB->registerPair(nameKey.data(), newId);
    auto node = amuse::MakeObj<std::vector<amuse::LayerMapping>>();
    group->getAudioGroup()->getPool().layers()[newId] = node;
    LayersNode& ret = coll->makeChildAtIdx<LayersNode>(insertIdx, newId, node);
    endInsertRows();
    return &ret;
}

ProjectModel::GroupNode* ProjectModel::getGroupOfSfx(amuse::SFXId id) const
{
    ProjectModel::GroupNode* ret = nullptr;
    m_root->oneLevelTraverse([id, &ret](INode* n)
    {
        GroupNode* gn = static_cast<GroupNode*>(n);
        amuse::AudioGroupDatabase* db = gn->getAudioGroup();
        for (const auto& p : db->getProj().sfxGroups())
        {
            if (p.second->m_sfxEntries.find(id) != p.second->m_sfxEntries.cend())
            {
                ret = gn;
                return false;
            }
        }
        return true;
    });
    return ret;
}

ProjectModel::GroupNode* ProjectModel::getGroupOfSong(amuse::SongId id) const
{
    ProjectModel::GroupNode* ret = nullptr;
    m_root->oneLevelTraverse([id, &ret](INode* n)
    {
        GroupNode* gn = static_cast<GroupNode*>(n);
        amuse::AudioGroupDatabase* db = gn->getAudioGroup();
        for (const auto& p : db->getProj().songGroups())
        {
            if (p.second->m_midiSetups.find(id) != p.second->m_midiSetups.cend())
            {
                ret = gn;
                return false;
            }
        }
        return true;
    });
    return ret;
}

QString ProjectModel::getMIDIPathOfSong(amuse::SongId id) const
{
    auto search = m_midiFiles.find(id);
    if (search == m_midiFiles.cend())
        return {};
    return search->second;
}

void ProjectModel::setMIDIPathOfSong(amuse::SongId id, const QString& path)
{
    m_midiFiles[id] = path;
}

void ProjectModel::setIdDatabases(INode* context) const
{
    m_projectDatabase.setIdDatabases();
    if (ProjectModel::GroupNode* group = getGroupNode(context))
        group->getAudioGroup()->setIdDatabases();
}
