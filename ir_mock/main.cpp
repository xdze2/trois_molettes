#include <iostream>
#include <cstdlib>
#include "IRsend_test.h"
#include "ir_Daikin.h"

// Pull in the portable frame builder from firmware/
#include "../firmware/daikin_frame.h"
#include "../firmware/daikin_frame.cpp"

static void print_frame(const char *label, const uint8_t *frame) {
    std::cout << label << ":\n";
    for (uint8_t i = 0; i < DAIKIN_FRAME_LEN; i++) {
        std::cout << "  [" << (int)i << "] 0x";
        if (frame[i] < 0x10) std::cout << "0";
        std::cout << std::hex << (int)frame[i] << std::dec << "\n";
    }
}

int main() {
    // --- Reference frame from the full library ---
    IRDaikinESP ref(0);
    ref.begin();
    ref.setPower(true);
    ref.setMode(kDaikinCool);
    ref.setTemp(22);
    ref.setFan(kDaikinFanAuto);
    ref.setSwingVertical(false);
    ref.setEcono(false);
    ref.setPowerful(false);
    uint8_t *ref_frame = ref.getRaw();

    std::cout << ref.toString() << "\n\n";
    print_frame("Reference (IRDaikinESP)", ref_frame);

    // --- Frame from the portable builder ---
    ACState st;
    st.power    = true;
    st.mode     = DAIKIN_MODE_COOL;
    st.temp     = 22;
    st.fan      = DAIKIN_FAN_AUTO;
    st.swing_v  = false;
    st.econo    = false;
    st.powerful = false;

    uint8_t my_frame[DAIKIN_FRAME_LEN];
    daikin_build_frame(&st, my_frame);
    std::cout << "\n";
    print_frame("Portable builder", my_frame);

    // --- Byte-for-byte comparison ---
    std::cout << "\nDiff:\n";
    bool ok = true;
    for (uint8_t i = 0; i < DAIKIN_FRAME_LEN; i++) {
        if (ref_frame[i] != my_frame[i]) {
            std::cout << "  [" << (int)i << "] ref=0x" << std::hex << (int)ref_frame[i]
                      << " got=0x" << (int)my_frame[i] << std::dec << "  <-- MISMATCH\n";
            ok = false;
        }
    }
    if (ok) std::cout << "  all bytes match\n";

    // --- Pulse timings from the portable frame ---
    std::cout << "\nPulse/space timings (portable frame):\n";
    IRsendTest irsend(0);
    irsend.begin();
    irsend.sendDaikin(my_frame);
    std::cout << irsend.outputStr() << "\n";

    return ok ? 0 : 1;
}
