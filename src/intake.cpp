#include "../include/intake.h"

#include "../include/forced_induction.h"
#include "../include/units.h"

#include <cmath>

Intake::Intake() {
    m_inputFlowK = 0;
    m_idleFlowK = 0;
    m_flow = 0;
    m_throttle = 1.0;
    m_idleThrottlePlatePosition = 0.0;
    m_crossSectionArea = 0.0;
    m_flowRate = 0;
    m_totalFuelInjected = 0;
    m_molecularAfr = 0;
    m_runnerLength = 0;
    m_atmospherePressure = units::pressure(1.0, units::atm);
    m_atmosphereTemperature = units::celcius(25.0);
    m_forcedInduction = nullptr;
    m_directInjection = false;
}

Intake::~Intake() {
    /* void */
}

void Intake::initialize(Parameters &params) {
    const double width = std::sqrt(params.CrossSectionArea);
    m_system.initialize(
        units::pressure(1.0, units::atm),
        params.volume,
        units::celcius(25.0));
    m_system.setGeometry(
        width,
        params.volume / params.CrossSectionArea,
        1.0,
        0.0);

    m_atmosphere.initialize(
        params.AtmospherePressure,
        units::volume(1000.0, units::m3),
        params.AtmosphereTemperature);
    m_atmosphere.setGeometry(
        units::distance(100.0, units::m),
        units::distance(100.0, units::m),
        1.0,
        0.0);

    m_inputFlowK = params.InputFlowK;
    m_molecularAfr = params.MolecularAfr;
    m_idleFlowK = params.IdleFlowK;
    m_idleThrottlePlatePosition = params.IdleThrottlePlatePosition;
    m_runnerLength = params.RunnerLength;
    m_crossSectionArea = params.CrossSectionArea;
    m_velocityDecay = params.VelocityDecay;
    m_runnerFlowRate = params.RunnerFlowRate;
    m_atmospherePressure = params.AtmospherePressure;
    m_atmosphereTemperature = params.AtmosphereTemperature;
}

void Intake::destroy() {
    /* void */
}

GasSystem::Mix Intake::getFuelAirMix() const {
    if (m_directInjection) {
        GasSystem::Mix air;
        air.p_fuel = 0;
        air.p_inert = 0.75;
        air.p_o2 = 0.25;
        return air;
    }

    const double ideal_afr = 0.8 * m_molecularAfr * 4;
    const double p_air = ideal_afr / (1 + ideal_afr);

    GasSystem::Mix fuelAirMix;
    fuelAirMix.p_fuel = 1 - p_air;
    fuelAirMix.p_inert = p_air * 0.75;
    fuelAirMix.p_o2 = p_air * 0.25;
    return fuelAirMix;
}

void Intake::process(double dt) {
    const GasSystem::Mix fuelAirMix = getFuelAirMix();

    const double idle_afr = 2.0;
    const double p_idle_air = idle_afr / (1 + idle_afr);
    GasSystem::Mix fuelMix;
    fuelMix.p_fuel = (1.0 - p_idle_air);
    fuelMix.p_inert = p_idle_air * 0.75;
    fuelMix.p_o2 = p_idle_air * 0.25;
    if (m_directInjection) fuelMix = fuelAirMix;

    const double throttle = getThrottlePlatePosition();
    const double flowAttenuation = std::cos(throttle * constants::pi / 2);

    GasSystem::FlowParameters flowParams;
    flowParams.crossSectionArea_0 = units::area(10, units::m2);
    flowParams.crossSectionArea_1 = m_crossSectionArea;
    flowParams.direction_x = 0.0;
    flowParams.direction_y = -1.0;
    flowParams.dt = dt;

    double idleCircuitFlow = 0;
    if (m_forcedInduction != nullptr && m_forcedInduction->isThrottleBeforeBlower()) {
        // Throttle before the blower: the throttle and the idle circuit
        // fill the blower's inlet, and the blower's delivery reaches the
        // manifold with nothing in the way
        GasSystem *inlet = m_forcedInduction->getInletSystem();
        m_atmosphere.reset(m_atmospherePressure, m_atmosphereTemperature, fuelAirMix);
        flowParams.system_0 = &m_atmosphere;
        flowParams.system_1 = inlet;
        flowParams.k_flow = flowAttenuation * m_inputFlowK;
        inlet->flow(flowParams);

        m_atmosphere.reset(m_atmospherePressure, m_atmosphereTemperature, fuelMix);
        flowParams.k_flow = m_idleFlowK;
        idleCircuitFlow = inlet->flow(flowParams);

        flowParams.crossSectionArea_0 = m_crossSectionArea;
        flowParams.system_0 = m_forcedInduction->getChargeSystem();
        flowParams.system_1 = &m_system;
        flowParams.k_flow = m_inputFlowK;
        m_flow = GasSystem::flow(flowParams);
    }
    else if (m_forcedInduction != nullptr) {
        // Boosted: the throttle and the idle circuit draw from the charge
        // air, which already carries the mixture
        GasSystem *charge = m_forcedInduction->getChargeSystem();
        flowParams.crossSectionArea_0 = m_crossSectionArea;
        flowParams.system_0 = charge;
        flowParams.system_1 = &m_system;
        flowParams.k_flow = flowAttenuation * m_inputFlowK;
        m_flow = GasSystem::flow(flowParams);

        flowParams.k_flow = m_idleFlowK;
        idleCircuitFlow = GasSystem::flow(flowParams);
        fuelMix = fuelAirMix;
    }
    else {
        m_atmosphere.reset(m_atmospherePressure, m_atmosphereTemperature, fuelAirMix);
        flowParams.system_0 = &m_atmosphere;
        flowParams.system_1 = &m_system;
        flowParams.k_flow = flowAttenuation * m_inputFlowK;
        m_flow = m_system.flow(flowParams);

        m_atmosphere.reset(m_atmospherePressure, m_atmosphereTemperature, fuelMix);
        flowParams.system_0 = &m_atmosphere;
        flowParams.system_1 = &m_system;
        flowParams.k_flow = m_idleFlowK;
        idleCircuitFlow = m_system.flow(flowParams);
    }

    m_system.dissipateExcessVelocity();
    m_system.updateVelocity(dt, m_velocityDecay);

    if (m_flow > 0) {
        m_totalFuelInjected += fuelAirMix.p_fuel * m_flow;
    }

    if (idleCircuitFlow > 0) {
        m_totalFuelInjected += fuelMix.p_fuel * idleCircuitFlow;
    }
}
