const { STORAGE_KEY, FACE_MODES, defaults } = require("./constants");
const { snapToPebblePalette } = require("./palette");

function loadConfig() {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);

    if (!raw) {
      return buildDefaultConfig();
    }

    return normaliseConfig(JSON.parse(raw));
  } catch (error) {
    return buildDefaultConfig();
  }
}

function buildDefaultConfig() {
  return {
    bgColour: defaults.bgColour,
    hourColour: defaults.hourColour,
    minuteColour: defaults.minuteColour,
    hourHandColour: defaults.hourHandColour,
    minuteHandColour: defaults.minuteHandColour,
    complicationBgColour: defaults.complicationBgColour,
    complicationBorderColour: defaults.complicationBorderColour,
    complicationTextColour: defaults.complicationTextColour,
    showHands: defaults.showHands,
    showTexture: defaults.showTexture,
    twelveHour: defaults.twelveHour,
    showDateComplication: defaults.showDateComplication,
    faceMode: defaults.faceMode,
    showDateTiles: defaults.showDateTiles
  };
}

function saveConfig(config) {
  localStorage.setItem(STORAGE_KEY, JSON.stringify(config));
}

function normaliseConfig(raw) {
  const legacyHandColour = sanitiseHandColour(raw && raw.handColour, "black");
  const legacyHandHex = legacyHandColour === "white" ? "#ffffff" : "#000000";

  return {
    bgColour: sanitiseHex(raw && raw.bgColour, defaults.bgColour),
    hourColour: sanitiseHex(raw && raw.hourColour, defaults.hourColour),
    minuteColour: sanitiseHex(raw && raw.minuteColour, defaults.minuteColour),
    hourHandColour: sanitiseHex(raw && raw.hourHandColour, legacyHandHex),
    minuteHandColour: sanitiseHex(raw && raw.minuteHandColour, legacyHandHex),
    complicationBgColour: sanitiseHex(raw && raw.complicationBgColour, defaults.complicationBgColour),
    complicationBorderColour: sanitiseHex(raw && raw.complicationBorderColour, defaults.complicationBorderColour),
    complicationTextColour: sanitiseHex(raw && raw.complicationTextColour, defaults.complicationTextColour),
    showHands: sanitiseBool(raw && raw.showHands, defaults.showHands),
    showTexture: sanitiseBool(raw && raw.showTexture, defaults.showTexture),
    twelveHour: sanitiseBool(raw && raw.twelveHour, defaults.twelveHour),
    showDateComplication: sanitiseBool(raw && raw.showDateComplication, defaults.showDateComplication),
    faceMode: sanitiseMode(raw && raw.faceMode, defaults.faceMode),
    showDateTiles: sanitiseBool(raw && raw.showDateTiles, defaults.showDateTiles)
  };
}

function parseConfigPayload(rawResponse) {
  const raw = String(rawResponse || "");
  const candidates = [raw];

  try {
    candidates.push(decodeURIComponent(raw));
  } catch (error) {
    // Keep raw candidate only.
  }

  for (let i = 0; i < candidates.length; i += 1) {
    const parsed = parseConfigCandidate(candidates[i]);

    if (parsed) {
      return parsed;
    }
  }

  return null;
}

function parseConfigCandidate(candidate) {
  if (!candidate || candidate === "null") {
    return null;
  }

  const cleaned = candidate.charAt(0) === "#" ? candidate.slice(1) : candidate;

  try {
    return JSON.parse(cleaned);
  } catch (error) {
    // Continue to query-style fallback.
  }

  const wrapped = extractQueryValue(cleaned, "response");

  if (!wrapped) {
    return null;
  }

  try {
    return JSON.parse(wrapped);
  } catch (error) {
    try {
      return JSON.parse(decodeURIComponent(wrapped));
    } catch (decodeError) {
      return null;
    }
  }
}

