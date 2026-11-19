#ifndef ATG_ENGINE_SIM_DIESEL_INJECTION_NODE_H
#define ATG_ENGINE_SIM_DIESEL_INJECTION_NODE_H

#include "object_reference_node.h"

#include "engine_sim.h"

#include "../../include/diesel_injection.h"

namespace es_script {

    // Diesel injection and combustion for an engine (docs/diesel.md)
    class DieselInjectionNode : public ObjectReferenceNode<DieselInjectionNode> {
    public:
        DieselInjectionNode() { /* void */ }
        virtual ~DieselInjectionNode() { /* void */ }

        // Gives the engine its diesel injection (before the forced
        // induction, which takes the intakes' mixture)
        void generate(Engine *engine) const {
            DieselInjection *injection = new DieselInjection;
            injection->initialize(m_parameters);
            engine->setDieselInjection(injection);
        }

    protected:
        virtual void registerInputs() {
            addInput("idle_speed", &m_parameters.idleSpeed);
            addInput("max_speed", &m_parameters.maxSpeed);
            addInput("governor_droop", &m_parameters.governorDroop);
            addInput("start_fuel", &m_parameters.startFuel);
            addInput("full_load_lambda", &m_parameters.fullLoadLambda);
            addInput("injection_duration", &m_parameters.injectionDuration);
            addInput("pilot_injection", &m_parameters.pilotInjection);
            addInput("pilot_fuel", &m_parameters.pilotFuel);
            addInput("pilot_separation", &m_parameters.pilotSeparation);
            addInput("ignition_delay", &m_parameters.ignitionDelay);
            addInput("premixed_share", &m_parameters.premixedShare);
            addInput("combustion_noise", &m_parameters.combustionNoise);
            addInput("injector_noise", &m_parameters.injectorNoise);

            ObjectReferenceNode<DieselInjectionNode>::registerInputs();
        }

        virtual void _evaluate() {
            setOutput(this);
            readAllInputs();
        }

        DieselInjection::Parameters m_parameters;
    };

} /* namespace es_script */

#endif /* ATG_ENGINE_SIM_DIESEL_INJECTION_NODE_H */
