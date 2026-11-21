#ifndef ATG_ENGINE_SIM_FORCED_INDUCTION_NODE_H
#define ATG_ENGINE_SIM_FORCED_INDUCTION_NODE_H

#include "object_reference_node.h"

#include "engine_sim.h"

#include "../../include/forced_induction.h"

#include <string>

namespace es_script {

    // A turbocharger or supercharger (from kyan0s-git/engine-sim). Unknown
    // names in the string inputs fall back to the defaults: a single
    // turbocharger with an atmospheric blow-off valve.
    class ForcedInductionNode : public ObjectReferenceNode<ForcedInductionNode> {
    public:
        ForcedInductionNode() { /* void */ }
        virtual ~ForcedInductionNode() { /* void */ }

        // Creates the turbocharger or supercharger and connects it to the
        // engine's intakes and exhausts
        void generate(Engine *engine) const {
            ForcedInduction::Parameters params;

            if (m_type == "roots") params.type = ForcedInduction::Type::Roots;
            else if (m_type == "twin_screw") params.type = ForcedInduction::Type::TwinScrew;
            else if (m_type == "centrifugal") params.type = ForcedInduction::Type::Centrifugal;
            else params.type = ForcedInduction::Type::Turbocharger;

            if (m_arrangement == "twin") params.arrangement = ForcedInduction::Arrangement::Twin;
            else if (m_arrangement == "sequential") params.arrangement = ForcedInduction::Arrangement::Sequential;
            else params.arrangement = ForcedInduction::Arrangement::Single;

            params.compressorDiameter = m_compressorDiameter;
            params.compressorFlow = m_compressorFlow;
            params.compressorEfficiency = m_compressorEfficiency;
            params.maxSpeed = m_maxSpeed;
            params.turbineAr = m_turbineAr;
            params.turbineDiameter = m_turbineDiameter;
            params.variableGeometry = m_variableGeometry;
            params.minTurbineAr = m_minTurbineAr;
            params.soundAfterTurbine = (m_exhaustSound != "before_turbine");
            params.turbineEfficiency = m_turbineEfficiency;
            params.inertia = m_inertia;
            params.ballBearings = (m_bearings == "ball");
            params.boost = m_boost;
            params.blowoffValve = m_blowoffValve;
            params.blowoffType = (m_blowoffType == "recirculating") ? ForcedInduction::BlowoffType::Recirculating
                : (m_blowoffType == "none") ? ForcedInduction::BlowoffType::None
                : ForcedInduction::BlowoffType::Atmospheric;
            params.boostControl = (m_boostControl == "spring")
                ? ForcedInduction::BoostControl::Spring
                : ForcedInduction::BoostControl::Electronic;
            params.externalWastegate = (m_wastegate == "external");
            params.twinScroll = m_twinScroll;
            params.whistlePitch = m_whistlePitch;
            params.compressorBlades = m_compressorBlades;
            params.splitterBlades = m_splitterBlades;
            params.compressorInlet = (m_compressorInlet == "airbox") ? ForcedInduction::CompressorInlet::Airbox
                : (m_compressorInlet == "open") ? ForcedInduction::CompressorInlet::Open
                : ForcedInduction::CompressorInlet::Silenced;
            params.bearingWear = m_bearingWear;
            params.wastegateRattle = m_wastegateRattle;
            params.boostLeak = m_boostLeak;
            params.antiLag = m_antiLag;
            params.antiLagTiming = m_antiLagTiming;
            params.antiLagThrottle = m_antiLagThrottle;
            params.antiLagMinSpeed = m_antiLagMinSpeed;
            params.driveRatio = m_driveRatio;
            params.displacement = m_displacement;
            params.bypassValve = m_bypassValve;
            params.intercoolerEffectiveness = m_intercoolerEffectiveness;
            params.chargeVolume = m_chargeVolume;
            params.sequentialSpeed = m_sequentialSpeed;

            ForcedInduction *forcedInduction = new ForcedInduction;
            forcedInduction->initialize(params);
            engine->setForcedInduction(forcedInduction);
        }

