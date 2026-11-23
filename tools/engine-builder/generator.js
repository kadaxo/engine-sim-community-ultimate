// Engine builder: turns a set of plain values (bore, stroke, cam timing,
// exhaust, vehicle...) into an Engine Simulator script (.mr).
//
// Runs in the browser (index.html, as the global EngineBuilder; a plain
// script so the page also works when opened straight from disk) and in Node
// (test/generate-presets.mjs, as a CommonJS module).
// The generated scripts follow Ange Yaghi's stock engines: the cylinder head
// and its flow table come from atg-video-2/03_2jz.mr, scaled to the bore.
//
// Crank pins are computed from the firing order. With the crankshaft's TDC
// at 90 degrees, a cylinder in a bank at angle `bank` fires at crank angle f
// when its rod journal sits at f + bank (mod 360). The stock V6, V8, flat-4
// and inline scripts all follow this, and so do the uneven ones: Ange's
// Harley (banks at -22.5 and 22.5 degrees, one pin, firing at 0 and 315).

(function (root, factory) {
    const api = factory();
    if (typeof module === 'object' && module.exports) module.exports = api;
    else root.EngineBuilder = api;
})(typeof self !== 'undefined' ? self : this, function () {
'use strict';

const FORMAT_VERSION = 1;

const SAVED_VALUES_PREFIX = '// engine-builder: ';

// Cylinders are numbered from 1 at the front. `bank` gives each cylinder's
// bank, `banks` the bank angles in degrees, `firing` the firing order.
// Optional:
//  - `angles`: the crank angle of each firing in the firing order, for
//    engines that don't fire evenly (otherwise every 720 / cylinders degrees)
//  - `pins`: groups of cylinders that share a crank pin (checked by the test)
//  - `flip`: banks whose cylinder head is drawn mirrored (default: bank 1)
//  - `collector`: all cylinders into one exhaust, whatever the routing
//    (a radial's exhaust ring)
const LAYOUTS = {
    single: {
        label: 'Single cylinder',
        banks: [0], bank: [0], firing: [1]
    },
    inline2: {
        label: 'Inline 2 (parallel twin, 360 degree crank)',
        banks: [0], bank: [0, 0], firing: [1, 2]
    },
    // Fires 180 then 540 degrees apart
    inline2_180: {
        label: 'Inline 2 (parallel twin, 180 degree crank, Kawasaki 650)',
        banks: [0], bank: [0, 0], firing: [1, 2], angles: [0, 180]
    },
    // 270 then 450 degrees apart, the firing of a 90 degree V-twin
    inline2_270: {
        label: 'Inline 2 (parallel twin, 270 degree crank, Yamaha MT-07)',
        banks: [0], bank: [0, 0], firing: [1, 2], angles: [0, 270]
    },
    // Opposed cylinders on pins 180 degrees apart reach top dead center
    // together, so a boxer twin fires evenly
    flat2: {
        label: 'Flat 2 (boxer twin, BMW)',
        banks: [90, -90], bank: [0, 1], firing: [1, 2]
    },
    // Both rods on one pin: 315 then 405 degrees apart
    vtwin_45: {
        label: 'V-twin, 45 degrees, shared crank pin (Harley-Davidson)',
        banks: [-22.5, 22.5], bank: [0, 1], firing: [1, 2], angles: [0, 315], pins: [[1, 2]]
    },
    // Both rods on one pin: 270 then 450 degrees apart
    vtwin_90: {
        label: 'V-twin, 90 degrees, shared crank pin (Ducati L-twin)',
        banks: [-45, 45], bank: [0, 1], firing: [1, 2], angles: [0, 270], pins: [[1, 2]]
    },
    inline3: {
        label: 'Inline 3',
        banks: [0], bank: [0, 0, 0], firing: [1, 2, 3]
    },
    inline4: {
        label: 'Inline 4',
        banks: [0], bank: [0, 0, 0, 0], firing: [1, 3, 4, 2]
    },
    // Crank pins 90 degrees apart: 270, 180, 90, 180 degrees between firings
    inline4_crossplane: {
        label: 'Inline 4, crossplane crank (Yamaha R1)',
        banks: [0], bank: [0, 0, 0, 0], firing: [1, 3, 2, 4], angles: [0, 270, 450, 540]
    },
    flat4: {
        label: 'Flat 4 (boxer, Subaru style firing)',
        banks: [90, -90], bank: [0, 1, 0, 1], firing: [1, 3, 2, 4]
    },
    // 1 and 2 in the front bank, 3 and 4 behind them; 1 and 3, 2 and 4 share
    // pins 180 degrees apart: 90, 180, 270, 180 degrees between firings
    v4_90: {
        label: 'V4, 90 degrees, 180 degree crank (Honda VFR800)',
        banks: [-45, 45], bank: [0, 0, 1, 1], firing: [1, 4, 3, 2], angles: [0, 90, 270, 540],
        pins: [[1, 3], [2, 4]]
    },
    // As v4_90 with the pins 70 degrees apart: Ducati's ignition points 0, 90,
    // 290 and 380 degrees, which fire like a big V-twin
    v4_twin_pulse: {
        label: 'V4, 90 degrees, Twin Pulse (Ducati Panigale V4)',
        banks: [-45, 45], bank: [0, 0, 1, 1], firing: [1, 4, 2, 3], angles: [0, 200, 290, 630],
        pins: [[1, 3], [2, 4]]
    },
    inline5: {
        label: 'Inline 5',
        banks: [0], bank: [0, 0, 0, 0, 0], firing: [1, 2, 4, 5, 3]
    },
    inline6: {
        label: 'Inline 6',
        banks: [0], bank: [0, 0, 0, 0, 0, 0], firing: [1, 5, 3, 6, 2, 4]
    },
    flat6: {
        label: 'Flat 6 (boxer)',
        banks: [90, -90], bank: [0, 0, 0, 1, 1, 1], firing: [1, 6, 2, 4, 3, 5]
    },
    v6_60: {
        label: 'V6, 60 degrees',
        banks: [30, -30], bank: [0, 1, 0, 1, 0, 1], firing: [1, 2, 3, 4, 5, 6]
    },
    v6_90: {
        label: 'V6, 90 degrees, even firing',
        banks: [45, -45], bank: [0, 1, 0, 1, 0, 1], firing: [1, 2, 3, 4, 5, 6]
    },
    // Three pins, each shared by a cylinder from each bank: 150 and 90
    // degrees between firings in turn
    v6_90_odd: {
        label: 'V6, 90 degrees, odd firing (Buick 231)',
        banks: [-45, 45], bank: [0, 1, 0, 1, 0, 1], firing: [1, 6, 5, 4, 3, 2],
        angles: [0, 150, 240, 390, 480, 630], pins: [[1, 2], [3, 4], [5, 6]]
    },
    // GM numbers odd cylinders down one bank and even down the other; 1 and
    // 2, 3 and 4, 5 and 6, 7 and 8 share a crank pin
    v8_cross: {
        label: 'V8, cross-plane crank, LS firing order (American V8 burble)',
        banks: [-45, 45], bank: [0, 1, 0, 1, 0, 1, 0, 1], firing: [1, 8, 7, 2, 6, 5, 4, 3],
        pins: [[1, 2], [3, 4], [5, 6], [7, 8]]
    },
    v8_cross_classic: {
        label: 'V8, cross-plane crank, classic firing order (small-block Chevrolet, Chrysler Hemi)',
        banks: [-45, 45], bank: [0, 1, 0, 1, 0, 1, 0, 1], firing: [1, 8, 4, 3, 6, 5, 7, 2],
        pins: [[1, 2], [3, 4], [5, 6], [7, 8]]
    },
    // Ford and Mercedes number cylinders 1-4 down one bank and 5-8 down the
    // other; 1 and 5, 2 and 6, 3 and 7, 4 and 8 share a crank pin
    v8_cross_ford: {
        label: 'V8, cross-plane crank, Ford firing order (5.0 Coyote)',
        banks: [45, -45], bank: [0, 0, 0, 0, 1, 1, 1, 1], firing: [1, 5, 4, 8, 6, 3, 7, 2],
        pins: [[1, 5], [2, 6], [3, 7], [4, 8]]
    },
    v8_cross_mb: {
        label: 'V8, cross-plane crank, Mercedes firing order (M113, M156)',
        banks: [45, -45], bank: [0, 0, 0, 0, 1, 1, 1, 1], firing: [1, 5, 4, 2, 6, 3, 7, 8],
        pins: [[1, 5], [2, 6], [3, 7], [4, 8]]
    },
    v8_flat: {
        label: 'V8, flat-plane crank (Ferrari style scream)',
        banks: [-45, 45], bank: [0, 0, 0, 0, 1, 1, 1, 1], firing: [1, 5, 3, 7, 4, 8, 2, 6]
    },
    v10: {
        label: 'V10, 72 degrees',
        banks: [-36, 36], bank: [0, 0, 0, 0, 0, 1, 1, 1, 1, 1], firing: [1, 6, 5, 10, 2, 7, 3, 8, 4, 9]
    },
    // Five pins 72 degrees apart, each shared by a cylinder from each bank:
    // 54 and 90 degrees between firings in turn
    v10_90_odd: {
        label: 'V10, 90 degrees, odd firing (Dodge Viper)',
        banks: [-45, 45], bank: [0, 1, 0, 1, 0, 1, 0, 1, 0, 1], firing: [1, 10, 9, 4, 3, 6, 5, 8, 7, 2],
        angles: [0, 54, 144, 198, 288, 342, 432, 486, 576, 630],
        pins: [[1, 2], [3, 4], [5, 6], [7, 8], [9, 10]]
    },
    v12: {
        label: 'V12, 60 degrees',
        banks: [-30, 30], bank: [0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1],
        firing: [1, 7, 5, 11, 3, 9, 6, 12, 2, 8, 4, 10]
    },
    radial7: radial(7, 'Radial 7 (aircraft)'),
    radial9: radial(9, 'Radial 9 (aircraft, Wright Cyclone 9)'),
    radial18: radial(18, 'Radial 18, two rows (aircraft, Pratt & Whitney R-2800 style)')
};

// A radial: one cylinder per bank, numbered against the crank's rotation so
// that firing every other cylinder follows it, all on one crank pin. Two rows
// (an even count) alternate front and back around the crank on two pins 180
// degrees apart, firing (count / 2 + 2) cylinders on each time.
function radial(count, label) {
    const twoRows = count % 2 === 0;
    const step = twoRows ? count / 2 + 2 : 2;
    const firing = [];
    for (let i = 0, c = 0; i < count; ++i, c = (c + step) % count) firing.push(c + 1);

    const numbers = Array.from({ length: count }, (_, i) => i + 1);
    return {
        label,
        banks: numbers.map(n => -(n - 1) * 360 / count),
        bank: numbers.map(n => n - 1),
        firing,
        pins: twoRows
            ? [numbers.filter(n => n % 2 === 1), numbers.filter(n => n % 2 === 0)]
            : [numbers],
        flip: [],
        collector: true
    };
}

// molecular_afr is oxygen molecules per fuel molecule for complete burning
// (12.5 for octane). `intakeAfr` is what the intake mixes: stoichiometric for
// petrol and E85, lean for diesel. `script` takes the turbulence effect,
// which sets how much faster the flame gets at high rpm: high-revving engines
// need more or they run out of power before redline (the stock Hayabusa uses
// 5.5).
const FUELS = {
    gasoline: {
        label: 'Petrol (gasoline)',
        intakeAfr: 12.5,
        script: turbulence => `fuel(\n            max_burning_efficiency: 0.9,\n            max_turbulence_effect: ${turbulence}\n        )`
    },
    e85: {
        label: 'E85 ethanol blend',
        intakeAfr: 3.7,
        script: turbulence => `fuel(\n            name: "E85",\n            molecular_mass: 50 * units.g,\n            energy_density: 29.2 * units.kJ / units.g,\n            density: 0.781 * units.kg / units.L,\n            molecular_afr: 3.7,\n            max_burning_efficiency: 0.9,\n            max_turbulence_effect: ${turbulence}\n        )`
    },
    diesel: {
        label: 'Diesel',
        intakeAfr: 23.0,
        script: turbulence => `fuel(\n            name: "Diesel",\n            molecular_mass: 167.3 * units.g,\n            energy_density: 45.68 * units.kJ / units.g,\n            density: 845.0 * units.g / units.L,\n            molecular_afr: 17.75,\n            max_burning_efficiency: 0.8,\n            burning_efficiency_randomness: 0.5,\n            max_turbulence_effect: ${turbulence},\n            max_dilution_effect: 10.0\n        )`
    }
};

// 2 up to a 6000 rpm redline, rising to 6 at 11000 rpm
function turbulenceEffect(redline) {
    return Math.round(Math.min(6, Math.max(2, 2 + (redline - 6000) / 1250)) * 100) / 100;
}

const IMPULSE_RESPONSES = {
    mild_exhaust_0: 'Mild exhaust: a road car with a muffler',
    mild_exhaust_0_reverb: 'Mild exhaust with reverb: as heard outside, with some room echo',
    minimal_muffling_01: 'Minimal muffling 1: loud, open sports exhaust',
    minimal_muffling_02: 'Minimal muffling 2: loud, a little brighter',
    minimal_muffling_03: 'Minimal muffling 3: loud and raw, like a race bike',
    sharp_0: 'Sharp: crisp, metallic',
    default_0: 'Default: Engine Simulator\'s standard sound (smooth 39)',
    real_engine_0: 'Recorded engine 1: from a real exhaust recording',
    real_engine_1: 'Recorded engine 2',
    real_engine_2: 'Recorded engine 3',
    ...Object.fromEntries(Array.from({ length: 49 }, (_, i) => {
        const n = String(i + 1).padStart(2, '0');
        return [`smooth_${n}`, `Smooth ${n}: one of 49 softer exhaust characters`];
    }))
};

const EXHAUST_ROUTING = {
    per_bank: 'One exhaust per cylinder bank (inline engines and radials get one)',
    single: 'All cylinders into one exhaust'
};

const VALVES = {
    4: 'Four valves per cylinder (modern engines)',
    3: 'Three valves per cylinder: two intake, one exhaust (Mercedes M113, Ford Modular 3V)',
    2: 'Two valves per cylinder (older and pushrod engines)'
};

const INJECTION = {
    common_rail: 'Common rail with a pilot injection (modern, quieter)',
    mechanical: 'Mechanical pump without a pilot injection (older, harder knock)'
};

// The form, section by section. `unit` is spoken with the label; numbers are
// in the units shown to the user and converted when the script is written.
const SECTIONS = [
    {
        id: 'engine', title: 'Engine',
        fields: [
            { id: 'name', label: 'Engine name', type: 'text', help: 'Shown in the simulator and spoken when the engine loads.' },
            { id: 'layout', label: 'Cylinder layout', type: 'select', options: () => Object.fromEntries(Object.entries(LAYOUTS).map(([k, v]) => [k, v.label])), help: 'The number and arrangement of cylinders, and the firing order that goes with it.' },
            { id: 'bore', label: 'Bore', unit: 'millimeters', min: 30, max: 200, step: 0.1, help: 'Cylinder diameter.' },
            { id: 'stroke', label: 'Stroke', unit: 'millimeters', min: 30, max: 250, step: 0.1, help: 'How far the piston travels.' },
            { id: 'rodLength', label: 'Connecting rod length', unit: 'millimeters', min: 50, max: 500, step: 0.1, help: 'Center to center. Usually about 1.5 to 1.8 times the stroke.' },
            { id: 'compression', label: 'Compression ratio', unit: 'to 1', min: 6, max: 25, step: 0.1, help: 'About 9 to 12 for petrol engines, 15 to 23 for diesels.' },
            { id: 'redline', label: 'Redline', unit: 'rpm', min: 1000, max: 20000, step: 50, help: 'Where the tachometer turns red and the dyno stops its sweep.' },
            { id: 'revLimit', label: 'Rev limiter', unit: 'rpm', min: 1000, max: 22000, step: 50, help: 'The ignition is cut above this speed. Usually a little above the redline.' },
            { id: 'limiterTime', label: 'Rev limiter cut time', unit: 'seconds', min: 0.01, max: 1, step: 0.01, help: 'How long each cut lasts. Short cuts (0.05) bounce fast like a modern car, long ones (0.5) drop far.' },
            { id: 'starterSpeed', label: 'Starter cranking speed', unit: 'rpm', min: 100, max: 1500, step: 10, help: 'Real starters crank at about 200 to 300 rpm.' },
            { id: 'flywheel', label: 'Flywheel mass', unit: 'kilograms', min: 0.5, max: 100, step: 0.1, help: 'A light flywheel revs up and down quickly, a heavy one slowly.' },
            { id: 'fuel', label: 'Fuel', type: 'select', options: () => Object.fromEntries(Object.entries(FUELS).map(([k, v]) => [k, v.label])), help: 'A diesel has no throttle: the pedal sets the fuel, which lights by itself in the hot compressed air. Its settings are in the Diesel injection section.' }
        ]
    },
    {
        id: 'valvetrain', title: 'Valves and camshafts',
        fields: [
            { id: 'valves', label: 'Valves per cylinder', type: 'select', options: () => VALVES, help: 'Four valves breathe better at high rpm.' },
            { id: 'intakeDuration', label: 'Intake cam duration at 0.050 inch lift', unit: 'degrees', min: 160, max: 320, step: 1, help: 'Longer duration moves power to higher rpm and makes a lumpier idle. Mild road cams are about 200 to 220.' },
            { id: 'exhaustDuration', label: 'Exhaust cam duration at 0.050 inch lift', unit: 'degrees', min: 160, max: 320, step: 1, help: 'Usually a little shorter than or equal to the intake.' },
            { id: 'intakeLift', label: 'Intake valve lift', unit: 'millimeters', min: 2, max: 20, step: 0.1, help: 'How far the valve opens.' },
            { id: 'exhaustLift', label: 'Exhaust valve lift', unit: 'millimeters', min: 2, max: 20, step: 0.1, help: '' },
            { id: 'lobeSeparation', label: 'Lobe separation angle', unit: 'degrees', min: 95, max: 125, step: 0.5, help: 'Smaller values give more valve overlap, a rougher idle and more mid-range punch. 110 to 116 is typical.' },
            { id: 'camAdvance', label: 'Cam advance', unit: 'degrees', min: -10, max: 15, step: 0.5, help: 'Advancing the cams moves power to lower rpm.' }
        ]
    },
    {
        id: 'intake', title: 'Intake',
        fields: [
            { id: 'throttleFlow', label: 'Throttle body flow', unit: 'cubic feet per minute', min: 50, max: 3000, step: 10, help: 'How much air the throttle passes when wide open. Too small and the engine runs out of breath at high rpm.' },
            { id: 'runnerLength', label: 'Intake runner length', unit: 'millimeters', min: 50, max: 1500, step: 5, help: 'Long runners help low rpm torque, short ones high rpm power.' },
            { id: 'plenumVolume', label: 'Intake plenum volume', unit: 'liters', min: 0.1, max: 20, step: 0.05, help: '' },
            { id: 'idleOpening', label: 'Throttle opening at idle', unit: 'percent', min: 0, max: 5, step: 0.01, help: 'Sets the idle speed. Raise it if the engine stalls, lower it if it idles too fast. Diesels don\'t use it: their governor holds the idle.' }
        ]
    },
    {
        id: 'diesel', title: 'Diesel injection (diesel fuel only)',
        fields: [
            { id: 'dieselIdleSpeed', label: 'Diesel idle speed', unit: 'rpm', min: 300, max: 2000, step: 10, help: 'The governor holds this speed with the fuel when the pedal is up.' },
            { id: 'injection', label: 'Injection system', type: 'select', options: () => INJECTION, help: 'A small pilot injection before the main one makes a modern diesel quieter.' },
            { id: 'dieselNoise', label: 'Diesel clatter loudness', unit: 'times', min: 0, max: 5, step: 0.05, help: '1 is the knock the combustion makes. 0 leaves the engine block silent.' }
        ]
    },
    {
        id: 'exhaust', title: 'Exhaust and sound',
        fields: [
            { id: 'exhaustRouting', label: 'Exhaust routing', type: 'select', options: () => EXHAUST_ROUTING, help: 'Separate exhausts per bank give a V8 its burble.' },
            { id: 'primaryLength', label: 'Header primary pipe length', unit: 'millimeters', min: 50, max: 3000, step: 5, help: 'The pipe from each cylinder to the collector. Changes the pitch of the exhaust pulses.' },
            { id: 'collectorDiameter', label: 'Exhaust pipe diameter', unit: 'millimeters', min: 20, max: 200, step: 1, help: '' },
            { id: 'pipeLength', label: 'Exhaust pipe length, collector to tailpipe', unit: 'millimeters', min: 200, max: 10000, step: 10, help: 'Longer pipes give a deeper, more resonant note.' },
            { id: 'impulseResponse', label: 'Exhaust character (impulse response)', type: 'select', options: () => IMPULSE_RESPONSES, help: 'A recording that shapes how the exhaust sounds: muffler, pipe and surroundings.' },
            { id: 'exhaustVolume', label: 'Exhaust loudness', unit: 'times', min: 0.05, max: 20, step: 0.05, help: '1 is a normal level. The volume leveler in the simulator evens out most differences.' }
        ]
    },
    {
        id: 'ignition', title: 'Ignition',
        fields: [
            { id: 'timingIdle', label: 'Ignition timing at idle', unit: 'degrees before top dead center', min: 0, max: 40, step: 0.5, help: 'For a diesel, this is when the main injection starts: about 2 to 8 at idle.' },
            { id: 'timingMax', label: 'Ignition timing at full advance', unit: 'degrees before top dead center', min: 0, max: 60, step: 0.5, help: 'Usually 30 to 38 for petrol engines, and an injection timing of 10 to 20 for diesels.' },
            { id: 'timingMaxRpm', label: 'Full advance reached at', unit: 'rpm', min: 1000, max: 15000, step: 100, help: '' }
        ]
    },
    {
        id: 'vehicle', title: 'Vehicle',
        fields: [
            { id: 'mass', label: 'Vehicle mass', unit: 'kilograms', min: 50, max: 60000, step: 1, help: '' },
            { id: 'dragCoefficient', label: 'Drag coefficient', unit: '', min: 0.05, max: 1.5, step: 0.01, help: 'About 0.3 for a modern car.' },
            { id: 'frontalArea', label: 'Frontal area', unit: 'square meters', min: 0.2, max: 15, step: 0.01, help: 'About 2 for a car.' },
            { id: 'tireRadius', label: 'Tyre radius', unit: 'millimeters', min: 150, max: 1000, step: 1, help: 'About 300 for a car.' },
            { id: 'finalDrive', label: 'Final drive ratio', unit: 'to 1', min: 1, max: 10, step: 0.01, help: '' },
            { id: 'rollingResistance', label: 'Rolling resistance', unit: 'newtons', min: 0, max: 20000, step: 10, help: '' },
            { id: 'brakeForce', label: 'Brake force', unit: 'newtons, 0 for automatic', min: 0, max: 500000, step: 100, help: '0 gives brakes of about 1 g for the vehicle\'s mass.' }
        ]
    },
    {
        id: 'transmission', title: 'Transmission',
        fields: [
            { id: 'gears', label: 'Gear ratios, first gear first, separated by commas', type: 'text', help: 'For example 3.5, 2.1, 1.4, 1.0, 0.8.' },
            { id: 'clutchTorque', label: 'Clutch torque capacity', unit: 'newton meters', min: 50, max: 20000, step: 10, help: 'More than the engine\'s peak torque, or the clutch slips under full throttle.' }
        ]
    }
];

// Diesel settings for scripts saved before they existed
const DIESEL_DEFAULTS = { dieselIdleSpeed: 750, injection: 'common_rail', dieselNoise: 1 };

const BASE = {
    ...DIESEL_DEFAULTS,
    exhaustRouting: 'per_bank', valves: '4', fuel: 'gasoline', impulseResponse: 'mild_exhaust_0',
    limiterTime: 0.08, starterSpeed: 250, camAdvance: 0, idleOpening: 0.2, exhaustVolume: 1,
    collectorDiameter: 55, pipeLength: 2500, timingIdle: 12, timingMax: 34,
    dragCoefficient: 0.32, frontalArea: 2.0, tireRadius: 310, rollingResistance: 300, brakeForce: 0
};

// Starting points the form can be filled with
const PRESETS = {
    i4_16: {
        label: 'Small inline 4, 1.6 liters', values: {
            ...BASE, name: 'Builder 1.6 I4', layout: 'inline4', bore: 79, stroke: 81.5, rodLength: 137,
            compression: 10.5, redline: 6800, revLimit: 7000, flywheel: 7,
            intakeDuration: 216, exhaustDuration: 212, intakeLift: 8.5, exhaustLift: 8.2,
            lobeSeparation: 112, throttleFlow: 450, runnerLength: 330, plenumVolume: 1.0,
            primaryLength: 600, timingMaxRpm: 4500, mass: 1150, finalDrive: 4.1,
            gears: '3.45, 1.95, 1.33, 1.03, 0.82', clutchTorque: 300
        }
    },
    i6_30: {
        label: 'Inline 6, 3.0 liters', values: {
            ...BASE, name: 'Builder 3.0 I6', layout: 'inline6', bore: 86, stroke: 86, rodLength: 142,
            compression: 10.5, redline: 7000, revLimit: 7200, flywheel: 9,
            intakeDuration: 224, exhaustDuration: 220, intakeLift: 9.8, exhaustLift: 9.6,
            lobeSeparation: 114, throttleFlow: 750, runnerLength: 400, plenumVolume: 1.8,
            primaryLength: 800, timingMaxRpm: 4500, mass: 1500, finalDrive: 3.3,
            gears: '3.8, 2.2, 1.5, 1.2, 1.0, 0.8', clutchTorque: 550, exhaustRouting: 'single'
        }
    },
    v8_57: {
        label: 'V8, 5.7 liters, cross-plane', values: {
            ...BASE, name: 'Builder 5.7 V8', layout: 'v8_cross', bore: 99, stroke: 92, rodLength: 155,
            idleOpening: 0.25, compression: 10.0, redline: 6200, revLimit: 6400, flywheel: 12, valves: '2',
            intakeDuration: 224, exhaustDuration: 230, intakeLift: 12.5, exhaustLift: 12.5,
            lobeSeparation: 112, throttleFlow: 1100, runnerLength: 300, plenumVolume: 2.5,
            primaryLength: 750, timingMaxRpm: 3500, mass: 1700, finalDrive: 3.42,
            gears: '2.97, 2.07, 1.43, 1.0, 0.84, 0.56', clutchTorque: 900, collectorDiameter: 65
        }
    },
    v8_flat: {
        label: 'V8, 4.5 liters, flat-plane', values: {
            ...BASE, name: 'Builder 4.5 flat-plane V8', layout: 'v8_flat', bore: 94, stroke: 81, rodLength: 140,
            compression: 12.5, redline: 9000, revLimit: 9200, flywheel: 5, limiterTime: 0.05,
            intakeDuration: 238, exhaustDuration: 234, intakeLift: 12, exhaustLift: 11,
            lobeSeparation: 114, throttleFlow: 1400, runnerLength: 220, plenumVolume: 2.5,
            primaryLength: 550, timingMaxRpm: 6000, mass: 1450, finalDrive: 4.4,
            gears: '3.08, 2.19, 1.63, 1.29, 1.03, 0.84, 0.69', clutchTorque: 800,
            impulseResponse: 'minimal_muffling_01', collectorDiameter: 60
        }
    },
    flat4_25: {
        label: 'Flat 4, 2.5 liters', values: {
            ...BASE, name: 'Builder 2.5 flat 4', layout: 'flat4', bore: 99.5, stroke: 79, rodLength: 130,
            compression: 10.0, redline: 6500, revLimit: 6700, flywheel: 7,
            intakeDuration: 224, exhaustDuration: 220, intakeLift: 9.5, exhaustLift: 9.0,
            lobeSeparation: 114, throttleFlow: 650, runnerLength: 330, plenumVolume: 1.4,
            primaryLength: 450, timingMaxRpm: 4500, mass: 1400, finalDrive: 3.9,
            gears: '3.45, 2.06, 1.45, 1.09, 0.78', clutchTorque: 450
        }
    },
    v6_35: {
        label: 'V6, 3.5 liters, 60 degrees', values: {
            ...BASE, name: 'Builder 3.5 V6', layout: 'v6_60', bore: 95, stroke: 81, rodLength: 152,
            compression: 10.3, redline: 6600, revLimit: 6800, flywheel: 9,
            intakeDuration: 228, exhaustDuration: 224, intakeLift: 10, exhaustLift: 9.5,
            lobeSeparation: 112, throttleFlow: 800, runnerLength: 380, plenumVolume: 2.0,
            primaryLength: 650, timingMaxRpm: 4500, mass: 1600, finalDrive: 3.5,
            gears: '3.6, 2.1, 1.5, 1.1, 0.87, 0.69', clutchTorque: 600
        }
    },
    single_450: {
        label: 'Single cylinder, 450 cc motorcycle', values: {
            ...BASE, name: 'Builder 450 single', layout: 'single', bore: 96, stroke: 62.1, rodLength: 110,
            compression: 12.5, redline: 11000, revLimit: 11300, flywheel: 1.5, limiterTime: 0.05,
            intakeDuration: 238, exhaustDuration: 230, intakeLift: 10, exhaustLift: 8.5, idleOpening: 0.08,
            lobeSeparation: 110, throttleFlow: 300, runnerLength: 250, plenumVolume: 1.5,
            primaryLength: 700, timingMax: 40, timingMaxRpm: 7000, mass: 200, finalDrive: 3.6,
            gears: '2.4, 1.8, 1.45, 1.2, 1.0', clutchTorque: 150, dragCoefficient: 0.6,
            frontalArea: 0.6, tireRadius: 320, rollingResistance: 80,
            impulseResponse: 'minimal_muffling_03', collectorDiameter: 45, pipeLength: 900
        }
    },
    v12_65: {
        label: 'V12, 6.5 liters', values: {
            ...BASE, name: 'Builder 6.5 V12', layout: 'v12', bore: 94, stroke: 78, rodLength: 140,
            compression: 13.0, redline: 8700, revLimit: 8900, flywheel: 6, limiterTime: 0.05,
            intakeDuration: 234, exhaustDuration: 230, intakeLift: 11.5, exhaustLift: 10.5,
            lobeSeparation: 114, throttleFlow: 1800, runnerLength: 250, plenumVolume: 3.5,
            primaryLength: 500, timingMaxRpm: 6000, mass: 1650, finalDrive: 3.9,
            gears: '3.1, 2.1, 1.6, 1.3, 1.05, 0.85, 0.7', clutchTorque: 1000,
            impulseResponse: 'minimal_muffling_01', collectorDiameter: 60
        }
    },
    i3_10: {
        label: 'Inline 3, 1.0 liter', values: {
            ...BASE, name: 'Builder 1.0 I3', layout: 'inline3', bore: 71, stroke: 84, rodLength: 133,
            compression: 10.5, redline: 6300, revLimit: 6500, flywheel: 7,
            intakeDuration: 212, exhaustDuration: 208, intakeLift: 8, exhaustLift: 7.6,
            lobeSeparation: 114, throttleFlow: 300, runnerLength: 350, plenumVolume: 0.8,
            primaryLength: 500, timingMaxRpm: 4000, mass: 1000, finalDrive: 4.1,
            gears: '3.6, 1.95, 1.28, 0.95, 0.76', clutchTorque: 220
        }
    },
    i5_25: {
        label: 'Inline 5, 2.5 liters', values: {
            ...BASE, name: 'Builder 2.5 I5', layout: 'inline5', bore: 82.5, stroke: 92.8, rodLength: 144,
            compression: 10.0, redline: 6800, revLimit: 7000, flywheel: 8,
            intakeDuration: 226, exhaustDuration: 222, intakeLift: 10, exhaustLift: 9.5,
            lobeSeparation: 112, throttleFlow: 650, runnerLength: 380, plenumVolume: 1.5,
            primaryLength: 700, timingMaxRpm: 4500, mass: 1400, finalDrive: 3.7,
            gears: '3.6, 2.1, 1.4, 1.03, 0.82, 0.68', clutchTorque: 500, exhaustRouting: 'single'
        }
    },
    // Sized on a Harley-Davidson Milwaukee-Eight 107 (100 x 111.1 mm)
    vtwin_45_18: {
        label: 'V-twin, 1.75 liters, 45 degrees (Harley-Davidson style)', values: {
            ...BASE, name: 'Builder 1.75 45-degree V-twin', layout: 'vtwin_45', bore: 100, stroke: 111.1, rodLength: 190,
            compression: 10.0, redline: 5500, revLimit: 5700, flywheel: 12, valves: '4',
            intakeDuration: 220, exhaustDuration: 226, intakeLift: 11.5, exhaustLift: 11.5,
            lobeSeparation: 108, throttleFlow: 600, runnerLength: 220, plenumVolume: 1.0,
            primaryLength: 900, timingIdle: 10, timingMax: 30, timingMaxRpm: 3500, mass: 420, finalDrive: 3.2,
            gears: '3.34, 2.31, 1.72, 1.39, 1.18, 1.0', clutchTorque: 300, dragCoefficient: 0.7,
            frontalArea: 0.9, tireRadius: 330, rollingResistance: 120,
            impulseResponse: 'minimal_muffling_01', collectorDiameter: 50, pipeLength: 1000
        }
    },
    // Sized on a Ducati Panigale V2 (100 x 60.8 mm)
    vtwin_90_10: {
        label: 'V-twin, 955 cc, 90 degrees (Ducati style)', values: {
            ...BASE, name: 'Builder 955 L-twin', layout: 'vtwin_90', bore: 100, stroke: 60.8, rodLength: 124,
            compression: 12.5, redline: 10750, revLimit: 11000, flywheel: 2, limiterTime: 0.05,
            intakeDuration: 250, exhaustDuration: 240, intakeLift: 11.5, exhaustLift: 10, idleOpening: 0.15,
            lobeSeparation: 106, throttleFlow: 500, runnerLength: 200, plenumVolume: 1.5,
            primaryLength: 600, timingMax: 38, timingMaxRpm: 8000, mass: 260, finalDrive: 5.0,
            gears: '2.67, 1.94, 1.56, 1.32, 1.17, 1.04', clutchTorque: 250, dragCoefficient: 0.6,
            frontalArea: 0.6, tireRadius: 320, rollingResistance: 80, exhaustRouting: 'single',
            impulseResponse: 'minimal_muffling_03', collectorDiameter: 50, pipeLength: 700
        }
    },
    // Sized on a Yamaha CP2 (80 x 68.6 mm)
    inline2_270_07: {
        label: 'Parallel twin, 690 cc, 270 degree crank (Yamaha MT-07 style)', values: {
            ...BASE, name: 'Builder 690 270-degree twin', layout: 'inline2_270', bore: 80, stroke: 68.6, rodLength: 116,
            compression: 11.5, redline: 9500, revLimit: 9800, flywheel: 2.5, limiterTime: 0.05,
            intakeDuration: 230, exhaustDuration: 226, intakeLift: 9, exhaustLift: 8, idleOpening: 0.2,
            lobeSeparation: 108, throttleFlow: 300, runnerLength: 250, plenumVolume: 1.2,
            primaryLength: 600, timingMax: 36, timingMaxRpm: 6500, mass: 260, finalDrive: 5.0,
            gears: '2.85, 2.13, 1.63, 1.3, 1.09, 0.96', clutchTorque: 200, dragCoefficient: 0.6,
            frontalArea: 0.6, tireRadius: 315, rollingResistance: 80, exhaustRouting: 'single',
            impulseResponse: 'minimal_muffling_02', collectorDiameter: 45, pipeLength: 800
        }
    },
    // Sized on a Yamaha R1 (79 x 50.9 mm)
    i4_crossplane_10: {
        label: 'Inline 4, 1.0 liter, crossplane crank (Yamaha R1 style)', values: {
            ...BASE, name: 'Builder 1.0 crossplane I4', layout: 'inline4_crossplane', bore: 79, stroke: 50.9, rodLength: 102,
            compression: 13.0, redline: 14000, revLimit: 14200, flywheel: 3, limiterTime: 0.05,
            intakeDuration: 260, exhaustDuration: 256, intakeLift: 9.3, exhaustLift: 8.7, idleOpening: 0.35,
            lobeSeparation: 108, throttleFlow: 600, runnerLength: 150, plenumVolume: 2.5,
            primaryLength: 450, timingIdle: 14, timingMax: 40, timingMaxRpm: 10000, mass: 270, finalDrive: 3.9,
            gears: '2.6, 2.18, 1.84, 1.6, 1.41, 1.2', clutchTorque: 300, dragCoefficient: 0.6,
            frontalArea: 0.6, tireRadius: 320, rollingResistance: 80, exhaustRouting: 'single',
            impulseResponse: 'minimal_muffling_03', collectorDiameter: 50, pipeLength: 700
        }
    },
    // Sized on a Ducati Desmosedici Stradale (81 x 53.5 mm)
    v4_twin_pulse_11: {
        label: 'V4, 1.1 liters, Twin Pulse (Ducati Panigale V4 style)', values: {
            ...BASE, name: 'Builder 1.1 Twin Pulse V4', layout: 'v4_twin_pulse', bore: 81, stroke: 53.5, rodLength: 104,
            compression: 14.0, redline: 14500, revLimit: 14700, flywheel: 3, limiterTime: 0.05,
            intakeDuration: 266, exhaustDuration: 258, intakeLift: 12, exhaustLift: 10, idleOpening: 0.3,
            lobeSeparation: 106, throttleFlow: 900, runnerLength: 150, plenumVolume: 3.0,
            primaryLength: 450, timingIdle: 14, timingMax: 40, timingMaxRpm: 10000, mass: 270, finalDrive: 4.0,
            gears: '2.69, 2.11, 1.77, 1.52, 1.35, 1.22', clutchTorque: 350, dragCoefficient: 0.6,
            frontalArea: 0.6, tireRadius: 320, rollingResistance: 80, exhaustRouting: 'single',
            impulseResponse: 'minimal_muffling_03', collectorDiameter: 55, pipeLength: 700
        }
    },
    // Sized on a Buick 231 (96.5 x 86.4 mm)
    v6_odd_38: {
        label: 'V6, 3.8 liters, odd firing (Buick 231 style)', values: {
            ...BASE, name: 'Builder 3.8 odd-fire V6', layout: 'v6_90_odd', bore: 96.5, stroke: 86.4, rodLength: 151,
            idleOpening: 0.65, compression: 8.0, redline: 4800, revLimit: 5000, flywheel: 12, valves: '2',
            intakeDuration: 200, exhaustDuration: 206, intakeLift: 9, exhaustLift: 9,
            lobeSeparation: 114, throttleFlow: 350, runnerLength: 250, plenumVolume: 2.0,
            primaryLength: 500, timingMax: 32, timingMaxRpm: 3000, mass: 1600, finalDrive: 2.73,
            gears: '2.52, 1.52, 1.0', clutchTorque: 400
        }
    },
    // Sized on a first Dodge Viper (101.6 x 98.6 mm)
    v10_odd_80: {
        label: 'V10, 8.0 liters, odd firing (Dodge Viper style)', values: {
            ...BASE, name: 'Builder 8.0 odd-fire V10', layout: 'v10_90_odd', bore: 101.6, stroke: 98.6, rodLength: 156,
            idleOpening: 0.45, compression: 9.1, redline: 6000, revLimit: 6200, flywheel: 14, valves: '2',
            intakeDuration: 212, exhaustDuration: 220, intakeLift: 11.5, exhaustLift: 11.5,
            lobeSeparation: 114, throttleFlow: 1100, runnerLength: 450, plenumVolume: 3.0,
            primaryLength: 700, timingMaxRpm: 4000, mass: 1550, finalDrive: 3.07,
            gears: '2.66, 1.78, 1.3, 1.0, 0.74, 0.5', clutchTorque: 900,
            impulseResponse: 'minimal_muffling_01', collectorDiameter: 65, pipeLength: 1500
        }
    },
    // Sized on a Ford Coyote (92.2 x 92.7 mm)
    v8_ford_50: {
        label: 'V8, 5.0 liters, Ford firing order (Coyote style)', values: {
            ...BASE, name: 'Builder 5.0 Ford V8', layout: 'v8_cross_ford', bore: 92.2, stroke: 92.7, rodLength: 150.7,
            idleOpening: 0.25, compression: 11.0, redline: 7400, revLimit: 7500, flywheel: 10,
            intakeDuration: 236, exhaustDuration: 232, intakeLift: 12, exhaustLift: 11.5,
            lobeSeparation: 115, throttleFlow: 1000, runnerLength: 350, plenumVolume: 3.0,
            primaryLength: 600, timingMaxRpm: 4500, mass: 1700, finalDrive: 3.73,
            gears: '3.66, 2.43, 1.69, 1.32, 1.0, 0.65', clutchTorque: 800, collectorDiameter: 65
        }
    },
    // Sized on a Wright R-1820 Cyclone (155.6 x 174.6 mm, 30 liters), on a
    // heavy truck for the dyno and gears
    radial9_30: {
        label: 'Radial 9, 30 liters (Wright Cyclone style)', values: {
            ...BASE, name: 'Builder 30 radial 9', layout: 'radial9', bore: 155.6, stroke: 174.6, rodLength: 330,
            compression: 6.7, redline: 2500, revLimit: 2700, flywheel: 60, valves: '2', starterSpeed: 150,
            intakeDuration: 240, exhaustDuration: 240, intakeLift: 14, exhaustLift: 14, idleOpening: 0.6,
            lobeSeparation: 110, throttleFlow: 2000, runnerLength: 600, plenumVolume: 8.0,
            primaryLength: 400, timingIdle: 20, timingMax: 25, timingMaxRpm: 1500, mass: 8000, finalDrive: 3.0,
            gears: '4.0, 2.5, 1.6, 1.0', clutchTorque: 6000, dragCoefficient: 0.6, frontalArea: 6,
            tireRadius: 500, rollingResistance: 2000,
            impulseResponse: 'minimal_muffling_01', collectorDiameter: 120, pipeLength: 1500
        }
    },
    diesel_i4: {
        label: 'Diesel inline 4, 2.0 liters', values: {
            ...BASE, name: 'Builder 2.0 diesel I4', layout: 'inline4', bore: 81, stroke: 95.5, rodLength: 144,
            compression: 16.5, redline: 4500, revLimit: 4800, flywheel: 11, fuel: 'diesel',
            intakeDuration: 200, exhaustDuration: 196, intakeLift: 8, exhaustLift: 7.5,
            lobeSeparation: 114, throttleFlow: 450, runnerLength: 350, plenumVolume: 1.2,
            primaryLength: 500, timingIdle: 6, timingMax: 20, timingMaxRpm: 3000, mass: 1450,
            finalDrive: 3.4, gears: '3.8, 2.1, 1.3, 0.9, 0.7, 0.58', clutchTorque: 500, idleOpening: 0.3,
            dieselIdleSpeed: 800, injection: 'common_rail', dieselNoise: 1
        }
    }
};

const DEFAULT_PRESET = 'i4_16';

function fieldList() {
    return SECTIONS.flatMap(section => section.fields);
}

function parseGears(text) {
    return String(text).split(/[,;\s]+/).filter(s => s.length > 0).map(Number);
}

// Problems with the values, as [{ field, message }]. Messages are full
// sentences that name the field, so they make sense when read out alone.
function validate(values) {
    const errors = [];

    for (const field of fieldList()) {
        const value = values[field.id];
        if (field.type === 'text') {
            if (field.id === 'name' && String(value || '').trim() === '') {
                errors.push({ field: 'name', message: 'Engine name is empty.' });
            }
            continue;
        }

        if (field.type === 'select') {
            const options = field.options();
            if (!(String(value) in options)) {
                errors.push({ field: field.id, message: `${field.label}: choose one of the options.` });
            }
            continue;
        }

        const n = Number(value);
        if (value === '' || value === null || value === undefined || !Number.isFinite(n)) {
            errors.push({ field: field.id, message: `${field.label} must be a number.` });
        }
        else if (n < field.min || n > field.max) {
            errors.push({ field: field.id, message: `${field.label} must be between ${field.min} and ${field.max} ${field.unit}, not ${n}.`.replace(/\s+\./, '.') });
        }
    }

    const gears = parseGears(values.gears);
    if (gears.length === 0 || gears.length > 10 || gears.some(g => !Number.isFinite(g) || g <= 0 || g > 10)) {
        errors.push({ field: 'gears', message: 'Gear ratios must be 1 to 10 numbers above 0 and at most 10, separated by commas.' });
    }

    const n = name => Number(values[name]);
    if (n('revLimit') < n('redline')) {
        errors.push({ field: 'revLimit', message: 'Rev limiter must be at or above the redline.' });
    }
    if (n('rodLength') < 1.2 * n('stroke') / 2 + 0.3 * n('bore')) {
        errors.push({ field: 'rodLength', message: 'Connecting rod length is too short for this stroke: the rod would hit the cylinder.' });
    }
    if (n('timingMax') < n('timingIdle')) {
        errors.push({ field: 'timingMax', message: 'Ignition timing at full advance must be at least the idle timing.' });
    }

    return errors;
}

// ---------------------------------------------------------------------------
// Script writing

function num(value, decimals = 4) {
    const s = Number(value).toFixed(decimals);
    return s.includes('.') ? s.replace(/0+$/, '').replace(/\.$/, '.0') : s;
}

function angle(degrees) {
    return ((degrees % 360) + 360) % 360;
}

function identifier(name) {
    const base = String(name).toLowerCase().replace(/[^a-z0-9]+/g, '_').replace(/^_+|_+$/g, '');
    return 'eb_' + (base || 'engine');
}

function quoted(text) {
    return '"' + String(text).replace(/\\/g, '\\\\').replace(/"/g, '\\"').replace(/[\r\n]+/g, ' ') + '"';
}

// The 2JZ head's flow bench numbers (CFM at 28 inH2O against lift in thou),
// for an 86 mm bore with four valves
const INTAKE_FLOW = [0, 58, 103, 156, 214, 249, 268, 280, 280, 281];
const EXHAUST_FLOW = [0, 37, 72, 113, 160, 196, 222, 235, 245, 246];

function flowFunction(name, table, attenuation) {
    const lines = table.map((flow, i) =>
        `        .add_flow_sample(${i * 50} * lift_scale, ${flow} * ${attenuation})`);
    return `    function ${name}(50 * units.thou)\n    ${name}\n${lines.join('\n')}`;
}

// The crank angle of each firing in the firing order [degrees]
function firingAngles(layout) {
    return layout.angles || layout.firing.map((_, i) => i * 720 / layout.firing.length);
}

// Per cylinder: firing angle and rod journal angle, both in degrees
function crankLayout(layoutId) {
    const layout = LAYOUTS[layoutId];
    const angles = firingAngles(layout);

    const cylinders = layout.bank.map((bank, i) => ({ number: i + 1, bank, firing: 0, journal: 0 }));
    layout.firing.forEach((number, position) => {
        const cylinder = cylinders[number - 1];
        cylinder.firing = angles[position];
        cylinder.journal = angle(cylinder.firing + layout.banks[cylinder.bank]);
    });

    return cylinders;
}

function generate(values) {
    const v = { ...values };
    const layout = LAYOUTS[v.layout];
    const cylinders = crankLayout(v.layout);
    const count = cylinders.length;
    const id = identifier(v.name);

    const bore = Number(v.bore);                 // mm
    const stroke = Number(v.stroke);             // mm
    const sweptCc = Math.PI / 4 * (bore / 10) ** 2 * (stroke / 10);
    const displacementL = sweptCc * count / 1000;
    const chamberCc = sweptCc / (Number(v.compression) - 1);
    const scale = bore / 86;
    // A two-valve port flows about 0.8 of a four-valve one; three valves are
    // two intake and one exhaust
    const intakeValveFactor = Number(v.valves) === 2 ? 0.8 : 1.0;
    const exhaustValveFactor = Number(v.valves) === 4 ? 1.0 : 0.8;
    const redline = Number(v.redline);

    const simulationFrequency = redline > 9500 ? 20000 : (redline > 7000 ? 15000 : 10000);
    // Up to 2000 lb-ft for an aircraft radial (400 couldn't turn 46 liters)
    const starterTorque = Math.min(2000, Math.max(40, 50 + 40 * displacementL));
    const crankMass = Math.max(2, 2.5 * count * stroke / 86);
    const pistonMass = 250 * scale ** 3;
    const rodMass = 500 * scale ** 2 * Number(v.rodLength) / 142;
    const compressionHeight = 0.38 * bore;
    const frictionTorque = Math.max(0.5, 2.0 * displacementL);

    // Runner and header flow follow the air each cylinder needs at redline,
    // scaled so the 2JZ (0.5 L per cylinder, 6000 rpm) gets its stock values
    const cylinderDemand = (sweptCc / 1000) * redline;
    const runnerFlow = Math.max(80, 0.07 * cylinderDemand);
    const primaryFlow = Math.max(150, 0.135 * cylinderDemand);

    // Port area follows the cylinder's size. The charge's speed through the
    // port drives the turbulence that speeds up burning, so oversized ports
    // (bore squared from the 2JZ's 23 cm2) cost a lot of power.
    const intakePortArea = Math.min(30, Math.max(6, 0.028 * sweptCc));
    const exhaustPortArea = Math.min(25, Math.max(5, 0.02 * sweptCc));

    const banks = layout.banks.map((bankAngle, b) => ({
        index: b,
        angle: bankAngle,
        cylinders: cylinders.filter(c => c.bank === b)
    }));

    const exhausts = (v.exhaustRouting === 'single' || layout.collector) ? 1 : banks.length;
    const flipped = layout.flip || [1];
    const exhaustOf = cylinder => (exhausts === 1) ? 0 : cylinder.bank;

    const out = [];
    out.push('// Made with the Engine Simulator engine builder (tools/engine-builder/index.html).');
    out.push('// Open this file in the builder to change it there, or edit it here by hand.');
    out.push(SAVED_VALUES_PREFIX + JSON.stringify({ version: FORMAT_VERSION, values: v }));
    out.push('');
    out.push('import "engine_sim.mr"');
    out.push('');
    out.push('units units()');
    out.push('constants constants()');
    out.push('impulse_response_library ir_lib()');
    out.push('label cycle(2 * 360 * units.deg)');
    out.push('');

    // Ignition wires
    out.push(`private node ${id}_wires {`);
    for (const c of cylinders) out.push(`    output wire${c.number}: ignition_wire();`);
    out.push('}');
    out.push('');

    // Cylinder head
    out.push(`private node ${id}_head {`);
    out.push('    input intake_camshaft;');
    out.push('    input exhaust_camshaft;');
    out.push('    input chamber_volume;');
    out.push(`    input intake_runner_volume: ${num(150 * scale ** 2, 1)} * units.cc;`);
    out.push(`    input intake_runner_cross_section_area: ${num(intakePortArea, 2)} * units.cm2;`);
    out.push(`    input exhaust_runner_volume: ${num(50 * scale ** 2, 1)} * units.cc;`);
    out.push(`    input exhaust_runner_cross_section_area: ${num(exhaustPortArea, 2)} * units.cm2;`);
    out.push(`    input flow_attenuation: ${num(scale ** 2 * intakeValveFactor)};`);
    out.push(`    input exhaust_flow_attenuation: ${num(scale ** 2 * exhaustValveFactor)};`);
    out.push(`    input lift_scale: ${num(scale)};`);
    out.push('    input flip_display: false;');
    out.push('    alias output __out: head;');
    out.push('');
    out.push(flowFunction('intake_flow', INTAKE_FLOW, 'flow_attenuation'));
    out.push('');
    out.push(flowFunction('exhaust_flow', EXHAUST_FLOW, 'exhaust_flow_attenuation'));
    out.push('');
    out.push('    generic_cylinder_head head(');
    out.push('        chamber_volume: chamber_volume,');
    out.push('        intake_runner_volume: intake_runner_volume,');
    out.push('        intake_runner_cross_section_area: intake_runner_cross_section_area,');
    out.push('        exhaust_runner_volume: exhaust_runner_volume,');
    out.push('        exhaust_runner_cross_section_area: exhaust_runner_cross_section_area,');
    out.push('        intake_port_flow: intake_flow,');
    out.push('        exhaust_port_flow: exhaust_flow,');
    out.push('        valvetrain: standard_valvetrain(');
    out.push('            intake_camshaft: intake_camshaft,');
    out.push('            exhaust_camshaft: exhaust_camshaft');
    out.push('        ),');
    out.push('        flip_display: flip_display');
    out.push('    )');
    out.push('}');
    out.push('');

    // One camshaft pair per bank, a lobe at each cylinder's firing angle
    for (const bank of banks) {
        out.push(`private node ${id}_camshaft_${bank.index} {`);
        out.push('    input intake_lobe_profile;');
        out.push('    input exhaust_lobe_profile;');
        out.push('    input intake_lobe_center;');
        out.push('    input exhaust_lobe_center;');
        out.push('    input base_radius;');
        out.push('    output intake_cam: _intake_cam;');
        out.push('    output exhaust_cam: _exhaust_cam;');
        out.push('');
        out.push('    camshaft_parameters params(advance: 0.0, base_radius: base_radius)');
        out.push('    camshaft _intake_cam(params, lobe_profile: intake_lobe_profile)');
        out.push('    camshaft _exhaust_cam(params, lobe_profile: exhaust_lobe_profile)');
        out.push('');
        out.push('    label rot360(360 * units.deg)');
        out.push('    _exhaust_cam');
        for (const c of bank.cylinders) {
            out.push(`        .add_lobe(rot360 - exhaust_lobe_center + ${num(c.firing, 3)} * units.deg) // cylinder ${c.number}`);
        }
        out.push('    _intake_cam');
        for (const c of bank.cylinders) {
            out.push(`        .add_lobe(rot360 + intake_lobe_center + ${num(c.firing, 3)} * units.deg) // cylinder ${c.number}`);
        }
        out.push('}');
        out.push('');
    }

    // The engine
    out.push(`public node ${id}_engine {`);
    out.push('    alias output __out: engine;');
    out.push('');
    out.push('    engine engine(');
    out.push(`        name: ${quoted(v.name)},`);
    out.push(`        starter_torque: ${num(starterTorque, 0)} * units.lb_ft,`);
    out.push(`        starter_speed: ${num(v.starterSpeed, 0)} * units.rpm,`);
    out.push(`        redline: ${num(redline, 0)} * units.rpm,`);
    out.push(`        dyno_min_speed: ${num(Math.min(1000, redline * 0.2), 0)} * units.rpm,`);
    out.push(`        dyno_max_speed: ${num(redline, 0)} * units.rpm,`);
    out.push(`        fuel: ${FUELS[v.fuel].script(num(turbulenceEffect(redline), 2))},`);
    // A diesel's pedal sets the fuel, which goes with the torque
    out.push(`        throttle_gamma: ${v.fuel === 'diesel' ? '1.0' : '2.0'},`);
    out.push('        hf_gain: 0.01,');
    out.push('        noise: 1.0,');
    out.push('        jitter: 0.4,');
    out.push(`        simulation_frequency: ${simulationFrequency}`);
    out.push('    )');
    out.push('');
    out.push(`    ${id}_wires wires()`);
    out.push('');
    out.push(`    label bore(${num(bore, 2)} * units.mm)`);
    out.push(`    label stroke(${num(stroke, 2)} * units.mm)`);
    out.push(`    label rod_length(${num(v.rodLength, 2)} * units.mm)`);
    out.push(`    label rod_mass(${num(rodMass, 0)} * units.g)`);
    out.push(`    label compression_height(${num(compressionHeight, 2)} * units.mm)`);
    out.push(`    label crank_mass(${num(crankMass, 2)} * units.kg)`);
    out.push(`    label flywheel_mass(${num(v.flywheel, 2)} * units.kg)`);
    out.push('    label flywheel_radius(7 * units.inch)');
    out.push('');
    out.push('    label crank_moment(disk_moment_of_inertia(mass: crank_mass, radius: stroke / 2))');
    out.push('    label flywheel_moment(disk_moment_of_inertia(mass: flywheel_mass, radius: flywheel_radius))');
    out.push(`    label other_moment(disk_moment_of_inertia(mass: ${num(Math.max(2, 1.5 * count), 1)} * units.kg, radius: 6.0 * units.cm))`);
    out.push('');
    out.push('    crankshaft c0(');
    out.push('        throw: stroke / 2,');
    out.push('        flywheel_mass: flywheel_mass,');
    out.push('        mass: crank_mass,');
    out.push(`        friction_torque: ${num(frictionTorque, 2)} * units.lb_ft,`);
    out.push('        moment_of_inertia: crank_moment + flywheel_moment + other_moment,');
    out.push('        position_x: 0.0,');
    out.push('        position_y: 0.0,');
    out.push('        tdc: 90 * units.deg');
    out.push('    )');
    out.push('');
    out.push('    // Firing order ' + layout.firing.join('-')
        + (layout.angles ? ', at ' + layout.angles.join(', ') + ' degrees' : ''));
    for (const c of cylinders) {
        out.push(`    rod_journal rj${c.number}(angle: ${num(c.journal, 3)} * units.deg)`);
    }
    out.push('    c0');
    for (const c of cylinders) out.push(`        .add_rod_journal(rj${c.number})`);
    out.push('');
    out.push('    piston_parameters piston_params(');
    out.push(`        mass: ${num(pistonMass, 0)} * units.g,`);
    out.push('        compression_height: compression_height,');
    out.push('        wrist_pin_position: 0.0,');
    out.push('        displacement: 0.0');
    out.push('    )');
    out.push('');
    out.push('    connecting_rod_parameters cr_params(');
    out.push('        mass: rod_mass,');
    out.push('        moment_of_inertia: rod_moment_of_inertia(mass: rod_mass, length: rod_length),');
    out.push('        center_of_mass: 0.0,');
    out.push('        length: rod_length');
    out.push('    )');
    out.push('');
    out.push('    cylinder_bank_parameters bank_params(');
    out.push('        bore: bore,');
    out.push('        deck_height: stroke / 2 + rod_length + compression_height');
    out.push('    )');
    out.push('');
    out.push('    intake intake(');
    out.push(`        plenum_volume: ${num(v.plenumVolume, 3)} * units.L,`);
    out.push(`        plenum_cross_section_area: ${num(Math.max(10, 8 * displacementL), 1)} * units.cm2,`);
    out.push(`        intake_flow_rate: k_carb(${num(v.throttleFlow, 1)}),`);
    out.push(`        runner_flow_rate: k_carb(${num(runnerFlow, 1)}),`);
    out.push(`        runner_length: ${num(v.runnerLength, 1)} * units.mm,`);
    out.push('        idle_flow_rate: k_carb(0.0),');
    out.push(`        molecular_afr: ${num(FUELS[v.fuel].intakeAfr, 2)},`);
    out.push(`        idle_throttle_plate_position: ${num(1 - Number(v.idleOpening) / 100, 5)},`);
    out.push('        velocity_decay: 0.5');
    out.push('    )');
    out.push('');
    out.push('    exhaust_system_parameters es_params(');
    out.push('        outlet_flow_rate: k_carb(1000.0),');
    out.push(`        primary_tube_length: ${num(v.primaryLength, 1)} * units.mm,`);
    out.push(`        primary_flow_rate: k_carb(${num(primaryFlow, 1)}),`);
    out.push(`        collector_cross_section_area: circle_area(${num(Number(v.collectorDiameter) / 2, 2)} * units.mm),`);
    out.push('        velocity_decay: 1.0');
    out.push('    )');
    out.push('');
    for (let e = 0; e < exhausts; ++e) {
        out.push(`    exhaust_system exhaust${e}(`);
        out.push('        es_params,');
        out.push(`        length: ${num(v.pipeLength, 1)} * units.mm,`);
        out.push(`        audio_volume: ${num(Number(v.exhaustVolume) / exhausts, 4)},`);
        out.push(`        impulse_response: ir_lib.${v.impulseResponse}`);
        out.push('    )');
    }
    out.push('');

    // Cam lobes
    const intakeCenter = Number(v.lobeSeparation) - Number(v.camAdvance);
    const exhaustCenter = Number(v.lobeSeparation) + Number(v.camAdvance);
    out.push('    harmonic_cam_lobe intake_lobe(');
    out.push(`        duration_at_50_thou: ${num(v.intakeDuration, 1)} * units.deg,`);
    out.push('        gamma: 1.1,');
    out.push(`        lift: ${num(v.intakeLift, 2)} * units.mm,`);
    out.push('        steps: 100');
    out.push('    )');
    out.push('    harmonic_cam_lobe exhaust_lobe(');
    out.push(`        duration_at_50_thou: ${num(v.exhaustDuration, 1)} * units.deg,`);
    out.push('        gamma: 1.1,');
    out.push(`        lift: ${num(v.exhaustLift, 2)} * units.mm,`);
    out.push('        steps: 100');
    out.push('    )');
    out.push('');

    for (const bank of banks) {
        out.push(`    ${id}_camshaft_${bank.index} camshaft${bank.index}(`);
        out.push('        intake_lobe_profile: intake_lobe,');
        out.push('        exhaust_lobe_profile: exhaust_lobe,');
        out.push(`        intake_lobe_center: ${num(intakeCenter, 2)} * units.deg,`);
        out.push(`        exhaust_lobe_center: ${num(exhaustCenter, 2)} * units.deg,`);
        out.push(`        base_radius: ${num(Math.max(10, 0.2 * bore), 1)} * units.mm`);
        out.push('    )');
        out.push('');

        out.push(`    cylinder_bank b${bank.index}(bank_params, angle: ${num(bank.angle, 2)} * units.deg)`);
        out.push(`    b${bank.index}`);
        bank.cylinders.forEach((c, position) => {
            // Small differences between cylinders, as in the stock engines
            const blowby = [0.1, 0.05, 0.08][c.number % 3];
            const attenuation = [0.9, 0.95, 1.0, 0.93][c.number % 4];
            out.push('        .add_cylinder(');
            out.push(`            piston: piston(piston_params, blowby: k_28inH2O(${blowby})),`);
            out.push('            connecting_rod: connecting_rod(cr_params),');
            out.push(`            rod_journal: rj${c.number},`);
            out.push('            intake: intake,');
            out.push(`            exhaust_system: exhaust${exhaustOf(c)},`);
            out.push(`            ignition_wire: wires.wire${c.number},`);
            out.push(`            primary_length: ${num(12 * (1 + Math.abs(position - (bank.cylinders.length - 1) / 2)), 1)} * units.mm,`);
            out.push(`            sound_attenuation: ${attenuation}`);
            out.push('        )');
        });
        out.push(`        .set_cylinder_head(${id}_head(`);
        out.push(`            chamber_volume: ${num(chamberCc, 2)} * units.cc,`);
        out.push(`            intake_camshaft: camshaft${bank.index}.intake_cam,`);
        out.push(`            exhaust_camshaft: camshaft${bank.index}.exhaust_cam,`);
        out.push(`            flip_display: ${flipped.includes(bank.index) ? 'true' : 'false'}`);
        out.push('        ))');
        out.push(`    engine.add_cylinder_bank(b${bank.index})`);
        out.push('');
    }

    out.push('    engine.add_crankshaft(c0)');
    out.push('');

    // Ignition. Timing curves are filtered over 1000 rpm, and between samples
    // further apart than that the advance drops towards zero, so the curve
    // gets a sample every 500 rpm: idle timing up to 1000 rpm, then a straight
    // rise to full advance.
    const fullAt = Math.max(1500, Math.min(Number(v.timingMaxRpm), redline));
    const advanceAt = rpm => (rpm <= 1000)
        ? Number(v.timingIdle)
        : Number(v.timingIdle) + (Number(v.timingMax) - Number(v.timingIdle)) * Math.min(1, (rpm - 1000) / (fullAt - 1000));
    out.push('    function timing_curve(1000 * units.rpm)');
    out.push('    timing_curve');
    for (let rpm = 0; rpm <= Math.max(redline, Number(v.revLimit)) + 2000; rpm += 500) {
        out.push(`        .add_sample(${rpm} * units.rpm, ${num(advanceAt(rpm), 2)} * units.deg)`);
    }
    out.push('');
    out.push('    ignition_module ignition(');
    out.push('        timing_curve: timing_curve,');
    out.push(`        rev_limit: ${num(v.revLimit, 0)} * units.rpm,`);
    out.push(`        limiter_duration: ${num(v.limiterTime, 3)} * units.sec`);
    out.push('    )');
    out.push('    ignition');
    for (const c of [...cylinders].sort((a, b) => a.firing - b.firing)) {
        out.push(`        .connect_wire(wires.wire${c.number}, ${num(c.firing, 3)} * units.deg)`);
    }
    out.push('    engine.add_ignition_module(ignition)');

    // Diesel injection: the timing curve above is the injection timing, and
    // the governor holds the idle and cuts the fuel at the rev limit
    if (v.fuel === 'diesel') {
        const pilot = v.injection !== 'mechanical';
        out.push('');
        out.push('    diesel_injection injection(');
        out.push(`        idle_speed: ${num(v.dieselIdleSpeed, 0)} * units.rpm,`);
        out.push(`        max_speed: ${num(v.revLimit, 0)} * units.rpm,`);
        out.push(`        pilot_injection: ${pilot},`);
        out.push(`        combustion_noise: ${num(v.dieselNoise, 2)},`);
        out.push(`        injector_noise: ${num(v.dieselNoise, 2)}`);
        out.push('    )');
        out.push('    engine.add_diesel_injection(injection)');
    }
    out.push('}');
    out.push('');

    // Vehicle and transmission
    out.push(`private node ${id}_vehicle {`);
    out.push('    alias output __out:');
    out.push('        vehicle(');
    out.push(`            mass: ${num(v.mass, 1)} * units.kg,`);
    out.push(`            drag_coefficient: ${num(v.dragCoefficient, 3)},`);
    out.push(`            cross_sectional_area: ${num(v.frontalArea, 3)} * units.m2,`);
    out.push(`            diff_ratio: ${num(v.finalDrive, 3)},`);
    out.push(`            tire_radius: ${num(v.tireRadius, 1)} * units.mm,`);
    out.push(`            rolling_resistance: ${num(v.rollingResistance, 1)} * units.N,`);
    out.push(`            max_brake_force: ${num(v.brakeForce, 1)} * units.N`);
    out.push('        );');
    out.push('}');
    out.push('');
    out.push(`private node ${id}_transmission {`);
    out.push('    alias output __out:');
    out.push(`        transmission(max_clutch_torque: ${num(v.clutchTorque, 1)} * units.Nm)`);
    const gears = parseGears(v.gears);
    gears.forEach((g, i) => out.push(`            .add_gear(${num(g, 3)})${i === gears.length - 1 ? ';' : ''}`));
    out.push('}');
    out.push('');
    out.push('public node main {');
    out.push(`    set_engine(${id}_engine())`);
    out.push(`    set_transmission(${id}_transmission())`);
    out.push(`    set_vehicle(${id}_vehicle())`);
    out.push('}');
    out.push('');

    return out.join('\n');
}

// The builder values saved in a script made by generate(), or null for any
// other script
function readSavedValues(scriptText) {
    for (const line of String(scriptText).split(/\r?\n/)) {
        if (!line.startsWith(SAVED_VALUES_PREFIX)) continue;
        try {
            const saved = JSON.parse(line.slice(SAVED_VALUES_PREFIX.length));
            return (saved && typeof saved.values === 'object')
                ? { ...DIESEL_DEFAULTS, ...saved.values }
                : null;
        }
        catch (e) {
            return null;
        }
    }

    return null;
}

function fileName(values) {
    return identifier(values.name).slice(3) + '.mr';
}

function summary(values) {
    const layout = LAYOUTS[values.layout];
    const count = layout ? layout.bank.length : 0;
    const liters = Math.PI / 4 * (values.bore / 10) ** 2 * (values.stroke / 10) * count / 1000;
    return `${values.name}: ${layout ? layout.label : 'unknown layout'}, ${liters.toFixed(2)} liters`;
}

return {
    FORMAT_VERSION, LAYOUTS, FUELS, IMPULSE_RESPONSES, EXHAUST_ROUTING, VALVES, INJECTION, SECTIONS,
    PRESETS, DEFAULT_PRESET, turbulenceEffect, parseGears, validate, crankLayout, generate,
    readSavedValues, fileName, summary
};
});
