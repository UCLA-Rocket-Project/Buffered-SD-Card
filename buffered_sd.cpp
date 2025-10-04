#include <buffered_sd.h>
#include <SD.h>

BufferedSD::BufferedSD(SPIClass &spi_bus, uint8_t CS, const char *base_filepath, const char *extension, size_t buffer_size) 
    :_spi(&spi_bus), _CS_pin(CS), _buffer_size(buffer_size), _base_path(base_filepath), _extension(extension)
{
    // add the () behind to zero-initialize the buffer
    _write_buffer = new uint8_t[buffer_size]();
    configASSERT(_write_buffer && "Failed to initialize write buffer");
    _buffer_idx = 0;
}

BufferedSD::~BufferedSD() {
    flush_buffer();
    delete[] _write_buffer;
}

bool BufferedSD::begin() {
    for (int i = 0; i < NUM_TRIES_TO_OPEN; ++i) {
        if (SD.begin(_CS_pin, *_spi)) {
            break;
        }
        else if (i == NUM_TRIES_TO_OPEN - 1) {
            return false;
        }
        delay(100);
    }

    find_first_available_file(_base_path, _filepath, _extension);
    
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
    Serial.printf("Length of data: %d | New index of buffer: %d\n", length, _buffer_idx + length);
    // if the data can be buffered, throw it into the buffer first
    if (_buffer_idx + length > _buffer_size) {
        Serial.println("Entering here...");
        flush_buffer();
    }

    memcpy(_write_buffer + _buffer_idx, data, length);
    _buffer_idx += length;
    
    return length;
}

int BufferedSD::write(const char *data, size_t length) {
    Serial.printf("Length of data: %d | New index of buffer: %d\n", length, _buffer_idx + length);
    // if the data can be buffered, throw it into the buffer first
    if (_buffer_idx + length > _buffer_size) {
        Serial.println("Entering here...");
        flush_buffer();
    }

    memcpy(_write_buffer + _buffer_idx, data, length);
    _buffer_idx += length;
    
    return length;
}

int BufferedSD::write_immediate(const char *data) {
    flush_buffer();

    size_t length = strlen(data);

    File f = SD.open(_filepath, FILE_APPEND);
    if (!f) return -1;
    size_t written_length = 0;
    written_length += f.write(reinterpret_cast<const uint8_t *>(data), strlen(data));
    f.close();

    return written_length;
}

int BufferedSD::write_immediate(const char *data, size_t length) {
    flush_buffer();

    File f = SD.open(_filepath, FILE_APPEND);
    if (!f) return -1;
    size_t written_length = 0;
    written_length += f.write(reinterpret_cast<const uint8_t *>(data), length);
    f.close();

    return written_length;
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

void BufferedSD::find_first_available_file(const char *planned_filepath, char *final_filepath, const char *extension) {
    char temp_filepath[FILEPATH_NAME_MAX_LENGTH];

    while (true) {
        // FAT32 (file format used by XTSD has a limit of 256 files per directory)
        for (int i = 0; i < 256; ++i) {
            snprintf(
                temp_filepath, 
                FILEPATH_NAME_MAX_LENGTH, 
                "/%s-%d%s", 
                planned_filepath, i, extension
            );

            if (!SD.exists(temp_filepath)) {
                strncpy(final_filepath, temp_filepath, FILEPATH_NAME_MAX_LENGTH);
                // null terminate the resulting string just in case
                final_filepath[FILEPATH_NAME_MAX_LENGTH - 1] = '\0';
                return;
            }
        }
    }
}