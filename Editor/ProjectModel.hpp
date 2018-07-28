#ifndef AMUSE_PROJECT_MODEL_HPP
#define AMUSE_PROJECT_MODEL_HPP

#include <QAbstractItemModel>
#include <QIdentityProxyModel>
#include <QDir>
#include <QIcon>
#include <map>
#include "Common.hpp"
#include "amuse/AudioGroup.hpp"
#include "amuse/AudioGroupData.hpp"
#include "amuse/AudioGroupProject.hpp"
#include "amuse/AudioGroupPool.hpp"
#include "amuse/AudioGroupSampleDirectory.hpp"

class ProjectModel;

class NullItemProxyModel : public QIdentityProxyModel
{
    Q_OBJECT
public:
    explicit NullItemProxyModel(ProjectModel* source);
    QModelIndex mapFromSource(const QModelIndex& sourceIndex) const;
    QModelIndex mapToSource(const QModelIndex& proxyIndex) const;
    int rowCount(const QModelIndex& parent) const;
    QModelIndex index(int row, int column, const QModelIndex& parent) const;
    QVariant data(const QModelIndex& proxyIndex, int role) const;
};

class ProjectModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    enum class ImportMode
    {
        Original,
        WAVs,
        Both
    };

private:
    QDir m_dir;
    NullItemProxyModel m_nullProxy;

    amuse::ProjectDatabase m_projectDatabase;
    std::map<QString, amuse::AudioGroupDatabase> m_groups;

