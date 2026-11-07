// --- NEW FEATURE: TOP SPEED SELECTOR LOGIC ---
window.userSelectedTopSpeed = 450;

function getLapMaxSpeed(lap) {
  return (typeof window.userSelectedTopSpeed !== 'undefined') ? window.userSelectedTopSpeed : 450;
}

document.getElementById('speed-select').addEventListener('change', function(e) {
  window.userSelectedTopSpeed = parseInt(e.target.value);
  maxSpeedKph = window.userSelectedTopSpeed;
  document.getElementById('val-max-speed').innerText = maxSpeedKph;
});

setInterval(() => {
  if (maxSpeedKph !== window.userSelectedTopSpeed) {
     maxSpeedKph = window.userSelectedTopSpeed;
     document.getElementById('val-max-speed').innerText = maxSpeedKph;
  }
}, 50);