#pragma once

#include <QAbstractItemModel>
#include <QIdentityProxyModel>
#include <QSortFilterProxyModel>
#include <QDir>
#include <QIcon>
#include <map>
#include "Common.hpp"
#include "NewSoundMacroDialog.hpp"
#include "amuse/AudioGroup.hpp"
#include "amuse/AudioGroupData.hpp"
#include "amuse/AudioGroupProject.hpp"
#include "amuse/AudioGroupPool.hpp"
#include "amuse/AudioGroupSampleDirectory.hpp"

class ProjectModel;
class EditorUndoCommand;

enum AmuseItemEditFlags {
  AmuseItemNone = 0,
  AmuseItemCut = 1,
  AmuseItemCopy = 2,
  AmuseItemPaste = 4,
  AmuseItemDelete = 8,
  AmuseItemNoCut = (AmuseItemCopy | AmuseItemPaste | AmuseItemDelete),
  AmuseItemAll = (AmuseItemCut | AmuseItemCopy | AmuseItemPaste | AmuseItemDelete)
};

class OutlineFilterProxyModel : public QSortFilterProxyModel {
  Q_OBJECT
  QRegExp m_usageKey;

public:
  explicit OutlineFilterProxyModel(ProjectModel* source);
public slots:
  void setFilterRegExp(const QString& pattern);
  bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const;
};

class NullItemProxyModel : public QIdentityProxyModel {
  Q_OBJECT
public:
  explicit NullItemProxyModel(ProjectModel* source);
  QModelIndex mapFromSource(const QModelIndex& sourceIndex) const;
  QModelIndex mapToSource(const QModelIndex& proxyIndex) const;
  int rowCount(const QModelIndex& parent) const;
  QModelIndex index(int row, int column, const QModelIndex& parent) const;
  QVariant data(const QModelIndex& proxyIndex, int role) const;
};

class PageObjectProxyModel : public QIdentityProxyModel {
  Q_OBJECT
public:
  explicit PageObjectProxyModel(ProjectModel* source);
  QModelIndex mapFromSource(const QModelIndex& sourceIndex) const;
  QModelIndex mapToSource(const QModelIndex& proxyIndex) const;
  QModelIndex parent(const QModelIndex& child) const;
  int rowCount(const QModelIndex& parent) const;
  QModelIndex index(int row, int column, const QModelIndex& parent) const;
  QVariant data(const QModelIndex& proxyIndex, int role) const;
  Qt::ItemFlags flags(const QModelIndex& proxyIndex) const;
};

class ProjectModel : public QAbstractItemModel {
  Q_OBJECT
public:
  enum class ImportMode { Original, WAVs, Both };

  struct NameUndoRegistry {
    std::unordered_map<amuse::SongId, std::string> m_songIDs;
    std::unordered_map<amuse::SFXId, std::string> m_sfxIDs;
    void registerSongName(amuse::SongId id) const;
    void unregisterSongName(amuse::SongId id);
    void registerSFXName(amuse::SongId id) const;
    void unregisterSFXName(amuse::SongId id);
    void clear() {
      m_songIDs.clear();
      m_sfxIDs.clear();
    }
  };

private:
  QDir m_dir;
  OutlineFilterProxyModel m_outlineProxy;
  NullItemProxyModel m_nullProxy;
  PageObjectProxyModel m_pageObjectProxy;

  amuse::ProjectDatabase m_projectDatabase;
  std::unordered_map<QString, std::unique_ptr<amuse::AudioGroupDatabase>> m_groups;

  struct Song {
    QString m_path;
    int m_refCount = 0;
  };
  std::unordered_map<amuse::SongId, Song> m_midiFiles;

public:
  class INode : public amuse::IObj {
    friend class ProjectModel;
    virtual void _sortChildren();

  public:
    enum class Type {
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
    QString m_name;
    INode* m_parent = nullptr;
    int m_row = -1;
    std::vector<amuse::IObjToken<INode>> m_children;
    amuse::IObjToken<INode> m_nullChild;

  public:
    virtual ~INode() = default;
    INode(const QString& name);
    INode(INode* parent) : m_parent(parent), m_row(0) { /* ONLY USED BY NULL NODE! */
    }

