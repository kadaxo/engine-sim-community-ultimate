#include "../include/combustion_chamber.h"

#include "../include/constants.h"
#include "../include/units.h"
#include "../include/piston.h"
#include "../include/connecting_rod.h"
#include "../include/utilities.h"
#include "../include/exhaust_system.h"
#include "../include/cylinder_bank.h"
#include "../include/engine.h"
#include "../include/diesel_injection.h"

#include <cmath>
#include <cstdlib>

CombustionChamber::CombustionChamber() {
    m_crankcasePressure = 0.0;
    m_piston = nullptr;
    m_head = nullptr;
    m_engine = nullptr;
    m_pistonSpeed = nullptr;
    m_pressure = nullptr;
    m_lit = false;
    m_litLastFrame = false;
    m_afterburn = false;
    m_afterburnFuel = 0;
    m_afterburnWait = 0;
    m_afterburnTime = 0;
    m_peakTemperature = 0;

    m_meanPistonSpeedToTurbulence = nullptr;
    m_nBurntFuel = 0;

    m_manifoldToRunnerFlowRate = 0;
    m_primaryToCollectorFlowRate = 0;
    m_cylinderWidthApproximation = 0;
    m_cylinderCrossSectionSurfaceArea = 0;

    m_lastTimestepTotalExhaustFlow = 0;
    m_lastTimestepTotalIntakeFlow = 0;
    m_exhaustFlow = 0;
    m_exhaustFlowRate = 0;
    m_intakeFlowRate = 0;

    m_fuel = nullptr;

    m_dieselFuelInjected = 0;
    m_injectorTicks = 0;
}

CombustionChamber::~CombustionChamber() {
    assert(m_pistonSpeed == nullptr);
    assert(m_pressure == nullptr);
}

void CombustionChamber::initialize(const Parameters &params) {
    m_piston = params.piston;
    m_head = params.Head;
    m_fuel = params.fuel;
    m_crankcasePressure = params.CrankcasePressure;
    m_meanPistonSpeedToTurbulence = params.MeanPistonSpeedToTurbulence;

    m_pistonSpeed = new double[StateSamples];
    m_pressure = new double[StateSamples];
    for (int i = 0; i < StateSamples; ++i) {
        m_pistonSpeed[i] = 0;
        m_pressure[i] = 0;
    }

    Intake *intake = m_head->getIntake(m_piston->getCylinderIndex());
    ExhaustSystem *exhaust = m_head->getExhaustSystem(m_piston->getCylinderIndex());

    m_manifoldToRunnerFlowRate = intake->getRunnerFlowRate();
    m_primaryToCollectorFlowRate = exhaust->getPrimaryFlowRate();

    const double bore_r = m_head->getCylinderBank()->getBore() / 2.0;
    m_cylinderCrossSectionSurfaceArea = constants::pi * bore_r * bore_r;
    m_cylinderWidthApproximation = std::sqrt(m_cylinderCrossSectionSurfaceArea);

    const double height = getVolume() / m_cylinderCrossSectionSurfaceArea;
    m_system.setGeometry(
        m_cylinderWidthApproximation,
        height,
        1.0,
        0.0);

    const double intakeRunnerCrossSection = m_head->getIntakeRunnerCrossSectionArea();
    const double intakeRunnerWidth = std::sqrt(intakeRunnerCrossSection);
    const double manifoldRunnerLength = intake->getRunnerLength();
    const double manifoldRunnerVolume = intakeRunnerCrossSection * manifoldRunnerLength;
    const double totalIntakeRunnerVolume = m_head->getIntakeRunnerVolume() + manifoldRunnerVolume;
    const double overallIntakeRunnerLength = totalIntakeRunnerVolume / intakeRunnerCrossSection;
    m_intakeRunnerAndManifold.initialize(
        units::pressure(1.0, units::atm),
        totalIntakeRunnerVolume,
        units::celcius(25.0));
    m_intakeRunnerAndManifold.setGeometry(
        overallIntakeRunnerLength,
        intakeRunnerWidth,
        1.0,
        0.0);

    const double exhaustRunnerCrossSection = m_head->getExhaustRunnerCrossSectionArea();
    const double exhaustRunnerWidth = std::sqrt(exhaustRunnerCrossSection);
    const double exhaustTubeLength =
        exhaust->getPrimaryTubeLength() + m_head->getHeaderPrimaryLength(m_piston->getCylinderIndex());
    const double exhaustTubeVolume = exhaustRunnerCrossSection * exhaustTubeLength;
    const double totalExhaustRunnerVolume = m_head->getExhaustRunnerVolume() + exhaustTubeVolume;
    const double overallExhaustRunnerLength = totalExhaustRunnerVolume / exhaustRunnerCrossSection;
    m_exhaustRunnerAndPrimary.initialize(
        units::pressure(1.0, units::atm),
        totalExhaustRunnerVolume,
        units::celcius(25.0));
    m_exhaustRunnerAndPrimary.setGeometry(
        overallExhaustRunnerLength,
        exhaustRunnerWidth,
        1.0,
        0.0);
}

