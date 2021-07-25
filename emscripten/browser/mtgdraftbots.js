import {Pool, spawn, Thread} from 'threads';

const createDraftbotsWorker = async (autoInitialize, url) => {
  const worker = await spawn(new Worker(new URL('./mtgdraftbotsWorker.js', import.meta.url)))
  if (autoInitialize) {
    await worker.initializeDraftbots(url);
  }
  return worker;
}

let draftbots = createDraftbotsWorker(false);

export const calculateBotPickFromOptions = async (drafterState, options) =>
    (await draftbots).calculatePickFromOptions({ drafterState, options });

export const calculateBotPick = (drafterState) => {
  const options = [];
  for (let i = 0; i < drafterState.cardsInPack.length; i++) options.push([i]);
  return calculateBotPickFromOptions(drafterState, options);
};

export const testRecognized = async (oracleIds) => (await draftbots).testRecognized(oracleIds);

export const initializeDraftbots = async (url) => {
  await (await draftbots).initializeDraftbots(url);
  return true;
}

export const terminateDraftbots = async () => {
  const worker = await draftbots;
  if (worker) {
    draftbots = null;
    if (worker.queue) {
      worker.terminate();
    } else {
      await Thread.terminate(worker);
    }
  }
  return true;
}

export const restartDraftbots = async (url) => {
  const worker = await draftbots;
  if (worker !== null) {
    await terminateDraftbots()
  }
  draftbots = createDraftbotsWorker(true, url);
  await draftbots;
  return true;
}

export const startPool = async (numWorkers = 4, url) => {
  const worker = await draftbots;
  if (worker !== null) await terminateDraftbots();
  draftbots = new Promise((resolve) => {
    if (!numWorkers) numWorkers = 4;
    const pool = Pool(() => createDraftbotsWorker(true, url), {name: 'MtgDraftBots', size: numWorkers});
    resolve(new Proxy(pool, {
      get: (target, name, receiver) => {
        if (Reflect.has(target, name)) {
          return Reflect.get(target, name, receiver);
        } else {
          return (...args) => target.queue(async (poolWorker) => poolWorker[name](...args))
        }
      }
    }));
  });
  await draftbots;
  return true;
}

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
