/*
 * TRIWAVE - TERMINAL STATION (ESP32)
 * ------------------------------------------------------------------
 * Receives a confirmed pickup request over SMS (SIM800L) or WiFi ->
 * LED + buzzer alert -> driver types their 3-digit ID on the matrix
 * keypad -> ID is checked against the driver list (drivers.h) -> the
 * driver ID is sent back to the school station -> IDLE.
 *
 * (The SIM800L / SMS version is replaced for now. The message strings
 *  are unchanged, so the cellular link can be swapped back in later.)
 *
 * BOARD:     "ESP32 by Espressif Systems" core (2.x or 3.x both work)
 * LIBRARIES (Arduino Library Manager):
 *   - Adafruit ILI9341   (install the dependencies when asked:
 *                         Adafruit GFX Library + Adafruit BusIO)
 *   - PubSubClient       (Nick O'Leary - needed for LINK_MQTT)
 *   - Keypad             (Mark Stanley, Alexander Brevig)
 *
 * WIRING (change the pins below if you wire differently):
 *   4x4 keypad rows : GPIO32, 33, 25, 26
 *   4x4 keypad cols : GPIO21, 14, 22, 19
 *   Buzzer (active) : GPIO16 -> buzzer(+), buzzer(-) -> GND
 *   Green LED       : GPIO13 -> 220 ohm -> LED -> GND
 *   SIM800L         : TXD -> GPIO34,  RXD <- GPIO5,  GND -> GND (common),
 *                     VCC -> its own 4.0 V / 2 A supply (not the ESP32 pin)
 *   2.4" TFT (ILI9341, SPI) - SAME pins as the school station:
 *     VCC -> 3.3V     GND -> GND      LED (backlight) -> 3.3V
 *     CS  -> GPIO17   RESET -> GPIO4  DC/RS -> GPIO27
 *     SDI (MOSI) -> GPIO23            SCK -> GPIO18
 *     SDO (MISO) -> not connected     touch pins -> not connected
 *
 * LINK MODE (LINK_MODE near the top of the code):
 *   LINK_HTTP = internet through your website on Vercel (see triwave-web).
 *       Each station joins its own phone hotspot (WIFI_SSID / WIFI_PASS) and
 *       talks to WEB_HOST using the secret WEB_KEY. No broker, no libraries.
 *   LINK_MQTT = internet, for stations at DIFFERENT locations. Each station
 *       joins its own phone hotspot (WIFI_SSID / WIFI_PASS) and both connect
 *       to the same cloud MQTT broker (MQTT_HOST / USER / PASS). Needs data
 *       on each hotspot phone. Set the broker values identically in both
 *       sketches.
 *   LINK_SMS  = SIM800L SMS. Needs a SIM with load, 2G/GSM coverage, the GSM
 *       antenna, a 4.0 V / 2 A supply, and PEER_NUMBER set.
 *   LINK_UDP  = both boards on the SAME hotspot, for bench testing.
 *       (iPhone hotspot: turn ON "Maximize Compatibility" - 2.4 GHz only.)
 *
 * KEYPAD:  0-9 = 3-digit driver ID (e.g. 023),  * = clear,  # = confirm
 *           (typing 23 and pressing # is treated as 023)
 *
 * DRIVER DATA: drivers.h (keep a copy in this sketch folder AND the school
 *              sketch folder - both use the same file).
 *
 * MESSAGE FORMAT (plain text):
 *   School -> Terminal : REQUEST,001,CONFIRMED
 *   Terminal -> School : DRIVER,001,023     (request ID, driver ID)
 */

// ======================= LINK MODE ================================
#define LINK_UDP   0    // local WiFi: both boards on the SAME hotspot (bench testing)
#define LINK_SMS   1    // SIM800L SMS
#define LINK_MQTT  2    // internet via a cloud MQTT broker
#define LINK_HTTP  3    // internet via a website on Vercel (simplest for far-apart sites)
#define LINK_MODE  LINK_HTTP

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Keypad.h>
#if LINK_MODE == LINK_UDP
#include <WiFi.h>
#include <WiFiUdp.h>
#elif LINK_MODE == LINK_MQTT
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>        // Library Manager: "PubSubClient" by Nick O'Leary
#elif LINK_MODE == LINK_HTTP
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#endif
#include "drivers.h"          // driver data set (3-digit IDs)

// =================== USER SETTINGS (edit these) ===================
// ---- SIM800L (SMS) - used when LINK_MODE is LINK_SMS ----
#define PEER_NUMBER       "+639XXXXXXXXX"   // SIM number of the SCHOOL station
#define MODEM_RX          34                // ESP32 pin <- SIM800L TXD
#define MODEM_TX          5                // ESP32 pin -> SIM800L RXD
#define MODEM_BAUD        9600
#define NETWORK_WAIT_MS   30000UL

// ---- WiFi - used by LINK_UDP and LINK_MQTT ----
// Each station uses the hotspot of the phone AT ITS OWN LOCATION. Turn the
// phone's Personal Hotspot on (iPhone: enable "Maximize Compatibility" -
// the ESP32 only does 2.4 GHz). The second network is an optional backup.
#define WIFI_SSID         "Harry's iPhone"
#define WIFI_PASS         "helloworld"
#define WIFI_SSID2        ""                // optional backup hotspot ("" = none)
#define WIFI_PASS2        ""
#define WIFI_CONNECT_TIMEOUT_MS 20000UL

