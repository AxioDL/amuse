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
    class INode : public amuse::IObj
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
            Layer,
            Sample
        };
    protected:
        INode* m_parent;
        std::vector<amuse::IObjToken<INode>> m_children;
        amuse::IObjToken<INode> m_nullChild;
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

        void insertChild(int row, amuse::ObjToken<INode> n)
        {
            m_children.insert(m_children.begin() + row, n.get());
            reindexRows(row);
        }
        amuse::ObjToken<INode> removeChild(int row)
        {
            amuse::ObjToken<INode> ret = m_children[row].get();
            m_children.erase(m_children.begin() + row);
            reindexRows(row);
            return ret;
        }

        void reserve(size_t sz) { m_children.reserve(sz); }
        template<class T, class... _Args>
        T& makeChild(_Args&&... args)
        {
            auto tok = amuse::MakeObj<T>(this, m_children.size(), std::forward<_Args>(args)...);
            m_children.push_back(tok.get());
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
    };
    struct SongGroupNode : INode
    {
        amuse::GroupId m_id;
        QString m_name;
        amuse::ObjToken<amuse::SongGroupIndex> m_index;
        SongGroupNode(INode* parent, int row, amuse::GroupId id, amuse::ObjToken<amuse::SongGroupIndex> index)
        : INode(parent, row), m_id(id), m_name(amuse::GroupId::CurNameDB->resolveNameFromId(id).data()), m_index(index) {}

        static QIcon Icon;
        Type type() const { return Type::SongGroup; }
        QString text() const { return m_name; }
        QIcon icon() const { return Icon; }
    };
    struct SoundGroupNode : INode
    {
        amuse::GroupId m_id;
        QString m_name;
        amuse::ObjToken<amuse::SFXGroupIndex> m_index;
        SoundGroupNode(INode* parent, int row, amuse::GroupId id, amuse::ObjToken<amuse::SFXGroupIndex> index)
        : INode(parent, row), m_id(id), m_name(amuse::GroupId::CurNameDB->resolveNameFromId(id).data()), m_index(index) {}

        static QIcon Icon;
        Type type() const { return Type::SoundGroup; }
        QString text() const { return m_name; }
        QIcon icon() const { return Icon; }
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
    };
    struct BasePoolObjectNode : INode
    {
        amuse::ObjectId m_id;
        QString m_name;
        BasePoolObjectNode(INode* parent, int row, amuse::ObjectId id, const QString& name)
        : INode(parent, row), m_id(id), m_name(name) {}
        amuse::ObjectId id() const { return m_id; }
        QString text() const { return m_name; }
        QIcon icon() const { return {}; }
    };
    template <class ID, class T, INode::Type TP>
    struct PoolObjectNode : BasePoolObjectNode
    {
        amuse::ObjToken<T> m_obj;
        PoolObjectNode(INode* parent, int row, ID id, amuse::ObjToken<T> obj)
        : BasePoolObjectNode(parent, row, id, ID::CurNameDB->resolveNameFromId(id).data()), m_obj(obj) {}

        Type type() const { return TP; }
    };
    using SoundMacroNode = PoolObjectNode<amuse::SoundMacroId, amuse::SoundMacro, INode::Type::SoundMacro>;
    using ADSRNode = PoolObjectNode<amuse::TableId, std::unique_ptr<amuse::ITable>, INode::Type::ADSR>;
    using CurveNode = PoolObjectNode<amuse::TableId, std::unique_ptr<amuse::ITable>, INode::Type::Curve>;
    using KeymapNode = PoolObjectNode<amuse::KeymapId, amuse::Keymap, INode::Type::Keymap>;
    using LayersNode = PoolObjectNode<amuse::LayersId, std::vector<amuse::LayerMapping>, INode::Type::Layer>;
    using SampleNode = PoolObjectNode<amuse::SampleId, amuse::SampleEntry, INode::Type::Sample>;

    amuse::ObjToken<RootNode> m_root;

    bool m_needsReset = false;
    void _resetModelData();

public:
    explicit ProjectModel(const QString& path, QObject* parent = Q_NULLPTR);

    bool clearProjectData();
    bool openGroupData(const QString& groupName, UIMessenger& messenger);
    bool reloadSampleData(const QString& groupName, UIMessenger& messenger);
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
    void _undoDel(const QModelIndex& index, amuse::ObjToken<ProjectModel::INode> node);
    amuse::ObjToken<ProjectModel::INode> _redoDel(const QModelIndex& index);
    void del(const QModelIndex& index);

    QString path() const { return m_dir.path(); }
    NullItemProxyModel* getNullProxy() { return &m_nullProxy; }
};


#endif //AMUSE_PROJECT_MODEL_HPP
