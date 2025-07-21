/*
  Antural MotorhomeCentral - Control y Dashboard WiFi para ESP32
  ©2025 Charlie & Iván
  - Control de 6 relés vía web (activo bajo)
  - Visualización de sensores reales BMP280, con actualización automática AJAX
  - Niveles de tanques y batería, todo en diseño moderno responsive
  - Descubrimiento automático por mDNS
  - Endpoint JSON /status para apps externas
*/

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <ESPmDNS.h>  // <--- para mDNS

// --- WiFi AP ---
const char* ssid = "MotorhomeCentral";
const char* password = "antural123";
AsyncWebServer server(80);

// --- Relés (activo bajo) ---
const int NUM_RELES = 6;
const int relePins[NUM_RELES] = {5, 12, 14, 27, 26, 25};
const char* labels[NUM_RELES] = {
  "Luz Central", "Luz Habitacion", "Alacenas", "Bomba", "Heladera", "Caldera"
};
String estadosReles[NUM_RELES] = {"OFF","OFF","OFF","OFF","OFF","OFF"};
const char* ICONOS_RELES[NUM_RELES] = {
  "<svg width='40' height='40'><circle cx='20' cy='22' r='10' fill='#ffe355'/><rect x='17' y='6' width='6' height='13' rx='3' fill='#fffbe3'/></svg>",
  "<svg width='40' height='40'><rect x='7' y='18' width='26' height='14' rx='6' fill='#ffb300'/><ellipse cx='20' cy='25' rx='10' ry='7' fill='#fffbe3' opacity='.7'/></svg>",
  "<svg width='40' height='40'><rect x='8' y='10' width='24' height='20' rx='3' fill='#b09560'/></svg>",
  "<svg width='40' height='40'><ellipse cx='20' cy='30' rx='11' ry='6' fill='#6bd2ff'/></svg>",
  "<svg width='40' height='40'><rect x='11' y='9' width='18' height='24' rx='4' fill='#a3e6f5'/></svg>",
  "<svg width='40' height='40'><ellipse cx='20' cy='27' rx='10' ry='6' fill='#ffac6d'/></svg>"
};

// --- Tanques y métricas ---
int tankVals[3] = {74, 51, 17};
String tankNames[3] = {"Blancas", "Grises", "Negras"};
String tempInt = "--", tempExt = "29.1";
String presion = "--";
String altitud = "--";
String airQ = "Buena";
String mq2 = "0 ppm";

// --- BMS Simulado ---
String bat_soc = "82%";
String bat_volt = "13.2V";
String bat_current = "33.2A";
String bat_temp = "27 C";
String bat_cycles = "140";
String bat_status = "Carga";
String bat_balance = "ON";

// --- BMP280 GLOBAL ---
Adafruit_BMP280 bmp; // Usa I2C (SDA=21, SCL=22)
unsigned long lastBmpUpdate = 0;

// --- SVGs de métricas ---
const char* SVG_TERMOMETRO = "<svg width='34' height='34'><circle cx='16' cy='22' r='8' fill='#e46d27'/><rect x='13' y='5' width='6' height='14' rx='3' fill='#fff' stroke='#e46d27' stroke-width='2'/></svg>";
const char* SVG_BARO = "<svg width='34' height='34'><circle cx='16' cy='16' r='13' fill='#55b2f2'/></svg>";
const char* SVG_MONTANA = "<svg width='34' height='34'><ellipse cx='16' cy='24' rx='12' ry='6' fill='#89d06a'/></svg>";
const char* SVG_AIRE = "<svg width='34' height='34'><circle cx='16' cy='16' r='13' fill='#eee12d'/></svg>";
const char* SVG_FUEGO = "<svg width='34' height='34'><rect x='8' y='20' width='16' height='7' rx='3' fill='#e13e2d'/></svg>";


