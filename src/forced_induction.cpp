#include "../include/forced_induction.h"

// Ported from kyan0s-git/engine-sim (commits 27ed482d and c1440107), see
// include/forced_induction.h

#include "../include/crankshaft.h"
#include "../include/intake.h"
#include "../include/units.h"
#include "../include/utilities.h"

#include <algorithm>
#include <cmath>

namespace {

    // Reference conditions the flow constants are defined at, and the
    // pressure the exhaust outlets vent to
    constexpr double Reference = units::pressure(1.0, units::atm);
    constexpr double ReferenceTemperature = units::celcius(25.0);
    constexpr double ExhaustAmbient = units::pressure(1.0, units::atm);

    // Air properties for the compressor
    constexpr double Gamma = 1.4;
    constexpr double Cp_mass = 1005.0;                          // [J/(kg K)]
    constexpr double Cp_molar = constants::R * Gamma / (Gamma - 1);
    constexpr double Exponent = (Gamma - 1) / Gamma;

    // Tip speed limit of aluminium compressor wheels [m/s]
    constexpr double MaxTipSpeed = 520.0;

    double approach(double value, double target, double dt, double timeConstant) {
        return value + (target - value) * std::min(1.0, dt / timeConstant);
    }

    GasSystem::Mix airMix() {
        GasSystem::Mix mix;
        mix.p_fuel = 0;
        mix.p_o2 = 0.25;
        mix.p_inert = 0.75;
        return mix;
    }

} /* namespace */

ForcedInduction::ForcedInduction() {
    m_type = Type::Turbocharger;
    m_arrangement = Arrangement::Single;
    m_unitCount = 1;

    m_crankshaft = nullptr;
    for (Intake *&intake : m_intakes) intake = nullptr;
    m_intakeCount = 0;
    m_exhaustSystemCount = 1;

    m_ambientPressure = Reference;
    m_ambientTemperature = ReferenceTemperature;

    m_tipRadius = 0;
    m_compressorK = 0;
    m_compressorEfficiency = 0.75;
    m_maxSpeed = 0;
    m_slip = 0.9;

    m_displacement = 0;
    m_leakK = 0;
    m_bypassK = 0;
    m_bypassValve = false;
    m_bypass = 0;
    m_driveRatio = 1;
    m_driveTorque = 0;
    m_drivePower = 0;

    m_turbineK = 0;
    m_turbineKOpen = 0;
    m_turbineKClosed = 0;
    m_variableGeometry = false;
    m_vanes = 1.0;
    m_soundAfterTurbine = false;
    m_mufflerShare = 1.0;
    m_exhaustMoles = 0;
    m_exhaustFlowRate = 0;
    m_mufflerK = 0;
    m_wastegateK = 0;
    m_turbineEfficiency = 0.7;
    m_inertia = 3e-5;
    m_bearingLoss = 0;
    m_turbineFraction = 0;
    m_sequentialSpeed = 0;
    m_turbineEnergy = 0;

    m_boostTarget = 0;
    m_wastegate = 0;
    m_blowoffEnabled = true;
    m_blowoff = 0;
    m_blowoffK = 0;
    m_blowoffFlowRate = 0;

    m_intercoolerEffectiveness = 0.7;
    m_compressorOutletTemperature = ReferenceTemperature;
    m_blowoffType = BlowoffType::Atmospheric;
    m_boostControl = BoostControl::Electronic;
    m_externalWastegate = false;
    m_twinScroll = false;
    m_whistlePitch = 1.0;
    m_surge = 0;
    m_wastegateIntegral = 0;
    m_lastBoost = 0;
    m_boostRate = 0;
    m_maxCompressorFlow = 0.3;
}

ForcedInduction::~ForcedInduction() {
    /* void */
}

