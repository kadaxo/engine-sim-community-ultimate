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

    // The gas the turbine works with. The simulated exhaust of a spark-fired
    // engine reaches the outlets at 2000 to 2900 K at full load, where a real
    // one reaches the turbine at no more than about 1300 K: the engine loses
    // heat to the cylinder walls, ports and manifold, and its control adds
    // fuel to hold the turbine there. Hotter gas carries more energy per mole
    // and passes the turbine more slowly, so a turbine without a wastegate
    // ran away. Diesel exhaust (about 850 K) is left as it is.
    double turbineInletTemperature(double T) {
        constexpr double Limit = 1300.0;
        return T / std::pow(1.0 + std::pow(T / Limit, 8.0), 1.0 / 8.0);
    }

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
    m_throttleBeforeBlower = false;
    m_bypass = 0;
    m_driveRatio = 1;
    m_driveTorque = 0;
    m_drivePower = 0;

    m_turbineK = 0;
    m_turbineKOpen = 0;
    m_turbineKClosed = 0;
    m_variableGeometry = false;
    m_vanes = 1.0;
    m_soundAfterTurbine = true;
    m_mufflerShare = 1.0;
    m_exhaustMoles = 0;
    m_exhaustFlowRate = 0;
    m_exhaustHeat = 0;
    m_exhaustTemperature = 1000.0;
    m_inletScale = 1.0;
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
    m_rotorLobes = 3;
    m_rotorEfficiency = 0.55;
    m_rotorTwist = 0;
    m_gearTeeth = 0;
    m_gearWhine = 0;
    m_whineVolume = 1.0;
    m_compressorBlades = 6;
    m_splitterBlades = 6;
    m_compressorInlet = CompressorInlet::Silenced;
    m_wastegateOpenOffBoost = false;
    m_antiLag = false;
    m_antiLagActive = false;
    m_antiLagTiming = 0;
    m_antiLagThrottle = 0;
    m_antiLagMinSpeed = 0;
    m_ballBearings = false;
    m_bearingWear = 0;
    m_wastegateRattle = 0;
    m_leakDiameter = 0;
    m_boostLeakK = 0;
    m_leakFlowRate = 0;
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

    // A centrifugal supercharger's impeller is larger than a car turbo's
    // (a ProCharger P-1SC's exducer is 133 mm)
    const double diameter = (params.compressorDiameter > 0)
        ? params.compressorDiameter
        : ((params.type == Type::Centrifugal) ? 0.133 : 0.09);
    const double d = clamp(diameter, 0.02, 0.2);
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
    // A centrifugal supercharger's impeller turns about ten times as fast as
    // the crank (a ProCharger P-1SC steps up 4.1:1 inside, and the belt
    // overdrives it about 2.5:1), a Roots or twin-screw's rotors about twice
    m_driveRatio = (params.driveRatio > 0)
        ? params.driveRatio
        : ((m_type == Type::Centrifugal) ? 10.0 : 2.0);
    m_rotorEfficiency = (params.rotorEfficiency > 0)
        ? clamp(params.rotorEfficiency, 0.3, 0.9)
        : ((m_type == Type::Roots) ? 0.55 : 0.72);
    m_bypassValve = params.bypassValve;
    m_throttleBeforeBlower = params.throttleBeforeBlower && isPositiveDisplacement();

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
    m_exhaustHeat = 0;
    m_exhaustTemperature = 1000.0;
    m_inletScale = 1.0;
    m_mufflerK = 0;
    m_externalWastegate = params.externalWastegate;
    m_wastegateK = (m_externalWastegate ? 6.0 : 4.0) * m_turbineKOpen;
    m_twinScroll = params.twinScroll;
    m_blowoffType = params.blowoffType;
    m_boostControl = params.boostControl;
    m_whistlePitch = clamp(params.whistlePitch, 0.25, 4.0);
    m_rotorLobes = (params.rotorLobes > 0)
        ? std::clamp(params.rotorLobes, 2, 8)
        : 3;
    m_rotorTwist = clamp(params.rotorTwist, 0.0, constants::pi);
    m_gearTeeth = std::clamp(params.gearTeeth, 0, 200);
    m_gearWhine = std::max(params.gearWhine, 0.0);
    m_whineVolume = isTurbocharger() ? 1.0 : clamp(params.whineVolume, 0.0, 4.0);
    m_compressorBlades = std::clamp(params.compressorBlades, 3, 20);
    m_splitterBlades = std::clamp(params.splitterBlades, 0, 20);
    m_compressorInlet = params.compressorInlet;
    m_maxCompressorFlow = clamp(params.compressorFlow, 0.01, 3.0);
    m_wastegateOpenOffBoost = params.wastegateOpenOffBoost;
    m_antiLag = params.antiLag && isTurbocharger();
    m_antiLagActive = false;
    m_antiLagTiming = clamp(params.antiLagTiming, -constants::pi / 3, constants::pi / 6);
    m_antiLagThrottle = clamp(params.antiLagThrottle, 0.0, 1.0);
    m_antiLagMinSpeed = std::max(params.antiLagMinSpeed, 0.0);
    m_ballBearings = params.ballBearings;
    m_bearingWear = clamp(params.bearingWear, 0.0, 1.0);
    m_wastegateRattle = clamp(params.wastegateRattle, 0.0, 1.0);
    m_leakDiameter = clamp(params.boostLeak, 0.0, 0.1);
    m_leakFlowRate = 0;

    // A leak is a sharp-edged hole (discharge coefficient 0.65)
    m_boostLeakK = 0;
    if (m_leakDiameter > 0) {
        const double area = constants::pi * m_leakDiameter * m_leakDiameter / 4;
        const double dp = 10.0e3;
        const double rho = (Reference + dp) * units::AirMolecularMass / (constants::R * ReferenceTemperature);
        const double flow = 0.65 * area * std::sqrt(2 * rho * dp) / units::AirMolecularMass;
        m_boostLeakK = GasSystem::flowConstant(flow, Reference + dp, dp, ReferenceTemperature, Gamma);
    }
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

    // The space between the throttle and the rotors: the hat and the
    // blower's inlet, about half a turn of displacement
    const double inletVolume = std::max(0.5 * m_displacement, 1.0e-3);
    m_inlet.initialize(m_ambientPressure, inletVolume, m_ambientTemperature, airMix());
    m_inlet.setGeometry(inletVolume / area, std::sqrt(area), 1.0, 0.0);
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
        m_inlet.reset(m_ambientPressure, m_ambientTemperature, intake->getFuelAirMix());
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
    k *= m_inletScale / std::max(m_exhaustSystemCount, 1);
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
    m_exhaustHeat += n * T;
    m_mufflerK = mufflerK;

    const double share = totalK * m_inletScale / std::max(m_exhaustSystemCount, 1);
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
    const double isentropicWork = cp * turbineInletTemperature(T) * (1.0 - std::pow(P_out / P, (g - 1) / g));

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
    if (m_throttleBeforeBlower) {
        processThrottledBlower(dt);
        return;
    }

    const double omega = crankSpeed() * m_driveRatio;
    m_units[0].omega = omega;

    const double pr = std::max(m_charge.pressure() / m_ambientPressure, 1.0);
    const double efficiency = m_rotorEfficiency;
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