void CombustionChamber::destroy() {
    if (m_pistonSpeed != nullptr) delete[] m_pistonSpeed;
    if (m_pressure != nullptr) delete[] m_pressure;

    m_pistonSpeed = nullptr;
    m_pressure = nullptr;
}

double CombustionChamber::getVolume() const {
    const double combustionPortVolume = m_head->getCombustionChamberVolume();
    const CylinderBank *bank = m_head->getCylinderBank();

    const double area = bank->boreSurfaceArea();
    const double s =
        m_piston->relativeX() * bank->getDx()
        + m_piston->relativeY() * bank->getDy();
    const double sweep =
        area * (bank->getDeckHeight() - s - m_piston->getCompressionHeight());

    return sweep + combustionPortVolume - m_piston->getDisplacement();
}

double CombustionChamber::pistonSpeed() const {
    const CylinderBank *bank = m_head->getCylinderBank();
    return
        m_piston->m_body.v_x * bank->getDx()
        + m_piston->m_body.v_y * bank->getDy();
}

double CombustionChamber::calculateMeanPistonSpeed() const {
    double avg = 0;
    for (int i = 0; i < StateSamples; ++i) {
        avg += m_pistonSpeed[i];
    }

    avg /= StateSamples;
    return avg;
}

double CombustionChamber::calculateFiringPressure() const {
    double firingPressure = 0;
    for (int i = 0; i < StateSamples; ++i) {
        if (m_pressure[i] > firingPressure) {
            firingPressure = m_pressure[i];
        }
    }

    return firingPressure;
}

bool CombustionChamber::popLitLastFrame() {
    const bool lit = m_litLastFrame;
    m_litLastFrame = false;

    return lit;
}

int CombustionChamber::popInjectorTicks() {
    const int ticks = m_injectorTicks;
    m_injectorTicks = 0;

    return ticks;
}

