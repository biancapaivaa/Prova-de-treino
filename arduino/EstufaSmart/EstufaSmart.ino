#include <WiFiS3.h>
#include <DHT.h>
#include <Arduino_LED_Matrix.h>
#include "secrets.h"

// ===================== PINOS =====================
#define DHT_PIN       2
#define DHT_TYPE      DHT11
#define JANELA_PIN    3
#define BOTAO_PIN     4
#define LED_VERDE     5
#define LED_AMARELO   6
#define LED_VERMELHO  7
#define BUZZER_PIN    8
#define RELE_PIN      9
#define LDR_PIN       A0

// Se o seu módulo de relé for "ativo em LOW", troque para true.
const bool RELE_ATIVO_LOW = false;

// ===================== REGRAS =====================
const float TEMP_MIN = 20.0;
const float TEMP_MAX = 28.0;

// Como a prova não define numericamente "perto do limite" e "luz muito alta",
// adotamos estes valores de treino:
const float TEMP_ATENCAO_BAIXA = 21.0;
const float TEMP_ATENCAO_ALTA  = 27.0;
const float LUZ_MUITO_ALTA_PCT = 85.0;

const unsigned long INTERVALO_LEITURA_MS = 5000;
const unsigned long JANELA_ATENCAO_MS    = 20000;
const unsigned long JANELA_CRITICO_MS    = 60000;
const unsigned long TEMP_CRITICO_MS      = 30000;
const unsigned long BIPE_ATENCAO_MS      = 10000;
const unsigned long RECONEXAO_WIFI_MS    = 10000;

DHT dht(DHT_PIN, DHT_TYPE);
ArduinoLEDMatrix matrix;

// ===================== ESTADOS =====================
enum Estado {
  NORMAL,
  ATENCAO,
  CRITICO,
  RECONHECIDO,
  FALHA_SENSOR
};

Estado estadoAtual = NORMAL;

// ===================== TELEMETRIA =====================
struct Telemetria {
  unsigned long seq;
  float temp;
  float umid;
  bool janela;
  float luzPct;
  Estado estado;
};

const int BUFFER_SIZE = 20;
Telemetria fila[BUFFER_SIZE];
int filaInicio = 0;
int filaQuantidade = 0;

unsigned long seq = 0;
float tempAtual = 0;
float umidAtual = 0;
float luzAtualPct = 0;
bool janelaAberta = false;
int falhasDhtSeguidas = 0;

unsigned long ultimaLeitura = 0;
unsigned long janelaAbertaDesde = 0;
unsigned long tempForaDesde = 0;
unsigned long ultimoBipeAtencao = 0;
unsigned long ultimaTentativaWifi = 0;

bool janelaContando = false;
bool tempForaContando = false;
bool criticoReconhecido = false;

// ===================== MATRIZ 12x8 =====================
// Desenhos simples das letras N, A, C, R, F.
const uint8_t LETRA_N[8][12] = {
  {1,0,0,0,0,0,0,0,0,0,0,1},
  {1,1,0,0,0,0,0,0,0,0,0,1},
  {1,0,1,0,0,0,0,0,0,0,0,1},
  {1,0,0,1,0,0,0,0,0,0,0,1},
  {1,0,0,0,1,0,0,0,0,0,0,1},
  {1,0,0,0,0,1,0,0,0,0,0,1},
  {1,0,0,0,0,0,1,0,0,0,0,1},
  {1,0,0,0,0,0,0,1,0,0,0,1}
};

const uint8_t LETRA_A[8][12] = {
  {0,0,0,1,1,1,1,1,1,0,0,0},
  {0,0,1,0,0,0,0,0,0,1,0,0},
  {0,1,0,0,0,0,0,0,0,0,1,0},
  {0,1,0,0,0,0,0,0,0,0,1,0},
  {0,1,1,1,1,1,1,1,1,1,1,0},
  {0,1,0,0,0,0,0,0,0,0,1,0},
  {0,1,0,0,0,0,0,0,0,0,1,0},
  {0,1,0,0,0,0,0,0,0,0,1,0}
};

