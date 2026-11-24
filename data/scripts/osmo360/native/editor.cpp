// SPDX-License-Identifier: GPL-3.0-or-later
#include "osmo.h"
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCommandLineParser>
#include <QDoubleSpinBox>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QProgressBar>
#include <QSignalBlocker>
#include <QSlider>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace Osmo;
namespace {
class Canvas : public QWidget
{
public:
    QImage image;
    std::function<void(QPoint, bool)> drag;
    std::function<void(int)> zoom;
    std::function<void()> resized;
    Canvas()
    {
        setMinimumSize(220, 100);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), QColor(23, 26, 29));
        if (image.isNull()) {
            p.setPen(Qt::white);
            p.drawText(rect(), Qt::AlignCenter, "Open or drop an Osmo 360 recording");
            return;
        }
        QRect r(QPoint(), image.size().scaled(size(), Qt::KeepAspectRatio));
        r.moveCenter(rect().center());
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.drawImage(r, image);
    }
    void resizeEvent(QResizeEvent *) override
    {
        if (resized) resized();
    }
    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton) {
            origin = e->position().toPoint();
            held = true;
        }
    }
    void mouseMoveEvent(QMouseEvent *e) override
    {
        if (held && drag) drag(e->position().toPoint() - origin, false);
    }
    void mouseReleaseEvent(QMouseEvent *e) override
    {
        if (held && e->button() == Qt::LeftButton) {
            held = false;
            if (drag) drag(e->position().toPoint() - origin, true);
        }
    }
    void wheelEvent(QWheelEvent *e) override
    {
        if (zoom && e->angleDelta().y()) zoom(e->angleDelta().y() > 0 ? -3 : 3);
    }

private:
    QPoint origin;
    bool held = false;
};

