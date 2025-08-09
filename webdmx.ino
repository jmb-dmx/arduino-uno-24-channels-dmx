/*
  Web DMX Controller for Wemos D1 Mini (ESP8266)
  Final Step: Save/Erase Functionality
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ESP-DMX.h>
#include <EEPROM.h>

// --- Configuration ---
const char* ssid = "VOTRE_SSID";
const char* password = "VOTRE_MOT_DE_PASSE";
// -----------------------------------------

ESP8266WebServer server(80);
WebSocketsServer webSocket(81);
DMX dmx;

const int DMX_CHANNELS = 48;
byte dmxValues[DMX_CHANNELS];

String buildHtmlPage() {
  String page = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Contrôleur DMX Web</title>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; background-color: #1e1e1e; color: #e0e0e0; margin: 0; padding: 15px; }
        h1 { text-align: center; color: #0097e6; }
        .controls { display: flex; justify-content: center; gap: 20px; margin-bottom: 20px; }
        .controls button { padding: 10px 20px; font-size: 1em; color: #fff; background-color: #007bff; border: none; border-radius: 5px; cursor: pointer; transition: background-color 0.2s; }
        .controls button:hover { background-color: #0056b3; }
        .controls button.danger { background-color: #dc3545; }
        .controls button.danger:hover { background-color: #c82333; }
        .container { display: grid; grid-template-columns: repeat(auto-fill, minmax(280px, 1fr)); gap: 15px; }
        .slider-group { background-color: #333; border-radius: 8px; padding: 15px; display: flex; flex-direction: column; box-shadow: 0 2px 4px rgba(0,0,0,0.2); }
        .slider-group .header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px; }
        .slider-group label { font-weight: bold; font-size: 1.1em; }
        .slider-group .value { font-family: monospace; font-size: 1.2em; color: #0097e6; }
        .slider-group input[type="range"] { width: 100%; -webkit-appearance: none; appearance: none; height: 10px; background: #555; border-radius: 5px; outline: none; }
        .slider-group input[type="range"]::-webkit-slider-thumb { -webkit-appearance: none; appearance: none; width: 20px; height: 20px; background: #0097e6; cursor: pointer; border-radius: 50%; }
        .slider-group input[type="range"]::-moz-range-thumb { width: 20px; height: 20px; background: #0097e6; cursor: pointer; border-radius: 50%; }
    </style>
</head>
<body>
    <h1>Contrôleur DMX Web</h1>
    <div class="controls">
        <button onclick="saveValues()">Sauvegarder</button>
        <button class="danger" onclick="eraseValues()">Tout Effacer</button>
    </div>
    <div class="container" id="sliders-container">
)rawliteral";

  for (int i = 1; i <= DMX_CHANNELS; i++) {
    page += "<div class='slider-group'>";
    page += "<div class='header'><label for='ch" + String(i) + "'>Canal " + String(i) + "</label><span class='value' id='val" + String(i) + "'>0</span></div>";
    page += "<input type='range' id='ch" + String(i) + "' min='0' max='255' value='0' oninput='updateSlider(" + String(i) + ")'>";
    page += "</div>";
  }

  page += R"rawliteral(
    </div>
    <script>
        var gateway = `ws://${window.location.hostname}:81/`;
        var websocket;

        window.addEventListener('load', onload);

        function onload(event) {
            initWebSocket();
        }

        function initWebSocket() {
            console.log('Trying to open a WebSocket connection...');
            websocket = new WebSocket(gateway);
            websocket.onopen    = onOpen;
            websocket.onclose   = onClose;
            websocket.onmessage = onMessage;
        }

        function onOpen(event) {
            console.log('Connection opened');
        }

        function onClose(event) {
            console.log('Connection closed');
            setTimeout(initWebSocket, 2000);
        }

        function onMessage(event) {
            console.log(event.data);
            if (event.data.startsWith('init:')) {
                let values = event.data.substring(5).split(',');
                for (let i = 0; i < values.length; i++) {
                    let channel = i + 1;
                    let slider = document.getElementById('ch' + channel);
                    let valueSpan = document.getElementById('val' + channel);
                    if(slider) {
                        slider.value = values[i];
                        valueSpan.textContent = values[i];
                    }
                }
            }
        }

        function updateSlider(channel) {
            var slider = document.getElementById('ch' + channel);
            var valueSpan = document.getElementById('val' + channel);
            valueSpan.textContent = slider.value;
            websocket.send(channel + ":" + slider.value);
        }

        function saveValues() {
            websocket.send('save');
            alert('Valeurs sauvegardées !');
        }

        function eraseValues() {
            if (confirm('Etes-vous sur de vouloir effacer toutes les valeurs ? Cette action est irreversible.')) {
                websocket.send('erase');
                for (let i = 1; i <= 48; i++) {
                    let slider = document.getElementById('ch' + i);
                    let valueSpan = document.getElementById('val' + i);
                    if(slider) slider.value = 0;
                    if(valueSpan) valueSpan.textContent = '0';
                }
            }
        }
    </script>
</body>
</html>
)rawliteral";
  return page;
}

void handleRoot() {
  server.send(200, "text/html", buildHtmlPage());
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Deconnecte!\n", num);
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connecte depuis %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

        String values = "init:";
        for (int i = 0; i < DMX_CHANNELS; i++) {
          values += String(dmxValues[i]);
          if (i < DMX_CHANNELS - 1) {
            values += ",";
          }
        }
        webSocket.sendTXT(num, values);
      }
      break;
    case WStype_TEXT:
      String message = String((char*)payload);
      if (message == "save") {
        for (int i = 0; i < DMX_CHANNELS; i++) {
          EEPROM.write(i, dmxValues[i]);
        }
        EEPROM.commit();
        Serial.println("Valeurs DMX sauvegardees dans l'EEPROM.");
      } else if (message == "erase") {
        for (int i = 0; i < DMX_CHANNELS; i++) {
          dmxValues[i] = 0;
          dmx.write(i + 1, 0);
          EEPROM.write(i, 0);
        }
        EEPROM.commit();
        Serial.println("EEPROM effacee.");
        String all_zero = "init:";
        for(int i = 0; i < DMX_CHANNELS; i++) {
          all_zero += "0";
          if (i < DMX_CHANNELS - 1) {
            all_zero += ",";
          }
        }
        webSocket.broadcastTXT(all_zero);
      } else {
        int colonIndex = message.indexOf(':');
        if (colonIndex > 0) {
          String channelStr = message.substring(0, colonIndex);
          String valueStr = message.substring(colonIndex + 1);
          int channel = channelStr.toInt();
          int value = valueStr.toInt();

          if (channel > 0 && channel <= DMX_CHANNELS) {
            dmxValues[channel - 1] = value;
            dmx.write(channel, value);
          }
        }
      }
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(10);

  // Initialisation de l'EEPROM
  EEPROM.begin(DMX_CHANNELS);

  // Initialisation du DMX
  dmx.init(DMX_CHANNELS);
  Serial.println("DMX initialise.");

  // Chargement des valeurs depuis l'EEPROM
  for (int i = 0; i < DMX_CHANNELS; i++) {
    dmxValues[i] = EEPROM.read(i);
    dmx.write(i + 1, dmxValues[i]);
  }
  Serial.println("Valeurs DMX chargees depuis l'EEPROM.");

  // Connexion au réseau WiFi
  Serial.println();
  Serial.print("Connexion a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connecte");
  Serial.println("Adresse IP: ");
  Serial.println(WiFi.localIP());

  // Lancement du serveur Web
  server.on("/", handleRoot);
  server.begin();
  Serial.println("Serveur web lance sur le port 80");

  // Lancement du serveur WebSocket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("Serveur WebSocket lance sur le port 81");
}

void loop() {
  server.handleClient();
  webSocket.loop();
  dmx.update(); // Important: Maintient le signal DMX a jour
}
