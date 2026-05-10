import { useState } from "react";

const API_DOCS = [
  {
    method: "GET", path: "/api/v1/status", tag: "Sistema",
    summary: "Estado del dispositivo",
    description: "Retorna el estado actual del ESP32: modo AP, conectando o conectado a red WiFi.",
    params: [],
    requestBody: null,
    responses: [
      {
        code: 200, label: "Modo AP",
        body: `{\n  "success": true,\n  "state": "ap_mode",\n  "ap_ssid": "ESP32-Config",\n  "ap_ip": "192.168.4.1",\n  "clients": 1,\n  "uptime_s": 42\n}`
      },
      {
        code: 200, label: "Conectado (STA)",
        body: `{\n  "success": true,\n  "state": "connected",\n  "ssid": "MiRedWiFi",\n  "ip": "192.168.1.105",\n  "rssi": -62,\n  "mac": "AA:BB:CC:DD:EE:FF",\n  "gateway": "192.168.1.1",\n  "dns": "8.8.8.8",\n  "uptime_s": 3600\n}`
      }
    ]
  },
  {
    method: "GET", path: "/api/v1/scan", tag: "WiFi",
    summary: "Escaneo de redes WiFi",
    description: "Inicia un escaneo activo (≈2–3 s) y retorna las redes encontradas, ordenadas por intensidad de señal (RSSI). Máximo 20 redes.",
    params: [],
    requestBody: null,
    responses: [
      {
        code: 200, label: "Éxito",
        body: `{\n  "success": true,\n  "count": 2,\n  "networks": [\n    {\n      "ssid": "MiRedWiFi",\n      "rssi": -45,\n      "channel": 6,\n      "encrypted": true,\n      "bssid": "AA:BB:CC:DD:EE:FF"\n    },\n    {\n      "ssid": "RedAbierta",\n      "rssi": -72,\n      "channel": 11,\n      "encrypted": false,\n      "bssid": "11:22:33:44:55:66"\n    }\n  ]\n}`
      }
    ]
  },
  {
    method: "POST", path: "/api/v1/credentials", tag: "Credenciales",
    summary: "Guardar credenciales WiFi",
    description: "Persiste SSID y contraseña en NVS (Preferences) y dispara el intento de conexión a la red. La respuesta se envía antes del intento de conexión.",
    params: [],
    requestBody: {
      contentType: "application/json",
      schema: [
        { name: "ssid", type: "string", required: true, desc: "Nombre de la red (1–32 caracteres)" },
        { name: "password", type: "string", required: false, desc: "Contraseña WPA2 (vacía = red abierta; si se provee: mín. 8 caracteres)" }
      ],
      example: `{\n  "ssid": "MiRedWiFi",\n  "password": "micontraseña123"\n}`
    },
    responses: [
      { code: 200, label: "Guardado OK", body: `{\n  "success": true,\n  "message": "Credenciales guardadas. Intentando conexión..."\n}` },
      { code: 400, label: "SSID inválido", body: `{\n  "success": false,\n  "message": "SSID inválido (1–32 caracteres)"\n}` },
      { code: 400, label: "Contraseña corta", body: `{\n  "success": false,\n  "message": "Contraseña debe tener ≥ 8 caracteres o estar vacía (red abierta)"\n}` },
      { code: 400, label: "JSON inválido", body: `{\n  "success": false,\n  "message": "JSON inválido: InvalidInput"\n}` }
    ]
  },
  {
    method: "DELETE", path: "/api/v1/credentials", tag: "Credenciales",
    summary: "Eliminar credenciales guardadas",
    description: "Borra las credenciales del NVS sin cambiar el estado de red actual. El dispositivo volverá al modo AP en el siguiente reinicio.",
    params: [],
    requestBody: null,
    responses: [
      { code: 200, label: "Eliminadas", body: `{\n  "success": true,\n  "message": "Credenciales eliminadas"\n}` }
    ]
  },
  {
    method: "POST", path: "/api/v1/reset", tag: "Sistema",
    summary: "Restablecer configuración → modo AP",
    description: "Elimina credenciales de NVS y reinicia el WiFi en modo AP inmediatamente. Equivalente al botón BOOT pulsado 3 segundos.",
    params: [],
    requestBody: null,
    responses: [
      { code: 200, label: "Reset OK", body: `{\n  "success": true,\n  "message": "Configuración restablecida. Volviendo al modo AP..."\n}` }
    ]
  },
  {
    method: "POST", path: "/api/v1/restart", tag: "Sistema",
    summary: "Reiniciar dispositivo",
    description: "Ejecuta ESP.restart(). Las credenciales guardadas en NVS se conservan.",
    params: [],
    requestBody: null,
    responses: [
      { code: 200, label: "Reiniciando", body: `{\n  "success": true,\n  "message": "Reiniciando dispositivo..."\n}` }
    ]
  },
  {
    method: "GET", path: "/api/v1/info", tag: "Sistema",
    summary: "Información del hardware/firmware",
    description: "Retorna datos del chip ESP32, memoria disponible, versión de SDK y uptime.",
    params: [],
    requestBody: null,
    responses: [
      {
        code: 200, label: "Info OK",
        body: `{\n  "success": true,\n  "chip_model": "ESP32-D0WD-V3",\n  "chip_revision": 3,\n  "cpu_freq_mhz": 240,\n  "flash_size_kb": 4096,\n  "free_heap_b": 213456,\n  "uptime_s": 7200,\n  "sdk_version": "v5.1.2",\n  "firmware": "2.0.0"\n}`
      }
    ]
  }
];

