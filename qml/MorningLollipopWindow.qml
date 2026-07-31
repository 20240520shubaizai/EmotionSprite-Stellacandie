import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 940; height: 820; visible: false
    title: "Stellacandie 的晨间糖果罐"
    color: "#FFF7F3"
    function openAndRaise() { show(); raise(); requestActivate() }
    function hm(v) { return String(Math.floor(v/60)).padStart(2,"0")+":"+String(v%60).padStart(2,"0") }

    header: Rectangle {
        height: 76; color: "#F3D8D2"
        RowLayout { anchors.fill: parent; anchors.margins: 18; spacing: 14
            Label { text: "🍭 晨间糖果罐"; font.pixelSize: 28; font.bold: true; color: "#743F45"; Layout.fillWidth: true }
            Switch { text: "每日赠送"; checked: appController.morningLollipopEnabled; onToggled: appController.setMorningLollipopEnabled(checked) }
            Button { text: "关闭 ×"; onClicked: root.close() }
        }
    }
    RowLayout { anchors.fill: parent; anchors.margins: 20; spacing: 18
        Rectangle { Layout.preferredWidth: 300; Layout.fillHeight: true; radius: 18; color: "#FFF0EB"; border.color: "#E7BDB5"
            ColumnLayout { anchors.fill: parent; anchors.margins: 16; spacing: 10
                Label { text: "我的糖果收藏"; font.pixelSize: 20; font.bold: true; color: "#743F45" }
                ListView { id: list; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 8; model: appController.morningLollipopItems
                    delegate: Rectangle { width: list.width; height: 68; radius: 12; color: ListView.isCurrentItem ? "#F3D2CC" : "#FFF9F6"
                        required property string modelData; required property int index
                        property var p: modelData.split("|")
                        MouseArea { anchors.fill: parent; onClicked: { list.currentIndex=index; appController.selectMorningLollipop(index) } }
                        Row { anchors.fill: parent; anchors.margins: 10; spacing: 10
                            Text { text: p[1]; font.pixelSize: 28 }
                            Column { Text { text: p[2]+(p[5]==="favorite"?"  ♥":""); color:"#6D3E43"; font.bold:true } Text { text:p[0]+" · "+p[3]; color:"#A17074"; font.pixelSize:12 } }
                        }
                    }
                }
            }
        }
        ColumnLayout { Layout.fillWidth: true; Layout.fillHeight: true; spacing: 14
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 390; radius: 24; color: "#FFFFFF"; border.color: "#E8C8C0"
                ColumnLayout { anchors.fill: parent; anchors.margins: 22; spacing: 10
                    Item { Layout.preferredWidth: 120; Layout.preferredHeight: 118; Layout.alignment: Qt.AlignHCenter
                        Canvas { id: candyArt; anchors.fill: parent
                            property color candyColor: appController.selectedLollipopColor || "#F2A8B5"
                            property string candyShape: appController.selectedLollipopShape
                            property string candyPattern: appController.selectedLollipopPattern
                            onCandyColorChanged: requestPaint(); onCandyShapeChanged: requestPaint(); onCandyPatternChanged: requestPaint()
                            onPaint: { var c=getContext("2d"); c.reset(); c.lineCap="round"; c.strokeStyle="#C8A17A"; c.lineWidth=7; c.beginPath(); c.moveTo(60,68); c.lineTo(60,114); c.stroke();
                                c.shadowColor="#D8A5AE"; c.shadowBlur=12; c.fillStyle=candyColor; c.strokeStyle="#FFFFFF"; c.lineWidth=4; c.beginPath()
                                if(candyShape==="heart"){c.moveTo(60,74);c.bezierCurveTo(15,47,28,12,60,34);c.bezierCurveTo(92,12,105,47,60,74)}
                                else if(candyShape==="star"){for(var i=0;i<10;i++){var a=-Math.PI/2+i*Math.PI/5,r=i%2===0?42:20,x=60+Math.cos(a)*r,y=45+Math.sin(a)*r;if(i===0)c.moveTo(x,y);else c.lineTo(x,y)}c.closePath()}
                                else if(candyShape==="paw"){c.arc(60,52,30,0,Math.PI*2)}
                                else if(candyShape==="crystal"){c.moveTo(60,4);c.lineTo(98,35);c.lineTo(82,78);c.lineTo(38,78);c.lineTo(22,35);c.closePath()}
                                else {c.arc(60,43,40,0,Math.PI*2)} c.fill();c.stroke();c.shadowBlur=0
                                c.strokeStyle="rgba(255,255,255,.72)";c.lineWidth=5;c.beginPath();if(candyPattern==="stars"){for(var j=0;j<3;j++){c.moveTo(40+j*18,31);c.lineTo(47+j*14,52)}}else{c.arc(60,43,24,.2,5.4);c.arc(60,43,12,3.4,7.8)}c.stroke()
                            }
                        }
                        Text { anchors.right: parent.right; anchors.top: parent.top; text: appController.selectedLollipopEmoji; font.pixelSize: 24 }
                    }
                    Label { text: appController.selectedLollipopFlavor || "今天的糖还在路上"; font.pixelSize: 24; font.bold: true; color: "#713E44"; Layout.alignment: Qt.AlignHCenter }
                    Label { text: appController.selectedLollipopDate+"  ·  "+appController.selectedLollipopRarity+"  ·  "+appController.selectedLollipopType; color: "#A37075"; Layout.alignment: Qt.AlignHCenter }
                    Label { text: appController.selectedLollipopGreeting || "到了合适的早晨，它会安静地出现在这里。"; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; color: "#694C4E"; font.pixelSize: 16; Layout.fillWidth: true }
                    Label { visible: text!==""; text: appController.selectedLollipopStory; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; color: "#947277"; font.italic: true; Layout.fillWidth: true }
                    Button { text: appController.selectedLollipopFavorite ? "取消珍藏" : "♥ 珍藏这颗"; Layout.alignment: Qt.AlignHCenter; enabled: appController.selectedLollipopFlavor!==""; onClicked: appController.toggleSelectedLollipopFavorite() }
                }
            }
            GroupBox { title: "赠送时间区间"; Layout.fillWidth: true
                GridLayout { columns: 4; anchors.fill: parent
                    Label { text: "工作日" } SpinBox { id: ws; from:300; to:690; stepSize:15; value:appController.lollipopWorkdayStart; textFromValue:function(v){return root.hm(v)}; valueFromText:function(t){var p=t.split(":");return Number(p[0])*60+Number(p[1])} }
                    Label { text: "至" } SpinBox { id: we; from:330; to:720; stepSize:15; value:appController.lollipopWorkdayEnd; textFromValue:function(v){return root.hm(v)}; valueFromText:ws.valueFromText }
                    Label { text: "周末" } SpinBox { id: ss; from:300; to:690; stepSize:15; value:appController.lollipopWeekendStart; textFromValue:function(v){return root.hm(v)}; valueFromText:ws.valueFromText }
                    Label { text: "至" } SpinBox { id: se; from:330; to:720; stepSize:15; value:appController.lollipopWeekendEnd; textFromValue:function(v){return root.hm(v)}; valueFromText:ws.valueFromText }
                    Button { text: "保存区间"; Layout.columnSpan: 4; Layout.alignment: Qt.AlignRight; onClicked: appController.saveMorningLollipopWindows(ws.value,we.value,ss.value,se.value) }
                }
            }
            GroupBox { title: "天气联动（仅保存城市名）"; Layout.fillWidth: true
                RowLayout { anchors.fill: parent
                    TextField { id: cityField; text: appController.lollipopCity; placeholderText: "例如：北京、上海、杭州"; Layout.fillWidth: true }
                    Button { text: "保存并同步"; onClicked: appController.setLollipopCity(cityField.text) }
                    Button { text: "刷新"; onClicked: appController.refreshLollipopWeather() }
                }
            }
            Label { text: appController.lollipopWeather; color: "#7F686A"; Layout.fillWidth: true }
            RowLayout { Layout.fillWidth: true
                Label { text: appController.morningLollipopStatus; color: "#8A5A60"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Button { text: "立即生成测试糖"; onClicked: appController.testMorningLollipop() }
            }
            Label { text: "每天最多一颗；不会因为你没回应而生气。全屏、长时间离开或勿扰时会延后或安静放入罐中，中午后不补发过期早安。"; wrapMode: Text.WordWrap; color: "#9A7779"; Layout.fillWidth: true }
        }
    }
}
