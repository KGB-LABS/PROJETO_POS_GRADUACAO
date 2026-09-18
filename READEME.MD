# Sistema de Monitoramento Pluviométrico Automatizado com ESP32 e Sensor MH-RD

## 📌 Descrição do Projeto
Este projeto consiste em um sistema embarcado de monitoramento em tempo real para detecção e classificação de precipitação pluviométrica (chuva). Desenvolvido como requisito para a conclusão do módulo de Sistemas Embarcados/IoT do curso de Pós-Graduação, o sistema utiliza o microcontrolador **ESP32** integrado ao sensor de chuva **MH-RD**.

O propósito principal é capturar variações de condutividade elétrica geradas pela presença de água em uma placa sensora, processar esses dados analógicos via Conversores Analógico-Digitais (ADC) e classificar a intensidade da chuva em três níveis: **Tempo Seco**, **Chuva Fraca/Garoa** e **Chuva Forte**.

---

## 🛠️ Especificações do Hardware

O projeto utiliza componentes de mercado selecionados pelo custo-benefício, estabilidade de sinal e compatibilidade lógica:

*   **Microcontrolador:** ESP32 NodeMCU (38 pinos)
*   **Sensor de Chuva:** MH-RD (Placa condutora + Módulo comparador baseado no CI LM393)
*   **Tensão de Operação:** 3.3V DC (Garante compatibilidade direta com os níveis lógicos do ESP32)

### 📌 Características Importantes da GPIO 34
Para este projeto, foi selecionada a **GPIO 34** do ESP32. Esta escolha baseia-se em critérios técnicos fundamentais:
1.  **Exclusividade de Entrada:** A GPIO 34 é um pino do tipo *Input Only* (apenas entrada), ideal para sensores, economizando pinos bidirecionais para atuadores.
2.  **Conversão Analógica (ADC1):** Pertence ao bloco ADC1 do ESP32, permitindo leituras analógicas estáveis mesmo quando o Wi-Fi do microcontrolador está ativo (evitando os conflitos conhecidos do bloco ADC2).
3.  **Independência de Pull-up/down:** Por não possuir resistores internos na arquitetura do chip, ela depende puramente do sinal ativo enviado pelo circuito integrado LM393 do sensor, evitando flutuações de leitura (*floating*).

---

## 🔌 Esquema de Ligação (Pinagem)

| Sensor MH-RD (Módulo LM393) | ESP32 NodeMCU | Descrição |
| :--- | :--- | :--- |
| **VCC** | **3V3** | Alimentação positiva (3.3V) |
| **GND** | **GND** | Referência de aterramento comum |
| **AO (Analog Out)** | **GPIO 34** | Saída Analógica para medição de intensidade |
| **DO (Digital Out)** | *Não conectado* | Opcional (Usa limite físico via potenciômetro) |

---

## 💻 Implementação do Firmware

O código foi desenvolvido em C++ utilizando a **Arduino IDE**. O algoritmo realiza a leitura do ADC de 12 bits (resolução de 0 a 4095) e aplica uma lógica de zonas de corte (*thresholds*) para classificação do estado climático.

```cpp
/**
 * @file ProjetoChuvaESP32.ino
 * @author Seu Nome / RA
 * @brief Sistema de classificação pluviométrica para Pós-Graduação
 * @version 1.0
 */

// Definição de Hardware
const int PINO_SENSOR = 34; // GPIO 34 (ADC1_CH6)

// Parâmetros de Calibração (Thresholds)
const int LIMITE_CHUVA_FORTE = 1500;
const int LIMITE_TEMPO_SECO   = 3500;

void setup() {
  // Inicialização da comunicação serial para telemetria
  Serial.begin(115200);
  
  // Configuração explícita do pino como entrada
  pinMode(PINO_SENSOR, INPUT); 
  
  Serial.println("[INFO] Sistema de Monitoramento Pluviométrico Inicializado.");
}

void loop() {
  // Leitura do valor bruto do ADC (0 a 4095)
  int leituraAnalogica = analogRead(PINO_SENSOR);
  
  Serial.print("[DATA] Leitura Bruta ADC: ");
  Serial.print(leituraAnalogica);

  // Lógica de classificação baseada na condutividade (Lógica Inversa)
  // Nota: Muita água = Alta condutividade = Baixa tensão/leitura no ADC.
  if (leituraAnalogica < LIMITE_CHUVA_FORTE) {
    Serial.println(" -> Status: CHUVA FORTE! 🌧️");
  } 
  else if (leituraAnalogica >= LIMITE_CHUVA_FORTE && leituraAnalogica < LIMITE_TEMPO_SECO) {
    Serial.println(" -> Status: CHUVA FRACA / GAROA 🌦️");
  } 
  else {
    Serial.println(" -> Status: TEMPO SECO ☀️");
  }

  // Intervalo de amostragem (1 segundo)
  delay(1000); 
}
```

---

## 📈 Resultados e Metodologia de Teste
Devido à natureza resistiva do sensor MH-RD, o comportamento do sinal apresenta uma **lógica inversa**:
*   **Placa Seca:** A resistência entre as trilhas é máxima, gerando uma leitura próxima do limite superior do ADC (**~4095**).
*   **Presença de Água:** A água atua como condutor elétrico, reduzindo a resistência proporcionalmente à sua saturação na placa. Isso faz com que a tensão lida caia progressivamente em direção a **0**.

Para validação acadêmica, o potenciômetro físico do módulo LM393 foi calibrado em bancada para garantir a estabilidade do comparador interno, mitigando ruídos de transição de sinal em ambientes de umidade relativa alta.

---

## 🚀 Como Executar o Projeto

1.  Instale a **Arduino IDE** (versão 2.0 ou superior recomendada).
2.  Nas preferências da IDE, adicione a URL da placa ESP32 nas preferências de placas adicionais.
3.  Instale o pacote de placas **esp32** da *Espressif Systems* através do Gerenciador de Placas.
4.  Selecione a placa correspondente (ex: `ESP32 Dev Module`).
5.  Conecte o ESP32 ao computador via cabo Micro-USB/USB-C de dados.
6.  Abra o Monitor Serial e configure a velocidade para **115200 baud**.
7.  Compile e faça o *Upload* do código.