const uint8_t LETRA_C[8][12] = {
  {0,0,1,1,1,1,1,1,1,1,0,0},
  {0,1,0,0,0,0,0,0,0,0,1,0},
  {0,1,0,0,0,0,0,0,0,0,0,0},
  {0,1,0,0,0,0,0,0,0,0,0,0},
  {0,1,0,0,0,0,0,0,0,0,0,0},
  {0,1,0,0,0,0,0,0,0,0,0,0},
  {0,1,0,0,0,0,0,0,0,0,1,0},
  {0,0,1,1,1,1,1,1,1,1,0,0}
};

const uint8_t LETRA_R[8][12] = {
  {0,1,1,1,1,1,1,1,1,0,0,0},
  {0,1,0,0,0,0,0,0,0,1,0,0},
  {0,1,0,0,0,0,0,0,0,1,0,0},
  {0,1,1,1,1,1,1,1,1,0,0,0},
  {0,1,0,0,1,0,0,0,0,0,0,0},
  {0,1,0,0,0,1,0,0,0,0,0,0},
  {0,1,0,0,0,0,1,0,0,0,0,0},
  {0,1,0,0,0,0,0,1,0,0,0,0}
};

const uint8_t LETRA_F[8][12] = {
  {0,1,1,1,1,1,1,1,1,1,0,0},
  {0,1,0,0,0,0,0,0,0,0,0,0},
  {0,1,0,0,0,0,0,0,0,0,0,0},
  {0,1,1,1,1,1,1,1,0,0,0,0},
  {0,1,0,0,0,0,0,0,0,0,0,0},
  {0,1,0,0,0,0,0,0,0,0,0,0},
  {0,1,0,0,0,0,0,0,0,0,0,0},
  {0,1,0,0,0,0,0,0,0,0,0,0}
};

const char* nomeEstado(Estado e) {
  switch (e) {
    case NORMAL: return "NORMAL";
    case ATENCAO: return "ATENCAO";
    case CRITICO: return "CRITICO";
    case RECONHECIDO: return "RECONHECIDO";
    case FALHA_SENSOR: return "FALHA_SENSOR";
  }
  return "DESCONHECIDO";
}

void mostrarMatriz(Estado e) {
  switch (e) {
    case NORMAL:       matrix.renderBitmap(LETRA_N, 8, 12); break;
    case ATENCAO:      matrix.renderBitmap(LETRA_A, 8, 12); break;
    case CRITICO:      matrix.renderBitmap(LETRA_C, 8, 12); break;
    case RECONHECIDO:  matrix.renderBitmap(LETRA_R, 8, 12); break;
    case FALHA_SENSOR: matrix.renderBitmap(LETRA_F, 8, 12); break;
  }
}

void setRele(bool ligado) {
  if (RELE_ATIVO_LOW) {
    digitalWrite(RELE_PIN, ligado ? LOW : HIGH);
  } else {
    digitalWrite(RELE_PIN, ligado ? HIGH : LOW);
  }
}

void desligarLeds() {
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_AMARELO, LOW);
  digitalWrite(LED_VERMELHO, LOW);
}

void aplicarSaidas() {
  desligarLeds();
  noTone(BUZZER_PIN);

  if (estadoAtual == NORMAL) {
    digitalWrite(LED_VERDE, HIGH);
    setRele(false);
  }
  else if (estadoAtual == ATENCAO) {
    digitalWrite(LED_AMARELO, HIGH);
    setRele(false);
  }
  else if (estadoAtual == CRITICO) {
    digitalWrite(LED_VERMELHO, HIGH);
    setRele(true);
    tone(BUZZER_PIN, 2200);
  }
  else if (estadoAtual == RECONHECIDO) {
    digitalWrite(LED_VERMELHO, HIGH);
    setRele(true);
    // buzzer fica silencioso
  }
  else if (estadoAtual == FALHA_SENSOR) {
    digitalWrite(LED_AMARELO, HIGH);
    digitalWrite(LED_VERMELHO, HIGH);
    setRele(false);
  }

  mostrarMatriz(estadoAtual);
}

void bipAtencaoNaoBloqueante() {
  static bool bipLigado = false;
  static unsigned long bipInicio = 0;
  unsigned long agora = millis();

  if (estadoAtual != ATENCAO) {
    if (bipLigado) {
      noTone(BUZZER_PIN);
      bipLigado = false;
    }
    return;
  }

  if (!bipLigado && (agora - ultimoBipeAtencao >= BIPE_ATENCAO_MS)) {
    tone(BUZZER_PIN, 1800);
    bipLigado = true;
    bipInicio = agora;
    ultimoBipeAtencao = agora;
  }

  if (bipLigado && (agora - bipInicio >= 180)) {
    noTone(BUZZER_PIN);
    bipLigado = false;
  }
}

