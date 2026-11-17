#ifndef TEST_ARDUINO_H
#define TEST_ARDUINO_H

#include <cstddef>
#include <cstdint>
#include <string>

#define F(text) text
#define HEX 16

class String {
public:
    bool reserve(size_t size) { value.reserve(size); return true; }
    String &operator+=(char byte) { value += byte; return *this; }
    const char *c_str() const { return value.c_str(); }
private:
    std::string value;
};

class Stream {
public:
    virtual ~Stream() = default;
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;
    virtual void flush() = 0;
    virtual size_t write(uint8_t byte) = 0;
    virtual size_t write(const uint8_t *buffer, size_t size) {
        for (size_t i = 0; i < size; ++i) { write(buffer[i]); }
        return size;
    }
    template<typename T> void print(const T &, int = 10) {}
    template<typename T> void println(const T &) {}
    void println() {}
};

uint32_t millis();

#endif
