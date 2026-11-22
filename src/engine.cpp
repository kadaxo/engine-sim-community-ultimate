
#include "../include/engine.h"

#include "../include/constants.h"
#include "../include/units.h"
#include "../include/fuel.h"
#include "../include/piston_engine_simulator.h"
#include "../include/utilities.h"

#include <cmath>
#include <assert.h>

Engine::Engine() {
    m_name = "";

    m_crankshafts = nullptr;
    m_cylinderBanks = nullptr;
    m_heads = nullptr;
    m_pistons = nullptr;
    m_connectingRods = nullptr;
    m_exhaustSystems = nullptr;
    m_intakes = nullptr;
    m_forcedInduction = nullptr;
    m_dieselInjection = nullptr;
    m_combustionChambers = nullptr;

    m_crankshaftCount = 0;
    m_cylinderBankCount = 0;
    m_cylinderCount = 0;
    m_intakeCount = 0;
    m_exhaustSystemCount = 0;
    m_starterSpeed = 0;
    m_starterTorque = 0;
    m_dynoMinSpeed = 0;
    m_dynoMaxSpeed = 0;
    m_dynoHoldStep = 0;
    m_redline = 0;

    m_throttle = nullptr;
    m_throttleValue = 0.0;
    m_displacement = 0.0;

    m_initialSimulationFrequency = 10000.0;
    m_initialHighFrequencyGain = 0.01;
    m_location = 0;
    m_initialJitter = 0.5;
    m_initialNoise = 1.0;
}

Engine::~Engine() {
    assert(m_crankshafts == nullptr);
    assert(m_cylinderBanks == nullptr);
    assert(m_pistons == nullptr);
    assert(m_connectingRods == nullptr);
    assert(m_heads == nullptr);
    assert(m_exhaustSystems == nullptr);
    assert(m_intakes == nullptr);
    assert(m_forcedInduction == nullptr);
    assert(m_combustionChambers == nullptr);
}

void Engine::initialize(const Parameters &params) {
    m_crankshaftCount = params.crankshaftCount;
    m_cylinderCount = params.cylinderCount;
    m_cylinderBankCount = params.cylinderBanks;
    m_exhaustSystemCount = params.exhaustSystemCount;
    m_intakeCount = params.intakeCount;
    m_starterTorque = params.starterTorque;
    m_starterSpeed = params.starterSpeed;
    m_dynoMinSpeed = params.dynoMinSpeed;
    m_dynoMaxSpeed = params.dynoMaxSpeed;
    m_dynoHoldStep = params.dynoHoldStep;
    m_redline = params.redline;
    m_name = params.name;
    m_throttle = params.throttle;
    m_initialHighFrequencyGain = params.initialHighFrequencyGain;
    m_location = params.location;
    m_initialSimulationFrequency = params.initialSimulationFrequency;
    m_initialJitter = params.initialJitter;
    m_initialNoise = params.initialNoise;

    m_crankshafts = new Crankshaft[m_crankshaftCount];
    m_cylinderBanks = new CylinderBank[m_cylinderBankCount];
    m_heads = new CylinderHead[m_cylinderBankCount];
    m_pistons = new Piston[m_cylinderCount];
    m_connectingRods = new ConnectingRod[m_cylinderCount];
    m_exhaustSystems = new ExhaustSystem[m_exhaustSystemCount];
    m_intakes = new Intake[m_intakeCount];
    m_combustionChambers = new CombustionChamber[m_cylinderCount];

    for (int i = 0; i < m_exhaustSystemCount; ++i) {
        m_exhaustSystems[i].m_index = i;
    }

    for (int i = 0; i < m_cylinderCount; ++i) {
        m_combustionChambers[i].setEngine(this);
    }
}

void Engine::setForcedInduction(ForcedInduction *forcedInduction) {
    if (m_forcedInduction != nullptr) {
        m_forcedInduction->destroy();
        delete m_forcedInduction;
    }

    m_forcedInduction = forcedInduction;
    if (forcedInduction == nullptr) return;

    forcedInduction->setCrankshaft(getOutputCrankshaft());
    forcedInduction->setExhaustSystemCount(m_exhaustSystemCount);
    for (int i = 0; i < m_intakeCount; ++i) {
        m_intakes[i].setForcedInduction(forcedInduction);
        forcedInduction->addIntake(&m_intakes[i]);
    }

    if (forcedInduction->isTurbocharger()) {
        for (int i = 0; i < m_exhaustSystemCount; ++i) {
            m_exhaustSystems[i].setForcedInduction(forcedInduction);
        }
    }
}

