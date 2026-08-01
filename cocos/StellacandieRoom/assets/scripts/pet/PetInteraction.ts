import { _decorator, Color, Component, Label, Node, tween, UIOpacity, UITransform, Vec3 } from 'cc';
import { roomEvents, RoomEvent } from '../core/EventBus';
import { InteractionEvent } from '../core/PetTypes';
const { ccclass } = _decorator;

@ccclass('PetInteraction')
export class PetInteraction extends Component {
    start() { this.node.on(Node.EventType.MOUSE_UP, this.onPet, this); this.node.on(Node.EventType.TOUCH_END, this.onPet, this); }
    onDestroy() { this.node.off(Node.EventType.MOUSE_UP, this.onPet, this); this.node.off(Node.EventType.TOUCH_END, this.onPet, this); }
    private onPet() {
        tween(this.node).to(0.13, { scale: new Vec3(1.06, 0.96, 1) }).to(0.28, { scale: Vec3.ONE }, { easing: 'backOut' }).start();
        this.playAffectionParticles();
        const event: InteractionEvent = {
            protocolVersion: 1,
            eventId: `${Date.now()}-${Math.random().toString(16).slice(2)}`,
            occurredAt: new Date().toISOString(), interaction: 'petted', detail: 'head',
        };
        roomEvents.emit(RoomEvent.INTERACTION, event);
    }

    private playAffectionParticles() {
        const symbols = ['♥', '✦', '♥'];
        symbols.forEach((symbol, index) => {
            const particle = new Node(`Affection-${index}`);
            particle.parent = this.node;
            particle.setPosition(-38 + index * 38, 78 + (index % 2) * 10);
            particle.addComponent(UITransform).setContentSize(36, 36);
            const opacity = particle.addComponent(UIOpacity);
            const label = particle.addComponent(Label);
            label.string = symbol;
            label.fontSize = symbol === '♥' ? 28 : 23;
            label.color = symbol === '♥' ? new Color(238, 150, 169) : new Color(247, 204, 105);
            particle.setScale(new Vec3(0.45, 0.45, 1));
            tween(particle)
                .delay(index * 0.06)
                .parallel(
                    tween().to(0.42, { position: new Vec3(particle.position.x + (index - 1) * 18, 150 + index * 8, 0) }, { easing: 'sineOut' }),
                    tween().to(0.24, { scale: new Vec3(1.05, 1.05, 1) }, { easing: 'backOut' })
                        .to(0.28, { scale: new Vec3(0.82, 0.82, 1) })
                )
                .call(() => particle.destroy())
                .start();
            tween(opacity).delay(0.28 + index * 0.06).to(0.26, { opacity: 0 }).start();
        });
    }
}
