/**
 * @file    WiFiProvisioningESP32.ino
 * @brief   ESP32 Dynamic WiFi Provisioning — Portal cautivo + API REST
 * @version 3.0.0
 *
 * Dependencias (Library Manager):
 *   - ArduinoJson >= 6.21.0
 *   (WebServer, DNSServer, Preferences vienen con el core esp32)
 */

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>

// ─── Pines ────────────────────────────────────────────────────────────────────
#define PIN_LED        2
#define PIN_BTN        0
#define RESET_HOLD_MS  3000

// ─── AP ───────────────────────────────────────────────────────────────────────
#define AP_SSID    "ESP32-Config"
#define AP_PASS    "admin1234"
#define AP_CHANNEL 6
static const IPAddress AP_IP(192, 168, 4, 1);
static const IPAddress AP_MASK(255, 255, 255, 0);

// ─── Tiempos ──────────────────────────────────────────────────────────────────
#define HTTP_PORT        80
#define DNS_PORT         53
#define CONNECT_TIMEOUT  15000   // ms
#define STATUS_INTERVAL   5000   // ms

// ─── NVS ──────────────────────────────────────────────────────────────────────
#define NVS_NS    "wifi_cfg"
#define NVS_SSID  "ssid"
#define NVS_PASS  "pass"
#define NVS_SAVED "saved"

// ─── Objetos globales ─────────────────────────────────────────────────────────
WebServer   server(HTTP_PORT);
DNSServer   dns;
Preferences prefs;

// ─── Estado ───────────────────────────────────────────────────────────────────
enum class State { AP, CONNECTING, CONNECTED };
State       devState  = State::AP;
String      savedSSID, savedPass;
bool        hasCreds  = false;

unsigned long lastCheck  = 0;
unsigned long lastBlink  = 0;
unsigned long btnDownAt  = 0;
bool          btnHeld    = false;
bool          ledOn      = false;

// ═══════════════════════════════════════════════════════════════════════════════
//  HTML — PROGMEM
//  El JS detecta si está en AP (192.168.4.1) o STA y ajusta la URL del API.
// ═══════════════════════════════════════════════════════════════════════════════
const char HTML[] PROGMEM = R"===(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 WiFi</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0f172a;color:#f1f5f9;font-family:'Segoe UI',system-ui,sans-serif;
     min-height:100vh;display:flex;align-items:center;justify-content:center;padding:1rem}
.card{background:#1e293b;border:1px solid #334155;border-radius:16px;
      padding:2rem;width:100%;max-width:400px}