function extractQueryValue(text, key) {
  const marker = `${key}=`;
  const index = text.lastIndexOf(marker);

  if (index === -1) {
    return null;
  }

  const start = index + marker.length;
  const end = text.indexOf("&", start);

  if (end === -1) {
    return text.slice(start);
  }

  return text.slice(start, end);
}

function sanitiseHex(value, fallback) {
  const text = typeof value === "string" ? value.trim() : "";
  const match = text.match(/^#?[0-9a-fA-F]{6}$/);

  if (!match) {
    return snapToPebblePalette(fallback);
  }

  const raw = text.startsWith("#") ? text.toLowerCase() : `#${text.toLowerCase()}`;
  return snapToPebblePalette(raw);
}

function sanitiseMode(value, fallback) {
  const mode = String(value || "").trim().toLowerCase();

  if (
    mode === FACE_MODES.HANDS ||
    mode === FACE_MODES.DIGITAL ||
    mode === FACE_MODES.LARGE_DIGITAL ||
    mode === FACE_MODES.NUMBERED ||
    mode === FACE_MODES.TICKS
  ) {
    return mode;
  }

  return fallback;
}

function sanitiseHandColour(value, fallback) {
  const colour = String(value || "").trim().toLowerCase();
  if (colour === "black" || colour === "white") {
    return colour;
  }
  return fallback;
}

function sanitiseBool(value, fallback) {
  if (value === true || value === false) {
    return value;
  }

  if (value === 1 || value === "1") {
    return true;
  }

  if (value === 0 || value === "0") {
    return false;
  }

  return fallback;
}

let activeSendToken = 0;
const MAX_SEND_ATTEMPTS = 12;
const BASE_RETRY_DELAY_MS = 250;

function buildConfigPayload(config) {
  return {
    BG_COLOUR: config.bgColour,
    HOUR_COLOUR: config.hourColour,
    MINUTE_COLOUR: config.minuteColour,
    H_HAND_COLOUR: config.hourHandColour,
    M_HAND_COLOUR: config.minuteHandColour,
    C_BG_COLOUR: config.complicationBgColour,
    C_BORDER_COLOUR: config.complicationBorderColour,
    C_TEXT_COLOUR: config.complicationTextColour,
    SHOW_HANDS: config.showHands ? 1 : 0,
    SHOW_TEXTURE: config.showTexture ? 1 : 0,
    TWELVE_HOUR: config.twelveHour ? 1 : 0,
    SHOW_DATE_COMPLICATION: config.showDateComplication ? 1 : 0,
    FACE_MODE: config.faceMode,
    SHOW_DATE_TILES: config.showDateTiles ? 1 : 0
  };
}

function retryDelayForAttempt(attempt) {
  return BASE_RETRY_DELAY_MS * (attempt + 1);
}

function sendPayloadWithRetry(payload, sendToken, attempt) {
  // An older retry may still be queued after a newer configuration was sent.
  if (sendToken !== activeSendToken) {
    return;
  }
  Pebble.sendAppMessage(
    payload,
    function() {
      if (sendToken === activeSendToken) {
        console.log(`Config sent on attempt ${attempt + 1}`);
      }
    },
    function(error) {
      if (sendToken !== activeSendToken) {
        return;
      }

      if (attempt + 1 >= MAX_SEND_ATTEMPTS) {
        console.log(`Config send failed after ${MAX_SEND_ATTEMPTS} attempts: ${JSON.stringify(error)}`);
        return;
      }

      const delayMs = retryDelayForAttempt(attempt);
      console.log(`Config send retry ${attempt + 2}/${MAX_SEND_ATTEMPTS} in ${delayMs}ms`);
      setTimeout(function() {
        sendPayloadWithRetry(payload, sendToken, attempt + 1);
      }, delayMs);
    }
  );
}

function sendConfigToWatch(config) {
  activeSendToken += 1;
  sendPayloadWithRetry(buildConfigPayload(config), activeSendToken, 0);
}

module.exports = {
  loadConfig,
  saveConfig,
  normaliseConfig,
  parseConfigPayload,
  sendConfigToWatch
};
