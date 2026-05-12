import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: nav
    color: "#111111"

    required property int currentIndex
    signal pageRequested(int index)

    readonly property var pages: [
        { icon: "⊞", label: qsTr("Dashboard") },
        { icon: "⬡", label: qsTr("Agents")    },
        { icon: "💬", label: qsTr("Chat")      },
        { icon: "🔧", label: qsTr("Tools")     },
        { icon: "⚙", label: qsTr("Settings")  },
    ]

    ColumnLayout {
        anchors { top: parent.top; left: parent.left; right: parent.right }
        spacing: 0

        // Logo / title
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 64

            Text {
                anchors.centerIn: parent
                text: "NeurX"
                font { family: "Inter"; pixelSize: 22; weight: Font.Bold }
                color: "#6c63ff"
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: "#2a2a2a" }

        // Nav items
        Repeater {
            model: nav.pages.length
            delegate: NavItem {
                required property int index
                readonly property var page: nav.pages[index]
                Layout.fillWidth: true
                icon:    page.icon
                label:   page.label
                active:  index === nav.currentIndex
                onClicked: nav.pageRequested(index)
            }
        }
    }

    // Runtime status at bottom
    Item {
        anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
        height: 48

        RowLayout {
            anchors { fill: parent; margins: 16 }
            spacing: 8

            StatusDot {
                active: Runtime.running
            }
            Text {
                text: Runtime.running ? qsTr("Runtime active") : qsTr("Runtime stopped")
                color: "#9a9a9a"
                font { family: "Inter"; pixelSize: 12 }
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }
    }

    // Inline NavItem component
    component NavItem: Rectangle {
        id: item
        required property string icon
        required property string label
        required property bool   active
        signal clicked()

        height: 48
        color: active ? "#1e1e2e"
             : hovered ? "#181818" : "transparent"
        radius: 0

        property bool hovered: false

        Rectangle {
            width: 3; height: parent.height
            color: "#6c63ff"
            visible: item.active
            anchors.left: parent.left
        }

        RowLayout {
            anchors { fill: parent; leftMargin: 20; rightMargin: 12 }
            spacing: 12

            Text {
                text: item.icon
                font.pixelSize: 18
                color: item.active ? "#6c63ff" : "#9a9a9a"
            }
            Text {
                text: item.label
                font { family: "Inter"; pixelSize: 14 }
                color: item.active ? "#f0f0f0" : "#9a9a9a"
                Layout.fillWidth: true
            }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onEntered: item.hovered = true
            onExited:  item.hovered = false
            onClicked: item.clicked()
        }
    }
}