void CombustionChamber::startDieselInjection() {
    const DieselInjection *injection = m_engine->getDieselInjection();
    const DieselInjection::Parameters &params = injection->getParameters();

    // The previous cycle's fuel that never lit is lost
    m_diesel = DieselEvent();

    const double o2PerFuel = m_fuel->getMolecularAfr();
    const double fullFuel = m_system.n_o2() / (o2PerFuel * params.fullLoadLambda);
    const double total = injection->getFuelDemand() * fullFuel;
    if (total <= 0 || fullFuel <= 0) {
        // No fuel this cycle (coasting): nothing for the air/fuel readout
        m_flameEvent.globalMix.p_fuel = 0;
        return;
    }

    m_diesel.active = true;
    m_diesel.rate = fullFuel / params.injectionDuration;      // [mol/s]
    m_diesel.pilotFuel = params.pilotInjection
        ? std::min(params.pilotFuel * fullFuel, 0.5 * total)
        : 0.0;
    m_diesel.mainFuel = total - m_diesel.pilotFuel;
    m_diesel.mainStart = params.pilotInjection ? params.pilotSeparation : 0.0;
    m_diesel.mainStarted = !params.pilotInjection;

    // Cycle to cycle variation of the spray and the burn
    const double r0 = (double)rand() / RAND_MAX;
    const double r1 = (double)rand() / RAND_MAX;
    m_diesel.premixedShare = clamp(params.premixedShare * (0.85 + 0.3 * r0));
    m_diesel.efficiency = m_fuel->getMaxBurningEfficiency()
        * (1.0 - 0.1 * m_fuel->getBurningEfficiencyRandomness() * r1);

    ++m_injectorTicks;

    // For the air/fuel ratio readout: the fuel as an equivalent mass of the
    // octane lastEventAfr() assumes
    constexpr double octaneMolarMass = units::mass(114.23, units::g);
    m_flameEvent.total_n = m_system.n();
    m_flameEvent.globalMix = m_system.mix();
    m_flameEvent.globalMix.p_fuel =
        total * m_fuel->getMolecularMass() / (octaneMolarMass * m_system.n());
    m_flameEvent.percentageLit = 0;
}

void CombustionChamber::burnDiesel(double dt) {
    if (!m_diesel.active) return;

    const DieselInjection *injection = m_engine->getDieselInjection();
    const double omega = std::abs(m_engine->getOutputCrankshaft()->m_body.v_theta);
    const double dTheta = omega * dt;
    m_diesel.angle += dTheta;

    // Spray: the pilot first, then the main injection at the same rate
    double sprayed = 0;
    if (m_diesel.pilotInjected < m_diesel.pilotFuel) {
        const double d = std::min(m_diesel.rate * dt, m_diesel.pilotFuel - m_diesel.pilotInjected);
        m_diesel.pilotInjected += d;
        sprayed += d;
    }

    if (m_diesel.angle >= m_diesel.mainStart && m_diesel.mainInjected < m_diesel.mainFuel) {
        if (!m_diesel.mainStarted) {
            m_diesel.mainStarted = true;
            ++m_injectorTicks;
        }

        const double d = std::min(m_diesel.rate * dt, m_diesel.mainFuel - m_diesel.mainInjected);
        m_diesel.mainInjected += d;
        sprayed += d;
    }

    m_dieselFuelInjected += sprayed;
    if (m_diesel.ignited) m_diesel.mixing += sprayed;
    else m_diesel.pending += sprayed;

    // Ignition delay, integrated over the changing pressure and temperature
    // (Livengood and Wu)
    if (!m_diesel.ignited && m_diesel.pending > 0) {
        const double delay = injection->ignitionDelayDegrees(
            m_system.pressure(), m_system.temperature(), calculateMeanPistonSpeed());
        m_diesel.delayProgress += (dTheta / units::angle(1, units::deg)) / delay;

        if (m_diesel.delayProgress >= 1.0) {
            m_diesel.ignited = true;
            m_diesel.premixed += m_diesel.pending * m_diesel.premixedShare;
            m_diesel.mixing += m_diesel.pending * (1 - m_diesel.premixedShare);
            m_diesel.pending = 0;
            m_lit = true;
            m_litLastFrame = true;
        }
    }

    // The premixed fuel burns within a fraction of a millisecond; the rest
    // as fast as it mixes, which follows the crank angle
    constexpr double premixedTime = 0.2e-3;
    const double mixingTime = std::max(
        units::angle(6, units::deg) / std::max(omega, 1.0),
        0.25e-3);
    const double burnPremixed = m_diesel.premixed * (1 - std::exp(-dt / premixedTime));
    const double burnMixing = m_diesel.mixing * (1 - std::exp(-dt / mixingTime));
    const double burn = burnPremixed + burnMixing;

    if (burn > 0) {
        const double burned = m_system.burnInjectedFuel(burn, m_fuel->getMolecularAfr());
        if (burned < burn * (1 - 1e-9)) {
            // Out of oxygen: the rest leaves as smoke
            m_diesel.premixed = m_diesel.mixing = 0;
        }
        else {
            m_diesel.premixed -= burnPremixed;
            m_diesel.mixing -= burnMixing;
        }

        const double massFuelBurned = burned * m_fuel->getMolecularMass();
        m_system.changeEnergy(massFuelBurned * m_fuel->getEnergyDensity() * m_diesel.efficiency);
        m_nBurntFuel += massFuelBurned;

        const double total = m_diesel.pilotFuel + m_diesel.mainFuel;
        if (total > 0) m_flameEvent.percentageLit += burned / total;
    }

    const bool injected =
        m_diesel.pilotInjected >= m_diesel.pilotFuel
        && m_diesel.mainInjected >= m_diesel.mainFuel;
    const double left = m_diesel.pending + m_diesel.premixed + m_diesel.mixing;
    const double total = m_diesel.pilotFuel + m_diesel.mainFuel;
    if ((injected && left <= 1e-4 * total) || m_diesel.angle > constants::pi) {
        // Burned out, or the exhaust valve is about to open on fuel that
        // never lit
        m_diesel.active = false;
        m_lit = false;
    }
}