void ForcedInduction::processThrottledBlower(double dt) {
    const double omega = crankSpeed() * m_driveRatio;
    m_units[0].omega = omega;

    // The rotors take their displacement at the inlet's density, which the
    // throttles set: at part throttle the inlet is below the outside's
    // pressure and the blower moves less air
    const double p_in = m_inlet.pressure();
    const double T_inlet = m_inlet.temperature();
    const double pr = std::max(m_charge.pressure() / std::max(p_in, 1.0), 1.0);
    const double T2 = T_inlet * (1.0 + (std::pow(pr, Exponent) - 1.0) / m_rotorEfficiency);
    m_compressorOutletTemperature = T2;
    const double T_out = T2 - m_intercoolerEffectiveness * (T2 - m_ambientTemperature);

    const double volumeFlow = m_displacement * omega / (2 * constants::pi);
    double n = 0.95 * p_in * volumeFlow * dt / (constants::R * T_inlet);
    n = clamp(n, 0.0, 0.5 * m_inlet.n());
    if (n > 0) {
        const GasSystem::Mix inletMix = m_inlet.mix();
        m_inlet.loseN(n, m_inlet.kineticEnergyPerMol());
        m_charge.inject(n, T_out, inletMix);
    }

    // Leakage back through the clearances, into the inlet
    const double leak = flowToInlet(m_leakK, dt);

    m_units[0].compressorFlow = (n - leak) / dt;
    m_units[0].compressorPower = (n / dt) * Cp_molar * (T2 - T_inlet) / 0.9;

    m_inlet.dissipateExcessVelocity();
    m_inlet.updateVelocity(dt, 0.5);
}

