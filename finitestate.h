/*
    This file is part of Kismet

    Kismet is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kismet is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Kismet; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef __FINITESTATE_H__
#define __FINITESTATE_H__

#include "config.h"

#include <time.h>
#include <sys/time.h>

#include "globalregistry.h"
#include "alertracker.h"

// Finite state automata superclass which handles a category of tracking conditions.
// It's possible that there can be multiple state machines of a single category
// (ie, tracking multiple potential quesitionable MACs); the finite_automata is
// responsible for handling these in whatever sane manner is necessary and for
// timing out old connections.
class finite_automata {
public:
    // An individual state element
    class _fsa_element {
    public:
        _fsa_element() {
            last_time.tv_sec = start_time.tv_sec = death_time.tv_sec = 0;
            last_time.tv_usec = start_time.tv_usec = death_time.tv_usec = 0;
            state = 0;
            counter = 0;
        }

        struct timeval last_time;
        struct timeval start_time;
        struct timeval death_time;
        int state;
        int counter;
    };

    virtual ~finite_automata() { }

    // Handle a packet
    virtual int process_packet(const packet_info *in_info) = 0;

    int fetch_alert_ref() { return alertid; }

protected:
    global_registry *globalreg;
    int alertid;
};

// Finite state automata to watch people who probe and never exchange data after an association
class dot11_probe_nojoin_automata : public finite_automata {
public:
    dot11_probe_nojoin_automata(global_registry *in_globalreg, alert_time_unit in_unit, 
                        int in_rate, int in_burstrate);
    ~dot11_probe_nojoin_automata();

    // States:
    // State 0: Probe only
    // State 1: Probe and response seen
    // State 2: Normal user, probe response and data seen

    // Threshold if state == 2 && counter is over threshold

    int process_packet(const packet_info *in_info);

protected:
    // Map of probing clients to responding people.  If the client sends any "normal" data
    // destined to that network, we reset them.
    map<mac_addr, _fsa_element *> bssid_map;
};

// FSA to look for a disassociate/deauth from a client who then keeps talking.  This is
// suspicious behavior.  Based on "802.11 Denial-of-Service Attacks:  Real Vulnerabilities
// and Practical Solutions", Bellardo, J. and Savage, S.
class DisassocTrafficAutomata : public finite_automata {
public:
    DisassocTrafficAutomata(global_registry *in_globalreg, alert_time_unit in_unit, 
                        int in_rate, int in_burstrate);
    ~DisassocTrafficAutomata();

    int process_packet(const packet_info *in_info);
protected:
    // State 0 - got a disassoc
    // State 1 - got a deauth
    map<mac_addr, _fsa_element *> source_map;
};

// FSA to look for spoofing via BSS timestamp. 
// BSS timestamps are monotonically increasing within the BSSID for all times they're defined
// An invalid timestamp increases us by 10, a valid timestamp decreases by one.  This is a 
// cheap way to keep track of how much we're flapping - we don't want a reboot of an AP to
// generate an alert, but we DO want a spoofed AP beaconing in the same space to generate
// one.
class dot11_bss_timestamp_automata : public finite_automata {
public:
    class _bs_fsa_element : public finite_automata::_fsa_element {
    public:
        _bs_fsa_element() {
            bss_timestamp = 0;
        }

        uint64_t bss_timestamp;
    };

    dot11_bss_timestamp_automata(global_registry *in_globalreg, alert_time_unit in_unit, 
                         int in_rate, int in_burstrate);
    ~dot11_bss_timestamp_automata();

    int process_packet(const packet_info *in_info);

protected:
    macmap<dot11_bss_timestamp_automata::_bs_fsa_element *> bss_map;
};

// Detect broadcast replay WEP attacks by looking for bursts of packets with the same
// IV and ICV
class WepRebroadcastAutomata : public finite_automata {
public:
    WepRebroadcastAutomata(global_registry *in_globalreg, alert_time_unit in_unit, 
                           int in_rate, int in_burstrate);
    ~WepRebroadcastAutomata();

    int process_packet(const packet_info *in_info);

protected:
    class _wreb_element : public _fsa_element {
        // Just add a wep field tracker
        uint32_t wepfield;
    };

    map<mac_addr, _wreb_element *> source_map;

};

#if 0
// This doesn't really work so we won't use it.
// Finite state automata to watch sequence numbers
class dot11_sequence_spoof_automata : public finite_automata {
public:
    dot11_sequence_spoof_automata(Packetracker *in_ptracker, alert_tracker *in_atracker,
                          alert_time_unit in_unit, int in_rate, int in_burstrate);
    ~dot11_sequence_spoof_automata();

    int process_packet(const packet_info *in_info);

protected:
    // State 0 - Undefined source
    // State 1 - Source with beacons only
    // State 2 - Source with real traffic

    map<mac_addr, _fsa_element *> seq_map;
};
#endif

#endif
