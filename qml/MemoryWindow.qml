import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: win
    width: 760
    height: 580
    minimumWidth: 600
    minimumHeight: 440
    visible: false
    title: "Stellacandie 的长期记忆"
    color: "#FFF9F4"
    flags: Qt.Tool | Qt.WindowTitleHint | Qt.WindowCloseButtonHint

    function openAndRaise() { show(); raise(); requestActivate() }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Label { text: "长期记忆"; font.pixelSize: 22; font.bold: true; color: "#6B3E43" }
                Label { text: "精灵会保存人物、偏好、习惯、事件和未完故事"; color: "#987579" }
            }
            Item { Layout.fillWidth: true }
            Switch {
                text: "启用模块"
                checked: appController.longTermMemoryEnabled
                onToggled: appController.setLongTermMemoryEnabled(checked)
            }
            Button { text: "数据清理"; onClicked: appController.openDataCleanup() }
            Button { text: "关闭 ✕"; flat: true; onClicked: win.close() }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: "#E5D1CB" }

        ListView {
            id: memoryList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            model: appController.memoryItems

            delegate: Rectangle {
                id: card
                required property string modelData
                property var fields: modelData.split("\u001f")
                property string category: fields.length > 0 ? fields[0] : "记忆"
                property string subject: fields.length > 1 ? fields[1] : "未命名记忆"
                property string memoryContent: fields.length > 2 ? fields[2] : ""
                property int importance: fields.length > 3 ? Number(fields[3]) : 0
                property string nextQuestion: fields.length > 4 ? fields[4] : ""
                width: memoryList.width
                height: nextQuestion.length > 0 ? 118 : 98
                radius: 12
                color: "#F6E8E1"
                border.color: "#E7D1C9"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 5
                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: "[" + (card.category || "记忆") + "] " + (card.subject || "未命名记忆")
                            elide: Text.ElideRight
                            font.bold: true
                            color: "#6B3E43"
                        }
                        Label { text: "重要度 " + card.importance; color: "#A27B7E" }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: card.memoryContent
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        color: "#60474A"
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: card.nextQuestion.length > 0
                        text: "还想追问：" + card.nextQuestion
                        elide: Text.ElideRight
                        color: "#A36267"
                        font.pixelSize: 12
                    }
                }
            }
            ScrollBar.vertical: ScrollBar {}
        }

        Label {
            visible: memoryList.count === 0
            Layout.alignment: Qt.AlignHCenter
            text: "还没有长期记忆。继续聊天后，值得记住的事情会出现在这里。"
            color: "#9A777A"
        }
    }
}
