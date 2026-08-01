import { _decorator, Component, Node, tween, Vec3 } from 'cc';
import { roomEvents, RoomEvent } from '../core/EventBus';
import { InteractionEvent } from '../core/PetTypes';
const { ccclass } = _decorator;

@ccclass('PetInteraction')
export class PetInteraction extends Component {
    start() { this.node.on(Node.EventType.MOUSE_UP, this.onPet, this); this.node.on(Node.EventType.TOUCH_END, this.onPet, this); }
    onDestroy() { this.node.off(Node.EventType.MOUSE_UP, this.onPet, this); this.node.off(Node.EventType.TOUCH_END, this.onPet, this); }
    private onPet() {
        tween(this.node).to(0.13, { scale: new Vec3(1.06, 0.96, 1) }).to(0.28, { scale: Vec3.ONE }, { easing: 'backOut' }).start();
        const event: InteractionEvent = {
            protocolVersion: 1,
            eventId: `${Date.now()}-${Math.random().toString(16).slice(2)}`,
            occurredAt: new Date().toISOString(), interaction: 'petted', detail: 'head',
        };
        roomEvents.emit(RoomEvent.INTERACTION, event);
    }
}
