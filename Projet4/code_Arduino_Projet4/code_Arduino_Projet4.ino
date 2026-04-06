#include <Arduino.h>
#include <driver/dac.h>

// =========================
// Configuration matérielle
// =========================
static const int DAC_PIN = 25;      // DAC sur ESP32 classique
static const int CAP_PIN = 32;      // Mesure du condensateur variable

// =========================
// Paramètres DDS
// =========================
static const uint32_t SAMPLE_RATE = 40000;   // 40 kHz
static const uint16_t TABLE_SIZE  = 256;

hw_timer_t *timer = nullptr;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

volatile uint32_t phaseAcc = 0;
volatile uint32_t phaseStep = 0;   // réglé depuis loop()

uint8_t sineTable[TABLE_SIZE];

// =========================
// Réglage fréquence
// =========================
float freqMin = 50.0;      // Hz
float freqMax = 3000.0;    // Hz

// bornes de temps RC à calibrer chez toi
uint32_t capTimeMin = 20;      // us
uint32_t capTimeMax = 3000;    // us

// =========================
// Génération de la table sinus
// =========================
void buildSineTable() {
  for (int i = 0; i < TABLE_SIZE; i++) {
    float angle = 2.0f * PI * i / TABLE_SIZE;
    float s = sin(angle);

    // DAC 8 bits : 0..255
    // centre à 127, amplitude quasi max
    int v = (int)(127.5f + 120.0f * s);

    if (v < 0) v = 0;
    if (v > 255) v = 255;

    sineTable[i] = (uint8_t)v;
  }
}

// =========================
// Mise à jour fréquence DDS
// =========================
void setFrequency(float freqHz) {
  if (freqHz < 1.0f) freqHz = 1.0f;
  if (freqHz > 10000.0f) freqHz = 10000.0f;

  uint32_t newStep = (uint32_t)((freqHz * 4294967296.0) / SAMPLE_RATE);

  portENTER_CRITICAL(&timerMux);
  phaseStep = newStep;
  portEXIT_CRITICAL(&timerMux);
}

// =========================
// ISR timer : sort 1 sample
// =========================
void IRAM_ATTR onTimer() {
  portENTER_CRITICAL_ISR(&timerMux);

  phaseAcc += phaseStep;
  uint8_t index = phaseAcc >> 24;   // 8 bits de poids fort -> table 256 points
  uint8_t out = sineTable[index];

  dacWrite(DAC_PIN, out);

  portEXIT_CRITICAL_ISR(&timerMux);
}

// =========================
// Mesure du condensateur
// =========================
uint32_t measureCapChargeTimeUs() {
  // 1) Décharge du condensateur
  pinMode(CAP_PIN, OUTPUT);
  digitalWrite(CAP_PIN, LOW);
  delayMicroseconds(2000);

  // 2) Passe en entrée pour laisser le RC remonter
  pinMode(CAP_PIN, INPUT);

  uint32_t t0 = micros();

  // attend le passage à HIGH
  while (digitalRead(CAP_PIN) == LOW) {
    if ((micros() - t0) > 10000) {  // timeout sécurité
      return 10000;
    }
  }

  return micros() - t0;
}

// =========================
// Mapping mesure -> fréquence
// =========================
float mapCapTimeToFrequency(uint32_t tUs) {
  if (tUs < capTimeMin) tUs = capTimeMin;
  if (tUs > capTimeMax) tUs = capTimeMax;

  float x = (float)(tUs - capTimeMin) / (float)(capTimeMax - capTimeMin);

  // Inversion :
  // petit C -> petite constante de temps -> fréquence haute
  // grand C -> grande constante de temps -> fréquence basse
  float freq = freqMax - x * (freqMax - freqMin);

  return freq;
}

void setup() {
  Serial.begin(115200);

  buildSineTable();

  // DAC
  dac_output_enable(DAC_CHANNEL_1); // GPIO25 sur ESP32 classique

  // Timer matériel
  timer = timerBegin(0, 80, true);   // 80 MHz / 80 = 1 MHz -> 1 tick = 1 us
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 1000000 / SAMPLE_RATE, true);
  timerAlarmEnable(timer);

  setFrequency(500.0f);

  Serial.println("Generateur sinus ESP32 pret");
}

void loop() {
  static uint32_t lastRead = 0;

  // lecture fréquence toutes les 50 ms
  if (millis() - lastRead >= 50) {
    lastRead = millis();

    uint32_t tUs = measureCapChargeTimeUs();
    float freq = mapCapTimeToFrequency(tUs);

    setFrequency(freq);

    Serial.print("tRC = ");
    Serial.print(tUs);
    Serial.print(" us   |   f = ");
    Serial.print(freq, 1);
    Serial.println(" Hz");
  }
}