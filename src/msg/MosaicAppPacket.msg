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

import inet.networklayer.contract.ipv4.Ipv4Address;
import inet.common.packet.Packet;

cplusplus {{
        #include "inet/networklayer/contract/ipv4/Ipv4Address.h"
        #include "inet/common/packet/Packet.h"
}}

namespace omnetpp_federate;

//
// Module for definition of message layout used by MOSAIC application layer.
//
class MosaicAppPacket extends inet::Packet {
    int nodeId;
    int	msgId;  
    int	length; // Length of v2x messages payload
    int channelId; // WLAN channel
    inet::Ipv4Address destAddr; // Destination ip address
}
