#ifndef ATG_ENGINE_SIM_FORCED_INDUCTION_H
#define ATG_ENGINE_SIM_FORCED_INDUCTION_H

#include "part.h"
#include "gas_system.h"
#include "scs.h"

#include <string>

class Crankshaft;
class Intake;

// Turbochargers and superchargers. Ported from kyan0s-git/engine-sim
// (commits 27ed482d and c1440107), keeping only the gas model this version
// has.
//
// The compressor fills a charge-air volume (pipes and intercooler) that the
// intakes' throttles draw from instead of the atmosphere.
//
//  - Centrifugal compressors (turbochargers and centrifugal superchargers)
//    are modelled as a pressure source behind a flow restriction. The source
//    pressure follows from the Euler work of the impeller,
//        PR = (1 + eta * sigma * U^2 / (c_p * T1))^(gamma / (gamma - 1)),
//    so the delivered boost depends on the wheel's tip speed U and drops as
//    the flow approaches the compressor's capacity. The impeller absorbs
//    sigma * U^2 of work per kilogram.
//  - Positive displacement superchargers (Roots and twin-screw) deliver their
//    displacement every revolution; some of it leaks back to the inlet
//    through the clearances. With the throttle before the blower (the
//    butterflies in a hat on top of it, as on drag and monster truck
//    engines) the throttles fill a small inlet volume that the blower draws
//    from, so at part throttle it moves its displacement of thin air, and
//    the charge feeds the intakes with no throttle after it.
//
// A turbocharger's turbine is the restriction at the exhaust outlet. It
// extracts eta_t * c_p * T * (1 - (p_out / p_in)^((gamma - 1) / gamma)) per
// unit of flow; the shaft's kinetic energy integrates the turbine power minus
// the compressor and bearing power. A wastegate bypasses the turbine once the
// boost reaches its set point. Superchargers are driven by the crankshaft
// through their drive ratio and load it with their drive power.
//
// The compressor draws from the intakes' atmosphere (atmosphere_pressure and
// atmosphere_temperature in the script), and boost is measured against it.
class ForcedInduction : public Part, public atg_scs::ForceGenerator {
    public:
        enum class Type {
            Turbocharger,
            Roots,
            TwinScrew,
            Centrifugal
        };

        // Turbochargers only
        enum class Arrangement {
            Single,
            Twin,           // two turbochargers in parallel
            Sequential      // the second turbocharger joins at high speed
        };

        // Where the blow-off valve vents (or no valve: the compressor surges
        // when the throttle closes)
        enum class BlowoffType {
            Atmospheric,
            Recirculating,
            None
        };

        // Spring: the actuator opens on charge pressure alone and lets the
        // boost creep up at high flow; electronic: closed loop on the target
        enum class BoostControl {
            Spring,
            Electronic
        };

        // What the compressor inlet draws through, which decides how much of
        // its whistle gets out: a factory airbox with a broadband resonator
        // (silenced), an airbox alone, or an open filter
        enum class CompressorInlet {
            Silenced,
            Airbox,
            Open
        };

        // The compressor's tones (centrifugal compressors), each as a
        // pressure amplitude relative to that of the blade passing tone of
        // a 60 mm wheel at 400 m/s tip speed and full flow, drawing through
        // an open filter
        struct CompressorTones {
            double shaftFrequency = 0;  // fastest shaft [Hz]
            double synchronous = 0;     // once per revolution (residual imbalance)
            double bladePass = 0;       // full blades past the inducer
            double splitterPass = 0;    // full and splitter blades past the exducer
            double buzzSaw = 0;         // shaft harmonics from the inducer's shocks
            double whirl = 0;           // worn bearings: the rotor's whirl, through the housing
            double whirlRatio = 0;      // its frequency over the shaft's (below 1: oil whirl in
                                        // journal bearings; above: ball bearings' defect tone)
        };

