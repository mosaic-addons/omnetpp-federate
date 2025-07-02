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

package omnetpp_federate.mgmt;

import inet.physicallayer.wireless.ieee80211.packetlevel.Ieee80211ScalarRadioMedium;
import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.gpsr.GpsrRouter;
import inet.node.inet.AdhocHost;

import omnetpp_federate.mgmt.MosaicScenarioManager;
import omnetpp_federate.node.Rsu;
import omnetpp_federate.node.Vehicle;

//
// Simulation setup for MOSAIC controlled simulation.
//
network Simulation {
    parameters:
        @display("bgb=440,300");
    submodules:
        radioMedium: Ieee80211ScalarRadioMedium {
            parameters:
                @display("p=60,50");
        }
        mgmt: MosaicScenarioManager {
            parameters:
                @display("p=57,137;is=s");
        }
        veh[0]: Vehicle {
            parameters:
                @dynamic;
                @display("p=309,52");
        }
        rsu[0]: Rsu {
            parameters:
                @dynamic;
                @display("p=309,140");
        }
}