double ForcedInduction::flowToInlet(double k, double dt) {
    // The air returns at the inlet's temperature: the heat of leaking and
    // backflowing through the rotors is already in the rotor efficiency
    // (with the throttle after the blower this air goes back to the
    // outside, and its heat with it). Recompressing it with its heat
    // counted the heat twice: the charge ran 120 K hotter.
    const double p_in = m_inlet.pressure();
    const double T_in = m_inlet.temperature();
    const GasSystem::Mix inletMix = m_inlet.mix();
    const GasSystem::Mix chargeMixture = m_charge.mix();
    const double n = m_charge.flow(k, dt, p_in, T_in, inletMix);
    if (n > 0) {
        m_inlet.inject(n, T_in, chargeMixture);
    }
    else if (n < 0) {
        m_inlet.loseN(std::min(-n, m_inlet.n()), m_inlet.kineticEnergyPerMol());
    }

    return n;
}

double ForcedInduction::rotorPressureRatio() const {
    const double p_in = m_throttleBeforeBlower ? m_inlet.pressure() : m_ambientPressure;
    return m_charge.pressure() / std::max(p_in, 1.0);
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
        // The exhaust's temperature at the outlets, for the turbine's inlet
        // (see turbineInletTemperature())
        m_exhaustFlowRate += (m_exhaustMoles / dt - m_exhaustFlowRate) * std::min(1.0, dt / 0.05);
        if (m_exhaustMoles > 0) {
            m_exhaustTemperature += (m_exhaustHeat / m_exhaustMoles - m_exhaustTemperature) * std::min(1.0, dt / 0.05);
        }
        m_exhaustMoles = 0;
        m_exhaustHeat = 0;
        m_inletScale = std::sqrt(m_exhaustTemperature / turbineInletTemperature(m_exhaustTemperature));

        double wastegateTarget = 1.0;
        if (m_boostTarget > 0 && m_variableGeometry && m_boostControl == BoostControl::Electronic) {
            const double band = std::max(m_boostTarget, units::pressure(0.5, units::psi));
            const double error = (boost - m_boostTarget) / band;
            if (error > -0.3) {
                m_wastegateIntegral = clamp(m_wastegateIntegral - 1.5 * error * dt, -3.0, 1.0);
            }

            // The exhaust manifold's pressure over the outside's: about 1.2
            // times the boost's absolute pressure holds it on a diesel. Hotter
            // gas does the same work with a smaller expansion: the share of
            // its heat the turbine takes falls with the cube of a diesel's
            // inlet temperature over its own. That is fitted, not derived:
            // the plain ratio left the petrol engines overshooting by about
            // 8 psi after a snap and the square by 5, the cube by 1 to 2.5,
            // still settling on the set point. The control
            // trims the pressure drop (up while the boost is short, down as it
            // rises quickly, and the integral near the target and above it);
            // trimming the ratio made the turbine's opening swing tenfold for
            // the small drops a petrol engine needs, and the boost hunted.
            // Each exhaust system's turbine share and muffler in series pass
            // its share of the flow at it.
            constexpr double Expansion = (1.33 - 1) / 1.33;
            const double diesel = 1.2 * (m_ambientPressure + m_boostTarget) / ExhaustAmbient;
            const double work = (1.0 - std::pow(diesel, -Expansion))
                * std::pow(850.0 / std::max(850.0, turbineInletTemperature(m_exhaustTemperature)), 3.0);
            const double feedForward = std::pow(1.0 - work, -1.0 / Expansion);
            const double trim = -1.5 * error - 0.4 * m_boostRate / band + m_wastegateIntegral;
            const double ratio = clamp(1.0 + (feedForward - 1.0) * std::exp(trim), 1.02, 4.0);
            const int systems = std::max(m_exhaustSystemCount, 1);
            const double series = (m_exhaustFlowRate / systems)
                * GasSystem::flowConstant(1.0, ratio * ExhaustAmbient, (ratio - 1) * ExhaustAmbient, 1000.0, 1.33);
            double turbine = m_turbineKOpen * m_inletScale;
            if (m_mufflerK <= 0) turbine = series;
            else if (series < m_mufflerK) turbine = 1.0 / std::sqrt(1.0 / (series * series) - 1.0 / (m_mufflerK * m_mufflerK));
            const double wanted = turbine / m_inletScale * systems / std::max(activeTurbines, 1);
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

                // Off boost (part throttle, idle, overrun) an electric
                // actuator can hold it wide open, which lowers the pressure
                // ahead of the turbine; it shuts as the throttle opens
                if (m_wastegateOpenOffBoost && p_plenum < 0.9 * m_ambientPressure) wastegateTarget = 1.0;
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

            // Bypass valve: recirculates the delivery at part throttle. It
            // opens with manifold vacuum, and also when the throttle holds
            // back more than a little of the charge pressure: on vacuum alone
            // it stayed shut with the manifold near atmospheric, and the
            // blower dead-headed against a part-open throttle (the LS made
            // 8 psi at a third throttle and 3.4 at full, the E55 28 psi).
            // From kyan0s-git/engine-sim 88c83a8, which opened it from 0.1 atm
            // across the throttle; from 0.03 atm here, as at half throttle the
            // LS still made more boost than at full (the plate costs about
            // 0.01 atm when it's wide open).
            if (m_bypassValve) {
                const double vacuum =
                    (0.9 * m_ambientPressure - p_plenum) / (0.2 * m_ambientPressure);
                const double restricted =
                    (m_charge.pressure() - p_plenum - 0.03 * m_ambientPressure) / (0.2 * m_ambientPressure);
                const double target = clamp(std::max(vacuum, restricted), 0.0, 1.0);
                m_bypass = approach(m_bypass, target, dt, 0.05);
                if (m_bypass > 0) {
                    // Back to the blower's inlet, which is outside air
                    // unless the throttle is before the blower
                    if (m_throttleBeforeBlower) flowToInlet(m_bypassK * m_bypass, dt);
                    else m_charge.flow(m_bypassK * m_bypass, dt, m_ambientPressure, m_ambientTemperature, mix);
                }
            }
        }

        m_drivePower = m_units[0].compressorPower;
        m_driveTorque = m_drivePower / std::max(crankSpeed(), 20.0);
    }

    // Blow-off valve: vents the charge when the throttle snaps shut
    double blowoffTarget = 0;
    // (an anti-lag system keeps it shut to hold the boost it makes)
    if (m_blowoffEnabled && !isPositiveDisplacement() && !m_antiLagActive) {
        const double dp = m_charge.pressure() - p_plenum;
        blowoffTarget = clamp((dp - units::pressure(4.0, units::psi)) / units::pressure(3.0, units::psi), 0.0, 1.0);
    }

    m_blowoff = approach(m_blowoff, blowoffTarget, dt, 0.01);
    double vented = 0;
    if (m_blowoff > 0) {
        vented = m_charge.flow(m_blowoffK * m_blowoff, dt, m_ambientPressure, m_ambientTemperature, mix);
    }

    m_blowoffFlowRate = approach(m_blowoffFlowRate, std::max(vented, 0.0) / dt, dt, 0.02);

    // A leak in the charge pipe vents whenever the charge is above the
    // outside's pressure (and draws in below it)
    double leaked = 0;
    if (m_boostLeakK > 0) {
        leaked = m_charge.flow(m_boostLeakK, dt, m_ambientPressure, m_ambientTemperature, mix);
    }
    m_leakFlowRate = approach(m_leakFlowRate, leaked / dt, dt, 0.02);

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
        // Supercharger whine is mostly the delivered air rushing back into
        // each rotor pocket as it opens to the outlet, so it grows with the
        // pressure ratio across the rotors, and faster than the rotor speed.
        // With the bypass open (idle, cruise, lifting off) there is little
        // pressure across them and the blower is much quieter.
        const double speed = clamp(m_units[0].omega / (2 * constants::pi * 250.0), 0.0, 1.3);
        const double pr = rotorPressureRatio();
        const double load = clamp((pr - 1.0) / 0.7, 0.0, 1.0);
        return std::pow(speed, 1.5) * (0.2 + 0.8 * load);
    }

    // Centrifugal compressors whistle instead, see getCompressorTones()
    return 0.0;
}