        // The noise of the air, each level relative to the reference blade
        // passing tone and as it gets out (through the inlet where it goes
        // that way)
        struct AirNoise {
            double whoosh = 0;              // compressor's broadband near surge
            double whooshFrequency = 0;     // its centre [Hz]
            double roar = 0;                // air rushing into the compressor
            double blowoff = 0;             // the blow-off valve's jet
            double blowoffFrequency = 0;    // its peak [Hz]
            double surge = 0;               // flow reversing through the compressor
            double surgeRate = 0;           // deep surge cycles per second [Hz]
            double leak = 0;                // a leak in the charge pipe: its jet
            double leakFrequency = 0;       // its peak [Hz]
        };

        struct Parameters {
            Type type = Type::Turbocharger;
            Arrangement arrangement = Arrangement::Single;

            // Compressor (centrifugal: per unit)
            double compressorDiameter = 0.060;      // impeller tip diameter [m]
            double compressorFlow = 0.30;           // maximum flow [kg/s]
            double compressorEfficiency = 0.75;
            double maxSpeed = 0;                    // shaft limit [rad/s], 0: from the tip speed limit

            // Turbine (turbochargers)
            double turbineAr = 0.64;                // housing A/R (variable geometry: vanes open)
            double turbineDiameter = 0;             // wheel [m], 0: sized from the compressor's flow
            double turbineEfficiency = 0.70;
            double inertia = 3.0e-5;                // rotating assembly [kg m^2]
            bool ballBearings = false;

            // Variable geometry turbine: vanes in the housing that act as
            // a small A/R when closed and turbineAr when open. They hold the
            // boost instead of a wastegate.
            bool variableGeometry = false;
            double minTurbineAr = 0;                // vanes closed, 0: 0.25 * turbineAr

            // Exhaust sound taken after the turbine, which smooths the pulses
            // going through it; false takes it before the turbine (as it was
            // before the default changed)
            bool soundAfterTurbine = true;

            // Boost control
            double boost = 1.0e5;                   // wastegate set point (gauge) [Pa]
            bool blowoffValve = true;
            BlowoffType blowoffType = BlowoffType::Atmospheric;
            BoostControl boostControl = BoostControl::Electronic;
            bool externalWastegate = false;         // larger, vents through its own pipe
            // Electronic control holds the wastegate open whenever the
            // engine is off boost (manifold below the outside's pressure),
            // as engines with an electric wastegate actuator do
            bool wastegateOpenOffBoost = false;
            bool twinScroll = false;                // divided turbine housing: better spool
            double whistlePitch = 1.0;              // sound only
            // Compressor wheel blades (centrifugal) and its inlet (sound only)
            int compressorBlades = 6;
            int splitterBlades = 6;
            CompressorInlet compressorInlet = CompressorInlet::Silenced;

            // Wear and faults, all off by default: worn shaft bearings
            // [0 new, 1 worn out], a worn wastegate linkage that lets the
            // open flapper rattle [0, 1], and a hole in the charge pipe or
            // a split coupler, as its diameter [m]
            double bearingWear = 0;
            double wastegateRattle = 0;
            double boostLeak = 0;

            // Anti-lag ("bang-bang"): off the throttle above a minimum speed
            // it holds the throttle partly open and fires the spark very
            // late, so the mixture burns on into the exhaust manifold and
            // keeps the turbo spinning
            bool antiLag = false;
            double antiLagTiming = -30.0 * 3.14159265358979 / 180;     // [rad], after top dead centre
            double antiLagThrottle = 0.25;                             // throttle opening it holds
            double antiLagMinSpeed = 2 * 3.14159265358979 * 3000 / 60; // [rad/s]

            // Superchargers
            // Compressor speed / crank speed (0: 2 for Roots and twin-screw,
            // 10 for centrifugal, the step-up gears and the belt together)
            double driveRatio = 1.0;
            double displacement = 1.9e-3;           // positive displacement, per rev [m^3]
            bool bypassValve = true;                // vents the charge at part throttle
            // Roots and twin-screw: the intakes' throttles sit before the
            // blower instead of after it
            bool throttleBeforeBlower = false;

            // Adiabatic efficiency of a Roots or twin-screw's rotors (0: 0.55
            // for Roots, 0.72 for twin-screw)
            double rotorEfficiency = 0;

