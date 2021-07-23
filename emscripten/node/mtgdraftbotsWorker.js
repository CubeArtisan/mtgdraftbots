import { expose } from 'threads/worker';
import { fileURLToPath } from 'url';

import createMtgDraftBots from './MtgDraftBotsWasmNodeWorker.cjs';

const MtgDraftBotsWasm = fileURLToPath(new URL('./MtgDraftBotWasmWorker.wasm', import.meta.url));

const MtgDraftBots = createMtgDraftBots();

const timeout = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

expose({
  calculatePickFromOptions: async ({ drafterState, options }) => {
    const result = (await MtgDraftBots).calculatePickFromOptions(drafterState, options);
    return result;
  },
});
