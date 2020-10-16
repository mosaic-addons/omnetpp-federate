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

simple MosaicScenarioManager {
    parameters:
        @class(MosaicScenarioManager);
        bool debug = default(false); // debug level for logging
        string vehModuleType = default("omnetpp_federate.node.Vehicle"); // module type to be used in the simulation for each managed vehicle
        string vehModuleName = default("veh"); // module name to be used in the simulation for each managed vehicle
        string rsuModuleType = default("omnetpp_federate.node.Rsu"); // module type to be used in the simulation for each managed vehicle
        string rsuModuleName = default("rsu"); // module name to be used in the simulation for each managed vehicle
        string moduleDisplayString = default("i=misc/node2;is=vs;r=0,,#707070,1"); // module displayString to be used in the simulation for each managed
    gates:
        input mosaicProxyIn[];
        output mosaicProxyOut[];
}
