import QtQuick
import QtQuick.Controls
import Omascript

// The formatted script beside the editor. Scrolls on its own with the wheel
// or scrollbar, and follows the caret so the line being written stays in view.
Item {
    id: pane
    objectName: "screenplayPane"

    property alias text: preview.text
    property alias backgroundColor: preview.backgroundColor
    property alias edgeColor: preview.edgeColor
    property alias markerColor: preview.markerColor
    readonly property int pageCount: preview.pageCount
    readonly property alias preview: preview

    property int followPosition: -1

    // Keep the formatted line for this Markdown offset in view, nudging the
    // pages only when it has drifted out of the middle band of the pane.
    function follow(sourcePosition) {
        followPosition = sourcePosition;
        preview.caretPosition = sourcePosition;
        followDelay.restart();
    }

    function revealNow() {
        if (followPosition < 0 || flick.moving || flick.dragging)
            return;
        const y = preview.yForSourcePosition(followPosition);
        const top = flick.contentY + height * 0.15;
        const bottom = flick.contentY + height * 0.8;
        if (y >= top && y <= bottom)
            return;
        const target = Math.max(0, Math.min(flick.contentHeight - flick.height, y - height * 0.35));
        scrollAnimation.stop();
        scrollAnimation.from = flick.contentY;
        scrollAnimation.to = target;
        scrollAnimation.start();
    }

    Timer {
        id: followDelay
        interval: 180
        onTriggered: pane.revealNow()
    }

    ScreenplayPreview {
        id: preview
        objectName: "screenplayPreview"
        anchors.fill: parent
        contentY: flick.contentY
        // Once the pages are redrawn after typing, the caret's line may have
        // moved to another page.
        onLayoutChanged: followDelay.restart()
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: Math.max(height, preview.contentHeight)
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        onMovementStarted: scrollAnimation.stop()

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        NumberAnimation {
            id: scrollAnimation
            target: flick
            property: "contentY"
            duration: 220
            easing.type: Easing.OutCubic
        }
    }
}
