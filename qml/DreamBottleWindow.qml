import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Dialogs

Window {
    id: win
    width: 1080
    height: 740
    minimumWidth: 920
    minimumHeight: 640
    visible: false
    color: "#FFF9F5"
    title: "梦境星星瓶"
    flags: Qt.Tool | Qt.WindowTitleHint | Qt.WindowCloseButtonHint | Qt.WindowStaysOnTopHint

    function openAndRaise() { show(); raise(); requestActivate() }

    FileDialog {
        id: photoPicker
        title: "选择现实回声照片"
        fileMode: FileDialog.OpenFile
        nameFilters: ["图片文件 (*.jpg *.jpeg *.png *.webp)"]
        onAccepted: appController.prepareDreamPhoto(selectedFile.toString())
    }

    Dialog {
        id: sendConfirm
        anchors.centerIn: parent
        width: Math.min(520, win.width - 60)
        modal: true
        title: "确认发送这张照片？"
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: appController.analyzeDreamPhoto(echoArea.text)
        ColumnLayout {
            width: parent.width
            spacing: 9
            Label {
                Layout.fillWidth: true
                text: "确认后，程序会在本地缩小并重新编码照片、去除 EXIF 信息，再发送给硅基流动的视觉模型。"
                wrapMode: Text.WordWrap
                color: "#69494E"
            }
            Label {
                Layout.fillWidth: true
                text: "原图不会写入情绪精灵数据库；数据库只保存识别出的文字摘要和梦境关联。"
                wrapMode: Text.WordWrap
                color: "#9A686F"
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Label { text: "✦ 梦境星星瓶"; font.pixelSize: 24; font.bold: true; color: "#6B3E43" }
                Label { text: "打开与收藏是你的秘密操作，精灵看不见，也不会催你。"; color: "#987579" }
            }
            Item { Layout.fillWidth: true }
            Switch { text: "允许做梦"; checked: appController.dreamEnabled; onToggled: appController.setDreamEnabled(checked) }
            Button { text: appController.dreamBusy ? "正在回想…" : "检查今日星星"; enabled: !appController.dreamBusy; onClicked: appController.collectTodayDream() }
            Button { text: "关闭 ✕"; flat: true; onClicked: win.close() }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: "#E5D1CB" }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 14

            Frame {
                Layout.preferredWidth: 285
                Layout.fillHeight: true
                background: Rectangle { radius: 18; color: "#F5ECF4"; border.color: "#E5CFDB" }
                ColumnLayout {
                    anchors.fill: parent
                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: appController.unopenedDreamCount > 0 ? appController.unopenedDreamCount + " 颗星星还没有展开" : "瓶中的星光很安静"
                        color: "#805F63"; font.bold: true
                    }
                    Item {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        Repeater {
                            model: Math.min(30, appController.dreamItems.length)
                            Image {
                                required property int index
                                source: "qrc:/assets/dreams/paper/star_paper_folded_v1.png"
                                width: 22 + (index % 3) * 3; height: width
                                x: 60 + ((index * 47) % 125)
                                y: 245 - Math.floor(index / 5) * 27 + ((index * 19) % 14)
                                rotation: ((index * 37) % 50) - 25
                                opacity: 0.72 + (index % 3) * 0.1
                                smooth: true; mipmap: true
                            }
                        }
                        Image { anchors.fill: parent; anchors.margins: 8; source: "qrc:/assets/dreams/bottle/star_bottle_empty_v1.png"; fillMode: Image.PreserveAspectFit; smooth: true; mipmap: true }
                    }
                    Label { Layout.alignment: Qt.AlignHCenter; text: "共收藏 " + appController.dreamItems.length + " 个梦"; color: "#987579" }
                }
            }

            Frame {
                Layout.preferredWidth: 270
                Layout.fillHeight: true
                background: Rectangle { radius: 14; color: "#FFFCF9"; border.color: "#E8D8D1" }
                ColumnLayout {
                    anchors.fill: parent
                    Label { text: "瓶中的星星纸"; font.bold: true; color: "#704B50" }
                    ListView {
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 7
                        model: appController.dreamItems
                        delegate: Rectangle {
                            required property string modelData
                            required property int index
                            property var parts: modelData.split("|")
                            width: ListView.view.width; height: 68; radius: 11
                            color: parts[3] === "sealed" ? "#F7EAF1" : "#F8F0E8"
                            border.color: parts[4] === "favorite" ? "#D9A441" : "transparent"
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 9
                                Image { source: "qrc:/assets/dreams/paper/star_paper_folded_v1.png"; Layout.preferredWidth: 38; Layout.preferredHeight: 38; opacity: parts[3] === "sealed" ? 1 : 0.58 }
                                ColumnLayout {
                                    Layout.fillWidth: true; spacing: 2
                                    Label { Layout.fillWidth: true; elide: Text.ElideRight; text: parts[3] === "sealed" ? "未打开的星星纸" : parts[1]; font.bold: true; color: "#6B3E43" }
                                    Label { text: parts[0] + (parts[4] === "favorite" ? "  ·  已珍藏" : ""); color: "#9A777A" }
                                }
                                Button { text: parts[3] === "sealed" ? "打开" : "查看"; onClicked: appController.selectDream(index) }
                            }
                        }
                        Label { anchors.centerIn: parent; visible: parent.count === 0; text: "星星瓶还是空的\n明天早上再来看一眼吧"; horizontalAlignment: Text.AlignHCenter; color: "#9A777A" }
                        ScrollBar.vertical: ScrollBar {}
                    }
                }
            }

            Frame {
                Layout.fillWidth: true
                Layout.fillHeight: true
                background: Rectangle { radius: 16; color: appController.selectedDreamTitle.length ? Qt.lighter(appController.selectedDreamColor, 1.72) : "#FFFCF9"; border.color: "#E4CBCF" }
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8
                    RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            Label { text: appController.selectedDreamTitle.length ? "《" + appController.selectedDreamTitle + "》" : "选择一颗星星纸"; font.pixelSize: 21; font.bold: true; color: "#6B3E43" }
                            Label { text: appController.selectedDreamDate; color: "#987579" }
                        }
                        Item { Layout.fillWidth: true }
                        Button { visible: appController.selectedDreamTitle.length > 0; text: appController.selectedDreamFavorite ? "★ 已珍藏" : "☆ 珍藏"; onClicked: appController.toggleSelectedDreamFavorite() }
                    }
                    ScrollView {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        TextArea { readOnly: true; text: appController.selectedDreamContent; placeholderText: "打开星星纸后，梦会在这里慢慢展开。"; wrapMode: TextEdit.Wrap; selectByMouse: true; font.pixelSize: 16; background: null }
                    }
                    Label { Layout.fillWidth: true; visible: appController.selectedDreamSymbols.length > 0; text: "梦的残片：" + appController.selectedDreamSymbols; wrapMode: Text.WordWrap; color: "#805F63" }
                    Label { Layout.fillWidth: true; visible: appController.selectedDreamHint.length > 0; text: "现实微光：" + appController.selectedDreamHint; wrapMode: Text.WordWrap; color: "#9B6B76"; font.italic: true }
                    Rectangle { Layout.fillWidth: true; height: 1; visible: appController.selectedDreamTitle.length > 0; color: "#E5D1CB" }
                    Label { visible: appController.selectedDreamTitle.length > 0; text: "现实回声（只有主动送出后，精灵才会知道）"; font.bold: true; color: "#704B50" }
                    TextArea {
                        id: echoArea
                        Layout.fillWidth: true; Layout.preferredHeight: 65
                        visible: appController.selectedDreamTitle.length > 0
                        text: appController.selectedDreamEcho
                        placeholderText: "可写现实经历，或给照片补一句背景说明……"
                        wrapMode: TextEdit.Wrap
                        background: Rectangle { radius: 9; color: "#FFFCFA"; border.color: "#DDBFC2" }
                    }

                    Frame {
                        Layout.fillWidth: true
                        visible: appController.selectedDreamTitle.length > 0
                        background: Rectangle { radius: 11; color: "#FFF6F2"; border.color: "#E7CBC8" }
                        RowLayout {
                            anchors.fill: parent
                            spacing: 9
                            Image {
                                Layout.preferredWidth: 82; Layout.preferredHeight: 82
                                visible: appController.hasPendingVisionPhoto
                                source: appController.visionPhotoUrl
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                RowLayout {
                                    Layout.fillWidth: true
                                    Switch { text: "照片共鸣"; checked: appController.visionRecognitionEnabled; onToggled: appController.setVisionRecognitionEnabled(checked) }
                                    Item { Layout.fillWidth: true }
                                    Button { text: "视觉设置"; flat: true; onClicked: appController.openSettings() }
                                }
                                Label { Layout.fillWidth: true; text: appController.hasPendingVisionPhoto ? appController.visionPhotoName : "选择照片，让精灵看看现实是否回应了梦。"; elide: Text.ElideMiddle; color: "#76565A" }
                                Label { Layout.fillWidth: true; text: appController.visionPhotoStatus; wrapMode: Text.WordWrap; color: "#A06E73"; font.pixelSize: 12 }
                                Label { Layout.fillWidth: true; visible: appController.visionResultSummary.length > 0; text: appController.visionResultSummary; wrapMode: Text.WordWrap; color: "#76565A"; font.pixelSize: 12 }
                                RowLayout {
                                    Button { text: "选择照片"; enabled: appController.visionRecognitionEnabled && !appController.visionBusy; onClicked: photoPicker.open() }
                                    Button { text: "移除"; visible: appController.hasPendingVisionPhoto; onClicked: appController.clearDreamPhoto() }
                                    Item { Layout.fillWidth: true }
                                    Button {
                                        text: appController.visionBusy ? "本喵正在看…" : "识别并送出"
                                        highlighted: true
                                        enabled: appController.visionConfigured && appController.hasPendingVisionPhoto && !appController.visionBusy
                                        onClicked: sendConfirm.open()
                                    }
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        visible: appController.selectedDreamTitle.length > 0
                        Label { Layout.fillWidth: true; text: "也可以只送出文字，不需要照片。"; color: "#A08385" }
                        Button { text: "把文字回声送给她"; enabled: echoArea.text.trim().length > 0 && !appController.aiBusy; onClicked: appController.submitDreamRealityEcho(echoArea.text) }
                    }
                }
            }
        }
        Label { Layout.fillWidth: true; visible: appController.dreamStatus.length > 0; text: appController.dreamStatus; color: "#A35561"; wrapMode: Text.WordWrap }
    }
}