    int childCount() const { return int(m_children.size()); }
    INode* child(int row) const {
      if (row == m_children.size())
        return nullChild();
      return m_children[row].get();
    }
    INode* nullChild() const { return m_nullChild.get(); }
    INode* parent() const { return m_parent; }
    int row() const { return m_row; }

    void reindexRows(int row) {
      for (auto it = m_children.begin() + row; it != m_children.end(); ++it)
        (*it)->m_row = row++;
      m_nullChild->m_row = row;
    }

    void insertChild(amuse::ObjToken<INode> n) {
      assert(n->m_parent == nullptr && "Inserting already-parented node");
      n->m_parent = this;
      int row = hypotheticalIndex(n->name());
      m_children.insert(m_children.begin() + row, n.get());
      reindexRows(row);
    }
    amuse::ObjToken<INode> removeChild(INode* n) {
      amuse::ObjToken<INode> ret = n;
      int row = ret->row();
      assert(ret.get() == m_children.at(row).get() && "Removing non-child from node");
      m_children.erase(m_children.begin() + row);
      reindexRows(row);
      ret->m_parent = nullptr;
      ret->m_row = -1;
      return ret;
    }

    void reserve(size_t sz) { m_children.reserve(sz); }
    template <class T, class... _Args>
    T& makeChild(_Args&&... args) {
      auto tok = amuse::MakeObj<T>(std::forward<_Args>(args)...);
      insertChild(tok.get());
      return static_cast<T&>(*tok);
    }
    template <class T, class... _Args>
    T& _appendChild(_Args&&... args) {
      auto tok = amuse::MakeObj<T>(std::forward<_Args>(args)...);
      tok->m_parent = this;
      tok->m_row = m_children.size();
      m_children.push_back(tok.get());
      m_nullChild->m_row = m_children.size();
      return static_cast<T&>(*tok);
    }

    INode* findChild(const QString& name) const {
      int idx = hypotheticalIndex(name);
      if (idx >= m_children.size())
        return nullptr;
      INode* ret = m_children[idx].get();
      if (ret->name() == name)
        return ret;
      return nullptr;
    }

    bool depthTraverse(const std::function<bool(INode* node)>& func) {
      for (auto& n : m_children)
        if (!n->depthTraverse(func))
          break;
      return func(this);
    }

    bool oneLevelTraverse(const std::function<bool(INode* node)>& func) {
      for (auto& n : m_children)
        if (!func(n.get()))
          return false;
      return true;
    }

    const QString& name() const { return m_name; }
    virtual int hypotheticalIndex(const QString& name) const;

    virtual amuse::NameDB* getNameDb() const { return nullptr; }

    virtual Type type() const = 0;
    virtual QString text() const = 0;
    virtual QIcon icon() const = 0;
    virtual Qt::ItemFlags flags() const { return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable; }
    virtual AmuseItemEditFlags editFlags() const { return AmuseItemNone; }

    virtual void registerNames(const NameUndoRegistry& registry) const {}
    virtual void unregisterNames(NameUndoRegistry& registry) const {}
  };
  struct NullNode final : INode {
    NullNode(INode* parent) : INode(parent) {}

    Type type() const { return Type::Null; }
    QString text() const { return {}; }
    QIcon icon() const { return {}; }
  };
  struct RootNode final : INode {
    RootNode() : INode(QStringLiteral("<root>")) {}

    Type type() const { return Type::Root; }
    QString text() const { return {}; }
    QIcon icon() const { return {}; }
    Qt::ItemFlags flags() const { return Qt::ItemIsEnabled; }
  };
  struct CollectionNode;
  struct BasePoolObjectNode;
  struct GroupNode final : INode {
    std::unordered_map<QString, std::unique_ptr<amuse::AudioGroupDatabase>>::iterator m_it;
    GroupNode(const QString& name) : INode(name) {}
    GroupNode(std::unordered_map<QString, std::unique_ptr<amuse::AudioGroupDatabase>>::iterator it)
    : INode(it->first), m_it(it) {}

    int hypotheticalIndex(const QString& name) const;
    void _sortChildren();

    static QIcon Icon;
    Type type() const { return Type::Group; }
    QString text() const { return m_name; }
    QIcon icon() const { return Icon; }
    AmuseItemEditFlags editFlags() const { return AmuseItemNoCut; }

