import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: win; width: 650; height: 680; minimumWidth:560; minimumHeight:560
    visible:false; title:"情绪精灵 AI 设置"; color:"#FFF9F4"
    flags:Qt.Tool|Qt.WindowTitleHint|Qt.WindowCloseButtonHint
    function openAndRaise(){chatUrl.text=appController.aiBaseUrl;chatModel.text=appController.aiModel;chatKey.clear();visionUrl.text=appController.visionBaseUrl;visionModel.text=appController.visionModel;visionKey.clear();show();raise();requestActivate()}
    ColumnLayout {
        anchors.fill:parent;anchors.margins:20;spacing:10
        RowLayout {Layout.fillWidth:true;Label{text:"AI 服务设置";color:"#6B3E43";font.pixelSize:22;font.bold:true}Item{Layout.fillWidth:true}Button{text:"关闭 ✕";flat:true;onClicked:win.close()}}
        Label {Layout.fillWidth:true;wrapMode:Text.Wrap;text:"所有API Key只保存到Windows凭据管理器，不写入项目、日志或聊天数据库。留空保存表示继续使用已有密钥。";color:"#806467"}
        TabBar {id:tabs;Layout.fillWidth:true;TabButton{text:"聊天与梦境"}TabButton{text:"视觉识图"}TabButton{text:"MySQL 同步"}}
        StackLayout {currentIndex:tabs.currentIndex;Layout.fillWidth:true;Layout.fillHeight:true
            Item {ColumnLayout {anchors.fill:parent;spacing:9
                Label{text:"API地址";color:"#6B3E43"}TextField{id:chatUrl;Layout.fillWidth:true;placeholderText:"https://api.deepseek.com"}
                Label{text:"模型";color:"#6B3E43"}TextField{id:chatModel;Layout.fillWidth:true;placeholderText:"deepseek-v4-flash"}
                Label{text:"API Key";color:"#6B3E43"}TextField{id:chatKey;Layout.fillWidth:true;echoMode:TextInput.Password;passwordCharacter:"●";placeholderText:appController.aiConfigured?"已安全保存；留空表示不修改":"粘贴API密钥"}
                Rectangle{Layout.fillWidth:true;height:chatStatus.implicitHeight+22;radius:10;color:appController.aiConfigured?"#EDF7ED":"#F6ECE8";Label{id:chatStatus;anchors.fill:parent;anchors.margins:11;wrapMode:Text.Wrap;text:appController.aiStatus;color:"#654C4F"}}
                Item{Layout.fillHeight:true}
                RowLayout{Layout.fillWidth:true;Button{text:"清除密钥";onClicked:{appController.clearAiKey();chatKey.clear()}}Item{Layout.fillWidth:true}Button{text:"保存";onClicked:appController.saveAiSettings(chatKey.text,chatUrl.text,chatModel.text)}Button{text:appController.aiBusy?"测试中…":"保存并测试";enabled:!appController.aiBusy;highlighted:true;onClicked:{appController.saveAiSettings(chatKey.text,chatUrl.text,chatModel.text);chatKey.clear();appController.testAiConnection()}}}
            }}
            Item {ColumnLayout {anchors.fill:parent;spacing:9
                Label{Layout.fillWidth:true;wrapMode:Text.Wrap;text:"视觉服务只在你确认后接收经过缩放、重新编码并移除EXIF信息的照片。目前用于梦境现实回声。";color:"#805F63"}
                Label{text:"API地址";color:"#6B3E43"}TextField{id:visionUrl;Layout.fillWidth:true;placeholderText:"https://api.siliconflow.cn/v1"}
                Label{text:"模型";color:"#6B3E43"}TextField{id:visionModel;Layout.fillWidth:true;placeholderText:"Qwen/Qwen3-VL-8B-Instruct"}
                Label{text:"硅基流动 API Key";color:"#6B3E43"}TextField{id:visionKey;Layout.fillWidth:true;echoMode:TextInput.Password;passwordCharacter:"●";placeholderText:appController.visionConfigured?"已安全保存；留空表示不修改":"粘贴硅基流动密钥"}
                Rectangle{Layout.fillWidth:true;height:visionStatus.implicitHeight+22;radius:10;color:appController.visionConfigured?"#EDF7ED":"#F6ECE8";Label{id:visionStatus;anchors.fill:parent;anchors.margins:11;wrapMode:Text.Wrap;text:appController.visionStatus;color:"#654C4F"}}
                Item{Layout.fillHeight:true}
                RowLayout{Layout.fillWidth:true;Button{text:"清除视觉密钥";onClicked:{appController.clearVisionKey();visionKey.clear()}}Item{Layout.fillWidth:true}Button{text:"保存";onClicked:appController.saveVisionSettings(visionKey.text,visionUrl.text,visionModel.text)}Button{text:appController.visionBusy?"测试中…":"保存并测试";enabled:!appController.visionBusy;highlighted:true;onClicked:{appController.saveVisionSettings(visionKey.text,visionUrl.text,visionModel.text);visionKey.clear();appController.testVisionConnection()}}}
            }}
            Item {ColumnLayout {anchors.fill:parent;spacing:9
                Label{Layout.fillWidth:true;wrapMode:Text.Wrap;text:"云同步默认关闭。SQLite 始终是本机事实库；关闭同步不会删除本地内容。聊天、图片、日记、梦境、秘密记忆与本地文件路径永不上传。";color:"#805F63"}
                Label{text:"同步服务地址（必须 HTTPS）";color:"#6B3E43"}TextField{id:syncUrl;Layout.fillWidth:true;text:syncController.cloudUrl;placeholderText:"https://sync.example.com"}
                Label{text:"同步访问令牌";color:"#6B3E43"}TextField{id:syncToken;Layout.fillWidth:true;echoMode:TextInput.Password;passwordCharacter:"●";placeholderText:syncController.cloudConfigured?"已安全保存；留空不修改":"由同步服务提供"}
                RowLayout{Layout.fillWidth:true;Button{text:"保存并连接";onClicked:{syncController.saveCloudConfiguration(syncUrl.text,syncToken.text);syncToken.clear()}}Button{text:"测试连接";enabled:syncController.cloudConfigured;onClicked:syncController.testCloudConnection()}Button{text:"清除配置";onClicked:{syncController.clearCloudConfiguration();syncUrl.clear();syncToken.clear()}}Item{Layout.fillWidth:true}Label{text:syncController.cloudConnectionStatus;color:syncController.cloudConfigured?"#52735B":"#A36E58"}}
                RowLayout{Layout.fillWidth:true;Label{text:"云同步总开关";font.bold:true;color:"#6B3E43"}Item{Layout.fillWidth:true}Switch{checked:syncController.masterEnabled;onClicked:syncController.setMasterEnabled(checked)}}
                GroupBox{title:"允许上传的数据类型";Layout.fillWidth:true;ColumnLayout{anchors.fill:parent
                    CheckBox{text:"应用设置（不含密钥和路径）";checked:syncController.settingsEnabled;onClicked:syncController.setCategoryEnabled("settings",checked)}
                    CheckBox{text:"精灵状态数值";checked:syncController.petStateEnabled;onClicked:syncController.setCategoryEnabled("pet_state",checked)}
                    CheckBox{text:"普通记忆（秘密与仅本地记忆仍拒绝）";checked:syncController.memoryEnabled;onClicked:syncController.setCategoryEnabled("memory",checked)}
                    CheckBox{text:"提醒与完成状态";checked:syncController.reminderEnabled;onClicked:syncController.setCategoryEnabled("reminder",checked)}
                }}
                GridLayout{columns:2;Layout.fillWidth:true;columnSpacing:12
                    Label{text:"设备标识";color:"#806467"}Label{Layout.fillWidth:true;elide:Text.ElideMiddle;text:syncController.deviceId;color:"#4E6870"}
                    Label{text:"待发送";color:"#806467"}Label{text:syncController.pendingCount+" 条"}
                    Label{text:"最近成功";color:"#806467"}Label{text:syncController.lastSuccess||"尚未成功同步"}
                    Label{text:"最近失败";color:"#806467"}Label{text:syncController.lastError||"无";color:syncController.lastError?"#A34F52":"#52735B"}
                }
                Label{Layout.fillWidth:true;wrapMode:Text.Wrap;text:syncController.lastOperation;color:"#52735B";visible:text.length>0}
                RowLayout{Layout.fillWidth:true;Button{text:"刷新状态";onClicked:{syncController.refreshStatus();syncController.refreshConflicts()}}Button{text:"导出云端数据";onClicked:syncController.exportCloudData()}Item{Layout.fillWidth:true}}
                Label{text:"待处理冲突（只显示类型与标识，不展示正文）";font.bold:true;color:"#6B3E43"}
                ListView{id:conflicts;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;model:syncController.conflictItems;delegate:RowLayout{width:conflicts.width;Label{Layout.fillWidth:true;text:modelData;elide:Text.ElideRight}Button{text:"保留本地";onClicked:syncController.resolveConflict(index,"local")}Button{text:"保留云端";onClicked:syncController.resolveConflict(index,"cloud")}}}
                RowLayout{Layout.fillWidth:true;TextField{id:deleteConfirm;Layout.fillWidth:true;placeholderText:"输入 DELETE CLOUD DATA"}Button{text:"删除云端副本";enabled:deleteConfirm.text==="DELETE CLOUD DATA";onClicked:{syncController.deleteCloudData(deleteConfirm.text);deleteConfirm.clear()}}}
                Label{Layout.fillWidth:true;wrapMode:Text.Wrap;text:"云端删除只清理同步服务中的副本；本机 SQLite 数据会保留。";color:"#806467"}
            }}
        }
    }
}
