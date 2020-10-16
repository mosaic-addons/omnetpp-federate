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

import inet.applications.contract.IApp;

//
// Application layer module for sending and receiving of udp messages
// controlled by MOSAIC applications.
//
simple MosaicProxyApp like IApp {
    parameters:
        @class(MosaicProxyApp);
        string localAddress = default("");
        int localPort = default(1001);	// local port for udp binding
        int destPort = default(1001);	// destination port for udp messages
        double maxProcDelay = default(1e-3);	// max val for random delay (synchronous dcf)
    gates:
        input socketIn @labels(UdpControlInfo/up);
        output socketOut @labels(UdpControlInfo/up);
        input fedIn @labels(Federate/up);
        output fedOut @labels(Federate/down);
}
