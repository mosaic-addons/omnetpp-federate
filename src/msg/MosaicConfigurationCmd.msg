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
    #include "inet/networklayer/contract/ipv4/Ipv4Address.h"
}}

namespace omnetpp_federate;

enum ConfigurationCommandType {
    CONFIGURATION_CMD_CONF_RADIO = 1;
};

//
// Messagetype for configuration command
//
class MosaicConfigurationCmd extends inet::Request {
    int msgId;
    int nodeId;
    int numRadios; // number of radios
    ConfigurationCommandType cmdType;
    
    //Configuration for radio 0
    bool turnedOn0;
    inet::Ipv4Address ip0;
    inet::Ipv4Address subnet0;
    double power0;
    int numchannels0;
    int channel00;
    int	channel01;
    
    //Configuration for radio 1
    bool turnedOn1;
    inet::Ipv4Address ip1;
    inet::Ipv4Address subnet1;
    double power1;
    int	numchannels1;
    int channel10;
    int	channel11;
}
