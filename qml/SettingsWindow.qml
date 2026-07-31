import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: win; width: 580; height: 570; minimumWidth:520; minimumHeight:500
    visible:false; title:"情绪精灵 AI 设置"; color:"#FFF9F4"
    flags:Qt.Tool|Qt.WindowTitleHint|Qt.WindowCloseButtonHint
    function openAndRaise(){chatUrl.text=appController.aiBaseUrl;chatModel.text=appController.aiModel;chatKey.clear();visionUrl.text=appController.visionBaseUrl;visionModel.text=appController.visionModel;visionKey.clear();show();raise();requestActivate()}
    ColumnLayout {
        anchors.fill:parent;anchors.margins:20;spacing:10
        RowLayout {Layout.fillWidth:true;Label{text:"AI 服务设置";color:"#6B3E43";font.pixelSize:22;font.bold:true}Item{Layout.fillWidth:true}Button{text:"关闭 ✕";flat:true;onClicked:win.close()}}
        Label {Layout.fillWidth:true;wrapMode:Text.Wrap;text:"所有API Key只保存到Windows凭据管理器，不写入项目、日志或聊天数据库。留空保存表示继续使用已有密钥。";color:"#806467"}
        TabBar {id:tabs;Layout.fillWidth:true;TabButton{text:"聊天与梦境"}TabButton{text:"视觉识图"}}
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
        }
    }
}
