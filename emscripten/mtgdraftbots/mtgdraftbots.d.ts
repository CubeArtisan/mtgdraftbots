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
	lands: number[32];
	oracleResults: OracleResult[];
}

interface BotResult extends DrafterState {
	options: number[][];
	chosenOption: number;
	scores: BotScore[];
}

declare function calculateBotPick(drafterState: DrafterState) : BotResult;

declare function calculateBotPickFromOptions(drafterState: DrafterState, options: number[][]) : BotResult;

declare const COLOR_COMBINATIONS: string[32];