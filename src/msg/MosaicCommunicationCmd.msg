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

import inet.common.INETDefs;
import inet.common.Units;
import inet.common.packet.Message;
import inet.networklayer.contract.ipv4.Ipv4Address;

cplusplus {{
    #include "inet/common/Units.h"
    #include "inet/networklayer/contract/ipv4/Ipv4Address.h"
    #include "inet/common/packet/chunk/cPacketChunk.h"
}}
cplusplus{{
    using inet::units::values::B;
    using inet::units::values::unit2string;
    using omnetpp::cValue;

}}

namespace omnetpp_federate;

enum CommunicationCommandType {
    COMMUNICATION_CMD_SEND_MESSAGE = 1;
};

class MosaicCommunicationCmd extends inet::Request {
    int nodeId;
    int channelId; // WLAN channel id
    int	msgId;
    inet::B length; // Length of v2x messages payload
    int ttl; // Time-To-Live for topological sending (or geographical if < 0)
    inet::Ipv4Address destAddr;
    CommunicationCommandType cmdType;
}