void Engine::setDieselInjection(DieselInjection *dieselInjection) {
    if (m_dieselInjection != nullptr) delete m_dieselInjection;

    m_dieselInjection = dieselInjection;
    for (int i = 0; i < m_intakeCount; ++i) {
        m_intakes[i].setDirectInjection(dieselInjection != nullptr);
    }

    m_ignitionModule.setEventAdvance(
        (dieselInjection != nullptr && dieselInjection->getParameters().pilotInjection)
            ? dieselInjection->getParameters().pilotSeparation
            : 0.0);

    // Re-apply the throttle so the plates open
    setThrottle(m_throttleValue);
}

void Engine::destroy() {
    if (m_forcedInduction != nullptr) {
        m_forcedInduction->destroy();
        delete m_forcedInduction;
    }

    if (m_dieselInjection != nullptr) {
        delete m_dieselInjection;
        m_dieselInjection = nullptr;
    }

    for (int i = 0; i < m_crankshaftCount; ++i) {
        m_crankshafts[i].destroy();
    }

    for (int i = 0; i < m_cylinderCount; ++i) {
        m_pistons[i].destroy();
        m_connectingRods[i].destroy();
        m_combustionChambers[i].destroy();
    }

    for (int i = 0; i < m_exhaustSystemCount; ++i) {
        m_exhaustSystems[i].destroy();
    }

    for (int i = 0; i < m_intakeCount; ++i) {
        m_intakes[i].destroy();
    }

    m_ignitionModule.destroy();

    if (m_throttle != nullptr) delete m_throttle;
    if (m_crankshafts != nullptr) delete[] m_crankshafts;
    if (m_cylinderBanks != nullptr) delete[] m_cylinderBanks;
    if (m_heads != nullptr) delete[] m_heads;
    if (m_pistons != nullptr) delete[] m_pistons;
    if (m_connectingRods != nullptr) delete[] m_connectingRods;
    if (m_exhaustSystems != nullptr) delete[] m_exhaustSystems;
    if (m_intakes != nullptr) delete[] m_intakes;
    if (m_combustionChambers != nullptr) delete[] m_combustionChambers;

    m_crankshafts = nullptr;
    m_cylinderBanks = nullptr;
    m_pistons = nullptr;
    m_connectingRods = nullptr;
    m_heads = nullptr;
    m_exhaustSystems = nullptr;
    m_intakes = nullptr;
    m_forcedInduction = nullptr;
    m_combustionChambers = nullptr;
    m_throttle = nullptr;
}

Crankshaft *Engine::getOutputCrankshaft() const {
    return &m_crankshafts[0];
}

void Engine::setTargetSpeedControl(double s) {
    m_targetSpeedControl = clamp(s);
}

double Engine::getTargetSpeedControl() {
    return m_targetSpeedControl;
}

void Engine::updateSpeedControl() {
    setSpeedControl(m_targetSpeedControl * 0.5 + 0.5 * getSpeedControl());
}

void Engine::setSpeedControl(double s) {
    m_throttle->setSpeedControl(s);
}

double Engine::getSpeedControl() {
    return m_throttle->getSpeedControl();
}

void Engine::setThrottle(double throttle) {
    // A diesel has no throttle plates: the pedal sets the fuel
    if (m_dieselInjection != nullptr) {
        m_dieselInjection->setPedal(clamp(1.0 - throttle));
    }

    // Anti-lag: with the driver's throttle below the opening it holds and
    // the engine above its minimum speed, it holds the plates that far open
    double plates = throttle;
    if (m_forcedInduction != nullptr && m_forcedInduction->hasAntiLag()) {
        const double opening = m_forcedInduction->getAntiLagThrottle();
        const bool active = (1.0 - throttle) < opening && getSpeed() > m_forcedInduction->getAntiLagMinSpeed();
        m_forcedInduction->setAntiLagActive(active);
        if (active) plates = std::min(plates, 1.0 - opening);
    }

    for (int i = 0; i < m_intakeCount; ++i) {
        m_intakes[i].m_throttle = (m_dieselInjection != nullptr) ? 0.0 : plates;
    }

    m_throttleValue = throttle;
}

double Engine::getThrottle() const {
    return m_throttleValue;
}

double Engine::getThrottlePlateAngle() const {
    return (1 - m_intakes[0].getThrottlePlatePosition()) * (constants::pi / 2);
}

