import { _decorator, Component, Node, resources, Sprite, SpriteFrame, UITransform } from 'cc';
import { StateBridge } from './bridge/StateBridge';
import { PetBehaviorController } from './pet/PetBehaviorController';
import { PetInteraction } from './pet/PetInteraction';
import { PetFaceRig } from './pet/PetFaceRig';
const { ccclass } = _decorator;

@ccclass('RoomBootstrap')
export class RoomBootstrap extends Component {
    start() {
        this.node.addComponent(StateBridge);
        this.createRoomBackground();
        this.createPet();
    }

    private createRoomBackground() {
        const background = new Node('HealingRoomBackground');
        background.parent = this.node;
        background.setPosition(0, 0);
        background.addComponent(UITransform).setContentSize(1160, 652);
        const sprite = background.addComponent(Sprite);
        sprite.sizeMode = Sprite.SizeMode.CUSTOM;
        resources.load('room/healing_room_v1/spriteFrame', SpriteFrame, (error, frame) => {
            if (error || !frame) {
                console.error('[RoomBootstrap] healing room unavailable', error);
                return;
            }
            sprite.spriteFrame = frame;
        });
    }

    private createPet() {
        const pet = new Node('Stellacandie');
        pet.parent = this.node;
        pet.setPosition(-70, -145);
        pet.addComponent(UITransform).setContentSize(280, 280);

        const bodyRoot = new Node('BodyRoot');
        bodyRoot.parent = pet;
        bodyRoot.addComponent(UITransform).setContentSize(280, 280);
        const sprite = bodyRoot.addComponent(Sprite);
        sprite.sizeMode = Sprite.SizeMode.CUSTOM;

        const headDetailRoot = new Node('HeadDetailRoot');
        headDetailRoot.parent = bodyRoot;
        headDetailRoot.addComponent(UITransform).setContentSize(280, 280);
        const faceRig = headDetailRoot.addComponent(PetFaceRig);

        const behavior = pet.addComponent(PetBehaviorController);
        behavior.body = sprite;
        behavior.detailLayer = bodyRoot;
        behavior.faceRig = faceRig;
        pet.addComponent(PetInteraction);

        resources.loadDir('states', SpriteFrame, (error, frames) => {
            if (error || !frames.length) {
                console.error('[RoomBootstrap] state images unavailable', error);
                return;
            }
            const ordered = frames.sort((a, b) => a.name.localeCompare(b.name));
            behavior.emotionFrames = ordered;
            behavior.setInitialFrame(ordered[0]);
        });
    }
}