double ForcedInduction::getPulsesPerTurn() const {
    // Both of a Roots blower's rotors open their pockets to the outlet; a
    // twin-screw's male rotor sets its pulses (three lobes against the
    // female's five on a Whipple)
    return (m_type == Type::Roots) ? 2.0 * m_rotorLobes : m_rotorLobes;
}

double ForcedInduction::getWhineHarmonicRatio() const {
    // Fitted, not derived: 0.55 at the 60 degree twist of three-lobe Eaton
    // and GMC rotors (the harmonics this whine always had), 0.2 at a TVS's
    // 160 degrees, 0.76 for straight lobes
    const double degrees = m_rotorTwist * 180 / constants::pi;
    return clamp(0.76 - 0.0035 * degrees, 0.1, 0.9);
}

double ForcedInduction::getGearFrequency() const {
    return m_gearTeeth * m_units[0].omega / (2 * constants::pi);
}

double ForcedInduction::getGearLevel() const {
    if (m_gearTeeth <= 0 || isTurbocharger()) return 0.0;

    // Gear whine comes from the teeth's transmission error under the drive's
    // load
    const double reference = isPositiveDisplacement() ? 2 * constants::pi * 250.0 : m_maxSpeed;
    const double speed = clamp(m_units[0].omega / std::max(reference, 1.0), 0.0, 1.3);
    const double pr = isPositiveDisplacement() ? rotorPressureRatio() : m_charge.pressure() / m_ambientPressure;
    const double load = clamp((pr - 1.0) / 0.7, 0.0, 1.0);
    return 0.5 * m_gearWhine * std::pow(speed, 1.5) * (0.4 + 0.6 * load);
}

