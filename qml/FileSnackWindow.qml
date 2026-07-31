import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Dialogs

Window {
    id: window
    width: 540; height: 700; minimumWidth: 500; minimumHeight: 620
    visible: false; title: "精灵零食工厂"; color: "#FFF9F4"
    flags: Qt.Tool | Qt.WindowTitleHint | Qt.WindowCloseButtonHint | Qt.WindowStaysOnTopHint
    function openAndRaise() { show(); raise(); requestActivate() }
    property string pendingAction: "eat"

    FileDialog {
        id: picker; title: "选择一个或多个确定不再需要的文件"; fileMode: FileDialog.OpenFiles
        onAccepted: appController.prepareFileSnacks(selectedFiles)
    }

    Dialog {
        id: confirmDialog; anchors.centerIn: parent; modal: true; width: 430
        title: pendingAction === "eat" ? "确认现在喂给精灵？" : "确认放入零食袋？"
        onOpened: strongCheck.checked = false
        contentItem: ColumnLayout {
            spacing: 12
            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: "所选原文件会分别移入 Windows 回收站，不会永久删除。\n\n本次：" + appController.snackFileName }
            Rectangle { Layout.fillWidth: true; height: 1; color: "#E8C9C2" }
            CheckBox {
                id: strongCheck; Layout.fillWidth: true; visible: appController.snackStrongConfirmationRequired
                text: "我确认这个近期／大型／可执行文件确实不再需要"
            }
            RowLayout {
                Layout.fillWidth: true; Item { Layout.fillWidth: true }
                Button { text: "取消"; onClicked: confirmDialog.close() }
                Button {
                    text: pendingAction === "eat" ? "移入回收站并喂食" : "加工并放入零食袋"
                    highlighted: true
                    enabled: !appController.snackStrongConfirmationRequired || strongCheck.checked
                    onClicked: {
                        confirmDialog.close()
                        if (pendingAction === "eat") appController.consumeFileSnack()
                        else appController.storeFileSnack()
                    }
                }
            }
        }
    }

    DropArea {
        anchors.fill: parent; enabled: appController.fileSnackEnabled
        onDropped: function(drop) { if (drop.hasUrls && drop.urls.length > 0) appController.prepareFileSnack(drop.urls[0].toString()) }
    }

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 18; spacing: 12
        RowLayout {
            Layout.fillWidth: true
            Label { text: "零食工厂"; font.pixelSize: 24; font.bold: true; color: "#6B3E43" }
            Item { Layout.fillWidth: true }
            Switch { text: "模块开启"; checked: appController.fileSnackEnabled; onToggled: appController.setFileSnackEnabled(checked) }
            Button { text: "关闭 ✕"; flat: true; onClicked: window.close() }
        }

        Label {
            Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#8D6B70"
            text: "可同时选择不同类型的文件触发融合配方，特殊组合还会解锁隐藏款。文件只会进入回收站。"
        }

        TabBar {
            id: tabs; Layout.fillWidth: true
            TabButton { text: "加工" }
            TabButton { text: "零食袋  " + appController.snackBagItems.length }
            TabButton { text: "口味图鉴" }
            TabButton { text: "记录" }
        }

        StackLayout {
            currentIndex: tabs.currentIndex; Layout.fillWidth: true; Layout.fillHeight: true

            Item {
                ColumnLayout {
                    anchors.fill: parent; spacing: 12
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 170; radius: 18
                        color: "#F7E8E2"; border.color: "#DCAAAA"; border.width: 2
                        Column {
                            anchors.centerIn: parent; width: parent.width - 35; spacing: 8
                            Label { anchors.horizontalCenter: parent.horizontalCenter; text: appController.hasPendingSnack ? appController.snackEmoji : "📄"; font.pixelSize: 48 }
                            Label { anchors.horizontalCenter: parent.horizontalCenter; font.bold: true; color: "#704B50"; text: appController.hasPendingSnack ? appController.snackName : "拖入一个文件" }
                            Label { width: parent.width; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideMiddle; color: "#99787B"; text: appController.hasPendingSnack ? appController.snackFileName + " · " + appController.snackFileInfo : "也可以点击下方选择文件" }
                        }
                    }
                    GroupBox {
                        title: "加工预览"; Layout.fillWidth: true; visible: appController.hasPendingSnack
                        ColumnLayout {
                            anchors.fill: parent
                            Label { text: appController.snackEmoji + "  " + appController.snackName; font.pixelSize: 19; font.bold: true; color: "#A35561" }
                            Label { Layout.fillWidth: true; elide: Text.ElideMiddle; text: "位置：" + appController.snackSourcePath; color: "#725A5E" }
                            Label { text: "大小：" + appController.snackFileInfo + "　修改时间：" + appController.snackModifiedText; color: "#725A5E" }
                            Label { text: "安全级别：" + appController.snackSafetyLevel; font.bold: true; color: appController.snackStrongConfirmationRequired ? "#C35B52" : "#5B8A65" }
                            Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: appController.snackWarning; color: "#805F63" }
                            ListView {
                                Layout.fillWidth: true; Layout.preferredHeight: Math.min(92, contentHeight); clip: true
                                visible: appController.snackPendingFiles.length > 1; model: appController.snackPendingFiles
                                delegate: Label { required property string modelData; width: ListView.view.width; text: "• " + modelData; elide: Text.ElideMiddle; color: "#725A5E" }
                            }
                            Button { text: "保护这个文件夹"; flat: true; onClicked: appController.protectPendingSnackDirectory() }
                        }
                    }
                    Label { Layout.fillWidth: true; wrapMode: Text.WordWrap; visible: appController.snackStatus.length > 0; text: appController.snackStatus; color: "#A35561" }
                    Item { Layout.fillHeight: true }
                    RowLayout {
                        Layout.fillWidth: true
                        Button { text: "选择文件（可多选）"; enabled: appController.fileSnackEnabled; onClicked: picker.open() }
                        Button { text: "取消"; visible: appController.hasPendingSnack; onClicked: appController.clearFileSnack() }
                        Item { Layout.fillWidth: true }
                        Button { text: "放入零食袋"; enabled: appController.hasPendingSnack; onClicked: { window.pendingAction="store"; confirmDialog.open() } }
                        Button { text: "现在吃掉"; highlighted: true; enabled: appController.hasPendingSnack; onClicked: { window.pendingAction="eat"; confirmDialog.open() } }
                    }
                }
            }

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    Label { text: "加工好的零食会一直保存在这里，下次打开程序也还在。"; color: "#8D6B70"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    ListView {
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 8
                        model: appController.snackBagItems
                        delegate: Rectangle {
                            required property string modelData; required property int index
                            width: ListView.view.width; height: 58; radius: 12; color: "#F7E8E2"
                            RowLayout { anchors.fill: parent; anchors.margins: 10
                                Label { text: modelData.split("|")[0]; color: "#704B50"; font.pixelSize: 16; Layout.fillWidth: true }
                                Button { text: "喂给精灵"; onClicked: appController.eatBagSnack(index) }
                            }
                        }
                        Label { anchors.centerIn: parent; visible: parent.count === 0; text: "零食袋还是空的"; color: "#A98B8F" }
                    }
                }
            }

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    Label { text: "当前饱腹度：" + appController.fullness + "/100。口味会随着投喂慢慢显现，太饱时精灵会拒绝继续吃。"; color: "#8D6B70"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    ListView {
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 7; model: appController.snackCatalogItems
                        delegate: Rectangle { required property string modelData; width: ListView.view.width; height: 54; radius: 11; color: "#F7E8E2"; Label { anchors.fill: parent; anchors.margins: 12; text: modelData; color: "#704B50"; verticalAlignment: Text.AlignVCenter } }
                        Label { anchors.centerIn: parent; visible: parent.count === 0; text: "喂过第一份零食后，图鉴就会解锁"; color: "#A98B8F" }
                    }
                }
            }

            Item {
                ListView {
                    anchors.fill: parent; clip: true; spacing: 7; model: appController.snackHistoryItems
                    delegate: Rectangle { required property string modelData; width: ListView.view.width; height: Math.max(48, historyText.implicitHeight + 20); radius: 10; color: "#FAEDE8"; Label { id: historyText; anchors.fill: parent; anchors.margins: 10; text: modelData; wrapMode: Text.WordWrap; color: "#704B50" } }
                    Label { anchors.centerIn: parent; visible: parent.count === 0; text: "还没有加工记录"; color: "#A98B8F" }
                }
            }
        }
    }
}
