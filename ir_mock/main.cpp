#include <iostream>
#include "IRsend_test.h"
#include "ir_Daikin.h"

int main() {
    // Build AC state (no pin hardware — only used for frame assembly)
    IRDaikinESP ac(0);
    ac.begin();
    ac.setPower(true);
    ac.setMode(kDaikinCool);
    ac.setTemp(22);
    ac.setFan(kDaikinFanAuto);
    ac.setSwingVertical(false);
    ac.setEcono(false);
    ac.setPowerful(false);

    std::cout << ac.toString() << "\n\n";

    // Dump raw 35-byte frame
    uint8_t* raw = ac.getRaw();
    std::cout << "Raw bytes (" << kDaikinStateLength << "):\n";
    for (uint16_t i = 0; i < kDaikinStateLength; i++) {
        std::cout << "  [" << i << "] 0x";
        std::cout << std::hex << static_cast<int>(raw[i]) << "\n";
    }
    std::cout << std::dec << "\n";

    // Send through the test stub to capture pulse/space timings
    IRsendTest irsend(0);
    irsend.begin();
    irsend.sendDaikin(raw);
    std::cout << "Pulse/space timings:\n" << irsend.outputStr() << "\n";

    return 0;
}
