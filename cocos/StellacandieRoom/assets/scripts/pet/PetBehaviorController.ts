import { _decorator, Component, Node, Sprite, SpriteFrame, tween, Vec3 } from 'cc';
import { PetEmotion, PetSnapshot } from '../core/PetTypes';
import { roomEvents, RoomEvent } from '../core/EventBus';
const { ccclass, property } = _decorator;

type Behavior = 'idle' | 'wander' | 'sleep' | 'eat' | 'study' | 'sick' | 'react';

@ccclass('PetBehaviorController')
export class PetBehaviorController extends Component {
    @property(Sprite) public body: Sprite | null = null;
    @property(Node) public detailLayer: Node | null = null;
    @property([SpriteFrame]) public emotionFrames: SpriteFrame[] = [];
    private behavior: Behavior = 'idle';
    private emotion: PetEmotion = 'neutral';
    private snapshot: PetSnapshot | null = null;
    private detailTimer = 0;

    start() {
        roomEvents.on(RoomEvent.SNAPSHOT_CHANGED, this.onSnapshot, this);
        this.schedule(() => this.chooseAutonomousBehavior(), 3);
        this.schedule(() => this.playRandomIdleDetail(), 5);
        this.playBreathing();
    }

    onDestroy() { roomEvents.off(RoomEvent.SNAPSHOT_CHANGED, this.onSnapshot, this); }

    private onSnapshot(snapshot: PetSnapshot) {
        this.snapshot = snapshot;
        this.emotion = snapshot.emotion;
        this.applyEmotionFrame(snapshot.emotion);
        this.chooseAutonomousBehavior();
    }

    private chooseAutonomousBehavior() {
        const s = this.snapshot;
        let next: Behavior = 'idle';
        if (s?.health !== undefined && s.health < 45) next = 'sick';
        else if (s?.energy !== undefined && s.energy < 22) next = 'sleep';
        else if (s?.activity === 'studying' || s?.activity === 'summarizing') next = 'study';
        else if (s?.satiety !== undefined && s.satiety < 20) next = 'eat';
        else if ((s?.mood ?? 0) > 78 && Math.random() < 0.35) next = 'wander';
        this.changeBehavior(next);
    }

    private changeBehavior(next: Behavior) {
        if (next === this.behavior) return;
        this.behavior = next;
        roomEvents.emit(RoomEvent.BEHAVIOR_CHANGED, next);
        if (next === 'wander') this.walkToRandomPoint();
        else if (next === 'sleep') this.curlDown();
        else if (next === 'sick') this.sickBreathing();
        else this.playBreathing();
    }

    private applyEmotionFrame(emotion: PetEmotion) {
        const order: PetEmotion[] = ['neutral','happy','curious','angry','pouting','affectionate','shy','sleepy','scared','sick','recovering','studying'];
        const frame = this.emotionFrames[order.indexOf(emotion)];
        if (frame && this.body) this.body.spriteFrame = frame;
    }

    private playBreathing() {
        tween(this.node).stop();
        this.node.setScale(Vec3.ONE);
        tween(this.node).repeatForever(
            tween().to(1.45, { scale: new Vec3(1.018, 0.988, 1) }, { easing: 'sineInOut' })
                .to(1.45, { scale: Vec3.ONE }, { easing: 'sineInOut' })
        ).start();
    }

    private sickBreathing() {
        tween(this.node).stop();
        tween(this.node).repeatForever(
            tween().to(2.3, { scale: new Vec3(1.008, 0.995, 1) }).to(2.3, { scale: Vec3.ONE })
        ).start();
    }

    private curlDown() {
        tween(this.node).stop();
        tween(this.node).to(0.55, { scale: new Vec3(1.04, 0.82, 1) }, { easing: 'quadOut' }).start();
    }

    private walkToRandomPoint() {
        const x = -330 + Math.random() * 660;
        tween(this.node).to(2.2, { position: new Vec3(x, this.node.position.y, 0) }, { easing: 'sineInOut' })
            .call(() => this.changeBehavior('idle')).start();
    }

    private playRandomIdleDetail() {
        if (this.behavior !== 'idle' || ++this.detailTimer % 2 !== 0) return;
        const roll = Math.random();
        if (roll < 0.35) this.blink();
        else if (roll < 0.6) this.earTwitch();
        else if (roll < 0.82) this.lookAround();
        else this.stretch();
    }

    private blink() { this.pulseDetail(0.16, new Vec3(1, 0.96, 1)); }
    private earTwitch() { this.pulseDetail(0.12, new Vec3(1.015, 1.01, 1)); }
    private lookAround() {
        tween(this.node).by(0.2, { angle: 3 }).by(0.35, { angle: -6 }).by(0.2, { angle: 3 }).start();
    }
    private stretch() {
        tween(this.node).to(0.35, { scale: new Vec3(1.08, 0.94, 1) }, { easing: 'quadOut' })
            .to(0.45, { scale: Vec3.ONE }, { easing: 'backOut' }).start();
    }
    private pulseDetail(seconds: number, scale: Vec3) {
        if (!this.detailLayer) return;
        tween(this.detailLayer).to(seconds, { scale }).to(seconds, { scale: Vec3.ONE }).start();
    }
}
