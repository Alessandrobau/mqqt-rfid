// IoT - Leitor RFID com MQTT (ESP32 + MFRC522) - Wokwi
#include <WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <time.h>

// ====== TROQUE PELO SEU NOME (sem espacos, ex.: joao) ======
const char* NOME = "alebau";

// ====== Pinos ======
#define SS_PIN  5
#define RST_PIN 22
#define SCK_PIN 18
#define MOSI_PIN 23
#define MISO_PIN 19
#define LED_PIN 2

// ====== Rede / broker ======
const char* WIFI_SSID = "Wokwi-GUEST";
const char* BROKER = "test.mosquitto.org";
const int   PORTA = 1883;

// ====== Objetos ======
MFRC522 mfrc522(SS_PIN, RST_PIN);
WiFiClient espClient;
PubSubClient client(espClient);

// ====== Topicos ======
String tMac, tCadastro, tAcesso, tLista, tCmd;
String macStr;

// ====== Lista de cartoes ======
#define MAX_CARTOES 10
String cartoes[MAX_CARTOES];
int nCartoes = 0;

// ====== Estado ======
bool modoCadastro = false;
unsigned long inicioCadastro = 0;
const unsigned long TEMPO_CADASTRO = 15000;

String ultimoUid = "";
unsigned long ultimoTempo = 0;

// LED nao bloqueante
unsigned long ledAcesoAte = 0;
unsigned long ledPiscaInicio = 0;
bool ledPisca3 = false;

// ---------- Utilidades ----------
String horaLocal() {
  struct tm t;
  if (!getLocalTime(&t, 100)) return "00:00:00";
  char buf[16];
  strftime(buf, sizeof(buf), "%H:%M:%S", &t);
  return String(buf);
}

String uidParaString(MFRC522::Uid *uid) {
  String s = "";
  for (byte i = 0; i < uid->size; i++) {   // funciona com 4 e 7 bytes
    if (i > 0) s += " ";
    if (uid->uidByte[i] < 0x10) s += "0";
    s += String(uid->uidByte[i], HEX);
  }
  s.toUpperCase();
  return s;
}

int buscaCartao(const String &uid) {
  for (int i = 0; i < nCartoes; i++) if (cartoes[i] == uid) return i;
  return -1;
}

// ---------- LED ----------
void ledAcender2s() { ledAcesoAte = millis() + 2000; }
void ledPiscar3x()  { ledPisca3 = true; ledPiscaInicio = millis(); }

void atualizaLed() {
  unsigned long agora = millis();
  bool nivel = LOW;
  if (ledPisca3) {
    unsigned long fase = (agora - ledPiscaInicio) / 100;   // 3 pulsos de 100ms
    if (fase < 6) nivel = (fase % 2 == 0);
    else ledPisca3 = false;
  } else if (agora < ledAcesoAte) {
    nivel = HIGH;
  } else if (modoCadastro) {
    nivel = ((agora / 250) % 2 == 0);                      // piscando
  }
  digitalWrite(LED_PIN, nivel);
}

// ---------- MQTT ----------
void publicaJson(const String &topico, const String &json, bool retain = false) {
  client.publish(topico.c_str(), json.c_str(), retain);
  Serial.println("[PUB] " + topico + " -> " + json);
}

void publicaMac() {
  String json = "{\"mac\":\"" + macStr + "\",\"ip\":\"" + WiFi.localIP().toString() +
                "\",\"hora\":\"" + horaLocal() + "\"}";
  publicaJson(tMac, json, true);   // com retain
}

void publicaCadastro(const String &uid, const char* status) {
  String json = "{\"mac\":\"" + macStr + "\",\"uid\":\"" + uid +
                "\",\"status\":\"" + status + "\",\"hora\":\"" + horaLocal() + "\"}";
  publicaJson(tCadastro, json);
}

void publicaAcesso(const String &uid, const char* resultado) {
  String json = "{\"mac\":\"" + macStr + "\",\"uid\":\"" + uid +
                "\",\"resultado\":\"" + resultado + "\",\"hora\":\"" + horaLocal() + "\"}";
  publicaJson(tAcesso, json);
}

void publicaLista() {
  String json = "{\"mac\":\"" + macStr + "\",\"tags\":[";
  for (int i = 0; i < nCartoes; i++) {
    if (i > 0) json += ",";
    json += "\"" + cartoes[i] + "\"";
  }
  json += "],\"hora\":\"" + horaLocal() + "\"}";
  publicaJson(tLista, json);
}

