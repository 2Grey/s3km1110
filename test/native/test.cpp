#include "s3km1110.h"

#include <cassert>
#include <cstring>
#include <deque>
#include <functional>
#include <iostream>
#include <vector>

using Bytes = std::vector<uint8_t>;
static uint32_t now = 0;
uint32_t millis() { return now++; }

Bytes frame(bool command, const Bytes &body)
{
    Bytes bytes = command ? Bytes{0xFD, 0xFC, 0xFB, 0xFA} : Bytes{0xF4, 0xF3, 0xF2, 0xF1};
    bytes.push_back(static_cast<uint8_t>(body.size()));
    bytes.push_back(static_cast<uint8_t>(body.size() >> 8));
    bytes.insert(bytes.end(), body.begin(), body.end());
    const Bytes footer = command ? Bytes{4, 3, 2, 1} : Bytes{0xF8, 0xF7, 0xF6, 0xF5};
    bytes.insert(bytes.end(), footer.begin(), footer.end());
    return bytes;
}

Bytes ack(uint8_t command, const Bytes &payload = {}, uint8_t status = 0)
{
    Bytes body{command, 1, status, 0};
    if (command == 0 || command == 0x11) {
        body.push_back(static_cast<uint8_t>(payload.size()));
        body.push_back(0);
    }
    body.insert(body.end(), payload.begin(), payload.end());
    return frame(true, body);
}

Bytes report(uint16_t distance = 321)
{
    Bytes body{1, static_cast<uint8_t>(distance), static_cast<uint8_t>(distance >> 8)};
    for (uint16_t i = 0; i < 16; ++i) {
        const uint16_t energy = 0x8000 + i * 257;
        body.push_back(static_cast<uint8_t>(energy));
        body.push_back(static_cast<uint8_t>(energy >> 8));
    }
    return frame(false, body);
}

class RadarStream : public Stream {
public:
    std::deque<uint8_t> input;
    std::vector<Bytes> requests;
    std::function<Bytes(const Bytes &)> respond;

    int available() override { return static_cast<int>(input.size()); }
    int read() override {
        if (input.empty()) { return -1; }
        const uint8_t byte = input.front();
        input.pop_front();
        return byte;
    }
    int peek() override { return input.empty() ? -1 : input.front(); }
    void flush() override {}
    size_t write(uint8_t) override { return 1; }
    size_t write(const uint8_t *buffer, size_t size) override {
        const Bytes request(buffer, buffer + size);
        requests.push_back(request);
        if (respond) { feed(respond(request)); }
        else if (request[6] == 8) { feed(ack(8, {request[8], 0, 0, 0})); }
        else { feed(ack(request[6])); }
        return size;
    }
    void feed(const Bytes &bytes) { input.insert(input.end(), bytes.begin(), bytes.end()); }
};

void testBeforeBegin()
{
    now = 0;
    s3km1110 radar;
    assert(!radar.isActive());
    assert(!radar.read());
    assert(!radar.readFirmwareVersion());
    assert(!radar.readAllRadarConfigs());
    assert(!radar.setRadarConfigurationMinimumGates(5));
}

void testReports()
{
    RadarStream stream, debug;
    s3km1110 radar;
    assert(radar.begin(stream, debug));
    const Bytes bytes = report();
    stream.feed(Bytes(bytes.begin(), bytes.begin() + 17));
    assert(!radar.read());
    assert(radar.distanceToTarget == -1);
    stream.feed(Bytes(bytes.begin() + 17, bytes.end()));
    assert(radar.read());
    assert(radar.isTargetDetected && radar.distanceToTarget == 321);
    for (uint16_t i = 0; i < 16; ++i) { assert(radar.distanceGateEnergy[i] == 0x8000 + i * 257); }

    // A footer inside the binary payload must not terminate the report.
    Bytes embeddedFooter = report(500);
    embeddedFooter[9] = 0xF8; embeddedFooter[10] = 0xF7;
    embeddedFooter[11] = 0xF6; embeddedFooter[12] = 0xF5;
    stream.feed(embeddedFooter);
    stream.feed(report(700));
    assert(radar.read() && radar.distanceToTarget == 500);
    assert(radar.distanceGateEnergy[0] == 0xF7F8);
    assert(radar.distanceGateEnergy[1] == 0xF5F6);
    assert(radar.read() && radar.distanceToTarget == 700);
    assert(!radar.read());
    assert(radar.isActive());
    now += 250;
    assert(!radar.isActive());

    now = UINT32_MAX - 20;
    stream.feed(report());
    assert(radar.read());
    now += 30;
    assert(radar.isActive());
    now += 250;
    assert(!radar.isActive());
}

