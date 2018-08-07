#ifndef AMUSE_LAYERS_EDITOR_HPP
#define AMUSE_LAYERS_EDITOR_HPP

#include "EditorWidget.hpp"
#include <QAbstractTableModel>
#include <QTableView>
#include <QAction>
#include <QToolButton>
#include <QStyledItemDelegate>
#include <QItemEditorFactory>

class SignedValueFactory : public QItemEditorFactory
{
public:
    QWidget* createEditor(int userType, QWidget *parent) const;
};

class UnsignedValueFactory : public QItemEditorFactory
{
public:
    QWidget* createEditor(int userType, QWidget *parent) const;
};

class EditorFieldProjectNode : public FieldProjectNode
{
Q_OBJECT
    bool m_deferPopupOpen = true;
public:
    explicit EditorFieldProjectNode(ProjectModel::CollectionNode* collection = Q_NULLPTR, QWidget* parent = Q_NULLPTR);
    bool shouldPopupOpen()
    {
        bool ret = m_deferPopupOpen;
        m_deferPopupOpen = false;
        return ret;
    }
};

class SoundMacroDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit SoundMacroDelegate(QObject* parent = Q_NULLPTR);
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const;
    void setEditorData(QWidget* editor, const QModelIndex& index) const;
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const;
private slots:
    void smIndexChanged();
};

class LayersModel : public QAbstractTableModel
{
    Q_OBJECT
    friend class LayersEditor;
    friend class SoundMacroDelegate;
    amuse::ObjToken<ProjectModel::LayersNode> m_node;
public:
    explicit LayersModel(QObject* parent = Q_NULLPTR);
    void loadData(ProjectModel::LayersNode* node);
    void unloadData();

    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    int columnCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole);
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    Qt::ItemFlags flags(const QModelIndex& index) const;
    Qt::DropActions supportedDropActions() const;
    Qt::DropActions supportedDragActions() const;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                      int row, int column, const QModelIndex &parent);

    bool insertRows(int row, int count, const QModelIndex& parent = QModelIndex());
    bool moveRows(const QModelIndex& sourceParent, int sourceRow, int count,
                  const QModelIndex& destinationParent, int destinationChild);
    bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex());
};

class LayersTableView : public QTableView
{
    Q_OBJECT
public:
    explicit LayersTableView(QWidget* parent = Q_NULLPTR);
    void doItemsLayout();
    void deleteSelection();
};

class LayersEditor : public EditorWidget
{
    Q_OBJECT
    LayersModel m_model;
    SoundMacroDelegate m_smDelegate;
    SignedValueFactory m_signedFactory;
    UnsignedValueFactory m_unsignedFactory;
    QStyledItemDelegate m_signedDelegate, m_unsignedDelegate;
    LayersTableView m_tableView;
    QAction m_addAction;
    QToolButton m_addButton;
    QAction m_removeAction;
    QToolButton m_removeButton;
public:
    explicit LayersEditor(QWidget* parent = Q_NULLPTR);
    bool loadData(ProjectModel::LayersNode* node);
    void unloadData();
    ProjectModel::INode* currentNode() const;
    void resizeEvent(QResizeEvent* ev);
public slots:
    void doAdd();
    void doSelectionChanged(const QItemSelection& selected);

    bool isItemEditEnabled() const;
    void itemCutAction();
    void itemCopyAction();
    void itemPasteAction();
    void itemDeleteAction();
};

#endif //AMUSE_LAYERS_EDITOR_HPP