    protected:
        virtual void registerInputs() {
            addInput("type", &m_type);
            addInput("arrangement", &m_arrangement);
            addInput("compressor_diameter", &m_compressorDiameter);
            addInput("compressor_flow", &m_compressorFlow);
            addInput("compressor_efficiency", &m_compressorEfficiency);
            addInput("max_speed", &m_maxSpeed);
            addInput("turbine_ar", &m_turbineAr);
            addInput("turbine_diameter", &m_turbineDiameter);
            addInput("variable_geometry", &m_variableGeometry);
            addInput("min_turbine_ar", &m_minTurbineAr);
            addInput("exhaust_sound", &m_exhaustSound);
            addInput("turbine_efficiency", &m_turbineEfficiency);
            addInput("inertia", &m_inertia);
            addInput("bearings", &m_bearings);
            addInput("boost", &m_boost);
            addInput("blowoff_valve", &m_blowoffValve);
            addInput("drive_ratio", &m_driveRatio);
            addInput("displacement", &m_displacement);
            addInput("bypass_valve", &m_bypassValve);
            addInput("intercooler_effectiveness", &m_intercoolerEffectiveness);
            addInput("charge_volume", &m_chargeVolume);
            addInput("sequential_speed", &m_sequentialSpeed);
            addInput("blowoff_type", &m_blowoffType);
            addInput("boost_control", &m_boostControl);
            addInput("wastegate", &m_wastegate);
            addInput("twin_scroll", &m_twinScroll);
            addInput("whistle_pitch", &m_whistlePitch);
            addInput("compressor_blades", &m_compressorBlades);
            addInput("splitter_blades", &m_splitterBlades);
            addInput("compressor_inlet", &m_compressorInlet);
            addInput("bearing_wear", &m_bearingWear);
            addInput("wastegate_rattle", &m_wastegateRattle);
            addInput("boost_leak", &m_boostLeak);
            addInput("anti_lag", &m_antiLag);
            addInput("anti_lag_timing", &m_antiLagTiming);
            addInput("anti_lag_throttle", &m_antiLagThrottle);
            addInput("anti_lag_min_speed", &m_antiLagMinSpeed);

            ObjectReferenceNode<ForcedInductionNode>::registerInputs();
        }

        virtual void _evaluate() {
            setOutput(this);
            readAllInputs();
        }

        std::string m_type = "turbo";
        std::string m_arrangement = "single";
        double m_compressorDiameter = 0.06;
        double m_compressorFlow = 0.3;
        double m_compressorEfficiency = 0.75;
        double m_maxSpeed = 0.0;
        double m_turbineAr = 0.64;
        double m_turbineDiameter = 0.0;
        bool m_variableGeometry = false;
        double m_minTurbineAr = 0.0;
        std::string m_exhaustSound = "after_turbine";
        double m_turbineEfficiency = 0.7;
        double m_inertia = 3e-5;
        std::string m_bearings = "journal";
        double m_boost = 1e5;
        bool m_blowoffValve = true;
        double m_driveRatio = 1.0;
        double m_displacement = 1.9e-3;
        bool m_bypassValve = true;
        double m_intercoolerEffectiveness = 0.7;
        double m_chargeVolume = 6e-3;
        double m_sequentialSpeed = 400.0;
        std::string m_blowoffType = "atmospheric";
        std::string m_boostControl = "electronic";
        std::string m_wastegate = "internal";
        bool m_twinScroll = false;
        double m_whistlePitch = 1.0;
        int m_compressorBlades = 6;
        int m_splitterBlades = 6;
        std::string m_compressorInlet = "silenced";
        double m_bearingWear = 0.0;
        double m_wastegateRattle = 0.0;
        double m_boostLeak = 0.0;
        bool m_antiLag = false;
        double m_antiLagTiming = -30.0 * 3.14159265358979 / 180;
        double m_antiLagThrottle = 0.25;
        double m_antiLagMinSpeed = 2 * 3.14159265358979 * 3000 / 60;
    };

} /* namespace es_script */

#endif /* ATG_ENGINE_SIM_FORCED_INDUCTION_NODE_H */
