#ifndef ATG_ENGINE_SIM_INTAKE_H
#define ATG_ENGINE_SIM_INTAKE_H

#include "part.h"

#include "gas_system.h"

class ForcedInduction;

class Intake : public Part {
    public:
        struct Parameters {
            // Plenum volume
            double volume;

            // Plenum dimensions
            double CrossSectionArea;

            // Input flow constant
            double InputFlowK;

            // Idle-circuit flow constant
            double IdleFlowK;

            // Flow rate from plenum to runner
            double RunnerFlowRate;

            // Molecular air fuel ratio (defaults to ideal for octane)
            double MolecularAfr = (25.0 / 2.0);

            // Throttle plate position at idle
            double IdleThrottlePlatePosition = 0.975;

            // Runner volume
            double RunnerLength = units::distance(4.0, units::inch);

            // Velocity decay factor
            double VelocityDecay = 0.5;

            // Atmosphere pressure
            double AtmospherePressure = units::pressure(1.0, units::atm);

            // Atmosphere temperature
            double AtmosphereTemperature = units::celcius(25.0);

        };

    public:
        Intake();
        virtual ~Intake();

        void initialize(Parameters &params);
        virtual void destroy();

        void process(double dt);

        inline double getRunnerFlowRate() const { return m_runnerFlowRate; }
        inline double getThrottlePlatePosition() const { return m_idleThrottlePlatePosition * m_throttle; }
        inline double getRunnerLength() const { return m_runnerLength; }
        inline double getPlenumCrossSectionArea() const { return m_crossSectionArea; }
        inline double getVelocityDecay() const { return m_velocityDecay; }

        GasSystem m_system;
        double m_throttle;

        double m_flow;
        double m_flowRate;
        double m_totalFuelInjected;

        // The throttle draws from the compressor's charge air instead of the
        // atmosphere when the engine is boosted
        void setForcedInduction(ForcedInduction *forcedInduction) { m_forcedInduction = forcedInduction; }
        ForcedInduction *getForcedInduction() const { return m_forcedInduction; }

        // Mixture of air and fuel drawn through the throttle
        GasSystem::Mix getFuelAirMix() const;

        // Diesel injection: the fuel goes straight into the cylinders, so
        // the intake (and its idle circuit) passes air only
        void setDirectInjection(bool directInjection) { m_directInjection = directInjection; }
        bool hasDirectInjection() const { return m_directInjection; }

        // Ambient conditions configured in the script (atmosphere_pressure/temperature)
        inline double getAtmospherePressure() const { return m_atmospherePressure; }
        inline double getAtmosphereTemperature() const { return m_atmosphereTemperature; }

    protected:
        double m_crossSectionArea;
        double m_inputFlowK;
        double m_idleFlowK;
        double m_runnerFlowRate;
        double m_molecularAfr;
        double m_idleThrottlePlatePosition;
        double m_runnerLength;
        double m_velocityDecay;
        double m_atmospherePressure;
        double m_atmosphereTemperature;

        GasSystem m_atmosphere;
        ForcedInduction *m_forcedInduction;
        bool m_directInjection;
};

#endif /* ATG_ENGINE_SIM_INTAKE_H */
