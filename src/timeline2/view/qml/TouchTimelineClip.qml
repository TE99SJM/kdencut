/* SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL */
pragma ComponentBehavior: Bound
import QtQuick

Item {
    id: clip
    required property int clipId
    required property int trackId
    required property int startFrame
    required property int durationFrames
    required property real scale
    required property string name
    property bool audio: false
    property bool composition: false
    property bool selected: false
    property bool locked: false
    property int inFrame: 0
    property real speed: 1
    property string thumbnail: ""
    property real viewportWidth: 430
    property real viewportStart: 0
    property real origin: 0
    property int previewDuration: -1
    property real moveOffset: 0
    property bool trimRight: true
    readonly property int shownDuration: previewDuration < 0 ? durationFrames : previewDuration
    readonly property real leftTrim: previewDuration < 0 || trimRight ? 0 : (durationFrames - previewDuration) * scale
    readonly property bool editing: body.held || leftHandle.pressed || rightHandle.pressed
    signal selectRequested(int id, int track)
    signal moveRequested(int id, int track, int frame, bool composition)
    signal trimRequested(int id, int duration, bool right)
    signal menuRequested(int id, bool composition)

    x: origin + startFrame * scale + moveOffset + leftTrim
    width: Math.max(3, shownDuration * scale)
    opacity: body.held ? 0.8 : 1
    z: editing || selected ? 2 : 0
    objectName: "touchClip_" + clipId

    Rectangle {
        anchors.fill: parent
        radius: 5
        color: clip.audio ? "#234c4d" : clip.composition ? "#51405a" : "#343434"
        clip: true
        Repeater {
            id: thumbs
            readonly property int first: Math.max(0, Math.floor((clip.viewportStart - clip.x) / 64))
            model: clip.audio || clip.thumbnail === "" ? 0 : Math.max(0, Math.min(Math.ceil(clip.width / 64) - first, Math.ceil(clip.viewportWidth / 64) + 2))
            delegate: Image {
                required property int index
                x: (thumbs.first + index) * 64
                width: 64
                height: clip.height
                fillMode: Image.PreserveAspectCrop
                sourceSize: Qt.size(96, 64)
                source: "image://thumbnail/" + clip.thumbnail + Math.max(0, Math.floor((clip.inFrame + (thumbs.first + index) * 64 / clip.scale) * clip.speed))
                asynchronous: true
            }
        }
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 22
            color: clip.audio ? "transparent" : "#99000000"
            Text {
                anchors.fill: parent
                anchors.leftMargin: 9
                anchors.rightMargin: 9
                text: clip.name
                color: "#ededed"
                font.pixelSize: 11
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }
    }
    Rectangle {
        anchors.fill: parent
        radius: 5
        color: "transparent"
        border.width: clip.selected ? 2 : 0
        border.color: "white"
    }
    MouseArea {
        id: body
        anchors.fill: parent
        property bool held: false
        property real pressX: 0
        pressAndHoldInterval: 450
        preventStealing: held
        onPressed: mouse => {
            pressX = mapToItem(null, mouse.x, mouse.y).x
        }
        onPressAndHold: {
            if (!clip.locked) {
                clip.selectRequested(clip.clipId, clip.trackId)
                held = true
            }
        }
        onPositionChanged: mouse => {
            if (held) clip.moveOffset = Math.max(-clip.startFrame * clip.scale, mapToItem(null, mouse.x, mouse.y).x - pressX)
        }
        onReleased: {
            if (held) {
                clip.moveRequested(clip.clipId, clip.trackId, Math.max(0, clip.startFrame + Math.round(clip.moveOffset / clip.scale)), clip.composition)
            }
            held = false
            clip.moveOffset = 0
        }
        onCanceled: { held = false; clip.moveOffset = 0 }
        onClicked: clip.selectRequested(clip.clipId, clip.trackId)
        onDoubleClicked: clip.menuRequested(clip.clipId, clip.composition)
    }
    component TrimHandle: MouseArea {
        required property bool rightEdge
        property real pressX: 0
        property int originalDuration: 0
        visible: clip.selected && !clip.locked
        width: 44
        height: Math.max(44, clip.height)
        anchors.verticalCenter: parent.verticalCenter
        x: rightEdge ? clip.width - 22 : -22
        preventStealing: true
        onPressed: mouse => {
            pressX = mapToItem(null, mouse.x, mouse.y).x
            originalDuration = clip.durationFrames
            clip.trimRight = rightEdge
        }
        onPositionChanged: mouse => {
            if (pressed) {
                const delta = Math.round((mapToItem(null, mouse.x, mouse.y).x - pressX) / clip.scale)
                clip.previewDuration = Math.max(1, originalDuration + (rightEdge ? delta : -delta))
            }
        }
        onReleased: {
            if (clip.previewDuration > 0) clip.trimRequested(clip.clipId, clip.previewDuration, rightEdge)
            clip.previewDuration = -1
        }
        onCanceled: clip.previewDuration = -1
        Rectangle {
            anchors.centerIn: parent
            width: 12
            height: clip.height
            radius: 4
            color: "white"
            Rectangle { anchors.centerIn: parent; width: 2; height: 18; radius: 1; color: "#343434" }
        }
    }
    TrimHandle { id: leftHandle; rightEdge: false }
    TrimHandle { id: rightHandle; rightEdge: true }
}
