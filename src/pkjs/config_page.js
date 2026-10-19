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
<html>
  <head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>TrioWay</title>
    <style>
      body {
        margin: 0;
        padding: 16px;
        font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
        background: #f6f2e8;
        color: #25201a;
      }
      h1 {
        margin-top: 0;
        font-size: 20px;
      }
      .row {
        margin-bottom: 14px;
      }
      label {
        display: block;
        margin-bottom: 6px;
      }
      .colour-label {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 10px;
      }
      .swatch {
        width: 24px;
        height: 24px;
        border-radius: 6px;
        border: 1px solid #7f7568;
        box-sizing: border-box;
        flex: 0 0 24px;
      }
      select {
        width: 100%;
        height: 44px;
        border: 1px solid #d8d0bd;
        border-radius: 8px;
        box-sizing: border-box;
        background: #fff;
      }
      .help {
        font-size: 12px;
        line-height: 1.3;
        color: #5a5044;
      }
      .preset-preview {
        margin-top: 8px;
        display: flex;
        align-items: center;
        gap: 8px;
        font-size: 12px;
        color: #5a5044;
      }
      .preset-preview .swatch {
        width: 16px;
        height: 16px;
        border-radius: 4px;
        flex: 0 0 16px;
      }
      .actions {
        display: grid;
        grid-template-columns: 1fr 1fr 1fr;
        gap: 10px;
      }
      .status {
        margin-top: 10px;
        min-height: 16px;
        font-size: 12px;
        color: #5a5044;
      }
      button {
        width: 100%;
        height: 44px;
        border: 0;
        border-radius: 10px;
        font-size: 16px;
      }
      #save {
        background: #c45d32;
        color: #fff;
      }
      #cancel {
        background: #ded5c2;
        color: #2e271f;
      }
      #apply {
        background: #5f6e77;
        color: #fff;
      }
    </style>
  </head>
  <body>
    <h1>Watchface settings</h1>
    <div class="row">
      <label for="preset">Colour preset</label>
      <select id="preset">
        <option value="">Custom (keep current)</option>
        ${presetOptionsHtml}
      </select>
      <div class="help">Choose a preset to auto-fill the three colours, or keep custom colours.</div>
      <div class="preset-preview" id="presetPreview">
        <span id="presetPreviewLabel">Custom</span>
        <span class="swatch" id="presetBgSwatch"></span>
        <span class="swatch" id="presetHourSwatch"></span>
        <span class="swatch" id="presetMinuteSwatch"></span>
      </div>
    </div>

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
      <label><input id="showHands" type="checkbox" /> Show analogue hands</label>
      <div class="help">Disable this to keep analogue mode without hands.</div>
    </div>

    <div class="row">
      <label><input id="showTexture" type="checkbox" /> Subtle background texture</label>
      <div class="help">Adds a light, static pattern behind the dial. Off by default.</div>
    </div>

    <div class="row">
      <label><input id="showDateTiles" type="checkbox" /> Show weekday/date badge</label>
      <div class="help">All Analogue modes and Hybrid use one compact weekday/date badge placed clear of the hands. Large Digital hides this badge.</div>
    </div>

    <div class="row">
      <label><input id="showDateComplication" type="checkbox" /> Show date complication</label>
      <div class="help">Analogue and Hybrid combine enabled date details into one badge clear of the hands. Large Digital keeps its date at 3 o'clock.</div>
    </div>

    <div class="row">
      <label class="colour-label" for="bgColour">Base colour <span class="swatch" id="bgSwatch"></span></label>
      <select id="bgColour"></select>
    </div>

    <div class="row">
      <label class="colour-label" for="hourColour">Hour colour <span class="swatch" id="hourSwatch"></span></label>
      <select id="hourColour"></select>
    </div>

    <div class="row">
      <label class="colour-label" for="minuteColour">Minute colour <span class="swatch" id="minuteSwatch"></span></label>
      <select id="minuteColour"></select>
      <div class="help">Only Pebble Time palette colours are shown.</div>
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
      <label class="colour-label" for="complicationBgColour">Complication background <span class="swatch" id="complicationBgSwatch"></span></label>
      <select id="complicationBgColour"></select>
    </div>

    <div class="row">
      <label class="colour-label" for="complicationBorderColour">Complication border <span class="swatch" id="complicationBorderSwatch"></span></label>
      <select id="complicationBorderColour"></select>
    </div>

    <div class="row">
      <label class="colour-label" for="complicationTextColour">Complication text <span class="swatch" id="complicationTextSwatch"></span></label>
      <select id="complicationTextColour"></select>
    </div>

    <div class="actions">
      <button id="cancel" type="button">Cancel</button>
      <button id="apply" type="button">Apply</button>
      <button id="save" type="button">Save</button>
    </div>
    <div class="status" id="status"></div>

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