// The sound of a centrifugal compressor is mostly tones locked to its shaft
// (Raitor and Neise, Sound generation in centrifugal compressors, JSV 2008):
//  - once per revolution, from the wheel's residual imbalance and
//    asymmetry: the whistle usually heard from a car, at 1.2 to 4.5 kHz.
//    For a given balance grade the imbalance force goes with D^2 U, and the
//    small housing radiates it in proportion to the frequency, U / D, so
//    the pressure goes with D U^2
//  - the blades passing: the full blades at the inducer, and with the
//    splitters at the exducer. Rotor tones rise steeply with tip speed
//    (pressure with U^3, sound power with U^6) and with the blade loading
//  - buzz-saw: once the flow meets the inducer tips faster than sound,
//    shocks stand off the blades, and the small differences between them
//    turn the blade passing tone into a spread of shaft harmonics
// The levels are relative to the blade passing tone of a 60 mm wheel at
// 400 m/s and full flow; the synchronous tone is taken at half that.
// Twin turbochargers add their sound powers at the pitch of the faster one.
ForcedInduction::CompressorTones ForcedInduction::getCompressorTones() const {
    CompressorTones tones;
    if (isPositiveDisplacement()) return tones;

    constexpr double ReferenceDiameter = 0.06;
    constexpr double ReferenceTipSpeed = 400.0;
    constexpr double InducerRatio = 0.72;   // inducer tip over exducer tip
    constexpr double HubRatio = 0.3;        // inducer hub over inducer tip

    const double airR = constants::R / units::AirMolecularMass;
    const double a1 = std::sqrt(Gamma * airR * m_ambientTemperature);
    const double rho1 = m_ambientPressure / (airR * m_ambientTemperature);
    const double size = 2 * m_tipRadius / ReferenceDiameter;
    const double inducerRadius = InducerRatio * m_tipRadius;
    const double inducerArea = constants::pi * inducerRadius * inducerRadius * (1 - HubRatio * HubRatio);

    double omegaMax = 0;
    double synchronous = 0, bladePass = 0, buzzSaw = 0, whirl = 0;
    for (int i = 0; i < m_unitCount; ++i) {
        const Unit &unit = m_units[i];
        if (!unit.active || unit.omega <= 0) continue;
        omegaMax = std::max(omegaMax, unit.omega);

        const double u = unit.omega * m_tipRadius / ReferenceTipSpeed;
        const double massFlow = std::abs(unit.compressorFlow) * units::AirMolecularMass;
        const double loading = 0.3 + 0.7 * clamp(massFlow / m_maxCompressorFlow, 0.0, 1.0);

        // Relative Mach number of the flow at the inducer tip
        const double axial = massFlow / (rho1 * inducerArea);
        const double tip = unit.omega * inducerRadius;
        const double machRelative = std::sqrt(tip * tip + axial * axial) / a1;
        const double shock = clamp((machRelative - 1.0) / 0.15, 0.0, 1.0);

        // Worn bearings let the imbalance move the rotor further
        const double s = 0.5 * size * u * u * (1.0 + m_bearingWear);
        const double b = size * u * u * u * loading;

        // Worn bearings' clearance lets the rotor whirl. In journal
        // (floating ring) bearings the inner oil film whirls at 0.35 to 0.45
        // of the shaft's speed once the shaft is fast enough, a tone below
        // the once per revolution one that new bearings keep faint; the more
        // worn, the earlier and the stronger. Worn ball bearings ring at the
        // balls passing a race defect instead. It is the whole rotor moving,
        // so it comes out through the housing, not only the inlet. Worn out,
        // the rotor orbits through its whole clearance, several times the
        // imbalance's orbit: taken as twice the once per revolution tone's
        // source.
        if (m_bearingWear > 0) {
            const double onset = m_ballBearings ? 0.0 : 0.45 * (1.0 - 0.5 * m_bearingWear);
            const double grow = clamp((u - onset) / 0.2, 0.0, 1.0);
            const double w = 2.0 * m_bearingWear * grow * 0.5 * size * u * u;
            whirl += w * w;
        }
        const double z = 0.8 * size * shock * loading;
        synchronous += s * s;
        bladePass += b * b;
        buzzSaw += z * z;
    }
    if (omegaMax <= 0) return tones;

    const double f = omegaMax / (2 * constants::pi);
    const double heard = f * m_whistlePitch;
    tones.shaftFrequency = f;
    tones.synchronous = std::sqrt(synchronous) * inletTransmission(m_compressorInlet, heard);
    tones.bladePass = std::sqrt(bladePass)
        * inletTransmission(m_compressorInlet, m_compressorBlades * heard);
    tones.splitterPass = (m_splitterBlades > 0)
        ? 0.5 * std::sqrt(bladePass)
            * inletTransmission(m_compressorInlet, (m_compressorBlades + m_splitterBlades) * heard)
        : 0.0;
    tones.buzzSaw = std::sqrt(buzzSaw) * inletTransmission(m_compressorInlet, 0.5 * m_compressorBlades * heard);
    if (whirl > 0) {
        const double u = omegaMax * m_tipRadius / ReferenceTipSpeed;
        tones.whirl = std::sqrt(whirl);
        tones.whirlRatio = m_ballBearings ? 3.6 : clamp(0.45 - 0.05 * u, 0.33, 0.45);
    }
    return tones;
}

