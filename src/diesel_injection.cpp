#include "../include/diesel_injection.h"

#include "../include/constants.h"
#include "../include/utilities.h"

#include <algorithm>
#include <cmath>

namespace {

    // Idle governor: proportional, integral and speed rate gains on the
    // speed error as a share of the idle speed
    constexpr double IdleKp = 2.0;
    constexpr double IdleKi = 1.5;          // [1/s]
    constexpr double IdleKd = 0.12;         // [s]
    constexpr double IdleMaxDemand = 0.6;

} /* namespace */

DieselInjection::DieselInjection() {
    m_pedal = 0;
    m_demand = 0;
    m_idleDemand = 0;
    m_idleIntegral = 0;
    m_lastSpeed = 0;
    m_speed[0] = m_speed[1] = 0;
    m_speedRate = 0;
}

DieselInjection::~DieselInjection() {
    /* void */
}

void DieselInjection::initialize(const Parameters &params) {
    m_params = params;
    m_params.idleSpeed = std::max(params.idleSpeed, units::rpm(100));
    m_params.maxSpeed = std::max(params.maxSpeed, 0.0);
    m_params.governorDroop = clamp(params.governorDroop, 0.01, 0.5);
    m_params.startFuel = clamp(params.startFuel, 0.0, 1.0);
    m_params.fullLoadLambda = std::max(params.fullLoadLambda, 0.8);
    m_params.injectionDuration = std::max(params.injectionDuration, 0.1e-3);
    m_params.pilotFuel = clamp(params.pilotFuel, 0.0, 0.5);
    m_params.pilotSeparation = std::max(params.pilotSeparation, 0.0);
    m_params.ignitionDelay = std::max(params.ignitionDelay, 0.01);
    m_params.premixedShare = clamp(params.premixedShare, 0.0, 1.0);
    m_params.combustionNoise = std::max(params.combustionNoise, 0.0);
    m_params.injectorNoise = std::max(params.injectorNoise, 0.0);

    m_pedal = 0;
    m_demand = 0;
    m_idleDemand = 0;
    m_idleIntegral = 0;
    m_lastSpeed = 0;
    m_speed[0] = m_speed[1] = 0;
    m_speedRate = 0;
}

void DieselInjection::update(double dt, double engineSpeed) {
    if (dt <= 0) return;

    // The governor follows the mean speed, as a flyweight governor does: the
    // crank slows down at every compression stroke, so the speed at the
    // injections is below the mean. Two filters of a third of a cycle each.
    const double idle = m_params.idleSpeed;
    const double cycle = 4 * constants::pi / std::max(m_speed[1], 0.25 * idle);
    const double alpha = std::min(1.0, dt / (cycle / 3.0));
    m_speed[0] += (std::abs(engineSpeed) - m_speed[0]) * alpha;
    m_speed[1] += (m_speed[0] - m_speed[1]) * alpha;
    const double speed = m_speed[1];

    m_speedRate = (speed - m_lastSpeed) / dt;
    m_lastSpeed = speed;

    if (speed < 0.5 * idle) {
        // Cranking: the start fuel, and the integral starts over once the
        // engine catches
        m_idleIntegral = 0;
        m_idleDemand = m_params.startFuel;
    }
    else {
        const double error = (idle - speed) / idle;
        m_idleIntegral = clamp(m_idleIntegral + IdleKi * error * dt, 0.0, IdleMaxDemand);
        m_idleDemand = clamp(
            IdleKp * error + m_idleIntegral - IdleKd * m_speedRate / idle,
            0.0,
            IdleMaxDemand);
    }

    double limit = 1.0;
    if (m_params.maxSpeed > 0) {
        limit = clamp(
            (m_params.maxSpeed - speed) / (m_params.governorDroop * m_params.maxSpeed),
            0.0,
            1.0);
    }

    m_demand = std::max(clamp(m_pedal), m_idleDemand) * limit;
}

double DieselInjection::ignitionDelayDegrees(double pressure, double temperature, double meanPistonSpeed) const {
    // Hardenberg and Hase (1979), from Heywood, Internal Combustion Engine
    // Fundamentals: tau = (0.36 + 0.22 Sp) exp(Ea (1 / (R T) - 1 / 17190)
    // (21.2 / (p - 12.4))^0.63) crank degrees, p in bar, Ea = 618840 /
    // (CN + 25) J/mol
    constexpr double CetaneNumber = 50.0;
    constexpr double Ea = 618840.0 / (CetaneNumber + 25.0);
    constexpr double R = 8.3143;

    const double p = pressure / units::bar;
    const double T = std::max(temperature, 300.0);
    const double pressureTerm = std::pow(21.2 / std::max(p - 12.4, 0.5), 0.63);
    const double exponent = Ea * (1.0 / (R * T) - 1.0 / 17190.0) * pressureTerm;
    const double tau = (0.36 + 0.22 * std::abs(meanPistonSpeed)) * std::exp(std::min(exponent, 50.0));

    return std::max(tau * m_params.ignitionDelay, 0.05);
}
