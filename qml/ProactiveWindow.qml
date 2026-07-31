import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: window
    width: 560; height: 690; minimumWidth: 500; minimumHeight: 580
    visible: false; title: "主动陪伴设置"; color: "#FFF9F4"
    flags: Qt.Tool | Qt.WindowTitleHint | Qt.WindowCloseButtonHint
    function openAndRaise() { limit.value=appController.proactiveDailyLimit; quietStart.value=appController.quietStartHour; quietEnd.value=appController.quietEndHour; show(); raise(); requestActivate() }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 24; spacing: 14
        RowLayout { Layout.fillWidth: true
            Label { text: "主动陪伴"; color: "#6B3E43"; font.pixelSize: 22; font.bold: true }
            Item { Layout.fillWidth: true }
            Button { text: "关闭 ✕"; flat: true; onClicked: window.hide() }
        }
        Label { Layout.fillWidth: true; wrapMode: Text.Wrap; color: "#806467"; text: "精灵可以回访故事、提醒约定和提出低压力的生活建议。所有主动通知都受每日上限、每小时冷却和安静时间约束。" }
        Switch { text: "启用主动陪伴"; checked: appController.proactiveEnabled; onToggled: appController.setProactiveEnabled(checked) }
        Switch { text: "勿扰模式（暂停所有普通主动通知）"; checked: appController.doNotDisturb; onToggled: appController.setDoNotDisturb(checked) }
        GroupBox { title: "频率和安静时间"; Layout.fillWidth: true
            GridLayout { columns: 2; anchors.fill: parent; rowSpacing: 10; columnSpacing: 14
                Label { text: "每日最多通知" }
                SpinBox { id: limit; from: 1; to: 6 }
                Label { text: "安静时间开始（时）" }
                SpinBox { id: quietStart; from: 0; to: 23 }
                Label { text: "安静时间结束（时）" }
                SpinBox { id: quietEnd; from: 0; to: 23 }
            }
        }
        GroupBox { title: "添加生活提醒"; Layout.fillWidth: true
            ColumnLayout { anchors.fill: parent
                TextField { id: reminderText; Layout.fillWidth: true; placeholderText: "例如：该起来走一走啦，别一直坐着。" }
                RowLayout { Label { text: "多少分钟后" } SpinBox { id: minutes; from: 1; to: 1440; value: 30 } Item { Layout.fillWidth: true }
                    Button { text: "添加提醒"; enabled: reminderText.text.trim().length>0; onClicked: { appController.addLifestyleReminder(reminderText.text,minutes.value); reminderText.clear() } }
                }
            }
        }
        GroupBox { title: "当前约定"; Layout.fillWidth: true; Layout.preferredHeight: 150
            ListView { id: commitments; anchors.fill: parent; clip: true; model: appController.activeCommitments
                delegate: RowLayout { required property int index; required property string modelData; width: ListView.view.width; spacing: 6
                    Label { text: modelData; Layout.fillWidth: true; elide: Text.ElideRight }
                    Button { text: "完成"; onClicked: appController.completeCommitment(index) }
                    Button { text: "取消"; flat: true; onClicked: appController.cancelCommitment(index) }
                }
                Label { anchors.centerIn: parent; visible: commitments.count===0; text: "还没有正式约定"; color: "#9A7778" }
            }
        }
        Label { Layout.fillWidth: true; wrapMode: Text.Wrap; color: "#947477"; text: "连续两次主动通知没有得到回应后，精灵当天会停止继续打扰。运动强度等具体目标仍需由你确认。" }
        Item { Layout.fillHeight: true }
        RowLayout { Layout.fillWidth: true; Item { Layout.fillWidth: true } Button { text: "保存设置"; highlighted: true; onClicked: { appController.saveProactiveSettings(limit.value,quietStart.value,quietEnd.value); window.hide() } } }
    }
}
