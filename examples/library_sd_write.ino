#include <Arduino.h>
#include <SPI.h>
#include <buffered_sd.h>

#define VCLK    10
#define VMISO   11
#define VMOSI   12
#define CS_XTSD 9

SPIClass vspi_bus;

const char str[] = "This is a sample C-style string that is designed to be approximately 200 bytes in length. It contains various characters and demonstrates how text can fill up memory space efficiently.";
const int num_writes = 1000;

void test_library_sd() {
  BufferedSD sd(vspi_bus, CS_XTSD, "/hi.txt");

  // Initialize SPI bus manually
  bool sd_init = sd.begin();
  configASSERT(sd_init && "Sd card not initialized");

  delay(1000);
  unsigned long totalTime = 0;

  for (int j = 0; j < 5; ++j) {
    unsigned long startTime = micros();

    for (int i = 0; i < num_writes; ++i) {
      sd.write(str);
    }

    sd.flush_buffer();

    unsigned long endTime = micros();
    Serial.printf("Time taken to write 1000 batches of 200 bytes: %.5f\n", (endTime - startTime) / (double)1'000'000.0);
    totalTime += endTime - startTime;
  }

  Serial.printf("\nAvg time taken for ~200KB of data %.5f\n", totalTime / ((double)5 * 1'000'000));

  sd.print_contents();
}

void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println("Setup start");

  vspi_bus.begin(VCLK, VMISO, VMOSI, -1);

  test_library_sd();
}


void loop() {
}