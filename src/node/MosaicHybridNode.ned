//
// MosaicHybridNode - Extended node with both WiFi and 5G/LTE capabilities
//
// This module extends the original MosaicNode by adding 5G/LTE cellular
// connectivity alongside the existing WiFi interfaces. It can operate as
// a car or RSU with multi-radio capabilities for V2X communications.
//

package omnetpp_federate.node;

import inet.common.MessageDispatcher;
import inet.linklayer.ieee80211.Ieee80211Interface;
import inet.networklayer.common.InterfaceTable;
import inet.networklayer.ipv4.Icmp;
import inet.networklayer.ipv4.Ipv4NetworkLayer;
import inet.networklayer.ipv4.Ipv4RoutingTable;
import inet.networklayer.configurator.ipv4.HostAutoConfigurator;
import inet.physicallayer.wireless.ieee80211.packetlevel.Ieee80211Radio;
import inet.routing.contract.IManetRouting;
import inet.routing.gpsr.Gpsr;
import inet.transportlayer.udp.Udp;
import simu5g.stack.ICellularNic;
import simu5g.corenetwork.statsCollector.UeStatsCollector;

import omnetpp_federate.node.MosaicMobility;
import omnetpp_federate.node.MosaicProxyApp;

module MosaicHybridNode {
    parameters:
        @node;
        @networkNode;
        @display("bgb=650,750");
        @display("i=device/car");
        
        // WiFi configuration
        string routingProtocol @enum("", "GPSR") = default("");
        *.interfaceTableModule = default(absPath(".interfaceTable"));
        **.routingTableModule = default(absPath(".networkLayer.routingTable"));
        *.mobilityModule = default(absPath(".mobility"));
        *.radio.typename = default("Ieee80211Radio");
        **.radio.transmitter.typename = default("Ieee80211ScalarTransmitter");
        **.radio.receiver.typename = default("Ieee80211ScalarReceiver");
        wlan0.mgmt.typename = default("Ieee80211MgmtAdhoc");
        wlan1.mgmt.typename = default("Ieee80211MgmtAdhoc");
        wlan0.radio.antenna.mobilityModule = absPath(".mobility");
        wlan1.radio.antenna.mobilityModule = absPath(".mobility");
        udp.icmpModule = absPath(".icmp");
        
        // Cellular configuration
        string cellularNicType = default("NRNicUe");  // Can be "LteNicUe" or "NRNicUe"
        string nodeType = default("UE");  // UE for cellular connectivity
        int masterId @mutable = default(0);
        int macNodeId @mutable = default(0); 
        int macCellId @mutable = default(0); 
        
        // NR-specific parameters (used when cellularNicType = "NRNicUe")
        int nrMasterId @mutable = default(1);
        int nrMacNodeId @mutable = default(0);
        int nrMacCellId @mutable = default(1);
        
        // RNI Support for MEC applications
        // TODO: understand
        bool hasRNISupport = default(false);
        cellularNic.mac.collectorModule = hasRNISupport ? absPath(".ueCollector") : "";
        cellularNic.phy.hasCollector = hasRNISupport;
        cellularNic.phy.dynamicCellAssociation = true;

        // Node's external IP address (can be used for emulation mode in simu)
        // Needs to be set
        string extHostAddress = default("");
    
        
    gates:
        input radioIn @directIn;     // connection to cellular base station
        input nrRadioIn @directIn;   // connection to NR base station (for NR mode)
        
    submodules:
        mobility: MosaicMobility {
            @display("p=550,400");
        }
        
        // WiFi interfaces (existing)
        wlan0: Ieee80211Interface {
            parameters:
                @display("p=149,650;q=queue");
        }
        wlan1: Ieee80211Interface {
            parameters:
                @display("p=300,650;q=queue");
        }
        
        // Cellular interface (new)
        cellularNic: <cellularNicType> like ICellularNic {
            nodeType = parent.nodeType;
            @display("p=450,650");
        }
        
        interfaceTable: InterfaceTable {
            parameters:
                @display("p=62,400;is=s");
        }
        
        // routingTable: Ipv4RoutingTable {
        //     parameters:
        //         @display("p=62,320;is=s");
        // }
        
        // Network layer with auto-configuration
        networkLayer: Ipv4NetworkLayer {
            parameters:
                // configurator.networkConfiguratorModule = "";
                @display("p=320,500;q=queue");
        }
        
        // Message dispatcher for all interfaces
        md: MessageDispatcher {
            parameters:
                @display("p=320,580;b=500,5,,,,1");
        }
        
        icmp: Icmp {
            @display("p=550,320");
        }
        
        udp: Udp {
            @display("p=320,400");
        }
        
        // Routing protocol (optional)
        gpsr: <routingProtocol> like IManetRouting if routingProtocol != "" {
            @display("p=500,500");
        }
        
        proxyApp: MosaicProxyApp {
            @display("p=320,250");
        }
        
        // Stats collector for MEC/RNI support
        ueCollector: UeStatsCollector if hasRNISupport {
            @display("p=62,500;is=s");
        }
        
    connections allowunconnected:
        // WiFi interface connections
        wlan0.upperLayerOut --> md.in++;
        wlan0.upperLayerIn <-- md.out++;
        
        wlan1.upperLayerOut --> md.in++;
        wlan1.upperLayerIn <-- md.out++;
        
        // Cellular interface connection
        cellularNic.upperLayerOut --> md.in++;
        cellularNic.upperLayerIn <-- md.out++;
        
        // Network layer connections
        md.out++ --> networkLayer.ifIn;
        md.in++ <-- networkLayer.ifOut;
        
        // Routing connections
        networkLayer.transportOut --> gpsr.ipIn if routingProtocol != "";
        networkLayer.transportIn <-- gpsr.ipOut if routingProtocol != "";
        networkLayer.transportOut --> udp.ipIn;
        networkLayer.transportIn <-- udp.ipOut;
        
        // Application connections
        udp.appOut --> proxyApp.socketIn;
        udp.appIn <-- proxyApp.socketOut;
        
        // Radio connections
        cellularNic.radioIn <-- radioIn;
        cellularNic.nrRadioIn <-- nrRadioIn if cellularNicType == "NRNicUe";
}