bool placeRod(
    const ConnectingRod &rod,
    const CylinderBank &bank,
    const Crankshaft &crankshaft,
    double crankshaftAngle,
    double *p_x,
    double *p_y,
    double *theta,
    double *s)
{
    double p_x_0, p_y_0, l_x, l_y, theta_0;
    if (rod.getMasterRod() != nullptr) {
        double s;
        const bool succeeded = placeRod(
            *rod.getMasterRod(),
            *rod.getMasterRod()->getPiston()->getCylinderBank(),
            *rod.getCrankshaft(),
            crankshaftAngle,
            &p_x_0,
            &p_y_0,
            &theta_0,
            &s);

        if (!succeeded) {
            return false;
        }

        rod.getMasterRod()->getRodJournalPositionLocal(rod.getJournal(), &l_x, &l_y);
        // Recursive placement returns the master's big-end position, while
        // its journal coordinates are relative to the rod body's origin.
        l_y -= rod.getMasterRod()->getBigEndLocal();
    }
    else {
        theta_0 = crankshaftAngle;
        p_x_0 = rod.getCrankshaft()->getPosX();
        p_y_0 = rod.getCrankshaft()->getPosY();
        rod.getCrankshaft()->getRodJournalPositionLocal(rod.getJournal(), &l_x, &l_y);
    }

    const double dx = std::cos(theta_0);
    const double dy = std::sin(theta_0);
    *p_x = p_x_0 + (dx * l_x - dy * l_y);
    *p_y = p_y_0 + (dy * l_x + dx * l_y);

    // (bank->m_x + bank->m_dx * s - p_x)^2 + (bank->m_y + bank->m_dy * s - p_y)^2 = (rod->m_length)^2
    const double a = bank.getDx() * bank.getDx() + bank.getDy() * bank.getDy();
    const double b = -2 * bank.getDx() * ((*p_x) - bank.getX()) - 2 * bank.getDy() * ((*p_y) - bank.getY());
    const double c =
        ((*p_x) - bank.getX()) * ((*p_x) - bank.getX())
        + ((*p_y) - bank.getY()) * ((*p_y) - bank.getY())
        - rod.getLength() * rod.getLength();

    const double det = b * b - 4 * a * c;
    if (det < 0) return false;

    const double sqrt_det = std::sqrt(det);
    const double s0 = (-b + sqrt_det) / (2 * a);
    const double s1 = (-b - sqrt_det) / (2 * a);

    *s = std::max(s0, s1);
    if (*s < 0) return false;
   
    if (theta != nullptr) {
        const double dx = (bank.getX() + bank.getDx() * (*s)) - (*p_x);
        const double dy = (bank.getY() + bank.getDy() * (*s)) - (*p_y);

        *theta = std::atan2(dy, dx) - constants::pi / 2;
    }

    return true;
}

void Engine::calculateDisplacement() {
    // There is a closed-form/correct way to do this which I really
    // don't feel like deriving right now, so I'm just going with this
    // numerical approximation.
    constexpr int Resolution = 1000;

    double *min_s = new double[m_cylinderCount];
    double *max_s = new double[m_cylinderCount];

    for (int i = 0; i < m_cylinderCount; ++i) {
        min_s[i] = DBL_MAX;
        max_s[i] = -DBL_MAX;
    }

    for (int j = 0; j < Resolution; ++j) {
        const double crankshaftAngle = 2 * (j / static_cast<double>(Resolution)) * constants::pi;

        for (int i = 0; i < m_cylinderCount; ++i) {
            const Piston &piston = m_pistons[i];
            const CylinderBank &bank = *piston.getCylinderBank();
            const ConnectingRod &rod = *piston.getRod();
            const Crankshaft &shaft = *rod.getCrankshaft();

            double p_x, p_y;
            double theta;
            double s;
            if (!placeRod(
                rod,
                bank,
                shaft,
                crankshaftAngle,
                &p_x,
                &p_y,
                &theta,
                &s))
            {
                continue;
            }

            min_s[i] = std::min(min_s[i], s);
            max_s[i] = std::max(max_s[i], s);
        }
    }

    double displacement = 0;
    for (int i = 0; i < m_cylinderCount; ++i) {
        const Piston &piston = m_pistons[i];
        const CylinderBank &bank = *piston.getCylinderBank();

        if (min_s[i] < max_s[i]) {
            const double r = bank.getBore() / 2.0;
            displacement += constants::pi * r * r * (max_s[i] - min_s[i]);
        }
    }

    delete[] min_s;
    delete[] max_s;
    m_displacement = displacement;
}

double Engine::getIntakeFlowRate() const {
    double airIntake = 0;
    for (int i = 0; i < m_intakeCount; ++i) {
        airIntake += m_intakes[i].m_flowRate;
    }

    return airIntake;
}

void Engine::update(double dt) {
    m_throttle->update(dt, this);

    if (m_dieselInjection != nullptr) {
        m_dieselInjection->update(dt, getSpeed());
    }
}

double Engine::getManifoldPressure() const {
    double pressureSum = 0.0;
    for (int i = 0; i < m_intakeCount; ++i) {
        pressureSum += m_intakes[i].m_system.pressure();
    }

    return pressureSum / m_intakeCount;
}

