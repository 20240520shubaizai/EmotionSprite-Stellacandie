import { _decorator, Color, Component, Graphics, Label, Node, resources, Sprite, SpriteFrame, UITransform, Vec3 } from 'cc';
import { StateBridge } from './bridge/StateBridge';
import { PetBehaviorController } from './pet/PetBehaviorController';
import { PetInteraction } from './pet/PetInteraction';
const { ccclass } = _decorator;

@ccclass('RoomBootstrap')
export class RoomBootstrap extends Component {
    start() {
        this.node.addComponent(StateBridge);
        this.drawRoom();
        this.createPet();
    }

    private drawRoom() {
        const background = new Node('HealingRoom');
        background.parent = this.node;
        const g = background.addComponent(Graphics);
        g.fillColor = this.color('#FFF6EC'); g.roundRect(-480, -270, 960, 540, 30); g.fill();
        g.fillColor = this.color('#E8CFC0'); g.roundRect(-440, -220, 245, 145, 22); g.fill();
        g.fillColor = this.color('#D9EAF1'); g.roundRect(190, 45, 220, 170, 18); g.fill();
        g.fillColor = this.color('#F2DEAE'); g.roundRect(-40, -210, 220, 105, 18); g.fill();
        this.label('小床', -320, -160);
        this.label('窗边', 300, 125);
        this.label('零食角', 70, -165);
    }

    private label(text: string, x: number, y: number) {
        const node = new Node(text); node.parent = this.node; node.setPosition(x, y);
        const label = node.addComponent(Label); label.string = text; label.fontSize = 22;
        label.color = this.color('#7B4D4D');
    }

    private color(hex: string): Color { return Color.fromHEX(new Color(), hex); }

    private createPet() {
        const pet = new Node('Stellacandie'); pet.parent = this.node; pet.setPosition(0, -35);
        pet.addComponent(UITransform).setContentSize(420, 420);
        const visual = new Node('Visual'); visual.parent = pet; visual.setScale(new Vec3(0.62, 0.62, 1));
        visual.addComponent(UITransform).setContentSize(420, 420);
        const sprite = visual.addComponent(Sprite);
        const behavior = pet.addComponent(PetBehaviorController); behavior.body = sprite; behavior.detailLayer = visual;
        pet.addComponent(PetInteraction);
        resources.loadDir('states', SpriteFrame, (error, frames) => {
            if (error || !frames.length) { console.error('[RoomBootstrap] state images unavailable', error); return; }
            const ordered = frames.sort((a, b) => a.name.localeCompare(b.name));
            behavior.emotionFrames = ordered;
            sprite.spriteFrame = ordered[0];
        });
    }
}
