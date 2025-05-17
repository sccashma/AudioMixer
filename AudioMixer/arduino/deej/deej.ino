const int NUM_SLIDERS = 5;
const int analogInputs[NUM_SLIDERS] = {12, 13, 15, 2, 4};

int analogSliderValues[NUM_SLIDERS];

void setup() { 
  // for (int i = 0; i < NUM_SLIDERS; i++) {
  //   pinMode(analogInputs[i], INPUT);
  // }

  //set the resolution to 12 bits (0-4095)
  analogReadResolution(12);

  Serial.begin(9600);
}

void loop() {
  updateSliderValues();
  sendSliderValues(); // Actually send data (all the time)
  // printSliderValues(); // For debug
  delay(100);
}

void updateSliderValues() {
  for (int i = 0; i < NUM_SLIDERS; i++) {
     analogSliderValues[i] = map(analogRead(analogInputs[i]), 0, 4095, 0, 1023);
  }
}

void sendSliderValues() {
  String builtString = String("");

  for (int i = 0; i < NUM_SLIDERS; i++) {
    builtString += String((int)analogSliderValues[i]);

    if (i < NUM_SLIDERS - 1) {
      builtString += String("|");
    }
  }
  
  Serial.println(builtString);
}

void printSliderValues() {
  for (int i = 0; i < NUM_SLIDERS; i++) {
    String printedString = String("Slider #") + String(i + 1) + String(": ") + String(analogSliderValues[i]) + String(" mV");
    Serial.write(printedString.c_str());

    if (i < NUM_SLIDERS - 1) {
      Serial.write(" | ");
    } else {
      Serial.write("\n");
    }
  }
}