void ForcedInduction::initialize(const Parameters &params) {
    m_type = params.type;
    m_arrangement = isTurbocharger() ? params.arrangement : Arrangement::Single;
    m_unitCount = (m_arrangement == Arrangement::Single) ? 1 : 2;

    const double d = clamp(params.compressorDiameter, 0.02, 0.2);
    m_tipRadius = d / 2;
    m_compressorEfficiency = clamp(params.compressorEfficiency, 0.4, 0.85);
    m_maxSpeed = (params.maxSpeed > 0)
        ? params.maxSpeed
        : MaxTipSpeed / m_tipRadius;

    // The compressor delivers its maximum flow when the source pressure
    // (about 3 bar at the design speed) drops by a quarter across it
    const double compressorFlow = clamp(params.compressorFlow, 0.01, 3.0) / units::AirMolecularMass;
    m_compressorK = GasSystem::flowConstant(
        compressorFlow, 3.0 * Reference, 0.75 * Reference, units::celcius(50.0), Gamma);

    m_displacement = std::max(params.displacement, 0.0);
    m_driveRatio = std::max(params.driveRatio, 0.0);
    m_bypassValve = params.bypassValve;

    // Rotor clearances leak back about 40% (Roots) or 20% (twin screw) of
    // the delivery at 1 bar of boost and 6000 rpm rotor speed
    const double leakFraction = (m_type == Type::Roots) ? 0.4 : 0.2;
    const double pdFlow = Reference * m_displacement * 100.0 / (constants::R * ReferenceTemperature);
    m_leakK = GasSystem::flowConstant(
        std::max(leakFraction * pdFlow, 1e-6), 2.0 * Reference, 1.0 * Reference, units::celcius(60.0), Gamma);
    m_bypassK = GasSystem::flowConstant(
        std::max(pdFlow, 1e-6), Reference, units::pressure(1.5, units::inHg), ReferenceTemperature, Gamma);

    const double ar = clamp(params.turbineAr, 0.3, 1.6);
    // Turbine flow capacity scales with the housing's A/R. Turbines are
    // sized to spool well below the engine's maximum flow (about half the
    // compressor's maximum flow at an expansion ratio of 2.2 for a 0.64 A/R
    // housing); the wastegate bypasses the excess. A turbine with its own
    // wheel size flows with the wheel's area: a 54 mm wheel (the size that
    // goes with the default 60 mm, 0.3 kg/s compressor) flows 0.15 kg/s.
    // A variable geometry turbine has no wastegate, so with its vanes open it
    // has to pass all of the compressor's flow
    double turbineFlowAt064 = (params.variableGeometry ? 1.0 : 0.5) * compressorFlow;
    if (params.turbineDiameter > 0) {
        const double dt = clamp(params.turbineDiameter, 0.02, 0.2);
        turbineFlowAt064 = (0.15 / units::AirMolecularMass) * std::pow(dt / 0.054, 2.0);
    }
    auto turbineK = [&](double a) {
        return GasSystem::flowConstant(
            turbineFlowAt064 * std::pow(a / 0.64, 0.9), 2.2 * Reference, 1.2 * Reference, 1000.0, 1.33);
    };

    m_variableGeometry = params.variableGeometry;
    m_turbineKOpen = turbineK(ar);
    m_turbineKClosed = m_variableGeometry
        ? turbineK((params.minTurbineAr > 0) ? clamp(params.minTurbineAr, 0.1, ar) : 0.25 * ar)
        : m_turbineKOpen;
    m_vanes = 1.0;
    m_turbineK = m_turbineKOpen;
    m_soundAfterTurbine = params.soundAfterTurbine;
    m_mufflerShare = 1.0;
    m_exhaustMoles = 0;
    m_exhaustFlowRate = 0;
    m_mufflerK = 0;
    m_externalWastegate = params.externalWastegate;
    m_wastegateK = (m_externalWastegate ? 6.0 : 4.0) * m_turbineKOpen;
    m_twinScroll = params.twinScroll;
    m_blowoffType = params.blowoffType;
    m_boostControl = params.boostControl;
    m_whistlePitch = clamp(params.whistlePitch, 0.25, 4.0);
    m_maxCompressorFlow = clamp(params.compressorFlow, 0.01, 3.0);
    m_surge = 0;
    m_wastegateIntegral = 0;
    m_lastBoost = 0;
    m_boostRate = 0;
    m_turbineEfficiency = clamp(params.turbineEfficiency, 0.3, 0.85);
    m_inertia = std::max(params.inertia, 1e-6);

    // Bearing and windage losses (P = c * omega^2) scale with the wheel
    // size; ball bearings lose a little over half as much as journal
    // bearings
    m_bearingLoss = (params.ballBearings ? 1.1e-6 : 2.0e-6) * std::pow(d / 0.06, 3.0);
    m_sequentialSpeed = params.sequentialSpeed;

    m_boostTarget = std::max(params.boost, 0.0);
    m_blowoffEnabled = params.blowoffValve && params.blowoffType != BlowoffType::None;
    m_blowoffK = GasSystem::flowConstant(
        0.5 * compressorFlow, 2.0 * Reference, 1.0 * Reference, units::celcius(50.0), Gamma);
    if (isPositiveDisplacement()) {
        m_blowoffK = m_bypassK;
    }

    m_intercoolerEffectiveness = clamp(params.intercoolerEffectiveness, 0.0, 1.0);

    for (Unit &unit : m_units) unit = Unit();
    m_units[1].active = (m_arrangement == Arrangement::Twin);

    // Charge pipes of about 2.5" diameter
    const double area = units::area(32.0, units::cm2);
    const double volume = std::max(params.chargeVolume, 0.5e-3);
    m_charge.initialize(m_ambientPressure, volume, m_ambientTemperature, airMix());
    m_charge.setGeometry(volume / area, std::sqrt(area), 1.0, 0.0);
}

