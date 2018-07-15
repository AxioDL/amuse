#ifndef AMUSE_PROJECT_MODEL_HPP
#define AMUSE_PROJECT_MODEL_HPP

#include <QAbstractItemModel>
#include <QDir>
#include <map>
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
    struct ProjectGroup
    {
        amuse::IntrusiveAudioGroupData m_data;
        amuse::AudioGroupProject m_proj;
        amuse::AudioGroupPool m_pool;
        amuse::AudioGroupSampleDirectory m_sdir;
        amuse::NameDB m_soundMacroDb;
        amuse::NameDB m_sampleDb;
        amuse::NameDB m_tableDb;
        amuse::NameDB m_keymapDb;
        amuse::NameDB m_layersDb;

        explicit ProjectGroup(amuse::IntrusiveAudioGroupData&& data);
        void setIdDatabases()
        {
            amuse::SoundMacroId::CurNameDB = &m_soundMacroDb;
            amuse::SampleId::CurNameDB = &m_sampleDb;
            amuse::TableId::CurNameDB = &m_tableDb;
            amuse::KeymapId::CurNameDB = &m_keymapDb;
            amuse::LayersId::CurNameDB = &m_layersDb;
        }
    };

private:
    QDir m_dir;

    amuse::NameDB m_songDb;
    amuse::NameDB m_sfxDb;
    std::map<QString, ProjectGroup> m_groups;

    void setIdDatabases()
    {
        amuse::SongId::CurNameDB = &m_songDb;
        amuse::SFXId::CurNameDB = &m_sfxDb;
    }

public:
    explicit ProjectModel(const QString& path, QObject* parent = Q_NULLPTR);

    bool importGroupData(const QString& groupName, amuse::IntrusiveAudioGroupData&& data, ImportMode mode);
    bool saveToFile(QWidget* parent);

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex& child) const;
    int rowCount(const QModelIndex& parent = QModelIndex()) const;
    int columnCount(const QModelIndex& parent = QModelIndex()) const;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const;

    QString path() const { return m_dir.path(); }
    bool canDelete() const;

public slots:
    void del();

signals:
    void canDeleteChanged(bool canDelete);
};


#endif //AMUSE_PROJECT_MODEL_HPP