void CombustionChamber::ignite() {
    if (m_engine != nullptr && m_engine->getDieselInjection() != nullptr) {
        startDieselInjection();
        return;
    }

    if (!m_lit) {
        if (m_system.mix().p_fuel == 0) return;

        // Anti-lag: it cuts the spark on about a third of the cycles, at
        // random. That cycle's whole charge is pushed out unburnt and lights
        // in the hot exhaust runner once it has gathered there: the bang (see
        // afterburn()). The other cycles burn very late in the cylinder.
        const ForcedInduction *fi = (m_engine != nullptr) ? m_engine->getForcedInduction() : nullptr;
        m_afterburn = false;
        if (fi != nullptr && fi->isAntiLagActive()
            && static_cast<double>(rand()) / RAND_MAX < 0.35)
        {
            m_afterburn = true;
            m_afterburnWait = 0.005 + 0.005 * static_cast<double>(rand()) / RAND_MAX;
            m_afterburnTime = 0.003;
            return;
        }

        const double afr = m_system.mix().p_o2 / m_system.mix().p_fuel;
        const double equivalenceRatio = afr / m_fuel->getMolecularAfr();
        if (equivalenceRatio < 0.5) return;
        else if (equivalenceRatio > 1.9) return;

        const double idealInert = m_system.mix().p_o2 / 0.7;
        const double dilution = (m_system.mix().p_inert / idealInert) - 1;

        m_flameEvent.lastVolume = getVolume();
        m_flameEvent.travel_x = 0;
        m_flameEvent.travel_y = 0;
        m_flameEvent.lit_n = 0;
        m_flameEvent.total_n = m_system.n();
        m_flameEvent.percentageLit = 0;
        m_flameEvent.globalMix = m_system.mix();
        m_lit = true;
        m_litLastFrame = true;

        const double randomness =
            m_fuel->getBurningEfficiencyRandomness();
        const double lowEfficiencyAttenuation =
            m_fuel->getLowEfficiencyAttenuation();
        const double maxBurningEfficiency =
            m_fuel->getMaxBurningEfficiency();
        const double maxTurbulenceEffect =
            m_fuel->getMaxTurbulenceEffect();
        const double maxDilutionEffect =
            m_fuel->getMaxDilutionEffect();

        const double turbulence =
            m_meanPistonSpeedToTurbulence->sampleTriangle(
                calculateMeanPistonSpeed());
        const double mixingFactor =
            1.0 - (
                clamp(turbulence / maxTurbulenceEffect)
                * clamp(1 - dilution / maxDilutionEffect));
        const double rand_s =
            lowEfficiencyAttenuation
            * ((1 - randomness) + randomness * ((double)rand() / RAND_MAX));
        const double efficiencyAttenuation =
            (mixingFactor * rand_s + (1 - mixingFactor));
        m_flameEvent.efficiency =
            efficiencyAttenuation * maxBurningEfficiency;
        m_flameEvent.flameSpeed = m_fuel->flameSpeed(
            turbulence,
            afr,
            m_system.temperature(),
            m_system.pressure(),
            calculateFiringPressure(),
            units::pressure(160, units::psi));
    }
}

