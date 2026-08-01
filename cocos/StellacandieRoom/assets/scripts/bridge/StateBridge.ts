import { _decorator, Component, sys } from 'cc';
import { DEFAULT_SNAPSHOT, InteractionEvent, PetSnapshot } from '../core/PetTypes';
import { roomEvents, RoomEvent } from '../core/EventBus';
const { ccclass, property } = _decorator;

declare const jsb: any;

@ccclass('StateBridge')
export class StateBridge extends Component {
    @property({ tooltip: 'Qt导出的状态文件；留空时使用用户数据目录' })
    public stateFile = '';
    @property public pollSeconds = 1;
    private snapshot: PetSnapshot = { ...DEFAULT_SNAPSHOT };
    private lastText = '';

    public get current(): Readonly<PetSnapshot> { return this.snapshot; }

    start() {
        this.readSnapshot();
        this.schedule(() => this.readSnapshot(), Math.max(0.25, this.pollSeconds));
        roomEvents.on(RoomEvent.INTERACTION, this.writeInteraction, this);
    }

    onDestroy() { roomEvents.off(RoomEvent.INTERACTION, this.writeInteraction, this); }

    private dataDir(): string {
        if (this.stateFile) return this.stateFile.replace(/[\\/][^\\/]+$/, '');
        if (typeof jsb !== 'undefined') return `${jsb.fileUtils.getWritablePath()}stellacandie_bridge`;
        return '';
    }

    private readSnapshot() {
        let text = '';
        if (typeof jsb !== 'undefined') {
            const path = this.stateFile || `${this.dataDir()}/pet_state.json`;
            if (jsb.fileUtils.isFileExist(path)) text = jsb.fileUtils.getStringFromFile(path);
        } else {
            text = sys.localStorage.getItem('stellacandie.pet_state') || '';
        }
        if (!text || text === this.lastText) return;
        try {
            const raw = JSON.parse(text);
            if (raw.protocolVersion !== 1) return;
            this.snapshot = { ...DEFAULT_SNAPSHOT, ...raw };
            this.lastText = text;
            roomEvents.emit(RoomEvent.SNAPSHOT_CHANGED, this.snapshot);
        } catch (error) { console.warn('[StateBridge] invalid pet_state.json', error); }
    }

    private writeInteraction(event: InteractionEvent) {
        const text = JSON.stringify(event, null, 2);
        if (typeof jsb !== 'undefined') {
            const dir = this.dataDir();
            if (!jsb.fileUtils.isDirectoryExist(dir)) jsb.fileUtils.createDirectory(dir);
            jsb.fileUtils.writeStringToFile(text, `${dir}/interaction_result.json`);
        } else sys.localStorage.setItem('stellacandie.interaction_result', text);
    }
}
