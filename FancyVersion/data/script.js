const testMode = false;
let isOpen = true, hasStartedFan = false;

function toggleCurtain(btn) {
  const label = btn.querySelector('.btn-label'),
        loader = btn.querySelector('.loader'),
        fan = document.getElementById('fan'),
        curtainL = document.getElementById('curtain-left'),
        curtainR = document.getElementById('curtain-right');

  btn.classList.add('loading');
  label.style.display = 'none';
  loader.style.display = 'inline-block';
  btn.disabled = true;

  const target = isOpen ? '/close' : '/open';

  function completeCurtainToggle() {
    isOpen = !isOpen;
    label.textContent = isOpen ? 'Close Curtain' : 'Open Curtain';
    fan.classList.remove('fan-error');
    curtainL.classList.toggle('closed', !isOpen);
    curtainR.classList.toggle('closed', !isOpen);
    loader.style.display = 'none';
    label.style.display = 'inline';
    btn.classList.remove('loading');
    label.style.visibility = 'visible';
    btn.disabled = false;
  }

  if (testMode) {
    setTimeout(completeCurtainToggle, 1000);
  } else {
    fetch(target)
      .catch(() => {
        fan.classList.remove('spinning-fan');
        fan.classList.add('fan-error');
      })
      .finally(() => {
        completeCurtainToggle();
      });
  }
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

function restartESP() {
  const btn = event.currentTarget;
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
      startFanSpeedLoop(); hasStartedFan = true;
    }
  }, 3000);
});
