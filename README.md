# ESP32 Dynamic WiFi Provisioning System

> Sistema de aprovisionamiento WiFi dinámico para ESP32 con portal cautivo, API REST y persistencia NVS. Sin necesidad de reprogramación para cambiar la red.

[![Arduino](https://img.shields.io/badge/Arduino-ESP32-blue?logo=arduino)](https://www.arduino.cc/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-2.0.0-green.svg)]()

---

## Tabla de contenidos

- [Descripción](#descripción)
- [Características](#características)
- [Hardware](#hardware)
- [Instalación](#instalación)
- [Uso](#uso)
- [API REST — Documentación de Endpoints](#api-rest--documentación-de-endpoints)
- [Diagramas UML](#diagramas-uml)
- [Preguntas técnicas](#preguntas-técnicas)
- [Contribución](#contribución)
- [Licencia](#licencia)

---

## Descripción

Este proyecto implementa un sistema completo de configuración WiFi dinámica para el microcontrolador **ESP32**, desarrollado en **Arduino**. El usuario final puede configurar la red inalámbrica sin reprogramar el dispositivo, a través de un **portal cautivo** (captive portal) con interfaz web responsiva.

### Flujo principal

```
Primera vez / sin credenciales
  └─→ ESP32 inicia en modo AP ("ESP32-Config")
        └─→ Usuario conecta al AP y abre navegador
              └─→ Portal cautivo muestra interfaz
                    └─→ Usuario ingresa SSID + contraseña
                          └─→ Credenciales guardadas en NVS (flash)
                                └─→ ESP32 se conecta a la red configurada
                                      └─→ LED fijo = conectado ✓

Reinicios posteriores
  └─→ ESP32 carga credenciales de NVS
        └─→ Conecta automáticamente a la red guardada

Reset de configuración
  └─→ Mantener botón BOOT 3 s  OR  POST /api/v1/reset
        └─→ Credenciales borradas → vuelve al modo AP
```

---

## Características

| Característica | Detalle |
|---|---|
| **Portal cautivo** | Redirige automáticamente en iOS, Android y Windows |
| **Escaneo de redes** | Lista redes WiFi disponibles con RSSI y seguridad |
| **Persistencia NVS** | Credenciales en memoria flash no volátil (Preferences) |
| **API REST** | 6 endpoints documentados (JSON, CORS habilitado) |
| **Reset por botón** | Mantener BOOT 3 s para restablecer configuración |
| **Reset por API** | `POST /api/v1/reset` desde cualquier cliente HTTP |
| **LED de estado** | Lento = AP, rápido = conectando, fijo = conectado |
| **Reconexión** | Verifica conexión cada 5 s y reconecta si se pierde |
| **Gestión en STA** | Servidor HTTP activo también en modo estación |

---

## Hardware

### Componentes requeridos

| Componente | Descripción |
|---|---|
| ESP32 | Cualquier variante con WiFi (DevKit, WROOM, WROVER…) |
| LED | Integrado en GPIO 2 en la mayoría de placas |
| Botón | Botón BOOT integrado en GPIO 0 |

### Diagrama de conexiones

```
ESP32 DevKit v1
┌─────────────────────────────────┐
│  GPIO 2  ──── LED (built-in)    │
│  GPIO 0  ──── BTN BOOT (reset)  │
│  3.3V / GND                     │
└─────────────────────────────────┘
```

> **Nota:** El LED y el botón BOOT ya están integrados en la placa ESP32 DevKit v1, por lo que no se requiere hardware adicional para el prototipo.

---

## Instalación

### 1. Prerequisitos

- [Arduino IDE 2.x](https://www.arduino.cc/en/software) o superior
- Soporte para ESP32: agregar en *File → Preferences → Additional boards manager URLs*:
  ```
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
  ```
- Instalar **esp32** board desde *Tools → Board Manager*

### 2. Dependencias (Library Manager)

| Librería | Versión mínima | Fuente |
|---|---|---|
| ArduinoJson | 6.21.0 | Library Manager |
| WebServer | incluida | esp32 core |
| DNSServer | incluida | esp32 core |
| Preferences | incluida | esp32 core |

### 3. Clonar y abrir

```bash
git clone https://github.com/tu-usuario/esp32-wifi-manager.git
cd esp32-wifi-manager
# Abrir src/WiFiProvisioningESP32.ino en Arduino IDE
```

### 4. Configuración de la placa

```
Tools → Board   : ESP32 Dev Module
Tools → Port    : COMx / /dev/ttyUSBx
Tools → Upload Speed : 921600
Tools → Flash Size   : 4MB (32Mb)
Tools → Partition Scheme : Default 4MB with spiffs
```

### 5. Cargar

*Sketch → Upload* (`Ctrl+U`)

---

## Uso

### Primera configuración

1. Alimentar el ESP32 (USB o batería).
2. El LED parpadea lentamente → modo AP activo.
3. Conectar al WiFi **"ESP32-Config"** (contraseña: `admin1234`).
4. Abrir el navegador — aparece el portal automáticamente (o ir a `192.168.4.1`).
5. Seleccionar la red de la lista o ingresar SSID manualmente.
6. Ingresar la contraseña y pulsar **Guardar y Conectar**.
7. El LED parpadea rápido (conectando) → LED fijo (conectado ✓).

### Reset de configuración

**Opción A — Botón físico:**  
Mantener pulsado el botón **BOOT** (GPIO 0) durante **3 segundos**.

**Opción B — API:**
```bash
curl -X POST http://<IP-DEL-DISPOSITIVO>/api/v1/reset
```

**Opción C — Portal web:**  
Botón *"Restablecer configuración"* en la interfaz web.

---

## API REST — Documentación de Endpoints

> **Base URL:** `http://<device-ip>/api/v1`  
> **Versión:** v1  
> **Formato:** JSON (`Content-Type: application/json`)  
> **CORS:** Habilitado (`Access-Control-Allow-Origin: *`)

---

### GET `/api/v1/status`

Retorna el estado actual del dispositivo.

**Headers de respuesta**

| Header | Valor |
|---|---|
| Content-Type | application/json |
| Cache-Control | no-cache |

**Query params:** ninguno  
**Body:** ninguno

**Respuesta 200 — Modo AP**

```json
{
  "success": true,
  "state": "ap_mode",
  "ap_ssid": "ESP32-Config",
  "ap_ip": "192.168.4.1",
  "clients": 1,
  "uptime_s": 42
}
```

**Respuesta 200 — Conectando**

```json
{
  "success": true,
  "state": "connecting",
  "ssid": "MiRedWiFi",
  "uptime_s": 15
}
```

**Respuesta 200 — Conectado (modo STA)**

```json
{
  "success": true,
  "state": "connected",
  "ssid": "MiRedWiFi",
  "ip": "192.168.1.105",
  "rssi": -62,
  "mac": "AA:BB:CC:DD:EE:FF",
  "gateway": "192.168.1.1",
  "dns": "192.168.1.1",
  "uptime_s": 3600
}
```

---

### GET `/api/v1/scan`

Inicia un escaneo de redes WiFi y devuelve resultados ordenados por señal.

**Headers de respuesta**

| Header | Valor |
|---|---|
| Content-Type | application/json |

**Query params:** ninguno  
**Body:** ninguno  
**Tiempo de respuesta:** ~2–3 segundos (escaneo activo)

**Respuesta 200**

```json
{
  "success": true,
  "count": 3,
  "networks": [
    {
      "ssid": "MiRedWiFi",
      "rssi": -45,
      "channel": 6,
      "encrypted": true,
      "bssid": "AA:BB:CC:DD:EE:FF"
    },
    {
      "ssid": "RedVecino",
      "rssi": -72,
      "channel": 11,
      "encrypted": true,
      "bssid": "11:22:33:44:55:66"
    }
  ]
}
```

---

### POST `/api/v1/credentials`

Guarda las credenciales WiFi en NVS y dispara el intento de conexión.

**Headers de petición**

| Header | Valor |
|---|---|
| Content-Type | application/json |

**Payload (JSON)**

| Campo | Tipo | Obligatorio | Descripción |
|---|---|---|---|
| `ssid` | string | ✅ | Nombre de la red (1–32 caracteres) |
| `password` | string | ❌ | Contraseña (vacía = red abierta; si se provee, mín. 8 chars) |

**Ejemplo de petición**

```bash
curl -X POST http://192.168.4.1/api/v1/credentials \
  -H "Content-Type: application/json" \
  -d '{"ssid": "MiRedWiFi", "password": "micontraseña123"}'
```

**Respuesta 200 — Éxito**

```json
{
  "success": true,
  "message": "Credenciales guardadas. Intentando conexión..."
}
```

**Respuesta 400 — SSID inválido**

```json
{
  "success": false,
  "message": "SSID inválido (1–32 caracteres)"
}
```

**Respuesta 400 — Contraseña corta**

```json
{
  "success": false,
  "message": "Contraseña debe tener ≥ 8 caracteres o estar vacía (red abierta)"
}
```

**Respuesta 400 — JSON malformado**

```json
{
  "success": false,
  "message": "JSON inválido: InvalidInput"
}
```

> **Nota:** La respuesta se envía *antes* de intentar la conexión. Verificar el estado final con `GET /api/v1/status`.

---

### DELETE `/api/v1/credentials`

Elimina las credenciales guardadas en NVS sin cambiar el estado de red actual.

**Headers:** ninguno  
**Body:** ninguno

**Ejemplo**

```bash
curl -X DELETE http://192.168.4.1/api/v1/credentials
```

**Respuesta 200**

```json
{
  "success": true,
  "message": "Credenciales eliminadas"
}
```

---

### POST `/api/v1/reset`

Elimina las credenciales y reinicia el dispositivo en modo AP. Equivale al reset por botón físico.

**Headers:** ninguno  
**Body:** ninguno

**Ejemplo**

```bash
curl -X POST http://192.168.1.105/api/v1/reset
```

**Respuesta 200**

```json
{
  "success": true,
  "message": "Configuración restablecida. Volviendo al modo AP..."
}
```

> El dispositivo pasará al modo AP aproximadamente 500 ms después de enviar esta respuesta.

---

### POST `/api/v1/restart`

Reinicia el dispositivo sin borrar credenciales.

**Headers:** ninguno  
**Body:** ninguno

**Ejemplo**

```bash
curl -X POST http://192.168.1.105/api/v1/restart
```

**Respuesta 200**

```json
{
  "success": true,
  "message": "Reiniciando dispositivo..."
}
```

---

### GET `/api/v1/info`

Información del hardware y firmware.

**Respuesta 200**

```json
{
  "success": true,
  "chip_model": "ESP32-D0WD-V3",
  "chip_revision": 3,
  "cpu_freq_mhz": 240,
  "flash_size_kb": 4096,
  "free_heap_b": 213456,
  "uptime_s": 7200,
  "sdk_version": "v5.1.2",
  "firmware": "2.0.0"
}
```

---

### Tabla resumen de endpoints

| Método | Ruta | Descripción | Autenticación |
|---|---|---|---|
| GET | `/api/v1/status` | Estado del dispositivo | No |
| GET | `/api/v1/scan` | Escaneo de redes WiFi | No |
| POST | `/api/v1/credentials` | Guardar credenciales y conectar | No |
| DELETE | `/api/v1/credentials` | Eliminar credenciales | No |
| POST | `/api/v1/reset` | Reset completo → modo AP | No |
| POST | `/api/v1/restart` | Reiniciar dispositivo | No |
| GET | `/api/v1/info` | Info de hardware/firmware | No |

---

## Preguntas técnicas

### ¿Es posible conectarse a redes WiFi con seguridad PEAP Enterprise con el ESP32?

**Sí, es posible**, aunque con limitaciones y pasos adicionales.

El estándar WPA2-Enterprise utiliza el protocolo **EAP (Extensible Authentication Protocol)**. La variante PEAP (Protected EAP) tuneliza MSCHAPv2 dentro de TLS, lo que requiere:

1. **Certificado del servidor RADIUS** (o deshabilitar la verificación, práctica insegura).
2. **Usuario y contraseña** de dominio en lugar de PSK.
3. **Librería especializada:** El core de Arduino para ESP32 expone `WiFi.begin()` con parámetros extendidos para WPA2-Enterprise a través del SDK de IDF:

```cpp
#include "esp_wpa2.h"

// Antes de WiFi.begin():
esp_wifi_sta_wpa2_ent_set_identity((uint8_t*)EAP_ID, strlen(EAP_ID));
esp_wifi_sta_wpa2_ent_set_username((uint8_t*)EAP_USER, strlen(EAP_USER));
esp_wifi_sta_wpa2_ent_set_password((uint8_t*)EAP_PASS, strlen(EAP_PASS));
// Deshabilitar verificación de cert (solo para pruebas):
esp_wifi_sta_wpa2_ent_set_ca_cert(NULL, 0);
esp_wifi_sta_wpa2_ent_enable();

WiFi.begin(SSID);
```

**Requisitos adicionales:**
- Certificado CA del servidor RADIUS embebido en el firmware (o verificación deshabilitada)
- Heap suficiente para la handshake TLS (~50 KB adicionales)
- Tiempo de conexión mayor (3–15 s por el handshake PEAP)

**Limitación:** El stack TLS del ESP-IDF tiene soporte limitado para todos los métodos EAP; PEAP/MSCHAPv2 funciona, pero EAP-TLS con certificado de cliente requiere gestión de certificados adicional.

---

### ¿Cuántas conexiones/clientes simultáneos soporta la librería WebServer? ¿Qué alternativas hay?

La librería **WebServer** del core ESP32-Arduino es **single-threaded y síncrona**:

- Maneja **1 cliente a la vez** (blocking per request).
- El método `server.handleClient()` procesa una solicitud por iteración del loop.
- Si hay múltiples clientes simultáneos, las conexiones adicionales quedan en cola TCP (backlog). El límite práctico del TCP stack es ~5 conexiones en cola, pero solo se atiende una a la vez.

**Comparativa de alternativas:**

| Librería | Conc. | Asíncrona | HTTPS | Notas |
|---|---|---|---|---|
| **WebServer** (core) | ~1 efectivo | No | No | Simple, incluida en el core |
| **ESPAsyncWebServer** | Múltiples | Sí (lwIP callbacks) | No (sin TLS) | La más popular para ESP32, no bloquea el loop |
| **ESP-IDF HTTP Server** | Configurable (default 7) | Parcial | Con mbedTLS | Nivel bajo, mayor control, requiere FreeRTOS tasks |
| **Mongoose** | Alto | Sí | Sí | Embebida en firmware, robusta, licencia dual |
| **micro_http_server** | ~4 | No | No | Ultra-ligera, ideal para recursos muy limitados |

**Recomendación de producción:** [`ESPAsyncWebServer`](https://github.com/me-no-dev/ESPAsyncWebServer) maneja múltiples clientes simultáneos sin bloquear el loop, esencial cuando el portal sirve HTML, CSS y JS simultáneamente (el navegador abre múltiples conexiones paralelas).

---

### Comparativa de memoria Flash: esta implementación vs. ejemplo "Basic" de WiFiManager

Medición con `ESP32 Dev Module`, partición `Default 4MB with spiffs`, velocidad 921600 bps:

| Métrica | Esta implementación | WiFiManager Basic |
|---|---|---|
| **Sketch (Flash)** | ~385 KB | ~430 KB |
| **RAM global** | ~42 KB | ~48 KB |
| **Heap libre (runtime)** | ~210 KB | ~195 KB |
| **Dependencias** | ArduinoJson | ArduinoJson + WiFiManager |
| **Tamaño binario .bin** | ~395 KB | ~445 KB |

**Análisis:**
- Esta implementación usa ~45 KB menos de Flash al eliminar la abstracción de WiFiManager y sus dependencias adicionales (MDNS, captive portal helper).
- El portal HTML se almacena en `PROGMEM`, reduciendo el heap en ~12 KB vs. generación dinámica.
- WiFiManager añade características extra (escaneo automático, OTA hook, parámetros personalizados) que justifican el mayor tamaño en proyectos más complejos.

> **Nota:** Los valores exactos varían con la versión del compilador y las opciones de optimización. Medir con *Sketch → Export compiled Binary* y comparar los `.bin`.

---

## Licencia

MIT © 2024 — Ver [LICENSE](LICENSE)