            // Supercharger sound only. Lobes on each rotor (0: 3 for Roots,
            // 3 on a twin-screw's male rotor); a Roots blower's two rotors
            // send twice their lobes past the outlet each turn. The rotors'
            // twist over their length smooths each lobe's backflow pulse, so
            // the more twist, the weaker the whine's harmonics [rad]. Drive
            // gears whine at their mesh: teeth times the speed of the gear on
            // the compressor's shaft (0: no gear whine), at gearWhine times
            // a typical level.
            int rotorLobes = 0;
            double rotorTwist = 60.0 * 3.14159265358979 / 180;
            int gearTeeth = 0;
            double gearWhine = 1.0;
            // Scales all of a supercharger's whine (lobe passing, compressor
            // tones, gears): makers silence some, as Mercedes did the M113K's
            double whineVolume = 1.0;

            // Charge air
            double intercoolerEffectiveness = 0.7;
            double chargeVolume = 6.0e-3;           // [m^3]

            // Sequential turbochargers: engine speed at which the second
            // unit comes in [rad/s]
            double sequentialSpeed = 2 * 3.14159265358979 * 4000 / 60;
        };

        struct Unit {
            double omega = 0;           // shaft speed [rad/s]
            double energy = 0;          // shaft kinetic energy [J]
            double compressorFlow = 0;  // [mol/s]
            double turbinePower = 0;    // [W]
            double compressorPower = 0;
            bool active = true;
        };

    public:
        ForcedInduction();
        virtual ~ForcedInduction();

        void initialize(const Parameters &params);
        virtual void destroy();

        // Connections (made by the engine)
        void setCrankshaft(Crankshaft *crankshaft) { m_crankshaft = crankshaft; }
        void addIntake(Intake *intake);
        void setExhaustSystemCount(int count) { m_exhaustSystemCount = count; }

        // Called by each exhaust system to get its outlet restriction and to
        // report its outlet flow (moles leaving through the outlet)
        double getExhaustOutletFlowK(double mufflerK) const;
        void addExhaustFlow(double n, double T, double P, double hcr, double mufflerK);

        // One fluid step (after the exhausts, before the intakes)
        void process(double dt);

        // Applies the supercharger's drive torque to the crankshaft
        virtual void apply(atg_scs::SystemState *system) override;

        GasSystem *getChargeSystem() { return &m_charge; }

        // With the throttle before the blower, the intakes' throttles fill
        // this volume and the blower draws from it
        bool isThrottleBeforeBlower() const { return m_throttleBeforeBlower; }
        GasSystem *getInletSystem() { return &m_inlet; }

        Type getType() const { return m_type; }
        Arrangement getArrangement() const { return m_arrangement; }
        bool isTurbocharger() const { return m_type == Type::Turbocharger; }
        bool isPositiveDisplacement() const { return m_type == Type::Roots || m_type == Type::TwinScrew; }
        std::string getDescription() const;

        int getUnitCount() const { return m_unitCount; }
        const Unit &getUnit(int i) const { return m_units[i]; }

        // Gauge pressure of the charge air against the intake atmosphere [Pa]
        double getBoost() const;
        double getChargePressure() const { return m_charge.pressure(); }
        double getChargeTemperature() const { return m_charge.temperature(); }
        double getCompressorOutletTemperature() const { return m_compressorOutletTemperature; }
        double getAmbientPressure() const { return m_ambientPressure; }
        double getAmbientTemperature() const { return m_ambientTemperature; }

        // Shaft speed of the (first) compressor [rad/s]
        double getShaftSpeed() const;
        double getMaxSpeed() const { return m_maxSpeed; }

        double getWastegateOpening() const { return m_wastegate; }
        bool hasVariableGeometry() const { return m_variableGeometry; }
        double getVaneOpening() const { return m_vanes; }                  // variable geometry [0 closed, 1 open]
        bool isSoundAfterTurbine() const { return m_soundAfterTurbine; }

        // Share of the exhaust pressure above the outside left after the
        // turbine and wastegate, for the muffler [0, 1]: how much of the
        // pulses reach the tailpipe
        double getMufflerShare() const { return m_mufflerShare; }
        double getBypassOpening() const { return m_bypass; }                // superchargers
        double getBlowoffOpening() const { return m_blowoff; }
        double getBlowoffFlow() const { return m_blowoffFlowRate; }    // [mol/s]
        double getCompressorFlow() const;                               // [mol/s]
        double getDrivePower() const { return m_drivePower; }           // superchargers [W]
        double getDriveTorque() const { return m_driveTorque; }         // superchargers [N m]
        double getTurbinePower() const;