void ForcedInduction::destroy() {
    /* void */
}

void ForcedInduction::addIntake(Intake *intake) {
    if (m_intakeCount >= 16) return;
    m_intakes[m_intakeCount++] = intake;

    if (m_intakeCount == 1) {
        // The compressor draws from the intake's atmosphere, and the charge
        // air carries the intake's mixture (the model meters the fuel at the
        // compressor inlet)
        m_ambientPressure = intake->getAtmospherePressure();
        m_ambientTemperature = intake->getAtmosphereTemperature();
        m_compressorOutletTemperature = m_ambientTemperature;
        m_charge.reset(m_ambientPressure, m_ambientTemperature, intake->getFuelAirMix());
    }
}

std::string ForcedInduction::getDescription() const {
    switch (m_type) {
    case Type::Roots: return "Roots supercharger";
    case Type::TwinScrew: return "Twin-screw supercharger";
    case Type::Centrifugal: return "Centrifugal supercharger";
    case Type::Turbocharger:
    default:
        switch (m_arrangement) {
        case Arrangement::Twin: return m_variableGeometry ? "Twin variable geometry turbochargers" : "Twin turbochargers";
        case Arrangement::Sequential: return m_variableGeometry ? "Sequential variable geometry turbochargers" : "Sequential turbochargers";
        case Arrangement::Single:
        default: return m_variableGeometry ? "Variable geometry turbocharger" : "Turbocharger";
        }
    }
}

double ForcedInduction::getExhaustOutletFlowK(double mufflerK) const {
    if (!isTurbocharger()) return mufflerK;

    double k = m_wastegateK * m_wastegate;
    for (int i = 0; i < m_unitCount; ++i) {
        if (m_units[i].active) k += m_turbineK;
    }

    // Each exhaust system feeds its share of the turbines; the turbine and
    // the muffler are restrictions in series
    k /= std::max(m_exhaustSystemCount, 1);
    if (k <= 0) return 0;
    if (mufflerK <= 0) return k;

    return 1.0 / std::sqrt(1.0 / (k * k) + 1.0 / (mufflerK * mufflerK));
}

void ForcedInduction::addExhaustFlow(double n, double T, double P, double hcr, double mufflerK) {
    if (!isTurbocharger() || n <= 0 || P <= ExhaustAmbient) return;

    double turbineK = 0;
    for (int i = 0; i < m_unitCount; ++i) {
        if (m_units[i].active) turbineK += m_turbineK;
    }

    const double totalK = turbineK + m_wastegateK * m_wastegate;
    if (totalK <= 0) return;

    m_exhaustMoles += n;
    m_mufflerK = mufflerK;

    const double share = totalK / std::max(m_exhaustSystemCount, 1);
    m_turbineFraction = turbineK / totalK;

    // Pressure drop across the turbine (restrictions in series share the
    // drop in proportion to 1 / k^2)
    double turbineDropFraction = 1.0;
    if (mufflerK > 0) {
        const double rt = 1.0 / (share * share);
        const double rm = 1.0 / (mufflerK * mufflerK);
        turbineDropFraction = rt / (rt + rm);
    }

    // What's left of the pressure (and its pulses) after the turbine and
    // wastegate, for the muffler: the sound after the turbine
    m_mufflerShare = 1.0 - turbineDropFraction;

    const double P_out = P - (P - ExhaustAmbient) * turbineDropFraction;
    const double g = std::max(hcr, 1.05);
    const double cp = constants::R * g / (g - 1);
    const double isentropicWork = cp * T * (1.0 - std::pow(P_out / P, (g - 1) / g));

    // A twin-scroll housing keeps the exhaust pulses of the cylinder groups
    // apart, which helps most at low flow
    double efficiency = m_turbineEfficiency;
    if (m_twinScroll) efficiency *= 1.0 + 0.12 * clamp(1.0 - m_turbineFraction * 0.5, 0.0, 1.0);

    // Nearly closed vanes lose some efficiency to the gas turning through
    // their narrow passages
    if (m_variableGeometry) efficiency *= 0.85 + 0.15 * std::min(1.0, 2.0 * m_vanes);

    m_turbineEnergy += efficiency * m_turbineFraction * n * isentropicWork;
}