// ---- Local WiFi (LINK_UDP): both boards on the SAME hotspot ----
#define PEER_IP_FIXED     ""                // "" = find the other board automatically
#define LINK_PORT         4210
#define SEND_TIMEOUT_MS   800UL

// ---- Internet (LINK_MQTT): boards at DIFFERENT locations ----
// Make a free HiveMQ Cloud cluster (or any MQTT broker with TLS) and fill
// in its host, username and password. Use the SAME broker values in both
// sketches. The prefix must be the same in both and unique to you.
#define MQTT_HOST         "xxxxxxxxxxxxxxxx.s1.eu.hivemq.cloud"
#define MQTT_PORT         8883
#define MQTT_USER         "triwave"
#define MQTT_PASS         "change-me"
#define MQTT_PREFIX       "triwave-bsu"
#define MQTT_CLIENT_ID    "triwave-terminal"
#define MQTT_TOPIC_IN     MQTT_PREFIX "/to_terminal"
#define MQTT_TOPIC_OUT    MQTT_PREFIX "/to_school"
#define MQTT_ACK_TIMEOUT_MS 3000UL

// ---- Website (LINK_HTTP): both stations talk through a site on Vercel ----
#define WEB_HOST          "your-project.vercel.app"   // your Vercel address (no https://)
#define WEB_KEY           "triwave_secret_key"        // same secret as API_KEY in Vercel
#define WEB_ME            "terminal"
#define WEB_PEER          "school"
#define POLL_MS           5000UL                     // how often to check for messages
#define HTTP_SHOULD_POLL  true

#define TFT_SCK           18
#define TFT_MOSI          23
#define TFT_CS            17
#define TFT_DC            27
#define TFT_RST           4

#define PIN_BUZZER        16
#define PIN_LED           13

#define DRIVER_DIGITS         3             // driver ID digits (023, 134, ...)
#define ALERT_SHOW_MS         2000UL        // "NEW REQUEST" screen time before entry screen
#define BUZZER_BEEP_MS        150UL         // one beep (on) / gap (off)
#define BUZZER_CYCLE_MS       5000UL        // 3 beeps, then silence until next cycle
#define BUZZER_MAX_MS         30000UL       // buzzer never sounds longer than this
#define MAX_SEND_ATTEMPTS     3
#define RETRY_DELAY_MS        2000UL
#define DONE_SHOW_MS          3000UL

// ==================================================================

SPIClass tftSPI(VSPI);                      // default SPI pins, same as the school station
Adafruit_ILI9341 tft(&tftSPI, TFT_DC, TFT_CS, TFT_RST);

// ------------------------- keypad ---------------------------------
const byte ROWS = 4, COLS = 4;
char keyMap[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};
byte rowPins[ROWS] = { 32, 33, 25, 26 };
byte colPins[COLS] = { 21, 14, 22, 19 };
Keypad keypad = Keypad(makeKeymap(keyMap), rowPins, colPins, ROWS, COLS);

// ------------------------- system states --------------------------
enum TerminalState {
  T_IDLE,     // STATE: no active request
  T_ALERT,    // STATE 4: terminal notified
  T_ENTRY,    // STATE 5: waiting for driver number
  T_NOTICE    // STATE 7/8: assigned + sent, then back to IDLE
};

TerminalState state = T_IDLE;
uint32_t stateStart = 0;
bool needsRender = true;
uint32_t tempUntil = 0;

bool activeRequest = false;
String activeId = "";
String entry = "";
String lastCompletedId = "", lastReply = "";   // for duplicate handling

bool buzzerActive = false;
uint32_t buzzerStart = 0;

// ======================= DISPLAY (2.4" TFT 240x320) ===============
// Landscape = 320 x 240. Line 1 is a colored header bar, lines 2-4 are
// body text. Text size is picked automatically so each line fits.
// All display output goes through lcdShow().
const int16_t TFT_W = 320;
const int16_t HEADER_H = 40;
const int16_t BAND_H = 56;
const int16_t MARGIN = 10;

String shown[4] = { "\x01", "\x01", "\x01", "\x01" };   // what is on screen now

uint16_t barColor(const String &t) {
  if (t.indexOf("FAIL") >= 0 || t.indexOf("ERROR") >= 0 || t.indexOf("INVALID") >= 0 ||
      t.indexOf("NOT FOUND") >= 0 || t.indexOf("NO LINK") >= 0) return ILI9341_RED;
  if (t.indexOf("NEW REQUEST") >= 0 || t.indexOf("ENTER DRIVER") >= 0) return ILI9341_ORANGE;
  if (t.indexOf("ASSIGNED") >= 0 || t.indexOf("READY") >= 0 || t.indexOf("DRIVER #") >= 0) return ILI9341_DARKGREEN;
  return ILI9341_NAVY;
}

uint8_t pickSize(size_t len, uint8_t maxSize) {
  for (uint8_t s = maxSize; s >= 2; s--)
    if ((int)len * 6 * s <= TFT_W - 2 * MARGIN) return s;
  return 2;
}