    CollectionNode* getCollectionOfType(Type tp) const;
    amuse::AudioGroupDatabase* getAudioGroup() const { return m_it->second.get(); }
    BasePoolObjectNode* pageObjectNodeOfId(amuse::ObjectId id) const;
  };
  struct SongGroupNode final : INode {
    amuse::GroupId m_id;
    amuse::ObjToken<amuse::SongGroupIndex> m_index;
    SongGroupNode(const QString& name, amuse::ObjToken<amuse::SongGroupIndex> index) : INode(name), m_index(index) {}
    SongGroupNode(amuse::GroupId id, amuse::ObjToken<amuse::SongGroupIndex> index)
    : INode(amuse::GroupId::CurNameDB->resolveNameFromId(id).data()), m_id(id), m_index(index) {}

    static QIcon Icon;
    Type type() const { return Type::SongGroup; }
    QString text() const { return m_name; }
    QIcon icon() const { return Icon; }
    AmuseItemEditFlags editFlags() const { return AmuseItemAll; }

    amuse::NameDB* getNameDb() const { return amuse::GroupId::CurNameDB; }

    void registerNames(const NameUndoRegistry& registry) const {
      amuse::GroupId::CurNameDB->registerPair(text().toUtf8().data(), m_id);
      for (auto& p : m_index->m_midiSetups)
        registry.registerSongName(p.first);
    }
    void unregisterNames(NameUndoRegistry& registry) const {
      amuse::GroupId::CurNameDB->remove(m_id);
      for (auto& p : m_index->m_midiSetups)
        registry.unregisterSongName(p.first);
    }
  };
  struct SoundGroupNode final : INode {
    amuse::GroupId m_id;
    amuse::ObjToken<amuse::SFXGroupIndex> m_index;
    SoundGroupNode(const QString& name, amuse::ObjToken<amuse::SFXGroupIndex> index) : INode(name), m_index(index) {}
    SoundGroupNode(amuse::GroupId id, amuse::ObjToken<amuse::SFXGroupIndex> index)
    : INode(amuse::GroupId::CurNameDB->resolveNameFromId(id).data()), m_id(id), m_index(index) {}

    static QIcon Icon;
    Type type() const { return Type::SoundGroup; }
    QString text() const { return m_name; }
    QIcon icon() const { return Icon; }
    AmuseItemEditFlags editFlags() const { return AmuseItemAll; }

    amuse::NameDB* getNameDb() const { return amuse::GroupId::CurNameDB; }

    void registerNames(const NameUndoRegistry& registry) const {
      amuse::GroupId::CurNameDB->registerPair(text().toUtf8().data(), m_id);
      for (auto& p : m_index->m_sfxEntries)
        registry.registerSFXName(p.first);
    }
    void unregisterNames(NameUndoRegistry& registry) const {
      amuse::GroupId::CurNameDB->remove(m_id);
      for (auto& p : m_index->m_sfxEntries)
        registry.unregisterSFXName(p.first);
    }
  };
  struct CollectionNode final : INode {
    QIcon m_icon;
    Type m_collectionType;
    CollectionNode(const QString& name, const QIcon& icon, Type collectionType)
    : INode(name), m_icon(icon), m_collectionType(collectionType) {}

    Type type() const { return Type::Collection; }
    QString text() const { return m_name; }
    QIcon icon() const { return m_icon; }
    Qt::ItemFlags flags() const { return Qt::ItemIsEnabled; }

    Type collectionType() const { return m_collectionType; }
    int indexOfId(amuse::ObjectId id) const;
    amuse::ObjectId idOfIndex(int idx) const;
    BasePoolObjectNode* nodeOfIndex(int idx) const;
    BasePoolObjectNode* nodeOfId(amuse::ObjectId id) const;
  };
  struct BasePoolObjectNode : INode {
    amuse::ObjectId m_id;
    BasePoolObjectNode(const QString& name) : INode(name) {}
    BasePoolObjectNode(amuse::ObjectId id, const QString& name) : INode(name), m_id(id) {}
    amuse::ObjectId id() const { return m_id; }
    QString text() const { return m_name; }
    QIcon icon() const { return {}; }
  };
  template <class ID, class T, INode::Type TP>
  struct PoolObjectNode final : BasePoolObjectNode {
    amuse::ObjToken<T> m_obj;
    PoolObjectNode(const QString& name, amuse::ObjToken<T> obj) : BasePoolObjectNode(name), m_obj(obj) {}
    PoolObjectNode(ID id, amuse::ObjToken<T> obj)
    : BasePoolObjectNode(id, ID::CurNameDB->resolveNameFromId(id).data()), m_obj(obj) {}

