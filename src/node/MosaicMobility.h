/*
 * Copyright (c) 2020 Fraunhofer FOKUS and others. All rights reserved.
 *
 * Contact: mosaic@fokus.fraunhofer.de
 *
 * This class is developed for the MOSAIC-NS-3 coupling.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef MOSAICMOBILITY_H_
#define MOSAICMOBILITY_H_

#include <omnetpp.h>

#include "inet/mobility/base/MobilityBase.h"
#include "inet/common/ModuleAccess.h"
#include "inet/common/geometry/common/Coord.h"

namespace omnetpp_federate {

/**
 * Mobility module for nodes, which are controlled by mobility input from Mosaic.
 *
 * @author rpr
 */
class MosaicMobility : public inet::MobilityBase {
public:
    MosaicMobility();
    virtual ~MosaicMobility();

    /** Initialize method to also initialize BasicMobility. */
    virtual void initialize(int stage) override;

    /** Mandatory stub of handleSelfMsg method. */
    virtual void handleSelfMessage(omnetpp::cMessage *message) override {};

    /** setNextPosition method to update node positions. */
    virtual void setNextPosition(inet::Coord& nextPos);

    void setNextOrientation(inet::EulerAngles nextOri);

    /** setExternalId method from MOSAIC id to OMNeT++ internal id. */
    virtual void setExternalId(int externalId);

    /**
     * Returns the current position at the current simulation time.
     */
    virtual inet::Coord getCurrentPosition() override;

    /**
     * Returns the current velocity at the current simulation time.
     */
    virtual inet::Coord getCurrentVelocity() override { return inet::Coord::ZERO; }

    /**
     * Returns the current angular position at the current simulation time.
     */
    virtual inet::Quaternion getCurrentAngularPosition() override;

    /** @brief Initializes the position from the display string or from module parameters. */
    virtual void setInitialPosition() override;

    /**
     * Returns the maximum possible speed at any future time.
     */
    virtual double getMaxSpeed() { return 9999; };
    /**
     * Returns the current acceleration at the current simulation time.
     */
    inet::Coord getCurrentAcceleration() { return inet::Coord::ZERO; };

    /**
     * Returns the current angular velocity at the current simulation time.
     */
    inet::Quaternion getCurrentAngularVelocity() { return inet::Quaternion::IDENTITY; };

    /**
     * Returns the current angular acceleration at the current simulation time.
     */
    inet::Quaternion getCurrentAngularAcceleration() { return inet::Quaternion::IDENTITY; };

    /**
     * Returns the maximum positions along each axes.
     */
    inet::Coord getConstraintAreaMax() { return inet::Coord::ZERO; };

    /**
     * Returns the minimum positions along each axes.
     */
    inet::Coord getConstraintAreaMin() { return inet::Coord::ZERO; };

private:
    /** Logging level of debug prints. */
    bool debug;

    /** MOSAIC controlled node id. */
    int externalId;
};

} // namespace omnetpp_federate

#endif /* MOSAICMOBILITY_H_ */
