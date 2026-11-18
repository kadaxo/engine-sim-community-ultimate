#include <gtest/gtest.h>

#include "../include/starter_motor.h"
#include "../include/units.h"

namespace {
    atg_scs::Constraint::Output limitsFor(double torque, double speed, bool enabled) {
        StarterMotor starter;
        starter.m_maxTorque = torque;
        starter.m_rotationSpeed = speed;
        starter.m_enabled = enabled;

        atg_scs::Constraint::Output output = {};
        starter.calculate(&output, nullptr);
        return output;
    }
}

TEST(StarterMotorTests, TorqueLimitsTheStarter) {
    const double torque = units::torque(150.0, units::ft_lb);
    const atg_scs::Constraint::Output output = limitsFor(torque, -units::rpm(500), true);

    EXPECT_DOUBLE_EQ(output.limits[0][0], -torque);
    EXPECT_DOUBLE_EQ(output.limits[0][1], 0.0);
}

TEST(StarterMotorTests, NegativeTorqueIsTheSameStarter) {
    // A script may write the torque as a negative number. The limits must
    // stay in order, or the solver pushes with no speed limit.
    const double torque = units::torque(50.0, units::ft_lb);
    for (const double speed : { -units::rpm(200), units::rpm(200) }) {
        const atg_scs::Constraint::Output positive = limitsFor(torque, speed, true);
        const atg_scs::Constraint::Output negative = limitsFor(-torque, speed, true);

        EXPECT_LE(negative.limits[0][0], negative.limits[0][1]);
        EXPECT_DOUBLE_EQ(negative.limits[0][0], positive.limits[0][0]);
        EXPECT_DOUBLE_EQ(negative.limits[0][1], positive.limits[0][1]);
    }
}

TEST(StarterMotorTests, DisabledStarterAppliesNoTorque) {
    const atg_scs::Constraint::Output output =
        limitsFor(units::torque(150.0, units::ft_lb), -units::rpm(500), false);

    EXPECT_DOUBLE_EQ(output.limits[0][0], 0.0);
    EXPECT_DOUBLE_EQ(output.limits[0][1], 0.0);
}