const METHOD_COLORS = {
  GET:    { bg: "#dbeafe", text: "#1e40af", border: "#93c5fd" },
  POST:   { bg: "#d1fae5", text: "#065f46", border: "#6ee7b7" },
  DELETE: { bg: "#fee2e2", text: "#991b1b", border: "#fca5a5" },
  PUT:    { bg: "#fef3c7", text: "#92400e", border: "#fcd34d" },
};

function MethodBadge({ method }) {
  const c = METHOD_COLORS[method] || METHOD_COLORS.GET;
  return (
    <span style={{
      background: c.bg, color: c.text, border: `1px solid ${c.border}`,
      borderRadius: 5, padding: "2px 10px", fontFamily: "monospace",
      fontSize: 12, fontWeight: 700, letterSpacing: "0.04em", whiteSpace: "nowrap"
    }}>{method}</span>
  );
}

function CodeBlock({ code }) {
  const [copied, setCopied] = useState(false);
  return (
    <div style={{ position: "relative" }}>
      <pre style={{
        background: "var(--color-background-tertiary)",
        border: "0.5px solid var(--color-border-tertiary)",
        borderRadius: 8, padding: "12px 16px", margin: 0,
        fontSize: 12, fontFamily: "monospace", overflowX: "auto",
        color: "var(--color-text-primary)", lineHeight: 1.6
      }}>{code}</pre>
      <button
        onClick={() => { navigator.clipboard?.writeText(code); setCopied(true); setTimeout(() => setCopied(false), 1500); }}
        style={{
          position: "absolute", top: 8, right: 8,
          background: "var(--color-background-secondary)", border: "0.5px solid var(--color-border-tertiary)",
          borderRadius: 5, padding: "2px 8px", fontSize: 11, cursor: "pointer",
          color: "var(--color-text-secondary)"
        }}
      >{copied ? "✓ copiado" : "copiar"}</button>
    </div>
  );
}