    Type type() const { return TP; }
    AmuseItemEditFlags editFlags() const { return TP == INode::Type::Sample ? AmuseItemNoCut : AmuseItemAll; }

    void registerNames(const NameUndoRegistry& registry) const {
      ID::CurNameDB->registerPair(text().toUtf8().data(), m_id);
    }
    void unregisterNames(NameUndoRegistry& registry) const { ID::CurNameDB->remove(m_id); }
    amuse::NameDB* getNameDb() const { return ID::CurNameDB; }
  };
  using SoundMacroNode = PoolObjectNode<amuse::SoundMacroId, amuse::SoundMacro, INode::Type::SoundMacro>;
  using ADSRNode = PoolObjectNode<amuse::TableId, std::unique_ptr<amuse::ITable>, INode::Type::ADSR>;
  using CurveNode = PoolObjectNode<amuse::TableId, std::unique_ptr<amuse::ITable>, INode::Type::Curve>;
  using KeymapNode = PoolObjectNode<amuse::KeymapId, std::array<amuse::Keymap, 128>, INode::Type::Keymap>;
  using LayersNode = PoolObjectNode<amuse::LayersId, std::vector<amuse::LayerMapping>, INode::Type::Layer>;
  using SampleNode = PoolObjectNode<amuse::SampleId, amuse::SampleEntry, INode::Type::Sample>;

  amuse::ObjToken<RootNode> m_root;

  bool m_needsReset = false;
  void _buildGroupNodeCollections(GroupNode& gn);
  void _buildGroupNode(GroupNode& gn, amuse::AudioGroup& group);
  void _resetModelData();
  void _resetSongRefCount();
  QString MakeDedupedSubprojectName(const QString& origName);
  static QString MakeDedupedName(const QString& origName, amuse::NameDB* db);

public:
  explicit ProjectModel(const QString& path, QObject* parent = Q_NULLPTR);

  bool clearProjectData();
  bool openGroupData(const QString& groupName, UIMessenger& messenger);
  void openSongsData();
  void importSongsData(const QString& path);
  bool reloadSampleData(const QString& groupName, UIMessenger& messenger);
  bool importGroupData(const QString& groupName, const amuse::AudioGroupData& data, ImportMode mode,
                       UIMessenger& messenger);
  void saveSongsIndex();
  bool saveToFile(UIMessenger& messenger);
  QStringList getGroupList() const;
  bool exportGroup(const QString& path, const QString& groupName, UIMessenger& messenger) const;
  bool importHeader(const QString& path, const QString& groupName, UIMessenger& messenger) const;
  bool exportHeader(const QString& path, const QString& groupName, bool& yesToAll, UIMessenger& messenger) const;

  void updateNodeNames();
  bool ensureModelData();

  QModelIndex proxyCreateIndex(int arow, int acolumn, void* adata) const;
  QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const;
  QModelIndex index(INode* node) const;
  QModelIndex parent(const QModelIndex& child) const;
  int rowCount(const QModelIndex& parent = QModelIndex()) const;
  int columnCount(const QModelIndex& parent = QModelIndex()) const;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
  bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole);
  Qt::ItemFlags flags(const QModelIndex& index) const;
  INode* node(const QModelIndex& index) const;
  GroupNode* getGroupNode(INode* node) const;
  AmuseItemEditFlags editFlags(const QModelIndex& index) const;
  RootNode* rootNode() const { return m_root.get(); }