String fitText(const String &s, uint8_t size) {
  size_t maxChars = (TFT_W - 2 * MARGIN) / (6 * size);
  return s.length() > maxChars ? s.substring(0, maxChars) : s;
}

void lcdShow(String l1, String l2 = "", String l3 = "", String l4 = "") {
  String lines[4] = { l1, l2, l3, l4 };
  for (uint8_t r = 0; r < 4; r++) {
    if (lines[r] == shown[r]) continue;            // unchanged -> no redraw, no flicker
    shown[r] = lines[r];

    if (r == 0) {                                  // header bar
      tft.fillRect(0, 0, TFT_W, HEADER_H, barColor(lines[0]));
      tft.setTextSize(2);
      tft.setTextColor(ILI9341_WHITE);
      tft.setCursor(MARGIN, (HEADER_H - 16) / 2);
      tft.print(fitText(lines[0], 2));
    } else {                                       // body line
      int16_t y = HEADER_H + (r - 1) * BAND_H + 8;
      tft.fillRect(0, y, TFT_W, BAND_H, ILI9341_BLACK);
      if (lines[r].length() == 0) continue;
      uint8_t size = pickSize(lines[r].length(), r == 1 ? 4 : 2);
      tft.setTextSize(size);
      tft.setTextColor(r == 1 ? ILI9341_WHITE : ILI9341_LIGHTGREY);
      tft.setCursor(MARGIN, y + (BAND_H - 8 * size) / 2);
      tft.print(fitText(lines[r], size));
    }
  }
}

void initializeLCD() {
  tftSPI.begin(TFT_SCK, -1, TFT_MOSI, -1);         // SCK, MISO (unused), MOSI, SS (handled by lib)
  tft.begin(20000000);
  tft.setRotation(1);                              // landscape; use 3 if upside down
  tft.fillScreen(ILI9341_BLACK);
  lcdShow("TRIWAVE TERMINAL", "STARTING...");
}

void showTemp(const String &l1, const String &l2, uint32_t ms) {
  lcdShow(l1, l2);
  tempUntil = millis() + ms;
  if (tempUntil == 0) tempUntil = 1;
}

// ------------------------- small string helpers -------------------
int splitCSV(const String &s, String *out, int maxParts) {
  int n = 0, start = 0;
  while (n < maxParts) {
    int c = s.indexOf(',', start);
    if (c < 0) { out[n++] = s.substring(start); break; }
    out[n++] = s.substring(start, c);
    start = c + 1;
  }
  for (int i = 0; i < n; i++) out[i].trim();
  return n;
}

bool isAllDigits(const String &s) {
  if (s.length() == 0) return false;
  for (size_t i = 0; i < s.length(); i++) if (!isdigit((unsigned char)s[i])) return false;
  return true;
}

// ======================= LINK LAYER ===============================
// LINK_SMS  -> SIM800L SMS (needs 2G/GSM coverage)
// LINK_HTTP -> internet via a website on Vercel (sites can be far apart)
// LINK_MQTT -> internet via a cloud MQTT broker
// LINK_UDP  -> both boards on the same hotspot (bench testing)
// The rest of the sketch only calls initializeLink(), sendMessage(text) and
// linkPoll(body), so both modes behave the same for the state machine.
#define LINK_TITLE "TRIWAVE TERMINAL"

// small queue of received messages (shared by both link modes)
#define INBOX_SIZE 6
String inbox[INBOX_SIZE];
uint8_t inHead = 0, inTail = 0;

void inboxPush(const String &s) {
  uint8_t n = (inTail + 1) % INBOX_SIZE;
  if (n == inHead) return;               // full -> drop
  inbox[inTail] = s;
  inTail = n;
}

bool inboxPop(String &s) {
  if (inHead == inTail) return false;
  s = inbox[inHead];
  inHead = (inHead + 1) % INBOX_SIZE;
  return true;
}

#if LINK_MODE == LINK_SMS
// ----------------------- SIM800L (AT commands / SMS) --------------
HardwareSerial &modem = Serial2;
String rxLine, capture, cmtSender;
bool capturing = false, awaitingBody = false, cellularReady = false;
uint8_t modemDeadCount = 0;

String digitsOnly(const String &s) {
  String o;
  for (size_t i = 0; i < s.length(); i++) if (isdigit((unsigned char)s[i])) o += s[i];
  return o;
}

// compares phone numbers by their last 9 digits (ignores +63 / 0 prefix)
bool sameNumber(const String &a, const String &b) {
  String x = digitsOnly(a), y = digitsOnly(b);
  if (x.length() > 9) x = x.substring(x.length() - 9);
  if (y.length() > 9) y = y.substring(y.length() - 9);
  return x.length() > 0 && x == y;
}

void handleModemLine(const String &line) {
  if (awaitingBody) {                     // line after "+CMT:" is the SMS text
    awaitingBody = false;
    if (sameNumber(cmtSender, PEER_NUMBER)) inboxPush(line);   // ignore strangers
    return;
  }
  if (line.startsWith("+CMT:")) {
    int a = line.indexOf('"');
    int b = line.indexOf('"', a + 1);
    cmtSender = (a >= 0 && b > a) ? line.substring(a + 1, b) : "";
    awaitingBody = true;
  }
}

