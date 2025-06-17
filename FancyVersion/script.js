const testMode = false;
let isOpen = null;
let hasStartedFan = false;
let curtainCooldown = false;

function toggleCurtain(btn) {
  if (curtainCooldown || isOpen === null) return; // Prevent double-press before state known
  curtainCooldown = true;

  const label = btn.querySelector('.btn-label'),
        loader = btn.querySelector('.loader'),
        fan = document.getElementById('fan'),
        curtainL = document.getElementById('curtain-left'),
        curtainR = document.getElementById('curtain-right');

  btn.classList.add('loading');
  if (label) label.style.display = 'none';
  if (loader) loader.style.display = 'inline-block';
  btn.disabled = true;

  const target = isOpen ? '/close' : '/open';

  function completeCurtainToggle() {
    isOpen = !isOpen;
    updateCurtainButtonText();
    fan.classList.remove('fan-error');
    curtainL.classList.toggle('closed', !isOpen);
    curtainR.classList.toggle('closed', !isOpen);
    if (loader) loader.style.display = 'none';
    if (label) label.style.display = 'inline';
    btn.classList.remove('loading');
    btn.disabled = false;
    curtainCooldown = false;
  }

  if (testMode) {
    setTimeout(completeCurtainToggle, 3000);
  } else {
    fetch(target)
      .catch(() => {
        fan.classList.remove('spinning-fan');
        fan.classList.add('fan-error');
      })
      .finally(() => {
        setTimeout(completeCurtainToggle, 3000);
      });
  }
}

function updateCurtainButtonText() {
  const curtainBtn = document.getElementById('curtain-toggle-btn');
  if (!curtainBtn || isOpen === null) return;  // skip early
  const label = curtainBtn.querySelector('.btn-label');
  if (label) {
    label.textContent = isOpen ? 'Close Curtain' : 'Open Curtain';
  }
}

function fetchCurtainState() {
  fetch('/curtain-status')
    .then(r => r.json())
    .then(data => {
      if (data && data.state) {
        isOpen = data.state === 'open';
        updateCurtainButtonText();

        // Match visual curtain state
        const curtainL = document.getElementById('curtain-left');
        const curtainR = document.getElementById('curtain-right');
        if (curtainL && curtainR) {
          curtainL.classList.toggle('closed', !isOpen);
          curtainR.classList.toggle('closed', !isOpen);
        }
      }
    })
    .catch(() => {
      console.warn('Failed to fetch curtain state.');
    });
}

function startFanSpeedLoop() {
  const fan = document.getElementById('fan');
  function setRandomSpeed() {
    return (Math.random() * 0.6 + 0.7).toFixed(2);
  }
  function loop() {
    const speed = setRandomSpeed();
    fan.style.setProperty('--spin-speed', `${speed}s`);
    const delay = parseFloat(speed) * (Math.floor(Math.random() * 7) + 4) * 1000;
    setTimeout(loop, delay);
  }
  fan.classList.add('spinning-fan');
  loop();
}

function restartESP(btn) {
  const label = btn.querySelector('.btn-label');
  const loader = btn.querySelector('.loader');

  if (label && loader) {
    btn.classList.add('loading');
    label.style.display = 'none';
    loader.style.display = 'inline-block';
    btn.disabled = true;
  } else {
    btn.textContent = 'Restarting...';
    btn.disabled = true;
  }

  fetch('/restart')
    .then(() => {
      if (label) {
        label.textContent = 'Restarting...';
      } else {
        btn.textContent = 'Restarting...';
      }
    })
    .catch(() => {
      if (label) {
        label.textContent = 'Failed to restart';
      } else {
        btn.textContent = 'Restart failed';
      }
    })
    .finally(() => {
      setTimeout(() => {
        if (label && loader) {
          btn.classList.remove('loading');
          loader.style.display = 'none';
          label.style.display = 'inline';
        } else {
          btn.textContent = 'Restart';
        }
        btn.disabled = false;
      }, 5000);
    });
}

window.addEventListener('DOMContentLoaded', () => {
  setTimeout(() => {
    if (!hasStartedFan) {
      startFanSpeedLoop();
      hasStartedFan = true;
    }
  }, 3000);

  fetchCurtainState(); // Fetch initial curtain state from ESP

  const restartBtn = document.getElementById('restart-btn');
  let holdTimer, triggered = false;

  if (restartBtn) {
    restartBtn.addEventListener('mousedown', () => {
      triggered = false;
      restartBtn.classList.add('holding');
      holdTimer = setTimeout(() => {
        triggered = true;
        restartESP(restartBtn);
      }, 2000);
    });

    restartBtn.addEventListener('mouseup', cancelHold);
    restartBtn.addEventListener('mouseleave', cancelHold);
    restartBtn.addEventListener('touchend', cancelHold);
    restartBtn.addEventListener('touchcancel', cancelHold);

    function cancelHold() {
      clearTimeout(holdTimer);
      restartBtn.classList.remove('holding');
      if (!triggered) {
        const fillBar = restartBtn.querySelector('.fill-bar');
        if (fillBar) fillBar.style.width = '0%';
      }
    }
  }
});
