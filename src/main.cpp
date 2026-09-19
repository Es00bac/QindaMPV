// SPDX-License-Identifier: GPL-3.0-or-later
//
// QindaMPV bootstrap. Composition (constructor-visible, GUI thread):
//
//   PlayerController          — the libmpv engine facade (`player` in QML)
//   ApplicationCoordinator    — the AppShell action/menu model (`coordinator`)
//   Main.qml (Tk.AppWindow)   — QindaTK chrome, MpvVideoItem content
//   composeFirstPartyMenuExport — publishes the coordinator's menus as the
//                               desktop global menu; fail-closed, so the
//                               in-window Tk.MenuBar stays visible whenever
//                               the export is not live
//
// Destruction order is the reverse of declaration below; the export dies
// before the window it announced (its contract), the engine after it.

#include "app/action_catalog.h"
#include "player/player_controller.h"

#include <qindaqt/app_shell/application_coordinator.h>
#include <qindaqt/app_shell/menu_export/first_party_composition.h>

#include <QCommandLineParser>
#include <QDBusConnection>
#include <QGuiApplication>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTimer>
#include <QUrl>

#include <cstdio>
#include <clocale>

int main(int argc, char **argv)
{
    // mpv's render_gl API needs an OpenGL scene graph; this must run before
    // the first QQuickWindow is created.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QGuiApplication application(argc, argv);
    // Qt adopts the environment locale (LC_ALL ""); libmpv refuses to run
    // under a non-C LC_NUMERIC and demands the client reset it.
    std::setlocale(LC_NUMERIC, "C");
    QGuiApplication::setApplicationName(QStringLiteral("qqmpv"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("QindaMPV"));
    QGuiApplication::setOrganizationName(QStringLiteral("QindaQt"));
    QGuiApplication::setDesktopFileName(QStringLiteral("org.qindaqt.QQMpv"));
    QGuiApplication::setApplicationVersion(QStringLiteral(QQMPV_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("QindaMPV — mpv media player for the QindaQt desktop"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("files"),
                                 QStringLiteral("Files or URLs to play"),
                                 QStringLiteral("[file|url...]"));
    // Verification seam: render the window offscreen and dump the first
    // settled frame, then exit. Used by packaging/harness screenshots.
    parser.addOption({QStringLiteral("screenshot"),
                      QStringLiteral("Save a window screenshot to PATH and exit"),
                      QStringLiteral("path")});
    parser.process(application);

    QindaMpv::Player::PlayerController player;
    QindaQt::AppShell::ApplicationCoordinator coordinator;
    coordinator.setApplicationName(QStringLiteral("QindaMPV"));
    if (const QindaQt::AppShell::Error error =
            coordinator.replaceActions(QindaMpv::App::buildPlayerActions());
        !error.ok()) {
        qFatal("qqmpv: action catalog rejected: %s", qPrintable(error.message));
    }

    QObject::connect(&coordinator, &QindaQt::AppShell::ApplicationCoordinator::actionRequested,
                     &player, &QindaMpv::Player::PlayerController::dispatchMenuAction);
    QObject::connect(&player, &QindaMpv::Player::PlayerController::mediaTitleChanged,
                     &coordinator, [&coordinator, &player] {
                         coordinator.setWindowTitle(player.mediaTitle());
                     });
    // Keep the exported checkable actions truthful.
    QObject::connect(&player, &QindaMpv::Player::PlayerController::mutedChanged,
                     &coordinator, [&coordinator, &player] {
                         static_cast<void>(coordinator.setActionChecked(QStringLiteral("audio.mute"), player.muted()));
                     });
    QObject::connect(&player, &QindaMpv::Player::PlayerController::fullscreenChanged,
                     &coordinator, [&coordinator, &player] {
                         static_cast<void>(coordinator.setActionChecked(QStringLiteral("video.fullscreen"),
                                                      player.fullscreen()));
                     });
    QObject::connect(&player, &QindaMpv::Player::PlayerController::subtitlesVisibleChanged,
                     &coordinator, [&coordinator, &player] {
                         static_cast<void>(coordinator.setActionChecked(QStringLiteral("video.sub-toggle"),
                                                      player.subtitlesVisible()));
                     });

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("player"), &player);
    engine.rootContext()->setContextProperty(QStringLiteral("coordinator"), &coordinator);

    QList<QUrl> pendingUrls;
    for (const QString &argument : parser.positionalArguments()) {
        const QUrl url = QUrl::fromUserInput(argument);
        if (url.isValid() && !url.isEmpty()) {
            pendingUrls.append(url);
        }
    }

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &application, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("QindaMPV"), QStringLiteral("Main"));

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    if (window == nullptr) {
        return 1;
    }

    QObject::connect(&player, &QindaMpv::Player::PlayerController::quitRequested,
                     window, &QQuickWindow::close);
    if (parser.isSet(QStringLiteral("screenshot"))) {
        const QString path = parser.value(QStringLiteral("screenshot"));
        const int delayMs = qEnvironmentVariableIntValue("QQMPV_SCREENSHOT_MS") > 0
            ? qEnvironmentVariableIntValue("QQMPV_SCREENSHOT_MS") : 1500;
        QTimer::singleShot(delayMs, window, [window, path]() {
            if (window->grabWindow().save(path)) {
                std::fprintf(stdout, "screenshot=%s\n", qPrintable(path));
            } else {
                std::fprintf(stderr, "qqmpv: could not save screenshot to %s\n",
                             qPrintable(path));
            }
            QCoreApplication::quit();
        });
    }
    if (!pendingUrls.isEmpty()) {
        player.openPlaylist(pendingUrls);
    }

    // Opt-in global menu export. Fail-closed: whenever the export is not
    // live, the callback keeps the in-window Tk.MenuBar visible. The object
    // is declared after the engine so it is destroyed before the window.
    QPointer<QQuickWindow> windowGuard(window);
    std::unique_ptr<QObject> menuExport =
        QindaQt::AppShell::MenuExport::composeFirstPartyMenuExport(
            coordinator, *window, QDBusConnection::sessionBus(),
            [windowGuard](bool localMenuVisible) {
                if (windowGuard != nullptr) {
                    windowGuard->setProperty("inWindowMenuVisible", localMenuVisible);
                }
            });

    const int result = QGuiApplication::exec();
    menuExport.reset();
    return result;
}
