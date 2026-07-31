import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: diaryWindow
    width: 920
    height: 670
    minimumWidth: 720
    minimumHeight: 520
    visible: false
    title: "Stellacandie 的反向日记"
    color: "#EEDFD2"
    flags: Qt.Tool | Qt.WindowTitleHint | Qt.WindowCloseButtonHint

    property int pageIndex: 0
    function openAndRaise() {
        if (appController.diaryCount > 0) selectPage(Math.min(pageIndex, appController.diaryCount - 1))
        show(); raise(); requestActivate()
    }
    function selectPage(index) {
        if (appController.diaryCount <= 0) return
        pageIndex = Math.max(0, Math.min(index, appController.diaryCount - 1))
        appController.selectDiary(pageIndex)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Stellacandie 的日记本"
                color: "#693F46"
                font.pixelSize: 24
                font.bold: true
            }
            Item { Layout.fillWidth: true }
            Switch {
                text: "日记模块"
                checked: appController.reverseDiaryEnabled
                onToggled: appController.setReverseDiaryEnabled(checked)
            }
            Button {
                text: appController.reverseDiaryGenerating ? "正在写……" : "写今天的日记"
                enabled: appController.reverseDiaryEnabled && !appController.reverseDiaryGenerating && !appController.aiBusy
                highlighted: true
                onClicked: appController.generateDiary()
            }
            Button { text: "关闭 ✕"; onClicked: diaryWindow.hide() }
        }

        Rectangle {
            id: cover
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 2
            radius: 20
            color: "#8E5F5B"
            border.color: "#704743"
            border.width: 2

            Rectangle {
                id: paper
                anchors.fill: parent
                anchors.margins: 14
                radius: 12
                color: "#FFFDF3"
                border.color: "#D8C29F"

                // 正文完成排版后得到实际文字底边，贴纸只能使用底边之后的空白区域。
                property real writtenBottom: contentColumn.y + diaryText.y
                                             + Math.min(diaryText.contentHeight, diaryText.height)
                property real stickerBandTop: Math.min(height - 96,
                    Math.max(writtenBottom + 22, height * 0.55))

                Rectangle {
                    width: 7; height: parent.height - 24
                    anchors.left: parent.left; anchors.leftMargin: 24
                    anchors.verticalCenter: parent.verticalCenter
                    radius: 3
                    color: "#E3CEAC"
                    opacity: 0.72
                }

                Column {
                    id: contentColumn
                    anchors.fill: parent
                    // 两侧为贴纸保留安全区域，正文永远不会伸进贴纸栏。
                    anchors.leftMargin: 154
                    anchors.rightMargin: 154
                    anchors.topMargin: 28
                    anchors.bottomMargin: 38
                    spacing: 8

                    Label {
                        width: parent.width
                        text: appController.selectedDiaryDate || "还没有日记"
                        horizontalAlignment: Text.AlignHCenter
                        color: "#6B4046"
                        font.pixelSize: 23
                        font.bold: true
                    }
                    Label {
                        width: parent.width
                        text: appController.selectedDiaryUpdatedAt ? "最后落笔  " + appController.selectedDiaryUpdatedAt : ""
                        horizontalAlignment: Text.AlignHCenter
                        color: "#A48178"
                        font.pixelSize: 11
                    }
                    TextArea {
                        id: diaryText
                        width: parent.width
                        height: parent.height - 76
                        text: appController.selectedDiaryContent || "今天还是一张空白页。\n\n先和 Stellacandie 分享一点事情，再让她写下今天吧。"
                        readOnly: true
                        wrapMode: Text.Wrap
                        selectByMouse: true
                        color: "#584347"
                        font.pixelSize: 17
                        font.family: "KaiTi, Microsoft YaHei UI"
                        background: null
                        padding: 12
                    }
                }

                Repeater {
                    model: appController.selectedDiaryStickers
                    delegate: Item {
                        required property int index
                        required property string modelData
                        property var parts: modelData.split("\u001f")
                        property int stickerCount: Math.max(1, appController.selectedDiaryStickers.length)
                        property bool hasBottomBand: paper.height - paper.writtenBottom >= 105
                        // 优先在正文下方的空白带均匀排开；正文过长时才退到左右页边。
                        x: hasBottomBand
                           ? 88 + index * Math.max(92, (paper.width - 176 - width) / Math.max(1, stickerCount - 1))
                           : (index % 2 === 0 ? 28 : paper.width - width - 28)
                        y: hasBottomBand
                           ? paper.stickerBandTop
                           : 110 + Math.floor(index / 2) * 112
                        width: 76
                        height: 76
                        rotation: Number(parts[4])
                        z: 3

                        Rectangle {
                            anchors.centerIn: parent
                            width: 66; height: 66; radius: 33
                            color: ["#F7DCCF", "#E5EED8", "#F5E7BD", "#DDDDF0"][index % 4]
                            opacity: 0.72
                            Rectangle {
                                anchors.centerIn: parent
                                width: 54; height: 54; radius: 27
                                color: "#FFF9EE"
                                opacity: 0.68
                            }
                        }
                        Column {
                            anchors.centerIn: parent; spacing: 0
                            Label { anchors.horizontalCenter: parent.horizontalCenter; text: parts[0]; font.pixelSize: 27 }
                            Label { anchors.horizontalCenter: parent.horizontalCenter; text: parts[1]; color: "#806861"; font.pixelSize: 9 }
                        }
                    }
                }

                Label {
                    anchors.right: parent.right; anchors.bottom: parent.bottom
                    anchors.margins: 16
                    text: appController.diaryCount > 0 ? (diaryWindow.pageIndex + 1) + " / " + appController.diaryCount : "0 / 0"
                    color: "#A68C7D"
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Button {
                text: "← 上一页"
                enabled: appController.diaryCount > 0 && diaryWindow.pageIndex < appController.diaryCount - 1
                onClicked: diaryWindow.selectPage(diaryWindow.pageIndex + 1)
            }
            Item { Layout.fillWidth: true }
            Label { text: appController.aiStatus; color: "#875B5F"; elide: Text.ElideRight; Layout.maximumWidth: 480 }
            Item { Layout.fillWidth: true }
            Button {
                text: "下一页 →"
                enabled: appController.diaryCount > 0 && diaryWindow.pageIndex > 0
                onClicked: diaryWindow.selectPage(diaryWindow.pageIndex - 1)
            }
        }
    }
}
