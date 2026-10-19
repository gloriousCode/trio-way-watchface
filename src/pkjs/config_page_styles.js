// Self-contained styles for the phone configuration webview.
const CONFIG_PAGE_STYLES = `
  * { box-sizing: border-box; }
  body { margin: 0; background: #eeede9; color: #252622;
    font: 16px/1.5 -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; }
  main { max-width: 600px; margin: auto; padding: 24px 16px 0; }
  header { padding: 0 4px 12px; }
  h1 { font-size: 28px; line-height: 1.2; margin: 4px 0 10px; }
  h2 { font-size: 18px; margin: 0 0 18px; }
  p { margin: 0 0 14px; color: #565950; }
  .eyebrow { font-size: 12px; font-weight: 750; letter-spacing: 2px; margin: 0; }
  section { padding: 20px; margin-bottom: 16px; background: #fafaf7;
    border: 1px solid #d8d8d0; border-radius: 16px; }
  .watch-preview { text-align: center; }
  #watchPreview { display: block; width: 200px; max-width: 100%; height: auto; margin: 0 auto 18px;
    background: #111; border: 6px solid #252622; border-radius: 10px; }
  .watch-preview label { font-size: 14px; }
  .watch-preview .help { margin: 12px 0 0; }
  .row { margin-bottom: 20px; }
  .row:last-child { margin-bottom: 0; }
  label { display: block; margin-bottom: 6px; font-weight: 600; }
  .toggle { display: flex; align-items: center; min-height: 44px; gap: 10px; cursor: pointer; }
  input[type=checkbox] { width: 22px; height: 22px; vertical-align: middle; accent-color: #365b66; flex-shrink: 0; }
  select { width: 100%; min-height: 48px; padding: 10px; border: 1px solid #acafa4;
    border-radius: 8px; background: #fff; color: #252622; font: inherit; }
  select:disabled, input:disabled { opacity: .5; cursor: not-allowed; }
  :focus-visible { outline: 3px solid #35728a; outline-offset: 3px; }
  .help { font-size: 13px; line-height: 1.45; color: #565950; margin-top: 6px; }
  .colour-label { display: flex; align-items: center; justify-content: space-between; gap: 10px; }
  .swatch { display: inline-block; width: 24px; height: 24px; border-radius: 6px;
    border: 1px solid #777b71; flex: 0 0 24px; }
  .preset-preview { display: flex; flex-wrap: wrap; align-items: center; gap: 8px; margin-top: 12px; font-size: 13px; }
  details { border-top: 1px solid #d8d8d0; padding-top: 12px; }
  summary { min-height: 44px; padding: 8px 0; cursor: pointer; font-weight: 650; }
  footer { position: sticky; bottom: 0; padding: 12px 0; padding-bottom: max(12px, env(safe-area-inset-bottom));
    background: #eeede9; border-top: 1px solid #d8d8d0; }
  .actions { display: flex; gap: 8px; }
  button { min-height: 48px; padding: 10px 14px; border: 1px solid #acafa4; border-radius: 10px;
    background: #fafaf7; color: #252622; font: inherit; font-weight: 600; cursor: pointer; }
  #save { flex: 1; background: #365b66; border-color: #365b66; color: #fff; }
  #apply { font-size: 14px; }
  .status { font-size: 12px; color: #565950; margin-top: 6px; min-height: 18px; }
  @media (max-width: 360px) { main { padding: 16px 10px 0; } section { padding: 16px; }
    button { padding: 10px; font-size: 14px; } }
`;

module.exports = { CONFIG_PAGE_STYLES };
