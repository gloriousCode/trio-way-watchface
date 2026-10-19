const assert = require('node:assert/strict');
const test = require('node:test');
const vm = require('node:vm');
const { defaults } = require('../src/pkjs/constants');
const { buildPebblePalette } = require('../src/pkjs/palette');
const { buildConfigDataUrl } = require('../src/pkjs/config_page');

// Execute the actual generated script against a small DOM adapter. This checks
// event wiring and control state, without claiming browser layout coverage.
function openPage(overrides = {}, returnTo = '') {
  const url = buildConfigDataUrl({ ...defaults, ...overrides }, buildPebblePalette());
  const html = decodeURIComponent(url.slice(url.indexOf(',') + 1));
  const elements = new Map();
  function element(tagName) {
    const listeners = new Map();
    return {
      tagName, value: '', checked: false, disabled: false, style: {}, options: [], children: [],
      appendChild(child) {
        this.children.push(child);
        if (child.tagName === 'option') this.options.push(child);
        if (child.tagName === 'optgroup') this.options.push(...child.options);
      },
      addEventListener(type, callback) {
        if (!listeners.has(type)) listeners.set(type, []);
        listeners.get(type).push(callback);
      },
      dispatch(type) {
        for (const callback of listeners.get(type) || []) callback({ target: this });
      }
    };
  }
  const markup = html.slice(0, html.indexOf('<script>'));
  for (const match of markup.matchAll(/<(\w+)[^>]*\bid="([^"]+)"[^>]*>/g)) {
    elements.set(match[2], element(match[1]));
  }
  const document = {
    location: '',
    getElementById(id) {
      assert.ok(elements.has(id), `Generated page is missing #${id}`);
      return elements.get(id);
    },
    createElement: element,
    body: element('body')
  };
  const callbacks = [];
  const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
  vm.runInNewContext(script, {
    document,
    window: { location: { search: returnTo ? `?return_to=${encodeURIComponent(returnTo)}` : '', href: 'data:text/html' } },
    Image: function() {
      Object.defineProperty(this, 'src', { set(value) { callbacks.push(value); } });
    },
    setTimeout() {}
  }, { filename: 'generated-config-page.js' });
  return {
    html, document, callbacks,
    get: id => document.getElementById(id),
    change(id, value) {
      const control = document.getElementById(id);
      if (typeof value === 'boolean') control.checked = value;
      else control.value = value;
      control.dispatch('change');
    }
  };
}

test('Hybrid exposes numeral and hand colours even when analogue hands were disabled', () => {
  const page = openPage({ faceMode: 'digital', showHands: false, showDateTiles: true });
  assert.match(page.html, /<option value="digital">Hybrid<\/option>/);
  assert.equal(page.get('showHands').disabled, true);
  assert.equal(page.get('showHands').checked, false);
  assert.equal(page.get('hourHandColour').disabled, false);
  assert.equal(page.get('minuteHandColour').disabled, false);
  assert.equal(page.get('showDateTiles').disabled, false);
  assert.equal(page.get('showDateTiles').checked, true);
});

test('mode changes update hand controls and suppress tiles only for large digital', () => {
  const page = openPage({ faceMode: 'hands', showHands: false, showDateTiles: true });
  assert.equal(page.get('showHands').disabled, false);
  assert.equal(page.get('hourHandColour').disabled, true);
  assert.equal(page.get('minuteHandColour').disabled, true);
  page.change('showHands', true);
  assert.equal(page.get('hourHandColour').disabled, false);
  assert.equal(page.get('minuteHandColour').disabled, false);
  page.change('showHands', false);
  page.change('faceMode', 'digital');
  assert.equal(page.get('hourHandColour').disabled, false);
  assert.equal(page.get('showDateTiles').checked, true);
  page.change('faceMode', 'largedigital');
  assert.equal(page.get('hourHandColour').disabled, false);
  assert.equal(page.get('showDateTiles').disabled, true);
  assert.equal(page.get('showDateTiles').checked, false);
  page.change('faceMode', 'hands');
  assert.equal(page.get('showHands').disabled, false);
  assert.equal(page.get('hourHandColour').disabled, true);
  assert.equal(page.get('showDateTiles').disabled, false);
});

test('date complication colours follow their checkbox in every face mode', () => {
  for (const faceMode of ['hands', 'numbered', 'ticks', 'digital', 'largedigital']) {
    const page = openPage({ faceMode, showDateComplication: true });
    assert.equal(page.get('showDateComplication').disabled, false);
    for (const enabled of [true, false, true]) {
      page.change('showDateComplication', enabled);
      for (const control of ['complicationBgColour', 'complicationBorderColour', 'complicationTextColour']) {
        assert.equal(page.get(control).disabled, !enabled);
      }
    }
  }
});

test('saving Hybrid retains its mode, date choice and configured hand colours', () => {
  const page = openPage({ faceMode: 'digital', showHands: false, showDateTiles: true });
  page.change('hourHandColour', '#ffaa55');
  page.change('minuteHandColour', '#55aaff');
  page.get('save').dispatch('click');
  assert.ok(page.document.location.startsWith('pebblejs://close#'));
  const payload = JSON.parse(decodeURIComponent(page.document.location.split('#')[1]));
  assert.equal(payload.faceMode, 'digital');
  assert.equal(payload.showHands, false);
  assert.equal(payload.showDateTiles, true);
  assert.equal(payload.hourHandColour, '#ffaa55');
  assert.equal(payload.minuteHandColour, '#55aaff');
});