float lerLuzPct() {
  int bruto = analogRead(LDR_PIN);
  // UNO R4 trabalha tipicamente com ADC de 10 bits por padrão nesta configuração.
  float pct = (bruto / 1023.0) * 100.0;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return pct;
}

bool botaoPressionado() {
  static bool anterior = HIGH;
  static unsigned long mudouEm = 0;
  static bool confirmado = HIGH;

  bool atual = digitalRead(BOTAO_PIN);

  if (atual != anterior) {
    mudouEm = millis();
    anterior = atual;
  }

  if ((millis() - mudouEm) > 40 && atual != confirmado) {
    confirmado = atual;
    if (confirmado == LOW) return true;
  }
  return false;
}

void atualizarTemporizadores() {
  unsigned long agora = millis();

  if (janelaAberta) {
    if (!janelaContando) {
      janelaContando = true;
      janelaAbertaDesde = agora;
    }
  } else {
    janelaContando = false;
  }

  bool fora = (tempAtual < TEMP_MIN || tempAtual > TEMP_MAX);
  if (fora) {
    if (!tempForaContando) {
      tempForaContando = true;
      tempForaDesde = agora;
    }
  } else {
    tempForaContando = false;
  }
}

Estado calcularEstado() {
  unsigned long agora = millis();

  if (falhasDhtSeguidas >= 3) return FALHA_SENSOR;

  bool tempFora = (tempAtual < TEMP_MIN || tempAtual > TEMP_MAX);
  bool tempCriticaTempo = tempFora && tempForaContando &&
                          (agora - tempForaDesde > TEMP_CRITICO_MS);

  bool janelaAtencao = janelaAberta && janelaContando &&
                       (agora - janelaAbertaDesde > JANELA_ATENCAO_MS);

  bool janelaCritica = janelaAberta && janelaContando &&
                       (agora - janelaAbertaDesde > JANELA_CRITICO_MS);

  bool pertoLimite = !tempFora &&
                     (tempAtual <= TEMP_ATENCAO_BAIXA || tempAtual >= TEMP_ATENCAO_ALTA);

  bool luzMuitoAlta = luzAtualPct >= LUZ_MUITO_ALTA_PCT;

  bool condicaoCritica = tempCriticaTempo || janelaCritica;

  if (estadoAtual == RECONHECIDO) {
    if (!condicaoCritica && !tempFora && !janelaAberta) {
      criticoReconhecido = false;
      return NORMAL;
    }
    return RECONHECIDO;
  }

  if (criticoReconhecido && condicaoCritica) {
    return RECONHECIDO;
  }

  if (condicaoCritica) return CRITICO;
  if (pertoLimite || janelaAtencao || luzMuitoAlta) return ATENCAO;

  // Se temperatura acabou de sair da faixa mas ainda não completou 30 s,
  // tratamos como ATENÇÃO durante o período de confirmação.
  if (tempFora) return ATENCAO;

  return NORMAL;
}

void adicionarFila(const Telemetria &t) {
  if (filaQuantidade < BUFFER_SIZE) {
    int pos = (filaInicio + filaQuantidade) % BUFFER_SIZE;
    fila[pos] = t;
    filaQuantidade++;
  } else {
    // Buffer cheio: mantém as 20 leituras mais recentes.
    fila[filaInicio] = t;
    filaInicio = (filaInicio + 1) % BUFFER_SIZE;
  }
}

bool enviarHttp(const Telemetria &t) {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClient client;
  if (!client.connect(SERVER_HOST, SERVER_PORT)) {
    return false;
  }

  String body = "{";
  body += "\"device_id\":\"ESTUFA_SALA01\",";
  body += "\"placa\":\"Arduino UNO R4 WiFi\",";
  body += "\"seq\":" + String(t.seq) + ",";
  body += "\"temp\":" + String(t.temp, 1) + ",";
  body += "\"umid\":" + String(t.umid, 1) + ",";
  body += "\"janela\":" + String(t.janela ? "true" : "false") + ",";
  body += "\"luz_pct\":" + String(t.luzPct, 1) + ",";
  body += "\"estado\":\"" + String(nomeEstado(t.estado)) + "\"";
  body += "}";

  client.println("POST /api/telemetria HTTP/1.1");
  client.print("Host: ");
  client.print(SERVER_HOST);
  client.print(":");
  client.println(SERVER_PORT);
  client.println("Content-Type: application/json");
  client.print("Content-Length: ");
  client.println(body.length());
  client.println("Connection: close");
  client.println();
  client.print(body);

  unsigned long inicio = millis();
  while (client.connected() && !client.available() && millis() - inicio < 1500) {
    // espera curta do socket; não interfere no intervalo principal de 5 s
  }

  String statusLine = client.readStringUntil('\n');
  client.stop();

  return statusLine.indexOf("201") >= 0;
}