function EndpointCard({ ep }) {
  const [open, setOpen] = useState(false);
  const [activeResp, setActiveResp] = useState(0);

  return (
    <div style={{
      border: "0.5px solid var(--color-border-tertiary)",
      borderRadius: 10, overflow: "hidden", marginBottom: 8
    }}>
      <button
        onClick={() => setOpen(o => !o)}
        style={{
          width: "100%", display: "flex", alignItems: "center", gap: 12,
          padding: "12px 16px", background: open ? "var(--color-background-secondary)" : "var(--color-background-primary)",
          border: "none", cursor: "pointer", textAlign: "left"
        }}
      >
        <MethodBadge method={ep.method} />
        <code style={{ fontFamily: "monospace", fontSize: 13, color: "var(--color-text-primary)", flex: 1 }}>{ep.path}</code>
        <span style={{ fontSize: 13, color: "var(--color-text-secondary)", maxWidth: 260, textAlign: "right" }}>{ep.summary}</span>
        <span style={{ color: "var(--color-text-secondary)", marginLeft: 8 }}>{open ? "▲" : "▼"}</span>
      </button>

      {open && (
        <div style={{ padding: "0 16px 16px", background: "var(--color-background-primary)" }}>
          <div style={{ height: "0.5px", background: "var(--color-border-tertiary)", margin: "0 -16px 16px" }} />

          <p style={{ fontSize: 13, color: "var(--color-text-secondary)", marginBottom: 16 }}>{ep.description}</p>

          {/* Headers section */}
          <div style={{ marginBottom: 12 }}>
            <div style={{ fontSize: 11, fontWeight: 500, textTransform: "uppercase", letterSpacing: "0.08em", color: "var(--color-text-secondary)", marginBottom: 6 }}>Headers de respuesta</div>
            <div style={{ display: "flex", gap: 8, flexWrap: "wrap" }}>
              {[["Content-Type", "application/json"], ["Access-Control-Allow-Origin", "*"], ["Cache-Control", "no-cache"]].map(([k,v]) => (
                <span key={k} style={{ background: "var(--color-background-secondary)", border: "0.5px solid var(--color-border-tertiary)", borderRadius: 5, padding: "2px 8px", fontSize: 11, fontFamily: "monospace" }}>
                  <span style={{ color: "var(--color-text-secondary)" }}>{k}: </span>
                  <span style={{ color: "var(--color-text-primary)" }}>{v}</span>
                </span>
              ))}
            </div>
          </div>

          {/* Request body */}
          {ep.requestBody && (
            <div style={{ marginBottom: 16 }}>
              <div style={{ fontSize: 11, fontWeight: 500, textTransform: "uppercase", letterSpacing: "0.08em", color: "var(--color-text-secondary)", marginBottom: 8 }}>
                Request body — <code style={{ fontFamily: "monospace" }}>{ep.requestBody.contentType}</code>
              </div>
              <table style={{ width: "100%", borderCollapse: "collapse", fontSize: 12, marginBottom: 10 }}>
                <thead>
                  <tr style={{ background: "var(--color-background-secondary)" }}>
                    {["Campo", "Tipo", "Requerido", "Descripción"].map(h => (
                      <th key={h} style={{ padding: "6px 10px", textAlign: "left", fontWeight: 500, fontSize: 11, color: "var(--color-text-secondary)", border: "0.5px solid var(--color-border-tertiary)" }}>{h}</th>
                    ))}
                  </tr>
                </thead>
                <tbody>
                  {ep.requestBody.schema.map(f => (
                    <tr key={f.name}>
                      <td style={{ padding: "6px 10px", fontFamily: "monospace", color: "var(--color-text-primary)", border: "0.5px solid var(--color-border-tertiary)" }}>{f.name}</td>
                      <td style={{ padding: "6px 10px", fontFamily: "monospace", color: "#2563eb", border: "0.5px solid var(--color-border-tertiary)" }}>{f.type}</td>
                      <td style={{ padding: "6px 10px", border: "0.5px solid var(--color-border-tertiary)", textAlign: "center" }}>
                        <span style={{ color: f.required ? "#16a34a" : "var(--color-text-secondary)" }}>{f.required ? "✓" : "—"}</span>
                      </td>
                      <td style={{ padding: "6px 10px", color: "var(--color-text-secondary)", border: "0.5px solid var(--color-border-tertiary)" }}>{f.desc}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
              <div style={{ fontSize: 11, color: "var(--color-text-secondary)", marginBottom: 6 }}>Ejemplo:</div>
              <CodeBlock code={ep.requestBody.example} />
            </div>
          )}

          {/* Responses */}
          <div>
            <div style={{ fontSize: 11, fontWeight: 500, textTransform: "uppercase", letterSpacing: "0.08em", color: "var(--color-text-secondary)", marginBottom: 8 }}>Respuestas</div>
            <div style={{ display: "flex", gap: 6, marginBottom: 10, flexWrap: "wrap" }}>
              {ep.responses.map((r, i) => (
                <button key={i} onClick={() => setActiveResp(i)} style={{
                  padding: "3px 12px", borderRadius: 5, border: "0.5px solid",
                  cursor: "pointer", fontSize: 12,
                  borderColor: i === activeResp ? (r.code >= 400 ? "#fca5a5" : "#6ee7b7") : "var(--color-border-tertiary)",
                  background: i === activeResp ? (r.code >= 400 ? "#fee2e2" : "#d1fae5") : "var(--color-background-secondary)",
                  color: i === activeResp ? (r.code >= 400 ? "#991b1b" : "#065f46") : "var(--color-text-secondary)"
                }}>
                  {r.code} {r.label}
                </button>
              ))}
            </div>
            <CodeBlock code={ep.responses[activeResp].body} />
          </div>
        </div>
      )}
    </div>
  );
}

export default function App() {
  const [filterTag, setFilterTag] = useState("Todos");
  const tags = ["Todos", ...new Set(API_DOCS.map(e => e.tag))];
  const filtered = filterTag === "Todos" ? API_DOCS : API_DOCS.filter(e => e.tag === filterTag);

  return (
    <div style={{ padding: "1rem 0", fontFamily: "var(--font-sans)" }}>
      {/* Header */}
      <div style={{ background: "var(--color-background-secondary)", border: "0.5px solid var(--color-border-tertiary)", borderRadius: 10, padding: "16px 20px", marginBottom: 20 }}>
        <div style={{ display: "flex", alignItems: "center", gap: 12, marginBottom: 8 }}>
          <div style={{ width: 40, height: 40, borderRadius: 8, background: "#1e3a5f", display: "flex", alignItems: "center", justifyContent: "center" }}>
            <span style={{ fontSize: 20 }}>📡</span>
          </div>
          <div>
            <div style={{ fontWeight: 500, fontSize: 16, color: "var(--color-text-primary)" }}>ESP32 WiFi Manager API</div>
            <div style={{ fontSize: 12, color: "var(--color-text-secondary)", fontFamily: "monospace" }}>Base URL: http://&lt;device-ip&gt;/api/v1 · Versión 2.0.0</div>
          </div>
          <span style={{ marginLeft: "auto", background: "#d1fae5", color: "#065f46", border: "1px solid #6ee7b7", borderRadius: 5, padding: "2px 10px", fontSize: 11, fontWeight: 500 }}>REST / JSON</span>
        </div>
        <div style={{ display: "flex", gap: 16, flexWrap: "wrap", fontSize: 12, color: "var(--color-text-secondary)" }}>
          <span>🔓 Sin autenticación</span>
          <span>🌐 CORS habilitado</span>
          <span>⚡ HTTP/1.1</span>
          <span>💾 NVS Preferences</span>
          <span>🔌 Puerto 80</span>
        </div>
      </div>

      {/* Filter tags */}
      <div style={{ display: "flex", gap: 8, marginBottom: 16, flexWrap: "wrap" }}>
        {tags.map(t => (
          <button key={t} onClick={() => setFilterTag(t)} style={{
            padding: "4px 14px", borderRadius: 20, border: "0.5px solid",
            cursor: "pointer", fontSize: 12, fontWeight: filterTag === t ? 500 : 400,
            borderColor: filterTag === t ? "var(--color-border-primary)" : "var(--color-border-tertiary)",
            background: filterTag === t ? "var(--color-background-secondary)" : "transparent",
            color: filterTag === t ? "var(--color-text-primary)" : "var(--color-text-secondary)"
          }}>{t}</button>
        ))}
      </div>

      {/* Endpoint list */}
      {filtered.map((ep, i) => <EndpointCard key={i} ep={ep} />)}

      {/* Footer note */}
      <div style={{ marginTop: 16, padding: "10px 14px", background: "var(--color-background-secondary)", borderRadius: 8, fontSize: 12, color: "var(--color-text-secondary)" }}>
        💡 <strong>Nota de implementación:</strong> El servidor HTTP (WebServer) atiende 1 cliente efectivo a la vez. Para producción con múltiples clientes simultáneos, considerar ESPAsyncWebServer.
      </div>
    </div>
  );
}
