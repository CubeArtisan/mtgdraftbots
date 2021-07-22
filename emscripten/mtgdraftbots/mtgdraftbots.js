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