double ForcedInduction::centrifugalSourcePressureRatio(double omega) const {
    const double U = omega * m_tipRadius;
    const double pr = std::pow(
        1.0 + m_compressorEfficiency * m_slip * U * U / (Cp_mass * m_ambientTemperature),
        1.0 / Exponent);

    return std::min(pr, 6.0);
}

double ForcedInduction::plenumPressure() const {
    if (m_intakeCount == 0) return m_ambientPressure;

    double sum = 0;
    for (int i = 0; i < m_intakeCount; ++i) {
        sum += m_intakes[i]->m_system.pressure();
    }

    return sum / m_intakeCount;
}

double ForcedInduction::crankSpeed() const {
    return (m_crankshaft != nullptr) ? std::abs(m_crankshaft->m_body.v_theta) : 0.0;
}

GasSystem::Mix ForcedInduction::chargeMix() const {
    return (m_intakeCount > 0)
        ? m_intakes[0]->getFuelAirMix()
        : airMix();
}

void ForcedInduction::processCentrifugal(
    Unit &unit,
    double dt,
    double scale,
    const GasSystem::Mix &mix)
{
    const double pr0 = centrifugalSourcePressureRatio(unit.omega);
    const double pr = std::max(m_charge.pressure() / m_ambientPressure, 1.0);

    // Compressor outlet temperature, then the intercooler
    const double T2 = m_ambientTemperature
        * (1.0 + (std::pow(pr, Exponent) - 1.0) / m_compressorEfficiency);
    m_compressorOutletTemperature = T2;
    const double T_in = T2 - m_intercoolerEffectiveness * (T2 - m_ambientTemperature);

    const double flow = m_charge.flow(scale * m_compressorK, dt, pr0 * m_ambientPressure, T_in, mix);
    const double dn = -flow;   // moles into the charge air

    unit.compressorFlow = dn / dt;

    // Reverse flow through the compressor: surge (the flutter heard without
    // a blow-off valve)
    const double reverse = std::max(-dn / dt * units::AirMolecularMass / (0.15 * m_maxCompressorFlow), 0.0);
    m_surge = m_surge + (std::min(reverse, 1.0) - m_surge) * std::min(1.0, dt / 0.05);

    // Euler work of the impeller; reverse flow (surge) brakes the wheel too
    const double U = unit.omega * m_tipRadius;
    const double massFlow = std::abs(dn) / dt * units::AirMolecularMass;
    unit.compressorPower = massFlow * m_slip * U * U;
}

void ForcedInduction::processPositiveDisplacement(double dt, const GasSystem::Mix &mix) {
    const double omega = crankSpeed() * m_driveRatio;
    m_units[0].omega = omega;

    const double pr = std::max(m_charge.pressure() / m_ambientPressure, 1.0);
    const double efficiency = (m_type == Type::Roots) ? 0.55 : 0.72;
    const double T2 = m_ambientTemperature
        * (1.0 + (std::pow(pr, Exponent) - 1.0) / efficiency);
    m_compressorOutletTemperature = T2;
    const double T_in = T2 - m_intercoolerEffectiveness * (T2 - m_ambientTemperature);

    // Displacement delivered this step
    const double volumeFlow = m_displacement * omega / (2 * constants::pi);
    const double n = 0.95 * m_ambientPressure * volumeFlow * dt / (constants::R * m_ambientTemperature);
    if (n > 0) {
        m_charge.inject(n, T_in, mix);
    }

    // Leakage back to the inlet
    const double leak = m_charge.flow(m_leakK, dt, m_ambientPressure, m_ambientTemperature, mix);

    const double netFlow = (n - leak) / dt;
    m_units[0].compressorFlow = netFlow;

    // Drive power: the work to heat the delivered air plus 10% mechanical
    // losses
    m_units[0].compressorPower = (n / dt) * Cp_molar * (T2 - m_ambientTemperature) / 0.9;
}

