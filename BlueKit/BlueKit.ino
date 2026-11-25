// BlueKit – ESP32 bridge: AwoX/EGLO Connect-Z BLE lamp -> Apple Home (HomeKit)
// Libraries: HomeSpan 2.x, NimBLE-Arduino 2.x  |  Board: ESP32 Dev Module
// Partition Scheme: "Huge APP (3MB No OTA/1MB SPIFFS)"

#include <Arduino.h>
#include "HomeSpan.h"
#include <NimBLEDevice.h>
#include "mbedtls/aes.h"
#include "esp_random.h"
#include "secrets.h"   // copy secrets.example.h -> secrets.h and fill in

// ---- Configuration (from secrets.h) ----
const char*    WIFI_SSID = WIFI_SSID_S;
const char*    WIFI_PASS = WIFI_PASS_S;
const char*    LAMP_MAC  = LAMP_MAC_S;
const char*    MESH_NAME = MESH_NAME_S;
const char*    MESH_PASS = MESH_PASS_S;
const uint16_t MESH_ID   = MESH_ID_S;

static const NimBLEUUID SVC_UUID   ("00010203-0405-0607-0809-0a0b0c0d1910");
static const NimBLEUUID STATUS_UUID("00010203-0405-0607-0809-0a0b0c0d1911");
static const NimBLEUUID CMD_UUID   ("00010203-0405-0607-0809-0a0b0c0d1912");
static const NimBLEUUID PAIR_UUID  ("00010203-0405-0607-0809-0a0b0c0d1914");

uint8_t sessionKey[16];
NimBLEClient* client = nullptr;

// ---------- Telink crypto ----------
// AES-128-ECB with byte-reversed key, input and output
void telinkEncrypt(const uint8_t key[16], const uint8_t in[16], uint8_t out[16]) {
  uint8_t k[16], v[16], r[16];
  for (int i = 0; i < 16; i++) { k[i] = key[15 - i]; v[i] = in[15 - i]; }
  mbedtls_aes_context ctx; mbedtls_aes_init(&ctx);
  mbedtls_aes_setkey_enc(&ctx, k, 128);
  mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_ENCRYPT, v, r);
  mbedtls_aes_free(&ctx);
  for (int i = 0; i < 16; i++) out[i] = r[15 - i];
}

void nameXorPass(uint8_t out[16]) {
  uint8_t n[16] = {0}, p[16] = {0};
  memcpy(n, MESH_NAME, strlen(MESH_NAME)); memcpy(p, MESH_PASS, strlen(MESH_PASS));
  for (int i = 0; i < 16; i++) out[i] = n[i] ^ p[i];
}

uint8_t reflect8(uint8_t x) { uint8_t r = 0; for (int i = 0; i < 8; i++) if (x & (1 << i)) r |= 1 << (7 - i); return r; }

// CRC-8, poly 0x36, init 0, reflected in/out
uint8_t crc8z(const uint8_t* d, size_t n) {
  uint8_t crc = 0;
  for (size_t k = 0; k < n; k++) {
    crc ^= reflect8(d[k]);
    for (int i = 0; i < 8; i++) crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x36) : (uint8_t)(crc << 1);
  }
  return reflect8(crc);
}

// ---------- Mesh login ----------
bool login(NimBLERemoteService* svc) {
  auto* pair = svc->getCharacteristic(PAIR_UUID);
  auto* status = svc->getCharacteristic(STATUS_UUID);
  if (!pair || !status) return false;

  uint8_t sessRand[8]; esp_fill_random(sessRand, 8);
  uint8_t np[16]; nameXorPass(np);
  uint8_t key[16] = {0}; memcpy(key, sessRand, 8);
  uint8_t enc[16]; telinkEncrypt(key, np, enc);

  uint8_t pkt[17]; pkt[0] = 0x0C; memcpy(pkt + 1, sessRand, 8); memcpy(pkt + 9, enc, 8);
  if (!pair->writeValue(pkt, sizeof(pkt), true)) return false;

  uint8_t one = 0x01; status->writeValue(&one, 1, true);
  delay(100);

  std::string reply = pair->readValue();
  if (reply.size() < 9 || (uint8_t)reply[0] != 0x0D) return false;   // 0x0E = wrong credentials

  uint8_t rnd[16]; memcpy(rnd, sessRand, 8); memcpy(rnd + 8, reply.data() + 1, 8);
  telinkEncrypt(np, rnd, sessionKey);
  return true;
}

