import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    visible: false
    width: screen.width
    height: screen.height
    title: "Linux Start Menu Clone"
    color: "transparent"

    // =========================
    // CLOSE ANIMATION FUNCTION
    // =========================
    function closeWindow() {
        // Slide down
        slideDown.start()
        // Fade out
        fadeOut.start()
    }

    MouseArea {
        anchors.fill: parent
        onClicked: closeWindow()
    }

    // =========================
    // WRAPPER RECTANGLE WITH ANIMATION
    // =========================
    Rectangle {
        id: contentWrapper
        width: 800
        height: 800
        color: "#1E1E1E"
        y: window.height       // start below window
        opacity: 0            // start invisible

        RowLayout {
            id: mainLayout
            anchors.fill: parent
            spacing: 0

            //------------------
            // LEFT SIDEBAR
            //------------------
            Rectangle {
                id: sidebar
                width: 50
                Layout.fillHeight: true
                color: "grey"

                MouseArea {
                    anchors.fill: parent
                    onClicked: closeWindow()
                }

                ColumnLayout {
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.margins: 5
                    spacing: 10

                    ToolButton {
                        icon.source: "icons/user.svg"
                        Layout.fillWidth: true
                        ToolTip.text: "User"
                    }

                    ToolButton {
                        icon.source: "icons/settings.svg"
                        Layout.fillWidth: true
                        ToolTip.text: "Settings"
                    }

                    ToolButton {
                        icon.source: "icons/power.svg"
                        Layout.fillWidth: true
                        ToolTip.text: "Power"
                    }
                }
            }

            //------------------
            // MIDDLE: APP LIST
            //------------------
            Rectangle {
                width: 400
                height: 800
                color: "#1E1E1E"

                ListView {
                    id: appListView
                    anchors.fill: parent
                    model: appModel
                    keyNavigationEnabled: true
                    keyNavigationWraps : true
                    highlightFollowsCurrentItem: true
                    clip: true
                    focus: true   // required for keyboard navigation
                    interactive: true

                    // 🚫 Disable bounce / overshoot
                    boundsBehavior: Flickable.StopAtBounds
                    flickableDirection: Flickable.VerticalFlick

                    // (optional) also disable overshoot glow effect
                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AlwaysOn
                        interactive: true
                    }

                    delegate: Column {
                        width: appListView.width

                        // header section
                        Rectangle {
                            width: parent.width
                            height: 70
                            color: "#2D2D30"
                            visible: model.headerVisible

                            Text {
                                anchors.left: parent.left
                                anchors.bottom: parent.bottom
                                text: model.letter
                                color: "#CCCCCC"
                                font.pointSize: 30
                            }
                        }

                        // app item section
                        Rectangle {
                            id: appRect
                            width: parent.width
                            height: 40
                            radius: 0
                            property bool hovered: false

                            color: (ListView.isCurrentItem || hovered) ? "#0078D7" : "#2D2D30"

                            Row {
                                anchors.fill: parent
                                anchors.margins: 5
                                spacing: 10

                                Image {
                                    id: appIcon
                                    source: icon
                                    width: 32
                                    height: 32
                                    fillMode: Image.PreserveAspectFit
                                }

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: model.name
                                    color: "white"
                                    font.pointSize: 12
                                }
                            }

                            MouseArea {
                                id: mouseArea
                                anchors.fill: parent
                                hoverEnabled: true

                                property bool dragStarted: false
                                property point pressPos

                                onEntered: appRect.hovered = true
                                onExited: appRect.hovered = false

                                onPressed: function(mouse) {
                                    dragStarted = false
                                    pressPos = Qt.point(mouse.x, mouse.y)
                                }

                                onPositionChanged: function(mouse) {
                                    if (!dragStarted) {
                                        let dx = mouse.x - pressPos.x
                                        let dy = mouse.y - pressPos.y
                                        if (Math.sqrt(dx*dx + dy*dy) > 10) {
                                            dragStarted = true
                                            AppLauncher.startSystemDrag(model.desktopFilePath, appIcon)
                                        }
                                    }
                                }

                                onReleased: dragStarted = false

                                onClicked: {
                                    appListView.currentIndex = index
                                    AppLauncher.launchApp(model.command)
                                    closeWindow()
                                }

                                onPressAndHold: AppLauncher.startSystemDrag(model.desktopFilePath, appIcon)
                            }
                        }
                    }

                    // handle Enter / Return key to launch selected app
                    Keys.onReturnPressed: {
                        if (currentIndex >= 0 && currentItem) {
                            AppLauncher.launchApp(currentItem.command)
                            // closeWindow()
                        }
                    }
                    WheelHandler {
                        id: wheelHandler
                        target: appListView
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                        property real speedFactor: 2.0

                        onWheel: (event) => {
                            appListView.contentY -= event.angleDelta.y * speedFactor
                            event.accepted = true
                        }
                    }

                }

            }



            //------------------
            // RIGHT: TILE AREA
            //------------------
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Rectangle {
                    id: container
                    width: parent.width
                    height: Math.max(parent.height, 800)
                    color: "transparent"

                    property int gridSize: 100
                    property int halfGrid: gridSize / 2
                    property int cols: Math.floor(width / halfGrid)

                    DropArea {
                        anchors.fill: parent
                        onDropped: (drop) => {
                            if (drop.hasUrls) {
                                for (let i = 0; i < drop.urls.length; ++i) {
                                    const url = drop.urls[i];
                                    if (url.toString().endsWith(".desktop")) {
                                        const localPath = url.toString().replace("file://", "");
                                        tileModel.addTileFromDesktopFile(localPath, drop.x, drop.y);
                                    }
                                }
                            }
                        }
                    }

                    Repeater {
                        model: tileModel

                        Rectangle {
                            id: tile
                            width: size === "small" ? container.halfGrid - 5
                            : size === "medium" ? container.halfGrid * 2 - 5
                            : container.halfGrid * 4 - 5
                            height: size === "small" ? container.halfGrid - 5
                            : size === "medium" ? container.halfGrid * 2 - 5
                            : container.halfGrid * 2 - 5

                            x: model.x
                            y: model.y
                            color: "#0078D7"
                            border.color: Qt.rgba(1,1,1,0.2)
                            border.width: 1

                            Text {
                                anchors.left: parent.left
                                anchors.bottom: parent.bottom
                                anchors.margins: 4
                                text: model.name
                                color: "white"
                                font.pointSize: size === "small" ? 1 : 12
                                wrapMode: Text.Wrap
                                width: parent.width - 10
                            }

                            Image {
                                anchors.centerIn: parent
                                width: parent.height / 2
                                height: width
                                fillMode: Image.PreserveAspectFit
                                source: AppLauncher.resolveIcon(model.icon)
                            }

                            MouseArea {
                                id: dragArea
                                anchors.fill: parent
                                drag.target: tile
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                cursorShape: Qt.OpenHandCursor
                                hoverEnabled: true

                                property bool dragging: false

                                onPressed: function(mouse) {
                                    if(mouse.button === Qt.LeftButton) {
                                        dragging = true
                                        dragArea.cursorShape = Qt.ClosedHandCursor
                                    }
                                }

                                onReleased: {
                                    if(mouse.button === Qt.LeftButton && dragging) {
                                        dragging = false
                                        dragArea.cursorShape = Qt.OpenHandCursor

                                        let snappedX = Math.round(tile.x / container.halfGrid) * container.halfGrid;
                                        let snappedY = Math.round(tile.y / container.halfGrid) * container.halfGrid;

                                        if (snappedX < 0) snappedX = 0;
                                        if (snappedY < 0) snappedY = 0;
                                        if (snappedX + tile.width > container.width)
                                            snappedX = container.width - tile.width;

                                        tile.x = snappedX
                                        tile.y = snappedY

                                        tileModel.updateTilePosition(model.index, tile.x, tile.y)
                                    }
                                }

                                onPositionChanged: {
                                    // optional: while dragging you can change color
                                    if(dragging) tile.color = "#005A9E"
                                }

                                onClicked: {
                                    if (mouse.button === Qt.LeftButton) {
                                        if (model.command && model.command.length > 0) {
                                            AppLauncher.launchApp(model.command)  // launch the app
                                            closeWindow() // then quit this QML app
                                        } else {
                                            console.warn("No command found for this tile")
                                        }
                                    } else if (mouse.button === Qt.RightButton) {
                                        contextMenu.open()
                                    }
                                }


                                onEntered: if(!dragging) tile.color = "#005A9E"
                                onExited: if(!dragging) tile.color = "#0078D7"
                            }


                            Menu {
                                id: contextMenu
                                MenuItem { text: "Small"; onTriggered: tileModel.resizeTile(model.index, "small") }
                                MenuItem { text: "Medium"; onTriggered: tileModel.resizeTile(model.index, "medium") }
                                MenuItem { text: "Large"; onTriggered: tileModel.resizeTile(model.index, "large") }
                                MenuSeparator {}
                                MenuItem { text: "Remove"; onTriggered: tileModel.removeTile(model.index) }
                            }

                            property string size: model.size
                        }
                    }
                }
            }
        }

        // =========================
        // STARTUP ANIMATION
        // =========================
        PropertyAnimation {
            id: slideUp
            target: contentWrapper
            property: "y"
            from: contentWrapper.height
            to: screen.height - contentWrapper.height
            duration: 300
            easing.type: Easing.Linear
        }

        PropertyAnimation {
            id: fadeIn
            target: contentWrapper
            property: "opacity"
            from: 0
            to: 1
            duration: 300
            easing.type: Easing.OutCubic
        }



        // =========================
        // SLIDE DOWN ANIMATION
        // =========================
        PropertyAnimation {
            id: slideDown
            target: contentWrapper
            property: "y"
            from: screen.height - contentWrapper.height
            to: contentWrapper.height
            duration: 300
            easing.type: Easing.Linear
            onFinished: window.close()  // actually close the window when animation ends
        }

        // =========================
        // FADE OUT ANIMATION
        // =========================
        PropertyAnimation {
            id: fadeOut
            target: contentWrapper
            property: "opacity"
            from: 1
            to: 0
            duration: 300
            easing.type: Easing.InCubic
        }


        Component.onCompleted: {
            slideUp.start()
            fadeIn.start()
        }
    }
}