        // Share of the exhaust flow going through the turbine(s) [0, 1]
        double getTurbineFlowFraction() const { return m_turbineFraction; }

        // Runtime adjustments
        void setBoostTarget(double boost) { m_boostTarget = boost; }
        double getBoostTarget() const { return m_boostTarget; }
        void setBlowoffValve(bool enabled) { m_blowoffEnabled = enabled; }
        bool hasBlowoffValve() const { return m_blowoffEnabled; }
        void setBlowoffType(BlowoffType type) { m_blowoffType = type; m_blowoffEnabled = (type != BlowoffType::None); }
        BlowoffType getBlowoffType() const { return m_blowoffEnabled ? m_blowoffType : BlowoffType::None; }
        void setBoostControl(BoostControl control) { m_boostControl = control; }
        BoostControl getBoostControl() const { return m_boostControl; }
        bool hasExternalWastegate() const { return m_externalWastegate; }
        bool isWastegateOpenOffBoost() const { return m_wastegateOpenOffBoost; }
        bool isTwinScroll() const { return m_twinScroll; }
        void setWhistlePitch(double pitch) { m_whistlePitch = pitch; }
        double getWhistlePitch() const { return m_whistlePitch; }
        void setIntercoolerEffectiveness(double e);
        double getIntercoolerEffectiveness() const { return m_intercoolerEffectiveness; }
        void setDriveRatio(double ratio) { m_driveRatio = ratio; }
        double getDriveRatio() const { return m_driveRatio; }

        // Audio: compressor whine and blow-off noise
        double getWhineFrequency() const;   // [Hz]
        double getWhineLevel() const;       // positive displacement [0, 1]
        int getRotorLobes() const { return m_rotorLobes; }
        double getRotorTwist() const { return m_rotorTwist; }
        // Lobes passing the outlet per turn of the compressor's shaft
        double getPulsesPerTurn() const;
        // Each harmonic of the lobe passing tone over the one below it
        double getWhineHarmonicRatio() const;
        // Drive gear mesh [Hz] and its whine level (0 without gears)
        double getGearFrequency() const;
        double getGearLevel() const;
        double getWhineVolume() const { return m_whineVolume; }
        CompressorTones getCompressorTones() const;
        int getCompressorBlades() const { return m_compressorBlades; }
        int getSplitterBlades() const { return m_splitterBlades; }
        CompressorInlet getCompressorInlet() const { return m_compressorInlet; }

        // Share of a compressor tone's pressure at a frequency [Hz] that gets
        // out through the inlet
        static double inletTransmission(CompressorInlet inlet, double frequency);
        AirNoise getAirNoise() const;
        double getSurgeLevel() const { return m_surge; }    // [0, 1]

        // Share of the exhaust pulses an external wastegate vents through its
        // own pipe, past the turbine and the muffler [0, 1]
        double getWastegateShare() const;

        // How hard the exhaust pulses knock a worn wastegate's open flapper
        // about, per unit of pulse [0, 1]
        double getRattleShare() const;

        // Anti-lag: the engine switches it on and off with the driver's
        // throttle and the engine speed (see Engine::setThrottle())
        bool hasAntiLag() const { return m_antiLag; }
        bool isAntiLagActive() const { return m_antiLagActive; }
        void setAntiLagActive(bool active) { m_antiLagActive = m_antiLag && active; }
        double getAntiLagTiming() const { return m_antiLagTiming; }
        double getAntiLagThrottle() const { return m_antiLagThrottle; }
        double getAntiLagMinSpeed() const { return m_antiLagMinSpeed; }

        double getBearingWear() const { return m_bearingWear; }
        double getWastegateRattle() const { return m_wastegateRattle; }
        double getBoostLeak() const { return m_leakDiameter; }             // [m]
        double getLeakFlow() const { return m_leakFlowRate; }              // out of the charge [mol/s]

