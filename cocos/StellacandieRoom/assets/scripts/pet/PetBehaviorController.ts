import { _decorator, Component, Node, Sprite, SpriteFrame, tween, Tween, Vec3 } from 'cc';
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
    private snapshot: PetSnapshot | null = null;
    private detailTimer = 0;
    private readonly visualBaseScale = new Vec3(0.62, 0.62, 1);

    start() {
        roomEvents.on(RoomEvent.SNAPSHOT_CHANGED, this.onSnapshot, this);
        this.schedule(() => this.chooseAutonomousBehavior(), 3);
        this.schedule(() => this.playRandomIdleDetail(), 5);
        this.playBreathing();
    }

    onDestroy() { roomEvents.off(RoomEvent.SNAPSHOT_CHANGED, this.onSnapshot, this); }

    private onSnapshot(snapshot: PetSnapshot) {
        this.snapshot = snapshot;
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

    private visualTarget(): Node { return this.detailLayer ?? this.node; }

    private playBreathing() {
        const target = this.visualTarget();
        Tween.stopAllByTarget(target);
        target.setScale(this.visualBaseScale);
        tween(target).repeatForever(
            tween().to(1.45, { scale: new Vec3(0.631, 0.613, 1) }, { easing: 'sineInOut' })
                .to(1.45, { scale: this.visualBaseScale }, { easing: 'sineInOut' })
        ).start();
    }

    private sickBreathing() {
        const target = this.visualTarget();
        Tween.stopAllByTarget(target);
        tween(target).repeatForever(
            tween().to(2.3, { scale: new Vec3(0.625, 0.617, 1) })
                .to(2.3, { scale: this.visualBaseScale })
        ).start();
    }

    private curlDown() {
        const target = this.visualTarget();
        Tween.stopAllByTarget(target);
        tween(target).to(0.55, { scale: new Vec3(0.645, 0.508, 1) }, { easing: 'quadOut' }).start();
    }

    private walkToRandomPoint() {
        const x = -330 + Math.random() * 660;
        Tween.stopAllByTarget(this.node);
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
        const target = this.visualTarget();
        tween(target).by(0.2, { angle: 3 }).by(0.35, { angle: -6 }).by(0.2, { angle: 3 }).start();
    }
    private stretch() {
        const target = this.visualTarget();
        Tween.stopAllByTarget(target);
        tween(target).to(0.35, { scale: new Vec3(0.67, 0.583, 1) }, { easing: 'quadOut' })
            .to(0.45, { scale: this.visualBaseScale }, { easing: 'backOut' })
            .call(() => this.playBreathing()).start();
    }
    private pulseDetail(seconds: number, scale: Vec3) {
        if (!this.detailLayer) return;
        const scaled = new Vec3(this.visualBaseScale.x * scale.x, this.visualBaseScale.y * scale.y, 1);
        tween(this.detailLayer).to(seconds, { scale: scaled }).to(seconds, { scale: this.visualBaseScale }).start();
    }
}
