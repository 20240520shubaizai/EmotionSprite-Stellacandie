import QtQuick
import QtQuick.Controls
import QtQuick.Window

Window {
    id: root
    width: 150
    height: 150
    visible: false
    color: "transparent"
    title: "情绪精灵 Stellacandie"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

    property bool debugVisible: false
    property bool reducedMotion: false
    readonly property bool roaming: appController.desktopRoaming
    readonly property int roamDirection: appController.desktopRoamDirection
    property bool jumpPlaying: false
    property string snackAction: ""
    property string activeSnackEmoji: "🍪"

    Item {
        id: studyMagicLayer; anchors.fill: parent; z: 9
        visible: false
        Item {
            id: glasses; width: 66; height: 25; x: 42; y: 50
            Rectangle { x: 3; y: 2; width: 25; height: 20; radius: 10; color: "#25FFFFFF"; border.color: "#6B3E43"; border.width: 2.5 }
            Rectangle { x: 38; y: 2; width: 25; height: 20; radius: 10; color: "#25FFFFFF"; border.color: "#6B3E43"; border.width: 2.5 }
            Rectangle { x: 27; y: 10; width: 12; height: 3; radius: 1.5; color: "#6B3E43" }
            Rectangle { x: -7; y: 8; width: 11; height: 2; rotation: 10; color: "#6B3E43" }
            Rectangle { x: 62; y: 8; width: 11; height: 2; rotation: -10; color: "#6B3E43" }
        }
        Label { text: "✦"; color: "#D89A48"; font.pixelSize: 18; x: 112; y: 34; SequentialAnimation on opacity { loops: Animation.Infinite; NumberAnimation{to:0.25;duration:650} NumberAnimation{to:1;duration:650} } }
        Label { text: "知识魔法"; color: "#6B3E43"; font.bold: true; font.pixelSize: 11; anchors.horizontalCenter: parent.horizontalCenter; y: 124 }
        SequentialAnimation { running: studyMagicLayer.visible; loops: Animation.Infinite; NumberAnimation{target:glasses;property:"y";to:49;duration:900;easing.type:Easing.InOutSine} NumberAnimation{target:glasses;property:"y";to:51;duration:900;easing.type:Easing.InOutSine} }
    }

    function triggerMouseEvent() {
        if (root.roaming) appController.stopDesktopRoaming()
        const direction = Math.random() < 0.5 ? -1 : 1
        appController.triggerDesktopRunAway(direction)
        mouseChaser.startChase(direction)
    }

    Image {
        id: sprite
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        width: 150; height: 150
        source: appController.summaryMagicBusy
                ? "qrc:/assets/states/stellacandie_11_summary_studying_v1.png"
                : appController.currentImage
        visible: root.snackAction === "" && !root.roaming && !root.jumpPlaying && appController.currentStateIndex !== 8
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true

        transform: [
            Translate { id: floatMotion; y: 0 },
            Scale {
                id: breathMotion
                origin.x: sprite.width / 2; origin.y: sprite.height * 0.72
                xScale: 1; yScale: 1
            },
            Scale {
                id: reactionMotion
                origin.x: sprite.width / 2; origin.y: sprite.height * 0.72
                xScale: 1; yScale: 1
            },
            Rotation {
                id: headMotion
                origin.x: sprite.width / 2; origin.y: sprite.height * 0.65
                angle: 0
            }
        ]

        Behavior on opacity {
            NumberAnimation { duration: 180 }
        }
    }

    AnimatedSprite {
        id: walkSprite
        visible: root.snackAction === "" && root.roaming && appController.desktopAnimation === "walk"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        width: 150; height: 141
        source: "qrc:/assets/animations/stellacandie_walk_right.png"
        frameCount: 6
        frameWidth: 362
        frameHeight: 340
        frameDuration: 105
        interpolate: false
        running: visible
        transform: Scale {
            origin.x: walkSprite.width / 2
            origin.y: walkSprite.height / 2
            xScale: root.roamDirection
            yScale: 1
        }
    }

    AnimatedSprite {
        id: runSprite
        visible: root.snackAction === "" && root.roaming && appController.desktopAnimation === "run"
        anchors.fill: parent
        source: "qrc:/assets/animations/stellacandie_run_right.png"
        frameCount: 6; frameWidth: 362; frameHeight: 380
        frameDuration: 82; interpolate: false; running: visible
        transform: Scale {
            origin.x: runSprite.width / 2; origin.y: runSprite.height / 2
            xScale: root.roamDirection; yScale: 1
        }
    }

    AnimatedSprite {
        id: jumpSprite
        visible: root.snackAction === "" && root.jumpPlaying && !root.roaming
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        width: 150; height: 150
        source: "qrc:/assets/animations/stellacandie_jump_right.png"
        frameCount: 6; frameWidth: 362; frameHeight: 380
        frameDuration: 115; loops: 1; interpolate: false
        onFinished: root.jumpPlaying = false
        onVisibleChanged: if (visible) restart()
    }

    AnimatedSprite {
        id: sleepSprite
        visible: root.snackAction === "" && !root.roaming && !root.jumpPlaying && appController.currentStateIndex === 8
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        width: 150; height: 150
        source: "qrc:/assets/animations/stellacandie_sleep_right.png"
        frameCount: 6; frameWidth: 362; frameHeight: 380
        frameDuration: 250; loops: 1; interpolate: false
        onVisibleChanged: if (visible) restart()
    }

    Item {
        id: snackActionLayer; anchors.fill: parent; visible: root.snackAction !== "" || opacity > 0; opacity: 0; z: 5
        Rectangle {
            id: magicRing; width: 34; height: 34; radius: 17; x: 58; y: 77; color: "transparent"; border.width: 1.5
            border.color: "#F1A8B8"; opacity: root.snackAction === "processing" ? 0.7 : 0
            transform: Scale { id: ringScale; origin.x: 17; origin.y: 17; xScale: 0.5; yScale: 0.5 }
        }
        Image {
            id: snackPose; anchors.fill: parent; fillMode: Image.PreserveAspectFit; smooth: true; mipmap: true
            source: root.snackAction === "processing"
                    ? "qrc:/assets/animations/stellacandie_snack_processing_v1.png"
                    : "qrc:/assets/animations/stellacandie_snack_eating_v2.png"
            transform: [
                Translate { id: snackPoseTranslate; y: 0 },
                Scale { id: snackPoseScale; origin.x: snackPose.width/2; origin.y: snackPose.height*0.78; xScale: 1; yScale: 1 },
                Rotation { id: snackPoseRotation; origin.x: snackPose.width/2; origin.y: snackPose.height*0.75; angle: 0 }
            ]
        }
        Label {
            id: flyingSnack; text: root.activeSnackEmoji; font.pixelSize: 26
            x: 63; y: 82
            transform: Scale { id: snackIconScale; origin.x: 13; origin.y: 13; xScale: 1; yScale: 1 }
        }
        Repeater { model: 3
            Label { required property int index; text: index===0?"✦":(index===1?"·":"✧"); color: "#F2A7B5"; font.pixelSize: 15
                opacity: root.snackAction === "processing" ? 0.85 : 0.25; x: 48+index*24; y: 70+(index%2)*18
                Behavior on opacity { NumberAnimation { duration: 180 } } }
        }
    }

    SequentialAnimation {
        id: processingSnackAnimation
        ScriptAction { script: { root.snackAction="processing"; snackActionLayer.opacity=0; flyingSnack.x=63; flyingSnack.y=82; flyingSnack.rotation=0; snackPoseTranslate.y=2; snackPoseRotation.angle=0; snackPoseScale.xScale=0.985; snackPoseScale.yScale=0.985; snackIconScale.xScale=0.25; snackIconScale.yScale=0.25; ringScale.xScale=0.5; ringScale.yScale=0.5; flyingSnack.opacity=0 } }
        ParallelAnimation {
            NumberAnimation { target: snackActionLayer; property: "opacity"; to: 1; duration: 170; easing.type: Easing.OutCubic }
            NumberAnimation { target: snackPoseTranslate; property: "y"; to: 0; duration: 260; easing.type: Easing.OutCubic }
            NumberAnimation { target: snackPoseScale; property: "xScale"; to: 1; duration: 260; easing.type: Easing.OutCubic }
            NumberAnimation { target: snackPoseScale; property: "yScale"; to: 1; duration: 260; easing.type: Easing.OutCubic }
        }
        ParallelAnimation {
            NumberAnimation { target: flyingSnack; property: "opacity"; to: 1; duration: 220 }
            NumberAnimation { target: snackIconScale; property: "xScale"; to: 1.05; duration: 520; easing.type: Easing.OutBack }
            NumberAnimation { target: snackIconScale; property: "yScale"; to: 1.05; duration: 520; easing.type: Easing.OutBack }
            NumberAnimation { target: ringScale; property: "xScale"; to: 1.25; duration: 680; easing.type: Easing.OutCubic }
            NumberAnimation { target: ringScale; property: "yScale"; to: 1.25; duration: 680; easing.type: Easing.OutCubic }
            NumberAnimation { target: snackPoseRotation; property: "angle"; to: -1.1; duration: 480; easing.type: Easing.InOutSine }
        }
        ParallelAnimation {
            NumberAnimation { target: snackPoseRotation; property: "angle"; to: 1.1; duration: 480; easing.type: Easing.InOutSine }
            NumberAnimation { target: flyingSnack; property: "rotation"; to: 240; duration: 720; easing.type: Easing.InOutSine }
        }
        ParallelAnimation {
            NumberAnimation { target: snackPoseRotation; property: "angle"; to: 0; duration: 260 }
            NumberAnimation { target: snackIconScale; property: "xScale"; to: 0.95; duration: 260 }
            NumberAnimation { target: snackIconScale; property: "yScale"; to: 0.95; duration: 260 }
        }
        PauseAnimation { duration: 280 }
        NumberAnimation { target: snackActionLayer; property: "opacity"; to: 0; duration: 150; easing.type: Easing.InCubic }
        ScriptAction { script: root.snackAction="" }
    }

    SequentialAnimation {
        id: eatingSnackAnimation
        ScriptAction { script: { root.snackAction="eating"; snackActionLayer.opacity=0; flyingSnack.opacity=0; flyingSnack.x=12; flyingSnack.y=18; flyingSnack.rotation=-18; snackPoseTranslate.y=2; snackPoseRotation.angle=0; snackIconScale.xScale=0.8; snackIconScale.yScale=0.8; snackPoseScale.xScale=0.99; snackPoseScale.yScale=0.99 } }
        ParallelAnimation {
            NumberAnimation { target: snackActionLayer; property: "opacity"; to: 1; duration: 160 }
            NumberAnimation { target: snackPoseTranslate; property: "y"; to: 0; duration: 220; easing.type: Easing.OutCubic }
            NumberAnimation { target: flyingSnack; property: "opacity"; to: 1; duration: 120 }
        }
        ParallelAnimation {
            NumberAnimation { target: flyingSnack; property: "x"; to: 63; duration: 420; easing.type: Easing.InOutCubic }
            NumberAnimation { target: flyingSnack; property: "y"; to: 55; duration: 420; easing.type: Easing.InCubic }
            NumberAnimation { target: flyingSnack; property: "rotation"; to: 12; duration: 420; easing.type: Easing.InOutSine }
            NumberAnimation { target: snackIconScale; property: "xScale"; to: 0.42; duration: 440 }
            NumberAnimation { target: snackIconScale; property: "yScale"; to: 0.42; duration: 440 }
        }
        NumberAnimation { target: flyingSnack; property: "opacity"; to: 0; duration: 90 }
        SequentialAnimation {
            loops: 3
            ParallelAnimation { NumberAnimation { target: snackPoseScale; property: "xScale"; to: 1.012; duration: 105; easing.type: Easing.OutQuad } NumberAnimation { target: snackPoseScale; property: "yScale"; to: 0.988; duration: 105; easing.type: Easing.OutQuad } NumberAnimation { target: snackPoseTranslate; property: "y"; to: 1.2; duration: 105 } }
            ParallelAnimation { NumberAnimation { target: snackPoseScale; property: "xScale"; to: 1; duration: 145; easing.type: Easing.InOutSine } NumberAnimation { target: snackPoseScale; property: "yScale"; to: 1; duration: 145; easing.type: Easing.InOutSine } NumberAnimation { target: snackPoseTranslate; property: "y"; to: 0; duration: 145 } }
        }
        NumberAnimation { target: snackPoseRotation; property: "angle"; to: 0.8; duration: 240; easing.type: Easing.OutCubic }
        PauseAnimation { duration: 300 }
        NumberAnimation { target: snackActionLayer; property: "opacity"; to: 0; duration: 170; easing.type: Easing.InCubic }
        ScriptAction { script: { snackPoseRotation.angle=0; root.snackAction="" } }
    }

    Timer {
        id: specialAnimationTimer
        interval: 16000; repeat: true; running: root.visible
        onTriggered: {
            interval = 13000 + Math.floor(Math.random() * 13000)
            const state = appController.currentStateIndex
            if (root.snackAction === "" && !root.roaming && !chatWindow.visible && (state === 2 || state === 3 || state === 6)) {
                root.jumpPlaying = true
                jumpSprite.restart()
            }
        }
    }

    // Long, low-amplitude loops keep the desktop pet alive without becoming distracting.
    SequentialAnimation {
        id: breathingAnimation
        running: root.visible && !root.reducedMotion
        loops: Animation.Infinite
        ParallelAnimation {
            NumberAnimation { target: breathMotion; property: "xScale"; to: 1.012; duration: 1750; easing.type: Easing.InOutSine }
            NumberAnimation { target: breathMotion; property: "yScale"; to: 0.992; duration: 1750; easing.type: Easing.InOutSine }
        }
        ParallelAnimation {
            NumberAnimation { target: breathMotion; property: "xScale"; to: 1.0; duration: 1850; easing.type: Easing.InOutSine }
            NumberAnimation { target: breathMotion; property: "yScale"; to: 1.0; duration: 1850; easing.type: Easing.InOutSine }
        }
    }

    SequentialAnimation {
        id: floatingAnimation
        running: root.visible && !root.reducedMotion
        loops: Animation.Infinite
        NumberAnimation { target: floatMotion; property: "y"; to: -3; duration: 2300; easing.type: Easing.InOutSine }
        NumberAnimation { target: floatMotion; property: "y"; to: 0; duration: 2500; easing.type: Easing.InOutSine }
    }

    SequentialAnimation {
        id: tapAnimation
        ParallelAnimation {
            NumberAnimation { target: reactionMotion; property: "xScale"; to: 1.045; duration: 90; easing.type: Easing.OutQuad }
            NumberAnimation { target: reactionMotion; property: "yScale"; to: 0.955; duration: 90; easing.type: Easing.OutQuad }
        }
        ParallelAnimation {
            NumberAnimation { target: reactionMotion; property: "xScale"; to: 0.985; duration: 100; easing.type: Easing.InOutQuad }
            NumberAnimation { target: reactionMotion; property: "yScale"; to: 1.025; duration: 100; easing.type: Easing.InOutQuad }
        }
        ParallelAnimation {
            NumberAnimation { target: reactionMotion; property: "xScale"; to: 1; duration: 130; easing.type: Easing.OutBack }
            NumberAnimation { target: reactionMotion; property: "yScale"; to: 1; duration: 130; easing.type: Easing.OutBack }
        }
    }

    SequentialAnimation {
        id: curiousAnimation
        NumberAnimation { target: headMotion; property: "angle"; to: -2.8; duration: 190; easing.type: Easing.OutQuad }
        PauseAnimation { duration: 420 }
        NumberAnimation { target: headMotion; property: "angle"; to: 2.0; duration: 260; easing.type: Easing.InOutQuad }
        NumberAnimation { target: headMotion; property: "angle"; to: 0; duration: 220; easing.type: Easing.OutQuad }
    }

    SequentialAnimation {
        id: happyHopAnimation
        NumberAnimation { target: floatMotion; property: "y"; to: -10; duration: 130; easing.type: Easing.OutQuad }
        NumberAnimation { target: floatMotion; property: "y"; to: 0; duration: 190; easing.type: Easing.OutBounce }
        PauseAnimation { duration: 70 }
        NumberAnimation { target: floatMotion; property: "y"; to: -5; duration: 100; easing.type: Easing.OutQuad }
        NumberAnimation { target: floatMotion; property: "y"; to: 0; duration: 150; easing.type: Easing.OutBounce }
    }

    SequentialAnimation {
        id: grumpyAnimation
        NumberAnimation { target: headMotion; property: "angle"; to: -2.2; duration: 80 }
        NumberAnimation { target: headMotion; property: "angle"; to: 2.2; duration: 80 }
        NumberAnimation { target: headMotion; property: "angle"; to: -1.5; duration: 70 }
        NumberAnimation { target: headMotion; property: "angle"; to: 0; duration: 100 }
    }

    Timer {
        id: spontaneousMotionTimer
        running: root.visible && !root.reducedMotion
        interval: 9000; repeat: true
        onTriggered: {
            interval = 7000 + Math.floor(Math.random() * 7000)
            const state = appController.currentStateIndex
            if (root.snackAction !== "") return
            if (state === 2 || state === 6) happyHopAnimation.restart()
            else if (state === 4 || state === 5) grumpyAnimation.restart()
            else curiousAnimation.restart()
        }
    }

    MouseArea {
        id: dragArea
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onPressed: function(mouse) {
            if (mouse.button === Qt.LeftButton) {
                appController.stopDesktopRoaming()
                root.startSystemMove()
            }
        }

        onClicked: function(mouse) {
            if (mouse.button === Qt.RightButton) {
                root.debugVisible = !root.debugVisible
                if (root.debugVisible) debugWindow.openNearPet()
                else debugWindow.hide()
            } else {
                if (!root.reducedMotion) tapAnimation.restart()
                chatWindow.toggleNearPet()
            }
        }
    }

    ChatWindow {
        id: chatWindow
        petWindow: root
        onOpenSettingsRequested: settingsWindow.openAndRaise()
    }

    SettingsWindow {
        id: settingsWindow
    }

    DiaryWindow { id: diaryWindow }
    Loader { id: memoryWindowLoader; active: false; source: "MemoryWindow.qml" }
    ProactiveWindow { id: proactiveWindow }
    LearningWindow { id: learningWindow }
    FileSnackWindow { id: fileSnackWindow }
    Loader { id: dataCleanupLoader; active: false; source: "DataCleanupWindow.qml" }
    Loader { id: summaryMagicLoader; active: false; source: "SummaryMagicWindow.qml" }
    Loader { id: dreamBottleLoader; active: false; source: "DreamBottleWindow.qml" }
    Loader { id: morningLollipopLoader; active: false; source: "MorningLollipopWindow.qml" }
    DebugWindow {
        id: debugWindow
        petWindow: root
        onClosing: root.debugVisible = false
        onRequestMouseEvent: root.triggerMouseEvent()
    }
    MouseChaser { id: mouseChaser; petWindow: root }

    Timer {
        id: randomDesktopEventTimer
        interval: 18000; repeat: true; running: root.visible
        onTriggered: {
            interval = 22000 + Math.floor(Math.random() * 24000)
            if (root.snackAction !== "" || root.roaming || root.jumpPlaying || chatWindow.visible || debugWindow.visible
                    || appController.currentStateIndex === 8) return
            if (Math.random() < 0.52) {
                root.triggerMouseEvent()
            }
        }
    }


    Connections {
        target: appController
        function onRequestSettingsWindow() { settingsWindow.openAndRaise() }
        function onRequestDiaryWindow() { diaryWindow.openAndRaise() }
        function onRequestMemoryWindow() { memoryWindowLoader.active=true; Qt.callLater(function(){if(memoryWindowLoader.item)memoryWindowLoader.item.openAndRaise()}) }
        function onRequestProactiveWindow() { proactiveWindow.openAndRaise() }
        function onRequestLearningWindow() { learningWindow.openAndRaise() }
        function onRequestFileSnackWindow() { fileSnackWindow.openAndRaise() }
        function onRequestDataCleanupWindow() { dataCleanupLoader.active=true; Qt.callLater(function(){if(dataCleanupLoader.item)dataCleanupLoader.item.openAndRaise()}) }
        function onRequestSummaryMagicWindow() { summaryMagicLoader.active=true; Qt.callLater(function(){if(summaryMagicLoader.item)summaryMagicLoader.item.openAndRaise()}) }
        function onRequestDreamWindow() { dreamBottleLoader.active=true; Qt.callLater(function(){if(dreamBottleLoader.item)dreamBottleLoader.item.openAndRaise()}) }
        function onRequestMorningLollipopWindow() { morningLollipopLoader.active=true; Qt.callLater(function(){if(morningLollipopLoader.item)morningLollipopLoader.item.openAndRaise()}) }
        function onSummaryMagicChanged() { if(appController.summaryMagicBusy){appController.stopDesktopRoaming();root.jumpPlaying=false} }
        function onSnackProcessingRequested(emoji) {
            appController.stopDesktopRoaming(); root.jumpPlaying=false; root.activeSnackEmoji=emoji; processingSnackAnimation.restart()
        }
        function onSnackEatingRequested(emoji) {
            appController.stopDesktopRoaming(); root.jumpPlaying=false; root.activeSnackEmoji=emoji; eatingSnackAnimation.restart()
        }
        function onRequestChatWindow() { chatWindow.openNearPet() }
        function onCurrentStateChanged() {
            if (root.reducedMotion) return
            const state = appController.currentStateIndex
            if (state === 2 || state === 6 || state === 10) happyHopAnimation.restart()
            else if (state === 4 || state === 5) grumpyAnimation.restart()
            else curiousAnimation.restart()
        }
    }

    Timer {
        id: savePositionTimer
        interval: 400
        repeat: false
        onTriggered: appController.saveWindowPosition()
    }

    onXChanged: savePositionTimer.restart()
    onYChanged: savePositionTimer.restart()

    Shortcut {
        sequence: "Ctrl+Right"
        onActivated: appController.nextState()
    }

    Shortcut {
        sequence: "Ctrl+Left"
        onActivated: appController.previousState()
    }

    Shortcut {
        sequence: "Ctrl+D"
        onActivated: {
            root.debugVisible = !root.debugVisible
            if (root.debugVisible) debugWindow.openNearPet()
            else debugWindow.hide()
        }
    }

    Shortcut {
        sequence: "Ctrl+Q"
        onActivated: appController.quitApplication()
    }
}
