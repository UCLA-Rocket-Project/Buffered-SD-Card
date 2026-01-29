#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

#include <buffered_sd.h>

#define PIN_NUM_MISO GPIO_NUM_19
#define PIN_NUM_MOSI GPIO_NUM_23
#define PIN_NUM_CLK  GPIO_NUM_18
#define PIN_NUM_CS   GPIO_NUM_5

// all boards will have their time offset synced to the radio board
// analog and radio board will receive radio board interrupts and 
// update time offset accordingly
long time_offset = 0;

SPIClass spi_bus;
BufferedSD sd(spi_bus, PIN_NUM_CS, "test", ".txt");

void setup() {
    Serial.begin(115200);
    delay(1000);

    spi_bus.begin(PIN_NUM_CLK, PIN_NUM_MISO, PIN_NUM_MOSI, -1);

    if (!sd.begin()) {
        Serial.println("Unable to start SD card...\n");
        while(1);
    }

    sd.write_immediate("120,2,3,4\n345,4,5,6\n10000,5\n4294967295,23,24,25\n");

    char name[256];
    sd.get_file_name(name);
    Serial.println("The new file is: %s\n", name);

    sd_card_update ud = sd.get_file_update();
    Serial.println("file size: %lu, last ts: %lu\n", ud.file_size, ud.last_written_timestamp);
}

void loop() {}
    