public:
    class INode : public std::enable_shared_from_this<INode>
    {
    public:
        enum class Type
        {
            Null,
            Root,
            Group, // Top-level group
            SongGroup,
            SoundGroup,
            Collection, // Classified object collection, one of the following:
            SoundMacro,
            ADSR,
            Curve,
            Keymap,
            Layer
        };
    protected:
        INode* m_parent;
        std::vector<std::shared_ptr<INode>> m_children;
        std::unique_ptr<INode> m_nullChild;
        int m_row;
    public:
        virtual ~INode() = default;
        INode(INode* parent) : m_parent(parent), m_row(0)
        {
            /* ONLY USED BY NULL NODE! */
        }
        INode(INode* parent, int row);

        int childCount() const { return int(m_children.size()); }
        INode* child(int row) const
        {
            if (row == m_children.size())
                return nullChild();
            return m_children[row].get();
        }
        INode* nullChild() const { return m_nullChild.get(); }
        INode* parent() const { return m_parent; }
        int row() const { return m_row; }

        void reindexRows(int row)
        {
            for (auto it = m_children.begin() + row; it != m_children.end(); ++it)
                (*it)->m_row = row++;
            m_nullChild->m_row = row;
        }

        void insertChild(int row, std::shared_ptr<INode>&& n)
        {
            m_children.insert(m_children.begin() + row, std::move(n));
            reindexRows(row);
        }
        std::shared_ptr<INode> removeChild(int row)
        {
            std::shared_ptr<INode> ret = std::move(m_children[row]);
            m_children.erase(m_children.begin() + row);
            reindexRows(row);
            return ret;
        }

        void reserve(size_t sz) { m_children.reserve(sz); }
        template<class T, class... _Args>
        T& makeChild(_Args&&... args)
        {
            m_children.push_back(std::make_shared<T>(this, m_children.size(), std::forward<_Args>(args)...));
            m_nullChild->m_row = int(m_children.size());
            return static_cast<T&>(*m_children.back());
        }

        bool depthTraverse(const std::function<bool(INode* node)>& func)
        {
            for (auto& n : m_children)
                if (!n->depthTraverse(func))
                    break;
            return func(this);
        }

        virtual Type type() const = 0;
        virtual QString text() const = 0;
        virtual QIcon icon() const = 0;
        virtual Qt::ItemFlags flags() const { return Qt::ItemIsEnabled | Qt::ItemIsSelectable; }
    };
    struct NullNode : INode
    {
        NullNode(INode* parent) : INode(parent) {}

        Type type() const { return Type::Null; }
        QString text() const { return {}; }
        QIcon icon() const { return {}; }
    };
    struct RootNode : INode
    {
        RootNode() : INode(nullptr, 0) {}

        Type type() const { return Type::Root; }
        QString text() const { return {}; }
        QIcon icon() const { return {}; }
        Qt::ItemFlags flags() const { return Qt::ItemIsEnabled; }
    };
    struct CollectionNode;
    struct GroupNode : INode
    {
        std::map<QString, amuse::AudioGroupDatabase>::iterator m_it;
        GroupNode(INode* parent, int row, std::map<QString, amuse::AudioGroupDatabase>::iterator it)
        : INode(parent, row), m_it(it) {}

        static QIcon Icon;
        Type type() const { return Type::Group; }
        QString text() const { return m_it->first; }
        QIcon icon() const { return Icon; }

        CollectionNode* getCollectionOfType(Type tp) const;
        amuse::AudioGroupDatabase* getAudioGroup() const { return &m_it->second; }

        std::shared_ptr<GroupNode> shared_from_this()
        { return std::static_pointer_cast<GroupNode>(INode::shared_from_this()); }
    };
    struct SongGroupNode : INode
    {
        amuse::GroupId m_id;
        QString m_name;
        std::shared_ptr<amuse::SongGroupIndex> m_index;
        SongGroupNode(INode* parent, int row, amuse::GroupId id, std::shared_ptr<amuse::SongGroupIndex> index)
        : INode(parent, row), m_id(id), m_name(amuse::GroupId::CurNameDB->resolveNameFromId(id).data()), m_index(index) {}

        static QIcon Icon;
        Type type() const { return Type::SongGroup; }
        QString text() const { return m_name; }
        QIcon icon() const { return Icon; }

        std::shared_ptr<SongGroupNode> shared_from_this()
        { return std::static_pointer_cast<SongGroupNode>(INode::shared_from_this()); }
    };
    struct SoundGroupNode : INode
    {
        amuse::GroupId m_id;
        QString m_name;
        std::shared_ptr<amuse::SFXGroupIndex> m_index;
        SoundGroupNode(INode* parent, int row, amuse::GroupId id, std::shared_ptr<amuse::SFXGroupIndex> index)
        : INode(parent, row), m_id(id), m_name(amuse::GroupId::CurNameDB->resolveNameFromId(id).data()), m_index(index) {}

        static QIcon Icon;
        Type type() const { return Type::SoundGroup; }
        QString text() const { return m_name; }
        QIcon icon() const { return Icon; }

        std::shared_ptr<SoundGroupNode> shared_from_this()
        { return std::static_pointer_cast<SoundGroupNode>(INode::shared_from_this()); }
    };
    struct BasePoolObjectNode;
    struct CollectionNode : INode
    {
        QString m_name;
        QIcon m_icon;
        Type m_collectionType;
        CollectionNode(INode* parent, int row, const QString& name, const QIcon& icon, Type collectionType)
        : INode(parent, row), m_name(name), m_icon(icon), m_collectionType(collectionType) {}

        Type type() const { return Type::Collection; }
        QString text() const { return m_name; }
        QIcon icon() const { return m_icon; }
        Qt::ItemFlags flags() const { return Qt::ItemIsEnabled; }

        Type collectionType() const { return m_collectionType; }
        int indexOfId(amuse::ObjectId id) const;
        amuse::ObjectId idOfIndex(int idx) const;
        BasePoolObjectNode* nodeOfIndex(int idx) const;

        std::shared_ptr<CollectionNode> shared_from_this()
        { return std::static_pointer_cast<CollectionNode>(INode::shared_from_this()); }
    };
    struct BasePoolObjectNode : INode
    {
        amuse::ObjectId m_id;
        BasePoolObjectNode(INode* parent, int row, amuse::ObjectId id)
        : INode(parent, row), m_id(id) {}
        amuse::ObjectId id() const { return m_id; }
    };
    template <class ID, class T, INode::Type TP>
    struct PoolObjectNode : BasePoolObjectNode
    {
        QString m_name;
        std::shared_ptr<T> m_obj;
        PoolObjectNode(INode* parent, int row, ID id, std::shared_ptr<T> obj)
        : BasePoolObjectNode(parent, row, id), m_name(ID::CurNameDB->resolveNameFromId(id).data()), m_obj(obj) {}

        Type type() const { return TP; }
        QString text() const { return m_name; }
        QIcon icon() const { return {}; }

        std::shared_ptr<PoolObjectNode<ID, T, TP>> shared_from_this()
        { return std::static_pointer_cast<PoolObjectNode<ID, T, TP>>(INode::shared_from_this()); }
    };
    using SoundMacroNode = PoolObjectNode<amuse::SoundMacroId, amuse::SoundMacro, INode::Type::SoundMacro>;
    using ADSRNode = PoolObjectNode<amuse::TableId, amuse::ITable, INode::Type::ADSR>;
    using CurveNode = PoolObjectNode<amuse::TableId, amuse::Curve, INode::Type::Curve>;
    using KeymapNode = PoolObjectNode<amuse::KeymapId, amuse::Keymap, INode::Type::Keymap>;
    using LayersNode = PoolObjectNode<amuse::LayersId, std::vector<amuse::LayerMapping>, INode::Type::Layer>;

    std::shared_ptr<RootNode> m_root;

    bool m_needsReset = false;
    void _resetModelData();

public:
    explicit ProjectModel(const QString& path, QObject* parent = Q_NULLPTR);

    bool clearProjectData();
    bool openGroupData(const QString& groupName, UIMessenger& messenger);
    bool importGroupData(const QString& groupName, const amuse::AudioGroupData& data,
                         ImportMode mode, UIMessenger& messenger);
    bool saveToFile(UIMessenger& messenger);

    void ensureModelData();

    QModelIndex proxyCreateIndex(int arow, int acolumn, void *adata) const;
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const;
    QModelIndex index(INode* node) const;
    QModelIndex parent(const QModelIndex& child) const;
    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    int columnCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
    Qt::ItemFlags flags(const QModelIndex& index) const;
    INode* node(const QModelIndex& index) const;
    GroupNode* getGroupNode(INode* node) const;
    bool canEdit(const QModelIndex& index) const;
    void _undoDel(const QModelIndex& index, std::shared_ptr<ProjectModel::INode>&& node);
    std::shared_ptr<ProjectModel::INode> _redoDel(const QModelIndex& index);
    void del(const QModelIndex& index);

    QString path() const { return m_dir.path(); }
    NullItemProxyModel* getNullProxy() { return &m_nullProxy; }
};


#endif //AMUSE_PROJECT_MODEL_HPP