// An airbox (an expansion chamber with the filter) takes about 10 dB off
// at 1 kHz, rising to 15 dB from 4 kHz up, and less below 1 kHz. A factory
// intake adds a broadband (hiss) resonator that takes about another 14 dB
// from 0.8 to 4 kHz, fading over an octave either side; above that it
// leaves the blade passing tones to the airbox (Liu et al. 2019; resonators
// cover about 0.8 to 4 kHz)
double ForcedInduction::inletTransmission(CompressorInlet inlet, double frequency) {
    if (inlet == CompressorInlet::Open) return 1.0;

    const double f = std::max(frequency, 1.0);
    double dB = (f < 1000.0)
        ? 10.0 * f / 1000.0
        : 10.0 + 2.5 * std::min(std::log2(f / 1000.0), 2.0);
    if (inlet == CompressorInlet::Silenced) {
        const double below = std::max(std::log2(800.0 / f), 0.0);
        const double above = std::max(std::log2(f / 4000.0), 0.0);
        dB += 14.0 * std::max(1.0 - below - above, 0.0);
    }

    return std::pow(10.0, -dB / 20.0);
}

// What the exhaust gas bypassing the turbine leaves through: an external
// wastegate's own pipe, with no muffler
double ForcedInduction::getWastegateShare() const {
    if (!m_externalWastegate || !isTurbocharger()) return 0.0;
    return clamp(1.0 - m_turbineFraction, 0.0, 1.0);
}

