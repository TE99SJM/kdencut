/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
pragma ComponentBehavior: Bound
import QtQuick
import QtQml.Models

Rectangle {
    id: touch
    required property var tracks
    required property var subtitles
    required property int frame
    required property int duration
    required property real fps
    required property real pixelsPerFrame
    required property int compositionType
    property bool editingClip: false
    readonly property real scale: Math.max(0.01, pixelsPerFrame)
    readonly property real scrollPosition: canvas.contentX
    readonly property real playheadX: width / 2
    readonly property int tickFrames: Math.max(1, Math.round(Math.max(1, Math.ceil(72 / (scale * fps))) * fps))
    signal seekRequested(int position)
    signal selectRequested(int id, int track, bool subtitle)
    signal moveRequested(int id, int track, int position, bool composition)
    signal trimRequested(int id, int size, bool right)
    signal menuRequested(int id, bool composition, bool subtitle)
    signal command(string name)
    color: "#1c1c1c"
    clip: true
    objectName: "touchTimeline"

    function clock(value) {
        const seconds = Math.max(0, Math.floor(value / Math.max(1, fps)))
        return Math.floor(seconds / 60).toString().padStart(2, "0") + ":" + (seconds % 60).toString().padStart(2, "0")
    }
    function followPlayhead() {
        if (!canvas.moving && !editingClip) canvas.contentX = Math.max(0, frame * scale)
    }
    function restoreScroll(value) { canvas.contentX = Math.max(0, value) }
    onFrameChanged: followPlayhead()
    onScaleChanged: followPlayhead()
    onWidthChanged: followPlayhead()
    Component.onCompleted: followPlayhead()

    Text {
        x: 14; y: 6
        text: touch.clock(touch.frame) + "  /  " + touch.clock(touch.duration)
        color: "#aaaaaa"
        font.pixelSize: 11
    }
    Text {
        anchors.right: parent.right
        anchors.rightMargin: 14
        y: 6
        text: touch.editingClip ? qsTr("Release to apply") : qsTr("Swipe to scrub · Hold a clip to move")
        color: "#888888"
        font.pixelSize: 10
        visible: touch.width > 370
    }
    Flickable {
        id: canvas
        objectName: "touchTimelineScroll"
        anchors.fill: parent
        anchors.topMargin: 28
        contentWidth: Math.max(width, Math.max(touch.duration, touch.fps * 5) * touch.scale + width)
        contentHeight: Math.max(height, rows.height + 34)
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.HorizontalAndVerticalFlick
        flickDeceleration: 1800
        maximumFlickVelocity: 2400
        interactive: !touch.editingClip
        pressDelay: 0
        clip: true
        property int pendingFrame: 0
        onContentXChanged: {
            if (moving) {
                pendingFrame = Math.max(0, Math.min(touch.duration, Math.round(contentX / touch.scale)))
                if (!scrubTimer.running) scrubTimer.start()
            }
        }
        onMovementEnded: {
            scrubTimer.stop()
            touch.seekRequested(Math.max(0, Math.min(touch.duration, Math.round(contentX / touch.scale))))
        }
        Timer {
            id: scrubTimer
            interval: 33
            onTriggered: touch.seekRequested(canvas.pendingFrame)
        }
        Repeater {
            readonly property int first: Math.max(0, Math.floor((canvas.contentX - touch.playheadX) / (touch.tickFrames * touch.scale)))
            id: ticks
            model: Math.ceil(canvas.width / (touch.tickFrames * touch.scale)) + 3
            delegate: Text {
                required property int index
                x: touch.playheadX + (ticks.first + index) * touch.tickFrames * touch.scale - width / 2
                text: touch.clock((ticks.first + index) * touch.tickFrames)
                color: "#888888"
                font.pixelSize: 10
            }
        }
        Column {
            id: rows
            y: 27
            width: canvas.contentWidth
            spacing: 6
            Repeater {
                model: trackDelegates
            }
            Rectangle {
                visible: touch.duration === 0
                x: touch.playheadX
                width: Math.max(150, canvas.width / 2 - 12)
                height: visible ? 64 : 0
                radius: 6
                color: "#2b2b2b"
                border.color: "#494949"
                Text { anchors.centerIn: parent; text: qsTr("+ Add video"); color: "#eeeeee"; font.pixelSize: 13 }
                TapHandler { onTapped: touch.command("import") }
            }
            Item {
                width: rows.width
                height: captions.count > 0 ? 44 : 0
                visible: captions.count > 0
                Repeater {
                    id: captions
                    model: touch.subtitles
                    delegate: Rectangle {
                        id: caption
                        required property var model
                        x: touch.playheadX + model.startframe * touch.scale
                        width: Math.max(3, (model.endframe - model.startframe) * touch.scale)
                        height: 40
                        radius: 4
                        color: "#524a31"
                        border.width: model.selected ? 2 : 0
                        border.color: "white"
                        Text { anchors.fill: parent; anchors.margins: 7; text: caption.model.subtitle; color: "#eeeeee"; elide: Text.ElideRight; font.pixelSize: 12 }
                        TapHandler {
                            onTapped: touch.selectRequested(caption.model.id, -1, true)
                            onDoubleTapped: touch.menuRequested(caption.model.id, false, true)
                        }
                    }
                }
            }
            Repeater {
                model: [qsTr("+ Add audio"), qsTr("+ Add text")]
                delegate: Rectangle {
                    id: addLane
                    required property int index
                    required property string modelData
                    x: touch.playheadX
                    width: Math.max(150, Math.max(touch.duration, touch.fps * 5) * touch.scale)
                    height: 38
                    radius: 4
                    color: "#272727"
                    Text { x: 12; anchors.verticalCenter: parent.verticalCenter; text: addLane.modelData; font.pixelSize: 12; color: "#969696" }
                    TapHandler { onTapped: touch.command(addLane.index === 0 ? "audio" : "text") }
                }
            }
        }
        Rectangle {
            x: touch.playheadX + touch.duration * touch.scale + 8
            y: 37
            width: 44
            height: 44
            radius: 6
            color: "#f3f3f3"
            visible: touch.duration > 0
            Text { anchors.centerIn: parent; text: "+"; font.pixelSize: 28; color: "#161616" }
            TapHandler { onTapped: touch.command("append") }
        }
    }
    Rectangle {
        objectName: "touchPlayhead"
        x: touch.playheadX - 1
        y: 45
        width: 2
        height: Math.max(0, touch.height - y - 10)
        radius: 1
        color: "#eeeeee"
    }
    DelegateModel {
        id: trackDelegates
        model: touch.tracks
        delegate: Item {
            id: row
            required property int index
            required property var model
            width: rows.width
            height: clips.count === 0 ? 0 : model.audio ? 46 : 66
            visible: clips.count > 0
            Repeater {
                id: clips
                model: DelegateModel {
                    model: touch.tracks
                    rootIndex: trackDelegates.modelIndex(row.index)
                    delegate: TouchTimelineClip {
                        id: item
                        required property var model
                        height: row.height
                        clipId: model.item
                        trackId: row.model.item
                        startFrame: model.start
                        durationFrames: model.duration
                        name: model.name
                        audio: row.model.audio
                        locked: row.model.locked
                        composition: model.clipType === touch.compositionType
                        selected: model.selected
                        inFrame: model.in || 0
                        speed: model.speed || 1
                        thumbnail: model.clipThumbId || ""
                        scale: touch.scale
                        origin: touch.playheadX
                        viewportWidth: canvas.width
                        viewportStart: canvas.contentX
                        onEditingChanged: touch.editingClip = editing
                        onSelectRequested: (id, track) => touch.selectRequested(id, track, false)
                        onMoveRequested: (id, track, frame, composition) => touch.moveRequested(id, track, frame, composition)
                        onTrimRequested: (id, duration, right) => touch.trimRequested(id, duration, right)
                        onMenuRequested: (id, composition) => touch.menuRequested(id, composition, false)
                    }
                }
            }
        }
    }
}
