// app.js - WebApp para controlar ESP32
let ws = null;
let isConnected = false;
let overrideTimeout = null;
let ignoreIncomingWhileOverride = false;

const espIpInput = document.getElementById('espIp');
const btnConnect = document.getElementById('btnConnect');
const btnDisconnect = document.getElementById('btnDisconnect');
const wsStatus = document.getElementById('wsStatus');

const cityInput = document.getElementById('cityInput');
const btnSendCity = document.getElementById('btnSendCity');

const manualTempInput = document.getElementById('manualTemp');
const btnManualTemp = document.getElementById('btnManualTemp');

const btnStop = document.getElementById('btnStop');
const btnResume = document.getElementById('btnResume');

const readingEl = document.getElementById('reading');
const logsEl = document.getElementById('logs');
const overrideBanner = document.getElementById('overrideBanner');
const overrideValue = document.getElementById('overrideValue');

function log(msg){
  const time = new Date().toLocaleTimeString();
  logsEl.innerHTML = `[${time}] ${msg}<br>` + logsEl.innerHTML;
}

// Construye URL ws con IP
function wsUrl(){
  const ip = espIpInput.value.trim();
  if(!ip) return null;
  return `ws://${ip}:81/`;
}

function connectWS(){
  const url = wsUrl();
  if(!url){ alert('Ingresa IP del ESP32'); return; }
  if(ws){ ws.close(); ws = null; }
  ws = new WebSocket(url);

  ws.onopen = () => {
    isConnected = true;
    wsStatus.textContent = `Conectado a ${url}`;
    btnConnect.disabled = true;
    btnDisconnect.disabled = false;
    log('WebSocket conectado.');
  };

  ws.onmessage = (evt) => {
    // Los broadcasts del ESP vienen como JSON: {"city":"Medellin","temp":23.2,"desc":"..."}
    try {
      const data = JSON.parse(evt.data);
      if(ignoreIncomingWhileOverride){
        log('Mensaje recibido (ignorado por override): ' + evt.data);
        return;
      }
      readingEl.textContent = `${data.city} — ${data.temp} °C — ${data.desc || ''}`;
      log('Mensaje recibido: ' + evt.data);
    } catch(e){
      // Si el ESP acepta texto (por ejemplo solo ciudad) lo mostramos
      log('Mensaje texto: ' + evt.data);
      if(!ignoreIncomingWhileOverride) readingEl.textContent = evt.data;
    }
  };

  ws.onclose = () => {
    isConnected = false;
    wsStatus.textContent = 'Desconectado';
    btnConnect.disabled = false;
    btnDisconnect.disabled = true;
    log('WebSocket desconectado.');
  };

  ws.onerror = (err) => {
    log('Error WS: ' + JSON.stringify(err));
  };
}

function disconnectWS(){
  if(ws) ws.close();
  ws = null;
  isConnected = false;
  wsStatus.textContent = 'No conectado';
  btnConnect.disabled = false;
  btnDisconnect.disabled = true;
}

function sendCityWS(city){
  if(!isConnected || !ws) { alert('WebSocket no conectado'); return; }
  ws.send(city);
  log('Enviado por WS: ciudad="' + city + '"');
}

// POST helper a /update del ESP
async function postUpdate(fields){
  const ip = espIpInput.value.trim();
  if(!ip) { alert('Ingresa IP del ESP32'); return; }
  const url = `http://${ip}/update`;

  // enviar como form-urlencoded (ESP Arduino suele manejarlo bien)
  const body = new URLSearchParams();
  for(const k in fields) body.append(k, String(fields[k]));

  try {
    const resp = await fetch(url, {
      method: 'POST',
      headers: {'Content-Type':'application/x-www-form-urlencoded'},
      body: body.toString()
    });
    const text = await resp.text();
    log(`POST /update -> ${resp.status} ${resp.statusText} : ${text}`);
    return {ok: resp.ok, status: resp.status, text};
  } catch (err) {
    log('Error POST /update: ' + err);
    return {ok:false, err};
  }
}

// Acción: inyectar temperatura manual 5 segundos
async function manualTempAction(){
  const val = manualTempInput.value;
  if(val === '') { alert('Ingresa temperatura'); return; }

  const currentCity = cityInput.value || 'Manual';
  // Enviamos POST con campos que ESP pueda interpretar
  await postUpdate({city: currentCity, weather: 'MANUAL', temp: val});

  // Mostrar override en UI 5s e ignorar mensajes entrantes durante ese tiempo
  overrideValue.textContent = val;
  overrideBanner.classList.remove('hidden');
  ignoreIncomingWhileOverride = true;
  log('Override activado: ' + val + ' °C (5s)');

  if(overrideTimeout) clearTimeout(overrideTimeout);
  overrideTimeout = setTimeout(() => {
    overrideBanner.classList.add('hidden');
    ignoreIncomingWhileOverride = false;
    overrideTimeout = null;
    log('Override finalizado, volviendo al modo automático.');
  }, 5000);
}

// Stop / Resume handlers
async function sendStop(){
  await postUpdate({city: cityInput.value || 'Control', weather: 'STOP', temp: 0});
  log('Comando STOP enviado.');
}
async function sendResume(){
  await postUpdate({city: cityInput.value || 'Control', weather: 'RESUME', temp: 0});
  log('Comando RESUME enviado.');
}

// Event listeners
btnConnect.addEventListener('click', connectWS);
btnDisconnect.addEventListener('click', disconnectWS);

btnSendCity.addEventListener('click', () => {
  const city = cityInput.value.trim();
  if(!city) return alert('Escribe una ciudad');
  sendCityWS(city);
});

btnManualTemp.addEventListener('click', manualTempAction);
btnStop.addEventListener('click', sendStop);
btnResume.addEventListener('click', sendResume);

// auto-scroll logs to top is already done by prepending
log('WebApp lista. Ingresa la IP del ESP32 y conecta.');
