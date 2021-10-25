#include "amuse/amuse.hpp"
#include "athena/FileReader.hpp"
#include "athena/DNAYaml.hpp"
#include "logvisor/logvisor.hpp"
#include <cstdio>
#include <cstring>

#if _WIN32
#include <nowide/args.hpp>
#endif

static logvisor::Module Log("amuseconv");

enum ConvType { ConvN64, ConvGCN, ConvPC };

static void ReportConvType(ConvType tp) {
  switch (tp) {
  case ConvN64:
    Log.report(logvisor::Info, FMT_STRING("using N64 format"));
    break;
  case ConvPC:
    Log.report(logvisor::Info, FMT_STRING("using PC format"));
    break;
  case ConvGCN:
  default:
    Log.report(logvisor::Info, FMT_STRING("using GameCube format"));
    break;
  }
}

static bool BuildAudioGroup(std::string_view groupBase, std::string_view targetPath) { return true; }

static bool ExtractAudioGroup(std::string_view inPath, std::string_view targetPath) {
  amuse::ContainerRegistry::Type type;
  auto groups = amuse::ContainerRegistry::LoadContainer(inPath.data(), type);

  if (groups.size()) {
    Log.report(logvisor::Info, FMT_STRING("Found '{}'"), amuse::ContainerRegistry::TypeToName(type));

    amuse::Mkdir(targetPath.data(), 0755);
    Log.report(logvisor::Info, FMT_STRING("Established directory at {}"), targetPath);

    for (auto& group : groups) {
      Log.report(logvisor::Info, FMT_STRING("Extracting {}"), group.first);
    }
  }

  auto songs = amuse::ContainerRegistry::LoadSongs(inPath.data());
  std::string songsDir = std::string(targetPath) + "/midifiles";
  bool madeDir = false;
  for (auto& pair : songs) {
    if (!madeDir) {
      amuse::Mkdir(targetPath.data(), 0755);
      amuse::Mkdir(songsDir.c_str(), 0755);
      madeDir = true;
    }

    std::string songPath = songsDir + '/' + pair.first + ".mid";
    FILE* fp = amuse::FOpen(songPath.c_str(), "wb");
    if (fp) {
      Log.report(logvisor::Info, FMT_STRING("Extracting {}"), pair.first);
      int extractedVersion;
      bool isBig;
      std::vector<uint8_t> mid = amuse::SongConverter::SongToMIDI(pair.second.m_data.get(), extractedVersion, isBig);
      fwrite(mid.data(), 1, mid.size(), fp);
      fclose(fp);
    }
  }

  return true;
}

static bool BuildSNG(std::string_view inPath, std::string_view targetPath, int version, bool big) {
  FILE* fp = amuse::FOpen(inPath.data(), "rb");
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

  fp = amuse::FOpen(targetPath.data(), "wb");
  fwrite(out.data(), 1, out.size(), fp);
  fclose(fp);

  return true;
}

static bool ExtractSNG(std::string_view inPath, std::string_view targetPath) {
  FILE* fp = amuse::FOpen(inPath.data(), "rb");
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

  fp = amuse::FOpen(targetPath.data(), "wb");
  fwrite(out.data(), 1, out.size(), fp);
  fclose(fp);

  return true;
}

int main(int argc, char** argv) {
#if _WIN32
  nowide::args _(argc, argv);
#endif

  logvisor::RegisterConsoleLogger();

  if (argc < 3) {
    fmt::print(FMT_STRING("Usage: amuseconv <in-file> <out-file> [n64|pc|gcn]\n"));
    return 0;
  }

  ConvType type = ConvGCN;
  if (argc >= 4) {
    if (!amuse::CompareCaseInsensitive(argv[3], "n64"))
      type = ConvN64;
    else if (!amuse::CompareCaseInsensitive(argv[3], "gcn"))
      type = ConvGCN;
    else if (!amuse::CompareCaseInsensitive(argv[3], "pc"))
      type = ConvPC;
    else {
      Log.report(logvisor::Error, FMT_STRING("unrecognized format: {}"), argv[3]);
      return 1;
    }
  }

  bool good = false;
  FILE* fin = amuse::FOpen(argv[1], "rb");
  if (fin) {
    fclose(fin);
    std::string barePath(argv[1]);
    size_t dotPos = barePath.rfind('.');
    const char* dot = barePath.c_str() + dotPos;
    if (dotPos != std::string::npos) {
      if (!amuse::CompareCaseInsensitive(dot, ".mid") ||
          !amuse::CompareCaseInsensitive(dot, ".midi")) {
        ReportConvType(type);
        good = BuildSNG(barePath, argv[2], 1, true);
      } else if (!amuse::CompareCaseInsensitive(dot, ".son") ||
                 !amuse::CompareCaseInsensitive(dot, ".sng")) {
        good = ExtractSNG(argv[1], argv[2]);
      } else {
        good = ExtractAudioGroup(argv[1], argv[2]);
      }
    }
  } else {
    amuse::Sstat theStat;
    if (!amuse::Stat(argv[1], &theStat) && S_ISDIR(theStat.st_mode)) {
      std::string projectPath(argv[1]);
      projectPath += "/project.yaml";
      fin = amuse::FOpen(projectPath.c_str(), "rb");
      if (fin) {
        fclose(fin);
        ReportConvType(type);
        good = BuildAudioGroup(argv[1], argv[2]);
      }
    }
  }

  if (!good) {
    Log.report(logvisor::Error, FMT_STRING("unable to convert {} to {}"), argv[1], argv[2]);
    return 1;
  }

  return 0;
}
