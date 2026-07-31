import QtQuick
import QtQuick.Window

Window {
    id: window
    required property Window petWindow
    property int chaseDirection: 1
    width: 72; height: 52
    visible: false; color: "transparent"
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowTransparentForInput

    function startChase(direction) {
        chaseDirection = direction
        x = chaseX()
        y = petWindow.y + petWindow.height - height - 4
        show(); chaseDuration.restart()
    }

    function chaseX() {
        return chaseDirection > 0
                ? petWindow.x - width + 8
                : petWindow.x + petWindow.width - 8
    }

    AnimatedSprite {
        id: mouseSprite
        anchors.fill: parent
        source: "qrc:/assets/animations/mouse_run_right.png"
        frameCount: 4; frameWidth: 543; frameHeight: 380
        frameDuration: 86; running: window.visible; interpolate: false
        transform: Scale {
            origin.x: mouseSprite.width / 2; origin.y: mouseSprite.height / 2
            xScale: window.chaseDirection; yScale: 1
        }
    }

    Timer {
        id: followTimer; interval: 16; repeat: true; running: window.visible
        onTriggered: {
            window.x = window.chaseX()
            window.y = petWindow.y + petWindow.height - window.height - 4
        }
    }
    Timer { id: chaseDuration; interval: 4200; onTriggered: window.hide() }
}
