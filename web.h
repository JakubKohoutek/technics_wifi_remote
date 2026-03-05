#ifndef WEB_H
#define WEB_H

const char INDEX_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Technics Amplifier</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif; background: #1a1a2e; color: #eee; padding: 16px; max-width: 480px; margin: 0 auto; }
    h1 { font-size: 1.3em; text-align: center; margin-bottom: 12px; color: #e94560; }
    .status { display: flex; justify-content: space-around; margin-bottom: 16px; padding: 12px; background: #16213e; border-radius: 8px; }
    .status-item { text-align: center; }
    .status-label { font-size: 0.75em; color: #888; text-transform: uppercase; }
    .status-value { font-size: 1.1em; font-weight: bold; margin-top: 4px; }
    .on { color: #4ecca3; }
    .off { color: #e94560; }
    .section { margin-bottom: 12px; }
    .section-title { font-size: 0.8em; color: #888; text-transform: uppercase; margin-bottom: 6px; padding-left: 4px; }
    .btn-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; }
    .btn-grid-2 { grid-template-columns: repeat(2, 1fr); }
    button { background: #16213e; color: #eee; border: 1px solid #333; border-radius: 8px; padding: 12px 8px; font-size: 0.85em; cursor: pointer; transition: all 0.15s; }
    button:active { background: #e94560; transform: scale(0.95); }
    button:hover { border-color: #e94560; }
    .btn-power { background: #e94560; border-color: #e94560; font-weight: bold; }
    .btn-power:active { background: #c73650; }
    .btn-mute { background: #533483; border-color: #533483; }
    .btn-vol { font-size: 1.2em; font-weight: bold; }
    .feedback { text-align: center; padding: 6px; font-size: 0.8em; color: #4ecca3; min-height: 24px; }
  </style>
</head>
<body>
  <h1>Technics Amplifier</h1>
  <div class="status">
    <div class="status-item">
      <div class="status-label">Power</div>
      <div class="status-value" id="power">--</div>
    </div>
    <div class="status-item">
      <div class="status-label">BT Audio</div>
      <div class="status-value" id="bt_audio">--</div>
    </div>
  </div>
  <div id="feedback" class="feedback"></div>

  <div class="section">
    <div class="section-title">Power &amp; Volume</div>
    <div class="btn-grid">
      <button class="btn-power" onclick="send('power_toggle')">POWER</button>
      <button class="btn-vol" onclick="send('volume_up')">VOL +</button>
      <button class="btn-vol" onclick="send('volume_down')">VOL -</button>
      <button class="btn-mute" onclick="send('mute')">MUTE</button>
    </div>
  </div>

  <div class="section">
    <div class="section-title">Input Select</div>
    <div class="btn-grid btn-grid-2">
      <button onclick="send('vcr1_select')">VCR1</button>
      <button onclick="send('cd_select')">CD</button>
      <button onclick="send('deck_select')">DECK</button>
    </div>
  </div>

  <div class="section">
    <div class="section-title">CD Control</div>
    <div class="btn-grid">
      <button onclick="send('cd_bwd')">&lt;&lt;</button>
      <button onclick="send('cd_play')">PLAY</button>
      <button onclick="send('cd_fwd')">&gt;&gt;</button>
      <button onclick="send('cd_stop')">STOP</button>
    </div>
  </div>

  <div class="section">
    <div class="section-title">Deck Control</div>
    <div class="btn-grid">
      <button onclick="send('deck_play_bwd')">&lt;&lt;</button>
      <button onclick="send('deck_play_fwd')">PLAY</button>
      <button onclick="send('deck_stop')">STOP</button>
      <button onclick="send('deck_pause')">PAUSE</button>
      <button onclick="send('deck_record')">REC</button>
    </div>
  </div>

  <script>
    function send(cmd) {
      fetch('/api/send?cmd=' + cmd).then(r => r.text()).then(t => {
        document.getElementById('feedback').textContent = t;
        setTimeout(() => document.getElementById('feedback').textContent = '', 2000);
      }).catch(() => {
        document.getElementById('feedback').textContent = 'Error sending command';
      });
    }
    function updateStatus() {
      fetch('/api/status').then(r => r.json()).then(d => {
        var p = document.getElementById('power');
        var b = document.getElementById('bt_audio');
        p.textContent = d.power ? 'ON' : 'OFF';
        p.className = 'status-value ' + (d.power ? 'on' : 'off');
        b.textContent = d.bt_audio ? 'Playing' : 'Silent';
        b.className = 'status-value ' + (d.bt_audio ? 'on' : 'off');
      }).catch(() => {});
    }
    updateStatus();
    setInterval(updateStatus, 3000);
  </script>
</body>
</html>
)=====";

#endif // WEB_H
