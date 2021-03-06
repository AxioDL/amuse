#include "amuse/amuse.hpp"
#include "athena/FileReader.hpp"
#include "athena/DNAYaml.hpp"
#include "logvisor/logvisor.hpp"
#include <cstdio>
#include <cstring>

static logvisor::Module Log("amuseconv");

enum ConvType { ConvN64, ConvGCN, ConvPC };

static void ReportConvType(ConvType tp) {
  switch (tp) {
  case ConvN64:
    Log.report(logvisor::Info, FMT_STRING(_SYS_STR("using N64 format")));
    break;
  case ConvPC:
    Log.report(logvisor::Info, FMT_STRING(_SYS_STR("using PC format")));
    break;
  case ConvGCN:
  default:
    Log.report(logvisor::Info, FMT_STRING(_SYS_STR("using GameCube format")));
    break;
  }
}

static bool BuildAudioGroup(amuse::SystemStringView groupBase, amuse::SystemStringView targetPath) { return true; }

static bool ExtractAudioGroup(amuse::SystemStringView inPath, amuse::SystemStringView targetPath) {
  amuse::ContainerRegistry::Type type;
  auto groups = amuse::ContainerRegistry::LoadContainer(inPath.data(), type);

  if (groups.size()) {
    Log.report(logvisor::Info, FMT_STRING(_SYS_STR("Found '{}'")), amuse::ContainerRegistry::TypeToName(type));

    amuse::Mkdir(targetPath.data(), 0755);
    Log.report(logvisor::Info, FMT_STRING(_SYS_STR("Established directory at {}")), targetPath);

    for (auto& group : groups) {
      Log.report(logvisor::Info, FMT_STRING(_SYS_STR("Extracting {}")), group.first);
    }
  }

  auto songs = amuse::ContainerRegistry::LoadSongs(inPath.data());
  amuse::SystemString songsDir = amuse::SystemString(targetPath) + _SYS_STR("/midifiles");
  bool madeDir = false;
  for (auto& pair : songs) {
    if (!madeDir) {
      amuse::Mkdir(targetPath.data(), 0755);
      amuse::Mkdir(songsDir.c_str(), 0755);
      madeDir = true;
    }

    amuse::SystemString songPath = songsDir + _SYS_STR('/') + pair.first + _SYS_STR(".mid");
    FILE* fp = amuse::FOpen(songPath.c_str(), _SYS_STR("wb"));
    if (fp) {
      Log.report(logvisor::Info, FMT_STRING(_SYS_STR("Extracting {}")), pair.first);
      int extractedVersion;
      bool isBig;
      std::vector<uint8_t> mid = amuse::SongConverter::SongToMIDI(pair.second.m_data.get(), extractedVersion, isBig);
      fwrite(mid.data(), 1, mid.size(), fp);
      fclose(fp);
    }
  }

  return true;
}

static bool BuildSNG(amuse::SystemStringView inPath, amuse::SystemStringView targetPath, int version, bool big) {
  FILE* fp = amuse::FOpen(inPath.data(), _SYS_STR("rb"));
  if (!fp)
    return false;

  fseek(fp, 0, SEEK_END);
  long sz = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  std::vector<uint8_t> data(sz, 0);
  fread(&data[0], 1, sz, fp);
  fclose(fp);

  std::vector<uint8_t> out = amuse::SongConverter::MIDIToSong(data, version, big);
  if (out.empty())
    return false;

  fp = amuse::FOpen(targetPath.data(), _SYS_STR("wb"));
  fwrite(out.data(), 1, out.size(), fp);
  fclose(fp);

  return true;
}

static bool ExtractSNG(amuse::SystemStringView inPath, amuse::SystemStringView targetPath) {
  FILE* fp = amuse::FOpen(inPath.data(), _SYS_STR("rb"));
  if (!fp)
    return false;

  fseek(fp, 0, SEEK_END);
  long sz = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  std::vector<uint8_t> data(sz, 0);
  fread(&data[0], 1, sz, fp);
  fclose(fp);

  int extractedVersion;
  bool isBig;
  std::vector<uint8_t> out = amuse::SongConverter::SongToMIDI(data.data(), extractedVersion, isBig);
  if (out.empty())
    return false;

  fp = amuse::FOpen(targetPath.data(), _SYS_STR("wb"));
  fwrite(out.data(), 1, out.size(), fp);
  fclose(fp);

  return true;
}

#if _WIN32
int wmain(int argc, const amuse::SystemChar** argv)
#else
int main(int argc, const amuse::SystemChar** argv)
#endif
{
  logvisor::RegisterConsoleLogger();

  if (argc < 3) {
    fmt::print(FMT_STRING("Usage: amuseconv <in-file> <out-file> [n64|pc|gcn]\n"));
    return 0;
  }

  ConvType type = ConvGCN;
  if (argc >= 4) {
    if (!amuse::CompareCaseInsensitive(argv[3], _SYS_STR("n64")))
      type = ConvN64;
    else if (!amuse::CompareCaseInsensitive(argv[3], _SYS_STR("gcn")))
      type = ConvGCN;
    else if (!amuse::CompareCaseInsensitive(argv[3], _SYS_STR("pc")))
      type = ConvPC;
    else {
      Log.report(logvisor::Error, FMT_STRING(_SYS_STR("unrecognized format: {}")), argv[3]);
      return 1;
    }
  }

  bool good = false;
  FILE* fin = amuse::FOpen(argv[1], _SYS_STR("rb"));
  if (fin) {
    fclose(fin);
    amuse::SystemString barePath(argv[1]);
    size_t dotPos = barePath.rfind(_SYS_STR('.'));
    const amuse::SystemChar* dot = barePath.c_str() + dotPos;
    if (dotPos != amuse::SystemString::npos) {
      if (!amuse::CompareCaseInsensitive(dot, _SYS_STR(".mid")) ||
          !amuse::CompareCaseInsensitive(dot, _SYS_STR(".midi"))) {
        ReportConvType(type);
        good = BuildSNG(barePath, argv[2], 1, true);
      } else if (!amuse::CompareCaseInsensitive(dot, _SYS_STR(".son")) ||
                 !amuse::CompareCaseInsensitive(dot, _SYS_STR(".sng"))) {
        good = ExtractSNG(argv[1], argv[2]);
      } else {
        good = ExtractAudioGroup(argv[1], argv[2]);
      }
    }
  } else {
    amuse::Sstat theStat;
    if (!amuse::Stat(argv[1], &theStat) && S_ISDIR(theStat.st_mode)) {
      amuse::SystemString projectPath(argv[1]);
      projectPath += _SYS_STR("/project.yaml");
      fin = amuse::FOpen(projectPath.c_str(), _SYS_STR("rb"));
      if (fin) {
        fclose(fin);
        ReportConvType(type);
        good = BuildAudioGroup(argv[1], argv[2]);
      }
    }
  }

  if (!good) {
    Log.report(logvisor::Error, FMT_STRING(_SYS_STR("unable to convert {} to {}")), argv[1], argv[2]);
    return 1;
  }

  return 0;
}
