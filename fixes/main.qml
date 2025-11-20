import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: root
    width: Screen.width
    height: 50
    visible: false
    color: "#202225"
    title: "Bottom Panel"

    property int startIndex: -1

    Rectangle {
        id: startButton
        anchors.left: parent.left
        height: parent.height
        width: parent.height
        color: "transparent"
        Image {
            anchors.fill: parent
            anchors.margins: 10
            source: "start.svg"
            smooth: true
        }
        MouseArea {
            anchors.fill: parent
            onClicked: {
                console.log("Launching Win10Menu terminal")
                appModel.toggleWin10Menu()
            }
        }
    }

    Rectangle {
        id: searchBar
        anchors.left: startButton.right
        height: parent.height
        width: 400
        color: "#402225"

        TextField {
            anchors.fill: parent
            anchors.margins: 8
            placeholderText: "Search"
            font.pixelSize: 16
            color: "white"
            background: Rectangle { color: "transparent" }
        }
    }

    // =======================
    //        DOCK
    // =======================
    Rectangle {
        id: dock
        anchors.left: searchBar.right
        // anchors.right: runningApps.left
        // anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 200
        height: parent.height
        width: appModel.count * (iconSize + spacing)
        color: "transparent"
        property int iconSize: 50
        property int spacing: 8

        ListView {
            id: dockView
            anchors.fill: parent
            orientation: ListView.Horizontal
            model: appModel
            spacing: dock.spacing
            clip: true
            interactive: false // we handle drag manually

            delegate: Item {
                id: iconItem
                anchors.verticalCenter: parent.verticalCenter
                width: dock.iconSize
                height: dock.iconSize/1.2
                property int startIndex: -1

                Rectangle {
                    anchors.fill: parent

                    color: mouseArea.containsMouse ? "#303030" : "transparent"
                    radius: 6
                }

                Image {
                    anchors.fill: parent
                    anchors.margins: 6
                    source: icon !== "" ? icon : "qrc:/icons/placeholder.svg"
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                }

                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    drag.target: iconItem
                    acceptedButtons: Qt.LeftButton | Qt.RightButton

                    property real dragStartX: 0

                    onPressed: {
                        console.log("[MouseArea] onPressed — button:", mouse.button)

                        if (mouse.button === Qt.RightButton) {
                            console.log("[MouseArea] Right-click detected on index:", index)

                            // IMPORTANT: disable drag
                            drag.target = null

                            appModel.removeApp(index)
                            return
                        }

                        console.log("[MouseArea] Left button pressed, starting drag")
                        drag.target = iconItem
                        iconItem.startIndex = index
                        dragStartX = mouse.x
                    }

                    onReleased: {
                        console.log("=== onReleased triggered ===")

                        var globalPos = iconItem.mapToItem(dockView.contentItem, iconItem.x + dock.spacing, 0)
                        var itemWidth = dock.iconSize + dock.spacing
                        var centerX = globalPos.x + itemWidth / 2
                        var targetIndex = Math.floor(centerX / (itemWidth * 2))
                        targetIndex = Math.max(0, Math.min(appModel.count - 1, targetIndex))

                        console.log("[MouseArea] Released — start:", iconItem.startIndex,
                                    "-> target:", targetIndex)

                        if (targetIndex !== iconItem.startIndex) {
                            console.log("[MouseArea] Moving app")
                            appModel.moveApp(iconItem.startIndex, targetIndex)
                        } else {
                            console.log("[MouseArea] No move needed")
                        }
                    }

                    onClicked: {
                        console.log("[MouseArea] onClicked — button:", mouse.button)

                        if (mouse.button === Qt.LeftButton) {
                            console.log("[MouseArea] Launch app index:", index)
                            appModel.launchApp(index)
                        }
                    }
                }


                Behavior on x {
                    NumberAnimation { duration: 150; easing.type: Easing.OutQuad }
                }
            }

            displaced: Transition {
                NumberAnimation { properties: "x,y"; duration: 150; easing.type: Easing.OutQuad }
            }
        }

        // Drop new apps
        DropArea {
            anchors.fill: parent
            onDropped: (drop) => {
                if (drop.hasUrls && drop.urls.length > 0) {
                    var path = drop.urls[0].toString().replace("file://", "")
                    appModel.addDesktopFile(path)
                }
            }
        }
    }

    // =======================
    //      RUNNING WINDOWS
    // =======================
    Rectangle {
        id: runningApps
        anchors.left: dock.right
        anchors.right: clock.left
        height: parent.height
        color: "transparent"
        property int iconSize: 50
        property int spacing: 8

        ListView {
            id: winView
            anchors.fill: parent
            orientation: ListView.Horizontal
            spacing: runningApps.spacing
            clip: true
            model: windowModel

            delegate: Rectangle {
                width: runningApps.iconSize
                height: winView.height
                radius: 8
                color: focused ? "#404040" : (hovered ? "#303030" : "transparent")

                property bool hovered: false

                Rectangle {
                    id: focusIndicator
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: focused ? 40 : 5
                    height: 2
                    color: "white"

                    // Add a smooth animation for width changes
                    Behavior on width {
                        NumberAnimation {
                            duration: 200
                            easing.type: Easing.InOutQuad
                        }
                    }
                }


                Column {
                    anchors.centerIn: parent
                    spacing: 2

                    Image {
                        property string cleanedIcon: model.icon.startsWith("qrc:/") ? model.icon.slice(4) : model.icon


                        id: appIcon
                        width: runningApps.iconSize * 0.6
                        height: width
                        source: cleanedIcon.startsWith("/") ? "file:" + cleanedIcon : cleanedIcon
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                    }

                    // Optional: show title when focused
                    // Text {
                    //     text: title
                    //     color: focused ? "#ffffff" : "#aaaaaa"
                    //     font.pixelSize: 10
                    //     elide: Text.ElideRight
                    //     horizontalAlignment: Text.AlignHCenter
                    //     width: runningApps.iconSize
                    // }
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton

                    onEntered: parent.hovered = true
                    onExited: parent.hovered = false

                    onClicked: (mouse) => {
                        if (mouse.button === Qt.RightButton) {
                            console.log("[QML] Right-click on", title)
                            windowModel.close(title)
                        } else if (mouse.button === Qt.LeftButton) {
                            console.log("[QML] Left-click on", title)
                            windowModel.activate(index)
                        }
                        windowModel.refresh()
                    }
                }

            }

            displaced: Transition {
                NumberAnimation { properties: "x,y"; duration: 120; easing.type: Easing.OutQuad }
            }
        }
    }

    // ======================
    //        tray
    //=======================

    Rectangle {
        id: trayrec
        anchors.right: clock.left
        height: parent.height
        width: 110
        color: "transparent"
        Row {
            id: trayView
            anchors.centerIn: parent
            spacing: 6

            Rectangle {
                width: 30
                height: 30
                color: "transparent"
                Image {
                    anchors.fill: parent
                    anchors.margins: 4
                    sourceSize: Qt.size(parent.width, parent.width)
                    source: "file://usr/share/icons/Windows10/scalable/status/audio-volume-high-symbolic.svg"
                }
                MouseArea {
                    anchors.fill: parent
                    onWheel: {
                        if (wheel.angleDelta.y > 0) {
                            osdController.volUp(); // Wheel up -> Volume Up
                        } else {
                            osdController.volDown(); // Wheel down -> Volume Down
                        }
                    }
                }
            }
            Rectangle {
                width: 30
                height: 30
                color: "transparent"
                Image {
                    anchors.fill: parent
                    anchors.margins: 4
                    sourceSize: Qt.size(parent.width, parent.width)
                    source: "file://usr/share/icons/Windows10/scalable/status/display-brightness-symbolic.svg"
                }
                MouseArea {
                    anchors.fill: parent
                    onWheel: {
                        if (wheel.angleDelta.y > 0) {
                            osdController.dispUp(); // Wheel up -> Brightness Up
                        } else {
                            osdController.dispDown(); // Wheel down -> Brightness Down
                        }
                    }
                }
            }
            Rectangle {
                width: 30
                height: 30
                color: "transparent"
                Image {
                    anchors.fill: parent
                    anchors.margins: 1
                    sourceSize: Qt.size(parent.width, parent.width)
                    source: "file://usr/share/icons/Windows10/scalable/status/bluetooth-active-symbolic.svg"
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        console.log("Launching blueman")
                        appModel.toggleblueman()
                    }
                }
            }
            Rectangle {
                width: 30
                height: 30
                color: "transparent"
                Image {
                    anchors.fill: parent
                    anchors.margins: 4
                    sourceSize: Qt.size(parent.width, parent.width)
                    source: "file://usr/share/icons/Windows10/scalable/status/nm-signal-100.svg"
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        console.log("Launching nmqt")
                        appModel.togglenmqt()
                    }
                }
            }

        }

    }

    // =======================
    //        CLOCK
    // =======================
    Rectangle {
        id: clock
        width: dateTimeText.width + 30
        anchors.right: parent.right
        height: parent.height
        color: "transparent"

        Text {
            id: dateTimeText
            anchors.centerIn: parent
            color: "#ffffff"
            font.pixelSize: parent.height * 0.3
            text: Qt.formatDateTime(new Date(), "hh:mm:ss\ndddd, MMM d")
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            // wrapMode: Text.Wrap
        }

        Timer {
            interval: 1000
            running: true
            repeat: true
            onTriggered: dateTimeText.text =
            Qt.formatDateTime(new Date(), "hh:mm:ss\ndddd, MMM d")
        }
    }
}
