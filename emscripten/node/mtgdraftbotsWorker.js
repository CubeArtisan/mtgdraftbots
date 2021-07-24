import axios from 'axios';
import { expose } from 'threads/worker';
import { fileURLToPath } from 'url';

import createMtgDraftBots from './MtgDraftBotsWasmNodeWorker.cjs';

const MtgDraftBotsWasm = fileURLToPath(new URL('./MtgDraftBotWasmWorker.wasm', import.meta.url));

const MtgDraftBots = createMtgDraftBots();

const timeout = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

expose({
  calculatePickFromOptions: async ({ drafterState, options }) =>
      (await MtgDraftBots).calculatePickFromOptions(drafterState, options),
  initializeDraftbots: async (url) => {
    const response = await axios.get(
        "https://storage.googleapis.com/storage/v1/b/cubeartisan/o/draftbotparams.bin?alt=media",
        { responseType: 'arraybuffer', headers: { "Content-Type": "application/json" } },
    );
    return (await MtgDraftBots).initializeDraftbots(response.data, response.data.length);
  },
  testRecognized: async (oracleIds) => (await MtgDraftBots).testRecognized(oracleIds),
});
