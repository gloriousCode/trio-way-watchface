const assert = require('node:assert/strict');
const test = require('node:test');
const { PRESETS, defaults, FACE_MODES } = require('../src/pkjs/constants');

const sectorKeys = ['bgColour', 'hourColour', 'minuteColour'];
const handKeys = ['hourHandColour', 'minuteHandColour'];
const complicationKeys = ['complicationBgColour', 'complicationBorderColour', 'complicationTextColour'];
const colourKeys = [...sectorKeys, ...handKeys, ...complicationKeys];
const neutral = colour => colour.slice(1, 3) === colour.slice(3, 5) &&
  colour.slice(3, 5) === colour.slice(5, 7);

function contrast(first, second) {
  function luminance(colour) {
    const channels = colour.slice(1).match(/../g).map(value => parseInt(value, 16) / 255)
      .map(value => value <= 0.04045 ? value / 12.92 : ((value + 0.055) / 1.055) ** 2.4);
    return channels[0] * 0.2126 + channels[1] * 0.7152 + channels[2] * 0.0722;
  }
  const a = luminance(first);
  const b = luminance(second);
  return (Math.max(a, b) + 0.05) / (Math.min(a, b) + 0.05);
}

test('curated presets contain explicit native-palette colours and distinct readable sector tones', () => {
  const combinations = new Set();
  for (const [key, preset] of Object.entries(PRESETS)) {
    assert.deepEqual(Object.keys(preset).sort(), ['label', ...colourKeys].sort(), key);
    assert.ok(preset.label.length > 0, key);
    for (const field of colourKeys) {
      assert.match(preset[field], /^#(?:00|55|aa|ff){3}$/, `${key}.${field}`);
    }
    const sectors = sectorKeys.map(field => preset[field]);
    assert.equal(new Set(sectors).size, 3, key);
    for (let first = 0; first < sectors.length; first += 1) {
      for (let second = first + 1; second < sectors.length; second += 1) {
        assert.ok(contrast(sectors[first], sectors[second]) >= 1.5, `${key} sector separation`);
      }
    }
    combinations.add(sectors.join(','));
    assert.ok(complicationKeys.every(field => neutral(preset[field])), `${key} neutral complication`);
    assert.ok(contrast(preset.complicationBgColour, preset.complicationTextColour) >= 7, key);
  }
  // The configuration UI recognises presets by their ordered sector colours.
  assert.equal(combinations.size, Object.keys(PRESETS).length);
});

test('neutral bases dominate and their highlights use at most one chromatic accent', () => {
  const neutralPresets = Object.values(PRESETS).filter(preset => sectorKeys.every(field => neutral(preset[field])));
  assert.ok(neutralPresets.length >= 7);
  assert.ok(neutralPresets.length / Object.keys(PRESETS).length > 0.5);
  for (const preset of neutralPresets) {
    const accents = new Set(handKeys.map(field => preset[field]).filter(colour => !neutral(colour)));
    assert.ok(accents.size <= 1, preset.label);
  }
  // The three tonal palettes stay within a single hue, including their accents.
  for (const [key, dominant] of [['navy', 2], ['forest', 1], ['burgundy', 0]]) {
    for (const field of [...sectorKeys, ...handKeys]) {
      const colour = PRESETS[key][field];
      if (neutral(colour)) continue;
      const channels = colour.slice(1).match(/../g).map(value => parseInt(value, 16));
      const others = channels.filter((_, index) => index !== dominant);
      assert.equal(others[0], others[1], `${key}.${field}`);
      assert.ok(channels[dominant] > others[0], `${key}.${field}`);
    }
  }
});

test('the catalogue and classic defaults remain stable without generated accent colours', () => {
  assert.deepEqual(Object.keys(PRESETS), [
    'classic', 'graphite', 'silver', 'paper', 'inkBlue', 'warmStone', 'porcelain', 'navy', 'forest', 'burgundy',
    'dusk', 'lagoon', 'ember'
  ]);
  assert.deepEqual(colourKeys.map(field => defaults[field]), [
    '#555555', '#000000', '#aaaaaa', '#ffffff', '#ffaa55', '#000000', '#aaaaaa', '#ffffff'
  ]);
  for (const field of colourKeys) assert.equal(defaults[field], PRESETS.classic[field]);
  assert.equal(defaults.faceMode, FACE_MODES.HANDS);
  assert.equal(FACE_MODES.TICKS, 'ticks');
});