void ForcedInduction::process(double dt) {
    if (dt <= 0) return;

    const GasSystem::Mix mix = chargeMix();

    const double p_plenum = plenumPressure();
    const double boost = m_charge.pressure() - m_ambientPressure;

    if (isTurbocharger()) {
        // Sequential turbochargers: the second unit joins above the
        // transition speed (with some hysteresis)
        if (m_arrangement == Arrangement::Sequential) {
            const double speed = crankSpeed();
            if (!m_units[1].active && speed > m_sequentialSpeed) m_units[1].active = true;
            else if (m_units[1].active && speed < 0.9 * m_sequentialSpeed) m_units[1].active = false;
        }

        int activeTurbines = 0;
        for (int i = 0; i < m_unitCount; ++i) {
            if (m_units[i].active) ++activeTurbines;
        }

        m_boostRate += ((boost - m_lastBoost) / dt - m_boostRate) * std::min(1.0, dt / 0.02);
        m_lastBoost = boost;

        // Wastegate. Electronic control: proportional-integral on the boost
        // error, holding the target. It sets the share of the exhaust that
        // bypasses the turbine, which the boost follows evenly, and turns it
        // into a valve opening: near closed, a small opening bypasses a lot,
        // and controlling the opening directly made the boost hunt there.
        // The proportional band is half the target, the integral only acts
        // within it (so it doesn't wind up while the turbo spools), and the
        // valve starts to open as the boost rises quickly towards the target,
        // which keeps the boost from overshooting after a snap. Spring
        // actuator: opens from 75% of the set point and only fully at 120%,
        // slowly, so boost overshoots and creeps up with the flow.
        //
        // Variable geometry: the vanes, with no wastegate. The control sets
        // the pressure the exhaust manifold should run at, which the boost
        // follows evenly at any engine speed, and the vanes open as far as
        // passes the exhaust flow at that pressure, so they open with the
        // flow on their own, as a real controller's feed-forward does.
        // Controlling the vane opening directly made the boost hunt: a
        // diesel's fuel follows its air, so more boost brings more exhaust
        // and more boost again, and nearly closed vanes move the boost a
        // lot. A spring actuator moves the vanes as it moves a wastegate.
        m_exhaustFlowRate += (m_exhaustMoles / dt - m_exhaustFlowRate) * std::min(1.0, dt / 0.05);
        m_exhaustMoles = 0;

        double wastegateTarget = 1.0;
        if (m_boostTarget > 0 && m_variableGeometry && m_boostControl == BoostControl::Electronic) {
            const double band = std::max(m_boostTarget, units::pressure(0.5, units::psi));
            const double error = (boost - m_boostTarget) / band;
            if (std::abs(error) < 0.3) {
                m_wastegateIntegral = clamp(m_wastegateIntegral - 1.5 * error * dt, -1.0, 1.0);
            }

            // The exhaust manifold's pressure over the outside's: about 1.2
            // times the boost's absolute pressure holds it, and the control
            // trims that (up while the boost is short, down as it rises
            // quickly, and the integral near the target). Each exhaust
            // system's turbine share and muffler in series pass its share of
            // the flow at it.
            const double feedForward = 1.2 * (m_ambientPressure + m_boostTarget) / ExhaustAmbient;
            const double trim = -1.5 * error - 0.4 * m_boostRate / band + m_wastegateIntegral;
            const double ratio = clamp(feedForward * std::exp(trim), 1.05, 4.0);
            const int systems = std::max(m_exhaustSystemCount, 1);
            const double series = (m_exhaustFlowRate / systems)
                * GasSystem::flowConstant(1.0, ratio * ExhaustAmbient, (ratio - 1) * ExhaustAmbient, 1000.0, 1.33);
            double turbine = m_turbineKOpen;
            if (m_mufflerK <= 0) turbine = series;
            else if (series < m_mufflerK) turbine = 1.0 / std::sqrt(1.0 / (series * series) - 1.0 / (m_mufflerK * m_mufflerK));
            const double wanted = turbine * systems / std::max(activeTurbines, 1);
            wastegateTarget = (wanted <= m_turbineKClosed)
                ? 0.0
                : clamp(std::log(wanted / m_turbineKClosed) / std::log(m_turbineKOpen / m_turbineKClosed), 0.0, 1.0);
        }
        else if (m_boostTarget > 0) {
            if (m_boostControl == BoostControl::Electronic) {
                const double band = std::max(0.5 * m_boostTarget, units::pressure(0.5, units::psi));
                const double error = (boost - m_boostTarget) / band;
                double bypass = 1.0 + error;
                if (std::abs(error) < 1.0) {
                    m_wastegateIntegral = clamp(m_wastegateIntegral + 2.0 * error * dt, -1.0, 1.0);
                    if (m_boostRate > 0) bypass += 0.05 * m_boostRate / band;
                }
                bypass = clamp(bypass + m_wastegateIntegral, 0.0, 1.0);

                // Opening w bypasses ratio * w / (1 + ratio * w) of the flow
                const double ratio = m_wastegateK / std::max(activeTurbines * m_turbineK, 1e-12);
                const double share = bypass * ratio / (1.0 + ratio);
                wastegateTarget = clamp(share / ((1.0 - share) * ratio), 0.0, 1.0);
            }
            else {
                wastegateTarget = clamp((boost - 0.75 * m_boostTarget) / (0.45 * m_boostTarget), 0.0, 1.0);
            }
        }

        if (m_variableGeometry) {
            // The vane actuator is a little slower than a wastegate's. Each
            // step of the vanes changes the turbine's flow by the same ratio:
            // nearly closed, the boost reacts strongly to a small change in
            // flow, and spacing them evenly made the boost hunt there.
            m_vanes = approach(m_vanes, wastegateTarget, dt, 0.12);
            m_turbineK = m_turbineKClosed * std::pow(m_turbineKOpen / m_turbineKClosed, m_vanes);
            m_wastegate = 0;
        }
        else {
            m_wastegate = approach(m_wastegate, wastegateTarget, dt,
                (m_boostControl == BoostControl::Electronic) ? 0.03 : 0.12);
        }

        for (int i = 0; i < m_unitCount; ++i) {
            Unit &unit = m_units[i];
            unit.turbinePower = 0;
            unit.compressorFlow = 0;
            unit.compressorPower = 0;

            if (unit.active) {
                processCentrifugal(unit, dt, 1.0, mix);
                const double turbineEnergy = m_turbineEnergy / std::max(activeTurbines, 1);
                unit.turbinePower = turbineEnergy / dt;
                unit.energy += turbineEnergy;
            }

            unit.energy -= (unit.compressorPower + m_bearingLoss * unit.omega * unit.omega) * dt;

            // Overspeed: the wastegate can't stop a badly mismatched turbo
            // but the wheel is limited to its maximum speed
            const double maxEnergy = 0.5 * m_inertia * m_maxSpeed * m_maxSpeed;
            unit.energy = clamp(unit.energy, 0.0, maxEnergy);
            unit.omega = std::sqrt(2 * unit.energy / m_inertia);
        }

        m_turbineEnergy = 0;
        m_drivePower = 0;
        m_driveTorque = 0;
    }
    else {
        if (m_type == Type::Centrifugal) {
            Unit &unit = m_units[0];
            unit.omega = std::min(crankSpeed() * m_driveRatio, m_maxSpeed);
            processCentrifugal(unit, dt, 1.0, mix);
            unit.compressorPower += m_bearingLoss * unit.omega * unit.omega;
        }
        else {
            processPositiveDisplacement(dt, mix);

            // Bypass valve: recirculates the delivery at part throttle
            if (m_bypassValve) {
                const double target =
                    clamp((0.9 * m_ambientPressure - p_plenum) / (0.2 * m_ambientPressure), 0.0, 1.0);
                m_bypass = approach(m_bypass, target, dt, 0.05);
                if (m_bypass > 0) {
                    m_charge.flow(m_bypassK * m_bypass, dt, m_ambientPressure, m_ambientTemperature, mix);
                }
            }
        }

        m_drivePower = m_units[0].compressorPower;
        m_driveTorque = m_drivePower / std::max(crankSpeed(), 20.0);
    }

    // Blow-off valve: vents the charge when the throttle snaps shut
    double blowoffTarget = 0;
    if (m_blowoffEnabled && !isPositiveDisplacement()) {
        const double dp = m_charge.pressure() - p_plenum;
        blowoffTarget = clamp((dp - units::pressure(4.0, units::psi)) / units::pressure(3.0, units::psi), 0.0, 1.0);
    }

    m_blowoff = approach(m_blowoff, blowoffTarget, dt, 0.01);
    double vented = 0;
    if (m_blowoff > 0) {
        vented = m_charge.flow(m_blowoffK * m_blowoff, dt, m_ambientPressure, m_ambientTemperature, mix);
    }

    m_blowoffFlowRate = approach(m_blowoffFlowRate, std::max(vented, 0.0) / dt, dt, 0.02);

    m_charge.dissipateExcessVelocity();
    m_charge.updateVelocity(dt, 0.5);
}