void feedChar(char c) {
  if (capturing && capture.length() < 400) capture += c;
  if (c == '\n') {
    rxLine.trim();
    if (rxLine.length()) handleModemLine(rxLine);
    rxLine = "";
  } else if (c != '\r') {
    if (rxLine.length() < 250) rxLine += c;
  }
}

void modemPoll() {
  while (modem.available()) feedChar((char)modem.read());
}

bool waitFor(const char *token, uint32_t timeoutMs) {
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    modemPoll();
    if (capture.indexOf(token) >= 0) return true;
    if (capture.indexOf("ERROR") >= 0) return false;
    delay(2);
  }
  return false;
}

bool atCommand(const char *cmd, const char *expect = "OK", uint32_t timeoutMs = 2000) {
  modemPoll();
  capture = "";
  capturing = true;
  modem.print(cmd);
  modem.print('\r');
  bool ok = waitFor(expect, timeoutMs);
  capturing = false;
  return ok;
}

bool modemRegistered() {
  if (!atCommand("AT+CREG?")) return false;
  return capture.indexOf(",1") >= 0 || capture.indexOf(",5") >= 0;   // home / roaming
}

// Signal strength 0-31 (higher is better, 10+ is usable), or -1 if unknown.
int signalQuality() {
  if (!atCommand("AT+CSQ")) return -1;
  int i = capture.indexOf("+CSQ:");
  if (i < 0) return -1;
  int v = capture.substring(i + 5).toInt();           // "+CSQ: 18,0" -> 18
  return (v >= 0 && v <= 31) ? v : -1;
}

void modemConfigure() {
  atCommand("ATE0");                          // echo off
  atCommand("AT+CMGF=1");                     // SMS text mode
  atCommand("AT+CSCS=\"GSM\"");
  atCommand("AT+CNMI=2,2,0,0,0");             // push incoming SMS straight to serial (+CMT)
}

// Remote-deployment watchdog: every 30 s check the modem is alive, still
// configured (it forgets everything after a brown-out reset) and registered.
void maintainModem() {
  static uint32_t last = 0;
  if (millis() - last < 30000UL) return;
  last = millis();

  if (!atCommand("AT", "OK", 1000)) {
    cellularReady = false;
    Serial.println("[MODEM] not responding");
    return;
  }
  if (!atCommand("AT+CNMI?") || capture.indexOf("+CNMI: 2,2") < 0) {
    Serial.println("[MODEM] settings lost -> reconfiguring");
    modemConfigure();
  }
  bool reg = modemRegistered();
  if (reg != cellularReady) Serial.println(reg ? "[MODEM] network back" : "[MODEM] network lost");
  cellularReady = reg;
}

void initializeLink() {
  lcdShow(LINK_TITLE, "STARTING MODEM...");
  if (String(PEER_NUMBER).indexOf('X') >= 0) {          // placeholder number still in the sketch
    lcdShow("SET PHONE NUMBER", "EDIT PEER_NUMBER", "IN THE SKETCH");
    delay(3000);
  }
  modem.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(1000);

  bool alive = false;
  for (int i = 0; i < 10 && !alive; i++) alive = atCommand("AT", "OK", 1000);
  if (!alive) {
    cellularReady = false;
    Serial.println("[MODEM] no response");
    lcdShow("MODEM NOT FOUND", "CHECK WIRING", "AND POWER");
    delay(2500);
    return;                                   // maintainModem() keeps retrying
  }

  modemConfigure();
  atCommand("AT+CMGD=1,4", "OK", 5000);       // clear old stored SMS

  lcdShow(LINK_TITLE, "FINDING NETWORK...");
  cellularReady = false;
  uint32_t t0 = millis();
  while (millis() - t0 < NETWORK_WAIT_MS) {
    if (modemRegistered()) { cellularReady = true; break; }
    delay(1000);
  }
  Serial.println(cellularReady ? "[MODEM] network ready" : "[MODEM] no network");

  if (cellularReady) {
    int q = signalQuality();
    String sig = (q >= 0) ? (String("SIGNAL ") + String(q) + "/31") : String("");
    lcdShow("NETWORK READY", sig);
  } else {
    lcdShow("NO NETWORK", "CHECK SIM + ANTENNA");
  }
  delay(1500);
}

bool sendSMS(const char *number, const String &text) {
  modemPoll();
  capture = "";
  capturing = true;
  modem.print("AT+CMGS=\"");
  modem.print(number);
  modem.print("\"\r");
  bool ok = waitFor(">", 5000);
  if (ok) {
    modem.print(text);
    modem.write(0x1A);                        // Ctrl+Z = send
    capture = "";
    ok = waitFor("+CMGS:", 30000);
  } else {
    modem.write(0x1B);                        // ESC = cancel
  }
  capturing = false;
  Serial.printf("[SMS] to %s : %s -> %s\n", number, text.c_str(), ok ? "OK" : "FAILED");
  return ok;
}

bool sendMessage(const String &text) {
  if (text.length() == 0 || text.length() > 150) return false;
  if (!cellularReady) cellularReady = modemRegistered();      // quick re-check
  if (!cellularReady) { Serial.println("[SMS] no network"); return false; }
  return sendSMS(PEER_NUMBER, text);
}

// Returns true and fills 'body' when an SMS from the other station is waiting.
bool linkPoll(String &body) {
  modemPoll();
  maintainModem();
  return inboxPop(body);
}

