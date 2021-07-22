import { expose } from 'threads/worker';

import createMtgDraftBots from './MtgDraftBotsWasm.js';
import mtgDraftBotsModule from './MtgDraftBotsWasm.wasm';

let MtgDraftBots = null;

const timeout = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

expose({
  calculatePickFromOptions: async ({ drafterState, options }) => {
    if (await MtgDraftBots === null) {
      MtgDraftBots = createMtgDraftBots({
			locateFile: (path) => {
			  if (path.endsWith('.wasm')) return mtgDraftBotsModule;
			  return path;
			},
		  });
	  MtgDraftBots = await MtgDraftBots;
      await MtgDraftBots.ready;
      await timeout(1000);
    }
    const result = MtgDraftBots.calculatePickFromOptions(convertedDrafterState, options);
    return result;
  },
});