double Engine::getIntakeAfr() const {
    // A diesel's intake carries no fuel: its ratio is the cylinders', from
    // their last injections. A cylinder given next to no fuel (the governor
    // easing off) counts as unfueled, as one given none does, rather than
    // reading thousands to one
    if (m_dieselInjection != nullptr) {
        constexpr double LeanestAfr = 200.0;
        double sum = 0;
        int n = 0;
        for (int i = 0; i < m_cylinderCount; ++i) {
            const double afr = m_combustionChambers[i].lastEventAfr();
            if (afr > 0 && afr < LeanestAfr) {
                sum += afr;
                ++n;
            }
        }

        return (n > 0) ? sum / n : 0.0;
    }

    double totalOxygen = 0.0;
    double totalFuel = 0.0;
    for (int i = 0; i < m_intakeCount; ++i) {
        totalOxygen += m_intakes[i].m_system.n_o2();
        totalFuel += m_intakes[i].m_system.n_fuel();
    }

    constexpr double octaneMolarMass = units::mass(114.23, units::g);
    constexpr double oxygenMolarMass = units::mass(31.9988, units::g);

    if (totalFuel == 0) return 0;
    else {
        return
            (oxygenMolarMass * totalOxygen / 0.21)
            / (totalFuel * octaneMolarMass);
    }
}

double Engine::getExhaustO2() const {
    double totalInert = 0.0;
    double totalOxygen = 0.0;
    double totalFuel = 0.0;
    for (int i = 0; i < m_exhaustSystemCount; ++i) {
        totalInert += m_exhaustSystems[i].m_system.n_inert();
        totalOxygen += m_exhaustSystems[i].m_system.n_o2();
        totalFuel += m_exhaustSystems[i].m_system.n_fuel();
    }

    constexpr double octaneMolarMass = units::mass(114.23, units::g);
    constexpr double oxygenMolarMass = units::mass(31.9988, units::g);
    constexpr double nitrogenMolarMass = units::mass(28.014, units::g);

    // A diesel's exhaust carries no unburned fuel gas but plenty of oxygen
    if (totalFuel == 0 && m_dieselInjection == nullptr) return 0;
    if (totalOxygen + totalInert + totalFuel <= 0) return 0;
    else {
        return
            (oxygenMolarMass * totalOxygen)
            / (
                totalFuel * octaneMolarMass
                + nitrogenMolarMass * totalInert
                + oxygenMolarMass * totalOxygen);
    }
}

void Engine::resetFuelConsumption() {
    for (int i = 0; i < m_intakeCount; ++i) {
        m_intakes[i].m_totalFuelInjected = 0;
    }

    for (int i = 0; i < m_cylinderCount; ++i) {
        m_combustionChambers[i].resetDieselFuelInjected();
    }
}

double Engine::getTotalFuelMassConsumed() const {
    double n_fuelConsumed = 0;
    for (int i = 0; i < m_intakeCount; ++i) {
        n_fuelConsumed += m_intakes[i].m_totalFuelInjected;
    }

    for (int i = 0; i < m_cylinderCount; ++i) {
        n_fuelConsumed += m_combustionChambers[i].getDieselFuelInjected();
    }

    return n_fuelConsumed * m_fuel.getMolecularMass();
}

double Engine::getTotalVolumeFuelConsumed() const {
    return getTotalFuelMassConsumed() / m_fuel.getDensity();
}

int Engine::getMaxDepth() const {
    int maxDepth = 0;
    for (int i = 0; i < m_crankshaftCount; ++i) {
        maxDepth = std::max(m_crankshafts[i].getRodJournalCount(), maxDepth);
    }

    return maxDepth;
}

Simulator *Engine::createSimulator(Vehicle *vehicle, Transmission *transmission) {
    PistonEngineSimulator *simulator = new PistonEngineSimulator;
    Simulator::Parameters simulatorParams;
    simulatorParams.systemType = Simulator::SystemType::NsvOptimized;
    simulator->initialize(simulatorParams);

    simulator->loadSimulation(this, vehicle, transmission);
    simulator->setFluidSimulationSteps(8);

    return static_cast<Simulator *>(simulator);
}

double Engine::getRpm() const {
    if (m_crankshaftCount == 0) return 0;
    else return std::abs(units::toRpm(getCrankshaft(0)->m_body.v_theta));
}

double Engine::getSpeed() const {
    if (m_crankshaftCount == 0) return 0;
    else return std::abs(getCrankshaft(0)->m_body.v_theta);
}

bool Engine::isSpinningCw() const {
    return getOutputCrankshaft()->m_body.v_theta <= 0;
}
