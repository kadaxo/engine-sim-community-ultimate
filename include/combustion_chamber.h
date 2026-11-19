#ifndef ATG_ENGINE_SIM_COMBUSTION_CHAMBER_H
#define ATG_ENGINE_SIM_COMBUSTION_CHAMBER_H

#include "scs.h"

#include "piston.h"
#include "gas_system.h"
#include "cylinder_head.h"
#include "units.h"
#include "fuel.h"

class Engine;
class CombustionChamber : public atg_scs::ForceGenerator {
    public:
        struct Parameters {
            Piston *piston;
            CylinderHead *Head;
            Fuel *fuel;
            Function *MeanPistonSpeedToTurbulence;

            double StartingPressure;
            double StartingTemperature;
            double CrankcasePressure;
        };

        struct FlameEvent {
            double lit_n = 0;
            double total_n = 0;
            double percentageLit = 0;
            double efficiency = 1.0;
            double flameSpeed = 0.0;

            double lastVolume = 0.0;
            double travel_x = 0.0;
            double travel_y = 0.0;
            GasSystem::Mix globalMix;
        };

        // One diesel injection and its combustion (see diesel_injection.h).
        // Amounts of fuel are in moles, angles are crank angles since the
        // first injection started.
        struct DieselEvent {
            bool active = false;
            bool ignited = false;
            bool mainStarted = false;
            double angle = 0.0;
            double mainStart = 0.0;
            double rate = 0.0;          // injection rate [mol/s]
            double pilotFuel = 0.0;
            double mainFuel = 0.0;
            double pilotInjected = 0.0;
            double mainInjected = 0.0;
            double delayProgress = 0.0; // ignites at 1
            double pending = 0.0;       // sprayed in, not lit yet
            double premixed = 0.0;      // lit, burning at once
            double mixing = 0.0;        // lit, burning as it mixes with the air
            double premixedShare = 0.0;
            double efficiency = 1.0;
        };

        struct FrictionModelParams {
            double frictionCoeff = 0.06;
            double breakawayFriction = units::force(50, units::N);
            double breakawayFrictionVelocity = units::distance(0.1, units::m);
            double viscousFrictionCoefficient = units::force(20, units::N);
        };

    public:
        CombustionChamber();
        virtual ~CombustionChamber();

        void initialize(const Parameters &params);
        void destroy();
        void setEngine(Engine *engine) { m_engine = engine; }
        virtual void apply(atg_scs::SystemState *system);

        CylinderHead *getCylinderHead() const { return m_head; }
        Piston *getPiston() const { return m_piston; }

        double getFrictionForce() const;
        double getVolume() const;
        double pistonSpeed() const;
        double calculateMeanPistonSpeed() const;
        double calculateFiringPressure() const;

        bool isLit() const { return m_lit; }
        bool popLitLastFrame();

        void ignite();
        void update(double dt);
        void flow(double dt);

        double lastEventAfr() const;

        // Diesel injection: fuel sprayed into this cylinder since the last
        // reset [mol], and injections started since the last pop (the
        // injector ticks)
        double getDieselFuelInjected() const { return m_dieselFuelInjected; }
        void resetDieselFuelInjected() { m_dieselFuelInjected = 0; }
        int popInjectorTicks();
        const DieselEvent &getDieselEvent() const { return m_diesel; }

        double getLastIterationExhaustFlow() const { return m_exhaustFlow; }

        void resetLastTimestepExhaustFlow() { m_lastTimestepTotalExhaustFlow = 0; }
        double getLastTimestepExhaustFlow() const { return m_lastTimestepTotalExhaustFlow; }

        void resetLastTimestepIntakeFlow() { m_lastTimestepTotalIntakeFlow = 0; }
        double getLastTimestepIntakeFlow() const { return m_lastTimestepTotalIntakeFlow; }

        Function *m_meanPistonSpeedToTurbulence;
        GasSystem m_system;
        GasSystem m_intakeRunnerAndManifold;
        GasSystem m_exhaustRunnerAndPrimary;
        FlameEvent m_flameEvent;
        bool m_lit;

        FrictionModelParams m_frictionModel;

        double m_peakTemperature;
        double m_nBurntFuel;

    protected:
        double calculateFrictionForce(double v) const;
        void updateCycleStates();

        void startDieselInjection();
        void burnDiesel(double dt);

        DieselEvent m_diesel;
        double m_dieselFuelInjected;
        int m_injectorTicks;

        double m_intakeFlowRate;
        double m_exhaustFlowRate;

        double m_manifoldToRunnerFlowRate;
        double m_primaryToCollectorFlowRate;
        double m_cylinderCrossSectionSurfaceArea;
        double m_cylinderWidthApproximation;

        double m_lastTimestepTotalExhaustFlow;
        double m_lastTimestepTotalIntakeFlow;
        double m_exhaustFlow;

        double m_crankcasePressure;

        double *m_pressure;
        double *m_pistonSpeed;
        static constexpr int StateSamples = 256;

        bool m_litLastFrame;

        Piston *m_piston;
        CylinderHead *m_head;
        Engine *m_engine;
        Fuel *m_fuel;
};

#endif /* ATG_ENGINE_SIM_COMBUSTION_CHAMBER_H */