// ---------- Connect-Z command (17 bytes) ----------
bool sendZ(NimBLERemoteService* svc, const uint8_t* cmd, size_t clen) {
  uint8_t block[16] = {0};
  uint8_t len = 2 + clen;
  block[1] = len;
  block[2] = MESH_ID >> 8; block[3] = MESH_ID & 0xFF;
  memcpy(block + 4, cmd, clen);
  block[0] = crc8z(block + 1, 1 + len);

  uint8_t pkt[17]; pkt[0] = 0x00;
  telinkEncrypt(sessionKey, block, pkt + 1);
  auto* c = svc->getCharacteristic(CMD_UUID);
  return c && c->writeValue(pkt, sizeof(pkt), true);
}

// The lamp accepts only one BLE connection: connect, send, disconnect
bool sendToLamp(bool setPower, bool on, bool setLevel, int percent) {
  for (int attempt = 0; attempt < 3; attempt++) {
    if (!client->connect(NimBLEAddress(LAMP_MAC, BLE_ADDR_PUBLIC))) { delay(500); continue; }
    bool ok = false;
    auto* svc = client->getService(SVC_UUID);
    if (svc && login(svc)) {
      delay(300);
      ok = true;
      if (setPower) {
        uint8_t cmd[] = {0x01, 0x06, 0x00, (uint8_t)(on ? 1 : 0)};
        ok &= sendZ(svc, cmd, sizeof(cmd));
        delay(150);
      }
      if (setLevel) {
        uint8_t v = map(constrain(percent, 1, 100), 1, 100, 3, 254);
        uint8_t cmd[] = {0x01, 0x08, 0x00, 0x04, v, 0x02, 0x00, 0x00, 0x00, 0x00};
        ok &= sendZ(svc, cmd, sizeof(cmd));
        delay(150);
      }
      delay(200);
    }
    client->disconnect();
    if (ok) return true;
    delay(500);
  }
  Serial.println("Lamp not reachable");
  return false;
}

// ---------- HomeKit ----------
struct AwoxLamp : Service::LightBulb {
  SpanCharacteristic* power;
  SpanCharacteristic* level;
  bool pendingPower = false, pendingLevel = false;

  AwoxLamp() : Service::LightBulb() {
    power = new Characteristic::On(false);
    level = new Characteristic::Brightness(100);
    level->setRange(1, 100, 1);
  }

  // Called by HomeKit: just remember the change and answer quickly
  boolean update() override {
    if (power->updated()) pendingPower = true;
    if (level->updated()) pendingLevel = true;
    return true;
  }

  // BLE work happens here, outside the HomeKit request
  void loop() override {
    if (!pendingPower && !pendingLevel) return;
    bool on = power->getVal();
    int pct = level->getVal();
    Serial.printf("HomeKit: %s, %d%%\n", on ? "ON" : "OFF", pct);
    sendToLamp(pendingPower, on, pendingLevel && on, pct);
    pendingPower = pendingLevel = false;
  }
};

void setup() {
  Serial.begin(115200);
  NimBLEDevice::init("");
  client = NimBLEDevice::createClient();

  homeSpan.setWifiCredentials(WIFI_SSID, WIFI_PASS);
  homeSpan.setPairingCode(PAIRING_CODE_S);   // custom setup code for the Home app (set in secrets.h)
  homeSpan.begin(Category::Lighting, "BlueKit Lamp");

  new SpanAccessory();
    new Service::AccessoryInformation();
      new Characteristic::Identify();
      new Characteristic::Name(ACCESSORY_NAME_S);
    new AwoxLamp();
}

void loop() {
  homeSpan.poll();
}