void CombustionChamber::update(double dt) {
    m_system.setVolume(getVolume());

    updateCycleStates();

    m_intakeFlowRate = m_head->intakeFlowRate(m_piston->getCylinderIndex());
    m_exhaustFlowRate = m_head->exhaustFlowRate(m_piston->getCylinderIndex());
}

void CombustionChamber::flow(double dt) {
    if (m_system.temperature() > m_peakTemperature) {
        m_peakTemperature = m_system.temperature();
    }

    const double volume = getVolume();
    const double cylinderHeight = volume / m_cylinderCrossSectionSurfaceArea;
    const double cylinderSurfaceArea =
        cylinderHeight * constants::pi * m_head->getCylinderBank()->getBore()
        + m_cylinderCrossSectionSurfaceArea * 2;

    const double dT = units::celcius(90.0) - m_system.temperature();

    m_system.changeEnergy(dT * cylinderSurfaceArea * 100 * dt);
    m_system.flow(m_piston->getBlowbyK(), dt, m_crankcasePressure, units::celcius(25.0));

    Intake *intake = m_head->getIntake(m_piston->getCylinderIndex());
    ExhaustSystem *exhaust = m_head->getExhaustSystem(m_piston->getCylinderIndex());

    const double start_n = m_system.n();

    GasSystem::FlowParameters flowParams;
    flowParams.dt = dt;

    flowParams.k_flow = m_manifoldToRunnerFlowRate;
    flowParams.crossSectionArea_0 = intake->getPlenumCrossSectionArea();
    flowParams.crossSectionArea_1 = m_head->getIntakeRunnerCrossSectionArea();
    flowParams.direction_x = 1.0;
    flowParams.direction_y = 0.0;
    flowParams.system_0 = &intake->m_system;
    flowParams.system_1 = &m_intakeRunnerAndManifold;
    GasSystem::flow(flowParams);

    m_intakeRunnerAndManifold.dissipateExcessVelocity();

    flowParams.k_flow = m_intakeFlowRate;
    flowParams.crossSectionArea_0 = m_head->getIntakeRunnerCrossSectionArea();
    flowParams.crossSectionArea_1 = volume / cylinderHeight;
    flowParams.direction_x = 1.0;
    flowParams.direction_y = 0.0;
    flowParams.system_0 = &m_intakeRunnerAndManifold;
    flowParams.system_1 = &m_system;
    const double intakeFlow = GasSystem::flow(flowParams);

    m_intakeRunnerAndManifold.dissipateExcessVelocity();
    m_system.dissipateExcessVelocity();

    flowParams.k_flow = m_exhaustFlowRate;
    flowParams.crossSectionArea_0 = volume / cylinderHeight;
    flowParams.crossSectionArea_1 = m_head->getExhaustRunnerCrossSectionArea();
    flowParams.direction_x = 1.0;
    flowParams.direction_y = 0.0;
    flowParams.system_0 = &m_system;
    flowParams.system_1 = &m_exhaustRunnerAndPrimary;
    const double exhaustFlow = GasSystem::flow(flowParams);

    m_system.dissipateExcessVelocity();
    m_exhaustRunnerAndPrimary.dissipateExcessVelocity();

    flowParams.k_flow = m_primaryToCollectorFlowRate;
    flowParams.crossSectionArea_0 = m_head->getExhaustRunnerCrossSectionArea();
    flowParams.crossSectionArea_1 = exhaust->getCollectorCrossSectionArea();
    flowParams.direction_x = 1.0;
    flowParams.direction_y = 0.0;
    flowParams.system_0 = &m_exhaustRunnerAndPrimary;
    flowParams.system_1 = exhaust->getSystem();
    GasSystem::flow(flowParams);

    m_intakeRunnerAndManifold.updateVelocity(dt, intake->getVelocityDecay());
    m_system.updateVelocity(dt, 0.5);
    m_exhaustRunnerAndPrimary.updateVelocity(dt, exhaust->getVelocityDecay());

    if (m_afterburn) afterburn(dt);

    if (std::abs(intakeFlow) > 1E-9 && m_lit) {
        m_lit = false;
    }

    m_exhaustFlow = exhaustFlow;
    m_lastTimestepTotalExhaustFlow += exhaustFlow;
    m_lastTimestepTotalIntakeFlow += intakeFlow;

    if (m_diesel.active) {
        if (std::abs(intakeFlow) > 1E-9) {
            m_diesel.active = false;
            m_lit = false;
        }
        else {
            burnDiesel(dt);
        }

        return;
    }

    if (m_lit) {
        CylinderBank *bank = m_head->getCylinderBank();
        const double totalTravel_x = bank->getBore() / 2;
        const double totalTravel_y = volume / bank->boreSurfaceArea();
        const double expansion = volume / m_flameEvent.lastVolume;
        const double lastTravel_x = m_flameEvent.travel_x;
        const double lastTravel_y = m_flameEvent.travel_y * expansion;
        const double flameSpeed = m_flameEvent.flameSpeed;

        m_flameEvent.travel_x =
            std::fmin(lastTravel_x + dt * flameSpeed, totalTravel_x);
        m_flameEvent.travel_y =
            std::fmin(lastTravel_y + dt * flameSpeed, totalTravel_y);

        if (lastTravel_x < m_flameEvent.travel_x || lastTravel_y < m_flameEvent.travel_y) {
            const double burnedVolume =
                m_flameEvent.travel_x * m_flameEvent.travel_x
                * constants::pi * m_flameEvent.travel_y;
            const double prevBurnedVolume =
                lastTravel_x * lastTravel_x * constants::pi * lastTravel_y;
            const double litVolume = burnedVolume - prevBurnedVolume;
            const double n = (litVolume / volume) * m_system.n();

            const double fuelBurned =
                m_system.react(n * m_flameEvent.efficiency, m_flameEvent.globalMix);
            const double massFuelBurned = fuelBurned * m_fuel->getMolecularMass();
            m_system.changeEnergy(
                massFuelBurned * m_fuel->getEnergyDensity());

            m_flameEvent.lit_n += n;
            m_flameEvent.percentageLit += litVolume / volume;

            m_nBurntFuel += massFuelBurned;
        }
        else {
            m_lit = false;
        }

        m_flameEvent.lastVolume = volume;
    }
}