test('date colours remain editable when day/date tiles are the only enabled date display', () => {
  for (const faceMode of ['hands', 'numbered', 'ticks', 'digital']) {
    const page = openPage({ faceMode, showDateComplication: false, showDateTiles: true });
    const colours = ['complicationBgColour', 'complicationBorderColour', 'complicationTextColour'];
    for (const control of colours) assert.equal(page.get(control).disabled, false);
    page.change('showDateTiles', false);
    for (const control of colours) assert.equal(page.get(control).disabled, true);
    page.change('showDateTiles', true);
    for (const control of colours) assert.equal(page.get(control).disabled, false);
    page.change('showDateComplication', true);
    page.change('showDateTiles', false);
    for (const control of colours) assert.equal(page.get(control).disabled, false);
    page.change('showDateComplication', false);
    for (const control of colours) assert.equal(page.get(control).disabled, true);
    page.change('showDateTiles', true);
    page.change('faceMode', 'largedigital');
    assert.equal(page.get('showDateTiles').checked, false);
    for (const control of colours) assert.equal(page.get(control).disabled, true);
  }
});

test('Analogue Numerals keeps hands and dates optional while numeral colours remain editable', () => {
  const page = openPage({ faceMode: 'numbered', showHands: false, showDateTiles: true });
  assert.match(page.html, /<option value="numbered">Analogue · Numerals<\/option>/);
  assert.equal(page.get('faceMode').value, 'numbered');
  assert.equal(page.get('showHands').disabled, false);
  assert.equal(page.get('showDateTiles').disabled, false);
  assert.equal(page.get('showDateComplication').disabled, false);
  for (const showHands of [false, true, false]) {
    page.change('showHands', showHands);
    assert.equal(page.get('hourHandColour').disabled, false);
    assert.equal(page.get('minuteHandColour').disabled, false);
  }
  page.change('hourHandColour', '#ffaa55');
  page.change('minuteHandColour', '#55aaff');
  page.get('save').dispatch('click');
  const payload = JSON.parse(decodeURIComponent(page.document.location.split('#')[1]));
  assert.equal(payload.faceMode, 'numbered');
  assert.equal(payload.showHands, false);
  assert.equal(payload.showDateTiles, true);
  assert.equal(payload.showDateComplication, true);
  assert.equal(payload.hourHandColour, '#ffaa55');
  assert.equal(payload.minuteHandColour, '#55aaff');
  page.change('faceMode', 'hands');
  assert.equal(page.get('hourHandColour').disabled, true);
  page.change('faceMode', 'numbered');
  assert.equal(page.get('hourHandColour').disabled, false);
});

test('Analogue Marks exposes optional hands and dates without enabling unused numeral colours', () => {
  const page = openPage({ faceMode: 'ticks', showHands: false, showDateTiles: true });
  assert.match(page.html, /<option value="ticks">Analogue · Marks<\/option>/);
  assert.equal(page.get('faceMode').value, 'ticks');
  assert.equal(page.get('showHands').disabled, false);
  assert.equal(page.get('showDateTiles').disabled, false);
  assert.equal(page.get('showDateComplication').disabled, false);
  for (const showHands of [false, true, false]) {
    page.change('showHands', showHands);
    assert.equal(page.get('hourHandColour').disabled, !showHands);
    assert.equal(page.get('minuteHandColour').disabled, !showHands);
  }
  page.get('save').dispatch('click');
  const payload = JSON.parse(decodeURIComponent(page.document.location.split('#')[1]));
  assert.equal(payload.faceMode, 'ticks');
  assert.equal(payload.showHands, false);
  assert.equal(payload.showDateTiles, true);
  assert.equal(payload.showDateComplication, true);
  page.change('faceMode', 'numbered');
  assert.equal(page.get('hourHandColour').disabled, false);
  page.change('faceMode', 'ticks');
  assert.equal(page.get('hourHandColour').disabled, true);
});

test('texture starts off and remains available across mode and preset changes', () => {
  const page = openPage({ showTexture: undefined });
  assert.match(page.html, /id="showTexture" type="checkbox"/);
  assert.equal(page.get('showTexture').checked, false);
  for (const enabled of [true, false]) {
    page.change('showTexture', enabled);
    for (const faceMode of ['hands', 'numbered', 'ticks', 'digital', 'largedigital']) {
      page.change('faceMode', faceMode);
      page.change('preset', 'classic');
      assert.equal(page.get('showTexture').disabled, false);
      assert.equal(page.get('showTexture').checked, enabled);
    }
  }
});

test('Apply and Save both send explicit texture toggles in every face mode', () => {
  for (const faceMode of ['hands', 'numbered', 'ticks', 'digital', 'largedigital']) {
    const page = openPage({ faceMode, showTexture: true }, 'http://localhost:8080/config');
    assert.equal(page.get('showTexture').checked, true);
    for (const enabled of [false, true, false]) {
      page.change('showTexture', enabled);
      page.get('apply').dispatch('click');
      const callback = new URL(page.callbacks.at(-1));
      const applied = JSON.parse(callback.searchParams.get('response'));
      assert.equal(applied.showTexture, enabled);
      assert.equal(applied.faceMode, faceMode);
      page.get('save').dispatch('click');
      const saved = JSON.parse(decodeURIComponent(page.document.location.split('#')[1]));
      assert.equal(saved.showTexture, enabled);
      assert.equal(saved.faceMode, faceMode);
    }
  }
});
