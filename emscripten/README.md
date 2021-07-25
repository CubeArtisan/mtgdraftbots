# MTG DraftBots

This package contains code to run the CubeArtisan MTG DraftBots anywhere
recent javascript versions are supported. It runs them in a separate thread
to reduce resource competition. To use the bots you first have to initialize
them as follows.

```javascript
import { initializeDraftbots } from 'mtgdraftbots';

const customUrl = 'https://example.com/draftbotparams.bin';

const main = async () => {
    await initializeDraftbots(customUrl); // If you don't supply a url it'll use the default.
}
```

There is an optional parameter to include a URL to a parameters file. You can use
this if you don't want to depend on the google storage delivery it uses by default.
Once the library is initialized you can query whether it recognizes the oracleIds
you want it to make decisions on with this.

```javascript
import { testRecognized } from 'mtgdraftbots';

const cardOracleIds = [
    "9056eba4-612b-4e82-8689-fe098241b007",
    "7060f2c8-fca0-4b7a-bddf-37682f434596",
    "4c702b96-3288-435d-9154-5b7454419896",
    "ea1eb902-a23c-44ff-9169-19baf71de238",
    "9965d9c5-2ebf-4a6c-930e-55c5890979be",
    "7ea71a36-8fa8-4ba3-9cb1-7fc6917c3ddd",
    "efe3204d-2013-47a1-ad68-adfbb2d0be8f",
    "2987c385-011a-4032-a516-a46d1e9dc9e8",
    "f60fb81a-969a-476c-a227-5231bbed4ad4",
    "e55d9377-f89a-41e5-a094-730d6f24caf0",
    "1de1b591-a73f-4974-b507-8c63e07a0868",
    "7a51780a-fa28-4ee0-94c7-4330800ca9cb",
    "104386d9-93b4-4a18-86d5-68718b474f4a",
    "91fbb25b-8521-483f-88b0-77778d25f7fd",
    "158a6225-a246-4fd6-aa57-0df8067b4383",
]; // Oracle/Gatherer IDs of all the cards referenced in this object

const main = async () => {
    const recognizedFlags = await testRecognized(cardOracleIds);
    const recognizedOracleIds = cardOracleIds.filter((_, idx) => recognizedFlags[idx] > 0);
}
```

Then finally getting to the meat of the library you can query what decision the bots
would make with an explanation as follows:

```javascript
import { calculateBotPick, calculateBotPickFromOptions } from 'mtgdraftbots';

const drafterState = {
  cardOracleIds,
  picked: [0, 1, 2], // Indices of the oracle IDs of the card this player has picked so far this draft.
  seen: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9], // Indices of the oracle IDs of all the cards this player has seen this draft, including duplicates.
  basics: [10, 11, 12, 13, 14], // Indices of the oracle IDs for the set of basics the drafter has access to unlimited copies of.
  cardsInPack: [7, 8, 9], // Indices of the oracle IDs for the cards in the current pack.
  packNum: 0, // 0-Indexed pack number
  numPacks: 3, // How many packs will this player open
  pickNum: 4, // 0-Indexed pick number from this pack (so this will be the 5th card they've picked since opening the first pack of the draft).
  numPicks: 15, // How many cards were in the pack when it was opened.
  seed: 37, // A random seed for the randomized portions of the algorithm, best not to use a constant, is reproducible if this is known.
};
const options = [
  [0, 1],
  [0, 2],
  [1, 2],
]; // Indices into cardsInPack for the different combinations of cards the player can pick.

const result = calculateBotPickFromOptions(drafterState, options);

const result2 = calculateBotPick(drafterState); // Creates an option for each card in the pack on its own.
```

If you don't track seen you can pass an empty array. The bots will still work, but may be less accurate. To get an
evaluation of a pool pass `[[]]` as the options.

The return value from these functions looks like

```javascript
result = {
  ...drafterState,
  options: options, // These make it so if you're handling these differently you can track the parameters that were given.
  chosenOption: 0, // The index in the options array that the bot thinks is best.
  scores: [ // Scores for each option with explanations.
	{
	  score: 0.7685, // Score for the card in the range [0, 1].
	  lands: [0, 0, 0, 7, 8, 0,
	          0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			  0, 0, 0, 0, 0, 2], // number of lands for each color combination (you can get the color combinations in canoncial order from the COLOR_COMBINATIONS export).
	  oracleResults: [ // Scores from each oracle that are combined to give the total score. Can be used to show explanations for the descisions.
	    {
		  title: 'Rating', // Display ready name for this oracle.
		  tooltip: 'The rating based on the current land combination.', // Display ready tooltip describing the oracle.
		  weight: 0.3576, // Relative weight of the oracle compared to the other oracles. All are non-negative and sum to 1.
		  value: 0.468, // Score from the oracle in the range [0, 1].
		  per_card: [0.532, 0.404], // Score for each card in the option individually (some oracles don't care about the card in the pack and will only have 1 item here regardless of the number in the option). 
		},
		...
	  ],
	},
	...
  ],
  recognized: [
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  ], // 1 if the card is in the data the draftbots understand or 0 if it is not recognized (same as result of testRecognized).
}
```

The set of oracles is not a part of the public API so do not rely on there being a specific number
or otherwise on specifics of the oracles. This is neccesary to allow improvements to be made without
breaking the API.

## Advanced

### Terminating the Worker(s)

Since the bots are started on import you likely want to mock them in all your tests
so you don't have to worry about terminating them. If you don't want to mock them
you can manually terminate them with

```javascript
import { terminateDraftbots, restartDraftbots } from 'mtgdraftbots';

beforeAll(async () => restartDraftbots(customUrl)); // customUrl is optional.
afterAll(async () => terminateDraftbots());
```

### Thread Pool

If you want more than one simulataneous thread running the draftbots you can replace
the single worker with a thread pool that you can access using the same API with.

```javascript
import { calculateBotPick, startPool } from 'mtgdraftbots';

const main = async () => {
    await startPool(8, customUrl); // 8 is the number of workers and customUrl is optional.
    const result = await calculateBotPick(drafterState);
};
```

### Webpack

If using with Webpack make sure you enable web assembly with

```javascript
module.exports = {
  experiments: {
    asyncWebAssembly: true
  },
  ...
}
```