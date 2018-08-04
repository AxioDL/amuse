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

ProjectModel::ProjectModel(const QString& path, QObject* parent)
: QAbstractItemModel(parent), m_dir(path), m_nullProxy(this)
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

    return true;
}

void ProjectModel::_resetModelData()
{
    beginResetModel();
    m_projectDatabase.setIdDatabases();
    m_root = amuse::MakeObj<RootNode>();
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
        return 0;

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
public:
    DeleteNodeUndoCommand(const QModelIndex& index)
     : QUndoCommand(ProjectModel::tr("Delete %1").arg(index.data().toString())), m_deleteIdx(index) {}
    void undo()
    {
        g_MainWindow->projectModel()->_undoDel(m_deleteIdx, std::move(m_node));
        m_node.reset();
    }
    void redo()
    {
        m_node = g_MainWindow->projectModel()->_redoDel(m_deleteIdx);
    }
};

void ProjectModel::_undoDel(const QModelIndex& index, amuse::ObjToken<ProjectModel::INode> n)
{
    beginInsertRows(index.parent(), index.row(), index.row());
    node(index.parent())->insertChild(index.row(), std::move(n));
    endInsertRows();
}

amuse::ObjToken<ProjectModel::INode> ProjectModel::_redoDel(const QModelIndex& index)
{
    node(index)->depthTraverse([](INode* node)
    {
        g_MainWindow->aboutToDeleteNode(node);
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
