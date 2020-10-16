//
// Copyright (c) 2020 Fraunhofer FOKUS and others. All rights reserved.
//
// Contact: mosaic@fokus.fraunhofer.de
//
// This class is developed for the MOSAIC-NS-3 coupling.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation;
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//

import inet.common.geometry.Geometry;
import inet.common.packet.Message;

cplusplus {{
    #include "inet/common/geometry/common/Coord.h"
}}

namespace omnetpp_federate;

enum MobilityCommandType {
    MOBILITY_CMD_MOVE_NODES = 1;
    MOBILITY_CMD_ADD_NODES = 2;
    MOBILITY_CMD_ADD_RSU_NODES = 3;
    MOBILITY_CMD_REMOVE_NODES = 4;
};

class MosaicMobilityCmd extends inet::Request {
    MobilityCommandType cmdType;
    int nodeId[];
    inet::Coord	position[];
}
