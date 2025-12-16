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
import inet.linklayer.ethernet.EthernetInterface;
import inet.linklayer.ethernet.basic.EthernetEncapsulation;

import inet.networklayer.common.InterfaceTable;
import inet.networklayer.ipv4.Icmp;
import inet.networklayer.ipv4.Ipv4NetworkLayer;
import inet.networklayer.ipv4.Ipv4RoutingTable;
import inet.transportlayer.udp.Udp;

import omnetpp_federate.node.MosaicMobility;
import omnetpp_federate.node.MosaicProxyApp;

module WiredNode {
    parameters:
        @node;
        @networkNode;
        @display("bgb=548.6,617.5");
        @display("i=device/server");
        *.routingTableModule = "^.routingTable";
        *.interfaceTableModule = "^.interfaceTable";
        **.numEthInterfaces = default(1);
        udp.icmpModule = absPath(".icmp");
        ipv4.arp.proxyArpInterfaces = default("");
    gates:
        inout pppg @labels(PppFrame-conn) @allowUnconnected; 
        inout ethg @labels(EtherFrame-conn) @allowUnconnected;
    submodules:
        mobility: MosaicMobility {
            @display("p=468.234,260.82");
        }
        eth: EthernetInterface {
            parameters:
                @display("p=149,505;q=queue");
        }
        ethernetEncap: EthernetEncapsulation {
            parameters:
                @display("p=149,452");
                registerProtocol = true;
        }
        interfaceTable: InterfaceTable {
            parameters:
                @display("p=62.399998,241.8;is=s");
        }
        routingTable: Ipv4RoutingTable {
            parameters:
                @display("p=62.399998,163.8;is=s");
        }
        // network layer
        ipv4: Ipv4NetworkLayer {
            parameters:
                @display("p=268,371;q=queue");
                forwarding = true;
        }
        md: MessageDispatcher {
            parameters:
                @display("p=268,452;b=500,5,,,,1");
        }
        // UDP
        // icmp: Icmp {
        //     @display("p=468.234,152.766");
        // }
        udp: Udp {
            @display("p=268,241");
        }
        proxyApp: MosaicProxyApp {
            @display("p=268,124");
        }
    connections allowunconnected:
        // Ethernet to Encapsulation layer
        eth.upperLayerOut --> ethernetEncap.lowerLayerIn;
        eth.upperLayerIn <-- ethernetEncap.lowerLayerOut;
        
        // Encapsulation to Message Dispatcher
        ethernetEncap.upperLayerOut --> md.in++;
        ethernetEncap.upperLayerIn <-- md.out++;
        
        ethg <--> eth.phys;

        md.out++ --> ipv4.ifIn;
        md.in++ <-- ipv4.ifOut;

        ipv4.transportOut --> udp.ipIn;
        ipv4.transportIn <-- udp.ipOut;

        udp.appOut --> proxyApp.socketIn;
        udp.appIn <-- proxyApp.socketOut;
}