// --- FUNCIONES AUXILIARES DE UI ---
String sensorCardHTML(const char* svgIcon, const char* name, const char* value, const char* units, const char* color) {
  String id = "";
  if (String(name) == "Temp. Int.")      id = "valTempInt";
  else if (String(name) == "Temp. Ext.") id = "valTempExt";
  else if (String(name) == "Presion")    id = "valPresion";
  else if (String(name) == "Altitud")    id = "valAltitud";
  else if (String(name) == "Calidad Aire") id = "valAirQ";
  else if (String(name) == "Gas (MQ2)")   id = "valMQ2";
  else {
    String tempName = String(name);
    tempName.replace(" ", "");
    id = "val" + tempName;
  }
  return String() +
    "<div class='sensor-card'><div class='svgwrap'>" + String(svgIcon) + "</div>"
      "<div class='label'>" + String(name) + "</div>"
      "<div class='value' style='color:" + String(color) + ";'><span id='" + id + "'>" + String(value) + "</span><span class='units'>" + String(units) + "</span></div>"
    "</div>";
}

String bmsCardHTML() {
  float soc = bat_soc.toFloat();
  float current = bat_current.toFloat();
  String pilaSVG = "<svg class='bat-svg' width='84' height='64' viewBox='0 0 84 64'>";
  pilaSVG += "<rect x='6' y='13' width='66' height='38' rx='9' fill='#fff' stroke='#5271d1' stroke-width='4'/>";
  int fillW = int(60 * (soc/100.0));
  if (current < 0) {
    pilaSVG += String("<rect x='10' y='17' width='") + String(fillW) + "' height='30' rx='5' fill='#ffe100' opacity='0.85'/>";
  } else {
    pilaSVG += String("<rect x='10' y='17' width='") + String(fillW) + "' height='30' rx='5' fill='#7be495' opacity='0.85'/>";
  }
  pilaSVG += "<rect x='20' y='21' width='8' height='22' rx='2.2' fill='none' stroke='#2e3d63' stroke-width='2'/>";
  pilaSVG += "<rect x='34' y='21' width='8' height='22' rx='2.2' fill='none' stroke='#2e3d63' stroke-width='2'/>";
  pilaSVG += "<rect x='48' y='21' width='8' height='22' rx='2.2' fill='none' stroke='#2e3d63' stroke-width='2'/>";
  pilaSVG += "<rect x='74' y='22' width='8' height='20' rx='4' fill='#c3c6df' stroke='#5271d1' stroke-width='2'/>";
  if (current >= 0) {
    pilaSVG += "<circle cx='64' cy='20' r='13' fill='#ffe100' stroke='#2e3d63' stroke-width='3'/>";
    pilaSVG += "<polygon points='64,13 70,22 66,22 68,27 61,19 65,19' fill='white' stroke='#2e3d63' stroke-width='1.5'/>";
  }
  pilaSVG += "</svg>";

  String bmsCard = "<div class='bms-battery'>";
  bmsCard += "<div class='bms-col1'>" + pilaSVG +
             "<div class='bms-mainval'>" + bat_soc + "</div>";
  if (current > 0.05) {
    bmsCard += "<div class='bms-status bms-bal'>Cargando</div>";
  } else if (current < -0.05) {
    bmsCard += "<div class='bms-status' style='color:#d2b400'>Descargando</div>";
  } else {
    bmsCard += "<div class='bms-status'>Standby</div>";
  }
  bmsCard += "</div><div class='bms-col2'>";
  bmsCard += "<div><b>Volt:</b> " + bat_volt + "</div>";
  bmsCard += "<div><b>Corriente:</b> " + bat_current + "</div>";
  bmsCard += "<div><b>Temp:</b> " + bat_temp + "</div>";
  bmsCard += "<div><b>Ciclos:</b> " + bat_cycles + "</div>";
  bmsCard += "</div></div>";
  return bmsCard;
}

