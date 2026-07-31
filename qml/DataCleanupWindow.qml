import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: win; width: 760; height: 650; minimumWidth: 650; minimumHeight: 520; visible: false
    title: "记忆治理与数据清理"; color: "#FFF9F4"
    flags: Qt.Tool | Qt.WindowTitleHint | Qt.WindowCloseButtonHint | Qt.WindowStaysOnTopHint
    function openAndRaise(){show();raise();requestActivate()}
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 16; spacing: 10
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout { Label { text: "记忆治理与数据清理"; font.pixelSize: 22; font.bold: true; color: "#6B3E43" } Label { text: "自动整理不会物理删除长期记忆；回收区内容可以恢复。"; color: "#987579" } }
            Item { Layout.fillWidth: true }
            Switch { text: "自动治理"; checked: appController.dataCleanupEnabled; onToggled: appController.setDataCleanupEnabled(checked) }
            Button { text: "关闭 ✕"; flat: true; onClicked: win.close() }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: "#E5D1CB" }
        RowLayout {
            Layout.fillWidth: true
            Label { text: appController.memoryCleanupSummary; color: "#704B50"; font.bold: true; Layout.fillWidth: true }
            Button { text: "立即安全整理"; highlighted: true; onClicked: appController.runDataCleanup() }
        }
        Label { Layout.fillWidth: true; visible: appController.memoryCleanupResult.length>0; text: appController.memoryCleanupResult; wrapMode: Text.WordWrap; color: "#56805B" }
        Frame {
            Layout.fillWidth: true
            Label { anchors.fill: parent; wrapMode: Text.WordWrap; color: "#805F63"; text: "规则：普通记忆90天未使用进入沉睡，180天后归档；临时记忆到期进入回收区。锁定记忆、核心资料和反向日记不会被自动清理。" }
        }
        ListView {
            id: list; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 8
            model: appController.managedMemoryItems
            delegate: Rectangle {
                required property string modelData; required property int index
                property var parts: modelData.split("|")
                property bool lockedMemory: parts[1] === "1"
                property string stateText: parts[2] || "活跃"
                width: ListView.view.width; height: body.implicitHeight+22; radius: 12; color: "#F6E8E1"; border.color: lockedMemory ? "#D69A52" : "#E7D1C9"
                ColumnLayout {
                    id: body; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 11; spacing: 5
                    RowLayout { Layout.fillWidth: true
                        Label { text: (lockedMemory?"🔒 ":"")+(parts[3]||"未命名"); font.bold: true; color: "#6B3E43" }
                        Label { text: stateText+" · 重要度 "+(parts[5]||"0"); color: "#A27B7E" }
                        Item { Layout.fillWidth: true }
                        Button { text: lockedMemory?"解锁":"锁定"; onClicked: appController.toggleMemoryLock(index) }
                    }
                    Label { Layout.fillWidth: true; text: parts[4]||""; wrapMode: Text.WordWrap; color: "#60474A"; maximumLineCount: 3; elide: Text.ElideRight }
                    RowLayout { Layout.fillWidth: true
                        Button { text: "唤醒"; enabled: stateText!=="活跃"&&stateText!=="回收区"; onClicked: appController.setManagedMemoryState(index,"active") }
                        Button { text: "沉睡"; enabled: !lockedMemory&&stateText==="活跃"; onClicked: appController.setManagedMemoryState(index,"sleeping") }
                        Button { text: "归档"; enabled: !lockedMemory&&stateText!=="归档"&&stateText!=="回收区"; onClicked: appController.setManagedMemoryState(index,"archived") }
                        Item { Layout.fillWidth: true }
                        Button { text: "恢复"; visible: stateText==="回收区"; onClicked: appController.restoreManagedMemory(index) }
                        Button { text: "移入回收区"; visible: stateText!=="回收区"; enabled: !lockedMemory; onClicked: appController.deleteManagedMemory(index) }
                    }
                }
            }
            Label { anchors.centerIn: parent; visible: parent.count===0; text: "目前还没有可管理的长期记忆"; color: "#9A777A" }
            ScrollBar.vertical: ScrollBar{}
        }
    }
}
