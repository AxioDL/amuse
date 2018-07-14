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

bool ProjectModel::importGroupData(const QString& groupName,
                                   amuse::IntrusiveAudioGroupData&& data,
                                   ImportMode mode, QWidget* parent)
{
    amuse::SongId::CurNameDB = &m_songDb;
    amuse::SongId::CurNameDB = &m_sfxDb;

    ProjectGroup& grp = m_groups.insert(std::make_pair(groupName, std::move(data))).first->second;

    for (const auto& p : grp.m_proj.songGroups())
    {
        for (const auto& song : p.second.m_midiSetups)
        {
            char name[16];
            snprintf(name, 16, "song%d", song.first.id);
            m_songDb.registerPair(name, song.first);
        }
    }

    for (const auto& p : grp.m_proj.sfxGroups())
    {
        for (const auto& sfx : p.second.m_sfxEntries)
        {
            char name[16];
            snprintf(name, 16, "sfx%d", sfx.first.id);
            m_sfxDb.registerPair(name, sfx.first);
        }
    }

    return true;
}

bool ProjectModel::saveToFile(QWidget* parent)
{
    amuse::SongId::CurNameDB = &m_songDb;
    amuse::SongId::CurNameDB = &m_sfxDb;

    if (!MkPath(m_dir.path(), parent))
        return false;

    for (auto& g : m_groups)
    {
        athena::io::YAMLDocWriter w("amuse::Group");

        QDir dir(QFileInfo(m_dir, g.first).filePath());
        if (!MkPath(dir.path(), parent))
            return false;

        if (auto __v = w.enterSubVector("songGroups"))
        {
            for (const auto& p : g.second.m_proj.songGroups())
            {
                if (auto __r = w.enterSubRecord(nullptr))
                {
                    if (auto __v2 = w.enterSubRecord("normPages"))
                    {
                        for (const auto& pg : p.second.m_normPages)
                        {
                            char name[16];
                            snprintf(name, 16, "%d", pg.first);
                            if (auto __r2 = w.enterSubRecord(name))
                                pg.second.toDNA<athena::Big>(pg.first).write(w);
                        }
                    }
                    if (auto __v2 = w.enterSubRecord("drumPages"))
                    {
                        for (const auto& pg : p.second.m_drumPages)
                        {
                            char name[16];
                            snprintf(name, 16, "%d", pg.first);
                            if (auto __r2 = w.enterSubRecord(name))
                                pg.second.toDNA<athena::Big>(pg.first).write(w);
                        }
                    }
                    if (auto __v2 = w.enterSubRecord("songs"))
                    {
                        for (const auto& song : p.second.m_midiSetups)
                        {
                            if (auto __v3 = w.enterSubVector(m_songDb.resolveNameFromId(song.first).data()))
                                for (int i = 0; i < 16; ++i)
                                    if (auto __r2 = w.enterSubRecord(nullptr))
                                        song.second[i].write(w);
                        }
                    }
                }
            }
        }

        if (auto __v = w.enterSubVector("sfxGroups"))
        {
            for (const auto& p : g.second.m_proj.sfxGroups())
            {
                if (auto __r = w.enterSubRecord(nullptr))
                {
                    for (const auto& sfx : p.second.m_sfxEntries)
                    {
                        if (auto __r2 = w.enterSubRecord(m_sfxDb.resolveNameFromId(sfx.first).data()))
                            sfx.second.toDNA<athena::Big>(sfx.first).write(w);
                    }
                }
            }
        }

        athena::io::FileWriter fo(QStringToSysString(dir.filePath("project.yaml")));
        w.finish(&fo);
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
