import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: window
    required property Window petWindow
    signal requestMouseEvent()
    width: 430; height: 650; visible: false
    title: "情绪精灵调试面板"; color: "#FFF9F4"
    flags: Qt.Tool | Qt.WindowTitleHint | Qt.WindowCloseButtonHint | Qt.WindowStaysOnTopHint
    function openNearPet(){x=Math.max(12,petWindow.x-width-10);y=Math.max(12,petWindow.y);show();raise();requestActivate()}
    ScrollView { anchors.fill: parent; anchors.margins: 16
        ColumnLayout { width: parent.width; spacing: 12
            RowLayout { Layout.fillWidth: true
                Label { text: "动作与状态调试"; font.pixelSize: 20; font.bold: true; color: "#6B3E43" }
                Item { Layout.fillWidth: true }
                Button { text: "关闭 ✕"; flat: true; onClicked: window.close() }
            }
            GroupBox { title: "状态切换"; Layout.fillWidth: true
                RowLayout { anchors.fill: parent
                    Button { text: "◀ 上一个"; onClicked: appController.previousState() }
                    Label { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter; font.bold: true; text: (appController.currentStateIndex+1)+"/12  "+appController.currentStateName }
                    Button { text: "下一个 ▶"; onClicked: appController.nextState() }
                }
            }
            GridLayout { columns: 3; Layout.fillWidth: true
                Label { text: "心情："+appController.mood } Button { text: "-10"; onClicked: appController.adjustPetStat("mood",-10) } Button { text: "+10"; onClicked: appController.adjustPetStat("mood",10) }
                Label { text: "精力："+appController.energy } Button { text: "-10"; onClicked: appController.adjustPetStat("energy",-10) } Button { text: "+10"; onClicked: appController.adjustPetStat("energy",10) }
                Label { text: "好奇："+appController.curiosity } Button { text: "-10"; onClicked: appController.adjustPetStat("curiosity",-10) } Button { text: "+10"; onClicked: appController.adjustPetStat("curiosity",10) }
                Label { text: "小脾气："+appController.irritation } Button { text: "-10"; onClicked: appController.adjustPetStat("irritation",-10) } Button { text: "+10"; onClicked: appController.adjustPetStat("irritation",10) }
                Label { text: "饱腹："+appController.fullness } Button { text: "-10"; onClicked: appController.adjustPetStat("fullness",-10) } Button { text: "+10"; onClicked: appController.adjustPetStat("fullness",10) }
            }
            GroupBox { title: "魔法感冒"; Layout.fillWidth: true
                ColumnLayout { anchors.fill: parent
                    Label { text: "健康 "+appController.health+" · "+appController.healthPhaseName+" · 恢复 "+appController.recoveryProgress+"%"; color: "#56805B" }
                    Label { text: "状况："+appController.conditionName; color: "#746064" }
                    RowLayout {
                        Button { text: "触发感冒"; onClicked: appController.forceMagicCold() }
                        Button { text: "恢复+25"; onClicked: appController.advanceHealthRecovery() }
                        Button { text: "休息"; onClicked: appController.letPetRest() }
                        Button { text: "痊愈"; onClicked: appController.healPet() }
                    }
                }
            }
            GroupBox { title: "随机事件测试"; Layout.fillWidth: true
                RowLayout { anchors.fill: parent; Button { text: "老鼠追逐"; onClicked: window.requestMouseEvent() } }
            }
            GridLayout { columns: 2; Layout.fillWidth: true
                Label { text: "亲密："+appController.closeness } Label { text: "无聊："+appController.boredom }
                Label { text: "冷落："+appController.neglect } Label { text: "桌面动作："+(appController.desktopRoaming?appController.desktopAnimation:"待机") }
            }
            RowLayout { Layout.fillWidth: true; Item { Layout.fillWidth: true } Button { text: "全部重置"; onClicked: appController.resetPetStats() } }
        }
    }
}
