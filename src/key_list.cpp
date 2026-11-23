#include "../include/key_list.h"

const std::vector<key_list::Group> &key_list::groups() {
    static const std::vector<Group> Groups = {
        { "ENGINE", {
            { "A", "IGNITION ON / OFF" },
            { "S", "STARTER (HOLD)" },
            { "Q W E R", "THROTTLE 1/10/20/100% (HOLD)" },
            { "SPACE + WHEEL", "FINE THROTTLE" },
            { "1 - 5", "SLOW MOTION (HOLD)" },
            { "ENTER", "RELOAD THE ENGINE" },
            { "O", "OPEN AN ENGINE" },
            { "[  ]", "PREVIOUS / NEXT ENGINE" },
        } },
        { "DRIVING", {
            { "UP / DOWN", "SHIFT UP / DOWN" },
            { "SHIFT", "CLUTCH (+SPACE: SLOWLY)" },
            { "T / U", "CLUTCH SLIP MORE / LESS" },
            { "Y", "KEEP THE CLUTCH AS IT IS" },
            { ".", "BRAKE (HOLD)" },
            { "K", "SMART SHIFTING ON / OFF" },
            { "P", "AVERAGE / INSTANT FUEL" },
        } },
        { "DYNO", {
            { "D", "DYNO ON / OFF" },
            { "H", "HOLD A STEADY RPM" },
            { "G + WHEEL", "HOLD SPEED" },
            { "I", "PEAK POWER AND TORQUE" },
        } },
        { "SOUND", {
            { "Z + WHEEL", "VOLUME" },
            { "X + WHEEL", "CONVOLUTION" },
            { "C + WHEEL", "HIGH FREQUENCY GAIN" },
            { "V + WHEEL", "LOW FREQUENCY NOISE" },
            { "B + WHEEL", "HIGH FREQUENCY NOISE" },
            { "N + WHEEL", "SIMULATION FREQUENCY" },
            { "L", "VOLUME LEVELER" },
            { "6", "WHERE YOU LISTEN FROM" },
        } },
        { "VIEW", {
            { "F1", "THIS KEY LIST" },
            { "TAB", "CHANGE SCREEN" },
            { "F", "FULLSCREEN" },
            { "M  ,", "VIEW LAYER UP / DOWN" },
            { "7 8 9", "TURN THE VIEW / RESET" },
            { "PG UP / DN", "ONE WHEEL STEP" },
        } },
        { "MORE", {
            { "F2", "OPEN THE ENGINE BUILDER" },
            { "F4", "ACCESSIBILITY GUIDE" },
            { "J", "SPEECH: HOW MUCH IS SAID" },
            { "F5 - F12", "READ OUT GAUGES (SPEECH)" },
            { "HOME", "REPEAT THE LAST MESSAGE" },
            { "ESC", "EXIT" },
        } },
    };

    return Groups;
}
