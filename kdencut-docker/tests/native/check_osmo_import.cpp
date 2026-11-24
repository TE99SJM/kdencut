// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../../src/kdencut/osmo360import.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QMainWindow>
#include <QProgressBar>
#include <QScopeGuard>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>
#include <cstdio>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    // This test owns a dedicated executable directory. The stand-in emits the
    // same protocol as the native editor, splitting one progress line in two.
    QFile helper(QCoreApplication::applicationDirPath() + QStringLiteral("/osmo360-editor"));
    if (helper.exists() || !helper.open(QIODevice::WriteOnly | QIODevice::NewOnly)) return 1;
    helper.write("#!/bin/sh\nprintf 'osmo360 rend' >&2\nsleep 0.1\nprintf 'er 42\\n' >&2\nsleep 0.4\n"
                 "case \"$2\" in *cancel.OSV) exit 2;; esac\nprintf '{\"clip\":\"%s\"}\\n' \"$OSMO_TEST_OUTPUT\"\n");
    helper.close();
    helper.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    const auto cleanup = qScopeGuard([&helper] { helper.remove(); });
    QTemporaryDir temp;
    QFile result(temp.filePath(QStringLiteral("render.mp4")));
    if (!result.open(QIODevice::WriteOnly)) return 2;
    result.close();
    qputenv("OSMO_TEST_OUTPUT", result.fileName().toUtf8());
    QMainWindow window;
    auto *edit = new QPushButton(QStringLiteral("Edit timeline"), &window);
    window.setCentralWidget(edit);
    window.show();
    bool clicked = false, stale = false;
    int completed = 0;
    QObject::connect(edit, &QPushButton::clicked, [&] { clicked = true; });
    bool currentReturned = false;
    const auto unexpected = [&](const QString &) { ++completed; };
    KdenCut::queueOsmo360Clip(&window, QStringLiteral("old.OSV"), [&] { return !stale; }, unexpected);
    KdenCut::queueOsmo360Clip(&window, QStringLiteral("cancel.OSV"), [] { return true; }, unexpected);
    KdenCut::queueOsmo360Clip(&window, QStringLiteral("current.OSV"), [] { return true; }, [&](const QString &path) {
        currentReturned = path == result.fileName();
        ++completed;
    });
    auto *bar = window.findChild<QProgressBar *>();
    QElapsedTimer deadline;
    deadline.start();
    while (deadline.elapsed() < 8000 && !currentReturned) {
        QTest::qWait(10);
        if (!clicked && bar && bar->isVisible() && bar->value() == 42) {
            QTest::mouseClick(edit, Qt::LeftButton);
            stale = true;
        }
    }
    if (!clicked || !currentReturned || completed != 1 || !bar || bar->isVisible()) {
        std::fprintf(stderr, "Expected input and progress during import, then exactly one completed clip.\n");
        return 3;
    }
    return 0;
}
