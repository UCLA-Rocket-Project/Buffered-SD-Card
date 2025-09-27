#ifndef BUFFERED_SD_H_
#define BUFFERED_SD_H_

#include <stdint.h>
#include <SPI.h>
#include <SD.h>

#define DEFAULT_BUFFER_SIZE 4096
#define FILEPATH_NAME_MAX_LENGTH 128
#define NUM_TRIES_TO_OPEN 5

class BufferedSD 
{
public:
    BufferedSD(SPIClass &spi_bus, uint8_t CS, const char* filepath, size_t buffer_size = DEFAULT_BUFFER_SIZE);
    ~BufferedSD();

    bool begin();
    int write(const char *data);
    void print_contents();

private:
    uint8_t *_write_buffer;
    size_t _buffer_idx;
    char _filepath[FILEPATH_NAME_MAX_LENGTH];
    File _file; // file handle: to constantly be kept in APPEND mode
    
    SPIClass _spi;
    uint8_t _CS_pin;
};

#endif // BUFFERED_SD_H_