void testMalformedFrames()
{
    RadarStream stream;
    s3km1110 radar;
    assert(radar.begin(stream));
    stream.feed({0x99, 0xFD, 0xF4}); // Header mismatch followed by a new header.
    Bytes good = report();
    stream.feed(Bytes(good.begin() + 1, good.end()));
    assert(radar.read());

    for (const Bytes &bad : {frame(true, {}), frame(true, {0}), frame(true, {8, 1, 0}),
                            frame(true, {0x11, 1, 0, 0}), frame(false, {1}),
                            Bytes{0xFD, 0xFC, 0xFB, 0xFA, 0xFF, 0xFF}}) {
        stream.feed(bad);
        assert(!radar.read());
        stream.feed(report(444));
        assert(radar.read() && radar.distanceToTarget == 444);
    }
    Bytes badFooter = report(999);
    badFooter.back() = 0;
    stream.feed(badFooter);
    assert(!radar.read());
    assert(radar.distanceToTarget == 444);
    stream.feed(report(555));
    assert(radar.read() && radar.distanceToTarget == 555);
}

void testCommandRequests()
{
    RadarStream stream;
    s3km1110 radar;
    assert(radar.begin(stream));
    assert(stream.requests[0] == (Bytes{0xFD, 0xFC, 0xFB, 0xFA, 4, 0, 0xFF, 0, 1, 0, 4, 3, 2, 1}));
    assert(stream.requests[1] == (Bytes{0xFD, 0xFC, 0xFB, 0xFA, 8, 0, 0x12, 0, 0, 0, 4, 0, 0, 0, 4, 3, 2, 1}));
    assert(stream.requests[2] == (Bytes{0xFD, 0xFC, 0xFB, 0xFA, 2, 0, 0xFE, 0, 4, 3, 2, 1}));
#ifndef S3KM1110_SKIP_READ_CONFIG_ON_BEGIN
    assert(stream.requests.size() == 12);
    assert(radar.radarConfiguration.detectionGatesMax == 1);
    assert(radar.radarConfiguration.targetDisappearanceDelay == 4);
#else
    assert(stream.requests.size() == 3);
#endif
    stream.requests.clear();
    assert(radar.setRadarConfigurationMinimumGates(255));
    assert(radar.radarConfiguration.detectionGatesMin == 15);
    assert(stream.requests[1] == (Bytes{0xFD, 0xFC, 0xFB, 0xFA, 8, 0, 7, 0, 0, 0, 15, 0, 0, 0, 4, 3, 2, 1}));
    assert(radar.setRadarConfigurationMaximumGates(14));
    assert(radar.radarConfiguration.detectionGatesMax == 14);
    assert(radar.setRadarConfigurationTargetDisappearanceDelay(65535));
    assert(radar.radarConfiguration.targetDisappearanceDelay == 65535);
    assert(stream.requests[7][10] == 0xFF && stream.requests[7][11] == 0xFF);

    stream.respond = [](const Bytes &request) {
        return request[6] == 8 ? ack(8, {0xFF, 0xFF, 0xFF, 0xFF}) : ack(request[6]);
    };
    assert(radar.readAllRadarConfigs());
    assert(radar.radarConfiguration.detectionGatesMin == 15);
    assert(radar.radarConfiguration.detectionGatesMax == 15);
    assert(radar.radarConfiguration.targetDisappearanceDelay == 65535);
}