String tanquesCardHTML() {
  const char* liquidos[3] = { "#3698f8", "#bfc4cb", "#cfa36b" };  // Azul, gris, marrón claro
  const char* border[3]   = { "#2196f3", "#babfc4", "#be8d4e" };
  const char* labelColor[3] = { "#3698f8", "#bfc4cb", "#cfa36b" };

  String card = R"rawliteral(
    <div class='tq-bms-card'>
      <div class='tq-bms-title'>Niveles de Tanques</div>
      <div class='tq-bms-grid'>
  )rawliteral";

  for (int i = 0; i < 3; i++) {
    int porcentaje = tankVals[i];
    int maxH = 100;
    int minY = 130;
    int hLleno = int(maxH * porcentaje / 100.0);
    int yLleno = minY - hLleno;

    String tanqueSVG = "<svg width='90' height='150' viewBox='0 0 200 150'>";
    tanqueSVG += String("<rect x='20' y='30' width='160' height='100' rx='32' fill='none' stroke='") + border[i] + "' stroke-width='12'/>";
    tanqueSVG += String("<rect x='32' y='") + String(yLleno) + "' width='136' height='" + String(hLleno) +
                "' rx='18' fill='" + liquidos[i] + "' opacity='0.8'/>";
    int nivel1 = 130 - int(maxH * 0.25);
    int nivel2 = 130 - int(maxH * 0.50);
    int nivel3 = 130 - int(maxH * 0.75);
    tanqueSVG += "<rect x='35' y='" + String(nivel1) + "' width='130' height='3' rx='1.2' fill='#fff' opacity='0.35'/>";
    tanqueSVG += "<rect x='35' y='" + String(nivel2) + "' width='130' height='3' rx='1.2' fill='#fff' opacity='0.55'/>";
    tanqueSVG += "<rect x='35' y='" + String(nivel3) + "' width='130' height='3' rx='1.2' fill='#fff' opacity='0.75'/>";
    tanqueSVG += String("<rect x='70' y='10' width='60' height='24' rx='10' fill='none' stroke='") + border[i] + "' stroke-width='10'/>";
    tanqueSVG += "</svg>";

    card += "<div class='tq-bms-tank'>";
    card += "<div class='tq-bms-svgwrap'>" + tanqueSVG + "</div>";
    card += String("<div class='tq-bms-perc' style='color:") + labelColor[i] + ";'>" + String(porcentaje) + "%</div>";
    card += String("<div class='tq-bms-label'>") + tankNames[i] + "</div>";
    card += "</div>";
  }
  card += "</div></div>";
  return card;
}

// --- RELÉS: lectura y control hardware ---
void leerEstadosReles() {
  for (int i = 0; i < NUM_RELES; i++) {
    estadosReles[i] = (digitalRead(relePins[i]) == LOW) ? "ON" : "OFF";
  }
}

void setRele(int idx, bool on) {
  digitalWrite(relePins[idx], on ? LOW : HIGH);
}

void toggleRele(int channel) {
  if (channel >= 1 && channel <= NUM_RELES) {
    int idx = channel - 1;
    bool actualOn = (digitalRead(relePins[idx]) == LOW);
    setRele(idx, !actualOn);
    delay(40);
    leerEstadosReles();
    Serial.printf("[DEBUG] Relé %d (%s): %s\n", channel, labels[idx], estadosReles[idx].c_str());
  }
}

