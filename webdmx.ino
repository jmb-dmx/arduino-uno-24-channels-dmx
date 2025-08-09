/*
  Web DMX Controller for Wemos D1 Mini (ESP8266)
  - 48 canaux (2 pages de 24 faders verticaux)
  - WebSocket temps réel
  - Sauvegarde / Effacement EEPROM

  Câblage DMX (TX uniquement) :
  - Sortie DMX via UART1 de l'ESP8266 : GPIO2 (D4) -> DI du MAX485/SN75176
  - RE & DE du MAX485 : reliez à 3V3 (mode TX permanent)
  - GND commun entre Wemos et la ligne DMX
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ESPDMX.h>
#include <EEPROM.h>

// --- Configuration WiFi ---
const char* ssid = "VOTRE_SSID";
const char* password = "VOTRE_MOT_DE_PASSE";

// --- Instances serveurs ---
ESP8266WebServer server(80);
WebSocketsServer webSocket(81);

// --- DMX ---
const int DMX_CHANNELS = 48;
DMXESPSerial dmx;  // <-- IMPORTANT : classe correcte pour ESPDMX (ESP8266)
byte dmxValues[DMX_CHANNELS];

// -------------------- UI HTML --------------------
String buildHtmlPage() {
  String page = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Contrôleur DMX Web</title>
<style>
  :root { color-scheme: dark; }
  body { font-family: -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif; background:#1e1e1e; color:#e0e0e0; margin:0; padding:15px; }
  h1 { text-align:center; color:#09c; margin:0 0 10px; }
  .top-controls { display:flex; gap:12px; justify-content:space-between; align-items:center; margin: 0 0 16px; flex-wrap:wrap; }
  .buttons { display:flex; gap:8px; }
  .btn { padding:10px 16px; font-size:14px; color:#fff; background:#007bff; border:none; border-radius:8px; cursor:pointer; transition:transform .05s, background .2s; }
  .btn:active { transform: translateY(1px); }
  .btn.danger { background:#dc3545; }
  .btn.alt { background:transparent; border:1px solid #09c; color:#09c; }
  .pagination { display:flex; gap:8px; flex-wrap:wrap; }
  .pagination .btn.active { background:#09c; border-color:#09c; color:#fff; }

  .page { display:none; flex-direction:column; align-items:center; gap:20px; }
  .page.active { display:flex; }
  .fader-row { display:flex; justify-content:center; flex-wrap:wrap; gap:10px; padding:15px; background:#282828; border-radius:12px; }

  .slider-group { background:#333; border-radius:10px; padding:10px; display:flex; flex-direction:column; align-items:center; width:64px; height:326px; box-shadow:0 2px 6px rgba(0,0,0,.25); }
  .slider-group .header { display:flex; flex-direction:column; align-items:center; margin-bottom:10px; height:46px; }
  .slider-group label { font-weight:700; font-size:.9em; }
  .slider-group .value { font-family: ui-monospace,SFMono-Regular,Menlo,monospace; font-size:1.1em; color:#09c; margin-top:4px; }
  input[type="range"].vertical { -webkit-appearance: slider-vertical; writing-mode: bt-lr; width: 22px; height: 210px; cursor: pointer; }
  footer { margin-top:12px; text-align:center; opacity:.7; font-size:12px; }
</style>
</head>
<body>
  <h1>Contrôleur DMX Web</h1>
  <div class="top-controls">
    <div class="pagination">
      <button id="btn-page1" class="btn alt active" onclick="showPage(1)">Page 1 (1–24)</button>
      <button id="btn-page2" class="btn alt" onclick="showPage(2)">Page 2 (25–48)</button>
    </div>
    <div class="buttons">
      <button class="btn" onclick="saveValues()">Sauvegarder</button>
      <button class="btn danger" onclick="eraseValues()">Tout effacer</button>
    </div>
  </div>

  <div id="page1" class="page active">
    <div class="fader-row">
)rawliteral";

  // Génération des 48 faders
  for (int i = 1; i <= DMX_CHANNELS; i++) {
    // Structure des pages et des rangées
    if (i == 13) page += "</div><div class='fader-row'>";                                // Fin rangée 1, début rangée 2
    if (i == 25) page += "</div></div><div id='page2' class='page'><div class='fader-row'>"; // Fin page 1, début page 2 (rangée 3)
    if (i == 37) page += "</div><div class='fader-row'>";                                // Fin rangée 3, début rangée 4

    // Fader individuel
    page += "<div class='slider-group'>";
    page += "<div class='header'><label for='ch" + String(i) + "'>CH " + String(i) + "</label><span class='value' id='val" + String(i) + "'>0</span></div>";
    page += "<input type='range' class='vertical' id='ch" + String(i) + "' min='0' max='255' value='0' oninput='updateSlider(" + String(i) + ")'>";
    page += "</div>";
  }

  page += R"rawliteral(
    </div>
  </div>

  <footer>DMX via ESP8266 UART1 (GPIO2/D4) · EEPROM persistante</footer>

  <script>
    const gateway = `ws://${location.hostname}:81/`;
    let websocket;

    addEventListener('load', () => initWebSocket());

    function initWebSocket() {
      console.log('Ouverture WebSocket…');
      websocket = new WebSocket(gateway);
      websocket.onopen = () => console.log('WebSocket connecté');
      websocket.onclose = () => { console.log('WebSocket fermé'); setTimeout(initWebSocket, 1500); };
      websocket.onmessage = onMessage;
    }

    function showPage(n) {
      for (const id of ['page1','page2']) document.getElementById(id).style.display = 'none';
      for (const id of ['btn-page1','btn-page2']) document.getElementById(id).classList.remove('active');
      document.getElementById('page'+n).style.display = 'flex';
      document.getElementById('btn-page'+n).classList.add('active');
    }

    function onMessage(event) {
      const data = event.data || '';
      if (data.startsWith('init:')) {
        const values = data.substring(5).split(',');
        for (let i = 0; i < values.length; i++) {
          const ch = i + 1;
          const s  = document.getElementById('ch' + ch);
          const v  = document.getElementById('val' + ch);
          if (s) s.value = values[i];
          if (v) v.textContent = values[i];
        }
      }
    }

    function updateSlider(ch) {
      const s = document.getElementById('ch' + ch);
      const v = document.getElementById('val' + ch);
      if (!s || !v) return;
      v.textContent = s.value;
      if (websocket && websocket.readyState === 1) websocket.send(ch + ':' + s.value);
    }

    function saveValues() {
      if (websocket && websocket.readyState === 1) {
        websocket.send('save');
        alert('Valeurs sauvegardées !');
      }
    }

    function eraseValues() {
      if (!confirm('Êtes-vous sûr de vouloir effacer toutes les valeurs ?')) return;
      if (websocket && websocket.readyState === 1) {
        websocket.send('erase');
      }
      for (let i = 1; i <= 48; i++) {
        const s = document.getElementById('ch' + i);
        const v = document.getElementById('val' + i);
        if (s) s.value = 0;
        if (v) v.textContent = '0';
      }
    }
  </script>
</body>
</html>
)rawliteral";
  return page;
}

// -------------------- HTTP --------------------
void handleRoot() {
  server.send(200, "text/html", buildHtmlPage());
}

// -------------------- WebSocket --------------------
void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Déconnecté\n", num);
      break;

    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connecté depuis %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);

        // Envoi des valeurs initiales aux clients
        String out = "init:";
        for (int i = 0; i < DMX_CHANNELS; i++) {
          out += String(dmxValues[i]);
          if (i < DMX_CHANNELS - 1) out += ",";
        }
        webSocket.sendTXT(num, out);
      }
      break;

    case WStype_TEXT:
      {
        String message;
        message.reserve(length);
        for (size_t i = 0; i < length; i++) {
          message += (char)payload[i];
        }

        if (message == "save") {
          for (int i = 0; i < DMX_CHANNELS; i++) EEPROM.write(i, dmxValues[i]);
          EEPROM.commit();
          Serial.println("EEPROM: valeurs DMX sauvegardées.");
          return;
        }

        if (message == "erase") {
          for (int i = 0; i < DMX_CHANNELS; i++) {
            dmxValues[i] = 0;
            dmx.write(i + 1, 0);
            EEPROM.write(i, 0);
          }
          EEPROM.commit();
          Serial.println("EEPROM effacée. DMX remis à 0.");
          // Broadcast des zéros pour resync UI
          String zeros = "init:";
          for (int i = 0; i < DMX_CHANNELS; i++) {
            zeros += "0";
            if (i < DMX_CHANNELS - 1) zeros += ",";
          }
          webSocket.broadcastTXT(zeros);
          return;
        }

        // message "channel:value"
        int colon = message.indexOf(':');
        if (colon > 0) {
          int channel = message.substring(0, colon).toInt();
          int value = message.substring(colon + 1).toInt();
          if (channel >= 1 && channel <= DMX_CHANNELS) {
            dmxValues[channel - 1] = (byte)constrain(value, 0, 255);
            dmx.write(channel, dmxValues[channel - 1]);
          }
        }
      }
      break;

    default:
      break;
  }
}

// -------------------- Setup / Loop --------------------
void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println();
  Serial.println(F("Boot Web DMX Controller (ESP8266)…"));

  // EEPROM
  EEPROM.begin(DMX_CHANNELS);  // 48 octets suffisent ici

  // DMX
  // Sur ESP8266, ESPDMX utilise UART1 (TX sur GPIO2/D4) pour transmettre.
  dmx.init(DMX_CHANNELS);  // nombre de canaux gérés
  Serial.println(F("DMX initialisé."));

  // Charger depuis EEPROM
  for (int i = 0; i < DMX_CHANNELS; i++) {
    dmxValues[i] = EEPROM.read(i);
    dmx.write(i + 1, dmxValues[i]);
  }
  Serial.println(F("Valeurs DMX chargées depuis EEPROM."));

  // WiFi
  Serial.printf("Connexion à \"%s\"…\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(350);
    Serial.print(".");
  }
  Serial.println();
  Serial.print(F("WiFi OK, IP: "));
  Serial.println(WiFi.localIP());

  // HTTP
  server.on("/", handleRoot);
  server.begin();
  Serial.println(F("Serveur HTTP sur :80"));

  // WebSocket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println(F("WebSocket sur :81"));
}

void loop() {
  server.handleClient();
  webSocket.loop();
  dmx.update();  // Maintient le signal DMX à jour (break/mark-after-break)
}