class Editor : public QMainWindow
{
public:
    Editor(QString output, bool integrated)
        : output(std::move(output))
        , integrated(integrated)
    {
        setWindowTitle("Osmo 360 Editor");
        setAcceptDrops(true);
        resize(860, 620);
        auto *file = menuBar()->addMenu("&File");
        file->addAction("&Open recording or edit…", QKeySequence::Open, this, [this] { choose(); });
        auto *central = new QWidget;
        setCentralWidget(central);
        auto *layout = new QVBoxLayout(central);
        canvas = new Canvas;
        layout->addWidget(canvas, 1);
        controls = new QWidget;
        layout->addWidget(controls);
        auto *panel = new QVBoxLayout(controls);
        panel->setContentsMargins(0, 0, 0, 0);
        auto row = [&] {
            auto *r = new QHBoxLayout;
            panel->addLayout(r);
            return r;
        };
        auto label = [](QHBoxLayout *r, const QString &s) { r->addWidget(new QLabel(s)); };
        auto button = [this](QHBoxLayout *r, const QString &s, auto fn) {
            auto *b = new QPushButton(s);
            r->addWidget(b);
            connect(b, &QPushButton::clicked, this, fn);
            return b;
        };
        auto *timeline = row();
        seek = new QSlider(Qt::Horizontal);
        timeline->addWidget(seek, 1);
        position = new QLabel;
        timeline->addWidget(position);
        auto *transport = row();
        playBtn = button(transport, "Play", [this] { playPause(); });
        button(transport, "Previous key", [this] { jump(false); });
        button(transport, "Next key", [this] { jump(true); });
        button(transport, "Set aim key", [this] { aim(view); });
        button(transport, "Delete key", [this] {
            if (!edit.track.rolls.contains(frame) && (!edit.track.aim.contains(frame) || edit.track.aim.size() == 1)) return;
            remember();
            if (edit.track.aim.size() > 1) edit.track.aim.remove(frame);
            edit.track.rolls.remove(frame);
            move(frame);
        });
        button(transport, "Undo", [this] {
            if (!history.isEmpty()) {
                edit.track = history.takeLast();
                dirty = true;
                move(frame);
            }
        });
        auto *angles = row();
        pan = new QCheckBox("Pan only (lock pitch)");
        angles->addWidget(pan);
        label(angles, "FOV");
        fov = new QDoubleSpinBox;
        fov->setRange(25, 145);
        fov->setSuffix("°");
        angles->addWidget(fov);
        label(angles, "Roll");
        roll = new QDoubleSpinBox;
        roll->setRange(-180, 180);
        roll->setSuffix("°");
        angles->addWidget(roll);
        button(angles, "Key roll", [this] {
            remember();
            edit.track.rolls[frame] = {roll->value(), false};
            move(frame);
        });
        auto *options = row();
        label(options, "Output");
        mode = new QComboBox;
        mode->addItems(modes);
        options->addWidget(mode);
        quality = new QComboBox;
        quality->addItems({"Standard", "High"});
        options->addWidget(quality);
        dimensions = new QLabel;
        options->addWidget(dimensions);
        options->addStretch();
        auto *actions = row();
        button(actions, integrated ? "Render and import" : "Export MP4", [this] { exportVideo(); });
        button(actions, "Save edit…", [this] { save(); });
        bar = new QProgressBar;
        bar->setAccessibleName("Osmo 360 processing progress");
        layout->addWidget(bar);
        bar->hide();
        cancel = new QPushButton("Cancel job");
        layout->addWidget(cancel);
        cancel->hide();
        status = new QLabel("Open a .OSV recording or a .k360 edit.");
        status->setWordWrap(true);
        layout->addWidget(status);
        connect(cancel, &QPushButton::clicked, this, [this] {
            if (workStop) workStop->store(true);
            cancel->setEnabled(false);
            status->setText("Cancelling…");
        });
        connect(seek, &QSlider::sliderMoved, this, [this](int n) {
            stopPlay();
            move(n);
        });
        connect(seek, &QSlider::valueChanged, this, [this](int n) {
            if (n != frame) {
                stopPlay();
                move(n);
            }
        });
        connect(pan, &QCheckBox::toggled, this, [this](bool b) {
            remember();
            edit.track.panOnly = b;
        });
        connect(fov, &QDoubleSpinBox::editingFinished, this, [this] {
            if (view.fov != fov->value()) {
                auto v = view;
                v.fov = fov->value();
                aim(v);
            }
        });
        connect(roll, &QDoubleSpinBox::valueChanged, this, [this](double value) {
            stopPlay();
            view.roll = value;
            request();
        });
        connect(mode, &QComboBox::currentTextChanged, this, [this](const QString &m) {
            edit.mode = m;
            dirty = true;
            updateSize();
            request();
        });
        connect(quality, &QComboBox::currentIndexChanged, this, [this] { updateSize(); });
        canvas->zoom = [this](int d) {
            if (!ready() || busy || sphere()) return;
            auto v = view;
            v.fov = std::clamp(v.fov + d, 25., 145.);
            aim(v);
        };
        canvas->drag = [this](QPoint delta, bool end) {
            if (!ready() || busy || sphere() || (delta.isNull() && !dragging)) return;
            if (!dragging) {
                stopPlay();
                dragOrigin = view;
                dragging = true;
            }
            auto display = target().scaled(canvas->size(), Qt::KeepAspectRatio);
            double step = dragOrigin.fov / std::max(1, display.width());
            view = dragOrigin;
            double yaw = view.yaw - delta.x() * step;
            view.yaw = yaw - 360 * std::floor((yaw + 180) / 360);
            if (!edit.track.panOnly) view.pitch = std::clamp(view.pitch + delta.y() * step, -90., 90.);
            if (end) {
                dragging = false;
                aim(view);
            } else
                request();
        };
        canvas->resized = [this] { request(); };
        controls->setEnabled(false);
        pool.setMaxThreadCount(2);
        timer.setInterval(125);
        connect(&timer, &QTimer::timeout, this, [this] {
            if (thumbnailActive || pending) return;
            int n = std::min(edit.clip.frames - 1, playStart + int(clock.elapsed() * .001 * edit.clip.fps));
            if (n >= edit.clip.frames - 1) stopPlay();
            if (n != frame) move(n);
        });
    }
    ~Editor() override
    {
        if (thumbStop) thumbStop->store(true);
        if (workStop) workStop->store(true);
        pool.waitForDone();
    }
    QString result;
    bool openFailed = false;
    bool open(const QString &path)
    {
        if (busy || !discard()) return false;
        stopPlay();
        beginWork("open", "Opening " + QFileInfo(path).fileName() + "…");
        auto stop = workStop;
        pool.start([this, path, stop] {
            Edit next;
            std::shared_ptr<QTemporaryDir> tmp;
            QString error;
            try {
                next = openEdit(path, stop);
                tmp = std::make_shared<QTemporaryDir>();
                if (!tmp->isValid() || !QDir(tmp->path()).mkdir("preview") || !QDir(tmp->path()).mkdir("proxy"))
                    fail("Cannot create temporary preview directory.");
            } catch (const std::exception &e) {
                error = QString::fromUtf8(e.what());
            }
            QMetaObject::invokeMethod(this, [this, next = std::move(next), tmp, stop, path, error]() mutable {
                endWork();
                if (stop->load() || !error.isEmpty()) {
                    status->setText(stop->load() ? "Opening cancelled." : error);
                    if (!ready() && !canOpen()) {
                        if (!stop->load()) {
                            std::fprintf(stderr, "%s\n", qPrintable(error));
                            openFailed = true;
                            if (!integrated) QMessageBox::warning(this, "Open Osmo 360 clip", error);
                        }
                        close();
                    }
                    return;
                }
                if (thumbStop) thumbStop->store(true);
                edit = std::move(next);
                folder = tmp;
                proxy = {};
                dirty = false;
                history.clear();
                result.clear();
                dragging = false;
                editPath = path.endsWith(".k360", Qt::CaseInsensitive) ? path : QString();
                QSignalBlocker modeBlock(mode), seekBlock(seek);
                mode->setCurrentText(edit.mode);
                controls->setEnabled(true);
                seek->setRange(0, edit.clip.frames - 1);
                canvas->image = {};
                canvas->update();
                setWindowTitle("Osmo 360 Editor — " + QFileInfo(edit.clip.path).fileName());
                status->setText("Preparing first preview…");
                updateSize();
                move(0);
            }, Qt::QueuedConnection);
        });
        return true;
    }

protected:
    void dragEnterEvent(QDragEnterEvent *e) override
    {
        if (canOpen() && !dropPath(e->mimeData()).isEmpty()) e->acceptProposedAction();
    }
    void dropEvent(QDropEvent *e) override
    {
        auto path = dropPath(e->mimeData());
        if (canOpen() && !path.isEmpty() && open(path)) e->acceptProposedAction();
    }
    void closeEvent(QCloseEvent *e) override
    {
        if (busy) {
            if (QMessageBox::question(this, "Cancel job", "Cancel the current job and close?") != QMessageBox::Yes) {
                e->ignore();
                return;
            }
        }
        if (!discard()) {
            e->ignore();
            return;
        }
        if (workStop) workStop->store(true);
        if (thumbStop) thumbStop->store(true);
        stopPlay();
        e->accept();
    }

private:
    bool ready() const { return bool(folder); }
    bool canOpen() const { return !busy && !integrated && output.isEmpty(); }
    static QString dropPath(const QMimeData *data)
    {
        auto urls = data->urls();
        if (urls.size() != 1 || !urls.first().isLocalFile()) return {};
        auto path = urls.first().toLocalFile();
        return path.endsWith(".osv", Qt::CaseInsensitive) || path.endsWith(".k360", Qt::CaseInsensitive) ? path : QString();
    }
    void choose()
    {
        if (canOpen()) {
            auto p = QFileDialog::getOpenFileName(this, "Open Osmo 360 clip", {}, "Osmo files (*.OSV *.osv *.k360);;All files (*)");
            if (!p.isEmpty()) open(p);
        }
    }
    bool discard()
    {
        if (!dirty) return true;
        auto answer =
            QMessageBox::question(this, "Camera track", "Save your camera keyframes?", QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        return answer == QMessageBox::Discard || (answer == QMessageBox::Save && save());
    }
    bool save()
    {
        auto path =
            QFileDialog::getSaveFileName(this, "Save editable camera track", editPath.isEmpty() ? suggested(".k360") : editPath, "Camera tracks (*.k360)");
        if (path.isEmpty()) return false;
        try {
            saveEdit(path, edit);
            editPath = path;
            dirty = false;
            return true;
        } catch (const std::exception &e) {
            QMessageBox::warning(this, "Save edit", QString::fromUtf8(e.what()));
            return false;
        }
    }
    QString suggested(const QString &suffix) const
    {
        QFileInfo info(edit.clip.path);
        return info.dir().filePath(info.completeBaseName() + suffix);
    }
    void remember()
    {
        if (history.size() >= 50) history.removeFirst();
        history << edit.track;
        dirty = true;
    }
    void aim(View v)
    {
        stopPlay();
        remember();
        v.roll = 0;
        edit.track.aim[frame] = v;
        move(frame);
    }
    void jump(bool forward)
    {
        auto keys = edit.track.frames();
        if (!forward) std::reverse(keys.begin(), keys.end());
        for (int k : keys)
            if (forward ? k > frame : k < frame) {
                stopPlay();
                move(k);
                return;
            }
    }
    bool sphere() const { return edit.mode == "360 panorama"; }
    QSize target() const { return outputSize(edit.mode, quality->currentIndex() == 1, edit.clip.width); }
    void updateSize()
    {
        if (ready()) {
            auto s = target();
            dimensions->setText(QString("%1 × %2").arg(s.width()).arg(s.height()));
        }
    }
    void move(int n)
    {
        frame = n;
        view = edit.track.at(frame);
        QSignalBlocker a(seek), b(pan), c(fov), d(roll);
        seek->setValue(frame);
        pan->setChecked(edit.track.panOnly);
        fov->setValue(view.fov);
        roll->setValue(view.roll);
        position->setText(QString("%1 s%2").arg(frame / edit.clip.fps, 0, 'f', 2).arg(edit.track.frames().contains(frame) ? " ◆" : ""));
        request();
    }
    void stopPlay()
    {
        timer.stop();
        if (playBtn) playBtn->setText("Play");
    }
    void playPause()
    {
        if (timer.isActive()) {
            stopPlay();
            return;
        }
        if (!proxy.frames) {
            background(true, {});
            return;
        }
        if (frame >= edit.clip.frames - 1) move(0);
        playStart = frame;
        clock.restart();
        timer.start();
        playBtn->setText("Pause");
    }
    void exportVideo()
    {
        stopPlay();
        auto path = output;
        if (path.isEmpty())
            path =
                QFileDialog::getSaveFileName(this, "Export from original lenses", suggested("-" + edit.mode.section(' ', 0, 0) + ".mp4"), "MP4 video (*.mp4)");
        if (!path.isEmpty()) background(false, QFileInfo(path).absoluteFilePath());
    }
    void showProgress(const QByteArray &stage, int percent = -1)
    {
        if (stage == currentStage && percent == currentPercent) return;
        currentStage = stage;
        currentPercent = percent;
        if (stage == "idle") {
            bar->hide();
        } else {
            bar->setRange(0, percent < 0 ? 0 : 100);
            if (percent >= 0) bar->setValue(percent);
            bar->show();
        }
        if (integrated) {
            std::fprintf(stderr, "osmo360 %s %d\n", stage.constData(), percent);
            std::fflush(stderr);
        } else if (!output.isEmpty() && stage == "render" && percent >= 0) {
            // Stock Kdenlive's Media Job monitor reads MLT-style progress on stderr.
            std::fprintf(stderr, "percentage: %d\n", percent);
            std::fflush(stderr);
        }
    }
    void beginWork(const QByteArray &stage, const QString &message)
    {
        busy = true;
        controls->setEnabled(false);
        cancel->setEnabled(true);
        cancel->show();
        workStop = std::make_shared<std::atomic_bool>(false);
        status->setText(message);
        showProgress(stage);
    }
    void endWork()
    {
        busy = false;
        workStop.reset();
        controls->setEnabled(ready());
        cancel->hide();
        showProgress(ready() && canvas->image.isNull() && thumbnailActive ? "preview" : "idle");
    }
    void background(bool building, QString path)
    {
        beginWork(building ? "proxy" : "render", building ? "Preparing playback proxy…" : "Rendering from the original lenses…");
        auto e = edit;
        auto tmp = folder;
        auto stop = workStop;
        auto size = target();
        pool.start([this, e, tmp, stop, size, path, building] {
            Proxy made;
            QString error;
            auto progress = [this, building, total = e.clip.frames, fps = e.clip.fps](int n) {
                if (n <= 0) return;
                int percent = std::clamp(int(n * 100. / (building ? std::max(1., total * 8 / fps) : total)), 0, 99);
                QMetaObject::invokeMethod(
                    this,
                    [this, percent, building] {
                        if (workStop && !workStop->load()) showProgress(building ? "proxy" : "render", percent);
                    },
                    Qt::QueuedConnection);
            };
            try {
                if (building)
                    made = makeProxy(e.clip, tmp->filePath("proxy"), stop, progress);
                else
                    Osmo::render(e, size, path, stop, progress);
            } catch (const std::exception &ex) {
                error = QString::fromUtf8(ex.what());
            }
            QMetaObject::invokeMethod(
                this,
                [this, building, made, error, path] {
                    if (error.isEmpty()) showProgress(building ? "proxy" : "render", 100);
                    endWork();
                    if (!error.isEmpty()) {
                        status->setText(error);
                        return;
                    }
                    if (building) {
                        proxy = made;
                        status->setText("Playback preview · 1024 × 512 · 8 fps");
                        playPause();
                    } else {
                        result = path;
                        status->setText("Exported " + path);
                        if (integrated || !output.isEmpty()) close();
                    }
                },
                Qt::QueuedConnection);
        });
    }
    void request()
    {
        if (!ready()) return;
        if (!busy && canvas->image.isNull()) showProgress("preview");
        ++generation;
        pending = true;
        if (thumbnailActive) return;
        pending = false;
        thumbnailActive = true;
        auto gen = generation;
        auto tmp = folder;
        auto c = edit.clip;
        auto v = view;
        auto p = proxy;
        int n = frame;
        bool full = sphere();
        QSize s = target().scaled(canvas->size().boundedTo({720, 720}).expandedTo({2, 2}), Qt::KeepAspectRatio);
        s = s.expandedTo({2, 2});
        thumbStop = std::make_shared<std::atomic_bool>(false);
        auto stop = thumbStop;
        pool.start([this, tmp, c, v, p, n, full, s, gen, stop] {
            QImage img;
            QString error;
            try {
                img = preview(c, n, v, s, full, p, tmp->filePath("preview"), stop);
            } catch (const std::exception &ex) {
                error = QString::fromUtf8(ex.what());
            }
            QMetaObject::invokeMethod(
                this,
                [this, img, error, gen, n] {
                    thumbnailActive = false;
                    if (gen == generation) {
                        if (!busy && currentStage == "preview") showProgress("idle");
                        if (error.isEmpty()) {
                            canvas->image = img;
                            canvas->update();
                            if (!busy && (status->text() == "Preparing first preview…" || status->text().startsWith("Preview failed")))
                                status->setText(edit.clip.calibration.empty() ? "Generic lens geometry" : "Factory lens calibration");
                        } else {
                            stopPlay();
                            if (!busy) status->setText(QString("Preview failed at frame %1. ").arg(n) + error);
                        }
                    }
                    if (pending) request();
                },
                Qt::QueuedConnection);
        });
    }
    Edit edit;
    QString output, editPath;
    bool integrated = false, dirty = false, busy = false, dragging = false;
    int frame = 0, playStart = 0;
    View view, dragOrigin;
    Proxy proxy;
    std::shared_ptr<QTemporaryDir> folder;
    Stop thumbStop, workStop;
    QThreadPool pool;
    QTimer timer;
    QElapsedTimer clock;
    QList<Track> history;
    bool pending = false, thumbnailActive = false;
    quint64 generation = 0;
    QByteArray currentStage = "idle";
    int currentPercent = -1;
    QProgressBar *bar;
    Canvas *canvas;
    QWidget *controls;
    QPushButton *playBtn = nullptr, *cancel;
    QCheckBox *pan;
    QDoubleSpinBox *fov, *roll;
    QSlider *seek;
    QComboBox *mode, *quality;
    QLabel *dimensions, *position, *status;
};
} // namespace
int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("osmo360-editor");
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addPositionalArgument("source", "Osmo recording or saved camera track.", "[source]");
    parser.addOption({"output", "MP4 destination supplied by Kdenlive.", "path"});
    parser.addOption({"integrated", "Return the finished clip to KdenCut."});
    parser.addOption({"install", "Register this executable as a Kdenlive Media Job."});
    parser.addOption({"data-dir", "Override the Kdenlive data directory for registration.", "directory"});
    parser.process(app);
    if (parser.isSet("install")) {
        try {
            installJob(parser.value("data-dir"));
            return 0;
        } catch (const std::exception &e) {
            std::fprintf(stderr, "%s\n", e.what());
            return 1;
        }
    }
    Editor editor(parser.value("output"), parser.isSet("integrated"));
    auto args = parser.positionalArguments();
    editor.show();
    if (!args.isEmpty()) editor.open(args.first());
    app.exec();
    if (editor.openFailed) return 1;
    if (editor.result.isEmpty()) return 2;
    auto reply = QJsonDocument(QJsonObject{{"clip", editor.result}}).toJson(QJsonDocument::Compact);
    std::fwrite(reply.constData(), 1, reply.size(), stdout);
    std::fputc('\n', stdout);
    return 0;
}