void testStringsAndFailures()
{
    RadarStream stream;
    s3km1110 radar;
    assert(radar.begin(stream));
    stream.respond = [](const Bytes &request) {
        if (request[6] == 0) { return ack(0, {'v', '1', '.', '2'}); }
        if (request[6] == 0x11) { return ack(0x11, {'S', 'N', 0, 'x'}); }
        return ack(request[6]);
    };
    assert(radar.readFirmwareVersion());
    assert(std::strcmp(radar.firmwareVersion.c_str(), "v1.2") == 0);
    assert(radar.readSerialNumber());
    assert(std::strcmp(radar.serialNumber.c_str(), "SN") == 0);

    // Reject both truncated strings and extra bytes beyond the declared length.
    for (uint16_t declaredLength : {uint16_t(0), uint16_t(1), uint16_t(30), uint16_t(256)}) {
        stream.respond = [declaredLength](const Bytes &request) {
            if (request[6] == 0 || request[6] == 0x11) {
                return frame(true, {request[6], 1, 0, 0,
                                    static_cast<uint8_t>(declaredLength),
                                    static_cast<uint8_t>(declaredLength >> 8), 'x', 'y'});
            }
            return ack(request[6]);
        };
        assert(!radar.readFirmwareVersion());
        assert(!radar.readSerialNumber());
        assert(std::strcmp(radar.firmwareVersion.c_str(), "v1.2") == 0);
        assert(std::strcmp(radar.serialNumber.c_str(), "SN") == 0);
    }

    stream.respond = [](const Bytes &request) {
        if (request[6] == 0) { return ack(0, {'b', 'a', 'd'}, 1); }
        if (request[6] == 8) { return ack(8, {255, 0, 0, 0}, 1); }
        if (request[6] == 7) { return ack(7, {}, 1); }
        return ack(request[6]);
    };
    const uint8_t oldMin = radar.radarConfiguration.detectionGatesMin;
    assert(!radar.readFirmwareVersion());
    assert(std::strcmp(radar.firmwareVersion.c_str(), "v1.2") == 0);
    assert(!radar.readRadarConfigMinimumGates());
    assert(!radar.setRadarConfigurationMinimumGates(10));
    assert(radar.radarConfiguration.detectionGatesMin == oldMin);
    assert(stream.requests.back()[6] == 0xFE); // Command mode closes on failure.

    stream.respond = [](const Bytes &request) {
        if (request[6] == 0) { return report(); } // Telemetry cannot replace an ACK.
        if (request[6] == 8) { return ack(8, {1, 2, 3}); }
        return ack(request[6]);
    };
    assert(!radar.readFirmwareVersion());
    assert(!radar.readRadarConfigMinimumGates());
    assert(radar.radarConfiguration.detectionGatesMin == oldMin);

    stream.respond = [](const Bytes &request) {
        if (request[6] == 0) { return frame(true, {0, 1, 0, 0}); }
        if (request[6] == 0x11) { return ack(0x11); }
        return ack(request[6]);
    };
    assert(!radar.readFirmwareVersion());
    assert(!radar.readSerialNumber());
    assert(std::strcmp(radar.firmwareVersion.c_str(), "v1.2") == 0);
    assert(std::strcmp(radar.serialNumber.c_str(), "SN") == 0);

    stream.respond = [](const Bytes &request) { return ack(request[6], {}, 1); };
    assert(!radar.begin(stream));
    stream.respond = [](const Bytes &) { return Bytes{}; };
    assert(!radar.begin(stream));
    assert(!radar.isActive());
}

void testCloseCommandModeFailures()
{
    for (bool missingAck : {false, true}) {
        RadarStream stream;
        s3km1110 radar;
        assert(radar.begin(stream));
        const uint8_t oldMin = radar.radarConfiguration.detectionGatesMin;
        stream.respond = [missingAck](const Bytes &request) {
            if (request[6] == 0xFE) {
                return missingAck ? Bytes{} : ack(0xFE, {}, 1);
            }
            return ack(request[6]);
        };
        assert(!radar.setRadarConfigurationMinimumGates(10));
        assert(radar.radarConfiguration.detectionGatesMin == oldMin);
        assert(stream.requests.back()[6] == 0xFE);
        assert(!radar.begin(stream));
        assert(stream.requests.back()[6] == 0xFE);
    }
}

void testReinitializationAndNoise()
{
    RadarStream first, second;
    s3km1110 radar;
    assert(radar.begin(first));
    first.feed({0xF4, 0xF3});
    assert(!radar.read());
    assert(radar.begin(second)); // Discard the previous stream's partial header.
    second.feed(report(808));
    assert(radar.read() && radar.distanceToTarget == 808);

    uint32_t random = 12345;
    for (size_t i = 0; i < 20000; ++i) {
        random = random * 1664525U + 1013904223U;
        second.feed({static_cast<uint8_t>(random >> 24)});
        radar.read();
    }
    // Drain any partial noise frame before checking that valid reports resume.
    second.feed(Bytes(s3km1110::kMaxFrameLength, 0));
    while (radar.read()) {}
    second.feed(report(909));
    assert(radar.read() && radar.distanceToTarget == 909);
}

int main()
{
    testBeforeBegin();
    testReports();
    testMalformedFrames();
    testCommandRequests();
    testStringsAndFailures();
    testCloseCommandModeFailures();
    testReinitializationAndNoise();
    std::cout << "All native protocol tests passed\n";
}
