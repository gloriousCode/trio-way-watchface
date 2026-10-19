const STORAGE_KEY = "emery-colour-hands-config";

const FACE_MODES = {
  HANDS: "hands",
  DIGITAL: "digital",
  LARGE_DIGITAL: "largedigital",
  NUMBERED: "numbered",
  TICKS: "ticks"
};

// Explicit palettes keep the large sectors quiet and reserve colour for detail.
// Saved configurations store colours, so replacing preset names preserves them.
const PRESETS = {
  classic: {
    label: "Classic · Gold",
    bgColour: "#555555",
    hourColour: "#000000",
    minuteColour: "#aaaaaa",
    hourHandColour: "#ffffff",
    minuteHandColour: "#ffaa55",
    complicationBgColour: "#000000",
    complicationBorderColour: "#aaaaaa",
    complicationTextColour: "#ffffff"
  },
  graphite: {
    label: "Graphite",
    bgColour: "#000000",
    hourColour: "#555555",
    minuteColour: "#aaaaaa",
    hourHandColour: "#ffffff",
    minuteHandColour: "#aaaaaa",
    complicationBgColour: "#000000",
    complicationBorderColour: "#aaaaaa",
    complicationTextColour: "#ffffff"
  },
  silver: {
    label: "Silver",
    bgColour: "#aaaaaa",
    hourColour: "#555555",
    minuteColour: "#ffffff",
    hourHandColour: "#000000",
    minuteHandColour: "#555555",
    complicationBgColour: "#ffffff",
    complicationBorderColour: "#555555",
    complicationTextColour: "#000000"
  },
  paper: {
    label: "Paper",
    bgColour: "#ffffff",
    hourColour: "#aaaaaa",
    minuteColour: "#555555",
    hourHandColour: "#000000",
    minuteHandColour: "#555555",
    complicationBgColour: "#ffffff",
    complicationBorderColour: "#000000",
    complicationTextColour: "#000000"
  },
  inkBlue: {
    label: "Ink · Blue",
    bgColour: "#555555",
    hourColour: "#000000",
    minuteColour: "#ffffff",
    hourHandColour: "#ffffff",
    minuteHandColour: "#55aaff",
    complicationBgColour: "#000000",
    complicationBorderColour: "#aaaaaa",
    complicationTextColour: "#ffffff"
  },
  warmStone: {
    label: "Stone · Gold",
    bgColour: "#aaaaaa",
    hourColour: "#000000",
    minuteColour: "#555555",
    hourHandColour: "#ffffff",
    minuteHandColour: "#ffaa55",
    complicationBgColour: "#000000",
    complicationBorderColour: "#aaaaaa",
    complicationTextColour: "#ffffff"
  },
  porcelain: {
    label: "Porcelain · Blue",
    bgColour: "#ffffff",
    hourColour: "#555555",
    minuteColour: "#aaaaaa",
    hourHandColour: "#000000",
    minuteHandColour: "#0055aa",
    complicationBgColour: "#ffffff",
    complicationBorderColour: "#555555",
    complicationTextColour: "#000000"
  },
  navy: {
    label: "Tonal · Navy",
    bgColour: "#000055",
    hourColour: "#5555aa",
    minuteColour: "#aaaaff",
    hourHandColour: "#ffffff",
    minuteHandColour: "#aaaaff",
    complicationBgColour: "#000000",
    complicationBorderColour: "#aaaaaa",
    complicationTextColour: "#ffffff"
  },
  forest: {
    label: "Tonal · Forest",
    bgColour: "#005500",
    hourColour: "#55aa55",
    minuteColour: "#aaffaa",
    hourHandColour: "#ffffff",
    minuteHandColour: "#aaffaa",
    complicationBgColour: "#000000",
    complicationBorderColour: "#aaaaaa",
    complicationTextColour: "#ffffff"
  },
  burgundy: {
    label: "Tonal · Burgundy",
    bgColour: "#550000",
    hourColour: "#aa5555",
    minuteColour: "#ffaaaa",
    hourHandColour: "#ffffff",
    minuteHandColour: "#ffaaaa",
    complicationBgColour: "#000000",
    complicationBorderColour: "#aaaaaa",
    complicationTextColour: "#ffffff"
  },
  dusk: {
    label: "Dusk · Mauve & Gold",
    bgColour: "#000055",
    hourColour: "#555555",
    minuteColour: "#aa55aa",
    hourHandColour: "#ffffff",
    minuteHandColour: "#ffaa55",
    complicationBgColour: "#000000",
    complicationBorderColour: "#aaaaaa",
    complicationTextColour: "#ffffff"
  },
  lagoon: {
    label: "Lagoon · Seafoam",
    bgColour: "#005555",
    hourColour: "#000055",
    minuteColour: "#55aaaa",
    hourHandColour: "#ffffff",
    minuteHandColour: "#ffffaa",
    complicationBgColour: "#000000",
    complicationBorderColour: "#aaaaaa",
    complicationTextColour: "#ffffff"
  },
  ember: {
    label: "Ember · Sand",
    bgColour: "#550000",
    hourColour: "#555555",
    minuteColour: "#ffaa55",
    hourHandColour: "#ffffff",
    minuteHandColour: "#ffaaaa",
    complicationBgColour: "#000000",
    complicationBorderColour: "#aaaaaa",
    complicationTextColour: "#ffffff"
  }
};

const defaults = {
  bgColour: PRESETS.classic.bgColour,
  hourColour: PRESETS.classic.hourColour,
  minuteColour: PRESETS.classic.minuteColour,
  hourHandColour: PRESETS.classic.hourHandColour,
  minuteHandColour: PRESETS.classic.minuteHandColour,
  complicationBgColour: PRESETS.classic.complicationBgColour,
  complicationBorderColour: PRESETS.classic.complicationBorderColour,
  complicationTextColour: PRESETS.classic.complicationTextColour,
  showHands: true,
  showTexture: false,
  showDateComplication: true,
  faceMode: FACE_MODES.HANDS,
  showDateTiles: false
};

const GROUP_ORDER = [
  "Neutrals",
  "Reds",
  "Oranges & Yellows",
  "Greens",
  "Cyans",
  "Blues",
  "Purples",
  "Pinks"
];

module.exports = {
  STORAGE_KEY,
  FACE_MODES,
  defaults,
  PRESETS,
  GROUP_ORDER
};