void ForcedInduction::apply(atg_scs::SystemState *system) {
    if (m_crankshaft == nullptr || m_driveTorque == 0) return;

    const int index = m_crankshaft->m_body.index;
    const double v = system->v_theta[index];
    if (v > 0) system->t[index] -= m_driveTorque;
    else if (v < 0) system->t[index] += m_driveTorque;
}

double ForcedInduction::getBoost() const {
    return m_charge.pressure() - m_ambientPressure;
}

double ForcedInduction::getShaftSpeed() const {
    return m_units[0].omega;
}

double ForcedInduction::getCompressorFlow() const {
    double flow = 0;
    for (int i = 0; i < m_unitCount; ++i) flow += m_units[i].compressorFlow;
    return flow;
}

double ForcedInduction::getTurbinePower() const {
    double power = 0;
    for (int i = 0; i < m_unitCount; ++i) power += m_units[i].turbinePower;
    return power;
}

void ForcedInduction::setIntercoolerEffectiveness(double e) {
    m_intercoolerEffectiveness = clamp(e, 0.0, 1.0);
}

double ForcedInduction::getWhineFrequency() const {
    // Shaft order of the compressor; the synthesizer turns it into the
    // whistle or the rotor lobe frequency
    return m_units[0].omega / (2 * constants::pi);
}

