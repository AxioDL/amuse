#ifndef AMUSE_PROJECT_MODEL_HPP
#define AMUSE_PROJECT_MODEL_HPP

#include <QAbstractItemModel>
#include <QDir>
#include <QIcon>
#include <map>
#include "Common.hpp"
#include "amuse/AudioGroup.hpp"
#include "amuse/AudioGroupData.hpp"
#include "amuse/AudioGroupProject.hpp"
#include "amuse/AudioGroupPool.hpp"
#include "amuse/AudioGroupSampleDirectory.hpp"

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

    amuse::ProjectDatabase m_projectDatabase;
    std::map<QString, amuse::AudioGroupDatabase> m_groups;

public:
    class INode
    {
    public:
        enum class Type
        {
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
    private:
        INode* m_parent;
        std::vector<std::unique_ptr<INode>> m_children;
        int m_row;
    public:
        virtual ~INode() = default;
        INode(INode* parent, int row) : m_parent(parent), m_row(row) {}

        int childCount() const { return int(m_children.size()); }
        INode* child(int row) const { return m_children[row].get(); }
        INode* parent() const { return m_parent; }
        int row() const { return m_row; }

        void reserve(size_t sz) { m_children.reserve(sz); }
        template<class T, class... _Args>
        T& makeChild(_Args&&... args)
        {
            m_children.push_back(std::make_unique<T>(this, m_children.size(), std::forward<_Args>(args)...));
            return static_cast<T&>(*m_children.back());
        }

        virtual Type type() const = 0;
        virtual QString text() const = 0;
        virtual QIcon icon() const = 0;
    };
    struct RootNode : INode
    {
        RootNode() : INode(nullptr, 0) {}

        Type type() const { return Type::Root; }
        QString text() const { return {}; }
        QIcon icon() const { return {}; }
    };
    struct GroupNode : INode
    {
        std::map<QString, amuse::AudioGroupDatabase>::iterator m_it;
        GroupNode(INode* parent, int row, std::map<QString, amuse::AudioGroupDatabase>::iterator it)
        : INode(parent, row), m_it(it) {}

        static QIcon Icon;
        Type type() const { return Type::Group; }
        QString text() const { return m_it->first; }
        QIcon icon() const { return Icon; }
    };
    struct SongGroupNode : INode
    {
        amuse::GroupId m_id;
        QString m_name;
        amuse::SongGroupIndex& m_index;
        SongGroupNode(INode* parent, int row, amuse::GroupId id, amuse::SongGroupIndex& index)
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
        amuse::SFXGroupIndex& m_index;
        SoundGroupNode(INode* parent, int row, amuse::GroupId id, amuse::SFXGroupIndex& index)
        : INode(parent, row), m_id(id), m_name(amuse::GroupId::CurNameDB->resolveNameFromId(id).data()), m_index(index) {}

        static QIcon Icon;
        Type type() const { return Type::SoundGroup; }
        QString text() const { return m_name; }
        QIcon icon() const { return Icon; }
    };
    struct CollectionNode : INode
    {
        QString m_name;
        QIcon m_icon;
        CollectionNode(INode* parent, int row, const QString& name, const QIcon& icon)
        : INode(parent, row), m_name(name), m_icon(icon) {}

        Type type() const { return Type::Collection; }
        QString text() const { return m_name; }
        QIcon icon() const { return m_icon; }
    };
    template <class ID, class T, INode::Type TP>
    struct PoolObjectNode : INode
    {
        ID m_id;
        QString m_name;
        T& m_obj;
        PoolObjectNode(INode* parent, int row, ID id, T& obj)
        : INode(parent, row), m_id(id), m_name(ID::CurNameDB->resolveNameFromId(id).data()), m_obj(obj) {}

        Type type() const { return TP; }
        QString text() const { return m_name; }
        QIcon icon() const { return {}; }
    };
    using SoundMacroNode = PoolObjectNode<amuse::SoundMacroId, amuse::SoundMacro, INode::Type::SoundMacro>;
    using ADSRNode = PoolObjectNode<amuse::TableId, amuse::ITable, INode::Type::ADSR>;
    using CurveNode = PoolObjectNode<amuse::TableId, amuse::Curve, INode::Type::Curve>;
    using KeymapNode = PoolObjectNode<amuse::KeymapId, amuse::Keymap, INode::Type::Keymap>;
    using LayersNode = PoolObjectNode<amuse::LayersId, std::vector<amuse::LayerMapping>, INode::Type::Layer>;

    std::unique_ptr<RootNode> m_root;

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

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex& child) const;
    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    int columnCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
    Qt::ItemFlags flags(const QModelIndex& index) const;
    INode* node(const QModelIndex& index) const;

    QString path() const { return m_dir.path(); }
    bool canDelete() const;

public slots:
    void del();

signals:
    void canDeleteChanged(bool canDelete);
};


#endif //AMUSE_PROJECT_MODEL_HPP
