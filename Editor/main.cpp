#include <cstdint>
#include <QApplication>
#include <QStyleFactory>
#include <QTranslator>
#include "MainWindow.hpp"
#include "SongGroupEditor.hpp"
#include <QResource>
#include <QCommandLineParser>
#include <logvisor/logvisor.hpp>

using namespace std::literals;

#ifdef __APPLE__
void MacOSSetDarkAppearance();
#endif

extern "C" const uint8_t MAINICON_QT[];

static QIcon MakeAppIcon() {
  QIcon ret;

  const uint8_t* ptr = MAINICON_QT;
  for (int i = 0; i < 6; ++i) {
    uint32_t size = *reinterpret_cast<const uint32_t*>(ptr);
    ptr += 4;

    QPixmap pm;
    pm.loadFromData(ptr, size);
    ret.addPixmap(pm);
    ptr += size;
  }

  return ret;
}

MainWindow* g_MainWindow = nullptr;

int main(int argc, char* argv[]) {
  QApplication::setAttribute(Qt::AA_Use96Dpi);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0) && QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
  QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
  QApplication::setStyle(new ColoredTabBarStyle(QStyleFactory::create(QStringLiteral("Fusion"))));
  QApplication a(argc, argv);
  QApplication::setWindowIcon(MakeAppIcon());

  a.setOrganizationName(QStringLiteral("AxioDL"));
  a.setApplicationName(QStringLiteral("Amuse"));

  QPalette darkPalette;
  darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
  darkPalette.setColor(QPalette::WindowText, Qt::white);
  darkPalette.setColor(QPalette::Base, QColor(42, 42, 42));
  darkPalette.setColor(QPalette::Disabled, QPalette::Base, QColor(25, 25, 25, 53));
  darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
  darkPalette.setColor(QPalette::ToolTipBase, QColor(42, 42, 42));
  darkPalette.setColor(QPalette::ToolTipText, Qt::white);
  darkPalette.setColor(QPalette::Text, Qt::white);
  darkPalette.setColor(QPalette::Disabled, QPalette::Text, QColor(255, 255, 255, 120));
  darkPalette.setColor(QPalette::Disabled, QPalette::Light, QColor(0, 0, 0, 0));
  darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
  darkPalette.setColor(QPalette::Disabled, QPalette::Button, QColor(53, 53, 53, 53));
  darkPalette.setColor(QPalette::ButtonText, Qt::white);
  darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(255, 255, 255, 120));
  darkPalette.setColor(QPalette::BrightText, Qt::red);
  darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
  darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
  darkPalette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(42, 130, 218, 53));
  darkPalette.setColor(QPalette::HighlightedText, Qt::white);
  darkPalette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(255, 255, 255, 120));
  a.setPalette(darkPalette);

#ifdef __APPLE__
  MacOSSetDarkAppearance();
#endif

  logvisor::RegisterConsoleLogger();
  logvisor::RegisterStandardExceptions();

  Q_INIT_RESOURCE(translation_res);
  QTranslator translator;
  if (translator.load(QLocale(), QStringLiteral("lang"), QStringLiteral("_"), QStringLiteral(":/translations"))) {
    a.installTranslator(&translator);
  }

  MainWindow w;
  g_MainWindow = &w;
  w.show();

  QCommandLineParser parser;
  parser.process(a);
  QStringList args = parser.positionalArguments();
  if (!args.empty())
    w.openProject(args.back());

  return a.exec();
}
