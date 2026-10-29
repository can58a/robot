// F1 JOYSTICK: DEAD ZONE - All-In-One (v1.0.0 - Code-Locked Zero Cal + 40-15° Throttle + +/-30° Relative Steering)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <DNSServer.h>
#include <Wire.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecure.h>
#include <Updater.h>

// 1. Controller's local network (For your phone/browser)
// NOTE: ESP8266 requires PASS to be at least 8 characters
const char* DEVICE_NAME = "F1 joystick v1.0.0";
const char* SSID = "F1 joystick"; 
const char* PASS = "f12345678";

// 2. Mobile Hotspot / Router Wi-Fi (For internet access)
const char* STA_SSID = "robot";
const char* STA_PASS = "happyworld";

// 3. Direct link to your compiled .bin file
const char* FIRMWARE_URL = "https://raw.githubusercontent.com/can58a/robot/refs/heads/main/esp8266_drone.ino.bin";

// 4. Fixed Zero Calibration Offsets Set in Code (User cannot alter manually)
const float FIXED_ROLL_ZERO  = 0.0;
const float FIXED_PITCH_ZERO = 0.0;

const uint8_t MPU = 0x68;

DNSServer dnsServer;
ESP8266WebServer http(80);
WebSocketsServer ws(81);

float roll = 0, pitch = 0, gxOff = 0, gyOff = 0, gzOff = 0;
float rollZero = FIXED_ROLL_ZERO, pitchZero = FIXED_PITCH_ZERO;
unsigned long lastUs = 0;
bool mpuFound = false;
bool triggerOtaUpdate = false;
String pendingOtaError = "";

// Forward declarations to prevent the Arduino IDE preprocessor from breaking R"=====(
bool readMPU(float &ax, float &ay, float &az, float &gx, float &gy, float &gz);
void calibrateMPU();
void onWs(uint8_t num, WStype_t type, uint8_t* payload, size_t len);
void startAccessPoint();
void runOtaUpdate();

