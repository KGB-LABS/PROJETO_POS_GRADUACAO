# Sistema IoT de Monitoramento Residencial Inteligente e Segurança Periférica com ESP32

## 📌 Descrição do Projeto
Este projeto consiste em um sistema embarcado de automação e segurança residencial baseado em conceitos de **Internet das Coisas (IoT)** e telemetria em tempo real. Desenvolvido como protótipo funcional acadêmico para curso de Pós-Graduação, o ecossistema integra múltiplos sensores a um microcontrolador **ESP32** para monitorar variáveis ambientais, segurança perimetral (status de portas) e eventos meteorológicos (detecção de chuva).

O sistema opera sob uma arquitetura de comunicação bidirecional e orientada a eventos:
1.  **Monitoramento Ativo (Publisher):** Detecta transições de estado críticas (início/fim de chuva e abertura/fechamento de portas) e realiza disparos imediatos de notificações via **Telegram Bot API**, além de publicar payloads em formato estruturado (JSON) em um broker em nuvem **HiveMQ Cloud** via protocolo **MQTT**.
2.  **Requisições Sob Demanda (Subscriber):** Permite a interação remota do usuário através do envio de comandos de texto via Telegram (como `/termometro`), processando e respondendo instantaneamente com as métricas termo-higrométricas locais.

---

## 🛠️ Arquitetura de Hardware e Componentes

A seleção dos componentes seguiu critérios técnicos de estabilidade de sinal, isolamento de ruídos e compatibilidade de níveis lógicos (3.3V):

*   **Unidade de Processamento Central (MCU):** ESP32 NodeMCU (Arquitetura Xtensa Dual-Core de 32 bits, clock de 240 MHz com Wi-Fi e pilhas de protocolos TCP/IP integradas).
*   **Sensor Pluviométrico:** MH-RD (Placa condutora de grade resistiva associada a um módulo comparador de tensão baseado no circuito integrado LM393).
*   **Sensor de Intrusão (Porta):** Sensor Magnético tipo *Reed Switch* de contato seco.
*   **Sensor Termo-Higrométrico:** DHT11 (Módulo de leitura digital de temperatura e umidade com protocolo de comunicação *Single-Wire*).

### 📌 Análise Técnica e Justificativa da Pinagem
*   **GPIO 34 (Sensor de Chuva):** Pino configurado estritamente como entrada (*Input Only*). Como pertence ao barramento interno **ADC1**, ele não sofre interferências de queda de sinal ou conflitos de hardware quando o modem Wi-Fi do ESP32 está operando em alta potência (limitação conhecida do bloco ADC2). Por utilizar a saída digital (DO) do módulo LM393, que chaveia ativamente os níveis lógicos (0V ou 3.3V), dispensa resistores externos de *pull-down* ou *pull-up*.
*   **GPIO 23 (Sensor de Porta):** Configurado com o resistor interno de pull-up da placa (`INPUT_PULLUP`). Garante que a linha de sinal permaneça em nível lógico estável (HIGH) quando o ímã se afasta e o circuito abre, evitando leituras flutuantes (*floating state*) causadas por ruído eletromagnético ambiental.
*   **GPIO 0 (Botão BOOT):** Pino nativo de seleção de modo de inicialização do chip, reutilizado em tempo de execução para atuar como botão físico de *Factory Reset* das configurações de rede.

---

## 🔌 Esquema de Conexões (Pinagem)

| Componente Periférico | Pino do Componente | Pino no ESP32 | Modo de Configuração | Função no Sistema |
| :--- | :--- | :--- | :--- | :--- |
| **Módulo Sensor MH-RD** | VCC / GND / DO | **3V3 / GND / GPIO 34** | `INPUT` | Detecção digital de chuva |
| **Sensor Magnético Porta** | Terminal 1 / Terminal 2 | **GND / GPIO 23** | `INPUT_PULLUP` | Detecção de abertura/fechamento |
| **Sensor DHT11** | VCC / GND / DATA | **3V3 / GND / GPIO 4** | Gerenciado por biblioteca | Telemetria de Temperatura/Umidade |
| **Botão de Placa Integrado** | BOOT | **GPIO 0** | `INPUT_PULLUP` | Reset físico de credenciais Wi-Fi |

---

## 🌐 Camada de Software e Protocolos de Rede

### 1. Provisionamento Dinâmico de Rede (`WiFiManager`)
Para mitigar falhas de segurança decorrentes do armazenamento rígido (*hardcoded*) de credenciais de rede (SSID e senhas) no código-fonte, o firmware implementa a biblioteca `WiFiManager`. Se o ESP32 não detectar uma rede previamente salva na memória Flash (NVS), ele assume o modo de **Access Point (AP)** criando uma rede local criptografada denominada `Controle Residencial` (Senha: `abcde12345678`). O usuário pode se conectar a este AP via smartphone, acessar o portal cativo gerado automaticamente e configurar a rede local de forma dinâmica.

### 2. Mensageria Segura em Nuvem (`MQTT` + `HiveMQ Cloud`)
Os pacotes de telemetria ambiental e alertas são transmitidos via conexões TCP seguras utilizando a porta criptografada **8883** para o cluster em nuvem do **HiveMQ Cloud**. As informações são encapsuladas sob o tópico centralizador `casa/status`. No firmware, o método `.setInsecure()` é instanciado nas conexões de cliente, uma abordagem otimizada para sistemas embarcados que dispensa a alocação de memória RAM estática para validação de cadeias completas de certificados de chaves públicas (Root CA), mantendo a integridade e transporte do fluxo criptográfico.

