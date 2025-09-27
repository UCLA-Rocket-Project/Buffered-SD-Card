#include <Arduino.h>
#include <SPI.h>

#define VCLK    10
#define VMISO   11
#define VMOSI   12
#define CS_XTSD 9

SPIClass vspi_bus;

const char str[] = "This is a sample C-style string that is designed to be approximately 200 bytes in length. It contains various characters and demonstrates how text can fill up memory space efficiently.";
const int num_writes = 1000;

void test_nomral_sd() {
  bool sd_init_2 = false;
  while (!sd_init_2) {
    sd_init_2 = SD.begin(CS_XTSD, vspi_bus);
    Serial.println("retyrying to init sd card");
  }
  
  delay(2000);
  unsigned long totalTime = 0;

  for (int j = 0; j < 5; ++j) {
    // testing code -- regular write speed
    unsigned long startTime = micros();
    for (int i = 0; i < num_writes; ++i) {
      File f = SD.open("/hi.txt", FILE_APPEND);
      f.print(str);
      f.close();
    }
    
    unsigned long endTime = micros();
    Serial.printf("Time taken to write 1000 batches of 200 bytes: %.5f\n", (endTime - startTime) / (double)1'000'000.0);
    totalTime += endTime - startTime;
  }

  Serial.printf("Expected size: %d bytes\n", strlen(str) * num_writes);
  Serial.printf("\nAvg time taken for ~200KB of data %.5f\n", totalTime / ((double)5 * 1'000'000));


  File f = SD.open("/hi.txt", FILE_READ);
  if (!f) {
    Serial.print("The file cannot be opened");
    return;
  }

  while (f.available()) {
    Serial.print((char)f.read());
    break;
  }
  f.close();

  delay(1000);
}

void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println("Setup start");

  vspi_bus.begin(VCLK, VMISO, VMOSI, -1);
  
  test_nomral_sd();
}


void loop() {
}