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

//
// Base configuration command message
// Extended by specific radio technology configuration commands
//
class MosaicConfigurationCmd extends inet::Request {
    int msgId;
    int nodeId;
}

//
// WiFi radio configuration command message
// Contains all parameters needed for WiFi radio setup including dual radio support
//
class MosaicWifiConfigurationCmd extends MosaicConfigurationCmd {
    // Number of WiFi radios (0, 1, or 2)
    int numRadios = 0;
    
    // Primary radio (radio 0) configuration
    bool turnedOn0 = false;
    inet::Ipv4Address ip0;
    inet::Ipv4Address subnet0;
    double power0 = 0.0;
    int numchannels0 = 0;
    int channel00 = -1; // -1 indicates unset
    int channel01 = -1;
    
    // Secondary radio (radio 1) configuration for dual radio setups
    bool turnedOn1 = false;
    inet::Ipv4Address ip1;
    inet::Ipv4Address subnet1;
    double power1 = 0.0;
    int numchannels1 = 0;
    int channel10 = -1;
    int channel11 = -1;
}

//
// Cellular radio configuration command message
// Contains all parameters needed for cellular radio setup
//
class MosaicCellularConfigurationCmd extends MosaicConfigurationCmd {
    inet::Ipv4Address ip;
    inet::Ipv4Address subnet;
}
