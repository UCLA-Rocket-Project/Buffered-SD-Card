#ifndef BUFFERED_SD_H_
#define BUFFERED_SD_H_

#include <stdint.h>
#include <SPI.h>
#include <SD.h>

#define DEFAULT_BUFFER_SIZE 4096
#define FILEPATH_NAME_MAX_LENGTH 128
#define NUM_TRIES_TO_OPEN 5
#define SD_IDLE_TIMEOUT 5000UL

class BufferedSD 
{
public:
    BufferedSD(SPIClass &spi_bus, uint8_t CS, const char* filepath, size_t buffer_size = DEFAULT_BUFFER_SIZE);
    ~BufferedSD();

    bool begin();
    int write(const char *data);
    void print_contents();
    
    inline bool has_buffered_data();
    inline void flush_buffer();

private:
    uint8_t *_write_buffer;
    size_t _buffer_idx;
    size_t _buffer_size;
    char _filepath[FILEPATH_NAME_MAX_LENGTH];

    unsigned long _last_flush_time;
    
    SPIClass _spi;
    uint8_t _CS_pin;
};


inline bool BufferedSD::has_buffered_data() {
    return _buffer_idx > 0;
}

inline void BufferedSD::flush_buffer() {
    File f = SD.open(_filepath, FILE_APPEND);
    for (int i = 0; i <= _buffer_idx; ++i) {
        f.print(static_cast<char>(_write_buffer[i]));
    }
    f.close();
    _last_flush_time = millis();
}

#endif // BUFFERED_SD_H_