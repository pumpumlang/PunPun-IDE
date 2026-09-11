#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>
#include <QIcon>
#include <QString>
#include <QStringList>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("PunPun");
    QCoreApplication::setApplicationName("PunPun IDE");
    QCoreApplication::setApplicationVersion(PPIDE_VERSION);
    app.setWindowIcon(QIcon(":/branding/punpun-mark.svg"));
    app.setStyle("Fusion");

    // Without a parser every argument was treated as a path to open, so
    // `punpun-ide --version` tried to open a file called "--version".
    QCommandLineParser parser;
    parser.setApplicationDescription(
        "PunPun IDE — native editing, compiler diagnostics and a terminal for "
        "PunPun, C and C++.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(
        "path", "File or folder to open. Repeat to open several files.", "[path...]");
    parser.process(app);

    // The desktop entry passes %F, so several paths can arrive at once.
    const QStringList paths = parser.positionalArguments();
    MainWindow window(paths.value(0));
    window.show();
    for (int i = 1; i < paths.size(); ++i) {
        if (QFileInfo(paths.at(i)).isFile()) window.openFile(paths.at(i));
    }
    return app.exec();
}