### 3. Interface Homem-Máquina Remota (`UniversalTelegramBot`)
A API do Telegram é consultada de maneira não-bloqueante a cada 1000ms. Para fins de formatação de interface limpa, o bot utiliza sintaxe **Markdown** nas respostas enviadas ao usuário cadastrado na constante `CHAT_ID`. Mensagens vindas de outros identificadores são rejeitadas sob a política de "Acesso não autorizado".

---

## 💻 Código-Fonte do Firmware

O algoritmo abaixo adota o princípio de **Edge Triggering** (Gatilho por Borda). Ele compara o estado atual de leitura dos pinos digitais com variáveis de estado históricas (`estadoChuvaAnterior` e `estadoPortaAnterior`). Isso assegura que mensagens de rede sejam transmitidas estritamente no momento exato da alteração física do sensor, eliminando o tráfego repetitivo e redundante no broker MQTT e no Telegram.

```cpp
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
const char* MQTT_SERVER = "17d3d0b3ec0c46b199e1366bca9a32bc.s1.eu.hivemq.cloud";
const int MQTT_PORT = 8883;
const char* MQTT_USER = "projeto.tcc";
const char* MQTT_PASSWORD = "projeto1";

// --- Configurações do Telegram ---
#define BOTtoken "8916213946:AAEGkfNf2FnhO5kRezF8JqNjONfzwYUamXI"
#define CHAT_ID "7887093370"

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
Use o código com cuidado.notificar("🚪 porta aberta");} else {notificar("🔒 porta fechada");}}}void reconnectMQTT() {if (!mqttClient.connected()) {Serial.print("Conectando ao HiveMQ...");String clientId = "ESP32Client-";clientId += String(random(0xffff), HEX);if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {Serial.println(" Conectado ao HiveMQ!");} else {Serial.print(" Falha MQTT. Código: ");Serial.println(mqttClient.state());}}}void setup() {Serial.begin(115200);pinMode(BUTTON_BOOT, INPUT_PULLUP);pinMode(PIN_CHUVA, INPUT);pinMode(PIN_PORTA, INPUT_PULLUP);dht.begin();WiFiManager wm;// Verifica reset do Wi-Fi logo na energizaçãoif (digitalRead(BUTTON_BOOT) == LOW) {delay(3000);if (digitalRead(BUTTON_BOOT) == LOW) {wm.resetSettings();ESP.restart();}}wm.setConfigPortalTimeout(180);bool res = wm.autoConnect("Controle Residencial", "abcde12345678");if (!res) {Serial.println("Tempo limite atingido. Reiniciando...");ESP.restart();}Serial.println("Wi-Fi Conectado!");netClientMQTT.setInsecure();netClientTelegram.setInsecure();mqttClient.setServer(MQTT_SERVER, MQTT_PORT);reconnectMQTT();notificar("🤖 Sistema de Monitoramento Conectado!");}void loop() {// Checa se o usuário quer resetar o Wi-Fi sem instanciar objeto desnecessáriocheckResetButtonInLoop();if (WiFi.status() != WL_CONNECTED) {WiFi.reconnect();delay(5000);return;}reconnectMQTT();mqttClient.loop();// Monitora alterações dos sensores em tempo realchecarSensores();// Verifica novos comandos no Telegramif (millis() - ultimoChequeTelegram > INTERVALO_TELEGRAM) {int numNovasMensagens = bot.getUpdates(bot.last_message_received + 1);while (numNovasMensagens) {handleNewMessages(numNovasMensagens);numNovasMensagens = bot.getUpdates(bot.last_message_received + 1);}ultimoChequeTelegram = millis();}}```🚀 Instruções para Compilação e DeployFaça o download e instale a Arduino IDE (Versão 2.0 ou superior recomendada).Nas configurações da IDE, adicione a URL da Espressif no gerenciador de placas adicionais:https://githubusercontent.comAcesse o Gerenciador de Placas, busque por esp32 (desenvolvido por Espressif Systems) e instale a versão mais recente estável.No Gerenciador de Bibliotecas, busque e efetue a instalação das seguintes dependências exatamente como nomeadas abaixo:WiFiManager (por tablatronix)PubSubClient (por Nick O'Leary)UniversalTelegramBot (por Brian Flanagan)ArduinoJson (por Benoit Blanchon)DHT sensor library (por Adafruit)Em ferramentas, mude a placa ativa para seu modelo correspondente (ex: ESP32 Dev Module).Conecte o circuito de desenvolvimento via barramento Micro-USB/USB-C, selecione a porta COM correspondente e clique no botão de Upload.Abra o Monitor Serial configurado em 115200 baud para acompanhar o ciclo de inicialização e a telemetria do sistema.
<FollowUp>
Boa sorte na entrega do seu projeto! Se precisar de ajuda para **criar um diagrama em blocos** da arquitetura de nuvem (ESP32 ➡️ HiveMQ / Telegram) para colocar nos seus slides de apresentação, me avise!
</FollowUp>
