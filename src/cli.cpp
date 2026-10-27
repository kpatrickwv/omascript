#include "cli.h"

#include "screenplay.h"

#include <QTextStream>

QString Cli::usage() {
    return QStringLiteral(
        "Usage: omascript [FILE]\n"
        "       omascript --pdf FILE [-o OUT.pdf] [--scene-numbers] [--contd] [--no-title-page]\n"
        "\n"
        "Opens FILE, a screenplay written in Markdown, or an empty document without\n"
        "one, with the formatted script beside it. Omascript is a graphical app and\n"
        "keeps running until its window is closed.\n"
        "\n"
        "With --pdf, writes FILE as a formatted screenplay PDF without opening a\n"
        "window (to FILE's name with .pdf unless -o names another).\n"
        "\n"
        "Options:\n"
        "  -h, --help         Show this message and exit\n"
        "  -v, --version      Show the version and exit\n"
        "  --pdf FILE         Export FILE to PDF and exit\n"
        "  -o, --output FILE  Where --pdf writes the PDF\n"
        "  --scene-numbers    Number scenes in both margins\n"
        "  --contd            Add (CONT'D) when a character speaks again after action\n"
        "  --no-title-page    Leave out the title page\n");
}

std::optional<int> Cli::handleArguments(const QStringList &arguments, const QString &version) {
    if (arguments.size() < 2)
        return std::nullopt;

    const QString &first = arguments.at(1);
    if (first == QLatin1String("-h") || first == QLatin1String("--help")) {
        QTextStream(stdout) << usage();
        return 0;
    }
    if (first == QLatin1String("-v") || first == QLatin1String("--version")) {
        QTextStream(stdout) << QStringLiteral("omascript %1 (based on Omawrite 0.7.2)\n").arg(version);
        return 0;
    }
    if (first == QLatin1String("--pdf"))
        return Screenplay::exportCommand(arguments);

    return std::nullopt;
}

std::optional<int> Cli::handleArguments(int argc, char *argv[], const QString &version) {
    QStringList arguments;
    arguments.reserve(argc);
    for (int index = 0; index < argc; ++index)
        arguments.append(QString::fromLocal8Bit(argv[index]));

    return handleArguments(arguments, version);
}