  void _postAddNode(INode* n, const NameUndoRegistry& registry);
  void _preDelNode(INode* n, NameUndoRegistry& registry);
  void _addNode(GroupNode* node, std::unique_ptr<amuse::AudioGroupDatabase>&& data, const NameUndoRegistry& registry);
  std::unique_ptr<amuse::AudioGroupDatabase> _delNode(GroupNode* node, NameUndoRegistry& registry);
  GroupNode* newSubproject(const QString& name);
  template <class NT, class T>
  void _addGroupNode(NT* node, GroupNode* parent, const NameUndoRegistry& registry, T& container);
  template <class NT, class T>
  void _delGroupNode(NT* node, GroupNode* parent, NameUndoRegistry& registry, T& container);
  void _addNode(SoundGroupNode* node, GroupNode* parent, const NameUndoRegistry& registry);
  void _delNode(SoundGroupNode* node, GroupNode* parent, NameUndoRegistry& registry);
  SoundGroupNode* newSoundGroup(GroupNode* group, const QString& name);
  void _addNode(SongGroupNode* node, GroupNode* parent, const NameUndoRegistry& registry);
  void _delNode(SongGroupNode* node, GroupNode* parent, NameUndoRegistry& registry);
  SongGroupNode* newSongGroup(GroupNode* group, const QString& name);
  template <class NT, class T>
  void _addPoolNode(NT* node, GroupNode* parent, const NameUndoRegistry& registry, T& container);
  template <class NT, class T>
  void _delPoolNode(NT* node, GroupNode* parent, NameUndoRegistry& registry, T& container);
  void _addNode(SoundMacroNode* node, GroupNode* parent, const NameUndoRegistry& registry);
  void _delNode(SoundMacroNode* node, GroupNode* parent, NameUndoRegistry& registry);
  SoundMacroNode* newSoundMacro(GroupNode* group, const QString& name, const SoundMacroTemplateEntry* templ = nullptr);
  void _addNode(ADSRNode* node, GroupNode* parent, const NameUndoRegistry& registry);
  void _delNode(ADSRNode* node, GroupNode* parent, NameUndoRegistry& registry);
  ADSRNode* newADSR(GroupNode* group, const QString& name);
  void _addNode(CurveNode* node, GroupNode* parent, const NameUndoRegistry& registry);
  void _delNode(CurveNode* node, GroupNode* parent, NameUndoRegistry& registry);
  CurveNode* newCurve(GroupNode* group, const QString& name);
  void _addNode(KeymapNode* node, GroupNode* parent, const NameUndoRegistry& registry);
  void _delNode(KeymapNode* node, GroupNode* parent, NameUndoRegistry& registry);
  KeymapNode* newKeymap(GroupNode* group, const QString& name);
  void _addNode(LayersNode* node, GroupNode* parent, const NameUndoRegistry& registry);
  void _delNode(LayersNode* node, GroupNode* parent, NameUndoRegistry& registry);
  LayersNode* newLayers(GroupNode* group, const QString& name);
  void _renameNode(INode* node, const QString& name);

  template <class NT>
  EditorUndoCommand* readMimeYAML(athena::io::YAMLDocReader& r, const QString& name, GroupNode* gn);
  template <class NT>
  void loadMimeData(const QMimeData* data, const QString& mimeType, GroupNode* gn);

  QStringList mimeTypes() const;
  QMimeData* mimeData(const QModelIndexList& indexes) const;
  bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent);

  void cut(const QModelIndex& index);
  void copy(const QModelIndex& index);
  void paste(const QModelIndex& index);
  QModelIndex duplicate(const QModelIndex& index);
  void del(const QModelIndex& index);

  const QDir& dir() const { return m_dir; }
  QString path() const { return m_dir.path(); }
  OutlineFilterProxyModel* getOutlineProxy() { return &m_outlineProxy; }
  NullItemProxyModel* getNullProxy() { return &m_nullProxy; }
  PageObjectProxyModel* getPageObjectProxy() { return &m_pageObjectProxy; }

  GroupNode* getGroupOfSfx(amuse::SFXId id) const;
  QString getMIDIPathOfSong(amuse::SongId id) const;
  void setMIDIPathOfSong(amuse::SongId id, const QString& path);
  std::pair<amuse::SongId, std::string> bootstrapSongId();
  void allocateSongId(amuse::SongId id, std::string_view name);
  void deallocateSongId(amuse::SongId oldId);
  amuse::SongId exchangeSongId(amuse::SongId oldId, std::string_view newName);

  void setIdDatabases(INode* context) const;
};
