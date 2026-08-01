import { EventTarget } from 'cc';

export const roomEvents = new EventTarget();
export const RoomEvent = {
    SNAPSHOT_CHANGED: 'snapshot-changed',
    INTERACTION: 'interaction',
    BEHAVIOR_CHANGED: 'behavior-changed',
} as const;
