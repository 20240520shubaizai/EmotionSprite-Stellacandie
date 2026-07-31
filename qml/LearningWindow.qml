import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: window
    width: 650; height: 620; minimumWidth: 520; minimumHeight: 480
    visible: false; title: "学习与共同梗"; color: "#FFF9F4"
    flags: Qt.Tool | Qt.WindowTitleHint | Qt.WindowCloseButtonHint
    function openAndRaise() { show(); raise(); requestActivate() }

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 24; spacing: 14
        RowLayout {
            Layout.fillWidth: true
            Label { text: "学习与共同梗"; color: "#6B3E43"; font.pixelSize: 22; font.bold: true }
            Item { Layout.fillWidth: true }
            Button { text: "关闭 ✕"; flat: true; onClicked: window.hide() }
        }
        Label {
            Layout.fillWidth: true; wrapMode: Text.Wrap; color: "#806467"
            text: "精灵只缓慢学习表达偏好，不会自动改写角色圣经。你指出‘这是一个梗’时，她会先联网查询，再请你补充；你的解释会优先保存。"
        }
        Switch {
            text: "启用启发式偏好学习"
            checked: appController.adaptiveLearningEnabled
            onToggled: appController.setAdaptiveLearningEnabled(checked)
        }
        Switch {
            text: "启用热梗、未知梗学习和共同梗"
            checked: appController.memeCultureEnabled
            onToggled: appController.setMemeCultureEnabled(checked)
        }
        GroupBox {
            title: "已经学会的个人梗与共同梗"
            Layout.fillWidth: true; Layout.fillHeight: true
            ListView {
                id: learnedList
                anchors.fill: parent; anchors.margins: 8; spacing: 6; clip: true
                model: appController.learnedMemes
                delegate: Rectangle {
                    required property int index
                    required property string modelData
                    width: ListView.view.width; height: Math.max(54, summary.implicitHeight + 16)
                    radius: 8; color: index % 2 ? "#FFF2EA" : "#FFF8F3"
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 8
                        Label { id: summary; text: modelData; wrapMode: Text.Wrap; Layout.fillWidth: true; color: "#674E51" }
                        Button { text: "删除"; flat: true; onClicked: appController.removeLearnedMeme(index) }
                    }
                }
                Label { anchors.centerIn: parent; visible: learnedList.count === 0; text: "还没有共同学习过新梗"; color: "#9A7778" }
            }
        }
        Label {
            Layout.fillWidth: true; wrapMode: Text.Wrap; color: "#947477"
            text: "共同梗示例：先描述一个场景，再说“以后这就叫『猫猫断电』”。未知梗示例：说完梗后补一句“哎呀，这是一个梗”。"
        }
    }
}