// Anti-lag: the fuel and air a cut spark leaves unburnt reach the exhaust
// runner, where the other cycles' late-burning gas keeps it hot enough
// (above about 900 K) to light them. They light 5 to 10 ms after the cut
// spark, once the charge has been pushed out and gathered (with what the
// late-burning cycles left), and go up in about 0.3 ms: a sharp rise in the
// runner's pressure, the bang, and the energy that keeps the turbo spinning.
// Only while the anti-lag system is on, so the other engines' unburnt fuel
// stays unburnt as it always has.
void CombustionChamber::afterburn(double dt) {
    const ForcedInduction *fi = (m_engine != nullptr) ? m_engine->getForcedInduction() : nullptr;
    if (fi == nullptr || !fi->isAntiLagActive()) {
        m_afterburn = false;
        return;
    }

    if (m_afterburnWait > 0) {
        m_afterburnWait -= dt;
        return;
    }
    m_afterburnTime -= dt;
    if (m_afterburnTime <= 0) m_afterburn = false;

    GasSystem &runner = m_exhaustRunnerAndPrimary;
    const GasSystem::Mix mix = runner.mix();
    if (mix.p_fuel <= 0 || mix.p_o2 <= 0 || runner.temperature() < 900.0) return;

    const double share = std::min(dt / 0.0003, 1.0);
    const double fuelBurned = runner.react(runner.n() * share, mix);
    runner.changeEnergy(fuelBurned * m_fuel->getMolecularMass() * m_fuel->getEnergyDensity());
    m_afterburnFuel += fuelBurned * m_fuel->getMolecularMass();
}

