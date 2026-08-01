import { _decorator, Color, Component, Graphics, Node, tween, UIOpacity, UITransform, Vec3 } from 'cc';
const { ccclass } = _decorator;

@ccclass('PetFaceRig')
export class PetFaceRig extends Component {
    private eyelids: Node[] = [];
    private playing = false;

    start() {
        this.eyelids = [this.createEyelid('LeftEyelid', -55), this.createEyelid('RightEyelid', 52)];
        this.eyelids.forEach(lid => lid.setScale(new Vec3(1, 0.03, 1)));
    }

    public blink(doubleBlink = false) {
        if (this.playing || this.eyelids.length !== 2) return;
        this.playing = true;
        this.playBlinkPass(() => {
            if (!doubleBlink) {
                this.playing = false;
                return;
            }
            this.scheduleOnce(() => this.playBlinkPass(() => { this.playing = false; }), 0.11);
        });
    }

    private playBlinkPass(done: () => void) {
        let finished = 0;
        this.eyelids.forEach(lid => {
            tween(lid)
                .to(0.075, { scale: new Vec3(1, 0.56, 1) }, { easing: 'sineIn' })
                .to(0.055, { scale: Vec3.ONE }, { easing: 'quadIn' })
                .delay(0.055)
                .to(0.065, { scale: new Vec3(1, 0.48, 1) }, { easing: 'quadOut' })
                .to(0.085, { scale: new Vec3(1, 0.03, 1) }, { easing: 'sineOut' })
                .call(() => { if (++finished === this.eyelids.length) done(); })
                .start();
        });
    }

    private createEyelid(name: string, x: number): Node {
        const node = new Node(name);
        node.parent = this.node;
        node.setPosition(x, 48);
        const transform = node.addComponent(UITransform);
        transform.setContentSize(42, 25);
        transform.setAnchorPoint(0.5, 1);
        node.addComponent(UIOpacity).opacity = 255;
        const g = node.addComponent(Graphics);
        g.fillColor = new Color(252, 239, 226, 255);
        g.strokeColor = new Color(91, 48, 48, 255);
        g.lineWidth = 2.2;
        g.moveTo(-20, 0);
        g.bezierCurveTo(-12, -15, 12, -15, 20, 0);
        g.bezierCurveTo(11, 8, -11, 8, -20, 0);
        g.fill();
        g.moveTo(-18, 0);
        g.bezierCurveTo(-8, -6, 8, -6, 18, 0);
        g.stroke();
        return node;
    }
}