#else
// ----------------------- WiFi (shared by UDP and MQTT) ------------
bool linkReady = false;
String connectSsid = WIFI_SSID, connectPass = WIFI_PASS;

bool wifiUp() { return WiFi.status() == WL_CONNECTED; }

// iPhones often name the hotspot with a curly apostrophe; treat it as a straight one.
String normalizeSsid(String s) {
  s.replace("\xE2\x80\x99", "'");
  s.replace("\xE2\x80\x98", "'");
  return s;
}

// Scans and picks the first configured hotspot that is visible.
bool pickNetwork() {
  const char *ssids[2]  = { WIFI_SSID, WIFI_SSID2 };
  const char *passes[2] = { WIFI_PASS, WIFI_PASS2 };
  bool ok = false;
  int n = WiFi.scanNetworks();
  for (int k = 0; k < 2 && !ok; k++) {
    if (strlen(ssids[k]) == 0) continue;
    String want = normalizeSsid(ssids[k]);
    for (int i = 0; i < n; i++) {
      if (normalizeSsid(WiFi.SSID(i)) == want) {
        connectSsid = WiFi.SSID(i);
        connectPass = passes[k];
        ok = true;
        break;
      }
    }
  }
  WiFi.scanDelete();
  if (!ok) { connectSsid = WIFI_SSID; connectPass = WIFI_PASS; }   // not seen -> try the first one anyway
  return ok;
}

