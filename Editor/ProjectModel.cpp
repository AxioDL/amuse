#include <athena/FileWriter.hpp>
#include <athena/FileReader.hpp>
#include "ProjectModel.hpp"
#include "Common.hpp"
#include "athena/YAMLDocWriter.hpp"

ProjectModel::ProjectModel(const QString& path, QObject* parent)
: QAbstractItemModel(parent), m_dir(path)
{

}

ProjectModel::ProjectGroup::ProjectGroup(amuse::IntrusiveAudioGroupData&& data)
: m_data(std::move(data)),
  m_proj(amuse::AudioGroupProject::CreateAudioGroupProject(m_data)),
  m_pool(amuse::AudioGroupPool::CreateAudioGroupPool(m_data)),
  m_sdir(amuse::AudioGroupSampleDirectory::CreateAudioGroupSampleDirectory(m_data))
{}

bool ProjectModel::importGroupData(const QString& groupName, amuse::IntrusiveAudioGroupData&& data, ImportMode mode)
{
    setIdDatabases();

    ProjectGroup& grp = m_groups.insert(std::make_pair(groupName, std::move(data))).first->second;
    grp.setIdDatabases();
    amuse::AudioGroupProject::BootstrapObjectIDs(grp.m_data);

    return true;
}

bool ProjectModel::saveToFile(QWidget* parent)
{
    setIdDatabases();

    if (!MkPath(m_dir.path(), parent))
        return false;

    for (auto& g : m_groups)
    {
        QDir dir(QFileInfo(m_dir, g.first).filePath());
        if (!MkPath(dir.path(), parent))
            return false;

        g.second.setIdDatabases();
        {
            athena::io::FileWriter fo(QStringToSysString(dir.filePath("!project.yaml")));
            g.second.m_proj.toYAML(fo);
        }
        {
            athena::io::FileWriter fo(QStringToSysString(dir.filePath("!pool.yaml")));
            g.second.m_pool.toYAML(fo);
        }
        //g.second.m_sdir.sampleEntries()
    }

    return true;
}

QModelIndex ProjectModel::index(int row, int column, const QModelIndex& parent) const
{
    return createIndex(row, column, nullptr);
}

QModelIndex ProjectModel::parent(const QModelIndex& child) const
{
    return {};
}

int ProjectModel::rowCount(const QModelIndex& parent) const
{
    return 0;
}

int ProjectModel::columnCount(const QModelIndex& parent) const
{
    return 0;
}

QVariant ProjectModel::data(const QModelIndex& index, int role) const
{
    return {};
}

bool ProjectModel::canDelete() const
{
    return false;
}

void ProjectModel::del()
{

}
