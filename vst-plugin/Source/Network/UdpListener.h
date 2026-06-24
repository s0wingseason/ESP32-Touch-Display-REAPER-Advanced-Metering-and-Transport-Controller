/*
 * MeterBridge — UDP Listener
 * Receives transport commands from the ESP32 display.
 */

#pragma once
#include <JuceHeader.h>
#include "Protocol.h"
#include <functional>

class UdpListener : public juce::Thread {
public:
    UdpListener() : Thread("MB-UdpListener") {}
    ~UdpListener() override { stopListening(); }

    using CommandCallback = std::function<void(uint8_t command, uint8_t param8, float paramFloat)>;

    void startListening(int listenPort, CommandCallback cb) {
        callback = cb;
        port = listenPort;
        socket.bindToPort(port);
        startThread();
    }

    void stopListening() {
        signalThreadShouldExit();
        socket.shutdown();
        stopThread(1000);
    }

private:
    void run() override {
        uint8_t buf[64];
        while (!threadShouldExit()) {
            int bytesRead = socket.read(buf, sizeof(buf), false);
            if (bytesRead >= (int)sizeof(mb_command_packet_t)) {
                auto* hdr = reinterpret_cast<const mb_header_t*>(buf);
                if (mb_validate_header(hdr)) {
                    auto* cmd = reinterpret_cast<const mb_command_packet_t*>(buf);
                    if (callback)
                        callback(cmd->command, cmd->param8, cmd->param_float);
                }
            }
            if (bytesRead <= 0)
                Thread::sleep(10);
        }
    }

    juce::DatagramSocket socket{false};
    int port = MB_DEFAULT_PORT;
    CommandCallback callback;
};