// Joins WiFi at boot (blocking, progress shown on the TFT).
void connectWifiBlocking() {
  lcdShow(LINK_TITLE, "SEARCHING WIFI...", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  bool seen = pickNetwork();
  Serial.printf("[LINK] hotspot %s: %s\n", seen ? "found" : "NOT found (trying anyway)", connectSsid.c_str());
  lcdShow(LINK_TITLE, "CONNECTING WIFI", connectSsid);
  WiFi.begin(connectSsid.c_str(), connectPass.c_str());
  uint32_t t0 = millis();
  while (!wifiUp() && millis() - t0 < WIFI_CONNECT_TIMEOUT_MS) delay(250);
}

// Re-joins if the hotspot dropped.
void reconnectWifiIfNeeded() {
  static uint32_t lastTry = 0;
  if (wifiUp() || millis() - lastTry < 5000) return;
  lastTry = millis();
  Serial.println("[LINK] WiFi down, reconnecting...");
  WiFi.disconnect();
  WiFi.begin(connectSsid.c_str(), connectPass.c_str());
}

#if LINK_MODE == LINK_UDP
// ----------------------- local WiFi (UDP) -------------------------
// Both boards join the same hotspot and talk over UDP. Every message is
// acknowledged by the other side ("ACK:" + the same text) and the sender
// retries until the ACK arrives, so sendMessage() still means "delivered".
// The other board's IP is found automatically (broadcast, then unicast).
WiFiUDP udp;
IPAddress peerIP(0, 0, 0, 0);            // other board's IP (learned automatically)
IPAddress lastIp(0, 0, 0, 0);
bool udpStarted = false;
String ackWanted = "";
bool ackGot = false;

bool peerKnown() { return !(peerIP == IPAddress(0, 0, 0, 0)); }

// Read every waiting UDP packet: ACKs resolve a pending send, data is acked + queued.
void pumpUdp() {
  while (udp.parsePacket() > 0) {
    char buf[200];
    int len = udp.read(buf, sizeof(buf) - 1);
    IPAddress from = udp.remoteIP();
    if (len <= 0) continue;
    buf[len] = '\0';
    if (from == WiFi.localIP()) continue;            // ignore our own broadcast
    String s(buf);

    if (s.startsWith("ACK:")) {
      if (ackWanted.length() && s.substring(4) == ackWanted) { ackGot = true; peerIP = from; }
      continue;
    }
    if (!strlen(PEER_IP_FIXED)) peerIP = from;       // remember who we talk to
    udp.beginPacket(from, LINK_PORT);                // acknowledge
    udp.print("ACK:" + s);
    udp.endPacket();
    inboxPush(s);
  }
}

// Keeps the WiFi + UDP socket alive (reconnects if the hotspot drops).
void maintainLink() {
  reconnectWifiIfNeeded();
  if (wifiUp()) {
    IPAddress ip = WiFi.localIP();
    if (!udpStarted || !(ip == lastIp)) {
      udp.stop();
      udpStarted = udp.begin(LINK_PORT);
      lastIp = ip;
      Serial.printf("[LINK] connected, my IP %s\n", ip.toString().c_str());
    }
    linkReady = udpStarted;
  } else {
    linkReady = false;
    udpStarted = false;
  }
}

void initializeLink() {
  connectWifiBlocking();
  if (strlen(PEER_IP_FIXED)) peerIP.fromString(PEER_IP_FIXED);
  maintainLink();

  if (linkReady) lcdShow("WIFI READY", "MY IP ADDRESS:", WiFi.localIP().toString(), connectSsid);
  else           lcdShow("NO LINK", "WIFI FAILED", "RETRYING IN BACKGROUND");
  delay(3000);                                       // time to read the IP
}

// Sends one message and waits for the other board's ACK.
bool sendMessage(const String &text) {
  maintainLink();
  if (!linkReady || text.length() == 0 || text.length() > 150) return false;

  ackWanted = text;
  ackGot = false;
  for (int tries = 0; tries < 2 && !ackGot; tries++) {
    if (peerKnown()) {
      udp.beginPacket(peerIP, LINK_PORT);
      udp.print(text);
      udp.endPacket();
    } else {                                         // don't know the peer yet -> broadcast
      udp.beginPacket(WiFi.broadcastIP(), LINK_PORT);
      udp.print(text);
      udp.endPacket();
      udp.beginPacket(IPAddress(255, 255, 255, 255), LINK_PORT);
      udp.print(text);
      udp.endPacket();
    }
    uint32_t t0 = millis();
    while (!ackGot && millis() - t0 < SEND_TIMEOUT_MS) { pumpUdp(); delay(5); }
  }
  ackWanted = "";

  if (!ackGot && !strlen(PEER_IP_FIXED)) peerIP = IPAddress(0, 0, 0, 0);   // peer moved? rediscover
  Serial.printf("[LINK] tx: %s -> %s\n", text.c_str(), ackGot ? "OK" : "FAILED");
  return ackGot;
}

// Returns true and fills 'body' when a message from the other board is waiting.
bool linkPoll(String &body) {
  maintainLink();
  if (!linkReady) return false;
  pumpUdp();
  return inboxPop(body);
}

#elif LINK_MODE == LINK_HTTP
// ----------------------- website on Vercel (HTTPS) ----------------
// Each station joins its own hotspot and talks to the same website:
//   send    -> POST https://WEB_HOST/api/send?to=<other station>   (200 = stored)
//   receive -> GET  https://WEB_HOST/api/receive?me=<this station> (every POLL_MS)
// A 200 reply to the POST is the delivery confirmation.
WiFiClientSecure httpsClient;

// Asks the website for a waiting message. Returns the HTTP code (200 message, 204 none).
int fetchOnce() {
  HTTPClient http;
  http.setTimeout(6000);
  http.setReuse(false);
  http.begin(httpsClient, String("https://") + WEB_HOST + "/api/receive?me=" + WEB_ME);
  http.addHeader("x-api-key", WEB_KEY);
  int code = http.GET();
  if (code == 200) {
    String s = http.getString();
    s.trim();
    if (s.length()) inboxPush(s);
  }
  http.end();
  return code;
}

void maintainLink() {
  reconnectWifiIfNeeded();
  linkReady = wifiUp();
}

void initializeLink() {
  if (String(WEB_HOST).indexOf("your-project") >= 0) {   // placeholder address still in the sketch
    lcdShow("SET WEB HOST", "EDIT WEB_HOST", "IN THE SKETCH");
    delay(3000);
  }
  connectWifiBlocking();
  httpsClient.setInsecure();                           // encrypted; server certificate not verified (prototype)
  maintainLink();
  if (!linkReady) {
    lcdShow("NO LINK", "WIFI FAILED", "RETRYING IN BACKGROUND");
    delay(3000);
    return;
  }
  lcdShow(LINK_TITLE, "CONTACTING WEBSITE...");
  int code = fetchOnce();
  if (code == 200 || code == 204) lcdShow("ONLINE READY", "WEBSITE OK", connectSsid);
  else if (code == 401)           lcdShow("WRONG WEB KEY", "CHECK WEB_KEY", "AND API_KEY");
  else                            lcdShow("NO WEBSITE", "CHECK WEB_HOST", "CODE " + String(code));
  delay(2500);
}

// Sends one message to the other station through the website.
bool sendMessage(const String &text) {
  maintainLink();
  if (!linkReady || text.length() == 0 || text.length() > 150) return false;
  HTTPClient http;
  http.setTimeout(8000);
  http.setReuse(false);
  http.begin(httpsClient, String("https://") + WEB_HOST + "/api/send?to=" + WEB_PEER);
  http.addHeader("x-api-key", WEB_KEY);
  http.addHeader("Content-Type", "text/plain");
  int code = http.POST(text);
  http.end();
  Serial.printf("[WEB] tx: %s -> %d\n", text.c_str(), code);
  return code == 200;
}

// Returns true and fills 'body' when a message for this station is waiting.
bool linkPoll(String &body) {
  maintainLink();
  if (inboxPop(body)) return true;
  static uint32_t last = 0;
  if (!linkReady || !(HTTP_SHOULD_POLL) || millis() - last < POLL_MS) return false;
  last = millis();
  fetchOnce();
  return inboxPop(body);
}

#else
// ----------------------- internet (MQTT over TLS) -----------------
// Each board joins ITS OWN hotspot and connects out to the same cloud MQTT
// broker, so the two stations can be in different places. Messages travel
// on two topics (<prefix>/to_school and <prefix>/to_terminal). Every
// message is acknowledged ("ACK:" + the same text) and the sender retries
// until the ACK arrives, so sendMessage() still means "delivered".
WiFiClientSecure secureClient;
PubSubClient mqtt(secureClient);
String ackWanted = "";
bool ackGot = false;

void onMqttMessage(char *topic, byte *payload, unsigned int length) {
  String s;
  for (unsigned int i = 0; i < length && i < 150; i++) s += (char)payload[i];

  if (s.startsWith("ACK:")) {
    if (ackWanted.length() && s.substring(4) == ackWanted) ackGot = true;
    return;
  }
  String ack = "ACK:" + s;                           // acknowledge, then hand over to the app
  mqtt.publish(MQTT_TOPIC_OUT, ack.c_str());
  inboxPush(s);
}

// Keeps WiFi and the broker connection alive; also pumps incoming messages.
void maintainLink() {
  static uint32_t lastTry = 0;
  reconnectWifiIfNeeded();
  if (!wifiUp()) { linkReady = false; return; }

  if (mqtt.connected()) {
    mqtt.loop();
    linkReady = true;
    return;
  }
  linkReady = false;
  if (lastTry && millis() - lastTry < 10000UL) return;
  lastTry = millis();

  Serial.println("[LINK] connecting to MQTT broker...");
  if (mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)) {
    mqtt.subscribe(MQTT_TOPIC_IN);
    linkReady = true;
    Serial.println("[LINK] MQTT connected");
  } else {
    Serial.printf("[LINK] MQTT failed, rc=%d\n", mqtt.state());
  }
}

