#if SIMULATOR_BLE

#include "ble.hpp"

#include <iostream>
#include <simpleble/SimpleBLE.h>
#include <unistd.h>

#include "Inputs/TurnTouch.hpp"

SimpleBLE::Peripheral turnTouchPeripheral;
uint16_t lastProcessedCode = 0;

void BleFindTurnTouch(std::string address, std::function<void(RemoteCommand command)> callback) {
    std::cout << "Searching for TurnTouch with address: " << address << std::endl;
    std::vector<SimpleBLE::Peripheral> peripherals;

    auto adapter_list = SimpleBLE::Adapter::get_adapters();
    if (adapter_list.size() == 0) {
        std::cerr << "No BLE adapter was found." << std::endl;
        exit(1);
    }
    SimpleBLE::Adapter &adapter = adapter_list[0];

    adapter.set_callback_on_scan_start([]() { std::cout << "Scan started." << std::endl; });
    adapter.set_callback_on_scan_stop([]() { std::cout << "Scan stopped." << std::endl; });
    adapter.set_callback_on_scan_found([&](SimpleBLE::Peripheral peripheral) {
        std::cout << "Found device: " << peripheral.identifier() << " [" << peripheral.address()
                  << "]" << std::endl;
        if (peripheral.address() == address) {
            std::cout << "Found a TurnTouch." << std::endl;
            turnTouchPeripheral = peripheral;
            adapter.scan_stop();
        }
    });

    // Scan until we find something
    adapter.scan_start();
    while (adapter.scan_is_active()) {
        usleep(100000);
    }

    if (turnTouchPeripheral.identifier() != "") {
        auto &peripheral = turnTouchPeripheral;
        std::cout << "Connecting to " << turnTouchPeripheral.identifier() << " ["
                  << peripheral.address() << "]" << std::endl;
        peripheral.connect();

        std::cout << "Successfully connected, listing services." << std::endl;
        for (auto service : peripheral.services()) {
            std::cout << "Service: " << service.uuid << std::endl;
            for (auto characteristic : service.characteristics) {
                std::cout << "  Characteristic: " << characteristic << std::endl;
            }
            if (service.uuid == TURNTOUCH_BUTTON_SVC_UUID) {
                peripheral.notify(
                    TURNTOUCH_BUTTON_SVC_UUID, TURNTOUCH_BUTTON_CHR_UUID,
                    [&callback](SimpleBLE::ByteArray bytes) {
                        if (bytes.length() != 2) {
                            std::cout
                                << "WARNING: Expected 2 byte notification from Turntouch; got "
                                << bytes.length() << "bailing early\n";
                            return;
                        }
                        uint16_t processed = (bytes[0] << 8) | (bytes[1]);
                        std::cout << "TT Received: " << std::hex << processed << std::endl;
                        // Hack to fix long press back. For some reason, FFFF is received here for
                        // all long presses regardless of the button. It's unclear if that's an
                        // issue with SimpleBLE, macOS or the TurnTouch
                        if (processed == 0xFFFF && lastProcessedCode == TT_W_PRESS) {
                            processed = TT_W_HOLD;
                        }
                        if (processed == TT_NONE) {
                            return;
                        }
                        lastProcessedCode = processed;
                        RemoteCommand command = DecodeTurntouch(processed);
                        if (command != RemoteCommand::NONE) {
                            callback(command);
                        }
                    });
            }
        }
    }
}

#endif
