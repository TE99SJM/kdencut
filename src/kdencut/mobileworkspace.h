/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
#pragma once

#include <QMap>
#include <QWidget>

class QAction;
class QDialog;
class QLabel;
class QScrollArea;
class QSplitter;
class QStackedWidget;
class QToolButton;

namespace KdenCut {

/** Phone workspace hosting the actual monitor and timeline, without desktop docks.
 * The caller lends the widgets and must return them to their docks before deletion.
 */
class MobileWorkspace : public QWidget
{
    Q_OBJECT
public:
    /** Keep large legacy forms reachable in a scrollable phone dialog. */
    static void fitDialog(QDialog *dialog, const QRect &available);
    MobileWorkspace(QWidget *preview, QWidget *timeline, const QMap<QString, QAction *> &actions, QWidget *parent = nullptr);
    void showPanel(QWidget *panel, const QString &title, bool media = false);
    QWidget *takePanel();
    void showEditor();
    void setPlaying(bool playing);
    void setTimecode(const QString &timecode);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

Q_SIGNALS:
    void command(const QString &name);
    void panelClosed();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    QToolButton *button(const QString &text, const QString &icon, const QString &command, bool label = true);
    QToolButton *actionButton(const QString &name, const QString &text, const QString &icon, bool label = true);
    QWidget *toolStrip(const QList<QWidget *> &buttons);
    const QMap<QString, QAction *> m_actions;
    QStackedWidget *m_pages;
    QWidget *m_editor;
    QSplitter *m_splitter;
    QStackedWidget *m_lower;
    QWidget *m_timeline;
    QWidget *m_panelPage;
    QScrollArea *m_panelScroll;
    QLabel *m_panelTitle;
    QWidget *m_mediaTools;
    QLabel *m_timecode;
    QToolButton *m_play;
    bool m_landscape{false};
};
} // namespace KdenCut