void initializeLink() {
  if (String(MQTT_HOST).indexOf("xxxx") >= 0) {       // placeholder broker still in the sketch
    lcdShow("SET MQTT HOST", "EDIT MQTT_HOST", "IN THE SKETCH");
    delay(3000);
  }
  connectWifiBlocking();

  secureClient.setInsecure();                         // encrypted; server certificate not verified (prototype)
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);
  mqtt.setBufferSize(256);
  mqtt.setKeepAlive(30);
  mqtt.setSocketTimeout(4);

  if (!wifiUp()) {
    lcdShow("NO LINK", "WIFI FAILED", "RETRYING IN BACKGROUND");
    delay(3000);
    return;
  }
  lcdShow(LINK_TITLE, "CONNECTING ONLINE...", connectSsid);
  uint32_t t0 = millis();
  while (!linkReady && millis() - t0 < 15000UL) { maintainLink(); delay(200); }

  if (linkReady) lcdShow("ONLINE READY", "WIFI + BROKER OK", connectSsid);
  else           lcdShow("NO BROKER", "CHECK MQTT SETTINGS", "RETRYING IN BACKGROUND");
  delay(2500);
}

// Publishes one message and waits for the other station's ACK.
bool sendMessage(const String &text) {
  maintainLink();
  if (!linkReady || text.length() == 0 || text.length() > 150) return false;

  ackWanted = text;
  ackGot = false;
  for (int tries = 0; tries < 2 && !ackGot; tries++) {
    mqtt.publish(MQTT_TOPIC_OUT, text.c_str());
    uint32_t t0 = millis();
    while (!ackGot && millis() - t0 < MQTT_ACK_TIMEOUT_MS) { mqtt.loop(); delay(5); }
  }
  ackWanted = "";
  Serial.printf("[LINK] tx: %s -> %s\n", text.c_str(), ackGot ? "OK" : "FAILED");
  return ackGot;
}

// Returns true and fills 'body' when a message from the other station is waiting.
bool linkPoll(String &body) {
  maintainLink();                                     // also runs mqtt.loop()
  return inboxPop(body);
}

#endif   // LINK_UDP / LINK_MQTT
#endif   // LINK_SMS / WiFi

// ======================= BUZZER / LED =============================
void initializeOutputs() {
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_LED, LOW);
}

void startBuzzer() { buzzerActive = true; buzzerStart = millis(); }

void stopBuzzer() { buzzerActive = false; digitalWrite(PIN_BUZZER, LOW); }

// BEEP/BEEP/BEEP every BUZZER_CYCLE_MS, never continuous, auto-stops
void updateBuzzer() {
  if (!buzzerActive) return;
  uint32_t t = millis() - buzzerStart;
  if (t >= BUZZER_MAX_MS) { stopBuzzer(); return; }
  uint32_t c = t % BUZZER_CYCLE_MS;
  bool on = (c < BUZZER_BEEP_MS * 6) && ((c / BUZZER_BEEP_MS) % 2 == 0);   // 3 beeps
  digitalWrite(PIN_BUZZER, on ? HIGH : LOW);
}

// ======================= DRIVER DATABASE ==========================
// "23" -> "023"
String padDriverId(String s) {
  while (s.length() < DRIVER_DIGITS) s = "0" + s;
  return s;
}

const Driver *getDriverInformation(const String &id3) {
  return findDriver(id3);                 // from drivers.h
}

bool validateDriver(const String &id3) {
  return getDriverInformation(id3) != nullptr;
}

// ======================= STATE HANDLING ===========================
void setState(TerminalState s) {
  state = s;
  stateStart = millis();
  tempUntil = 0;
  needsRender = true;
}

void resetSystem() {                          // back to IDLE
  activeRequest = false;
  activeId = "";
  entry = "";
  stopBuzzer();
  digitalWrite(PIN_LED, LOW);
  setState(T_IDLE);
}

