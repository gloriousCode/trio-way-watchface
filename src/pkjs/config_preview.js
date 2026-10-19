// Runs inside the self-contained phone webview; no network or watch messages.
function renderWatchPreview() {
  function value(id) { return document.getElementById(id).value; }
  function checked(id) { return document.getElementById(id).checked; }
  var round = value('previewShape') === 'round';
  var width = 200;
  var height = round ? 200 : 228;
  var cy = height / 2;
  var mode = value('faceMode');
  var large = mode === 'largedigital';
  var hybrid = mode === 'digital';
  var hour = 48.5; // 13:37, including the hour hand's minute progress.
  var minute = 222;
  var split = 315.25;
  var svg = document.getElementById('watchPreview');
  svg.style.borderRadius = round ? '50%' : '10px';
  svg.setAttribute('viewBox', '0 0 200 ' + height);
  svg.setAttribute('aria-label', 'Illustrative ' + (round ? 'round' : 'rectangular') + ' watch preview at ' +
    ((large || hybrid) && checked('twelveHour') ? '01:37' : '13:37'));
  function point(angle, radius) {
    var radians = angle * Math.PI / 180;
    return [100 + Math.sin(radians) * radius, cy - Math.cos(radians) * radius];
  }
  function text(label, x, y, size, colour) {
    return '<text x="' + x + '" y="' + y + '" text-anchor="middle" font-family="monospace" font-weight="bold" font-size="' + size +
      '" fill="' + colour + '" stroke="#000" stroke-width="1" paint-order="stroke">' + label + '</text>';
  }
  function line(angle, start, end, colour, thickness) {
    var a = point(angle, start), b = point(angle, end);
    return '<path d="M' + a.join(' ') + ' L' + b.join(' ') + '" fill="none" stroke="' + colour + '" stroke-width="' + thickness + '"/>';
  }
  var clip = round ? '<circle cx="100" cy="100" r="99"/>' : '<rect width="200" height="228" rx="4"/>';
  var output = '<defs><clipPath id="screenClip">' + clip + '</clipPath>' +
    '<pattern id="previewGrain" width="6" height="6" patternUnits="userSpaceOnUse"><circle cx="1" cy="1" r=".6" fill="#000" opacity=".2"/></pattern></defs><g clip-path="url(#screenClip)">';
  var rays = [hour, minute, split, hour + 360];
  var colours = [value('hourColour'), value('minuteColour'), value('bgColour')];
  for (var i = 0; i < 3; i += 1) {
    var a = point(rays[i], 350), b = point(rays[i + 1], 350);
    output += '<path d="M100 ' + cy + ' L' + a.join(' ') + ' A350 350 0 0 1 ' + b.join(' ') + ' Z" fill="' + colours[i] + '"/>';
  }
  if (checked('showTexture')) output += '<rect width="200" height="' + height + '" fill="url(#previewGrain)"/>';
  if (mode === 'ticks') {
    for (var tick = 0; tick < 60; tick += 1) {
      output += line(tick * 6, tick % 5 ? 92 : 86, 96, '#000', 3);
      output += line(tick * 6, tick % 5 ? 92 : 86, 96, '#ffffff', 1);
    }
  }
  if (mode === 'numbered') {
    for (var number = 1; number <= 12; number += 1) {
      var p = point(number * 30, 82);
      output += text(number, p[0], p[1] + 6, 19, number === 1 ? value('hourHandColour') : number === 7 ? value('minuteHandColour') : '#ffffff');
    }
  }
  if (large) {
    output += text(checked('twelveHour') ? '01' : '13', 100, cy - 17, 70, value('hourHandColour'));
    output += text('37', 100, cy + 78, 70, value('minuteHandColour'));
  }
  if (hybrid || (!large && checked('showHands'))) {
    var length = hybrid || mode === 'numbered' ? 65 : 81;
    output += line(hour, 0, length, '#000', 7) + line(minute, 0, length + 6, '#000', 5);
    output += line(hour, 0, length, value('hourHandColour'), 5) + line(minute, 0, length + 6, value('minuteHandColour'), 3);
  }
  if (hybrid) {
    var hp = point(hour, 84), mp = point(minute, 84);
    output += text(checked('twelveHour') ? '01' : '13', hp[0], hp[1] + 7, 23, value('hourHandColour'));
    output += text('37', mp[0], mp[1] + 7, 23, value('minuteHandColour'));
  }
  if (large || hybrid || checked('showHands')) {
    output += '<circle cx="100" cy="' + cy + '" r="4" fill="#000"/><circle cx="100" cy="' + cy + '" r="2" fill="' + value('minuteHandColour') + '"/>';
  }
  var weekday = !large && checked('showDateTiles');
  if (weekday || checked('showDateComplication')) {
    var dateX = large ? 156 : 100, dateY = large ? cy : cy + 45;
    var badgeWidth = weekday ? 66 : 30;
    output += '<rect x="' + (dateX - badgeWidth / 2) + '" y="' + (dateY - 12) + '" width="' + badgeWidth + '" height="24" rx="4" fill="' + value('complicationBgColour') + '" stroke="' + value('complicationBorderColour') + '"/>';
    output += '<text x="' + dateX + '" y="' + (dateY + 5) + '" text-anchor="middle" font-family="sans-serif" font-size="14" fill="' + value('complicationTextColour') + '">' + (weekday ? 'Mon 19' : '19') + '</text>';
  }
  svg.innerHTML = output + '</g>';
}

module.exports = { renderWatchPreview };
