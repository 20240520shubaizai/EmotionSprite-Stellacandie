export type PetEmotion = 'neutral' | 'happy' | 'curious' | 'angry' | 'pouting' |
    'affectionate' | 'shy' | 'sleepy' | 'scared' | 'sick' | 'recovering' | 'studying';

export interface PetSnapshot {
    protocolVersion: number;
    revision: number;
    updatedAt: string;
    mood: number;
    energy: number;
    health: number;
    satiety: number;
    intimacy: number;
    emotion: PetEmotion;
    activity: string;
    weather: string;
    timePeriod: string;
}

export interface InteractionEvent {
    protocolVersion: number;
    eventId: string;
    occurredAt: string;
    interaction: 'petted' | 'fed' | 'played';
    detail?: string;
}

export const DEFAULT_SNAPSHOT: PetSnapshot = {
    protocolVersion: 1, revision: 0, updatedAt: new Date(0).toISOString(),
    mood: 70, energy: 70, health: 100, satiety: 70, intimacy: 50,
    emotion: 'neutral', activity: 'idle', weather: 'clear', timePeriod: 'day',
};
