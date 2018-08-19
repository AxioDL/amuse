#include <athena/FileWriter.hpp>
#include <athena/FileReader.hpp>
#include "ProjectModel.hpp"
#include "Common.hpp"
#include "athena/YAMLDocWriter.hpp"
#include "MainWindow.hpp"
#include "amuse/SongConverter.hpp"
#include "amuse/ContainerRegistry.hpp"
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
    ProjectModel::INode* node;
    if (!parent.isValid())
        node = static_cast<ProjectModel*>(sourceModel())->node(parent);
    else
        node = static_cast<ProjectModel::INode*>(parent.internalPointer());
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

void ProjectModel::NameUndoRegistry::registerSongName(amuse::SongId id) const
{
    auto search = m_songIDs.find(id);
    if (search != m_songIDs.cend())
        g_MainWindow->projectModel()->_allocateSongId(id, search->second);
}
void ProjectModel::NameUndoRegistry::unregisterSongName(amuse::SongId id)
{
    auto search = amuse::SongId::CurNameDB->m_idToString.find(id);
    if (search != amuse::SongId::CurNameDB->m_idToString.cend())
        m_songIDs[id] = search->second;
    g_MainWindow->projectModel()->deallocateSongId(id);
}
void ProjectModel::NameUndoRegistry::registerSFXName(amuse::SongId id) const
{
    auto search = m_sfxIDs.find(id);
    if (search != m_sfxIDs.cend())
        amuse::SFXId::CurNameDB->registerPair(search->second, id);
}
void ProjectModel::NameUndoRegistry::unregisterSFXName(amuse::SongId id)
{
    auto search = amuse::SFXId::CurNameDB->m_idToString.find(id);
    if (search != amuse::SFXId::CurNameDB->m_idToString.cend())
        m_sfxIDs[id] = search->second;
    amuse::SFXId::CurNameDB->remove(id);
}

ProjectModel::INode::INode(const QString& name)
: m_name(name)
{
    auto nullNode = amuse::MakeObj<NullNode>(this);
    m_nullChild = nullNode.get();
}

int ProjectModel::INode::hypotheticalIndex(const QString& name) const
{
    auto search = std::lower_bound(m_children.cbegin(), m_children.cend(), name,
    [](const amuse::IObjToken<INode>& item, const QString& name)
    {
        return item->name() < name;
    });
    return int(search - m_children.cbegin());
}

