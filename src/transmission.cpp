#include "../include/transmission.h"

#include "../include/units.h"

#include <cmath>

namespace {
    // A shift that hasn't matched revs by then engages the clutch anyway, so
    // it can't hang (e.g. throttle held through an upshift)
    constexpr double MaxShiftTime = 0.5;

    // Pulling away in first: the clutch starts slipping at this pressure and
    // is fully in after LaunchTime
    constexpr double LaunchClutchPressure = 0.1;
    constexpr double LaunchTime = 1.5;
}

Transmission::Transmission() {
    m_gear = -1;
    m_newGear = -1;
    m_gearCount = 0;
    m_gearRatios = nullptr;
    m_maxClutchTorque = units::torque(1000.0, units::ft_lb);
    m_rotatingMass = nullptr;
    m_vehicle = nullptr;
    m_engine = nullptr;
    m_clutchPressure = 0.0;
    m_targetRPM = 0.0;
    m_smartShifting = false;
    m_upshift = false;
    m_downshift = false;
    m_shiftTime = 0.0;
}

Transmission::~Transmission() {
    if (m_gearRatios != nullptr) {
        delete[] m_gearRatios;
    }

    m_gearRatios = nullptr;
}

void Transmission::initialize(const Parameters &params) {
    m_gearCount = params.GearCount;
    m_maxClutchTorque = params.MaxClutchTorque;
    m_gearRatios = new double[params.GearCount];
    memcpy(m_gearRatios, params.GearRatios, sizeof(double) * m_gearCount);
}

void Transmission::update(double dt) {
    if (isShifting()) {
        m_shiftTime += dt;
    }

    if (m_gear == -1) {
        m_clutchConstraint.m_minTorque = 0;
        m_clutchConstraint.m_maxTorque = 0;
    }
    else {
        m_clutchConstraint.m_minTorque = -m_maxClutchTorque * m_clutchPressure;
        m_clutchConstraint.m_maxTorque = m_maxClutchTorque * m_clutchPressure;
    }
}

void Transmission::addToSystem(
    atg_scs::RigidBodySystem *system,
    atg_scs::RigidBody *rotatingMass,
    Vehicle *vehicle,
    Engine *engine)
{
    m_rotatingMass = rotatingMass;
    m_vehicle = vehicle;
    m_engine = engine;

    m_clutchConstraint.setBody1(&engine->getOutputCrankshaft()->m_body);
    m_clutchConstraint.setBody2(m_rotatingMass);

    system->addConstraint(&m_clutchConstraint);
}

bool Transmission::getShiftThrottle(double *throttle) const {
    if (m_upshift && m_gear > 0) {
        // Lift off so the revs drop to the new gear
        *throttle = 0.0;
        return true;
    }
    else if (m_downshift) {
        // Blip to raise the revs, unless the new gear would over-rev the engine
        *throttle = wouldOverrev() ? 0.0 : 1.0;
        return true;
    }

    return false;
}

void Transmission::setSmartShifting(bool enabled) {
    if (!enabled && isShifting()) {
        finishShift();
    }

    m_smartShifting = enabled;
}

void Transmission::upshift() {
    // can't shift further
    if (m_gear == m_gearCount - 1) {
        return;
    }

    changeGear(m_gear + 1);

    if (!m_smartShifting) {
        return;
    }

    m_upshift = true;
    m_downshift = false;
    m_shiftTime = 0.0;

    // to actually start moving, clutch needs to be a bit engaged to prevent engine stall
    m_targetClutchPressure = (m_gear == 0) ? LaunchClutchPressure : 0.0;
}

void Transmission::downshift() {
    if (m_gear == -1) {
        return;
    }

    changeGear(m_gear - 1);

    if (!m_smartShifting) {
        return;
    }

    m_upshift = false;
    m_shiftTime = 0.0;

    // neutral, nothing to match
    m_downshift = (m_gear != -1);
    m_targetClutchPressure = m_downshift ? 0.0 : 1.0;
}

void Transmission::finishShift() {
    m_upshift = false;
    m_downshift = false;
    m_targetClutchPressure = 1.0;
}

bool Transmission::wouldOverrev() const {
    return m_targetRPM > units::toRpm(m_engine->getRedline());
}

void Transmission::changeGear(int newGear) {
    if (newGear < -1 || newGear >= m_gearCount) return;
    else if (newGear != -1) {
        const double m_car = m_vehicle->getMass();
        const double gear_ratio = m_gearRatios[newGear];
        const double diff_ratio = m_vehicle->getDiffRatio();
        const double tire_radius = m_vehicle->getTireRadius();
        const double f = tire_radius / (diff_ratio * gear_ratio);

        const double new_I = m_car * f * f;
        const double E_r =
            0.5 * m_rotatingMass->I * m_rotatingMass->v_theta * m_rotatingMass->v_theta;
        const double new_v_theta = m_rotatingMass->v_theta < 0
            ? -std::sqrt(E_r * 2 / new_I)
            : std::sqrt(E_r * 2 / new_I);

        m_rotatingMass->I = new_I;
        m_rotatingMass->p_x = m_rotatingMass->p_y = 0;
        m_rotatingMass->m = m_car;
        m_rotatingMass->v_theta = new_v_theta;
    }

    m_gear = newGear;
    recalculateTargetRPM();
}

void Transmission::recalculateTargetRPM() {
    // Not attached to a vehicle yet (addToSystem() hasn't run)
    if (m_vehicle == nullptr) {
        m_targetRPM = 0.0;
        return;
    }

    double vs = units::convert(m_vehicle->getSpeed(), units::m / units::minute);
    double tr = units::convert(m_vehicle->getTireRadius(), units::m);
    double wheelRPM = (vs / tr) / (constants::pi * 2);
    m_targetRPM = (m_gear != -1) ? (wheelRPM * m_vehicle->getDiffRatio() * m_gearRatios[m_gear]) : 0.0;
}

void Transmission::updateClutchPressure(double clutch_s) {
    m_clutchPressure = m_clutchPressure * (1 - clutch_s) + m_targetClutchPressure * clutch_s;

    recalculateTargetRPM();

    const double rpm = m_engine->getRpm();
    if (m_upshift && m_gear == 0) {
        // Pulling away: let the clutch in gradually
        if (rpm < m_targetRPM || m_shiftTime >= LaunchTime) {
            finishShift();
        }
        else {
            m_targetClutchPressure = LaunchClutchPressure
                + (1.0 - LaunchClutchPressure) * (m_shiftTime / LaunchTime);
        }
    }
    else if (m_upshift) {
        if (rpm < m_targetRPM || m_shiftTime >= MaxShiftTime) {
            finishShift();
        }
    }
    else if (m_downshift) {
        if (rpm > m_targetRPM || m_shiftTime >= MaxShiftTime) {
            finishShift();
        }
        else {
            // When the new gear would over-rev, slip the clutch to bring the
            // revs up gently instead of blipping
            m_targetClutchPressure = wouldOverrev() ? 0.1 : 0.0;
        }
    }
}