void callback(char* topic, byte* payload, unsigned int len) {
  String msg = "";
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
  msg.trim();
  Serial.println("[CMD] " + msg);

  String up = msg;
  up.toUpperCase();

  if (up == "CADASTRO") {
    modoCadastro = true;
    inicioCadastro = millis();
    Serial.println("[MODO] CADASTRO (15 s)");
  } else if (up.startsWith("REMOVER")) {
    String uid = up.substring(7);
    uid.trim();
    int idx = buscaCartao(uid);
    if (idx >= 0) {
      for (int i = idx; i < nCartoes - 1; i++) cartoes[i] = cartoes[i + 1];
      nCartoes--;
      publicaCadastro(uid, "removida");
    } else {
      publicaCadastro(uid, "nao_encontrada");
    }
  } else if (up == "LISTAR") {
    publicaLista();
  } else {
    Serial.println("[CMD] comando desconhecido");
  }
}

// ---------- Conexoes ----------
void conectaWiFi() {
  Serial.print("Conectando ao Wi-Fi");
  WiFi.begin(WIFI_SSID, "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("Wi-Fi OK | IP: " + WiFi.localIP().toString());
}

void conectaMQTT() {
  while (!client.connected()) {
    Serial.print("Conectando ao broker...");
    String id = "esp32-rfid-" + String(NOME) + "-" + String(random(0xffff), HEX);
    if (client.connect(id.c_str())) {
      Serial.println(" OK");
      client.subscribe(tCmd.c_str());
      Serial.println("Inscrito em " + tCmd);
      publicaMac();                       // apresenta-se ao conectar
    } else {
      Serial.print(" falhou, rc=");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

// ---------- Tratamento do cartao ----------
void tratarCartao(const String &uid) {
  Serial.println("[UID] " + uid);

  if (modoCadastro) {
    modoCadastro = false;
    if (buscaCartao(uid) >= 0) {
      publicaCadastro(uid, "ja_cadastrada");
    } else if (nCartoes >= MAX_CARTOES) {
      publicaCadastro(uid, "lista_cheia");
    } else {
      cartoes[nCartoes++] = uid;
      publicaCadastro(uid, "cadastrada");
    }
    Serial.println("[MODO] NORMAL");
    return;
  }

  if (buscaCartao(uid) >= 0) {
    publicaAcesso(uid, "liberado");
    ledAcender2s();
  } else {
    publicaAcesso(uid, "negado");
    ledPiscar3x();
  }
}

// ---------- Arduino ----------
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  mfrc522.PCD_Init();
  Serial.println("Leitor RFID iniciado");

  conectaWiFi();
  macStr = WiFi.macAddress();
  Serial.println("MAC: " + macStr);

  // R3: hora por NTP (UTC-3)
  configTime(-3 * 3600, 0, "pool.ntp.org");

  Serial.print("Sincronizando hora (NTP)");
  struct tm t;
  int tentativas = 0;
  while (!getLocalTime(&t, 500) && tentativas < 20) {
    Serial.print(".");
    tentativas++;
  }
  Serial.println();
  Serial.println("Hora: " + horaLocal());
  String base = "sis1a/" + String(NOME) + "/rfid/";
  tMac = base + "mac";
  tCadastro = base + "cadastro";
  tAcesso = base + "acesso";
  tLista = base + "lista";
  tCmd = base + "cmd";

  client.setServer(BROKER, PORTA);
  client.setCallback(callback);
  client.setBufferSize(512);
  conectaMQTT();
}

void loop() {
  // reconexao automatica
  if (WiFi.status() != WL_CONNECTED) conectaWiFi();
  if (!client.connected()) conectaMQTT();
  client.loop();

  // tempo esgotado do modo cadastro
  if (modoCadastro && millis() - inicioCadastro >= TEMPO_CADASTRO) {
    modoCadastro = false;
    publicaCadastro("", "tempo_esgotado");
    Serial.println("[MODO] NORMAL (tempo esgotado)");
  }

  atualizaLed();

  // leitura do cartao
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    String uid = uidParaString(&mfrc522.uid);
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();

    // ignora o mesmo cartao em menos de 2 s
    if (uid == ultimoUid && millis() - ultimoTempo < 2000) return;
    ultimoUid = uid;
    ultimoTempo = millis();

    tratarCartao(uid);
  }
}
