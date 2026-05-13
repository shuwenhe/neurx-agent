pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: nav
    color: "#181818"

    required property int currentIndex
    property string explorerRoot: "/Users/feifei"
    signal pageRequested(int index)

    readonly property var pages: [
        { icon: "⌂", label: qsTr("Dashboard") },
        { icon: "◌", label: qsTr("Agents") },
        { icon: "◫", label: qsTr("Chat") },
        { icon: "◇", label: qsTr("Tools") },
        { icon: "⚙", label: qsTr("Settings") },
    ]

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 48
            color: "#181818"
            border.color: "#2a2a2a"

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                ActivityButton {
                    icon: "▱"
                    active: true
                    tooltip: qsTr("Explorer")
                }

                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#2a2a2a" }

                Repeater {
                    model: nav.pages.length
                    delegate: ActivityButton {
                        required property int index
                        readonly property var page: nav.pages[index]
                        icon: page.icon
                        active: index === nav.currentIndex
                        tooltip: page.label
                        onClicked: nav.pageRequested(index)
                    }
                }

                Item { Layout.fillHeight: true }

                ActivityButton {
                    icon: "◉"
                    active: Runtime.running
                    tooltip: Runtime.running ? qsTr("Runtime active") : qsTr("Runtime stopped")
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#111111"
            border.color: "#2a2a2a"

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    spacing: 6

                    Text {
                        Layout.leftMargin: 20
                        Layout.fillWidth: true
                        text: qsTr("EXPLORER")
                        color: "#cccccc"
                        font { pixelSize: 11; capitalization: Font.AllUppercase }
                        elide: Text.ElideRight
                    }

                    Text {
                        Layout.rightMargin: 12
                        text: "..."
                        color: "#858585"
                        font.pixelSize: 14
                    }
                }

                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#252525" }

                Flickable {
                    id: explorerScroll
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    contentWidth: Math.max(width, explorerColumn.implicitWidth)
                    contentHeight: explorerColumn.implicitHeight
                    boundsBehavior: Flickable.StopAtBounds

                    Column {
                        id: explorerColumn
                        width: Math.max(explorerScroll.width, implicitWidth)

                        ExplorerNode {
                            name: "FEIFEI"
                            path: nav.explorerRoot
                            isDir: true
                            depth: 0
                            expanded: true
                        }
                    }
                }
            }
        }
    }

    component ActivityButton: Rectangle {
        id: activity
        required property string icon
        property bool active: false
        property string tooltip: ""
        signal clicked()

        Layout.fillWidth: true
        Layout.preferredHeight: 48
        color: mouse.containsMouse ? "#202020" : "transparent"

        Rectangle {
            width: 2
            height: 28
            radius: 1
            color: "#ffffff"
            visible: activity.active
            anchors { left: parent.left; verticalCenter: parent.verticalCenter }
        }

        Text {
            anchors.centerIn: parent
            text: activity.icon
            color: activity.active ? "#ffffff" : "#858585"
            font.pixelSize: 20
        }

        ToolTip.visible: mouse.containsMouse && activity.tooltip.length > 0
        ToolTip.text: activity.tooltip
        ToolTip.delay: 500

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: activity.clicked()
        }
    }
}