// =========================================================================
// EMBEDDED HTML GAME CODE (Base Code + 40-15° Top Speed + +/-30° Relative Steering)
// =========================================================================
const char GAME_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no" />
  <title>F1: DEAD ZONE</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; user-select: none; -webkit-user-select: none; }
    body, html { width: 100%; height: 100%; overflow: hidden; background: #000; font-family: 'Orbitron', 'Courier New', monospace; }
    #canvas { position: absolute; inset: 0; width: 100%; height: 100%; z-index: 1; }

    /* UI Overlays */
    .overlay {
      position: absolute; inset: 0; z-index: 50; background: #000;
      display: flex; flex-direction: column; align-items: center; justify-content: center;
      text-align: center; padding: 20px; color: #fff; overflow-y: auto;
    }
    .overlay h1 { font-size: 28px; color: #ef4444; letter-spacing: 3px; margin-bottom: 12px; text-shadow: 0 0 14px rgba(239, 68, 68, 0.8); }
    .overlay p { font-size: 13px; color: #cbd5e1; max-width: 420px; line-height: 1.5; margin-bottom: 16px; font-family: sans-serif; }
    
    .action-btn {
      background: #dc2626; color: #fff; border: 2px solid #f87171; border-radius: 4px;
      padding: 12px 28px; font-size: 16px; font-weight: bold; cursor: pointer;
      box-shadow: 0 0 20px rgba(220, 38, 38, 0.6); text-transform: uppercase; letter-spacing: 2px;
      font-family: 'Orbitron', sans-serif; margin-top: 10px;
    }
    .action-btn:active { background: #f87171; color: #000; box-shadow: 0 0 10px rgba(220, 38, 38, 0.8); }

    /* Simple Wave Graph Telemetry Panel */
    #telemetry-report {
      width: 100%; max-width: 460px; background: rgba(15, 23, 42, 0.95);
      border: 1px solid #334155; border-left: 4px solid #38bdf8; border-radius: 6px;
      padding: 12px 16px; margin-bottom: 14px; text-align: left; font-size: 12px;
      color: #e2e8f0; box-shadow: 0 10px 25px rgba(0,0,0,0.8);
    }
    .rep-row { display: flex; justify-content: space-between; margin-bottom: 8px; }
    .rep-label { color: #94a3b8; font-weight: 700; }
    .rep-val { color: #fff; font-weight: 700; text-align: right; }

    /* Wave Graph Canvas Styles */
    .wave-container {
      background: #080c14; border: 1px solid #1e293b; border-radius: 6px;
      padding: 8px; margin-top: 6px; text-align: center;
    }
    .wave-title { font-size: 11px; color: #38bdf8; font-weight: bold; margin-bottom: 4px; text-transform: uppercase; letter-spacing: 1px; }
    #wave-canvas { width: 100%; height: 110px; background: #020617; border-radius: 4px; display: block; }

    /* Realistic F1 Dashboard HUD */
    .hud-layer { position: absolute; inset: 0; z-index: 10; pointer-events: none; }
    
    #f1-wheel {
      position: absolute; bottom: 15px; left: 50%; transform: translateX(-50%);
      width: 480px; height: 260px;
      background: linear-gradient(27deg, #151515 5px, transparent 5px) 0 5px,
                  linear-gradient(207deg, #151515 5px, transparent 5px) 10px 0px,
                  linear-gradient(27deg, #222 5px, transparent 5px) 0px 10px,
                  linear-gradient(207deg, #222 5px, transparent 5px) 10px 5px, #000;
      background-size: 20px 20px;
      border: 4px solid #333; border-bottom: none;
      border-radius: 40px 40px 0 0; padding: 25px 20px;
      box-shadow: 0 20px 50px rgba(0,0,0,0.9), inset 0 5px 15px rgba(255,255,255,0.1);
      display: flex; flex-direction: column; align-items: center;
    }

    .rpm-leds {
      display: flex; justify-content: center; gap: 4px; margin-bottom: 15px; width: 100%;
      background: #000; padding: 6px; border-radius: 8px; border: 1px solid #222;
      box-shadow: inset 0 2px 5px rgba(0,0,0,1);
    }
    .led { width: 22px; height: 12px; border-radius: 2px; opacity: 0.2; }

    #f1-lcd {
      width: 100%; height: 100%; background: #080c14; border-radius: 8px;
      border: 2px solid #1e293b; position: relative; overflow: hidden;
      box-shadow: inset 0 0 30px rgba(0,0,0,1); padding: 12px;
      display: flex; flex-direction: column; justify-content: space-between;
    }
    
    #f1-lcd::after {
      content: ''; position: absolute; top: -50%; left: -50%; width: 200%; height: 200%;
      background: linear-gradient(135deg, rgba(255,255,255,0.08) 0%, rgba(255,255,255,0) 40%);
      transform: rotate(15deg); pointer-events: none;
    }

    .lcd-top { display: flex; justify-content: space-between; align-items: center; font-size: 14px; color: #38bdf8; font-weight: 700; letter-spacing: 1px; }
    .lcd-top span { color: #fff; text-shadow: 0 0 5px rgba(255,255,255,0.5); }
    
    .lcd-center { display: flex; justify-content: center; align-items: baseline; gap: 15px; margin-top: 5px; }
    
    .gear-box { text-align: center; color: #fff; margin-right: 15px; }
    .gear-label { font-size: 12px; color: #94a3b8; }
    .gear-val { font-size: 54px; font-weight: 900; color: #facc15; text-shadow: 0 0 15px rgba(250, 204, 21, 0.6); line-height: 1; }
    
    .speed-box { text-align: center; }
    .speed-val { font-size: 64px; font-weight: 900; color: #fff; line-height: 0.9; text-shadow: 0 0 20px rgba(255,255,255,0.8); }
    .speed-unit { font-size: 16px; color: #94a3b8; font-weight: 700; letter-spacing: 2px; }

    .lcd-bottom { display: flex; justify-content: space-between; font-size: 12px; color: #cbd5e1; }
    .lcd-bottom div { background: #111827; padding: 4px 8px; border-radius: 4px; border: 1px solid #334155; }
    .lcd-bottom span { color: #f59e0b; font-weight: bold; }

    #top-bar {
      position: absolute; top: 16px; left: 16px; right: 16px; display: flex; justify-content: space-between;
      font-size: 16px; color: #fff; font-weight: 700; text-shadow: 2px 2px 4px #000;
    }
    .top-panel { background: rgba(0,0,0,0.75); padding: 10px 20px; border-radius: 6px; border-left: 4px solid #ef4444; }

    /* Lightweight Lap 1+ Update Panel */
    #ota-panel {
      position: absolute; top: 16px; left: 50%; transform: translateX(-50%);
      width: 280px; background: #0f172a; border: 1px solid #334155; border-top: 3px solid #2563eb;
      border-radius: 8px; padding: 10px 12px; text-align: center; font-size: 11px;
      box-shadow: 0 8px 20px rgba(0,0,0,0.85); z-index: 60; display: block; pointer-events: auto;
    }
    .ota-bar-bg {
      width: 100%; height: 18px; background: #020617; border: 1px solid #1e293b;
      border-radius: 4px; overflow: hidden; margin: 6px 0; display: none; position: relative;
    }
    .ota-bar-fill {
      height: 100%; width: 0%; background: linear-gradient(90deg, #2563eb, #38bdf8);
      transition: width 0.55s linear;
    }
    .ota-bar-text {
      position: absolute; inset: 0; display: flex; align-items: center; justify-content: center;
      font-size: 10px; font-weight: bold; color: #fff; text-shadow: 0 1px 2px #000;
    }
    #ota-log { font-family: 'Courier New', monospace; font-size: 11px; color: #94a3b8; line-height: 1.35; margin-top: 4px; display: none; }
    #fw-update-btn {
      width: 100%; background: #2563eb; color: #fff; border: 1px solid #60a5fa;
      border-radius: 4px; padding: 7px 12px; font-size: 12px; font-weight: bold;
      cursor: pointer; font-family: 'Orbitron', sans-serif; letter-spacing: 1px;
    }
    #fw-update-btn:active { background: #60a5fa; color: #000; }
    #reload-btn {
      width: 100%; background: #16a34a; color: #fff; border: 2px solid #4ade80;
      border-radius: 4px; padding: 8px 12px; font-size: 11px; font-weight: bold;
      cursor: pointer; margin-top: 6px; font-family: 'Orbitron', sans-serif; display: none;
    }

    #level-up-overlay {
      position: absolute; top: 20%; left: 50%; transform: translateX(-50%); z-index: 40;
      background: rgba(15, 23, 42, 0.95); border: 2px solid #22c55e; border-radius: 8px;
      padding: 16px 24px; text-align: center; color: #22c55e; display: none;
      box-shadow: 0 0 30px rgba(34, 197, 94, 0.5); font-weight: bold;
    }

    #screen-flash { position: absolute; inset: 0; background: white; opacity: 0; pointer-events: none; z-index: 20; }
    #damage-flash { position: absolute; inset: 0; background: radial-gradient(circle, transparent 40%, rgba(220,38,38,0.8) 100%); opacity: 0; pointer-events: none; z-index: 20; }
  </style>
</head>
<body>

  <!-- Start Screen: ONLY Version & Ignition Button -->
  <div id="start-overlay" class="overlay">
    <p style="font-size: 18px; margin-bottom: 12px; letter-spacing: 1px;">
      <b style="color:#facc15;">F1 joystick v1.0.0</b>
    </p>
    <button id="start-btn" class="action-btn">IGNITION</button>
  </div>

  <!-- Lap 1+ Update Control Box (Shown from Lap 1) -->
  <div id="ota-panel">
    <button id="fw-update-btn" type="button">SYSTEM UPDATE</button>
    <div class="ota-bar-bg" id="ota-bar-wrap">
      <div class="ota-bar-fill" id="ota-bar"></div>
      <div class="ota-bar-text" id="ota-bar-pct">0%</div>
    </div>
    <div id="ota-log">if internet is available update will be live...</div>
    <button id="reload-btn" type="button" onclick="window.location.replace('http://192.168.4.1/?v=' + Date.now())">UPDATE SUCCESSFUL - TAP TO RELOAD</button>
  </div>

  <!-- Game Over / Wave Graph Telemetry Screen -->
  <div id="game-over-overlay" class="overlay" style="display:none;">
    <h1 id="go-title">CAR DESTROYED</h1>
    <p id="go-subtitle" style="font-size: 14px; color: #f87171; font-weight: bold; margin-bottom: 8px;"></p>

    <div id="telemetry-report">
      <div class="rep-row">
        <span class="rep-label">ATTEMPT RECORD:</span>
        <span class="rep-val" id="rep-lap-record" style="color:#38bdf8;">LAP 1 (TRY #1)</span>
      </div>
      <div class="rep-row">
        <span class="rep-label">DISTANCE COVERED:</span>
        <span class="rep-val" id="rep-dist-summary" style="color:#22c55e;">0.00 / 12.0 km</span>
      </div>

      <div class="wave-container">
        <div class="wave-title">Speed vs Time Wave Graph</div>
        <canvas id="wave-canvas" width="420" height="110"></canvas>
      </div>
    </div>

    <p style="font-size: 15px; margin-bottom: 4px;">SCORE: <span id="go-score" style="color: #38bdf8; font-weight: bold;">0</span> | BEST: <span id="go-best">0</span></p>
    <button id="resume-btn" class="action-btn" style="margin-top: 8px;">RETRY LAP</button>
  </div>

  <!-- Lap Notification -->
  <div id="level-up-overlay">
    <h2>LAP <span id="toast-lvl"></span> COMPLETED!</h2>
    <p style="color: #fff; font-size: 16px; margin-top: 8px;">Next Target: <b style="color:#f59e0b;"><span id="toast-target"></span> km</b> | Top Speed: <b style="color:#38bdf8;"><span id="toast-maxspd">400</span> km/h</b></p>
    <p style="color: #94a3b8; font-size: 14px; margin-top: 4px;">Systems Restored.</p>
  </div>

  <canvas id="canvas"></canvas>
  <div id="screen-flash"></div>
  <div id="damage-flash"></div>

  <div class="hud-layer">
    <div id="top-bar">
      <div class="top-panel">
        <div style="margin-bottom: 4px;">SCORE: <span id="val-score" style="color:#38bdf8;">00000000</span></div>
        <div>LAP <span id="val-lap" style="color:#eab308;">1</span> | CHANCES: <span id="val-chances" style="color:#22c55e;">∞ (#1)</span></div>
        <div style="margin-top: 4px;">DIST: <span id="val-lap-dist">0.00</span> / <span id="val-lap-target">12.0</span> km</div>
      </div>
      <div class="top-panel" style="border-left-color: #38bdf8; text-align: right;">
        <div style="margin-bottom: 4px;">TIME LEFT: <span id="val-lap-time" style="color:#22c55e;">120s</span></div>
        <div>F1 JOYSTICK v1.0.0: <span id="val-link" style="color:#ef4444;">SEARCHING</span> (<span id="val-lat">--ms</span>)</div>
      </div>
    </div>

    <div id="f1-wheel">
      <div class="rpm-leds" id="rpm-lights"></div>
      
      <div id="f1-lcd">
        <div class="lcd-top">
          <div>HULL: <span id="val-hull" style="color:#22c55e;">100%</span></div>
          <div>REACT: <span id="val-reaction">---</span></div>
        </div>
        
        <div class="lcd-center">
          <div class="gear-box">
            <div class="gear-label">GEAR</div>
            <div id="val-gear" class="gear-val">N</div>
          </div>
          <div class="speed-box">
            <div id="val-speed" class="speed-val">0</div>
            <div class="speed-unit">KM/H (MAX <span id="val-max-speed">400</span>)</div>
          </div>
        </div>

        <div class="lcd-bottom">
          <div>STR <span id="val-roll">0.0°</span></div>
          <div>THR <span id="val-pitch">0.0°</span></div>
          <div>COMBO <span id="val-combo">x1</span></div>
        </div>
      </div>
    </div>
  </div>

  <script>
    const rpmContainer = document.getElementById('rpm-lights');
    const ledColors = ['#22c55e','#22c55e','#22c55e','#22c55e','#22c55e', '#eab308','#eab308','#eab308','#eab308','#eab308', '#ef4444','#ef4444','#ef4444','#ef4444', '#3b82f6'];
    ledColors.forEach(color => {
      const led = document.createElement('div'); led.className = 'led'; led.style.background = color; led.dataset.color = color;
      rpmContainer.appendChild(led);
    });
    const leds = document.querySelectorAll('.led');

    const valSpeedEl = document.getElementById('val-speed');
    const valGearEl = document.getElementById('val-gear');
    const valRollEl = document.getElementById('val-roll');
    const valPitchEl = document.getElementById('val-pitch');
    const valScoreEl = document.getElementById('val-score');
    const valLapEl = document.getElementById('val-lap');
    const valLapDistEl = document.getElementById('val-lap-dist');
    const valLapTargetEl = document.getElementById('val-lap-target');
    const valLapTimeEl = document.getElementById('val-lap-time');
    const valComboEl = document.getElementById('val-combo');
    const valHullEl = document.getElementById('val-hull');
    const valLinkEl = document.getElementById('val-link');
    const valLatEl = document.getElementById('val-lat');
    const valReactionEl = document.getElementById('val-reaction');
    const valChancesEl = document.getElementById('val-chances');
    const valMaxSpeedEl = document.getElementById('val-max-speed');

    /* =========================================================================
       1. SYNTHESIZED F1 ENGINE AUDIO & ZERO-LAG DEEP MALE VOICE ANNOUNCER
       ========================================================================= */
    let actx = null, audioUnlocked = false;
    let engineOsc = null, engineGain = null;
    let cachedCrashBuf = null;
    let cachedMaleVoice = null;

    function initMaleVoice() {
      try {
        if (!('speechSynthesis' in window)) return;
        const voices = window.speechSynthesis.getVoices();
        if (!voices || !voices.length) return;
        const enVoices = voices.filter(v => v.lang && v.lang.toLowerCase().startsWith('en'));
        const pool = enVoices.length ? enVoices : voices;
        cachedMaleVoice = pool.find(v =>
          /male|david|mark|daniel|george|james|alex|fred|guy|arthur|oliver|thomas|reed|rishi/i.test(v.name) &&
          !/female|zira|hazel|susan|samantha|victoria|karen|moira|tessa/i.test(v.name)
        ) || pool.find(v => !/female|zira|hazel|susan|samantha|victoria|karen|moira|tessa/i.test(v.name)) || pool[0];
      } catch(e){}
    }
    if ('speechSynthesis' in window) {
      initMaleVoice();
      window.speechSynthesis.onvoiceschanged = initMaleVoice;
    }

    function speakMaleVoice(text) {
      setTimeout(() => {
        try {
          if (!('speechSynthesis' in window)) return;
          if (!cachedMaleVoice) initMaleVoice();
          window.speechSynthesis.cancel();
          const u = new SpeechSynthesisUtterance(text);
          if (cachedMaleVoice) u.voice = cachedMaleVoice;
          u.pitch = 0.75;
          u.rate = 1.06;
          u.volume = 1.0;
          window.speechSynthesis.speak(u);
        } catch(e){}
      }, 0);
    }

    function unlockAudio() {
      try {
        if (!actx) actx = new (window.AudioContext || window.webkitAudioContext)();
        if (actx.state === 'suspended') actx.resume();
        
        if (!engineOsc) {
          engineOsc = actx.createOscillator(); engineGain = actx.createGain();
          engineOsc.type = 'sawtooth'; engineOsc.frequency.value = 50; engineGain.gain.value = 0; 
          const filter = actx.createBiquadFilter(); filter.type = 'lowpass'; filter.frequency.value = 600;
          engineOsc.connect(filter); filter.connect(engineGain); engineGain.connect(actx.destination);
          engineOsc.start();
        }
        if (!cachedCrashBuf) {
          const dur = 0.6;
          cachedCrashBuf = actx.createBuffer(1, Math.floor(actx.sampleRate * dur), actx.sampleRate);
          const d = cachedCrashBuf.getChannelData(0);
          for (let i = 0; i < d.length; i++) {
            d[i] = (Math.random() * 2 - 1) * Math.exp(-i / (actx.sampleRate * 0.15));
          }
        }
        audioUnlocked = true;
      } catch(e){}
    }

    document.getElementById('start-btn').addEventListener('click', () => {
      document.getElementById('start-overlay').style.display = 'none';
      lastFrame = performance.now();
      isGameOver = false;
      setTimeout(() => {
        unlockAudio();
        speakMaleVoice("Lights out and away we go");
      }, 10);
    });

    function playCrash() {
      if (!actx || !audioUnlocked || !cachedCrashBuf) return;
      try {
        const src = actx.createBufferSource(); src.buffer = cachedCrashBuf;
        const lp = actx.createBiquadFilter(); lp.type = 'lowpass'; lp.frequency.setValueAtTime(400, actx.currentTime);
        const g = actx.createGain(); g.gain.setValueAtTime(1.4, actx.currentTime); g.gain.exponentialRampToValueAtTime(0.01, actx.currentTime + 0.55);
        src.connect(lp); lp.connect(g); g.connect(actx.destination); src.start();
      } catch(e){}
    }

    function playLevelUp() {
      if (!actx || !audioUnlocked) return;
      try {
        const osc = actx.createOscillator(); const g = actx.createGain();
        osc.type = 'sine'; osc.frequency.setValueAtTime(440, actx.currentTime); osc.frequency.exponentialRampToValueAtTime(1200, actx.currentTime + 0.5);
        g.gain.setValueAtTime(0.4, actx.currentTime); g.gain.exponentialRampToValueAtTime(0.01, actx.currentTime + 0.6);
        osc.connect(g); g.connect(actx.destination); osc.start(); osc.stop(actx.currentTime + 0.6);
      } catch(e){}
    }

    /* =========================================================================
       2. F1 RACING ENGINE: 40-15° TOP SPEED & +/-30° RELATIVE DIRECTION STEERING
       ========================================================================= */
    const canvas = document.getElementById('canvas');
    const ctx = canvas.getContext('2d');
    let W = canvas.width = window.innerWidth;
    let H = canvas.height = window.innerHeight;
    let HORIZON_Y = H * 0.45;
    let skyGrad = ctx.createLinearGradient(0, 0, 0, HORIZON_Y);
    skyGrad.addColorStop(0, '#1e3a8a'); skyGrad.addColorStop(1, '#60a5fa');

    window.addEventListener('resize', () => {
      W = canvas.width = window.innerWidth;
      H = canvas.height = window.innerHeight;
      HORIZON_Y = H * 0.45;
      skyGrad = ctx.createLinearGradient(0, 0, 0, HORIZON_Y);
      skyGrad.addColorStop(0, '#1e3a8a'); skyGrad.addColorStop(1, '#60a5fa');
    });

    const FOV = 350;
    const CAMERA_Y = 2.5; 
    const TRACK_PACE_SCALE = 1.42;

    function project(x, y, z) {
      if (z <= 0.1) return null;
      const scale = FOV / z;
      return { x: W / 2 + x * scale, y: HORIZON_Y + (CAMERA_Y - y) * scale, s: scale };
    }

    function getLapTargetMeters(lap) { return (11 + lap) * 1000; }
    function getLapMaxSpeed(lap) { return lap <= 4 ? 400 : Math.min(500, 400 + (lap - 4) * 20); }

    // Fixed Zero Calibration in Code (No manual adjustment)
    const CODE_ROLL_ZERO = 0.0;
    const CODE_PITCH_ZERO = 0.0;

    let carX = 0, targetX = 0;
    let speedKph = 0, targetSpeed = 0; 
    let rollDeg = 0, pitchDeg = 0;
    let controllerActive = false;
    let steerAnchor = 0, steerMin = 0, steerMax = 0, steerDir = 0, steerInitialized = false;
    
    let currentPingMs = 15;
    let isGameOver = true;
    let hull = 100, score = 0, combo = 1;
    let bestScore = parseInt(localStorage.getItem('f1BestScore')) || 0;

    let currentLap = 1, lapAttemptCount = 1;
    let maxSpeedKph = getLapMaxSpeed(1);
    let lapTimeRemaining = 120.0;
    let lapDistanceTarget = getLapTargetMeters(1);
    let lapDistanceCovered = 0;

    let distanceVoiceStage = 4;
    let tenSecWarningGiven = false;
    let otaPanelShown = true;

    let speedHistory = [];
    let reactionTimes = [], avgReaction = 0, f1Dodges = 0;
    let lastHitTime = 0, obstacles = [];

    function updateControllerTargets() {
      if (!controllerActive || isGameOver) return;

      if (!steerInitialized) {
        steerAnchor = rollDeg;
        steerMin = rollDeg;
        steerMax = rollDeg;
        steerInitialized = true;
      }

      // 1. STEERING: + or - 30° change in direction triggers turn without waiting to cross 0°
      if (steerDir === 0) {
        if (rollDeg < steerMin) steerMin = rollDeg;
        if (rollDeg > steerMax) steerMax = rollDeg;

        if (rollDeg - steerMin >= 30) {
          steerDir = 1;
          targetX = -4.4;
          steerAnchor = rollDeg;
        } else if (rollDeg - steerMax <= -30) {
          steerDir = -1;
          targetX = 4.4;
          steerAnchor = rollDeg;
        }
      } else if (steerDir === 1) {
        targetX = -4.4;
        if (rollDeg > steerAnchor) {
          steerAnchor = rollDeg;
        } else if (rollDeg - steerAnchor <= -30) {
          steerDir = -1;
          targetX = 4.4;
          steerAnchor = rollDeg;
        }
      } else if (steerDir === -1) {
        targetX = 4.4;
        if (rollDeg < steerAnchor) {
          steerAnchor = rollDeg;
        } else if (rollDeg - steerAnchor >= 30) {
          steerDir = 1;
          targetX = -4.4;
          steerAnchor = rollDeg;
        }
      }

      // 2. THROTTLE: 40° to 15° maps to 0-Top Speed. Under 15° maintains Top Speed.
      if (pitchDeg <= 15) {
        targetSpeed = maxSpeedKph;
      } else if (pitchDeg <= 40) {
        let throttlePct = (40 - pitchDeg) / 25.0; // Maps the 25° range to 0.0 - 1.0
        targetSpeed = throttlePct * maxSpeedKph;
      } else {
        targetSpeed = 0;
        // Brake when tilted far back (standing upright past 45°)
        if (pitchDeg > 45) {
          speedKph = Math.max(0, speedKph - 200 * 0.05);
        }
      }
    }

    function spawnBarricade() {
      let maxZ = 0;
      for (let i = 0; i < obstacles.length; i++) {
        if (obstacles[i].z > maxZ) maxZ = obstacles[i].z;
      }
      if (maxZ < 400) {
        let spawnZ = maxZ + 60; 
        let isTargeting = Math.random() > 0.3; 
        let ox = isTargeting ? carX : (Math.random() * 8 - 4);
        let triggerDist = Math.random() * 50 + 40; 
        obstacles.push({ z: spawnZ, x: ox, w: 2.5, h: 1.2, isThreat: false, dodged: false, triggerDist: triggerDist });
      }
    }
    for (let i = 0; i < 8; i++) spawnBarricade();

    function resetGame() {
      speedKph = 0; hull = 100; combo = 1; targetX = 0; carX = 0; targetSpeed = 0;
      steerDir = 0; steerInitialized = false;
      reactionTimes = []; avgReaction = 0; f1Dodges = 0; lastHitTime = 0;
      
      maxSpeedKph = getLapMaxSpeed(currentLap);
      lapDistanceTarget = getLapTargetMeters(currentLap);
      lapTimeRemaining = 120.0;
      lapDistanceCovered = 0;
      distanceVoiceStage = 4;
      tenSecWarningGiven = false;
      speedHistory = [];

      valChancesEl.innerText = `∞ (#${lapAttemptCount})`;
      valMaxSpeedEl.innerText = maxSpeedKph;

      obstacles = []; for (let i = 0; i < 8; i++) spawnBarricade();
      
      document.getElementById('game-over-overlay').style.display = 'none';
      valReactionEl.innerText = "---";
      lastFrame = performance.now();
      isGameOver = false;
      updateControllerTargets();
    }

    document.getElementById('resume-btn').addEventListener('click', resetGame);

    function evaluateReaction(ms) {
      reactionTimes.push(ms);
      if (reactionTimes.length > 10) reactionTimes.shift(); 
      avgReaction = Math.round(reactionTimes.reduce((a,b) => a+b, 0) / reactionTimes.length);
      valReactionEl.innerText = avgReaction + " ms";
    }

    function drawWaveGraph() {
      const wCanvas = document.getElementById('wave-canvas');
      const wCtx = wCanvas.getContext('2d');
      const cw = wCanvas.width;
      const ch = wCanvas.height;

      wCtx.clearRect(0, 0, cw, ch);

      wCtx.strokeStyle = '#1e293b';
      wCtx.lineWidth = 1;
      wCtx.beginPath();
      wCtx.moveTo(0, ch * 0.25); wCtx.lineTo(cw, ch * 0.25);
      wCtx.moveTo(0, ch * 0.50); wCtx.lineTo(cw, ch * 0.50);
      wCtx.moveTo(0, ch * 0.75); wCtx.lineTo(cw, ch * 0.75);
      wCtx.stroke();

      if (speedHistory.length < 2) return;

      wCtx.strokeStyle = '#38bdf8';
      wCtx.lineWidth = 2.5;
      wCtx.beginPath();

      for (let i = 0; i < speedHistory.length; i++) {
        const pt = speedHistory[i];
        const px = (pt.time / 120.0) * cw;
        const py = ch - (pt.speed / 500.0) * (ch - 10) - 5;
        if (i === 0) wCtx.moveTo(px, py);
        else wCtx.lineTo(px, py);
      }
      wCtx.stroke();

      wCtx.lineTo(cw, ch);
      wCtx.lineTo(0, ch);
      wCtx.closePath();
      const waveGrad = wCtx.createLinearGradient(0, 0, 0, ch);
      waveGrad.addColorStop(0, 'rgba(56, 189, 248, 0.35)');
      waveGrad.addColorStop(1, 'rgba(56, 189, 248, 0.0)');
      wCtx.fillStyle = waveGrad;
      wCtx.fill();
    }

    function handleFailure(reason) {
      isGameOver = true;
      if (engineGain) engineGain.gain.value = 0;
      if (score > bestScore) { bestScore = score; localStorage.setItem('f1BestScore', bestScore); }
      
      const coveredKm = lapDistanceCovered / 1000;
      const targetKm = lapDistanceTarget / 1000;

      document.getElementById('go-title').innerText = reason;
      document.getElementById('go-subtitle').innerText = `RETRY LAP ${currentLap} (ATTEMPT #${lapAttemptCount})`;
      document.getElementById('resume-btn').innerText = `RETRY LAP ${currentLap}`;

      document.getElementById('rep-lap-record').innerText = `LAP ${currentLap} (TRY #${lapAttemptCount})`;
      document.getElementById('rep-dist-summary').innerText = `${coveredKm.toFixed(2)} / ${targetKm.toFixed(1)} km`;

      lapAttemptCount++;
      document.getElementById('go-score').innerText = String(score).padStart(8, '0');
      document.getElementById('go-best').innerText = String(bestScore).padStart(8, '0');
      document.getElementById('game-over-overlay').style.display = 'flex';

      setTimeout(drawWaveGraph, 50);
    }

    /* =========================================================================
       3. WEBSOCKET CONTROLLER & LAP 1+ 60-SECOND OTA PROGRESS BAR
       ========================================================================= */
    const host = window.location.hostname || "192.168.4.1";
    let ws = null, pingTs = 0, reconnectTimeout = null;
    let isDownloadingOta = false;
    let isRebootingOta = false;
    let otaWentOffline = false;
    let currentOtaPct = 0;
    let simProgressTimer = null;

    const otaPanel = document.getElementById('ota-panel');
    const otaLog = document.getElementById('ota-log');
    const otaBarWrap = document.getElementById('ota-bar-wrap');
    const otaBar = document.getElementById('ota-bar');
    const otaBarPct = document.getElementById('ota-bar-pct');
    const fwBtn = document.getElementById('fw-update-btn');
    const reloadBtn = document.getElementById('reload-btn');

    function setOtaProgress(pct) {
      currentOtaPct = Math.max(currentOtaPct, Math.min(100, Math.round(pct)));
      otaBarWrap.style.display = "block";
      otaBar.style.width = currentOtaPct + "%";
      otaBarPct.innerText = currentOtaPct + "%";
    }

    // Gradually loads from 0% to 100% in 60 seconds (1% every 600ms)
    function start60SecondProgress() {
      currentOtaPct = 0;
      otaBarWrap.style.display = "block";
      otaLog.style.display = "block";
      otaBar.style.background = "linear-gradient(90deg, #2563eb, #38bdf8)";
      otaLog.style.color = "#facc15";
      otaLog.innerText = "if internet is available update will be live...";
      setOtaProgress(1);

      if (simProgressTimer) clearInterval(simProgressTimer);
      simProgressTimer = setInterval(() => {
        if (!isDownloadingOta && !isRebootingOta) {
          clearInterval(simProgressTimer);
          return;
        }
        if (currentOtaPct < 100) {
          setOtaProgress(currentOtaPct + 1);
          otaLog.innerText = "if internet is available update will be live...";
        } else {
          clearInterval(simProgressTimer);
          if (valLinkEl.innerText === "ONLINE") {
            completeOtaAndReload();
          }
        }
      }, 600);
    }

    function completeOtaAndReload() {
      if (simProgressTimer) clearInterval(simProgressTimer);
      isDownloadingOta = false;
      isRebootingOta = false;
      otaWentOffline = false;

      setOtaProgress(100);
      otaBar.style.background = "#22c55e";
      otaLog.style.color = "#22c55e";
      otaLog.innerText = "UPDATE SUCCESSFUL! Auto-reloading page now...";
      fwBtn.style.display = "none";
      reloadBtn.style.display = "block";

      setTimeout(() => {
        window.location.replace("http://192.168.4.1/?updated=" + Date.now());
      }, 1200);
    }

    function connectWs() {
      if (reconnectTimeout) clearTimeout(reconnectTimeout);
      if (ws) {
        try { ws.onclose = null; ws.close(); } catch(e){}
      }
      ws = new WebSocket("ws://" + host + ":81/");
      
      ws.onopen = () => {
        valLinkEl.innerText = "ONLINE";
        valLinkEl.style.color = "#22c55e";
        if ((isRebootingOta || (isDownloadingOta && otaWentOffline)) && currentOtaPct >= 100) {
          completeOtaAndReload();
        }
      };
      ws.onclose = () => {
        valLinkEl.innerText = "OFFLINE";
        valLinkEl.style.color = "#ef4444";
        if (isDownloadingOta || isRebootingOta) {
          otaWentOffline = true;
          otaLog.innerText = "if internet is available update will be live...";
        }
        reconnectTimeout = setTimeout(connectWs, 800);
      };
      ws.onerror = () => { try { ws.close(); } catch(e){} };

      ws.onmessage = (e) => {
        const data = e.data;

        if (data.charCodeAt(0) === 80) { // 'P'
          currentPingMs = Math.max(0, Math.min(250, Date.now() - pingTs));
          valLatEl.innerText = currentPingMs + "ms";
          return;
        }

        if (data.startsWith("OTA:")) {
          const parts = data.split(':');
          const type = parts[1];
          if (type === "DONE") {
            isRebootingOta = true;
            otaWentOffline = true;
            otaLog.innerText = "if internet is available update will be live...";
          } else if (type === "ERR") {
            if (simProgressTimer) clearInterval(simProgressTimer);
            isDownloadingOta = false;
            isRebootingOta = false;
            otaWentOffline = false;
            otaBar.style.background = "#ef4444";
            otaLog.style.color = "#ef4444";
            otaLog.innerText = "Restored previous firmware: " + parts.slice(2).join(':');
            fwBtn.disabled = false;
            fwBtn.innerText = "SYSTEM UPDATE";
          }
          return;
        }

        const c1 = data.indexOf(',');
        if (c1 !== -1) {
          const c2 = data.indexOf(',', c1 + 1);
          controllerActive = true;
          const rawRoll = parseFloat(data.substring(0, c1)) || 0;
          const rawPitch = parseFloat(c2 !== -1 ? data.substring(c1 + 1, c2) : data.substring(c1 + 1)) || 0;

          rollDeg = rawRoll - CODE_ROLL_ZERO;
          pitchDeg = rawPitch - CODE_PITCH_ZERO;

          updateControllerTargets();
        }
      };
    }
    connectWs();
    setInterval(() => {
      if (!isDownloadingOta && !isRebootingOta && ws && ws.readyState === WebSocket.OPEN) {
        pingTs = Date.now();
        ws.send("p" + pingTs);
      }
    }, 500);

    fwBtn.addEventListener('click', () => {
      fwBtn.disabled = true;
      fwBtn.innerText = "UPDATING...";
      isDownloadingOta = true;
      isRebootingOta = false;
      otaWentOffline = false;
      start60SecondProgress();

      fetch('/firmware-update')
        .then(r => r.text())
        .then(() => {
          otaLog.innerText = "if internet is available update will be live...";
        })
        .catch(() => {
          otaWentOffline = true;
          otaLog.innerText = "if internet is available update will be live...";
        });
    });

    window.addEventListener('keydown', (e) => {
      if (!isGameOver) {
        controllerActive = false;
        if (e.key === 'ArrowLeft' || e.key === 'a') targetX = -4.4;
        if (e.key === 'ArrowRight' || e.key === 'd') targetX = 4.4;
        if (e.key === 'ArrowUp' || e.key === 'w') targetSpeed = Math.min(maxSpeedKph, targetSpeed + 80);
        if (e.key === 'ArrowDown' || e.key === 's') { targetSpeed = 0; speedKph = Math.max(0, speedKph - 25); }
      }
    });

    /* =========================================================================
       4. TRACK RENDERING & INSTANT RECOVERY ENGINE
       ========================================================================= */
    const damageFlash = document.getElementById('damage-flash');

    function triggerDamage(now) {
      if (now - lastHitTime < 600) return; 
      lastHitTime = now;
      hull = Math.max(0, hull - 10);
      combo = 1;
      speedKph *= 0.45;
      updateControllerTargets();
      
      damageFlash.style.opacity = '1'; setTimeout(() => damageFlash.style.opacity = '0', 160);
      playCrash();

      if (hull <= 0 && !isGameOver) handleFailure("HULL BREACH");
    }

    function drawF1Car(screenX, screenY, scale) {
      ctx.save(); ctx.translate(screenX, screenY);
      ctx.fillStyle = 'rgba(0,0,0,0.6)'; ctx.beginPath(); ctx.ellipse(0, 5, 80 * scale, 20 * scale, 0, 0, Math.PI*2); ctx.fill();
      ctx.fillStyle = '#111'; ctx.fillRect(-45*scale, -35*scale, 90*scale, 10*scale);
      ctx.fillStyle = '#ef4444'; ctx.fillRect(-40*scale, -35*scale, 80*scale, 8*scale);
      ctx.fillStyle = '#000';
      ctx.beginPath(); ctx.roundRect ? ctx.roundRect(-55*scale, -20*scale, 25*scale, 35*scale, 4) : ctx.rect(-55*scale, -20*scale, 25*scale, 35*scale); ctx.fill();
      ctx.beginPath(); ctx.roundRect ? ctx.roundRect(30*scale, -20*scale, 25*scale, 35*scale, 4) : ctx.rect(30*scale, -20*scale, 25*scale, 35*scale); ctx.fill();
      ctx.fillStyle = '#dc2626'; ctx.beginPath(); ctx.moveTo(-15*scale, -30*scale); ctx.lineTo(15*scale, -30*scale); ctx.lineTo(25*scale, 10*scale); ctx.lineTo(15*scale, 40*scale); ctx.lineTo(-15*scale, 40*scale); ctx.lineTo(-25*scale, 10*scale); ctx.fill();
      ctx.fillStyle = '#111'; ctx.beginPath(); ctx.ellipse(0, -5*scale, 12*scale, 18*scale, 0, 0, Math.PI*2); ctx.fill();
      ctx.fillStyle = '#facc15'; ctx.beginPath(); ctx.arc(0, -8*scale, 8*scale, 0, Math.PI*2); ctx.fill();
      ctx.fillStyle = '#000'; ctx.beginPath(); ctx.roundRect ? ctx.roundRect(-45*scale, 20*scale, 18*scale, 25*scale, 4) : ctx.rect(-45*scale, 20*scale, 18*scale, 25*scale); ctx.fill();
      ctx.beginPath(); ctx.roundRect ? ctx.roundRect(27*scale, 20*scale, 18*scale, 25*scale, 4) : ctx.rect(27*scale, 20*scale, 18*scale, 25*scale); ctx.fill();
      ctx.fillStyle = '#111'; ctx.fillRect(-45*scale, 40*scale, 90*scale, 8*scale);
      ctx.fillStyle = '#fff'; ctx.fillRect(-35*scale, 40*scale, 70*scale, 4*scale);
      ctx.restore();
    }

    let lastFrame = performance.now();
    let speedRecordTimer = 0;
    let hudRefreshTimer = 0;
    let lastActiveLeds = -1;

    function gameLoop(now) {
      const dt = Math.min((now - lastFrame) / 1000, 0.05);
      lastFrame = now;

      if (!isGameOver) {
        updateControllerTargets();

        const steerAlpha = 1 - Math.exp(-30 * dt);
        const speedAlpha = (targetSpeed >= speedKph) ? (1 - Math.exp(-12.0 * dt)) : (1 - Math.exp(-7.0 * dt));

        carX += (targetX - carX) * steerAlpha;
        speedKph += (targetSpeed - speedKph) * speedAlpha;
        if (speedKph > maxSpeedKph) speedKph = maxSpeedKph;
        if (speedKph < 1) speedKph = 0;

        const elapsedSec = 120.0 - lapTimeRemaining;

        speedRecordTimer += dt;
        if (speedRecordTimer >= 0.5) {
          speedRecordTimer = 0;
          speedHistory.push({ time: elapsedSec, speed: speedKph });
        }

        let speedMs = (speedKph / 3.6) * TRACK_PACE_SCALE;
        let frameDist = speedMs * dt;
        
        lapDistanceCovered += frameDist;
        lapTimeRemaining -= dt;
        score += Math.round(frameDist * combo);

        const metersRemaining = lapDistanceTarget - lapDistanceCovered;
        if (distanceVoiceStage === 4 && metersRemaining <= 3000 && metersRemaining > 2000) {
          distanceVoiceStage = 3;
          speakMaleVoice("3 kilometers remaining");
        } else if (distanceVoiceStage === 3 && metersRemaining <= 2000 && metersRemaining > 1000) {
          distanceVoiceStage = 2;
          speakMaleVoice("2 kilometers remaining");
        } else if (distanceVoiceStage === 2 && metersRemaining <= 1000 && metersRemaining > 500) {
          distanceVoiceStage = 1;
          speakMaleVoice("1 kilometer remaining");
        } else if (distanceVoiceStage === 1 && metersRemaining <= 500 && metersRemaining > 0) {
          distanceVoiceStage = 0;
          speakMaleVoice("0.5 kilometers, final push");
        }

        if (!tenSecWarningGiven && lapTimeRemaining <= 10.0 && lapTimeRemaining > 0 && metersRemaining > 150) {
          tenSecWarningGiven = true;
          speakMaleVoice("10 seconds remaining, push now");
        }

        if (now - lastHitTime > 2000 && hull < 100) hull = Math.min(100, hull + 10 * dt); 

        if (lapTimeRemaining <= 0) handleFailure("TIME OUT");

        if (lapDistanceCovered >= lapDistanceTarget && !isGameOver) {
            const completedLap = currentLap;
            currentLap++;
            lapAttemptCount = 1;
            lapDistanceCovered = 0;
            lapTimeRemaining = 120.0;
            distanceVoiceStage = 4;
            tenSecWarningGiven = false;
            maxSpeedKph = getLapMaxSpeed(currentLap);
            lapDistanceTarget = getLapTargetMeters(currentLap);
            speedHistory = [];
            
            // Reveal update button from Lap 1
            if (currentLap >= 1 && !otaPanelShown) {
              otaPanelShown = true;
              otaPanel.style.display = 'block';
            }

            valChancesEl.innerText = `∞ (#${lapAttemptCount})`;
            valMaxSpeedEl.innerText = maxSpeedKph;
            document.getElementById('toast-lvl').innerText = completedLap;
            document.getElementById('toast-target').innerText = (lapDistanceTarget / 1000).toFixed(1);
            document.getElementById('toast-maxspd').innerText = maxSpeedKph;
            
            const toast = document.getElementById('level-up-overlay');
            toast.style.display = 'block'; playLevelUp();
            setTimeout(() => { toast.style.display = 'none'; }, 2500);

            speakMaleVoice("Lap " + completedLap + " completed. Systems restored.");
        }

        valSpeedEl.innerText = Math.round(speedKph);
        let gear = speedKph < 2 ? "N" : Math.min(8, Math.ceil((speedKph / maxSpeedKph) * 8));
        valGearEl.innerText = gear;

        hudRefreshTimer += dt;
        if (hudRefreshTimer >= 0.05) {
          hudRefreshTimer = 0;
          valPitchEl.innerText = pitchDeg.toFixed(1) + "°";
          valRollEl.innerText = rollDeg.toFixed(1) + "°";
          valScoreEl.innerText = String(score).padStart(8, '0');
          valLapEl.innerText = currentLap;
          valLapDistEl.innerText = (lapDistanceCovered / 1000).toFixed(2);
          valLapTargetEl.innerText = (lapDistanceTarget / 1000).toFixed(1);
          
          valLapTimeEl.innerText = Math.ceil(lapTimeRemaining) + "s";
          valLapTimeEl.style.color = lapTimeRemaining < 30 ? '#ef4444' : (lapTimeRemaining < 60 ? '#eab308' : '#22c55e');

          valComboEl.innerText = 'x' + combo;
          
          valHullEl.innerText = Math.round(hull) + '%';
          valHullEl.style.color = hull < 40 ? '#ef4444' : (hull < 75 ? '#eab308' : '#22c55e');
        }

        let activeLeds = Math.floor((speedKph / maxSpeedKph) * 15);
        if (activeLeds !== lastActiveLeds) {
          lastActiveLeds = activeLeds;
          for (let i = 0; i < leds.length; i++) {
            if (i < activeLeds) {
              leds[i].style.opacity = '1';
              leds[i].style.boxShadow = `0 0 8px ${leds[i].dataset.color}`;
            } else {
              leds[i].style.opacity = '0.2';
              leds[i].style.boxShadow = 'none';
            }
          }
        }

        if (engineGain) {
          engineOsc.frequency.value = 50 + (speedKph * 1.2);
          engineGain.gain.value = speedKph > 5 ? 0.3 : 0.05;
        }
      } else {
        if (engineGain) engineGain.gain.value = 0;
      }

      ctx.fillStyle = skyGrad; ctx.fillRect(0, 0, W, HORIZON_Y);
      ctx.fillStyle = '#166534'; ctx.fillRect(0, HORIZON_Y, W, H - HORIZON_Y);

      let drawDist = 300, segLength = 10, speedMs = (speedKph / 3.6) * TRACK_PACE_SCALE;

      for (let z = drawDist; z > 0; z -= segLength) {
        let zNear = z - (score/10 % segLength); 
        let zFar = zNear + segLength;
        if (zNear < 0.1) continue;

        let pNearL = project(-5 - carX, 0, zNear);
        let pNearR = project(5 - carX, 0, zNear);
        let pFarL = project(-5 - carX, 0, zFar);
        let pFarR = project(5 - carX, 0, zFar);

        if (pNearL && pFarL) {
          let isDark = Math.floor((score/10 + zNear) / segLength) % 2 === 0;

          ctx.fillStyle = isDark ? '#334155' : '#475569';
          ctx.beginPath(); ctx.moveTo(pFarL.x, pFarL.y); ctx.lineTo(pFarR.x, pFarR.y); ctx.lineTo(pNearR.x, pNearR.y); ctx.lineTo(pNearL.x, pNearL.y); ctx.fill();

          ctx.fillStyle = isDark ? '#ef4444' : '#ffffff';
          ctx.beginPath(); ctx.moveTo(pFarL.x - 0.5*pFarL.s, pFarL.y); ctx.lineTo(pFarL.x, pFarL.y); ctx.lineTo(pNearL.x, pNearL.y); ctx.lineTo(pNearL.x - 0.5*pNearL.s, pNearL.y); ctx.fill();
          ctx.beginPath(); ctx.moveTo(pFarR.x, pFarR.y); ctx.lineTo(pFarR.x + 0.5*pFarR.s, pFarR.y); ctx.lineTo(pNearR.x + 0.5*pNearR.s, pNearR.y); ctx.lineTo(pNearR.x, pNearR.y); ctx.fill();
        }
      }

      spawnBarricade();

      obstacles.sort((a,b) => b.z - a.z); 
      for (let i = 0; i < obstacles.length; i++) {
        const o = obstacles[i];
        if (!isGameOver) o.z -= speedMs * dt;

        if (o.z < o.triggerDist && o.z > 5 && !o.dodged && !isGameOver) {
          let collisionCourse = Math.abs(targetX - o.x) < (o.w/2 + 0.8);
          if (collisionCourse && !o.isThreat) {
            o.isThreat = true; o.threatTime = performance.now();
          } else if (!collisionCourse && o.isThreat) {
            o.dodged = true;
            let rawRt = performance.now() - o.threatTime - (currentPingMs * 0.5);
            let rt = Math.max(85, Math.round(rawRt));
            if (speedKph >= 150) { evaluateReaction(rt); if (rt <= 200) f1Dodges++; }
            score += 500; combo = Math.min(9, combo + 1);
          }
        }

        if (o.z < 2 && o.z > -5 && !isGameOver) {
          if (Math.abs(carX - o.x) < (o.w/2 + 0.7)) triggerDamage(now);
        }

        let pTopL = project(o.x - o.w/2 - carX, o.h, o.z);
        let pBotR = project(o.x + o.w/2 - carX, 0, o.z);
        if (pTopL && pBotR) {
          let bw = pBotR.x - pTopL.x; let bh = pBotR.y - pTopL.y;
          
          ctx.fillStyle = o.isThreat ? '#fef08a' : '#2563eb'; ctx.fillRect(pTopL.x, pTopL.y, bw, bh);
          ctx.strokeStyle = '#000'; ctx.lineWidth = 2; ctx.strokeRect(pTopL.x, pTopL.y, bw, bh);
          ctx.fillStyle = '#000'; ctx.fillRect(pTopL.x, pTopL.y + bh*0.2, bw, bh*0.2); ctx.fillRect(pTopL.x, pTopL.y + bh*0.6, bw, bh*0.2);

          let textAlpha = Math.max(0, Math.min(1, (o.z - 20) / 20));
          if (textAlpha > 0.01) {
              ctx.save(); ctx.globalAlpha = textAlpha;
              let labelStr = ((lapDistanceTarget - lapDistanceCovered) / 1000).toFixed(1) + "km : " + Math.ceil(lapTimeRemaining) + "s";
              let fontSize = Math.max(10, Math.min(28, 22 * pTopL.s));
              ctx.font = `bold ${fontSize}px 'Courier New', monospace`; ctx.textAlign = "center";
              ctx.lineWidth = 2; ctx.strokeStyle = '#000'; ctx.strokeText(labelStr, pTopL.x + bw/2, pTopL.y - 12);
              ctx.fillStyle = o.isThreat ? '#ef4444' : '#fff'; ctx.fillText(labelStr, pTopL.x + bw/2, pTopL.y - 12);
              ctx.restore();
          }
        }
      }

      for (let i = obstacles.length - 1; i >= 0; i--) {
        if (obstacles[i].z <= -10) obstacles.splice(i, 1);
      }

      drawF1Car(W/2, H*0.8, Math.min(1.5, W/800));
      requestAnimationFrame(gameLoop);
    }
    requestAnimationFrame(gameLoop);
  </script>
</body>
</html>
)=====";

bool readMPU(float &ax, float &ay, float &az, float &gx, float &gy, float &gz) {
  Wire.beginTransmission(MPU);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return false;
  
  if (Wire.requestFrom((int)MPU, 14) == 14) {
    int16_t raw[7];
    for (int i = 0; i < 7; i++) raw[i] = (Wire.read() << 8) | Wire.read();
    ax = raw[0] / 16384.0; ay = raw[1] / 16384.0; az = raw[2] / 16384.0;
    gx = raw[4] / 131.0;   gy = raw[5] / 131.0;   gz = raw[6] / 131.0;
    return true;
  }
  return false;
}

void calibrateMPU() {
  float ax, ay, az, gx, gy, gz, sx = 0, sy = 0, sz = 0;
  int valid = 0;
  for (int i = 0; i < 50; i++) {
    if (readMPU(ax, ay, az, gx, gy, gz)) {
      sx += gx; sy += gy; sz += gz;
      valid++;
    }
    delay(4);
    yield();
  }
  if (valid > 0) {
    gxOff = sx / valid; gyOff = sy / valid; gzOff = sz / valid;
    rollZero  = FIXED_ROLL_ZERO;
    pitchZero = FIXED_PITCH_ZERO;
    roll = 0; pitch = 0;
  }
}

void onWs(uint8_t num, WStype_t type, uint8_t* payload, size_t len) {
  if (type == WStype_CONNECTED) {
    if (pendingOtaError.length() > 0) {
      ws.sendTXT(num, pendingOtaError);
      pendingOtaError = "";
    }
  } else if (type == WStype_TEXT && len > 0) {
    if (payload[0] == 'p') {
      payload[0] = 'P';
      ws.sendTXT(num, payload, len);
    }
  }
}

void startAccessPoint() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  WiFi.setOutputPower(20.5);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  IPAddress apIP(192, 168, 4, 1);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(SSID, PASS, 6, 0, 4);

  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", apIP);
}

// Fail-Safe Dual-Bank OTA Update
void runOtaUpdate() {
  pendingOtaError = "";
  ws.broadcastTXT("OTA:START:if internet is available update will be live...");
  for (int i = 0; i < 5; i++) { ws.loop(); delay(20); }

  if (ESP.getFreeSketchSpace() < 250000) {
    pendingOtaError = "OTA:ERR:Insufficient secondary flash space for safe dual-bank rollback.";
    return;
  }

  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(STA_SSID, STA_PASS);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 20000) {
    delay(250);
    yield();
  }

  if (WiFi.status() != WL_CONNECTED) {
    pendingOtaError = "OTA:ERR:Hotspot '" + String(STA_SSID) + "' unreachable. Running existing firmware.";
    startAccessPoint();
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setBufferSizes(16384, 512);
  client.setTimeout(12000);

  ESPhttpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  ESPhttpUpdate.setClientTimeout(12000);
  ESPhttpUpdate.rebootOnUpdate(false);

  t_httpUpdate_return ret = ESPhttpUpdate.update(client, FIRMWARE_URL);

  if (ret == HTTP_UPDATE_OK && !Update.hasError()) {
    delay(300);
    ESP.restart();
  } else {
    Update.end(false);
    pendingOtaError = "OTA:ERR:Interrupted (" + ESPhttpUpdate.getLastErrorString() + "). Fell back to old firmware.";
    startAccessPoint();
  }
}

void serveGamePage() {
  http.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  http.sendHeader("Pragma", "no-cache");
  http.sendHeader("Expires", "-1");
  http.sendHeader("Connection", "close");
  http.send_P(200, "text/html", GAME_HTML);
}

void send204Validation() {
  http.sendHeader("Connection", "close");
  http.send(204, "text/plain", "");
}

void sendAppleSuccess() {
  http.sendHeader("Connection", "close");
  http.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
}

void setup() {
  Serial.begin(115200);
  Wire.begin(4, 5); 
  Wire.setClock(400000); 

  Wire.beginTransmission(MPU);
  if (Wire.endTransmission() == 0) {
    mpuFound = true;
    Wire.beginTransmission(MPU); Wire.write(0x6B); Wire.write(0); Wire.endTransmission();
    Wire.beginTransmission(MPU); Wire.write(0x1A); Wire.write(3); Wire.endTransmission(); 
    calibrateMPU();
    Serial.println("F1 joystick v1.0.0 MPU6050 OK");
  } else {
    Serial.println("F1 joystick v1.0.0 MPU6050 Error: Check D1/D2/VCC/GND");
  }

  WiFi.hostname("F1-joystick");
  startAccessPoint();

  http.on("/", HTTP_GET, serveGamePage);
  http.on("/generate_204", HTTP_GET, send204Validation);
  http.on("/gen_204", HTTP_GET, send204Validation);
  http.on("/favicon.ico", HTTP_GET, send204Validation);
  http.on("/hotspot-detect.html", HTTP_GET, sendAppleSuccess);
  http.on("/library/test/success.html", HTTP_GET, sendAppleSuccess);
  http.on("/ncsi.txt", HTTP_GET, []() {
    http.sendHeader("Connection", "close");
    http.send(200, "text/plain", "Microsoft NCSI");
  });
  http.on("/connecttest.txt", HTTP_GET, []() {
    http.sendHeader("Connection", "close");
    http.send(200, "text/plain", "Microsoft Connect Test");
  });
  http.onNotFound(send204Validation);

  http.on("/firmware-update", HTTP_GET, []() {
    triggerOtaUpdate = true;
    http.sendHeader("Connection", "close");
    http.send(200, "text/plain", "if internet is available update will be live...");
  });

  http.begin();
  ws.onEvent(onWs);
  ws.begin();
  lastUs = micros();
}

void loop() {
  dnsServer.processNextRequest();
  http.handleClient();
  ws.loop();
  yield(); 

  if (triggerOtaUpdate) {
    triggerOtaUpdate = false;
    delay(100);
    runOtaUpdate();
    lastUs = micros();
  }

  static unsigned long lastSend = 0;
  unsigned long currentMillis = millis();

  if (currentMillis - lastSend >= 20) {
    lastSend = currentMillis;

    float ax, ay, az, gx, gy, gz;
    if (mpuFound && readMPU(ax, ay, az, gx, gy, gz)) {
      unsigned long now = micros();
      float dt = (now - lastUs) / 1000000.0;
      lastUs = now;

      gx -= gxOff; gy -= gyOff; gz -= gzOff;

      float aRoll  = (atan2(ay, -az) * 57.2958) - rollZero;
      float aPitch = (atan2(-ax, sqrt(ay * ay + az * az)) * 57.2958) - pitchZero;

      roll  = 0.98 * (roll  + gx * dt) + 0.02 * aRoll;
      pitch = 0.98 * (pitch + gy * dt) + 0.02 * aPitch;

      if (ws.connectedClients() > 0) {
        char buf[40];
        snprintf(buf, sizeof(buf), "%.2f,%.2f,%.2f", roll, pitch, gz);
        ws.broadcastTXT(buf);
      }
    }
  }
}