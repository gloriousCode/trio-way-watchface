const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const { createRequire } = require('node:module');
const test = require('node:test');
const vm = require('node:vm');

function createConfigHarness(initialStorage = null) {
  const filename = path.resolve(__dirname, '../src/pkjs/config.js');
  const sent = [];
  const timers = [];
  let stored = initialStorage;
  const context = {
    module: { exports: {} },
    require: createRequire(filename),
    console: { log() {} },
    localStorage: {
      getItem() { return stored; },
      setItem(key, value) { stored = value; }
    },
    Pebble: {
      sendAppMessage(payload, success, failure) {
        sent.push({ payload, success, failure });
      }
    },
    setTimeout(callback, delay) {
      timers.push({ callback, delay });
    }
  };
  vm.runInNewContext(fs.readFileSync(filename, 'utf8'), context, { filename });
  return { config: context.module.exports, sent, timers, stored: () => stored };
}

test('a queued older retry cannot overwrite a newer successful configuration', () => {
  const { config, sent, timers } = createConfigHarness();
  config.sendConfigToWatch(config.normaliseConfig({ bgColour: '#000000' }));
  sent[0].failure({ error: 'busy' });
  assert.equal(timers.length, 1);

  config.sendConfigToWatch(config.normaliseConfig({ bgColour: '#ffffff' }));
  sent[1].success();
  timers.shift().callback();

  assert.equal(sent.length, 2);
  assert.equal(sent[1].payload.BG_COLOUR, '#ffffff');
  assert.equal(timers.length, 0);
});

test('an obsolete in-flight failure cannot schedule more traffic', () => {
  const { config, sent, timers } = createConfigHarness();
  config.sendConfigToWatch(config.normaliseConfig({ bgColour: '#000000' }));
  config.sendConfigToWatch(config.normaliseConfig({ bgColour: '#ffffff' }));
  sent[0].failure({ error: 'busy' });
  assert.equal(timers.length, 0);
  assert.equal(sent.length, 2);
});

test('the current configuration retries transient failures and stops on success', () => {
  const { config, sent, timers } = createConfigHarness();
  config.sendConfigToWatch(config.normaliseConfig({ bgColour: '#aaffaa' }));
  sent[0].failure({ error: 'busy' });
  assert.equal(timers[0].delay, 250);
  timers.shift().callback();
  assert.equal(sent.length, 2);
  assert.equal(sent[1].payload.BG_COLOUR, '#aaffaa');
  sent[1].success();
  assert.equal(timers.length, 0);
});

test('persistent failure is bounded to twelve sends with increasing delays', () => {
  const { config, sent, timers } = createConfigHarness();
  config.sendConfigToWatch(config.normaliseConfig({}));
  for (let attempt = 0; attempt < 12; attempt += 1) {
    sent[attempt].failure({ error: 'disconnected' });
    if (attempt < 11) {
      assert.equal(timers.length, 1);
      assert.equal(timers[0].delay, 250 * (attempt + 1));
      timers.shift().callback();
    }
  }
  assert.equal(sent.length, 12);
  assert.equal(timers.length, 0);
});

test('all existing and appended analogue modes survive normalisation and watch payload creation', () => {
  for (const faceMode of ['hands', 'digital', 'largedigital', 'numbered', 'ticks']) {
    const { config, sent } = createConfigHarness();
    const normalised = config.normaliseConfig({ faceMode: ` ${faceMode.toUpperCase()} `,
      showHands: false, showDateTiles: true });
    assert.equal(normalised.faceMode, faceMode);
    assert.equal(normalised.showHands, false);
    assert.equal(normalised.showDateTiles, true);
    config.sendConfigToWatch(normalised);
    assert.equal(sent[0].payload.FACE_MODE, faceMode);
    assert.equal(sent[0].payload.SHOW_HANDS, 0);
    assert.equal(sent[0].payload.SHOW_DATE_TILES, 1);
  }
});

test('texture defaults off for new, legacy and damaged stored configurations', () => {
  for (const raw of [null, '{invalid', JSON.stringify({ faceMode: 'numbered', showDateTiles: true })]) {
    const { config, sent } = createConfigHarness(raw);
    const loaded = config.loadConfig();
    assert.equal(loaded.showTexture, false);
    config.sendConfigToWatch(loaded);
    assert.equal(sent[0].payload.SHOW_TEXTURE, 0);
  }
  const { config } = createConfigHarness();
  const legacy = config.parseConfigPayload(encodeURIComponent(JSON.stringify({ faceMode: 'ticks', showHands: false })));
  const migrated = config.normaliseConfig(legacy);
  assert.equal(migrated.showTexture, false);
  assert.equal(migrated.faceMode, 'ticks');
  assert.equal(migrated.showHands, false);
});

test('texture normalisation, saved settings and watch messages preserve explicit toggles in every mode', () => {
  const values = [[true, true], [false, false], [1, true], [0, false],
    ['1', true], ['0', false], [undefined, false], [null, false], ['invalid', false]];
  for (const faceMode of ['hands', 'digital', 'largedigital', 'numbered', 'ticks']) {
    for (const [value, expected] of values) {
      const harness = createConfigHarness();
      const normalised = harness.config.normaliseConfig({ faceMode, showTexture: value });
      assert.equal(normalised.showTexture, expected);
      harness.config.saveConfig(normalised);
      assert.equal(JSON.parse(harness.stored()).showTexture, expected);
      assert.equal(harness.config.loadConfig().showTexture, expected);
      harness.config.sendConfigToWatch(normalised);
      assert.equal(harness.sent[0].payload.SHOW_TEXTURE, expected ? 1 : 0);
      assert.equal(harness.sent[0].payload.FACE_MODE, faceMode);
    }
  }
});

test('texture message key is appended without renumbering existing settings', () => {
  const { pebble } = require('../package.json');
  assert.deepEqual(pebble.messageKeys, ['BG_COLOUR', 'HOUR_COLOUR', 'MINUTE_COLOUR',
    'H_HAND_COLOUR', 'M_HAND_COLOUR', 'C_BG_COLOUR', 'C_BORDER_COLOUR', 'C_TEXT_COLOUR',
    'FACE_MODE', 'SHOW_DATE_TILES', 'SHOW_DATE_COMPLICATION', 'SHOW_HANDS', 'HAND_COLOUR', 'SHOW_TEXTURE']);
});
