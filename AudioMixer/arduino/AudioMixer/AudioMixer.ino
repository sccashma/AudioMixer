const int NUM_SLIDERS = 5;
const int analogInputs[NUM_SLIDERS] = {12, 13, 15, 2, 4};
int analogSliderValues[NUM_SLIDERS];

const char* HANDSHAKE_KEY = "AUDIOMIXER_HELLO";
const char* HANDSHAKE_RESPONSE = "AUDIOMIXER_READY";
const char* HEARTBEAT = "AUDIOMIXER_V1_HEARTBEAT";

const unsigned long handshakeRetryInterval = 1000; // ms
const unsigned long handshakeTimeout = 10000;      // ms
const unsigned long heartbeatInterval = 500;       // ms
const unsigned long heartbeatTimeout = 1500;       // ms

enum State { HANDSHAKE, DATA };
State state = HANDSHAKE;

unsigned long lastHandshakeSent = 0;
unsigned long handshakeStart = 0;
unsigned long lastHeartbeatSent = 0;
unsigned long lastHeartbeatAck = 0;

void setup() {
  analogReadResolution(12);
  Serial.begin(115200);
  handshakeStart = millis();
}

void loop() {
  switch (state) {
    case HANDSHAKE:
      // Send handshake key every handshakeRetryInterval
      if (millis() - lastHandshakeSent > handshakeRetryInterval) {
        Serial.println(HANDSHAKE_KEY);
        lastHandshakeSent = millis();
      }
      // Listen for handshake response
      if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        if (input == HANDSHAKE_RESPONSE) {
          state = DATA;
          lastHeartbeatAck = millis();
          break;
        }
      }
      // Optional: If handshake phase takes too long, reset or handle error
      if (millis() - handshakeStart > handshakeTimeout) {
        handshakeStart = millis(); // restart handshake timer
      }
      break;

    case DATA:
      updateSliderValues();
      sendSliderValues();

      // Send heartbeat if interval elapsed
      if (millis() - lastHeartbeatSent > heartbeatInterval) {
        Serial.println(HEARTBEAT);
        lastHeartbeatSent = millis();
      }

      // Listen for heartbeat response (non-blocking)
      if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        if (input == HEARTBEAT) {
          lastHeartbeatAck = millis();
        }
      }

      // If no heartbeat ack from PC in time, go back to handshake
      if (millis() - lastHeartbeatAck > heartbeatTimeout) {
        state = HANDSHAKE;
        handshakeStart = millis();
      }

      delay(100);
      break;
  }
}

void updateSliderValues() {
  for (int i = 0; i < NUM_SLIDERS; i++) {
     analogSliderValues[i] = map(analogRead(analogInputs[i]), 0, 4095, 0, 1023);
  }
}

void sendSliderValues() {
  String builtString = "";
  for (int i = 0; i < NUM_SLIDERS; i++) {
    builtString += String((int)analogSliderValues[i]);
    if (i < NUM_SLIDERS - 1) {
      builtString += "|";
    }
  }
  Serial.println(builtString);
}