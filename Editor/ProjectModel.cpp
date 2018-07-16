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

bool ProjectModel::importGroupData(const QString& groupName, amuse::IntrusiveAudioGroupData&& data)
{
    setIdDatabases();

    ProjectGroup& grp = m_groups.insert(std::make_pair(groupName, std::move(data))).first->second;
    grp.setIdDatabases();
    amuse::AudioGroupProject::BootstrapObjectIDs(grp.m_data);

    return true;
}

bool ProjectModel::extractSamples(ImportMode mode, QWidget* parent)
{
    setIdDatabases();

    if (!MkPath(m_dir.path(), parent))
        return false;

    for (auto& g : m_groups)
    {
        g.second.setIdDatabases();

        QDir dir(QFileInfo(m_dir, g.first).filePath());
        if (!MkPath(dir.path(), parent))
            return false;

        amuse::SystemString sysDir = QStringToSysString(dir.path());
        switch (mode)
        {
        case ImportMode::Original:
            g.second.m_sdir.extractAllCompressed(sysDir, g.second.m_data.getSamp());
            break;
        case ImportMode::WAVs:
            g.second.m_sdir.extractAllWAV(sysDir, g.second.m_data.getSamp());
            break;
        case ImportMode::Both:
            g.second.m_sdir.extractAllCompressed(sysDir, g.second.m_data.getSamp());
            g.second.m_sdir.extractAllWAV(sysDir, g.second.m_data.getSamp());
            break;
        default:
            break;
        }
    }

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
        amuse::SystemString groupPath = QStringToSysString(dir.path());
        g.second.m_proj.toYAML(groupPath);
        g.second.m_pool.toYAML(groupPath);
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
