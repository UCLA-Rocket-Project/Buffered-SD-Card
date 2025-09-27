#include <buffered_sd.h>
#include <SD.h>

BufferedSD::BufferedSD(SPIClass &spi_bus, uint8_t CS, const char *filepath, size_t buffer_size) 
    :_spi(spi_bus), _CS_pin(CS), _buffer_size(buffer_size), _last_flush_time(millis())
{
    // add the () behind to zero-initialize the buffer
    _write_buffer = new uint8_t[buffer_size]();
    configASSERT(_write_buffer && "Failed to initialize write buffer");
    _buffer_idx = 0;

    strncpy(_filepath, filepath, FILEPATH_NAME_MAX_LENGTH);
    // just in case the file path name overflows
    _filepath[FILEPATH_NAME_MAX_LENGTH - 1] = '\0';
}

BufferedSD::~BufferedSD() {
    delete[] _write_buffer;
}

bool BufferedSD::begin() {
    for (int i = 0; i < NUM_TRIES_TO_OPEN; ++i) {
        if (SD.begin(_CS_pin, _spi)) {
            break;
        }
        else if (i == NUM_TRIES_TO_OPEN - 1) {
            return false;
        }
        delay(100);
    }
    
    File f = SD.open(_filepath, FILE_WRITE);
    if (!f) {
        Serial.print("Failed to create file: ");
        Serial.println(_filepath);
        return false;
    }
    f.close();

    return true;
}

int BufferedSD::write(const char *data) {
    size_t length = strlen(data);
    // if the data can be buffered, throw it into the buffer first
    if (_buffer_idx + length >= _buffer_size) {
        flush_buffer();
        _buffer_idx = 0;
    }

    memcpy(_write_buffer + _buffer_idx, data, length);
    _buffer_idx += length;
    
    return length;
}

void BufferedSD::print_contents() {
    File f = SD.open(_filepath, FILE_READ);
    if (!f) {
        Serial.print("The file cannot be opened");
        return;
    }

    while (f.available()) {
        Serial.print((char)f.read());
    }

    f.close();
}