// Generate a local review gallery from the real emulator captures.
const fs = require('node:fs');
const path = require('node:path');
const { PRESETS } = require('../src/pkjs/constants');

const root = path.resolve(__dirname, '..');
const escape = value => value.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/"/g, '&quot;');
const creative = new Set(['dusk', 'lagoon', 'ember']);
const cards = entries => entries.map(([key, preset]) => `
<figure>
  <img data-preset="${key}" src="${key}/gabbro-ticks-1009.png" width="260" height="260" alt="${escape(preset.label)} marked dial at 10:09">
  <figcaption>${escape(preset.label)}</figcaption>
  <div class="swatches">${['bgColour', 'hourColour', 'minuteColour', 'hourHandColour', 'minuteHandColour'].map(field =>
    `<span title="${field}: ${preset[field]}" style="background:${preset[field]}"></span>`).join('')}</div>
</figure>`).join('\n');

const entries = Object.entries(PRESETS);
const output = path.join(root, 'mockups/presets');
fs.mkdirSync(output, { recursive: true });
fs.writeFileSync(path.join(output, 'index.html'), `<!doctype html>
<html lang="en-AU">
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>TrioWay — revised palettes</title>
<style>
body{margin:0;background:#eeede9;color:#252622;font:16px/1.5 system-ui,sans-serif}
main{max-width:1260px;margin:auto;padding:40px 24px}h1{margin:0;font-size:32px}
p{max-width:760px;color:#5b5d57}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:20px;margin:24px 0 48px}
figure{margin:0;padding:20px;background:#fafaf7;border:1px solid #d8d8d0;border-radius:14px;text-align:center}
img{display:block;margin:0 auto 16px;image-rendering:pixelated}figcaption{font-weight:650}
.swatches{display:flex;justify-content:center;gap:5px;margin-top:12px}.swatches span{width:22px;height:8px;border:1px solid #b4b4ae;border-radius:3px}
.comparison,.controls{display:flex;gap:12px;flex-wrap:wrap}.controls{margin:24px 0 32px}
button{font:inherit;border:1px solid #777a70;border-radius:24px;padding:10px 18px;cursor:pointer;background:#fafaf7;color:#252622}
button[aria-pressed=true]{background:#252622;color:white}button:focus-visible{outline:3px solid #547eac;outline-offset:3px}
small{color:#686a63}@media(max-width:380px){main{padding:24px 12px}.grid{grid-template-columns:1fr}figure{padding:16px 4px}}
</style>
<main>
<h1>TrioWay · revised palettes</h1>
<p>Actual emulator captures at native resolution. Seven neutral palettes, three tonal palettes and three more creative colour combinations. All show 10:09.</p>
<div class="controls" aria-label="Watch display">
  <button type="button" data-platform="emery" aria-pressed="false">Pebble Time 2</button>
  <button type="button" data-platform="gabbro" aria-pressed="true">Pebble Round 2</button>
</div>
<p id="display-note" aria-live="polite">Pebble Round 2 · 260 × 260 pixels</p>
<h2>New colour sets</h2>
<div class="grid">${cards(entries.filter(([key]) => creative.has(key)))}</div>
<h2>Neutral and tonal palettes</h2>
<div class="grid">${cards(entries.filter(([key]) => !creative.has(key)))}</div>
<h2>Optional subtle texture</h2>
<p>Off by default. A static, sparse grain shades the sector transitions within Pebble’s native colour palette.</p>
<div class="comparison">
  <figure><img id="flat" src="navy/gabbro-ticks-1009.png" width="260" height="260" alt="Navy with flat sectors"><figcaption>Flat</figcaption></figure>
  <figure><img id="texture" src="../texture-preview/production/gabbro-ticks-1009.png" width="260" height="260" alt="Navy with subtle texture enabled"><figcaption>Texture enabled</figcaption></figure>
</div>
<p><small>Browser zoom may enlarge pixels. Swatches show the three sectors and two hand colours.</small></p>
</main>
<script>
document.querySelectorAll('[data-platform]').forEach(button => button.addEventListener('click', () => {
  const platform = button.dataset.platform;
  const round = platform === 'gabbro';
  document.querySelectorAll('[data-platform]').forEach(item => item.setAttribute('aria-pressed', String(item === button)));
  document.querySelectorAll('img').forEach(image => { image.width = round ? 260 : 200; image.height = round ? 260 : 228; });
  document.querySelectorAll('[data-preset]').forEach(image => { image.src = image.dataset.preset + '/' + platform + '-ticks-1009.png'; });
  document.getElementById('flat').src = 'navy/' + platform + '-ticks-1009.png';
  document.getElementById('texture').src = '../texture-preview/production/' + platform + '-ticks-1009.png';
  document.getElementById('display-note').textContent = round ? 'Pebble Round 2 · 260 × 260 pixels' : 'Pebble Time 2 · 200 × 228 pixels';
}));
</script>
</html>`);
console.log('Generated mockups/presets/index.html');