double CombustionChamber::lastEventAfr() const {
    const double totalFuel = m_flameEvent.globalMix.p_fuel * m_flameEvent.total_n;
    const double totalOxygen = m_flameEvent.globalMix.p_o2 * m_flameEvent.total_n;
    const double totalInert = m_flameEvent.globalMix.p_inert * m_flameEvent.total_n;

    constexpr double octaneMolarMass = units::mass(114.23, units::g);
    constexpr double oxygenMolarMass = units::mass(31.9988, units::g);
    constexpr double nitrogenMolarMass = units::mass(28.014, units::g);

    if (totalFuel == 0) return 0;
    else {
        return
            (oxygenMolarMass * totalOxygen + totalInert * nitrogenMolarMass)
            / (totalFuel * octaneMolarMass);
    }
}

double CombustionChamber::calculateFrictionForce(double v_s) const {
    const double cylinderWallForce = m_piston->calculateCylinderWallForce();

    const double F_coul = m_frictionModel.frictionCoeff * cylinderWallForce;
    const double v_st = m_frictionModel.breakawayFrictionVelocity * constants::root_2;
    const double v_coul = m_frictionModel.breakawayFrictionVelocity / 10;
    const double F_brk = m_frictionModel.breakawayFriction;
    const double v = std::abs(v_s);

    const double F_0 = constants::root_2 * constants::e * (F_brk - F_coul);
    const double F_1 = v / v_st;
    const double F_2 = std::exp(-F_1 * F_1) * F_1;
    const double F_3 = F_coul * std::tanh(v / v_coul);
    const double F_4 = m_frictionModel.viscousFrictionCoefficient * v;

    return F_0 * F_2 + F_3 + F_4;
}

void CombustionChamber::updateCycleStates() {
    double crankAngle = m_engine->getOutputCrankshaft()->getCycleAngle();
    if (std::isnan(crankAngle) || std::isinf(crankAngle)) {
        crankAngle = 0.0;
    }

    const int i = (int)std::round((crankAngle / (4 * constants::pi)) * (StateSamples - 1.0));

    m_pistonSpeed[i] = std::abs(pistonSpeed());
    m_pressure[i] = m_system.pressure();
}

void CombustionChamber::apply(atg_scs::SystemState *system) {
    CylinderBank *bank = m_head->getCylinderBank();
    const double area = (bank->getBore() * bank->getBore() / 4.0) * constants::pi;
    const double v_x = system->v_x[m_piston->m_body.index];
    const double v_y = system->v_y[m_piston->m_body.index];

    const double v_s =
        v_x * bank->getDx() + v_y * bank->getDy();

    const double pressureDifferential = m_system.pressure() - m_crankcasePressure;
    const double force = -area * pressureDifferential;

    if (std::isnan(force) || std::isinf(force)) {
        assert(false);
    }

    constexpr double limit = 1E-3;
    const double abs_v_s = std::fmin(std::abs(v_s), limit);
    const double attenuation = abs_v_s / limit;

    const double F = calculateFrictionForce(v_s) * attenuation;
    const double F_fric = (v_s > 0)
        ? -F
        : F;

    system->applyForce(
        0.0,
        0.0,
        (force + F_fric) * bank->getDx(),
        (force + F_fric) * bank->getDy(),
        m_piston->m_body.index);
}

double CombustionChamber::getFrictionForce() const {
    CylinderBank *bank = m_head->getCylinderBank();
    const double v_x = m_piston->m_body.v_x;
    const double v_y = m_piston->m_body.v_y;

    const double v_s =
        v_x * bank->getDx() + v_y * bank->getDy();

    return calculateFrictionForce(v_s);
}
