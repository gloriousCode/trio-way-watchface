const { CONFIG_PAGE_STYLES } = require("./config_page_styles");
const { PRESETS, GROUP_ORDER } = require("./constants");
const { buildConfigPageScript } = require("./config_page_script");

function buildPresetOptionsHtml() {
  return Object.keys(PRESETS).map(function(key) {
    const preset = PRESETS[key];
    return `<option value="${key}">${preset.label}</option>`;
  }).join("");
}

function buildConfigDataUrl(config, palette) {
  const presetOptionsHtml = buildPresetOptionsHtml();
  const initialConfig = JSON.stringify(config);
  const presetJson = JSON.stringify(PRESETS);
  const paletteJson = JSON.stringify(palette);

  const html = `<!doctype html>
<html lang="en-AU">
  <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>TrioWay</title>
    <style>${CONFIG_PAGE_STYLES}</style>
  </head>
  <body>
    <main>
    <header><p class="eyebrow">TRIOWAY</p><h1>Make it yours</h1>
    <p>Choose a layout and colour preset, then fine-tune the details.</p></header>
    <section aria-labelledby="layoutHeading"><h2 id="layoutHeading">Time &amp; layout</h2>
    <div class="row">
      <label for="faceMode">Face mode</label>
      <select id="faceMode">
        <option value="hands">Analogue</option>
        <option value="numbered">Analogue · Numerals</option>
        <option value="ticks">Analogue · Marks</option>
        <option value="digital">Hybrid</option>
        <option value="largedigital">Digital Large</option>
      </select>
      <div class="help">Analogue · Numerals adds a numbered dial. Analogue · Marks uses simple hour marks. Hybrid puts unboxed hour and minute numbers at the outer edge. Large Digital uses a fixed, stacked readout.</div>
    </div>

    <div class="row">
      <label class="toggle"><input id="twelveHour" type="checkbox" /> Use 12-hour time</label>
      <div class="help" id="clockHelp">Applies to Hybrid and Digital Large.</div>
    </div>

    <div class="row">
      <label class="toggle"><input id="showHands" type="checkbox" /> Show analogue hands</label>
      <div class="help" id="handsHelp">Show or hide hands on analogue layouts.</div>
    </div>

    </section>
    <section aria-labelledby="dateHeading"><h2 id="dateHeading">Date</h2>
    <div class="row">
      <label class="toggle"><input id="showDateTiles" type="checkbox" /> Show weekday/date badge</label>
      <div class="help">All Analogue modes and Hybrid use one compact weekday/date badge placed clear of the hands. Large Digital hides this badge.</div>
    </div>

    <div class="row">
      <label class="toggle"><input id="showDateComplication" type="checkbox" /> Show date number</label>
      <div class="help">Analogue and Hybrid combine enabled date details into one badge clear of the hands. Large Digital keeps its date at 3 o'clock.</div>
    </div>

    </section>
    <section aria-labelledby="colourHeading"><h2 id="colourHeading">Colours &amp; finish</h2>
    <div class="row">
      <label for="preset">Colour preset</label>
      <select id="preset">
        <option value="">Custom (keep current)</option>
        ${presetOptionsHtml}
      </select>
      <div class="help">Sets all face, hand and date colours. Your layout and clock format stay the same.</div>
      <div class="preset-preview" id="presetPreview">
        <span id="presetPreviewLabel">Custom</span>
        <span class="swatch" id="presetBgSwatch"></span>
        <span class="swatch" id="presetHourSwatch"></span>
        <span class="swatch" id="presetMinuteSwatch"></span>
      </div>
    </div>

    <div class="row">
      <label class="toggle"><input id="showTexture" type="checkbox" /> Subtle background texture</label>
      <div class="help">Adds a light, static pattern behind the dial. Off by default.</div>
    </div>

    <details><summary>Customise individual colours</summary>
    <p class="help">Start with a preset, then adjust any colour below.</p>
    <div class="row">
      <label class="colour-label" for="bgColour">Base sector <span class="swatch" id="bgSwatch"></span></label>
      <select id="bgColour"></select>
    </div>

    <div class="row">
      <label class="colour-label" for="hourColour">Hour sector <span class="swatch" id="hourSwatch"></span></label>
      <select id="hourColour"></select>
    </div>

    <div class="row">
      <label class="colour-label" for="minuteColour">Minute sector <span class="swatch" id="minuteSwatch"></span></label>
      <select id="minuteColour"></select>
      <div class="help">Colours are limited to those supported by your watch.</div>
    </div>

    <div class="row">
      <label class="colour-label" for="hourHandColour">Hour hand / numeral colour <span class="swatch" id="hourHandSwatch"></span></label>
      <select id="hourHandColour"></select>
    </div>

    <div class="row">
      <label class="colour-label" for="minuteHandColour">Minute hand / numeral colour <span class="swatch" id="minuteHandSwatch"></span></label>
      <select id="minuteHandColour"></select>
    </div>

    <div class="row">
      <label class="colour-label" for="complicationBgColour">Date background <span class="swatch" id="complicationBgSwatch"></span></label>
      <select id="complicationBgColour"></select>
    </div>

    <div class="row">
      <label class="colour-label" for="complicationBorderColour">Date border <span class="swatch" id="complicationBorderSwatch"></span></label>
      <select id="complicationBorderColour"></select>
    </div>

    <div class="row">
      <label class="colour-label" for="complicationTextColour">Date text <span class="swatch" id="complicationTextSwatch"></span></label>
      <select id="complicationTextColour"></select>
    </div>

    </details></section>
    <footer>
    <div class="actions">
      <button id="cancel" type="button">Cancel</button>
      <button id="apply" type="button">Apply live</button>
      <button id="save" type="button">Save &amp; close</button>
    </div>
    <div class="status" id="status" role="status" aria-live="polite">Changes are saved when you tap Save &amp; close.</div>
    </footer></main>

    ${buildConfigPageScript(presetJson, paletteJson, JSON.stringify(GROUP_ORDER), initialConfig)}
  </body>
</html>`;

  // Guard against clients that append raw return_to text into the data payload.
  // Leaving an open HTML comment at the end swallows that suffix from rendering.
  return `data:text/html;charset=utf-8,${encodeURIComponent(`${html}\n<!--`)}`;
}

module.exports = {
  buildConfigDataUrl
};
