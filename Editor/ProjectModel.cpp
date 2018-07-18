#include <athena/FileWriter.hpp>
#include <athena/FileReader.hpp>
#include "ProjectModel.hpp"
#include "Common.hpp"
#include "athena/YAMLDocWriter.hpp"

QIcon ProjectModel::GroupNode::Icon;
QIcon ProjectModel::SongGroupNode::Icon;
QIcon ProjectModel::SoundGroupNode::Icon;

ProjectModel::ProjectModel(const QString& path, QObject* parent)
: QAbstractItemModel(parent), m_dir(path)
{
    m_root = std::make_unique<RootNode>();

    GroupNode::Icon = QIcon(":/icons/IconGroup.svg");
    SongGroupNode::Icon = QIcon(":/icons/IconSongGroup.svg");
    SoundGroupNode::Icon = QIcon(":/icons/IconSoundGroup.svg");
}

bool ProjectModel::clearProjectData()
{
    m_projectDatabase = amuse::ProjectDatabase();
    m_groups.clear();

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

bool ProjectModel::importGroupData(const QString& groupName, const amuse::AudioGroupData& data,
                                   ImportMode mode, UIMessenger& messenger)
{
    m_projectDatabase.setIdDatabases();

    amuse::AudioGroupDatabase& grp = m_groups.insert(std::make_pair(groupName, data)).first->second;
    grp.setIdDatabases();
    amuse::AudioGroupProject::BootstrapObjectIDs(data);

    if (!MkPath(m_dir.path(), messenger))
        return false;
    QDir dir(QFileInfo(m_dir, groupName).filePath());
    if (!MkPath(dir.path(), messenger))
        return false;

    amuse::SystemString sysDir = QStringToSysString(dir.path());
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

    return true;
}

void ProjectModel::_resetModelData()
{
    beginResetModel();
    m_projectDatabase.setIdDatabases();
    m_root = std::make_unique<RootNode>();
    m_root->reserve(m_groups.size());
    for (auto it = m_groups.begin() ; it != m_groups.end() ; ++it)
    {
        it->second.setIdDatabases();
        GroupNode& gn = m_root->makeChild<GroupNode>(it);
        amuse::AudioGroup& group = it->second;
        auto& songGroups = group.getProj().songGroups();
        auto& sfxGroups = group.getProj().sfxGroups();
        auto& soundMacros = group.getPool().soundMacros();
        auto& tables = group.getPool().tables();
        auto& keymaps = group.getPool().keymaps();
        auto& layers = group.getPool().layers();
        gn.reserve(songGroups.size() + sfxGroups.size() + 4);
        for (const auto& grp : SortUnorderedMap(songGroups))
            gn.makeChild<SongGroupNode>(grp.first, grp.second.get());
        for (const auto& grp : SortUnorderedMap(sfxGroups))
            gn.makeChild<SoundGroupNode>(grp.first, grp.second.get());
        if (soundMacros.size())
        {
            CollectionNode& col =
                gn.makeChild<CollectionNode>(tr("Sound Macros"), QIcon(":/icons/IconSoundMacro.svg"));
            col.reserve(soundMacros.size());
            for (const auto& macro : SortUnorderedMap(soundMacros))
                col.makeChild<SoundMacroNode>(macro.first, macro.second.get());
        }
        if (tables.size())
        {
            auto tablesSort = SortUnorderedMap(tables);
            size_t ADSRCount = 0;
            size_t curveCount = 0;
            for (auto& t : tablesSort)
            {
                amuse::ITable::Type tp = t.second.get()->Isa();
                if (tp == amuse::ITable::Type::ADSR || tp == amuse::ITable::Type::ADSRDLS)
                    ADSRCount += 1;
                else if (tp == amuse::ITable::Type::Curve)
                    curveCount += 1;
            }
            if (ADSRCount)
            {
                CollectionNode& col =
                    gn.makeChild<CollectionNode>(tr("ADSRs"), QIcon(":/icons/IconADSR.svg"));
                col.reserve(ADSRCount);
                for (auto& t : tablesSort)
                {
                    amuse::ITable::Type tp = t.second.get()->Isa();
                    if (tp == amuse::ITable::Type::ADSR || tp == amuse::ITable::Type::ADSRDLS)
                        col.makeChild<ADSRNode>(t.first, *t.second.get());
                }
            }
            if (curveCount)
            {
                CollectionNode& col =
                    gn.makeChild<CollectionNode>(tr("Curves"), QIcon(":/icons/IconCurve.svg"));
                col.reserve(curveCount);
                for (auto& t : tablesSort)
                {
                    amuse::ITable::Type tp = t.second.get()->Isa();
                    if (tp == amuse::ITable::Type::Curve)
                        col.makeChild<CurveNode>(t.first, static_cast<amuse::Curve&>(*t.second.get()));
                }
            }
        }
        if (keymaps.size())
        {
            CollectionNode& col =
                gn.makeChild<CollectionNode>(tr("Keymaps"), QIcon(":/icons/IconKeymap.svg"));
            col.reserve(keymaps.size());
            for (auto& keymap : SortUnorderedMap(keymaps))
                col.makeChild<KeymapNode>(keymap.first, keymap.second.get());
        }
        if (layers.size())
        {
            CollectionNode& col =
                gn.makeChild<CollectionNode>(tr("Layers"), QIcon(":/icons/IconLayers.svg"));
            col.reserve(layers.size());
            for (auto& keymap : SortUnorderedMap(layers))
                col.makeChild<LayersNode>(keymap.first, keymap.second.get());
        }
    }
    endResetModel();
}

void ProjectModel::ensureModelData()
{
    if (m_needsReset)
    {
        _resetModelData();
        m_needsReset = false;
    }
}

QModelIndex ProjectModel::index(int row, int column, const QModelIndex& parent) const
{
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
        return 0;

    return QAbstractItemModel::flags(index);
}

ProjectModel::INode* ProjectModel::node(const QModelIndex& index) const
{
    if (!index.isValid())
        return nullptr;
    return static_cast<INode*>(index.internalPointer());
}

bool ProjectModel::canDelete() const
{
    return false;
}

void ProjectModel::del()
{

}
