import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Dialogs

Window {
    id: win; width: 820; height: 700; minimumWidth: 680; minimumHeight: 560; visible: false
    title: "AI总结魔法"; color: "#FFF9F4"
    flags: Qt.Tool | Qt.WindowTitleHint | Qt.WindowCloseButtonHint | Qt.WindowStaysOnTopHint
    function openAndRaise(){show();raise();requestActivate()}
    property string summaryMode: "standard"

    FileDialog {
        id: picker; title: "选择要总结的纯文本文件"; fileMode: FileDialog.OpenFile
        nameFilters: ["支持的文本文件 (*.txt *.md *.log *.csv *.json *.cpp *.c *.h *.hpp *.qml *.py *.js *.ts *.html *.xml)", "所有文件 (*)"]
        onAccepted: { if(appController.loadSummaryFile(selectedFile.toString())) inputArea.text=appController.summaryInputText }
    }
    Popup {
        id: rewardPopup; anchors.centerIn: parent; width: 430; height: 390; modal: true; focus: true; closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { radius: 16; color: "#FFF9F4"; border.color: "#DCAAAA" }
        contentItem: ColumnLayout {
            spacing: 10
            RowLayout { Layout.fillWidth:true;Label{text:"选择一份总结奖励";font.pixelSize:19;font.bold:true;color:"#6B3E43"}Item{Layout.fillWidth:true}Button{text:"关闭 ✕";flat:true;onClicked:rewardPopup.close()} }
            Label { Layout.fillWidth:true;wrapMode:Text.WordWrap;text:"如果精灵已经吃饱，零食不会被消耗，而会预约到饱腹度低于60后再次征求你的同意。";color:"#805F63" }
            ListView { Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:7;model:appController.snackBagItems
                delegate:Rectangle{required property string modelData;required property int index;width:ListView.view.width;height:54;radius:10;color:"#F6E8E1"
                    RowLayout{anchors.fill:parent;anchors.margins:9;Label{text:modelData.split("|")[0];Layout.fillWidth:true;color:"#704B50"}Button{text:"奖励她";onClicked:{appController.rewardSummarySnack(index);rewardPopup.close()}}}
                }
                Label{anchors.centerIn:parent;visible:parent.count===0;text:"零食袋空空的，夸夸她也可以抵零食！";color:"#9A777A"}
            }
        }
    }
    DropArea {
        anchors.fill: parent
        onDropped: function(drop){if(drop.hasUrls&&drop.urls.length>0&&appController.loadSummaryFile(drop.urls[0].toString()))inputArea.text=appController.summaryInputText}
    }
    Connections {
        target: appController
        function onSummaryMagicChanged(){if(appController.summaryInputText.length>0&&inputArea.text.length===0)inputArea.text=appController.summaryInputText}
    }

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 16; spacing: 10
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout { Label { text: "✨ AI总结魔法"; font.pixelSize: 23; font.bold: true; color: "#6B3E43" } Label { text: "精灵只读原文，不修改文件；总结结果保存在本地历史。"; color: "#987579" } }
            Item { Layout.fillWidth: true }
            Switch { text: "模块开启"; checked: appController.summaryMagicEnabled; onToggled: appController.setSummaryMagicEnabled(checked) }
            Button { text: "关闭 ✕"; flat: true; onClicked: win.close() }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: "#E5D1CB" }
        TabBar { id: tabs; Layout.fillWidth: true; TabButton{text:"原文"} TabButton{text:"总结结果"} TabButton{text:"历史记录  "+appController.summaryHistoryItems.length} }
        Frame {
            Layout.fillWidth: true
            Layout.preferredHeight: 92
            visible: tabs.currentIndex < 2
            ColumnLayout {
                anchors.fill: parent; spacing: 4
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "本次总结要求（可选）"; font.bold: true; color: "#704B50" }
                    Item { Layout.fillWidth: true }
                    Label { text: requirementArea.length + " / 2000"; color: requirementArea.length > 2000 ? "#C35B52" : "#987579" }
                }
                ScrollView {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    TextArea {
                        id: requirementArea
                        placeholderText: "例如：重点分析实验方法；用适合初学者的语言；特别列出风险和待办事项……"
                        wrapMode: TextEdit.Wrap; selectByMouse: true; background: null
                    }
                }
            }
        }
        StackLayout {
            currentIndex: tabs.currentIndex; Layout.fillWidth: true; Layout.fillHeight: true
            Item {
                ColumnLayout { anchors.fill: parent; spacing: 9
                    RowLayout { Layout.fillWidth: true
                        Button { text: "选择文件"; onClicked: picker.open() }
                        Label { Layout.fillWidth: true; elide: Text.ElideMiddle; text: appController.summarySourceName.length?appController.summarySourceName+" · "+appController.summarySourceInfo:"也可以把支持的文本文件拖进窗口"; color: "#805F63" }
                        Button { text: "清空"; onClicked: {inputArea.clear();requirementArea.clear();appController.clearSummaryMagic()} }
                    }
                    Frame { Layout.fillWidth: true; Layout.fillHeight: true
                        ScrollView { anchors.fill: parent
                            TextArea { id: inputArea; placeholderText: "在这里粘贴笔记、文章或会议记录……"; wrapMode: TextEdit.Wrap; selectByMouse: true; background: null }
                        }
                    }
                    RowLayout { Layout.fillWidth: true
                        Label { text: "总结深度"; color: "#704B50" }
                        ComboBox { id: modeBox; model:["极简速览","标准总结","学习笔记"]; currentIndex:1; onCurrentIndexChanged: win.summaryMode=currentIndex===0?"brief":currentIndex===2?"study":"standard" }
                        Label { text: inputArea.length+" / 60000 字符"; color: inputArea.length>60000?"#C35B52":"#987579" }
                        Item { Layout.fillWidth: true }
                        Button { text: appController.summaryMagicBusy?"正在施法……":"开始总结"; highlighted: true; enabled: appController.summaryMagicEnabled&&!appController.summaryMagicBusy&&inputArea.length>=20&&requirementArea.length<=2000; onClicked:{appController.generateSummary(inputArea.text,win.summaryMode,requirementArea.text);tabs.currentIndex=1} }
                    }
                    RowLayout { Layout.fillWidth:true;visible:appController.summaryResult.length>0&&!appController.summaryMagicBusy
                        Label{text:"本喵完成得怎么样？";color:"#704B50";font.bold:true}
                        Button{text:"夸夸她";onClicked:appController.praiseSummaryMagic()}
                        Button{text:"奖励零食";onClicked:rewardPopup.open()}
                        Button{text:"领取预约奖励";visible:appController.summaryRewardReserved;enabled:appController.summaryRewardCanClaim;onClicked:appController.claimReservedSummarySnack()}
                        Item{Layout.fillWidth:true}
                    }
                    RowLayout { Layout.fillWidth:true;visible:appController.summaryResult.length>0&&!appController.summaryMagicBusy
                        Label{text:"总结反馈：";color:"#987579"}
                        Button{text:"很准确";flat:true;onClicked:appController.rateSummaryMagic("accurate")}
                        Button{text:"太简略";flat:true;onClicked:appController.rateSummaryMagic("short")}
                        Button{text:"太啰嗦";flat:true;onClicked:appController.rateSummaryMagic("verbose")}
                        Button{text:"遗漏重点";flat:true;onClicked:appController.rateSummaryMagic("missed")}
                        Item{Layout.fillWidth:true}
                    }
                }
            }
            Item {
                ColumnLayout { anchors.fill: parent; spacing: 9
                    RowLayout { Layout.fillWidth: true
                        Label { text: appController.summaryMagicBusy?"精灵正在阅读原文……":"总结结果"; font.bold: true; color: "#704B50" }
                        BusyIndicator { running: appController.summaryMagicBusy; visible: running; implicitWidth:28; implicitHeight:28 }
                        Item { Layout.fillWidth: true }
                        Button { text: "重新总结"; enabled: appController.summaryMagicEnabled&&!appController.summaryMagicBusy&&inputArea.length>=20&&requirementArea.length<=2000; onClicked: appController.generateSummary(inputArea.text,win.summaryMode,requirementArea.text) }
                        Button { text: "复制结果"; enabled: appController.summaryResult.length>0; onClicked: appController.copySummaryResult() }
                    }
                    Frame { Layout.fillWidth: true; Layout.fillHeight: true
                        ScrollView { anchors.fill: parent
                            TextArea { readOnly:true; text:appController.summaryResult; placeholderText:appController.summaryMagicBusy?"魔法正在汇聚……":"生成后的结构化总结会显示在这里"; wrapMode:TextEdit.Wrap; selectByMouse:true; background:null }
                        }
                    }
                }
            }
            Item {
                ListView { id:historyList; anchors.fill:parent; clip:true; spacing:8; model:appController.summaryHistoryItems
                    delegate: Rectangle { required property string modelData; required property int index; property var parts:modelData.split("|"); width:ListView.view.width;height:64;radius:11;color:"#F6E8E1"
                        RowLayout { anchors.fill:parent;anchors.margins:10
                            ColumnLayout { Layout.fillWidth:true;Label{text:parts[2]||"未命名总结";font.bold:true;color:"#6B3E43"}Label{text:(parts[1]||"")+" · "+(parts[3]||"粘贴文本");color:"#987579";elide:Text.ElideRight;Layout.fillWidth:true} }
                            Button{text:"查看";onClicked:{appController.selectSummaryHistory(index);tabs.currentIndex=1}}
                            Button{text:"删除";onClicked:appController.deleteSummaryHistory(index)}
                        }
                    }
                    Label { anchors.centerIn:parent;visible:parent.count===0;text:"还没有保存过总结";color:"#9A777A" }
                    ScrollBar.vertical:ScrollBar{}
                }
            }
        }
        Label { Layout.fillWidth:true;visible:appController.summaryStatus.length>0;text:appController.summaryStatus;wrapMode:Text.WordWrap;color:"#A35561" }
    }
}
