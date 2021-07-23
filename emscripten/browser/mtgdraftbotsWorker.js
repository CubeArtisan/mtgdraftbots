import { expose } from 'threads/worker';

import createMtgDraftBots from './MtgDraftBotsWasmBrowser.js';
import MtgDraftBotsWasm from './MtgDraftBotsWasmBrowser.wasm';

const timeout = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
const MtgDraftBots = createMtgDraftBots({
			locateFile: (path) => {
			  if (path.endsWith('.wasm')) return MtgDraftBotsWasm;
			  return path;
			},
		  });
		  
expose({
  calculatePickFromOptions: async ({ drafterState, options }) =>
    (await MtgDraftBots).calculatePickFromOptions(convertedDrafterState, options)
});
