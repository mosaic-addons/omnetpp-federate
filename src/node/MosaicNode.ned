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

package omnetpp_federate.node;

import inet.common.MessageDispatcher;
import inet.linklayer.ieee80211.Ieee80211Interface;
import inet.networklayer.common.InterfaceTable;
import inet.networklayer.ipv4.Icmp;
import inet.networklayer.ipv4.Ipv4NetworkLayer;
import inet.networklayer.ipv4.Ipv4RoutingTable;
import inet.physicallayer.ieee80211.packetlevel.Ieee80211Radio;
import inet.routing.contract.IManetRouting;
import inet.routing.gpsr.Gpsr;
import inet.transportlayer.udp.Udp;

import omnetpp_federate.node.MosaicMobility;
import omnetpp_federate.node.MosaicProxyApp;

module MosaicNode {
    parameters:
        @node;
        @networkNode;
        @display("bgb=548.6,617.5");
        @display("i=device/wifilaptop");
        string routingProtocol @enum("GPSR") = default("");	//Preparation to eventually support geo routing
        *.interfaceTableModule = default(absPath(".interfaceTable"));
        **.routingTableModule = default(absPath(".routingTable"));
        *.radio.typename = default("Ieee80211Radio");
        **.radio.transmitter.typename = default("Ieee80211ScalarTransmitter");
        **.radio.receiver.typename = default("Ieee80211ScalarReceiver");
        wlan0.mgmt.typename = default("Ieee80211MgmtAdhoc");
        wlan1.mgmt.typename = default("Ieee80211MgmtAdhoc");
        wlan0.radio.antenna.mobilityModule = absPath(".mobility");
        wlan1.radio.antenna.mobilityModule = absPath(".mobility");
        udp.icmpModule = absPath(".icmp");
    gates:
        //None, since the radio will be connected dynamically upon configuration message
    submodules:
        mobility: MosaicMobility {
            @display("p=468.234,260.82");
        }
        wlan0: Ieee80211Interface {
            parameters:
                @display("p=149,505;q=queue");
        }
        wlan1: Ieee80211Interface {
            parameters:
                @display("p=370,513;q=queue");
        }
        interfaceTable: InterfaceTable {
            parameters:
                @display("p=62.399998,241.8;is=s");
        }
		//routing table
        routingTable: Ipv4RoutingTable {
            parameters:
                @display("p=62.399998,163.8;is=s");
        }
        // network layer
        networkLayer: Ipv4NetworkLayer {
            parameters:
                configurator.networkConfiguratorModule = "";
                @display("p=268,371;q=queue");
        }
        md: MessageDispatcher {
            parameters:
                @display("p=268,452;b=500,5,,,,1");
        }
        //UDP
        icmp: Icmp {
            @display("p=468.234,152.766");
        }
        udp: Udp {
            @display("p=268,241");
        }
        //Routing, may be only be connected to networkLayer0
        gpsr: <routingProtocol> like IManetRouting if routingProtocol != "" {
            @display("p=469,371");
        }
        proxyApp: MosaicProxyApp {
            @display("p=268,124");
        }
    connections allowunconnected:
        wlan0.upperLayerOut --> md.in++;
        wlan0.upperLayerIn <-- md.out++;

        wlan1.upperLayerOut --> md.in++;
        wlan1.upperLayerIn <-- md.out++;

        md.out++ --> networkLayer.ifIn;
        md.in++ <-- networkLayer.ifOut;

        networkLayer.transportOut --> gpsr.ipIn if routingProtocol != "";
        networkLayer.transportIn <-- gpsr.ipOut if routingProtocol != "";
        networkLayer.transportOut --> udp.ipIn;
        networkLayer.transportIn <-- udp.ipOut;

        udp.appOut --> proxyApp.socketIn;
        udp.appIn <-- proxyApp.socketOut;
}
