import { spawn, Worker as ThreadsWorker } from 'threads';

let draftbots;
export const calculateBotPickFromOptions = async (drafterState, options) => {
  if (!draftbots) {
    let worker;
    if (typeof window !== "undefined") {
      // eslint-disable-next-line prettier/prettier
      worker = new Worker(new URL('./newWorker.js', import.meta.url));
    } else {
      worker = new ThreadsWorker('./newWorker.js');
    }
    draftbots = await spawn(worker);
  }
  return draftbots.calculatePickFromOptions({ drafterState, options });
};

export const calculateBotPick = (drafterState) => {
  const options = [];
  for (let i = 0; i < drafterState.cardsInPack.length; i++) options.push([i]);
  return calculateBotPickFromOptions(drafterState, options);
};

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