int ProjectModel::GroupNode::hypotheticalIndex(const QString& name) const
{
    /* Insert prior to pool object collections */
    auto search = std::lower_bound(m_children.cbegin(), m_children.cend() - 6, name,
    [](const amuse::IObjToken<INode>& item, const QString& name)
    {
        return item->name() < name;
    });
    return int(search - m_children.cbegin());
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

bool ProjectModel::clearProjectData()
{
    m_projectDatabase = amuse::ProjectDatabase();
    m_groups.clear();
    m_midiFiles.clear();

    m_needsReset = true;
    return true;
}

bool ProjectModel::openGroupData(const QString& groupName, UIMessenger& messenger)
{
    m_projectDatabase.setIdDatabases();
    QString path = QFileInfo(m_dir, groupName).filePath();
    m_groups.insert(std::make_pair(groupName, std::make_unique<amuse::AudioGroupDatabase>(QStringToSysString(path))));

    m_needsReset = true;
    return true;
}

void ProjectModel::_resetSongRefCount()
{
    for (auto& song : m_midiFiles)
        song.second.m_refCount = 0;
    for (const auto& g : m_groups)
        for (const auto& g2 : g.second->getProj().songGroups())
            for (const auto& m : g2.second->m_midiSetups)
                ++m_midiFiles[m.first].m_refCount;
    for (auto it = m_midiFiles.begin(); it != m_midiFiles.end();)
    {
        if (it->second.m_refCount == 0)
        {
            it = m_midiFiles.erase(it);
            continue;
        }
        ++it;
    }
}

void ProjectModel::openSongsData()
{
    m_midiFiles.clear();
    QFileInfo songsFile(m_dir, QStringLiteral("!songs.yaml"));
    if (songsFile.exists())
    {
        athena::io::FileReader r(QStringToSysString(songsFile.filePath()));
        if (!r.hasError())
        {
            athena::io::YAMLDocReader dr;
            if (dr.parse(&r))
            {
                m_midiFiles.reserve(dr.getRootNode()->m_mapChildren.size());
                for (auto& p : dr.getRootNode()->m_mapChildren)
                {
                    char* endPtr;
                    amuse::SongId id = uint16_t(strtoul(p.first.c_str(), &endPtr, 16));
                    if (endPtr == p.first.c_str() || id.id == 0xffff)
                        continue;
                    QString path = QString::fromStdString(p.second->m_scalarString);
                    setMIDIPathOfSong(id, path);
                }
                _resetSongRefCount();
            }
        }
    }
}

void ProjectModel::importSongsData(const QString& path)
{
    std::vector<std::pair<amuse::SystemString, amuse::ContainerRegistry::SongData>> songs =
        amuse::ContainerRegistry::LoadSongs(QStringToSysString(path).c_str());

    for (const auto& song : songs)
    {
        int version;
        bool isBig;
        auto midiData =
            amuse::SongConverter::SongToMIDI(song.second.m_data.get(), version, isBig);
        if (!midiData.empty())
        {
            QFileInfo fi(m_dir, SysStringToQString(song.first + _S(".mid")));
            QFile f(fi.filePath());
            if (f.open(QFile::WriteOnly))
            {
                f.write((const char*)midiData.data(), midiData.size());
                setMIDIPathOfSong(
                    song.second.m_setupId, m_dir.relativeFilePath(fi.filePath()));
            }
        }
    }

    saveSongsIndex();
    _resetSongRefCount();
}

bool ProjectModel::reloadSampleData(const QString& groupName, UIMessenger&)
{
    m_projectDatabase.setIdDatabases();
    QString path = QFileInfo(m_dir, groupName).filePath();
    auto search = m_groups.find(groupName);
    if (search != m_groups.end())
    {
        search->second->setIdDatabases();
        search->second->getSdir().reloadSampleData(QStringToSysString(path));
    }

    m_needsReset = true;
    return true;
}

bool ProjectModel::importGroupData(const QString& groupName, const amuse::AudioGroupData& data,
                                   ImportMode mode, UIMessenger& messenger)
{
    m_projectDatabase.setIdDatabases();

    amuse::AudioGroupDatabase& grp = *m_groups.insert(std::make_pair(groupName,
                                      std::make_unique<amuse::AudioGroupDatabase>(data))).first->second;

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

void ProjectModel::saveSongsIndex()
{
    if (!m_midiFiles.empty())
    {
        QFileInfo songsFile(m_dir, QStringLiteral("!songs.yaml"));
        athena::io::YAMLDocWriter dw("amuse::Songs");
        for (auto& p : amuse::SortUnorderedMap(m_midiFiles))
        {
            char id[16];
            snprintf(id, 16, "%04X", p.first.id);
            dw.writeString(id, p.second.get().m_path.toUtf8().data());
        }
        athena::io::FileWriter w(QStringToSysString(songsFile.filePath()));
        if (!w.hasError())
            dw.finish(&w);
    }
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

        g.second->setIdDatabases();
        amuse::SystemString groupPath = QStringToSysString(dir.path());
        g.second->getProj().toYAML(groupPath);
        g.second->getPool().toYAML(groupPath);
    }

    saveSongsIndex();

    return true;
}

QStringList ProjectModel::getGroupList() const
{
    QStringList list;
    list.reserve(m_root->childCount());
    m_root->oneLevelTraverse([&list](INode* node)
    {
        list.push_back(node->name());
        return true;
    });
    return list;
}

bool ProjectModel::exportGroup(const QString& path, const QString& groupName, UIMessenger& messenger) const
{
    auto search = m_groups.find(groupName);
    if (search == m_groups.cend())
    {
        messenger.critical(tr("Export Error"), tr("Unable to find group %1").arg(groupName));
        return false;
    }
    const amuse::AudioGroupDatabase& group = *search->second;
    m_projectDatabase.setIdDatabases();
    auto basePath = QStringToSysString(QFileInfo(QDir(path), groupName).filePath());
    group.setIdDatabases();
    if (!group.getProj().toGCNData(basePath, group.getPool(), group.getSdir()))
    {
        messenger.critical(tr("Export Error"), tr("Unable to export %1.proj").arg(groupName));
        return false;
    }
    if (!group.getPool().toData<athena::Big>(basePath))
    {
        messenger.critical(tr("Export Error"), tr("Unable to export %1.pool").arg(groupName));
        return false;
    }
    if (!group.getSdir().toGCNData(basePath, group))
    {
        messenger.critical(tr("Export Error"), tr("Unable to export %1.sdir").arg(groupName));
        return false;
    }
    return true;
}

void ProjectModel::_buildGroupNodeCollections(GroupNode& gn)
{
    gn.reserve(6);
    gn._appendChild<CollectionNode>(tr("Sound Macros"), QIcon(":/icons/IconSoundMacro.svg"), INode::Type::SoundMacro);
    gn._appendChild<CollectionNode>(tr("ADSRs"), QIcon(":/icons/IconADSR.svg"), INode::Type::ADSR);
    gn._appendChild<CollectionNode>(tr("Curves"), QIcon(":/icons/IconCurve.svg"), INode::Type::Curve);
    gn._appendChild<CollectionNode>(tr("Keymaps"), QIcon(":/icons/IconKeymap.svg"), INode::Type::Keymap);
    gn._appendChild<CollectionNode>(tr("Layers"), QIcon(":/icons/IconLayers.svg"), INode::Type::Layer);
    gn._appendChild<CollectionNode>(tr("Samples"), QIcon(":/icons/IconSample.svg"), INode::Type::Sample);
}

void ProjectModel::_buildGroupNode(GroupNode& gn)
{
    amuse::AudioGroup& group = *gn.m_it->second;
    auto& songGroups = group.getProj().songGroups();
    auto& sfxGroups = group.getProj().sfxGroups();
    auto& soundMacros = group.getPool().soundMacros();
    auto& tables = group.getPool().tables();
    auto& keymaps = group.getPool().keymaps();
    auto& layers = group.getPool().layers();
    auto& samples = group.getSdir().sampleEntries();
    gn.reserve(songGroups.size() + sfxGroups.size() + 6);
    _buildGroupNodeCollections(gn);
    for (const auto& grp : SortUnorderedMap(songGroups))
        gn.makeChild<SongGroupNode>(grp.first, grp.second.get());
    for (const auto& grp : SortUnorderedMap(sfxGroups))
        gn.makeChild<SoundGroupNode>(grp.first, grp.second.get());
    {
        CollectionNode& col = *gn.getCollectionOfType(INode::Type::SoundMacro);
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
            CollectionNode& col = *gn.getCollectionOfType(INode::Type::ADSR);
            col.reserve(ADSRCount);
            for (auto& t : tablesSort)
            {
                amuse::ITable::Type tp = (*t.second.get())->Isa();
                if (tp == amuse::ITable::Type::ADSR || tp == amuse::ITable::Type::ADSRDLS)
                    col.makeChild<ADSRNode>(t.first, t.second.get());
            }
        }
        {
            CollectionNode& col = *gn.getCollectionOfType(INode::Type::Curve);
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
        CollectionNode& col = *gn.getCollectionOfType(INode::Type::Keymap);
        col.reserve(keymaps.size());
        for (auto& keymap : SortUnorderedMap(keymaps))
            col.makeChild<KeymapNode>(keymap.first, keymap.second.get());
    }
    {
        CollectionNode& col = *gn.getCollectionOfType(INode::Type::Layer);
        col.reserve(layers.size());
        for (auto& keymap : SortUnorderedMap(layers))
            col.makeChild<LayersNode>(keymap.first, keymap.second.get());
    }
    {
        CollectionNode& col = *gn.getCollectionOfType(INode::Type::Sample);
        col.reserve(samples.size());
        for (auto& sample : SortUnorderedMap(samples))
            col.makeChild<SampleNode>(sample.first, sample.second.get());
    }
}

void ProjectModel::_resetModelData()
{
    beginResetModel();
    m_projectDatabase.setIdDatabases();
    m_root = amuse::MakeObj<RootNode>();
    m_root->reserve(m_groups.size());
    for (auto it = m_groups.begin(); it != m_groups.end(); ++it)
    {
        it->second->setIdDatabases();
        GroupNode& gn = m_root->makeChild<GroupNode>(it);
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
        {
            parentItem = m_root.get();
        }
        else
        {
            assert(parent.model() == this && "Not ProjectModel");
            parentItem = static_cast<INode*>(parent.internalPointer());
        }

        INode* childItem = parentItem->nullChild();
        return createIndex(childItem->row(), column, childItem);
    }

    if (!hasIndex(row, column, parent))
        return QModelIndex();

    INode* parentItem;
    if (!parent.isValid())
    {
        parentItem = m_root.get();
    }
    else
    {
        assert(parent.model() == this && "Not ProjectModel");
        parentItem = static_cast<INode*>(parent.internalPointer());
    }

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
    assert(index.model() == this && "Not ProjectModel");

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
    assert(index.model() == this && "Not ProjectModel");

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
    assert(index.model() == this && "Not ProjectModel");

    return static_cast<INode*>(index.internalPointer())->flags();
}

ProjectModel::INode* ProjectModel::node(const QModelIndex& index) const
{
    if (!index.isValid())
        return m_root.get();
    assert(index.model() == this && "Not ProjectModel");
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
    assert(index.model() == this && "Not ProjectModel");
    return (static_cast<INode*>(index.internalPointer())->flags() & Qt::ItemIsSelectable) != Qt::NoItemFlags;
}

void ProjectModel::_postAddNode(INode* n, const NameUndoRegistry& registry)
{
    setIdDatabases(n);
    n->depthTraverse([&registry](INode* node)
    {
        node->registerNames(registry);
        return true;
    });
}

void ProjectModel::_preDelNode(INode* n, NameUndoRegistry& registry)
{
    n->depthTraverse([&registry](INode* node)
    {
        g_MainWindow->aboutToDeleteNode(node);
        node->unregisterNames(registry);
        return true;
    });
}

class GroupNodeUndoCommand : public QUndoCommand
{
protected:
    std::unique_ptr<amuse::AudioGroupDatabase> m_data;
    amuse::ObjToken<ProjectModel::GroupNode> m_node;
    ProjectModel::NameUndoRegistry m_nameReg;
    void add()
    {
        g_MainWindow->projectModel()->_addNode(m_node.get(), std::move(m_data), m_nameReg);
        g_MainWindow->recursiveExpandAndSelectOutline(g_MainWindow->projectModel()->index(m_node.get()));
    }
    void del()
    {
        m_data = g_MainWindow->projectModel()->_delNode(m_node.get(), m_nameReg);
    }
public:
    explicit GroupNodeUndoCommand(const QString& text, std::unique_ptr<amuse::AudioGroupDatabase>&& data, ProjectModel::GroupNode* node)
    : QUndoCommand(text.arg(node->text())), m_data(std::move(data)), m_node(node) {}
};

class GroupNodeAddUndoCommand : public GroupNodeUndoCommand
{
    using base = GroupNodeUndoCommand;
public:
    explicit GroupNodeAddUndoCommand(const QString& text, std::unique_ptr<amuse::AudioGroupDatabase>&& data, ProjectModel::GroupNode* node)
    : GroupNodeUndoCommand(text, std::move(data), node) {}
    void undo() { base::del(); }
    void redo() { base::add(); }
};

class GroupNodeDelUndoCommand : public GroupNodeUndoCommand
{
    using base = GroupNodeUndoCommand;
public:
    explicit GroupNodeDelUndoCommand(const QString& text, ProjectModel::GroupNode* node)
    : GroupNodeUndoCommand(text, {}, node) {}
    void undo() { base::add(); }
    void redo() { base::del(); }
};

void ProjectModel::_addNode(GroupNode* node, std::unique_ptr<amuse::AudioGroupDatabase>&& data, const NameUndoRegistry& registry)
{
    int idx = m_root->hypotheticalIndex(node->name());
    beginInsertRows(QModelIndex(), idx, idx);
    QDir dir(QFileInfo(m_dir, node->name()).filePath());
    node->m_it = m_groups.emplace(std::make_pair(node->name(), std::move(data))).first;
    m_root->insertChild(node);
    _postAddNode(node, registry);
    endInsertRows();
}

std::unique_ptr<amuse::AudioGroupDatabase> ProjectModel::_delNode(GroupNode* node, NameUndoRegistry& registry)
{
    int idx = node->row();
    beginRemoveRows(QModelIndex(), idx, idx);
    _preDelNode(node, registry);
    std::unique_ptr<amuse::AudioGroupDatabase> ret = std::move(node->m_it->second);
    m_groups.erase(node->m_it);
    node->m_it = {};
    m_root->removeChild(node);
    endRemoveRows();
    return ret;
}

ProjectModel::GroupNode* ProjectModel::newSubproject(const QString& name)
{
    if (m_groups.find(name) != m_groups.cend())
    {
        g_MainWindow->uiMessenger().
        critical(tr("Subproject Conflict"), tr("The subproject %1 is already defined").arg(name));
        return nullptr;
    }
    QString path = QFileInfo(m_dir, name).filePath();
    auto data = std::make_unique<amuse::AudioGroupDatabase>(QStringToSysString(path));
    auto node = amuse::MakeObj<GroupNode>(name);
    _buildGroupNodeCollections(*node);
    g_MainWindow->pushUndoCommand(new GroupNodeAddUndoCommand(tr("Add Subproject %1"), std::move(data), node.get()));
    return node.get();
}

template <class NT>
class NodeUndoCommand : public QUndoCommand
{
protected:
    amuse::ObjToken<ProjectModel::GroupNode> m_parent;
    amuse::ObjToken<NT> m_node;
    ProjectModel::NameUndoRegistry m_nameReg;
    void add()
    {
        g_MainWindow->projectModel()->_addNode(m_node.get(), m_parent.get(), m_nameReg);
        g_MainWindow->recursiveExpandAndSelectOutline(g_MainWindow->projectModel()->index(m_node.get()));
    }
    void del()
    {
        g_MainWindow->projectModel()->_delNode(m_node.get(), m_parent.get(), m_nameReg);
    }
public:
    explicit NodeUndoCommand(const QString& text, NT* node, ProjectModel::GroupNode* parent)
    : QUndoCommand(text.arg(node->text())), m_parent(parent), m_node(node){}
};

template <class NT>
class NodeAddUndoCommand : public NodeUndoCommand<NT>
{
    using base = NodeUndoCommand<NT>;
public:
    explicit NodeAddUndoCommand(const QString& text, NT* node, ProjectModel::GroupNode* parent)
    : NodeUndoCommand<NT>(text, node, parent) {}
    void undo() { base::del(); }
    void redo() { base::add(); }
};

template <class NT>
class NodeDelUndoCommand : public NodeUndoCommand<NT>
{
    using base = NodeUndoCommand<NT>;
public:
    explicit NodeDelUndoCommand(const QString& text, NT* node)
    : NodeUndoCommand<NT>(text, node, g_MainWindow->projectModel()->getGroupNode(node)) {}
    void undo() { base::add(); }
    void redo() { base::del(); }
};

template <class NT, class T>
void ProjectModel::_addGroupNode(NT* node, GroupNode* parent, const NameUndoRegistry& registry, T& container)
{
    int idx = parent->hypotheticalIndex(node->name());
    beginInsertRows(index(parent), idx, idx);
    setIdDatabases(parent);
    amuse::GroupId newId = amuse::GroupId::CurNameDB->generateId(amuse::NameDB::Type::Group);
    amuse::GroupId::CurNameDB->registerPair(node->name().toUtf8().data(), newId);
    container[newId] = node->m_index;
    node->m_id = newId;
    parent->insertChild(node);
    _postAddNode(node, registry);
    endInsertRows();
}

template <class NT, class T>
void ProjectModel::_delGroupNode(NT* node, GroupNode* parent, NameUndoRegistry& registry, T& container)
{
    int idx = node->row();
    beginRemoveRows(index(parent), idx, idx);
    _preDelNode(node, registry);
    setIdDatabases(parent);
    amuse::GroupId::CurNameDB->remove(node->m_id);
    container.erase(node->m_id);
    node->m_id = {};
    parent->removeChild(node);
    endRemoveRows();
}

void ProjectModel::_addNode(SoundGroupNode* node, GroupNode* parent, const NameUndoRegistry& registry)
{
    _addGroupNode(node, parent, registry, parent->getAudioGroup()->getProj().sfxGroups());
}

void ProjectModel::_delNode(SoundGroupNode* node, GroupNode* parent, NameUndoRegistry& registry)
{
    _delGroupNode(node, parent, registry, parent->getAudioGroup()->getProj().sfxGroups());
}

ProjectModel::SoundGroupNode* ProjectModel::newSoundGroup(GroupNode* group, const QString& name)
{
    setIdDatabases(group);
    if (amuse::GroupId::CurNameDB->m_stringToId.find(name.toUtf8().data()) !=
        amuse::GroupId::CurNameDB->m_stringToId.cend())
    {
        g_MainWindow->uiMessenger().
        critical(tr("Sound Group Conflict"), tr("The group %1 is already defined").arg(name));
        return nullptr;
    }
    auto node = amuse::MakeObj<SoundGroupNode>(name, amuse::MakeObj<amuse::SFXGroupIndex>());
    g_MainWindow->pushUndoCommand(new NodeAddUndoCommand(tr("Add Sound Group %1"), node.get(), group));
    return node.get();
}

void ProjectModel::_addNode(SongGroupNode* node, GroupNode* parent, const NameUndoRegistry& registry)
{
    _addGroupNode(node, parent, registry, parent->getAudioGroup()->getProj().songGroups());
}

void ProjectModel::_delNode(SongGroupNode* node, GroupNode* parent, NameUndoRegistry& registry)
{
    _delGroupNode(node, parent, registry, parent->getAudioGroup()->getProj().songGroups());
}

ProjectModel::SongGroupNode* ProjectModel::newSongGroup(GroupNode* group, const QString& name)
{
    setIdDatabases(group);
    if (amuse::GroupId::CurNameDB->m_stringToId.find(name.toUtf8().data()) !=
        amuse::GroupId::CurNameDB->m_stringToId.cend())
    {
        g_MainWindow->uiMessenger().
        critical(tr("Song Group Conflict"), tr("The group %1 is already defined").arg(name));
        return nullptr;
    }
    auto node = amuse::MakeObj<SongGroupNode>(name, amuse::MakeObj<amuse::SongGroupIndex>());
    g_MainWindow->pushUndoCommand(new NodeAddUndoCommand(tr("Add Song Group %1"), node.get(), group));
    return node.get();
}

static constexpr ProjectModel::INode::Type GetINodeType(ProjectModel::SoundMacroNode*)
{ return ProjectModel::INode::Type::SoundMacro; }
static constexpr ProjectModel::INode::Type GetINodeType(ProjectModel::ADSRNode*)
{ return ProjectModel::INode::Type::ADSR; }
static constexpr ProjectModel::INode::Type GetINodeType(ProjectModel::CurveNode*)
{ return ProjectModel::INode::Type::Curve; }
static constexpr ProjectModel::INode::Type GetINodeType(ProjectModel::KeymapNode*)
{ return ProjectModel::INode::Type::Keymap; }
static constexpr ProjectModel::INode::Type GetINodeType(ProjectModel::LayersNode*)
{ return ProjectModel::INode::Type::Layer; }

static constexpr amuse::NameDB::Type GetNameDBType(ProjectModel::SoundMacroNode*)
{ return amuse::NameDB::Type::SoundMacro; }
static constexpr amuse::NameDB::Type GetNameDBType(ProjectModel::ADSRNode*)
{ return amuse::NameDB::Type::Table; }
static constexpr amuse::NameDB::Type GetNameDBType(ProjectModel::CurveNode*)
{ return amuse::NameDB::Type::Table; }
static constexpr amuse::NameDB::Type GetNameDBType(ProjectModel::KeymapNode*)
{ return amuse::NameDB::Type::Keymap; }
static constexpr amuse::NameDB::Type GetNameDBType(ProjectModel::LayersNode*)
{ return amuse::NameDB::Type::Layer; }

static amuse::NameDB* GetNameDB(ProjectModel::SoundMacroNode*) { return amuse::SoundMacroId::CurNameDB; }
static amuse::NameDB* GetNameDB(ProjectModel::ADSRNode*) { return amuse::TableId::CurNameDB; }
static amuse::NameDB* GetNameDB(ProjectModel::CurveNode*) { return amuse::TableId::CurNameDB; }
static amuse::NameDB* GetNameDB(ProjectModel::KeymapNode*) { return amuse::KeymapId::CurNameDB; }
static amuse::NameDB* GetNameDB(ProjectModel::LayersNode*) { return amuse::LayersId::CurNameDB; }

template <class NT, class T>
void ProjectModel::_addPoolNode(NT* node, GroupNode* parent, const NameUndoRegistry& registry, T& container)
{
    setIdDatabases(parent);
    CollectionNode* coll = parent->getCollectionOfType(GetINodeType(node));
    int insertIdx = coll->hypotheticalIndex(node->name());
    beginInsertRows(index(coll), insertIdx, insertIdx);
    auto newId = GetNameDB(node)->generateId(GetNameDBType(node));
    GetNameDB(node)->registerPair(node->name().toUtf8().data(), newId);
    container[newId] = node->m_obj;
    node->m_id = newId;
    coll->insertChild(node);
    _postAddNode(node, registry);
    endInsertRows();
}

template <class NT, class T>
void ProjectModel::_delPoolNode(NT* node, GroupNode* parent, NameUndoRegistry& registry, T& container)
{
    int idx = node->row();
    CollectionNode* coll = parent->getCollectionOfType(GetINodeType(node));
    beginRemoveRows(index(coll), idx, idx);
    _preDelNode(node, registry);
    setIdDatabases(parent);
    GetNameDB(node)->remove(node->m_id);
    container.erase(node->m_id);
    node->m_id = {};
    coll->removeChild(node);
    endRemoveRows();
}

void ProjectModel::_addNode(SoundMacroNode* node, GroupNode* parent, const NameUndoRegistry& registry)
{
    _addPoolNode(node, parent, registry, parent->getAudioGroup()->getPool().soundMacros());
}

void ProjectModel::_delNode(SoundMacroNode* node, GroupNode* parent, NameUndoRegistry& registry)
{
    _delPoolNode(node, parent, registry, parent->getAudioGroup()->getPool().soundMacros());
}

ProjectModel::SoundMacroNode* ProjectModel::newSoundMacro(GroupNode* group, const QString& name,
                                                          const SoundMacroTemplateEntry* templ)
{
    setIdDatabases(group);
    if (amuse::SoundMacroId::CurNameDB->m_stringToId.find(name.toUtf8().data()) !=
        amuse::SoundMacroId::CurNameDB->m_stringToId.cend())
    {
        g_MainWindow->uiMessenger().
        critical(tr("Sound Macro Conflict"), tr("The macro %1 is already defined").arg(name));
        return nullptr;
    }
    auto dataNode = amuse::MakeObj<amuse::SoundMacro>();
    if (templ)
    {
        athena::io::MemoryReader r(templ->m_data, templ->m_length);
        dataNode->readCmds<athena::utility::NotSystemEndian>(r, templ->m_length);
    }
    auto node = amuse::MakeObj<SoundMacroNode>(name, dataNode);
    g_MainWindow->pushUndoCommand(new NodeAddUndoCommand(tr("Add Sound Macro %1"), node.get(), group));
    return node.get();
}

void ProjectModel::_addNode(ADSRNode* node, GroupNode* parent, const NameUndoRegistry& registry)
{
    _addPoolNode(node, parent, registry, parent->getAudioGroup()->getPool().tables());
}

void ProjectModel::_delNode(ADSRNode* node, GroupNode* parent, NameUndoRegistry& registry)
{
    _delPoolNode(node, parent, registry, parent->getAudioGroup()->getPool().tables());
}

ProjectModel::ADSRNode* ProjectModel::newADSR(GroupNode* group, const QString& name)
{
    setIdDatabases(group);
    if (amuse::TableId::CurNameDB->m_stringToId.find(name.toUtf8().data()) !=
        amuse::TableId::CurNameDB->m_stringToId.cend())
    {
        g_MainWindow->uiMessenger().
        critical(tr("ADSR Conflict"), tr("The ADSR %1 is already defined").arg(name));
        return nullptr;
    }
    auto dataNode = amuse::MakeObj<std::unique_ptr<amuse::ITable>>();
    *dataNode = std::make_unique<amuse::ADSR>();
    auto node = amuse::MakeObj<ADSRNode>(name, dataNode);
    g_MainWindow->pushUndoCommand(new NodeAddUndoCommand(tr("Add ADSR %1"), node.get(), group));
    return node.get();
}

void ProjectModel::_addNode(CurveNode* node, GroupNode* parent, const NameUndoRegistry& registry)
{
    _addPoolNode(node, parent, registry, parent->getAudioGroup()->getPool().tables());
}

void ProjectModel::_delNode(CurveNode* node, GroupNode* parent, NameUndoRegistry& registry)
{
    _delPoolNode(node, parent, registry, parent->getAudioGroup()->getPool().tables());
}

ProjectModel::CurveNode* ProjectModel::newCurve(GroupNode* group, const QString& name)
{
    setIdDatabases(group);
    if (amuse::TableId::CurNameDB->m_stringToId.find(name.toUtf8().data()) !=
        amuse::TableId::CurNameDB->m_stringToId.cend())
    {
        g_MainWindow->uiMessenger().
        critical(tr("Curve Conflict"), tr("The Curve %1 is already defined").arg(name));
        return nullptr;
    }
    auto dataNode = amuse::MakeObj<std::unique_ptr<amuse::ITable>>();
    *dataNode = std::make_unique<amuse::Curve>();
    auto node = amuse::MakeObj<CurveNode>(name, dataNode);
    g_MainWindow->pushUndoCommand(new NodeAddUndoCommand(tr("Add Curve %1"), node.get(), group));
    return node.get();
}

void ProjectModel::_addNode(KeymapNode* node, GroupNode* parent, const NameUndoRegistry& registry)
{
    _addPoolNode(node, parent, registry, parent->getAudioGroup()->getPool().keymaps());
}

void ProjectModel::_delNode(KeymapNode* node, GroupNode* parent, NameUndoRegistry& registry)
{
    _delPoolNode(node, parent, registry, parent->getAudioGroup()->getPool().keymaps());
}

ProjectModel::KeymapNode* ProjectModel::newKeymap(GroupNode* group, const QString& name)
{
    setIdDatabases(group);
    if (amuse::KeymapId::CurNameDB->m_stringToId.find(name.toUtf8().data()) !=
        amuse::KeymapId::CurNameDB->m_stringToId.cend())
    {
        g_MainWindow->uiMessenger().
        critical(tr("Keymap Conflict"), tr("The Keymap %1 is already defined").arg(name));
        return nullptr;
    }
    auto dataNode = amuse::MakeObj<std::array<amuse::Keymap, 128>>();
    auto node = amuse::MakeObj<KeymapNode>(name, dataNode);
    g_MainWindow->pushUndoCommand(new NodeAddUndoCommand(tr("Add Keymap %1"), node.get(), group));
    return node.get();
}

void ProjectModel::_addNode(LayersNode* node, GroupNode* parent, const NameUndoRegistry& registry)
{
    _addPoolNode(node, parent, registry, parent->getAudioGroup()->getPool().layers());
}

void ProjectModel::_delNode(LayersNode* node, GroupNode* parent, NameUndoRegistry& registry)
{
    _delPoolNode(node, parent, registry, parent->getAudioGroup()->getPool().layers());
}

ProjectModel::LayersNode* ProjectModel::newLayers(GroupNode* group, const QString& name)
{
    setIdDatabases(group);
    if (amuse::LayersId::CurNameDB->m_stringToId.find(name.toUtf8().data()) !=
        amuse::LayersId::CurNameDB->m_stringToId.cend())
    {
        g_MainWindow->uiMessenger().
        critical(tr("Layers Conflict"), tr("Layers %1 is already defined").arg(name));
        return nullptr;
    }
    auto dataNode = amuse::MakeObj<std::vector<amuse::LayerMapping>>();
    auto node = amuse::MakeObj<LayersNode>(name, dataNode);
    g_MainWindow->pushUndoCommand(new NodeAddUndoCommand(tr("Add Layers %1"), node.get(), group));
    return node.get();
}

void ProjectModel::del(const QModelIndex& index)
{
    if (!index.isValid())
        return;
    assert(index.model() == this && "Not ProjectModel");
    INode* n = node(index);
    QUndoCommand* cmd = nullptr;
    switch (n->type())
    {
    case INode::Type::Group:
        cmd = new GroupNodeDelUndoCommand(tr("Delete Subproject %1"), static_cast<GroupNode*>(n));
        break;
    case INode::Type::SongGroup:
        cmd = new NodeDelUndoCommand(tr("Delete SongGroup %1"), static_cast<SongGroupNode*>(n));
        break;
    case INode::Type::SoundGroup:
        cmd = new NodeDelUndoCommand(tr("Delete SFXGroup %1"), static_cast<SoundGroupNode*>(n));
        break;
    case INode::Type::SoundMacro:
        cmd = new NodeDelUndoCommand(tr("Delete SoundMacro %1"), static_cast<SoundMacroNode*>(n));
        break;
    case INode::Type::ADSR:
        cmd = new NodeDelUndoCommand(tr("Delete ADSR %1"), static_cast<ADSRNode*>(n));
        break;
    case INode::Type::Curve:
        cmd = new NodeDelUndoCommand(tr("Delete Curve %1"), static_cast<CurveNode*>(n));
        break;
    case INode::Type::Keymap:
        cmd = new NodeDelUndoCommand(tr("Delete Keymap %1"), static_cast<KeymapNode*>(n));
        break;
    case INode::Type::Layer:
        cmd = new NodeDelUndoCommand(tr("Delete Layers %1"), static_cast<LayersNode*>(n));
        break;
    default:
        break;
    }
    if (cmd)
        g_MainWindow->pushUndoCommand(cmd);
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

QString ProjectModel::getMIDIPathOfSong(amuse::SongId id) const
{
    auto search = m_midiFiles.find(id);
    if (search == m_midiFiles.cend())
        return {};
    return search->second.m_path;
}

void ProjectModel::setMIDIPathOfSong(amuse::SongId id, const QString& path)
{
    m_midiFiles[id].m_path = path;
}

void ProjectModel::_allocateSongId(amuse::SongId id, std::string_view name)
{
    m_projectDatabase.setIdDatabases();
    amuse::SongId::CurNameDB->registerPair(name, id);
    Song& song = m_midiFiles[id];
    ++song.m_refCount;
}

std::pair<amuse::SongId, std::string> ProjectModel::allocateSongId()
{
    m_projectDatabase.setIdDatabases();
    std::pair<amuse::SongId, std::string> ret;
    ret.first = amuse::SongId::CurNameDB->generateId(amuse::NameDB::Type::Song);
    ret.second = amuse::SongId::CurNameDB->generateName(ret.first, amuse::NameDB::Type::Song);
    amuse::SongId::CurNameDB->registerPair(ret.second, ret.first);
    m_midiFiles[ret.first] = {{}, 1};
    return ret;
}

void ProjectModel::deallocateSongId(amuse::SongId oldId)
{
    Song& oldSong = m_midiFiles[oldId];
    --oldSong.m_refCount;
    if (oldSong.m_refCount <= 0)
    {
        oldSong.m_refCount = 0;
        amuse::SongId::CurNameDB->remove(oldId);
        m_midiFiles.erase(oldId);
    }
}

amuse::SongId ProjectModel::exchangeSongId(amuse::SongId oldId, std::string_view newName)
{
    m_projectDatabase.setIdDatabases();
    amuse::SongId newId;
    auto search = amuse::SongId::CurNameDB->m_stringToId.find(newName.data());
    if (search == amuse::SongId::CurNameDB->m_stringToId.cend())
    {
        newId = amuse::SongId::CurNameDB->generateId(amuse::NameDB::Type::Song);
        amuse::SongId::CurNameDB->registerPair(newName, newId);
    }
    else
        newId = search->second;
    if (oldId == newId)
        return newId;
    Song& oldSong = m_midiFiles[oldId];
    Song& newSong = m_midiFiles[newId];
    ++newSong.m_refCount;
    if (newSong.m_path.isEmpty())
        newSong.m_path = oldSong.m_path;
    --oldSong.m_refCount;
    if (oldSong.m_refCount <= 0)
    {
        oldSong.m_refCount = 0;
        amuse::SongId::CurNameDB->remove(oldId);
        m_midiFiles.erase(oldId);
    }
    return newId;
}

void ProjectModel::setIdDatabases(INode* context) const
{
    m_projectDatabase.setIdDatabases();
    if (ProjectModel::GroupNode* group = getGroupNode(context))
        group->getAudioGroup()->setIdDatabases();
}
