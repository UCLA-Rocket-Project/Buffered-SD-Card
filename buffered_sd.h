#ifndef BUFFERED_SD_H_
#define BUFFERED_SD_H_

#include <stdint.h>
#include <SPI.h>
#include <SD.h>

#define DEFAULT_BUFFER_SIZE 4096
#define FILEPATH_NAME_MAX_LENGTH 128
#define NUM_TRIES_TO_OPEN 5
#define SD_IDLE_TIMEOUT 5000UL

struct sd_card_update {
    uint32_t file_size;
    uint32_t last_written_timestamp;
};

class BufferedSD 
{
public:
    /**
     * @param spi_bus: the custom SPI bus
     * @param CS: the CS pin
     * @param base_filepath: the base filepath that will be used to find the next available file to write to
     * @param extension: the file extension of the log file
     * @param buffer_size: the size of the write buffer
    */
    BufferedSD(SPIClass &spi_bus, uint8_t CS, const char *base_filepath = "", const char *extension = ".txt", size_t buffer_size = DEFAULT_BUFFER_SIZE);
    ~BufferedSD();

    bool begin();
    int write(const char *data);
    int write(const char *data, size_t length);
    int write_immediate(const char *data);
    int write_immediate(const char *data, size_t length);
    void print_contents();

    /**
     * @brief: function specific to the debugging module on the esp32s
     * 
     * @returns: file size + timestamp of last written log entry
     */
    sd_card_update get_file_update();

    void get_file_name(char *buf);

    /**
     * @brief: find the first available file name, which is achieved by adding a number to the end of the current file name
     * 
     * @param planned_filepath: the start filepath to search from
     * @param final_filepath: the first file name that is found
     * @param extension: the file extension
     */
    void find_first_available_file(const char *planned_filepath, const char *extension);
    
    inline bool has_buffered_data();
    inline void flush_buffer();

private:
    uint8_t *_write_buffer;
    size_t _buffer_idx;
    size_t _buffer_size;
    char _filepath[FILEPATH_NAME_MAX_LENGTH];
    const char *_base_path;
    const char *_extension;
    
    SPIClass *_spi;
    uint8_t _CS_pin;
};


inline bool BufferedSD::has_buffered_data() {
    return _buffer_idx > 0;
}

inline void BufferedSD::flush_buffer() {
    if (_buffer_idx == 0) return;

    File f = SD.open(_filepath, FILE_APPEND);
    if (!f) {
        Serial.println("Cannot open file for appending");
        return;
    }

    size_t written_bytes = f.write(_write_buffer, _buffer_idx);
    f.close();
    _buffer_idx = 0;
}

#endif // BUFFERED_SD_H_