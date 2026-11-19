#ifndef ATG_ENGINE_SIM_DIESEL_INJECTION_H
#define ATG_ENGINE_SIM_DIESEL_INJECTION_H

#include "units.h"

// Diesel fuel injection and combustion (docs/diesel.md).
//
// An engine with diesel injection has no throttle: its intakes pass air only,
// held wide open, and the pedal sets how much fuel is sprayed into each
// cylinder. The ignition module's events are the start of the main injection
// (its timing curve is the injection timing). The fuel ignites by itself
// after an ignition delay that depends on the compressed air's pressure and
// temperature; the fuel sprayed in by then burns at once (the premixed burn,
// the sharp rise in pressure heard as diesel knock) and the rest burns as it
// mixes with the air. A small pilot injection before the main one lights the
// charge first, so little of the main fuel waits to burn at once: common rail
// engines are quieter than old mechanical pumps for that reason.
//
// A governor holds the idle speed with the fuel when the pedal is up and, if
// the engine has a governed maximum speed, cuts the fuel back as the engine
// reaches it.
class DieselInjection {
    public:
        struct Parameters {
            // Governor
            double idleSpeed = units::rpm(750);
            double maxSpeed = 0;            // governed speed, 0: none (the rev limiter only)
            double governorDroop = 0.06;    // share of maxSpeed over which the fuel tapers to none
            double startFuel = 0.6;         // fuel while cranking, share of full

            // Fuel per injection at full load: the air in the cylinder over
            // this lambda (the smoke limit)
            double fullLoadLambda = 1.3;

            // Main injection length at full fuel, shorter in proportion to
            // the fuel at part load. The rail pressure sets the rate, so at
            // high rpm the injection spans more crank angle and burns later.
            // [s]
            double injectionDuration = 0.002;

            // Pilot injection: share of the full load fuel, sprayed this many
            // crank degrees before the main injection
            bool pilotInjection = true;
            double pilotFuel = 0.03;
            double pilotSeparation = units::angle(15, units::deg);

            // Scales the ignition delay (1: Hardenberg and Hase's
            // correlation for diesel fuel of cetane number 50)
            double ignitionDelay = 1.0;

            // Share of the fuel sprayed in during the ignition delay that
            // burns at once when the charge lights
            double premixedShare = 0.7;

            // Sound levels: the engine block's noise from the combustion
            // pressure rise, and the injectors' ticks
            double combustionNoise = 1.0;
            double injectorNoise = 1.0;
        };

    public:
        DieselInjection();
        ~DieselInjection();

        void initialize(const Parameters &params);
        const Parameters &getParameters() const { return m_params; }

        // Pedal position (0 up, 1 floored)
        void setPedal(double pedal) { m_pedal = pedal; }
        double getPedal() const { return m_pedal; }

        // Runs the governor once per simulation step
        void update(double dt, double engineSpeed);

        // Fuel for the next injections, as a share of the full load fuel
        double getFuelDemand() const { return m_demand; }
        double getIdleDemand() const { return m_idleDemand; }

        // Ignition delay in crank degrees (Hardenberg and Hase) at the given
        // cylinder pressure [Pa], temperature [K] and mean piston speed [m/s]
        double ignitionDelayDegrees(double pressure, double temperature, double meanPistonSpeed) const;

    protected:
        Parameters m_params;

        double m_pedal;
        double m_demand;
        double m_idleDemand;
        double m_idleIntegral;
        double m_speed[2];      // mean engine speed, filtered twice [rad/s]
        double m_lastSpeed;
        double m_speedRate;
};

#endif /* ATG_ENGINE_SIM_DIESEL_INJECTION_H */