void reenviarFila() {
  while (filaQuantidade > 0 && WiFi.status() == WL_CONNECTED) {
    Telemetria &t = fila[filaInicio];
    if (!enviarHttp(t)) break;

    filaInicio = (filaInicio + 1) % BUFFER_SIZE;
    filaQuantidade--;
  }
}

void garantirWifi() {
  if (WiFi.status() == WL_CONNECTED) return;

  unsigned long agora = millis();
  if (agora - ultimaTentativaWifi < RECONEXAO_WIFI_MS) return;

  ultimaTentativaWifi = agora;
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void registrarEEnviar() {
  Telemetria t;
  t.seq = ++seq;
  t.temp = tempAtual;
  t.umid = umidAtual;
  t.janela = janelaAberta;
  t.luzPct = luzAtualPct;
  t.estado = estadoAtual;

  Serial.print("seq=");
  Serial.print(t.seq);
  Serial.print(" temp=");
  Serial.print(t.temp);
  Serial.print(" umid=");
  Serial.print(t.umid);
  Serial.print(" janela=");
  Serial.print(t.janela ? "ABERTA" : "FECHADA");
  Serial.print(" luz=");
  Serial.print(t.luzPct);
  Serial.print("% estado=");
  Serial.println(nomeEstado(t.estado));

  // Primeiro tenta limpar o buffer antigo; depois envia a leitura atual.
  reenviarFila();

  if (!enviarHttp(t)) {
    adicionarFila(t);
    Serial.print("Sem servidor/rede. Leituras no buffer: ");
    Serial.println(filaQuantidade);
  }
}

void fazerLeitura() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  janelaAberta = (digitalRead(JANELA_PIN) == LOW);
  luzAtualPct = lerLuzPct();

  if (isnan(t) || isnan(h)) {
    falhasDhtSeguidas++;
    Serial.print("Falha DHT11 #");
    Serial.println(falhasDhtSeguidas);

    if (falhasDhtSeguidas >= 3) {
      estadoAtual = FALHA_SENSOR;
      aplicarSaidas();
    }
    return;
  }

  falhasDhtSeguidas = 0;
  tempAtual = t;
  umidAtual = h;

  atualizarTemporizadores();
  estadoAtual = calcularEstado();
  aplicarSaidas();
  registrarEEnviar();
}

void setup() {
  Serial.begin(115200);

  pinMode(JANELA_PIN, INPUT_PULLUP);
  pinMode(BOTAO_PIN, INPUT_PULLUP);

  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_AMARELO, OUTPUT);
  pinMode(LED_VERMELHO, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELE_PIN, OUTPUT);

  dht.begin();
  matrix.begin();

  setRele(false);
  aplicarSaidas();

  Serial.println("EstufaSmart iniciando...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  ultimaTentativaWifi = millis();

  // Faz a primeira leitura imediatamente.
  ultimaLeitura = millis() - INTERVALO_LEITURA_MS;
}

void loop() {
  garantirWifi();

  if (botaoPressionado() && estadoAtual == CRITICO) {
    criticoReconhecido = true;
    estadoAtual = RECONHECIDO;
    aplicarSaidas();
    Serial.println("Alarme reconhecido localmente.");
  }

  bipAtencaoNaoBloqueante();

  unsigned long agora = millis();
  if (agora - ultimaLeitura >= INTERVALO_LEITURA_MS) {
    ultimaLeitura = agora;
    fazerLeitura();
  }

  // Quando a rede voltar, tenta reenviar o que ficou armazenado.
  if (WiFi.status() == WL_CONNECTED && filaQuantidade > 0) {
    reenviarFila();
  }
}
