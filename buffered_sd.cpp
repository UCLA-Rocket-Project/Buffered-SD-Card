#include <buffered_sd.h>
#include <SD.h>

BufferedSD::BufferedSD(SPIClass &spi_bus, uint8_t CS, const char *filepath, size_t buffer_size) 
    :_spi(spi_bus), _CS_pin(CS)
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
    if (_file) _file.close();
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
    }
    
    _file = SD.open(_filepath, FILE_WRITE);
    if (!_file) {
        Serial.print("Failed to create file: ");
        Serial.println(_filepath);
        return false;
    }
    _file.close();
    
    // leave the file open in append mode so we dont have to open and close the file everytime
    _file = SD.open(_filepath, FILE_APPEND);

    return true;
}

int BufferedSD::write(const char *data) {
    if (!_file) {
        return 0;
    }
    int bytes_written = _file.println(data);
    return bytes_written;
}

void BufferedSD::print_contents() {
    _file.close();
    _file = SD.open(_filepath, FILE_READ);
    if (!_file) {
        Serial.print("The file cannot be opened");
        return;
    }

    while (_file.available()) {
        Serial.write(_file.read());
    }

    _file.close();
    _file = SD.open(_filepath, FILE_APPEND);
}