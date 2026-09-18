#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <WiFiManager.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <DHT.h>

// --- Definição dos Pinos ---
#define BUTTON_BOOT 0     // Botão BOOT da placa para Reset do Wi-Fi
#define PIN_CHUVA 34      // Pino digital do Sensor de Chuva
#define PIN_PORTA 23      // Pino digital do Sensor de Porta (Magnético)
#define PIN_DHT 4         // Pino do Sensor de Temperatura/Umidade
#define DHTTYPE DHT11     // Mude para DHT22 se estiver usando o DHT22

// --- Configurações do HiveMQ Cloud ---
const char* MQTT_SERVER = "SEUSERVER.s1.eu.hivemq.cloud";
const int MQTT_PORT = 8883;
const char* MQTT_USER = "SEU USUÁRIO";
const char* MQTT_PASSWORD = "SUA SENHA";

// --- Configurações do Telegram ---
#define BOTtoken "SEU TOKEN DO BOT"
#define CHAT_ID "SEU CHAT ID"

// Tópicos MQTT
const char* TOPIC_STATUS = "casa/status";

// Inicialização dos sensores e conexões
DHT dht(PIN_DHT, DHTTYPE);

WiFiClientSecure netClientMQTT;
WiFiClientSecure netClientTelegram;

PubSubClient mqttClient(netClientMQTT);
UniversalTelegramBot bot(BOTtoken, netClientTelegram);

// Variáveis de estado dos sensores
int estadoChuvaAnterior = -1;
int estadoPortaAnterior = -1;

unsigned long ultimoChequeTelegram = 0;
const unsigned long INTERVALO_TELEGRAM = 1000; // Checa novas mensagens a cada 1 segundo

// Função para enviar mensagem ao Telegram e publicar no HiveMQ
void notificar(String mensagem) {
  Serial.println("Notificação: " + mensagem);
  bot.sendMessage(CHAT_ID, mensagem, "Markdown");
  if (mqttClient.connected()) {
    mqttClient.publish(TOPIC_STATUS, mensagem.c_str());
  }
}

// Botão BOOT para resetar as credenciais Wi-Fi (Pressione por 3 segundos)
void checkResetButtonInLoop() {
  if (digitalRead(BUTTON_BOOT) == LOW) {
    Serial.println("\nBotão BOOT pressionado! Segure por 3 segundos para resetar o Wi-Fi...");
    delay(3000);
    if (digitalRead(BUTTON_BOOT) == LOW) {
      Serial.println("Limpando configurações de Wi-Fi...");
      WiFiManager wm;
      wm.resetSettings();
      delay(1000);
      ESP.restart();
    }
  }
}

// Processa mensagens recebidas pelo Bot do Telegram
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    
    // Responde apenas ao usuário autorizado
    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "Acesso não autorizado.", "");
      continue;
    }

    String text = bot.messages[i].text;

    if (text == "/termometro" || text == "/termometro@SeuBot") {
      float temp = dht.readTemperature();
      float umi = dht.readHumidity();

      if (isnan(temp) || isnan(umi)) {
        bot.sendMessage(CHAT_ID, "⚠️ Erro ao ler os dados do sensor de temperatura!", "");
      } else {
        // 1. Envia resposta formatada para o Telegram
        String resposta = "🌡️ *Leitura Térmica*\n\n";
        resposta += "Temperatura: " + String(temp, 1) + " °C\n";
        resposta += "Umidade: " + String(umi, 1) + " %";
        bot.sendMessage(CHAT_ID, resposta, "Markdown");

        // 2. Publica os dados de leitura no HiveMQ Cloud
        if (mqttClient.connected()) {
          String payloadMQTT = "{\"temperatura\":" + String(temp, 1) + ",\"umidade\":" + String(umi, 1) + "}";
          mqttClient.publish(TOPIC_STATUS, payloadMQTT.c_str());
          Serial.println("Publicado no HiveMQ: " + payloadMQTT);
        }
      }
    }
  }
}

// Monitora alterações dos sensores de Chuva e Porta
void checarSensores() {
  // Sensor de chuva (LOW indica presença de água no módulo)
  int leituraChuva = digitalRead(PIN_CHUVA);
  if (leituraChuva != estadoChuvaAnterior) {
    estadoChuvaAnterior = leituraChuva;
    if (leituraChuva == LOW) {
      notificar("🌧️ chovendo");
    } else {
      notificar("☀️ sem chuva no momento");
    }
  }

  // Sensor de porta (HIGH = Aberta com pull-up, LOW = Fechada)
  int leituraPorta = digitalRead(PIN_PORTA);
  if (leituraPorta != estadoPortaAnterior) {
    estadoPortaAnterior = leituraPorta;
    if (leituraPorta == HIGH) {
      notificar("🚪 porta aberta");
    } else {
      notificar("🔒 porta fechada");
    }
  }
}

void reconnectMQTT() {
  if (!mqttClient.connected()) {
    Serial.print("Conectando ao HiveMQ...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);

    if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
      Serial.println(" Conectado ao HiveMQ!");
    } else {
      Serial.print(" Falha MQTT. Código: ");
      Serial.println(mqttClient.state());
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_BOOT, INPUT_PULLUP);
  pinMode(PIN_CHUVA, INPUT);
  pinMode(PIN_PORTA, INPUT_PULLUP);

  dht.begin();

  WiFiManager wm;
  
  // Verifica reset do Wi-Fi logo na energização
  if (digitalRead(BUTTON_BOOT) == LOW) {
    delay(3000);
    if (digitalRead(BUTTON_BOOT) == LOW) {
      wm.resetSettings();
      ESP.restart();
    }
  }

  wm.setConfigPortalTimeout(180);

  bool res = wm.autoConnect("Controle Residencial", "abcde12345678");
  if (!res) {
    Serial.println("Tempo limite atingido. Reiniciando...");
    ESP.restart();
  }

  Serial.println("Wi-Fi Conectado!");

  netClientMQTT.setInsecure();
  netClientTelegram.setInsecure();

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);

  reconnectMQTT();
  notificar("🤖 *Sistema de Monitoramento Conectado!*");
}

void loop() {
  // Checa se o usuário quer resetar o Wi-Fi sem instanciar objeto desnecessário
  checkResetButtonInLoop();

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    delay(5000);
    return;
  }

  reconnectMQTT();
  mqttClient.loop();

  // Monitora alterações dos sensores em tempo real
  checarSensores();

  // Verifica novos comandos no Telegram
  if (millis() - ultimoChequeTelegram > INTERVALO_TELEGRAM) {
    int numNovasMensagens = bot.getUpdates(bot.last_message_received + 1);
    while (numNovasMensagens) {
      handleNewMessages(numNovasMensagens);
      numNovasMensagens = bot.getUpdates(bot.last_message_received + 1);
    }
    ultimoChequeTelegram = millis();
  }
}
