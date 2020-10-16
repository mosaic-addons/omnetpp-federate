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

#include "MosaicMobility.h"

namespace omnetpp_federate {

Define_Module(MosaicMobility);

MosaicMobility::MosaicMobility() {
    // nop
}

MosaicMobility::~MosaicMobility() {
    // nop
}

/**
 * Initializes also parent module BasicMobility.
 *
 * @param stage
 */
void MosaicMobility::initialize(int stage) {
    EV << "Initialize MosaicMobility stage " << stage << std::endl;
    inet::MobilityBase::initialize(stage);

    if (stage == 1) {
        debug = par("debug");
    }
}

/**
 * Sets Mosaic controlled nodeId for this module.
 *
 * @param externalId
 * 		nodeId as managed within Mosaic
 */
void MosaicMobility::setExternalId(int externalId) {
    this->externalId = externalId;
}

/**
 * Update the orientation
 *
 * @param nextOri
 * 		the new Orientation in EulerAngles
 */
void MosaicMobility::setNextOrientation(inet::EulerAngles nextOri) {
    // Update position coordinates
    lastOrientation = inet::Quaternion(nextOri);
    emitMobilityStateChangedSignal();
    refreshDisplay();
}

/**
 * Provides mobility update with projected coordinates from Mosaic
 *
 * @param nextPos
 *      next position to be updated as (x,y)
 */
void MosaicMobility::setNextPosition(inet::Coord& nextPos) {
    // Update position coordinates
    lastPosition.x = nextPos.x;
    lastPosition.y = nextPos.y;
    lastPosition.z = nextPos.z;

    emitMobilityStateChangedSignal();
    refreshDisplay();
}

/**
 * @return the current position in the simulation
 */
inet::Coord MosaicMobility::getCurrentPosition() {
    return lastPosition;
}

/**
 * @return the current orientation of the object
 */
inet::Quaternion MosaicMobility::getCurrentAngularPosition() {
    return lastOrientation;
}

void MosaicMobility::setInitialPosition() {
    if (lastPosition == inet::Coord::NIL) { //only call the original setInitialPosition if position was never changed (dirty: you can not insert nodes at 0.0,0.0,0.0)
        inet::MobilityBase::setInitialPosition();
    }
}

} // namespace omnetpp_federate