        // Sound type for the synthesizer: 10 * type (1 turbocharger, 2 Roots,
        // 3 twin-screw, 4 centrifugal) + blow-off type (0 vented to the
        // atmosphere, 1 recirculating, 2 none) + 100 * full blades + 10000 *
        // splitter blades
        int getSoundMode() const;

    protected:
        double centrifugalSourcePressureRatio(double omega) const;
        void processCentrifugal(Unit &unit, double dt, double scale, const GasSystem::Mix &mix);
        void processPositiveDisplacement(double dt, const GasSystem::Mix &mix);
        void processThrottledBlower(double dt);

        // Flow from the charge back into the blower's inlet volume
        // (positive), at the inlet's temperature [mol]
        double flowToInlet(double k, double dt);
        double plenumPressure() const;
        double crankSpeed() const;
        GasSystem::Mix chargeMix() const;

        Type m_type;
        Arrangement m_arrangement;

        Unit m_units[2];
        int m_unitCount;

        GasSystem m_charge;
        GasSystem m_inlet;

        Crankshaft *m_crankshaft;
        Intake *m_intakes[16];
        int m_intakeCount;
        int m_exhaustSystemCount;

        // Air the compressor draws from
        double m_ambientPressure;
        double m_ambientTemperature;

        // Compressor
        double m_tipRadius;
        double m_compressorK;
        double m_compressorEfficiency;
        double m_maxSpeed;
        double m_slip;

        // Positive displacement
        double m_displacement;
        double m_leakK;
        double m_bypassK;
        bool m_bypassValve;
        bool m_throttleBeforeBlower;

        // Pressure ratio across a positive displacement blower's rotors
        double rotorPressureRatio() const;
        double m_bypass;
        double m_driveRatio;
        double m_driveTorque;
        double m_drivePower;

        // Turbine. m_turbineK is the current flow constant; with variable
        // geometry it moves between the closed and open vanes' constants.
        double m_turbineK;
        double m_turbineKOpen;
        double m_turbineKClosed;
        bool m_variableGeometry;
        double m_vanes;
        bool m_soundAfterTurbine;
        double m_mufflerShare;
        double m_exhaustMoles;      // through the outlets since the last process() [mol]
        double m_exhaustFlowRate;   // [mol/s], smoothed
        double m_exhaustHeat;       // moles times temperature since the last process() [mol K]
        double m_exhaustTemperature; // at the outlets, smoothed [K]
        double m_inletScale;        // the turbine's flow for its inlet temperature, see turbineInletTemperature()
        double m_mufflerK;          // each exhaust system's outlet restriction after the turbine
        double m_wastegateK;
        double m_turbineEfficiency;
        double m_inertia;
        double m_bearingLoss;
        double m_turbineFraction;
        double m_sequentialSpeed;
        double m_turbineEnergy;     // turbine work since the last process() [J]

        // Control
        double m_boostTarget;
        double m_wastegate;
        bool m_blowoffEnabled;
        double m_blowoff;
        double m_blowoffK;
        double m_blowoffFlowRate;

        double m_intercoolerEffectiveness;
        BlowoffType m_blowoffType;
        BoostControl m_boostControl;
        bool m_externalWastegate;
        bool m_twinScroll;
        double m_whistlePitch;
        int m_rotorLobes;
        double m_rotorEfficiency;
        double m_rotorTwist;
        int m_gearTeeth;
        double m_gearWhine;
        double m_whineVolume;
        int m_compressorBlades;
        int m_splitterBlades;
        CompressorInlet m_compressorInlet;
        bool m_wastegateOpenOffBoost;
        bool m_antiLag;
        bool m_antiLagActive;
        double m_antiLagTiming;
        double m_antiLagThrottle;
        double m_antiLagMinSpeed;
        bool m_ballBearings;
        double m_bearingWear;
        double m_wastegateRattle;
        double m_leakDiameter;
        double m_boostLeakK;
        double m_leakFlowRate;
        double m_surge;
        double m_wastegateIntegral;
        double m_lastBoost;
        double m_boostRate;
        double m_maxCompressorFlow;     // [kg/s]
        double m_compressorOutletTemperature;
};

#endif /* ATG_ENGINE_SIM_FORCED_INDUCTION_H */
