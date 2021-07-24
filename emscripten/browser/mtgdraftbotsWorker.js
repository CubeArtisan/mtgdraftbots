import axios from 'axios';
import { expose } from 'threads/worker';

import createMtgDraftBots from './MtgDraftBotsWasmWebWorker.js';
import MtgDraftBotsWasm from './MtgDraftBotsWasmWebWorker.wasm';

const timeout = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
const MtgDraftBots = createMtgDraftBots({
			locateFile: (path) => {
			  if (path.endsWith('.wasm')) return MtgDraftBotsWasm;
			  return path;
			},
		  });

expose({
	calculatePickFromOptions: async ({ drafterState, options }) =>
		(await MtgDraftBots).calculatePickFromOptions(drafterState, options),
	initializeDraftbots: async (url) => {
		const response = await axios.get(
			"https://storage.googleapis.com/storage/v1/b/cubeartisan/o/draftbotparams.bin?alt=media",
			{ responseType: 'arraybuffer', headers: { "Content-Type": "application/json" } },
		);
		return (await MtgDraftBots).initializeWithData(response.data, response.data.length);
	},
	testRecognized: async (oracleIds) => (await MtgDraftBots).testRecognized(oracleIds),
});