// A worn wastegate linkage (its arm in a worn bushing) lets the flapper move
// in the play once it is off its seat, so each exhaust pulse knocks it
// against the linkage: a metallic rattle at the firing rate. Shut, the
// actuator holds it on its seat. A spring actuator holds it more loosely
// than an electronic one, and an external wastegate's poppet valve rides on
// its own guide and rattles less. (How hard the steady flow pins it against
// the linkage is in the synthesizer, from the pulses themselves.)
double ForcedInduction::getRattleShare() const {
    if (!isTurbocharger() || m_variableGeometry || m_wastegateRattle <= 0) return 0.0;
    const double open = clamp(m_wastegate / 0.05, 0.0, 1.0);
    const double hold = (m_boostControl == BoostControl::Spring) ? 1.0 : 0.6;
    const double valve = m_externalWastegate ? 0.3 : 1.0;
    return m_wastegateRattle * open * hold * valve;
}

// The air's noise:
//  - whoosh: broadband noise the compressor makes as it nears surge (high
//    pressure ratio at low flow), from the flow stalling in the wheel and
//    diffuser, 4 to 12 kHz in automotive compressors (Dehner et al., ASME
//    J. Turbomach. 2021). Its level climbs with the cube of the tip speed
//    like the tones'. The flow coefficient against the compressor's choke
//    flow places it on the map: from about half of it down to a quarter
//    the whoosh comes in fully
//  - roar: the turbulent air rushing into the inlet, rising with the cube of
//    its velocity, around 1.5 kHz
//  - blow-off: a gas jet (Lighthill: sound power ~ D^2 u^8, so pressure
//    ~ D u^4). The jet's speed comes from the charge pressure expanding to
//    the outside, its diameter from the vented flow at that speed, and its
//    peak frequency from a Strouhal number of 0.2, so a lift from full
//    boost starts as a loud, high hiss that falls in pitch and level as the
//    charge empties. A recirculating valve vents into the compressor's inlet,
//    so its jet only gets out through the inlet
//  - surge: the flow reversing through the compressor, in deep surge cycles
//    at about 0.4 times the Helmholtz frequency of the charge volume with
//    the compressor's duct (Greitzer), so a larger intercooler and pipes
//    flutter slower
ForcedInduction::AirNoise ForcedInduction::getAirNoise() const {
    AirNoise noise;

    const double airR = constants::R / units::AirMolecularMass;
    const double a1 = std::sqrt(Gamma * airR * m_ambientTemperature);
    const double rho1 = m_ambientPressure / (airR * m_ambientTemperature);
    const double inducerRadius = 0.72 * m_tipRadius;
    const double inducerArea = constants::pi * inducerRadius * inducerRadius * (1 - 0.3 * 0.3);

    if (!isPositiveDisplacement()) {
        constexpr double ReferenceDiameter = 0.06;
        constexpr double ReferenceTipSpeed = 400.0;
        const double d = 2 * m_tipRadius;
        const double size = d / ReferenceDiameter;
        const double chokeCoefficient = m_maxCompressorFlow / (rho1 * MaxTipSpeed * d * d);
        const double pressureRatio = std::max(m_charge.pressure() / m_ambientPressure, 1.0);

        double whoosh = 0, roar = 0, omegaMax = 0;
        for (int i = 0; i < m_unitCount; ++i) {
            const Unit &unit = m_units[i];
            if (!unit.active || unit.omega <= 0) continue;
            omegaMax = std::max(omegaMax, unit.omega);

            const double U = unit.omega * m_tipRadius;
            const double massFlow = std::max(unit.compressorFlow, 0.0) * units::AirMolecularMass;
            const double coefficient = massFlow / (rho1 * U * d * d) / chokeCoefficient;
            const double nearSurge = clamp((0.5 - coefficient) / 0.25, 0.0, 1.0)
                * clamp((pressureRatio - 1.0) / 0.5, 0.0, 1.0);
            const double u = U / ReferenceTipSpeed;
            const double w = 0.6 * size * u * u * u * (0.1 + 0.9 * nearSurge);
            whoosh += w * w;

            const double velocity = massFlow / (rho1 * inducerArea) / 100.0;
            const double r = 0.3 * size * velocity * velocity * velocity;
            roar += r * r;
        }

        if (omegaMax > 0) {
            const double f = omegaMax / (2 * constants::pi) * m_whistlePitch;
            noise.whooshFrequency = clamp(0.55 * m_compressorBlades * f, 2000.0, 12000.0);
            noise.whoosh = std::sqrt(whoosh) * inletTransmission(m_compressorInlet, noise.whooshFrequency);
            noise.roar = std::sqrt(roar) * inletTransmission(m_compressorInlet, 1500.0);
        }

        // Deep surge
        const double helmholtz = a1 / (2 * constants::pi)
            * std::sqrt(inducerArea / (std::max(m_charge.volume(), 1e-4) * 0.5));
        noise.surgeRate = clamp(0.4 * helmholtz, 4.0, 40.0);
        noise.surge = m_surge * inletTransmission(m_compressorInlet, 380.0);
    }

    // A leak's jet: the same as the blow-off valve's below, through the
    // hole's own diameter, and straight out into the engine bay
    if (m_leakDiameter > 0 && m_leakFlowRate > 1e-6) {
        const double leakRatio = m_charge.pressure() / m_ambientPressure;
        if (leakRatio > 1.0) {
            const double T = m_charge.temperature();
            const double expansion = 1.0 - std::pow(1.0 / leakRatio, Exponent);
            const double u = std::sqrt(2 * Cp_mass * T * expansion);
            noise.leak = std::min((m_leakDiameter / 0.02) * std::pow(u / 300.0, 4.0), 3.0);
            noise.leakFrequency = clamp(0.2 * u / m_leakDiameter, 800.0, 16000.0);
        }
    }

    // The blow-off valve's jet
    const double vented = m_blowoffFlowRate * units::AirMolecularMass;   // [kg/s]
    const double ratio = m_charge.pressure() / m_ambientPressure;
    if (vented > 1e-5 && ratio > 1.0) {
        const double T = m_charge.temperature();
        const double expansion = 1.0 - std::pow(1.0 / ratio, Exponent);
        const double u = std::sqrt(2 * Cp_mass * T * expansion);
        const double rhoJet = m_ambientPressure / (airR * T * (1.0 - expansion));
        const double area = vented / (rhoJet * std::max(u, 1.0));
        const double diameter = std::sqrt(4 * area / constants::pi);
        const double jet = (diameter / 0.02) * std::pow(u / 300.0, 4.0);

        noise.blowoffFrequency = clamp(0.2 * u / std::max(diameter, 1e-3), 800.0, 12000.0);
        noise.blowoff = std::min(jet, 3.0);
        if (getBlowoffType() == BlowoffType::Recirculating) {
            noise.blowoff *= inletTransmission(m_compressorInlet, noise.blowoffFrequency);
        }
    }

    return noise;
}

int ForcedInduction::getSoundMode() const {
    int type = 1;
    switch (m_type) {
    case Type::Roots: type = 2; break;
    case Type::TwinScrew: type = 3; break;
    case Type::Centrifugal: type = 4; break;
    default: break;
    }

    return 10 * type + static_cast<int>(getBlowoffType())
        + 100 * m_compressorBlades + 10000 * m_splitterBlades;
}
