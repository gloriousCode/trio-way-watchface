function buildConfigPageScript(presetJson, paletteJson, groupOrderJson, initialConfig) {
  return `<script>
      var presets = ${presetJson};
      var groupedPalette = ${paletteJson};
      var groupOrder = ${groupOrderJson};
      var initial = ${initialConfig};
      var weekdayPreference = !!initial.showDateTiles;

      function getReturnToUrl() {
        var value = null;
        var query = window.location.search || "";
        var match = query.match(/[?&]return_to=([^&]+)/);

        if (match && match[1]) {
          value = match[1];
        }

        if (!value) {
          var hrefMatch = window.location.href.match(/[?&]return_to=([^&]+)/);
          if (hrefMatch && hrefMatch[1]) {
            value = hrefMatch[1];
          }
        }

        return value ? decodeURIComponent(value) : null;
      }

      function buildReturnToCallback(baseUrl, encodedPayload) {
        function endsWith(text, suffix) {
          if (!text || !suffix) {
            return false;
          }

          return text.slice(-suffix.length) === suffix;
        }

        var separator = baseUrl.indexOf("?") === -1
          ? "?"
          : (endsWith(baseUrl, "?") || endsWith(baseUrl, "&") ? "" : "&");

        return baseUrl + separator + "response=" + encodedPayload;
      }

      function postReturnTo(encoded) {
        var returnTo = getReturnToUrl();

        if (!returnTo || returnTo.indexOf("http") !== 0) {
          return false;
        }

        var callbackUrl = buildReturnToCallback(returnTo, encoded) + "&_ts=" + Date.now();
        try {
          var beacon = new Image();
          beacon.src = callbackUrl;
        } catch (error) {
          // Keep trying with iframe below.
        }

        try {
          var iframe = document.getElementById("applyTransportFrame");

          if (!iframe) {
            iframe = document.createElement("iframe");
            iframe.id = "applyTransportFrame";
            iframe.style.display = "none";
            document.body.appendChild(iframe);
          }

          iframe.src = callbackUrl;
        } catch (error) {
          // Keep best-effort transport.
        }

        return true;
      }

      function sendConfig(response, closeAfter) {
        var encoded = encodeURIComponent(JSON.stringify(response));
        var status = document.getElementById("status");

        if (!closeAfter) {
          if (postReturnTo(encoded)) {
            status.textContent = "Sent to emulator. Save & close when finished.";
          } else {
            status.textContent = "Live apply unavailable here. Use Save.";
          }
          return;
        }

        // Preferred path: Pebble app intercepts this scheme and closes reliably.
        document.location = "pebblejs://close#" + encoded;

        // Fallback path for emulator return_to callbacks.
        setTimeout(function() {
          postReturnTo(encoded);
        }, 150);
      }

      function addGroupedOptions(selectId) {
        var select = document.getElementById(selectId);

        for (var i = 0; i < groupOrder.length; i += 1) {
          var groupName = groupOrder[i];
          var options = groupedPalette[groupName];
          var optgroup = document.createElement("optgroup");
          optgroup.label = groupName;

          for (var j = 0; j < options.length; j += 1) {
            var option = document.createElement("option");
            option.value = options[j].colour;
            option.textContent = groupName + " · " + options[j].colour.toUpperCase();
            option.style.backgroundColor = options[j].colour;
            option.style.color = getContrastText(options[j].colour);
            option.style.fontWeight = "700";
            optgroup.appendChild(option);
          }

          select.appendChild(optgroup);
        }
      }

      function snapChannel(value) {
        var steps = [0, 85, 170, 255];
        var best = steps[0];
        var bestDistance = Math.abs(value - best);

        for (var i = 1; i < steps.length; i += 1) {
          var distance = Math.abs(value - steps[i]);
          if (distance < bestDistance) {
            bestDistance = distance;
            best = steps[i];
          }
        }

        return best;
      }

      function toHex(value) {
        var text = value.toString(16);
        return text.length === 1 ? "0" + text : text;
      }

      function toPaletteColour(value, fallback) {
        var text = String(value || "").trim().toLowerCase();
        var match = text.match(/^#?[0-9a-f]{6}$/);
        var source = match ? (text.charAt(0) === "#" ? text.slice(1) : text) : fallback.replace("#", "");
        var r = parseInt(source.slice(0, 2), 16);
        var g = parseInt(source.slice(2, 4), 16);
        var b = parseInt(source.slice(4, 6), 16);

        return "#" + toHex(snapChannel(r)) + toHex(snapChannel(g)) + toHex(snapChannel(b));
      }

      function setColourSelectValue(selectId, value, fallback) {
        var select = document.getElementById(selectId);
        var snapped = toPaletteColour(value, fallback);
        select.value = snapped;

        if (!select.value && select.options.length > 0) {
          select.selectedIndex = 0;
        }
      }

      function getColourSelectValue(selectId, fallback) {
        var select = document.getElementById(selectId);
        var value = select.value;
        return toPaletteColour(value, fallback);
      }

      function syncSwatches() {
        document.getElementById("bgSwatch").style.backgroundColor = document.getElementById("bgColour").value;
        document.getElementById("hourSwatch").style.backgroundColor = document.getElementById("hourColour").value;
        document.getElementById("minuteSwatch").style.backgroundColor = document.getElementById("minuteColour").value;
        document.getElementById("hourHandSwatch").style.backgroundColor = document.getElementById("hourHandColour").value;
        document.getElementById("minuteHandSwatch").style.backgroundColor = document.getElementById("minuteHandColour").value;
        document.getElementById("complicationBgSwatch").style.backgroundColor = document.getElementById("complicationBgColour").value;
        document.getElementById("complicationBorderSwatch").style.backgroundColor = document.getElementById("complicationBorderColour").value;
        document.getElementById("complicationTextSwatch").style.backgroundColor = document.getElementById("complicationTextColour").value;
        syncSelectPreview("bgColour");
        syncSelectPreview("hourColour");
        syncSelectPreview("minuteColour");
        syncSelectPreview("hourHandColour");
        syncSelectPreview("minuteHandColour");
        syncSelectPreview("complicationBgColour");
        syncSelectPreview("complicationBorderColour");
        syncSelectPreview("complicationTextColour");
      }

      function applyPreset(name) {
        if (!name || !presets[name]) {
          return;
        }

        var preset = presets[name];
        setColourSelectValue("bgColour", preset.bgColour, initial.bgColour);
        setColourSelectValue("hourColour", preset.hourColour, initial.hourColour);
        setColourSelectValue("minuteColour", preset.minuteColour, initial.minuteColour);
        setColourSelectValue("hourHandColour", preset.hourHandColour || preset.hourColour, "#000000");
        setColourSelectValue("minuteHandColour", preset.minuteHandColour || preset.minuteColour, "#000000");
        setColourSelectValue("complicationBgColour", preset.complicationBgColour || preset.bgColour, initial.complicationBgColour);
        setColourSelectValue("complicationBorderColour", preset.complicationBorderColour || preset.minuteColour, initial.complicationBorderColour);
        setColourSelectValue("complicationTextColour", preset.complicationTextColour || preset.hourColour, initial.complicationTextColour);
      }

      function findMatchingPresetKey() {
        for (var key in presets) {
          if (!Object.prototype.hasOwnProperty.call(presets, key)) {
            continue;
          }

          var preset = presets[key];
          var fields = ["bgColour", "hourColour", "minuteColour", "hourHandColour", "minuteHandColour",
            "complicationBgColour", "complicationBorderColour", "complicationTextColour"];
          var matches = fields.every(function(field) {
            return toPaletteColour(preset[field], initial[field]) === document.getElementById(field).value;
          });
          if (matches) {
            return key;
          }
        }

        return "";
      }

      function syncPresetPreview() {
        var key = document.getElementById("preset").value;
        var label = document.getElementById("presetPreviewLabel");
        var bg = document.getElementById("presetBgSwatch");
        var hour = document.getElementById("presetHourSwatch");
        var minute = document.getElementById("presetMinuteSwatch");
        var source = presets[key];

        if (!source) {
          label.textContent = "Custom";
          source = {
            bgColour: document.getElementById("bgColour").value,
            hourColour: document.getElementById("hourColour").value,
            minuteColour: document.getElementById("minuteColour").value
          };
        } else {
          label.textContent = source.label;
        }

        bg.style.backgroundColor = toPaletteColour(source.bgColour, initial.bgColour);
        hour.style.backgroundColor = toPaletteColour(source.hourColour, initial.hourColour);
        minute.style.backgroundColor = toPaletteColour(source.minuteColour, initial.minuteColour);
      }

      function getContrastText(hex) {
        var safe = String(hex || "").replace("#", "");

        if (safe.length !== 6) {
          return "#111";
        }

        var r = parseInt(safe.slice(0, 2), 16);
        var g = parseInt(safe.slice(2, 4), 16);
        var b = parseInt(safe.slice(4, 6), 16);
        var luminance = (r * 299 + g * 587 + b * 114) / 1000;
        return luminance >= 148 ? "#1f1a15" : "#ffffff";
      }

      function syncSelectPreview(selectId) {
        var select = document.getElementById(selectId);
        var colour = select.value || "#ffffff";
        select.style.backgroundColor = colour;
        select.style.color = getContrastText(colour);
        select.style.fontWeight = "700";
      }

      function syncDateTileControl() {
        var mode = document.getElementById("faceMode").value;
        var dateToggle = document.getElementById("showDateTiles");
        var largeDigital = mode === "largedigital";

        dateToggle.checked = largeDigital ? false : weekdayPreference;

        dateToggle.disabled = largeDigital;
      }

      function syncDateComplicationControl() {
        var mode = document.getElementById("faceMode").value;
        var toggle = document.getElementById("showDateComplication");
        var tiles = document.getElementById("showDateTiles");
        var bg = document.getElementById("complicationBgColour");
        var border = document.getElementById("complicationBorderColour");
        var text = document.getElementById("complicationTextColour");
        var supported = mode === "hands" || mode === "numbered" || mode === "ticks" || mode === "digital" || mode === "largedigital";

        if (!supported) {
          toggle.checked = false;
        }

        toggle.disabled = !supported;
        var hasDate = toggle.checked || tiles.checked;
        bg.disabled = !supported || !hasDate;
        border.disabled = !supported || !hasDate;
        text.disabled = !supported || !hasDate;
      }

      function syncHandControls() {
        var mode = document.getElementById("faceMode").value;
        var showHands = document.getElementById("showHands");
        var hourHandColour = document.getElementById("hourHandColour");
        var minuteHandColour = document.getElementById("minuteHandColour");
        var isAnalogue = mode === "hands" || mode === "numbered" || mode === "ticks";
        var hasNumerals = mode === "numbered" || mode === "digital" || mode === "largedigital";

        showHands.disabled = !isAnalogue;
        document.getElementById("handsHelp").textContent = mode === "digital"
          ? "Hybrid always uses hands to point to the hour and minute labels."
          : (mode === "largedigital" ? "Digital Large uses numbers without hands." : "Show or hide hands on this analogue dial.");
        document.getElementById("twelveHour").disabled = isAnalogue;
        syncClockHelp();
        hourHandColour.disabled = !hasNumerals && (!isAnalogue || !showHands.checked);
        minuteHandColour.disabled = !hasNumerals && (!isAnalogue || !showHands.checked);
      }

      function syncClockHelp() {
        var control = document.getElementById("twelveHour");
        document.getElementById("clockHelp").textContent = control.disabled
          ? "Choose Hybrid or Digital Large to use this setting. Analogue dials remain 1–12."
          : (control.checked ? "12-hour: 01:37. Midnight and noon show 12:00. No AM/PM label."
                             : "24-hour: 13:37. Midnight shows 00:00.");
      }

      var returnTo = getReturnToUrl();
      document.getElementById("apply").style.display = returnTo && /^https?:/.test(returnTo) ? "" : "none";
      document.getElementById("twelveHour").addEventListener("change", syncClockHelp);
      ["faceMode", "preset", "twelveHour", "showHands", "showTexture", "showDateTiles", "showDateComplication",
       "bgColour", "hourColour", "minuteColour", "hourHandColour", "minuteHandColour",
       "complicationBgColour", "complicationBorderColour", "complicationTextColour"].forEach(function(id) {
        document.getElementById(id).addEventListener("change", function() {
          document.getElementById("status").textContent = "Unsaved changes — tap Save & close when ready.";
        });
      });

      addGroupedOptions("bgColour");
      addGroupedOptions("hourColour");
      addGroupedOptions("minuteColour");
      addGroupedOptions("hourHandColour");
      addGroupedOptions("minuteHandColour");
      addGroupedOptions("complicationBgColour");
      addGroupedOptions("complicationBorderColour");
      addGroupedOptions("complicationTextColour");

      document.getElementById("faceMode").value = initial.faceMode;
      document.getElementById("showHands").checked = initial.showHands !== false;
      document.getElementById("showTexture").checked = !!initial.showTexture;
      document.getElementById("twelveHour").checked = !!initial.twelveHour;
      document.getElementById("showDateTiles").checked = !!initial.showDateTiles;
      document.getElementById("showDateComplication").checked = !!initial.showDateComplication;
      setColourSelectValue("bgColour", initial.bgColour, initial.bgColour);
      setColourSelectValue("hourColour", initial.hourColour, initial.hourColour);
      setColourSelectValue("minuteColour", initial.minuteColour, initial.minuteColour);
      setColourSelectValue("hourHandColour", initial.hourHandColour, "#000000");
      setColourSelectValue("minuteHandColour", initial.minuteHandColour, "#000000");
      setColourSelectValue("complicationBgColour", initial.complicationBgColour, initial.bgColour);
      setColourSelectValue("complicationBorderColour", initial.complicationBorderColour, initial.minuteColour);
      setColourSelectValue("complicationTextColour", initial.complicationTextColour, initial.hourColour);
      document.getElementById("preset").value = findMatchingPresetKey();

      document.getElementById("preset").addEventListener("change", function(event) {
        applyPreset(event.target.value);
        syncSwatches();
        syncPresetPreview();
      });

      document.getElementById("faceMode").addEventListener("change", function() {
        syncDateTileControl();
        syncDateComplicationControl();
        syncHandControls();
      });

      document.getElementById("showHands").addEventListener("change", function() {
        syncHandControls();
      });
      document.getElementById("showDateComplication").addEventListener("change", function() {
        syncDateComplicationControl();
      });
      document.getElementById("showDateTiles").addEventListener("change", function() {
        weekdayPreference = document.getElementById("showDateTiles").checked;
        syncDateComplicationControl();
      });

      document.getElementById("cancel").addEventListener("click", function() {
        sendConfig(null, true);
      });

      document.getElementById("apply").addEventListener("click", function() {
        sendConfig({
          faceMode: document.getElementById("faceMode").value,
          showHands: document.getElementById("showHands").checked,
          showTexture: document.getElementById("showTexture").checked,
          twelveHour: document.getElementById("twelveHour").checked,
          showDateTiles: document.getElementById("showDateTiles").checked,
          showDateComplication: document.getElementById("showDateComplication").checked,
          bgColour: getColourSelectValue("bgColour", initial.bgColour),
          hourColour: getColourSelectValue("hourColour", initial.hourColour),
          minuteColour: getColourSelectValue("minuteColour", initial.minuteColour),
          hourHandColour: getColourSelectValue("hourHandColour", "#000000"),
          minuteHandColour: getColourSelectValue("minuteHandColour", "#000000"),
          complicationBgColour: getColourSelectValue("complicationBgColour", initial.bgColour),
          complicationBorderColour: getColourSelectValue("complicationBorderColour", initial.minuteColour),
          complicationTextColour: getColourSelectValue("complicationTextColour", initial.hourColour)
        }, false);
      });

      document.getElementById("save").addEventListener("click", function() {
        sendConfig({
          faceMode: document.getElementById("faceMode").value,
          showHands: document.getElementById("showHands").checked,
          showTexture: document.getElementById("showTexture").checked,
          twelveHour: document.getElementById("twelveHour").checked,
          showDateTiles: document.getElementById("showDateTiles").checked,
          showDateComplication: document.getElementById("showDateComplication").checked,
          bgColour: getColourSelectValue("bgColour", initial.bgColour),
          hourColour: getColourSelectValue("hourColour", initial.hourColour),
          minuteColour: getColourSelectValue("minuteColour", initial.minuteColour),
          hourHandColour: getColourSelectValue("hourHandColour", "#000000"),
          minuteHandColour: getColourSelectValue("minuteHandColour", "#000000"),
          complicationBgColour: getColourSelectValue("complicationBgColour", initial.bgColour),
          complicationBorderColour: getColourSelectValue("complicationBorderColour", initial.minuteColour),
          complicationTextColour: getColourSelectValue("complicationTextColour", initial.hourColour)
        }, true);
      });

      ["bgColour", "hourColour", "minuteColour", "hourHandColour", "minuteHandColour",
       "complicationBgColour", "complicationBorderColour", "complicationTextColour"].forEach(function(id) {
        document.getElementById(id).addEventListener("change", function() {
          document.getElementById("preset").value = findMatchingPresetKey();
          syncSwatches();
          syncPresetPreview();
        });
      });

      syncDateTileControl();
      syncDateComplicationControl();
      syncHandControls();
      syncSwatches();
      syncPresetPreview();
    </script>`;
}

module.exports = {
  buildConfigPageScript
};