h1{font-size:1.2rem;font-weight:700;text-align:center;margin-bottom:.25rem}
.sub{color:#94a3b8;font-size:.85rem;text-align:center;margin-bottom:1.5rem}
.status{display:flex;align-items:center;gap:.5rem;padding:.6rem .9rem;
        background:#0f172a;border:1px solid #334155;border-radius:8px;
        margin-bottom:1rem;font-size:.8rem}
.dot{width:8px;height:8px;border-radius:50%;flex-shrink:0}
.dot.ap  {background:#f59e0b;animation:pulse 1.5s infinite}
.dot.conn{background:#3b82f6;animation:pulse .7s infinite}
.dot.ok  {background:#22c55e}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.3}}
.lbl{font-size:.7rem;font-weight:600;text-transform:uppercase;
     letter-spacing:.08em;color:#94a3b8;margin-bottom:.5rem}
.net-list{display:flex;flex-direction:column;gap:.4rem;
          max-height:190px;overflow-y:auto;margin-bottom:.5rem}
.net{display:flex;align-items:center;justify-content:space-between;
     padding:.45rem .75rem;background:#0f172a;border:1px solid #334155;
     border-radius:8px;cursor:pointer;font-size:.85rem;transition:border-color .15s}
.net:hover{border-color:#3b82f6}
.rssi{font-size:.75rem;color:#94a3b8}
hr{border:none;border-top:1px solid #334155;margin:1rem 0}
label{display:block;font-size:.8rem;color:#94a3b8;margin-bottom:.3rem}
input[type=text],input[type=password]{
  width:100%;padding:.6rem .85rem;background:#0f172a;
  border:1px solid #334155;border-radius:8px;
  color:#f1f5f9;font-size:.95rem;outline:none;transition:border-color .2s}
input:focus{border-color:#3b82f6}
.field{margin-bottom:.9rem;position:relative}
.eye{position:absolute;right:.75rem;top:50%;transform:translateY(-50%);
     background:none;border:none;color:#94a3b8;cursor:pointer}
.row{display:flex;align-items:center;justify-content:space-between;margin-bottom:.5rem}
.btn{padding:.7rem;border:none;border-radius:8px;font-size:.9rem;
     font-weight:600;cursor:pointer;transition:opacity .2s;width:100%}
.btn:disabled{opacity:.5;cursor:not-allowed}
.primary{background:#3b82f6;color:#fff}
.primary:hover:not(:disabled){background:#2563eb}
.outline{background:transparent;border:1px solid #334155;color:#94a3b8;
         width:auto;padding:.4rem .75rem;font-size:.8rem}
.outline:hover{border-color:#3b82f6;color:#3b82f6}
.danger{background:transparent;border:1px solid #ef4444;
        color:#ef4444;font-size:.8rem;margin-top:.75rem}
.danger:hover{background:rgba(239,68,68,.1)}
.info-box{background:#0f172a;border:1px solid #334155;border-radius:8px;
          padding:.75rem 1rem;font-size:.85rem;
          display:flex;flex-direction:column;gap:.45rem}
.info-row{display:flex;justify-content:space-between}
.info-val{font-family:monospace}
#toast{position:fixed;bottom:1.5rem;left:50%;
       transform:translateX(-50%) translateY(60px);
       background:#1e293b;border:1px solid #334155;border-radius:10px;
       padding:.65rem 1.1rem;font-size:.85rem;min-width:200px;
       text-align:center;transition:transform .3s,opacity .3s;opacity:0;z-index:99}
#toast.show{transform:translateX(-50%) translateY(0);opacity:1}
#toast.ok {border-color:#22c55e;color:#22c55e}
#toast.err{border-color:#ef4444;color:#ef4444}
#toast.inf{border-color:#3b82f6;color:#3b82f6}
.spin{display:inline-block;width:13px;height:13px;
      border:2px solid rgba(255,255,255,.3);border-top-color:#fff;
      border-radius:50%;animation:sp .7s linear infinite;
      vertical-align:middle;margin-right:.4rem}
@keyframes sp{to{transform:rotate(360deg)}}
</style>
</head>
<body>
<div class="card">
  <h1>&#128225; ESP32 WiFi Setup</h1>
  <p class="sub">Configure la red inalambrica del dispositivo</p>

  <div class="status">
    <div class="dot ap" id="dot"></div>
    <span id="stxt">Cargando...</span>
  </div>

  <!-- Panel modo AP -->
  <div id="apPanel">
    <div class="row">
      <span class="lbl">Redes disponibles</span>
      <button class="btn outline" id="scanBtn" onclick="doScan()">&#8635; Escanear</button>
    </div>
    <div class="net-list" id="netList">
      <div style="color:#94a3b8;padding:.5rem .75rem;font-size:.85rem">
        Presione Escanear para buscar redes
      </div>
    </div>
    <hr>
    <div class="lbl" style="margin-bottom:.75rem">Configuracion manual</div>
    <div class="field">
      <label>Nombre de red (SSID)</label>
      <input type="text" id="ssid" placeholder="Mi Red WiFi"
             maxlength="32" autocomplete="off">
    </div>
    <div class="field">
      <label>Contrasena</label>
      <input type="password" id="pass" placeholder="&#8226;&#8226;&#8226;&#8226;&#8226;&#8226;&#8226;&#8226;" maxlength="64">
      <button class="eye" onclick="togglePass()">&#128065;</button>
    </div>
    <button class="btn primary" id="connectBtn" onclick="doConnect()">
      Guardar y Conectar
    </button>
  </div>

  <!-- Panel modo STA (conectado) -->
  <div id="staPanel" style="display:none">
    <div class="lbl" style="margin-bottom:.75rem">Conexion activa</div>
    <div class="info-box">
      <div class="info-row">
        <span style="color:#94a3b8">Red</span>
        <span class="info-val" id="iSSID">-</span>
      </div>
      <div class="info-row">
        <span style="color:#94a3b8">IP local</span>
        <span class="info-val" id="iIP">-</span>
      </div>
      <div class="info-row">
        <span style="color:#94a3b8">Senal</span>
        <span class="info-val" id="iRSSI">-</span>
      </div>
      <div class="info-row">
        <span style="color:#94a3b8">Gateway</span>
        <span class="info-val" id="iGW">-</span>
      </div>
    </div>
  </div>

  <hr>
  <button class="btn danger" onclick="doReset()">
    &#9888; Restablecer configuracion
  </button>
</div>
<div id="toast"></div>

<script>
// La URL base cambia segun el modo:
//   AP  -> la pagina viene de 192.168.4.1, usamos IP absoluta
//   STA -> la pagina viene de la IP del ESP32, usamos ruta relativa
var BASE = (location.hostname === '192.168.4.1')
           ? 'http://192.168.4.1'
           : '';
var API = BASE + '/api/v1';

function toast(msg, type) {
  var t = document.getElementById('toast');
  t.textContent = msg;
  t.className = 'show ' + (type || 'inf');
  clearTimeout(t._t);
  t._t = setTimeout(function(){ t.className = ''; }, 3500);
}

function esc(s) {
  return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}

function bars(rssi) {
  if (rssi >= -55) return '\u2582\u2584\u2586\u2588';
  if (rssi >= -65) return '\u2582\u2584\u2586';
  if (rssi >= -75) return '\u2582\u2584';
  return '\u2582';
}

// ── Status ────────────────────────────────────────────────────────────────────
function refreshStatus() {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', API + '/status', true);
  xhr.timeout = 5000;
  xhr.onload = function() {
    if (xhr.status !== 200) return;
    var d;
    try { d = JSON.parse(xhr.responseText); } catch(e) { return; }
    var dot = document.getElementById('dot');
    var txt = document.getElementById('stxt');
    dot.className = 'dot';
    if (d.state === 'connected') {
      dot.classList.add('ok');
      txt.textContent = 'Conectado a ' + d.ssid + '  IP: ' + d.ip;
      document.getElementById('apPanel').style.display  = 'none';
      document.getElementById('staPanel').style.display = 'block';
      document.getElementById('iSSID').textContent = d.ssid;
      document.getElementById('iIP').textContent   = d.ip;
      document.getElementById('iRSSI').textContent = d.rssi + ' dBm';
      document.getElementById('iGW').textContent   = d.gateway || '-';
    } else if (d.state === 'connecting') {
      dot.classList.add('conn');
      txt.textContent = 'Conectando a ' + d.ssid + '...';
    } else {
      dot.classList.add('ap');
      txt.textContent = 'Modo AP — ' + (d.ap_ssid || 'ESP32-Config');
      document.getElementById('apPanel').style.display  = 'block';
      document.getElementById('staPanel').style.display = 'none';
    }
  };
  xhr.onerror = xhr.ontimeout = function() {
    document.getElementById('stxt').textContent = 'Sin respuesta del dispositivo';
  };
  xhr.send();
}

// ── Escaneo ───────────────────────────────────────────────────────────────────
function doScan() {
  var list = document.getElementById('netList');
  var btn  = document.getElementById('scanBtn');
  list.innerHTML = '<div style="color:#94a3b8;padding:.5rem .75rem;font-size:.85rem">Escaneando (espere ~3 s)...</div>';
  btn.disabled = true;

  var xhr = new XMLHttpRequest();
  xhr.open('GET', API + '/scan', true);
  xhr.timeout = 10000;
  xhr.onload = function() {
    btn.disabled = false;
    var d;
    try { d = JSON.parse(xhr.responseText); } catch(e) {
      list.innerHTML = '<div style="color:#ef4444;padding:.5rem;font-size:.85rem">Error al parsear respuesta</div>';
      return;
    }
    if (!d.success || !d.networks || d.networks.length === 0) {
      list.innerHTML = '<div style="color:#94a3b8;padding:.5rem .75rem;font-size:.85rem">No se encontraron redes</div>';
      return;
    }
    var html = '';
    for (var i = 0; i < d.networks.length; i++) {
      var n = d.networks[i];
      html += '<div class="net" onclick="pick(\'' + esc(n.ssid).replace(/'/g,"\\'") + '\')">'
            + '<span>' + esc(n.ssid) + '</span>'
            + '<span><span class="rssi">' + bars(n.rssi) + ' ' + n.rssi + ' dBm</span>'
            + (n.encrypted ? ' <span style="color:#f59e0b;font-size:.7rem">&#128274;</span>' : '')
            + '</span></div>';
    }
    list.innerHTML = html;
  };
  xhr.onerror = xhr.ontimeout = function() {
    btn.disabled = false;
    list.innerHTML = '<div style="color:#ef4444;padding:.5rem;font-size:.85rem">Error de conexion con el dispositivo</div>';
    toast('Error al escanear', 'err');
  };
  xhr.send();
}

function pick(ssid) {
  document.getElementById('ssid').value = ssid;
  document.getElementById('pass').focus();
}

// ── Conectar ──────────────────────────────────────────────────────────────────
function doConnect() {
  var ssid = document.getElementById('ssid').value.trim();
  var pass = document.getElementById('pass').value;
  if (!ssid) { toast('Ingrese el SSID', 'err'); return; }
  if (pass.length > 0 && pass.length < 8) {
    toast('Contrasena minimo 8 caracteres', 'err'); return;
  }

  var btn = document.getElementById('connectBtn');
  btn.disabled = true;
  btn.innerHTML = '<span class="spin"></span>Guardando...';

  var xhr = new XMLHttpRequest();
  xhr.open('POST', API + '/credentials', true);
  xhr.setRequestHeader('Content-Type', 'application/json');
  xhr.timeout = 8000;
  xhr.onload = function() {
    var d;
    try { d = JSON.parse(xhr.responseText); } catch(e) { d = {success:false,message:'Error'}; }
    if (d.success) {
      toast('Guardado. Conectando...', 'ok');
      btn.innerHTML = '<span class="spin"></span>Conectando...';
      var tries = 0;
      var iv = setInterval(function() {
        tries++;
        refreshStatus();
        if (tries >= 12) {
          clearInterval(iv);
          btn.disabled = false;
          btn.textContent = 'Guardar y Conectar';
        }
      }, 2000);
    } else {
      toast(d.message || 'Error', 'err');
      btn.disabled = false;
      btn.textContent = 'Guardar y Conectar';
    }
  };
  xhr.onerror = xhr.ontimeout = function() {
    // Si el ESP32 se desconecta del AP para conectarse a la red,
    // el XHR puede fallar aunque el guardado fue exitoso.
    toast('Guardado. Verificando conexion...', 'inf');
    btn.innerHTML = '<span class="spin"></span>Conectando...';
    var tries = 0;
    var iv = setInterval(function() {
      tries++;
      refreshStatus();
      if (tries >= 12) {
        clearInterval(iv);
        btn.disabled = false;
        btn.textContent = 'Guardar y Conectar';
      }
    }, 2000);
  };
  xhr.send(JSON.stringify({ssid: ssid, password: pass}));
}

// ── Reset ─────────────────────────────────────────────────────────────────────
function doReset() {
  if (!confirm('Restablecer configuracion WiFi?')) return;
  var xhr = new XMLHttpRequest();
  xhr.open('POST', API + '/reset', true);
  xhr.timeout = 5000;
  xhr.onload = function() {
    var d;
    try { d = JSON.parse(xhr.responseText); } catch(e) { d = {success:false}; }
    toast(d.message || 'Restablecido', d.success ? 'inf' : 'err');
    if (d.success) setTimeout(refreshStatus, 2500);
  };
  xhr.onerror = xhr.ontimeout = function() {
    toast('Restablecido (sin respuesta)', 'inf');
    setTimeout(refreshStatus, 3000);
  };
  xhr.send();
}

function togglePass() {
  var el = document.getElementById('pass');
  el.type = el.type === 'password' ? 'text' : 'password';
}

// ── Init ──────────────────────────────────────────────────────────────────────
refreshStatus();
setInterval(refreshStatus, 8000);
</script>
</body>
</html>
)===";

// ═══════════════════════════════════════════════════════════════════════════════
//  HELPERS — definidos ANTES de registerRoutes()
//  (lambdas de GCC no resuelven forward declarations)
// ═══════════════════════════════════════════════════════════════════════════════

void cors() {
  server.sendHeader("Access-Control-Allow-Origin",  "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET,POST,DELETE,OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.sendHeader("Cache-Control", "no-cache,no-store");
}

String jsonOk(const String& msg) {
  return "{\"success\":true,\"message\":\"" + msg + "\"}";
}

String jsonErr(const String& msg) {
  return "{\"success\":false,\"message\":\"" + msg + "\"}";
}

String statusJson() {
  DynamicJsonDocument doc(512);
  doc["success"] = true;
  switch (devState) {
    case State::CONNECTED:
      doc["state"]   = "connected";
      doc["ssid"]    = WiFi.SSID();
      doc["ip"]      = WiFi.localIP().toString();
      doc["rssi"]    = WiFi.RSSI();
      doc["mac"]     = WiFi.macAddress();
      doc["gateway"] = WiFi.gatewayIP().toString();
      doc["dns"]     = WiFi.dnsIP().toString();
      break;
    case State::CONNECTING:
      doc["state"] = "connecting";
      doc["ssid"]  = savedSSID;
      break;
    default:
      doc["state"]   = "ap_mode";
      doc["ap_ssid"] = AP_SSID;
      doc["ap_ip"]   = WiFi.softAPIP().toString();
      doc["clients"] = WiFi.softAPgetStationNum();
      break;
  }
  doc["uptime_s"] = millis() / 1000;
  String out; serializeJson(doc, out); return out;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  NVS
// ═══════════════════════════════════════════════════════════════════════════════

bool loadCreds() {
  prefs.begin(NVS_NS, true);
  bool ok = prefs.getBool(NVS_SAVED, false);
  if (ok) {
    savedSSID = prefs.getString(NVS_SSID, "");
    savedPass = prefs.getString(NVS_PASS, "");
    ok = savedSSID.length() > 0;
  }
  prefs.end();
  hasCreds = ok;
  return ok;
}

void saveCreds(const String& ssid, const String& pass) {
  prefs.begin(NVS_NS, false);
  prefs.putBool(NVS_SAVED, true);
  prefs.putString(NVS_SSID, ssid);
  prefs.putString(NVS_PASS, pass);
  prefs.end();
  savedSSID = ssid; savedPass = pass; hasCreds = true;
  Serial.printf("[NVS] Guardado SSID='%s'\n", ssid.c_str());
}

void clearCreds() {
  prefs.begin(NVS_NS, false);
  prefs.clear();
  prefs.end();
  savedSSID = ""; savedPass = ""; hasCreds = false;
  Serial.println("[NVS] Credenciales borradas");
}

// ═══════════════════════════════════════════════════════════════════════════════
//  MODO AP — forward declaration necesaria porque startAP y registerRoutes
//  se llaman mutuamente
// ═══════════════════════════════════════════════════════════════════════════════
void startAP();

// ═══════════════════════════════════════════════════════════════════════════════
//  RUTAS HTTP
// ═══════════════════════════════════════════════════════════════════════════════

void registerRoutes() {
  server.stop();

  // Portal cautivo
  auto portal = []() { server.send_P(200, "text/html", HTML); };
  server.on("/",                    HTTP_GET, portal);
  server.on("/generate_204",        HTTP_GET, portal);
  server.on("/hotspot-detect.html", HTTP_GET, portal);
  server.on("/ncsi.txt",            HTTP_GET, portal);
  server.on("/connecttest.txt",     HTTP_GET, portal);
  server.on("/redirect",            HTTP_GET, []() {
    server.sendHeader("Location", "/"); server.send(302);
  });

  // GET /api/v1/status
  server.on("/api/v1/status", HTTP_GET, []() {
    cors();
    server.send(200, "application/json", statusJson());
  });

  // GET /api/v1/scan
  server.on("/api/v1/scan", HTTP_GET, []() {
    cors();
    WiFi.scanDelete();
    int n = WiFi.scanNetworks(false, false, false, 500);

    if (n < 0) {
      server.send(500, "application/json", jsonErr("Escaneo fallido"));
      return;
    }

    int cnt = min(n, 20);
    // Ordenar por RSSI descendente (índices)
    int idx[20];
    for (int i = 0; i < cnt; i++) idx[i] = i;
    for (int i = 0; i < cnt - 1; i++)
      for (int j = 0; j < cnt - i - 1; j++)
        if (WiFi.RSSI(idx[j]) < WiFi.RSSI(idx[j+1])) {
          int t = idx[j]; idx[j] = idx[j+1]; idx[j+1] = t;
        }

    DynamicJsonDocument doc(4096);
    doc["success"] = true;
    doc["count"]   = cnt;
    JsonArray arr  = doc.createNestedArray("networks");
    for (int i = 0; i < cnt; i++) {
      JsonObject o   = arr.createNestedObject();
      o["ssid"]      = WiFi.SSID(idx[i]);
      o["rssi"]      = WiFi.RSSI(idx[i]);
      o["channel"]   = WiFi.channel(idx[i]);
      o["encrypted"] = (WiFi.encryptionType(idx[i]) != WIFI_AUTH_OPEN);
      o["bssid"]     = WiFi.BSSIDstr(idx[i]);
    }
    WiFi.scanDelete();
    String out; serializeJson(doc, out);
    server.send(200, "application/json", out);
  });

  // POST /api/v1/credentials
  server.on("/api/v1/credentials", HTTP_POST, []() {
    cors();
    if (!server.hasArg("plain")) {
      server.send(400, "application/json", jsonErr("Body vacio")); return;
    }
    DynamicJsonDocument req(512);
    if (deserializeJson(req, server.arg("plain"))) {
      server.send(400, "application/json", jsonErr("JSON invalido")); return;
    }
    String ssid = req["ssid"] | "";
    String pass = req["password"] | "";
    ssid.trim();

    if (ssid.isEmpty() || ssid.length() > 32) {
      server.send(400, "application/json", jsonErr("SSID invalido")); return;
    }
    if (pass.length() > 0 && pass.length() < 8) {
      server.send(400, "application/json",
                  jsonErr("Contrasena minimo 8 caracteres")); return;
    }

    saveCreds(ssid, pass);
    server.send(200, "application/json", jsonOk("Credenciales guardadas"));

    delay(300);
    dns.stop();
    devState = State::CONNECTING;
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());

    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < CONNECT_TIMEOUT)
      delay(200);

    if (WiFi.status() == WL_CONNECTED) {
      devState = State::CONNECTED;
      Serial.printf("[WiFi] Conectado IP=%s\n",
                    WiFi.localIP().toString().c_str());
      registerRoutes();
      server.begin();
    } else {
      WiFi.disconnect(true);
      Serial.println("[WiFi] Timeout -> AP");
      startAP();
    }
  });

  // OPTIONS preflight
  server.on("/api/v1/credentials", HTTP_OPTIONS, []() {
    cors(); server.send(204);
  });

  // DELETE /api/v1/credentials
  server.on("/api/v1/credentials", HTTP_DELETE, []() {
    cors();
    clearCreds();
    server.send(200, "application/json", jsonOk("Credenciales eliminadas"));
  });

  // POST /api/v1/reset
  server.on("/api/v1/reset", HTTP_POST, []() {
    cors();
    clearCreds();
    server.send(200, "application/json",
                jsonOk("Restablecido. Volviendo a modo AP..."));
    delay(400);
    startAP();
  });

  // POST /api/v1/restart
  server.on("/api/v1/restart", HTTP_POST, []() {
    cors();
    server.send(200, "application/json", jsonOk("Reiniciando..."));
    delay(400); ESP.restart();
  });

  // GET /api/v1/info
  server.on("/api/v1/info", HTTP_GET, []() {
    cors();
    DynamicJsonDocument doc(512);
    doc["success"]     = true;
    doc["chip"]        = ESP.getChipModel();
    doc["revision"]    = ESP.getChipRevision();
    doc["cpu_mhz"]     = ESP.getCpuFreqMHz();
    doc["flash_kb"]    = ESP.getFlashChipSize() / 1024;
    doc["free_heap_b"] = ESP.getFreeHeap();
    doc["uptime_s"]    = millis() / 1000;
    doc["sdk"]         = ESP.getSdkVersion();
    doc["firmware"]    = "3.0.0";
    String out; serializeJson(doc, out);
    server.send(200, "application/json", out);
  });

  // 404
  server.onNotFound([]() {
    if (devState == State::AP) {
      server.sendHeader("Location", "/"); server.send(302);
    } else {
      server.send(404, "application/json", jsonErr("Not found"));
    }
  });
}

// ═══════════════════════════════════════════════════════════════════════════════
//  MODO AP
// ═══════════════════════════════════════════════════════════════════════════════

void startAP() {
  WiFi.disconnect(true);
  delay(100);
  // WIFI_AP_STA: necesario para que scanNetworks() funcione desde modo AP
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(AP_IP, AP_IP, AP_MASK);
  WiFi.softAP(AP_SSID, AP_PASS, AP_CHANNEL);
  WiFi.disconnect();  // desconectar STA sin apagar el radio

  Serial.printf("[AP] SSID=%s  IP=%s\n",
                AP_SSID, WiFi.softAPIP().toString().c_str());

  dns.start(DNS_PORT, "*", AP_IP);
  registerRoutes();
  server.begin();
  devState = State::AP;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  LED
// ═══════════════════════════════════════════════════════════════════════════════

void updateLED() {
  if (devState == State::CONNECTED) {
    digitalWrite(PIN_LED, HIGH); return;
  }
  unsigned long iv = (devState == State::CONNECTING) ? 100 : 500;
  if (millis() - lastBlink >= iv) {
    lastBlink = millis();
    ledOn = !ledOn;
    digitalWrite(PIN_LED, ledOn);
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  BOTÓN RESET (GPIO 0 / BOOT — mantener 3 s)
// ═══════════════════════════════════════════════════════════════════════════════

void checkButton() {
  bool pressed = (digitalRead(PIN_BTN) == LOW);
  if (pressed && !btnHeld) {
    btnHeld = true; btnDownAt = millis();
  } else if (!pressed) {
    btnHeld = false;
  } else if (btnHeld && millis() - btnDownAt >= RESET_HOLD_MS) {
    Serial.println("[BTN] Reset 3s -> AP");
    btnHeld = false;
    clearCreds();
    delay(100);
    startAP();
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  RECONEXIÓN AUTOMÁTICA
// ═══════════════════════════════════════════════════════════════════════════════

void checkConnection() {
  if (devState != State::CONNECTED) return;
  if (millis() - lastCheck < STATUS_INTERVAL) return;
  lastCheck = millis();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Perdida -> reconectando...");
    devState = State::CONNECTING;
    WiFi.reconnect();
    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < CONNECT_TIMEOUT)
      delay(200);

    if (WiFi.status() == WL_CONNECTED) {
      devState = State::CONNECTED;
      Serial.println("[WiFi] Reconectado");
    } else {
      Serial.println("[WiFi] Fallo -> AP");
      startAP();
    }
  }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  SETUP / LOOP
// ═══════════════════════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== ESP32 WiFi Provisioning v3.0 ===");

  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BTN, INPUT_PULLUP);
  digitalWrite(PIN_LED, LOW);

  if (loadCreds()) {
    Serial.printf("[NVS] Credenciales encontradas: '%s'\n", savedSSID.c_str());
    devState = State::CONNECTING;
    WiFi.mode(WIFI_STA);
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());

    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < CONNECT_TIMEOUT) {
      updateLED(); delay(200);
    }

    if (WiFi.status() == WL_CONNECTED) {
      devState = State::CONNECTED;
      Serial.printf("[WiFi] Conectado  IP=%s\n",
                    WiFi.localIP().toString().c_str());
      registerRoutes();
      server.begin();
    } else {
      Serial.println("[WiFi] Timeout -> modo AP");
      startAP();
    }
  } else {
    Serial.println("[NVS] Sin credenciales -> modo AP");
    startAP();
  }
}

void loop() {
  if (devState == State::AP) dns.processNextRequest();
  server.handleClient();
  updateLED();
  checkButton();
  checkConnection();
}