double ForcedInduction::getWhineLevel() const {
    if (isPositiveDisplacement()) {
        // Supercharger whine follows rotor speed and load
        const double speed = clamp(m_units[0].omega / (2 * constants::pi * 250.0), 0.0, 1.0);
        const double load = clamp(getBoost() / units::pressure(10.0, units::psi), 0.0, 1.0);
        return speed * (0.3 + 0.7 * load);
    }

    const double s = clamp(m_units[0].omega / m_maxSpeed, 0.0, 1.0);
    const double flow = clamp(
        std::abs(getCompressorFlow()) * units::AirMolecularMass / 0.3, 0.0, 1.0);
    return s * s * (0.25 + 0.75 * flow);
}

double ForcedInduction::getFlowLevel() const {
    return clamp(std::abs(getCompressorFlow()) * units::AirMolecularMass / std::max(m_maxCompressorFlow, 0.01), 0.0, 1.0);
}

double ForcedInduction::getWastegateNoise() const {
    if (!m_externalWastegate || !isTurbocharger()) return 0.0;
    return clamp(m_wastegate * (1.0 - m_turbineFraction) * 2.0, 0.0, 1.0);
}

double ForcedInduction::getBlowoffLevel() const {
    // The hiss is the jet of charge air leaving the valve, so it follows the
    // pressure driving it as well as the flow: a lift from full boost gives
    // a sharp hiss, while the little air the compressor pushes through the
    // open valve when cruising (with the charge at about ambient pressure)
    // is nearly silent
    const double flow = clamp(m_blowoffFlowRate * units::AirMolecularMass / 0.15, 0.0, 1.0);
    const double drive = clamp(getBoost() / units::pressure(5.0, units::psi), 0.0, 1.0);
    return flow * drive;
}

int ForcedInduction::getSoundMode() const {
    int type = 1;
    switch (m_type) {
    case Type::Roots: type = 2; break;
    case Type::TwinScrew: type = 3; break;
    case Type::Centrifugal: type = 4; break;
    default: break;
    }

    return 10 * type + static_cast<int>(getBlowoffType());
}
