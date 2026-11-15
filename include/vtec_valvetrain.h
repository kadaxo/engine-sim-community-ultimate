#ifndef ATG_ENGINE_SIM_VTEC_STANDARD_VALVETRAIN_H
#define ATG_ENGINE_SIM_VTEC_STANDARD_VALVETRAIN_H

#include "valvetrain.h"

#include <vector>

class Engine;
class VtecValvetrain : public Valvetrain {
public:
    struct Parameters {
        double minRpm;
        double minSpeed;
        double manifoldVacuum;
        double minThrottlePosition;

        Camshaft *intakeCamshaft;
        Camshaft *exhaustCamshaft;

        Camshaft *vtecIntakeCamshaft;
        Camshaft *vtexExhaustCamshaft;

        Engine *engine;
    };

public:
    VtecValvetrain();
    virtual ~VtecValvetrain();

    void initialize(const Parameters &parameters);

    virtual double intakeValveLift(int cylinder) override;
    virtual double exhaustValveLift(int cylinder) override;

    virtual Camshaft *getActiveIntakeCamshaft() override;
    virtual Camshaft *getActiveExhaustCamshaft() override;

private:
    bool isVtecEnabled() const;

    // The cam a cylinder's valve follows. It changes only while the valve is
    // closed on both cams, so the lift never jumps in the middle of an opening.
    Camshaft *followedCamshaft(
        int cylinder, std::vector<bool> &onVtec, Camshaft *normal, Camshaft *vtec);

    std::vector<bool> m_intakeOnVtec;
    std::vector<bool> m_exhaustOnVtec;

    Camshaft *m_intakeCamshaft;
    Camshaft *m_exhaustCamshaft;

    Camshaft *m_vtecIntakeCamshaft;
    Camshaft *m_vtecExhaustCamshaft;

    Engine *m_engine;

    double m_minRpm;
    double m_minSpeed;
    double m_manifoldVacuum;
    double m_minThrottlePosition;
};

#endif /* ATG_ENGINE_SIM_VTEC_STANDARD_VALVETRAIN_H */
