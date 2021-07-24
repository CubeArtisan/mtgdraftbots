import { spawn } from 'threads';

const draftbots = spawn(new Worker(new URL('./mtgdraftbotsWorker.js', import.meta.url)));

export const calculateBotPickFromOptions = async (drafterState, options) =>
    (await draftbots).calculatePickFromOptions({ drafterState, options });

export const calculateBotPick = (drafterState) => {
  const options = [];
  for (let i = 0; i < drafterState.cardsInPack.length; i++) options.push([i]);
  return calculateBotPickFromOptions(drafterState, options);
};

export const testRecognized = async (oracleIds) => (await draftbots).testRecognized(oracleIds);

export const initializeDraftbots = async (url) => (await draftbots).initializeDraftbots(url);

export const COLOR_COMBINATIONS = [
  [],
  ['W'],
  ['U'],
  ['B'],
  ['R'],
  ['G'],
  ['W', 'U'],
  ['U', 'B'],
  ['B', 'R'],
  ['R', 'G'],
  ['G', 'W'],
  ['W', 'B'],
  ['U', 'R'],
  ['B', 'G'],
  ['R', 'W'],
  ['G', 'U'],
  ['G', 'W', 'U'],
  ['W', 'U', 'B'],
  ['U', 'B', 'R'],
  ['B', 'R', 'G'],
  ['R', 'G', 'W'],
  ['R', 'W', 'B'],
  ['G', 'U', 'R'],
  ['W', 'B', 'G'],
  ['U', 'R', 'W'],
  ['B', 'G', 'U'],
  ['U', 'B', 'R', 'G'],
  ['B', 'R', 'G', 'W'],
  ['R', 'G', 'W', 'U'],
  ['G', 'W', 'U', 'B'],
  ['W', 'U', 'B', 'R'],
  ['W', 'U', 'B', 'R', 'G'],
];