// --- HTML PRINCIPAL ---
String htmlPage() {
  String html;
  html.reserve(16000);

  html += R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <title>Antural Motorhome</title>
      <meta name='viewport' content='width=device-width, initial-scale=1'>
      <style>
        :root { --primary:#1fc800; --danger:#d30e0e; --card:#232a3b; --bg:#181f2a; }
        body { font-family: 'Segoe UI',Roboto,sans-serif; background:var(--bg); color:#eee; margin:0;}
        h1 { margin:18px 0 16px 0; font-size:2.1em; letter-spacing:2px; text-align:center;}
        .relaygrid {display:grid; grid-template-columns:repeat(auto-fit,minmax(140px,1fr)); gap:18px; max-width:960px; margin:16px auto 14px auto;}
        .relaycard {
          background:var(--card); border-radius:20px; min-height:148px;
          display:flex; flex-direction:column; align-items:center; justify-content:center;
          box-shadow: 1px 2px 9px #0005; transition:box-shadow .2s;
          border: 2px solid transparent;
        }
        .relaycard.on { box-shadow:0 0 0 2.5px var(--primary) inset;}
        .relaycard.off { box-shadow:0 0 0 2.5px var(--danger) inset;}
        .relayicon {margin-top:11px;}
        .relaylabel {font-size:1.11em; margin-top:10px; margin-bottom:7px; font-weight:500;}
        .relaybtn {
          margin:10px 0 15px 0; font-size:1em; border-radius:14px; padding:7px 16px;
          border:none; color:#fff; font-weight:600; cursor:pointer;
          background:linear-gradient(90deg,var(--primary),#2cfc74); box-shadow:1px 2px 10px #0005;
          transition: background .2s;
        }
        .relaybtn.off {background:linear-gradient(90deg,var(--danger),#fc6c3d);}
        .tq-bms-card {
          background: var(--card);
          border-radius: 22px;
          box-shadow: 0 2px 16px #0007;
          padding: 18px 12px 10px 12px;
          max-width: 800px;
          margin: 25px auto 14px auto;
        }
        .tq-bms-title {
          font-size: 1.23em;
          font-weight: 600;
          text-align: center;
          margin-bottom: 15px;
          letter-spacing: 1px;
        }
        .tq-bms-grid {
          display: flex;
          justify-content: center;
          gap: 54px;
          align-items: flex-end;
        }
        .tq-bms-tank {
          display: flex;
          flex-direction: column;
          align-items: center;
          min-width: 66px;
        }
        .tq-bms-svgwrap {
          margin-bottom: -6px;
        }
        .tq-bms-perc {
          font-size: 1.95em;
          font-weight: 800;
          margin: 5px 0 2px 0;
          text-shadow: 0 2px 8px #0008;
          letter-spacing: 1px;
        }
        .tq-bms-label {
          font-size: 1.04em;
          color: #b6bdcb;
          margin-top: 2px;
        }
        @media (max-width:650px) {
          .tq-bms-grid {gap: 13px;}
          .tq-bms-card {padding: 7px 1vw 4px 1vw;}
          .tq-bms-perc {font-size: 1.15em;}
        }
        .sensor-cards { display:grid; grid-template-columns:repeat(auto-fit,minmax(170px,1fr)); gap:14px; margin: 15px auto 10px auto; max-width:1050px; padding: 0 12px;}
        .sensor-card { background:var(--card); border-radius:18px; padding:15px 10px 10px 10px; min-width:150px; min-height:120px; display:flex; flex-direction:column; align-items:center; box-shadow:1px 2px 10px #0006;}
        .svgwrap {height:40px; margin-bottom:5px;}
        .label { font-size:1.1em; color:#e4e4e4; margin-bottom:6px;}
        .value { font-size:2em; font-weight:700;}
        .units { font-size:.7em; margin-left:3px;}
        .bms-section { margin: 22px auto 8px auto; max-width: 650px;}
        .bms-battery { display:flex; gap:20px; align-items:center; background:var(--card); border-radius:22px; box-shadow:0 2px 16px #0006; padding:20px 24px; flex-wrap:wrap; justify-content:center;}
        .bat-svg {margin-right:10px;}
        .bms-col1 { text-align:center;}
        .bms-mainval { font-size:2.1em; margin-top:8px;}
        .bms-status { font-size:1.1em; margin-top:3px;}
        .bms-col2 { margin-left:18px; font-size:1.05em; }
        .bms-bal { color:#19f06e; font-weight:bold;}
      </style>
      <script>
        var estados = [%ESTADOS%];
        function setRelayState(ch,state) {
          let card = document.getElementById('relaycard'+ch);
          let btn = document.getElementById('btn'+ch);
          if(card) card.className = 'relaycard ' + (state=='ON'?'on':'off');
          if(btn) btn.className = 'relaybtn ' + (state=='ON'?'':'off');
          if(btn) btn.textContent = (state=='ON') ? 'Apagar' : 'Encender';
        }
        function toggle(ch) {
          fetch('/toggle4ch?ch='+ch).then(_=> { updateEstados(); });
        }
        function updateEstados() {
          fetch('/estados').then(res=>res.json()).then(j=>{
            for (let i=1;i<=%NRELES%;i++) {
              estados[i-1] = j['ch'+i];
              setRelayState(i, estados[i-1]);
            }
          });
        }
        function updateSensores() {
          fetch('/sensores').then(res=>res.json()).then(j=>{
            if (document.getElementById('valTempInt'))  document.getElementById('valTempInt').textContent = j['tempInt'];
            if (document.getElementById('valTempExt'))  document.getElementById('valTempExt').textContent = j['tempExt'];
            if (document.getElementById('valPresion'))  document.getElementById('valPresion').textContent = j['presion'];
            if (document.getElementById('valAltitud'))  document.getElementById('valAltitud').textContent = j['altitud'];
            if (document.getElementById('valAirQ'))     document.getElementById('valAirQ').textContent = j['airQ'];
            if (document.getElementById('valMQ2'))      document.getElementById('valMQ2').textContent = j['mq2'];
          });
        }
        setInterval(updateEstados, 3000);
        setInterval(updateSensores, 3000);
        window.onload = () => {
          for(let i=1;i<=%NRELES%;i++) setRelayState(i, estados[i-1]);
          updateEstados();
          updateSensores();
        };
      </script>
    </head>
    <body>
      <h1>Antural Motorhome</h1>
      <div class='relaygrid'>
  )rawliteral";

  for (int i=0; i<NUM_RELES; i++) {
    String cardClass = String("relaycard ") + (estadosReles[i]=="ON"?"on":"off");
    String btnClass = String("relaybtn ") + (estadosReles[i]=="ON"?"":"off");
    html += "<div class='" + cardClass + "' id='relaycard" + String(i+1) + "'>"
              "<div class='relayicon'>" + String(ICONOS_RELES[i]) + "</div>"
              "<div class='relaylabel'>" + String(labels[i]) + "</div>"
              "<button class='" + btnClass + "' id='btn" + String(i+1) + "' onclick='toggle(" + String(i+1) + ")'>" +
                (estadosReles[i]=="ON"?"Apagar":"Encender") +
              "</button></div>";
  }

  html += "</div>";
  html += "<div class='bms-section'>" + bmsCardHTML() + "</div>";
  html += tanquesCardHTML();
  html += "<div class='sensor-cards'>";
  html += sensorCardHTML(SVG_TERMOMETRO, "Temp. Int.", tempInt.c_str(), " C", "#e46d27");
  html += sensorCardHTML(SVG_TERMOMETRO, "Temp. Ext.", tempExt.c_str(), " C", "#e46d27");
  html += sensorCardHTML(SVG_BARO, "Presion", presion.c_str(), "hPa", "#55b2f2");
  html += sensorCardHTML(SVG_MONTANA, "Altitud", altitud.c_str(), "m", "#89d06a");
  html += sensorCardHTML(SVG_AIRE, "Calidad Aire", airQ.c_str(), "", "#eee12d");
  html += sensorCardHTML(SVG_FUEGO, "Gas (MQ2)", mq2.c_str(), "", "#e13e2d");
  html += "</div></body></html>";

  String estadoVals = "";
  for (int i=0; i<NUM_RELES; i++) {
    estadoVals += "'" + estadosReles[i] + "'";
    if (i < NUM_RELES-1) estadoVals += ",";
  }
  html.replace("%ESTADOS%", estadoVals);
  html.replace("%NRELES%", String(NUM_RELES));
  return html;
}

// --- SETUP ESP32 ---
void setup() {
  Serial.begin(115200);
  Serial.println("[DEBUG] Iniciando WiFi AP...");
  WiFi.softAP(ssid, password);
  delay(200);
  WiFi.softAPConfig(IPAddress(192,168,0,50), IPAddress(192,168,0,50), IPAddress(255,255,255,0));
  Serial.println("[DEBUG] AP configurado. IP: 192.168.0.50");

  for (int i = 0; i < NUM_RELES; i++) {
    pinMode(relePins[i], OUTPUT);
    digitalWrite(relePins[i], HIGH); // Todos apagados
  }
  leerEstadosReles();

  // ---- BMP280 INIT
  Wire.begin(21, 22); // SDA=21, SCL=22
  if (!bmp.begin(0x76)) {
    Serial.println("No se encontró BMP280, revisa el cableado o la dirección I2C (0x76/0x77)!");
    tempInt = "--"; presion = "--"; altitud = "--";
  } else {
    Serial.println("BMP280 inicializado OK.");
  }

  // ---- INICIA mDNS ----
  if (!MDNS.begin("motorhome")) {
    Serial.println("[mDNS] Error al iniciar mDNS responder!");
  } else {
    Serial.println("[mDNS] Respondiendo en: http://motorhome.local/");
    // También podés agregar servicios personalizados si querés, ej:
    // MDNS.addService("http", "tcp", 80);
  }

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", htmlPage());
  });
  server.on("/toggle4ch", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("ch")) {
      int ch = request->getParam("ch")->value().toInt();
      if (ch >= 1 && ch <= NUM_RELES) {
        toggleRele(ch);
      }
    }
    request->send(200, "text/plain", "OK");
  });
  server.on("/estados", HTTP_GET, [](AsyncWebServerRequest *request){
    leerEstadosReles();
    String json = "{";
    for (int i=0; i<NUM_RELES; i++) {
      json += "\"ch"+String(i+1)+"\":\""+estadosReles[i]+"\"";
      if (i<NUM_RELES-1) json += ",";
    }
    json += "}";
    request->send(200, "application/json", json);
  });
  server.on("/sensores", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    json += "\"tempInt\":\"" + tempInt + "\",";
    json += "\"tempExt\":\"" + tempExt + "\",";
    json += "\"presion\":\"" + presion + "\",";
    json += "\"altitud\":\"" + altitud + "\",";
    json += "\"airQ\":\"" + airQ + "\",";
    json += "\"mq2\":\"" + mq2 + "\"";
    json += "}";
    request->send(200, "application/json", json);
  });
  // ---- ENDPOINT GLOBAL /status PARA APP EXTERNA/FLUTTER ----
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    // Ejemplo de JSON, podés agregar o quitar lo que quieras
    String json = "{";
    json += "\"relays\":[" + String((digitalRead(5)==LOW)?"1":"0") + "," + String((digitalRead(12)==LOW)?"1":"0") + "," + String((digitalRead(14)==LOW)?"1":"0") + "," + String((digitalRead(27)==LOW)?"1":"0") + "," + String((digitalRead(26)==LOW)?"1":"0") + "," + String((digitalRead(25)==LOW)?"1":"0") + "],";
    json += "\"tankVals\":[" + String(tankVals[0]) + "," + String(tankVals[1]) + "," + String(tankVals[2]) + "],";
    json += "\"bat_soc\":\"" + bat_soc + "\",";
    json += "\"bat_volt\":\"" + bat_volt + "\",";
    json += "\"bat_current\":\"" + bat_current + "\",";
    json += "\"bat_temp\":\"" + bat_temp + "\",";
    json += "\"bat_cycles\":\"" + bat_cycles + "\",";
    json += "\"tempInt\":\"" + tempInt + "\",";
    json += "\"presion\":\"" + presion + "\",";
    json += "\"altitud\":\"" + altitud + "\",";
    json += "\"airQ\":\"" + airQ + "\",";
    json += "\"mq2\":\"" + mq2 + "\"";
    json += "}";
    request->send(200, "application/json", json);
  });

  server.begin();
  Serial.println("[DEBUG] Servidor HTTP iniciado.");
}

// --- LOOP PRINCIPAL: lee BMP280 cada 2s ---
void loop() {
  if (millis() - lastBmpUpdate > 2000) {
    lastBmpUpdate = millis();
    float t = bmp.readTemperature();
    float p = bmp.readPressure() / 100.0F;
    float a = bmp.readAltitude(1013.25);
    if (!isnan(t) && !isnan(p) && !isnan(a) && t > -50 && t < 100 && p > 200 && p < 1200) {
      tempInt = String(t, 1);
      presion = String(p, 0);
      altitud = String(a, 0);
    } else {
      tempInt = "--";
      presion = "--";
      altitud = "--";
    }
  }
}