void renderTerminal() {
  switch (state) {
    case T_IDLE:
      lcdShow("TRIWAVE TERMINAL", "NO ACTIVE REQUEST");
      break;
    case T_ALERT:
      lcdShow("NEW REQUEST", "ASSIGN DRIVER", "REQUEST #" + activeId);
      break;
    case T_ENTRY: {
      if (entry.length() == 0) {
        String blanks;
        for (int i = 0; i < DRIVER_DIGITS; i++) blanks += (i ? " _" : "_");
        lcdShow("ENTER DRIVER NO.", blanks, "#=CONFIRM  *=CLEAR", "REQUEST #" + activeId);
      } else {
        lcdShow("ENTER DRIVER NO.", "DRIVER #" + entry, "#=CONFIRM  *=CLEAR", "REQUEST #" + activeId);
      }
      break;
    }
    case T_NOTICE:
      lcdShow("DRIVER ASSIGNED", "SENT SUCCESSFULLY");
      break;
  }
}

void updateDisplay() {
  if (tempUntil && (int32_t)(millis() - tempUntil) >= 0) { tempUntil = 0; needsRender = true; }
  if (needsRender && !tempUntil) { renderTerminal(); needsRender = false; }
}

void handleCommunicationError() {             // corrupted / incomplete message received
  showTemp("COMMUNICATION ERROR", "WAITING...", 2000);
}

// ------------------------- receiving requests ---------------------
void notifyTerminal(const String &id) {
  activeRequest = true;
  activeId = id;
  entry = "";
  digitalWrite(PIN_LED, HIGH);                // green light
  startBuzzer();
  setState(T_ALERT);
}

void receiveRequest() {
  String body;
  if (!linkPoll(body)) return;
  Serial.printf("[LINK] rx: %s\n", body.c_str());

  String p[4];
  int n = splitCSV(body, p, 4);
  if (n != 3 || p[0] != "REQUEST" || p[1].length() != 3 || !isAllDigits(p[1]) || p[2] != "CONFIRMED") {
    handleCommunicationError();
    return;
  }
  String id = p[1];

  // --- duplicate prevention ---
  if (activeRequest && id == activeId) return;       // retransmission of the active request
  if (!activeRequest && id == lastCompletedId) {     // school never got our reply -> resend it
    if (lastReply.length()) sendMessage(lastReply);
    return;
  }

  notifyTerminal(id);                                // new request (a newer one replaces an unanswered one)
}

// ------------------------- driver input ---------------------------
bool sendDriverInformation(const String &reply) {
  for (int attempt = 1; attempt <= MAX_SEND_ATTEMPTS; attempt++) {
    if (sendMessage(reply)) return true;
    if (attempt < MAX_SEND_ATTEMPTS) {
      lcdShow("SEND FAILED", "RETRYING...", "ATTEMPT " + String(attempt + 1) + " OF " + String(MAX_SEND_ATTEMPTS));
      delay(RETRY_DELAY_MS);
    }
  }
  return false;
}

void confirmDriver() {
  String id3 = padDriverId(entry);                     // 23 -> 023

  if (!validateDriver(id3)) {
    entry = "";
    showTemp("INVALID DRIVER", "ENTER AGAIN", 2000);   // keypad stays open for a new number
    return;
  }

  const Driver *d = getDriverInformation(id3);
  stopBuzzer();                                        // driver acknowledged
  lcdShow("DRIVER #" + id3, d->name, "PROCESSING...");
  delay(700);
  lcdShow("DRIVER ASSIGNED", "#" + id3, "SENDING...");

  String reply = "DRIVER," + activeId + "," + id3;     // school looks up name + plate itself

  if (sendDriverInformation(reply)) {
    lastCompletedId = activeId;
    lastReply = reply;
    activeRequest = false;
    digitalWrite(PIN_LED, LOW);
    setState(T_NOTICE);                                // "SENT SUCCESSFULLY", then IDLE
  } else {
    lcdShow("CONNECTION FAILED", "ENTER DRIVER AGAIN");
    delay(2500);
    entry = "";
    setState(T_ENTRY);                                 // request stays open, driver can retry
  }
}

void handleDriverInput() {
  char key = keypad.getKey();                          // library debounces: 1 press = 1 key
  if (!key) return;
  if (tempUntil) return;                               // ignore keys while a message is showing
  if (state == T_ALERT) setState(T_ENTRY);             // driver started typing
  if (state != T_ENTRY) return;

  if (key >= '0' && key <= '9') {
    if (entry.length() < DRIVER_DIGITS) entry += key;
    needsRender = true;
  } else if (key == '*') {
    entry = "";
    needsRender = true;
  } else if (key == '#') {
    if (entry.length() > 0) confirmDriver();
  }
}

// ------------------------- timers per state -----------------------
void updateState() {
  uint32_t elapsed = millis() - stateStart;
  switch (state) {
    case T_ALERT:
      if (elapsed >= ALERT_SHOW_MS) setState(T_ENTRY);
      break;
    case T_NOTICE:
      if (elapsed >= DONE_SHOW_MS) resetSystem();
      break;
    default:
      break;
  }
}

// ======================= SETUP / LOOP =============================
void initializeKeypad() {
  keypad.setDebounceTime(30);
}

void setup() {
  Serial.begin(115200);
  initializeOutputs();
  initializeLCD();
  initializeKeypad();
  initializeLink();
  resetSystem();
}

void loop() {
  receiveRequest();
  handleDriverInput();
  updateState();
  updateBuzzer();
  updateDisplay();
}
