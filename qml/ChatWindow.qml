import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Dialogs

Window {
    id: chatWindow
    width: 660
    height: 700
    minimumWidth: 520
    minimumHeight: 480
    visible: false
    title: "和 Stellacandie 聊天"
    color: "#FFF9F4"
    flags: Qt.Tool | Qt.WindowTitleHint | Qt.WindowCloseButtonHint

    required property Window petWindow
    signal openSettingsRequested()

    function toggleNearPet() {
        if (visible) { hide(); return }
        x = Math.min(Screen.width - width - 16, Math.max(16, petWindow.x - width - 12))
        y = Math.min(Screen.height - height - 16, Math.max(16, petWindow.y + petWindow.height - height))
        show(); raise(); requestActivate(); input.forceActiveFocus(); messageList.positionViewAtEnd()
    }
    function openNearPet() { if (visible) { raise(); requestActivate(); return } toggleNearPet() }

    FileDialog {
        id: imagePicker
        title: "选择要发给精灵的照片"
        fileMode: FileDialog.OpenFile
        nameFilters: ["图片文件 (*.jpg *.jpeg *.png *.webp)"]
        onAccepted: appController.prepareChatPhoto(selectedFile.toString())
    }

    Dialog {
        id: photoConfirm
        anchors.centerIn: parent
        width: Math.min(500, chatWindow.width - 50)
        modal: true
        title: "确认发送照片？"
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: { appController.sendChatPhoto(input.text.trim()); input.clear() }
        ColumnLayout {
            width: parent.width
            Label { Layout.fillWidth: true; text: "照片会先在本地缩小、重新编码并移除 EXIF 信息，再发送给硅基流动进行识别。"; wrapMode: Text.WordWrap; color: "#69494E" }
            Label { Layout.fillWidth: true; text: "原图不会写入聊天数据库；聊天记录只保存图片名称、你的附言和精灵的回复。"; wrapMode: Text.WordWrap; color: "#9A686F" }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#FFF9F4"
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                ColumnLayout {
                    spacing: 1
                    Label { text: "Stellacandie"; color: "#6B3E43"; font.pixelSize: 22; font.bold: true }
                    Label { text: "当前状态：" + appController.currentStateName; color: "#9A7778"; font.pixelSize: 12 }
                }
                Item { Layout.fillWidth: true }
                ToolButton {
                    text: appController.aiBusy || appController.visionBusy ? "正在思考…" : appController.chatRouteLabel
                    ToolTip.visible: hovered
                    ToolTip.text: appController.aiStatus + "\n点击切换：Agent主链路 / 旧链路 / 强制离线"
                    onClicked: {
                        if (appController.chatRouteMode === "agent_main") appController.setChatRouteMode("legacy")
                        else if (appController.chatRouteMode === "legacy") appController.setChatRouteMode("offline")
                        else appController.setChatRouteMode("agent_main")
                    }
                }
                ToolButton { text: "记忆"; Layout.preferredWidth: 45; onClicked: appController.openMemory() }
                ToolButton { text: "零食"; Layout.preferredWidth: 45; onClicked: appController.openFileSnack() }
                ToolButton { text: "魔法"; Layout.preferredWidth: 45; onClicked: appController.openSummaryMagic() }
                ToolButton { text: "日记"; Layout.preferredWidth: 45; onClicked: appController.openDiary() }
                ToolButton { text: "设置"; Layout.preferredWidth: 45; onClicked: chatWindow.openSettingsRequested() }
                ToolButton { text: "关闭"; Layout.preferredWidth: 45; onClicked: chatWindow.close() }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#E8D5CF" }

            ListView {
                id: messageList
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 8
                clip: true
                model: appController.chatModel
                delegate: Item {
                    required property string messageText
                    required property string timestamp
                    required property bool isUser
                    width: messageList.width
                    height: bubble.height + 4
                    Rectangle {
                        id: bubble
                        width: Math.min(messageList.width * 0.78, Math.max(150, bubbleText.implicitWidth + 28))
                        height: bubbleText.implicitHeight + 32
                        anchors.right: parent.isUser ? parent.right : undefined
                        anchors.left: parent.isUser ? undefined : parent.left
                        radius: 14
                        color: parent.isUser ? "#D98D8D" : "#F3E4DC"
                        Text { id: bubbleText; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 12; text: messageText; wrapMode: Text.Wrap; color: parent.parent.isUser ? "white" : "#563B3E"; font.pixelSize: 14 }
                        Text { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 7; text: timestamp; color: parent.parent.isUser ? "#FFF0F0" : "#A68984"; font.pixelSize: 9 }
                    }
                }
                onCountChanged: Qt.callLater(positionViewAtEnd)
                ScrollBar.vertical: ScrollBar {}
            }

            Frame {
                Layout.fillWidth: true
                visible: appController.hasPendingVisionPhoto
                background: Rectangle { radius: 12; color: "#FFF3EF"; border.color: "#E5C6C3" }
                RowLayout {
                    anchors.fill: parent
                    Image { Layout.preferredWidth: 92; Layout.preferredHeight: 72; source: appController.visionPhotoUrl; fillMode: Image.PreserveAspectCrop; smooth: true }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; text: appController.visionPhotoName; elide: Text.ElideMiddle; font.bold: true; color: "#6B3E43" }
                        Label { Layout.fillWidth: true; text: appController.visionPhotoStatus; wrapMode: Text.WordWrap; color: "#9A686F"; font.pixelSize: 12 }
                    }
                    Button { text: "移除"; onClicked: appController.clearDreamPhoto() }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: Math.max(58, input.implicitHeight + 20)
                radius: 16
                color: "white"
                border.color: input.activeFocus ? "#C9797E" : "#E5D2CC"
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 7
                    Button { text: "＋图片"; flat: true; enabled: appController.visionRecognitionEnabled && !appController.visionBusy && !appController.aiBusy; onClicked: imagePicker.open() }
                    TextArea {
                        id: input
                        Layout.fillWidth: true
                        placeholderText: appController.hasPendingVisionPhoto ? "可以补一句想让她看的内容……" : "讲讲今天发生的事……"
                        wrapMode: TextEdit.Wrap
                        background: null
                        selectByMouse: true
                        Keys.onReturnPressed: function(event) {
                            if ((event.modifiers & Qt.ShiftModifier) !== 0) event.accepted = false
                            else { sendButton.send(); event.accepted = true }
                        }
                    }
                    Button {
                        id: sendButton
                        text: appController.visionBusy ? "看图中…" : "发送"
                        enabled: (input.text.trim().length > 0 || appController.hasPendingVisionPhoto) && !appController.aiBusy && !appController.visionBusy
                        function send() {
                            if (appController.hasPendingVisionPhoto) { photoConfirm.open(); return }
                            const content = input.text.trim()
                            if (content.length === 0) return
                            appController.sendMessage(content); input.clear()
                        }
                        onClicked: send()
                    }
                }
            }
        }
    }

}
