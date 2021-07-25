declare module 'mtgdraftbots';

interface DrafterState {
    cardsInPack: number[];
    picked: number[];
    seen: number[];
    basics: number[];
    cardOracleIds: string[];
    packNum: number;
    numPacks: number;
    pickNum: number;
    numPicks: number;
    seed: number;
}

interface OracleResult {
    title: string;
    tooltip: string;
    weight: number;
    value: number;
    per_card: number[];
}

interface BotScore {
    score: number;
    lands: number[];
    oracleResults: OracleResult[];
}

interface BotResult extends DrafterState {
    options: number[][];
    chosenOption: number;
    scores: BotScore[];
}

declare function calculateBotPick(drafterState: DrafterState) : Promise<BotResult>;

declare function calculateBotPickFromOptions(drafterState: DrafterState, options: number[][]) : Promise<BotResult>;

declare function initializeDraftbots(url: string) : Promise<boolean>;

declare function testRecognized(oracleIds: string[]) : Promise<boolean[]>;

declare function terminateDraftbots() : Promise<boolean>;

declare function restartDraftbots(url: string) : Promise<boolean>;

declare function startPool(numWorkers: number, url: string) : Promise<boolean>;

declare const COLOR_COMBINATIONS: string[];