STANAG 5066 - Implementation Summary for Phoenix Nest MARS Suite
What It Is
STANAG 5066 (Ed. 3 is current) defines the Subnetwork Interface for HF data communications. Think of it as the "socket layer" between applications and HF radio/modem systems.

Architecture Overview
┌────────────────────────────────────────────────────────┐
│                    Client Applications                  │
├────────────────────────────────────────────────────────┤
│              Subnetwork Interface (S_*)                │  ← Your API boundary
├────────────────────────────────────────────────────────┤
│  ┌──────────────────────────────────────────────────┐  │
│  │        Subnetwork Sublayers:                     │  │
│  │  ┌─────────────────────────────────────────────┐ │  │
│  │  │  Channel Access (ALE integration)           │ │  │
│  │  ├─────────────────────────────────────────────┤ │  │
│  │  │  Data Transfer (ARQ, non-ARQ modes)         │ │  │
│  │  ├─────────────────────────────────────────────┤ │  │
│  │  │  Subnetwork Management                      │ │  │
│  │  └─────────────────────────────────────────────┘ │  │
│  └──────────────────────────────────────────────────┘  │
├────────────────────────────────────────────────────────┤
│              Link/Modem (MIL-STD-188-110)              │
├────────────────────────────────────────────────────────┤
│                     Physical (Radio)                    │
└────────────────────────────────────────────────────────┘

The S_Primitives (Your API)
These are the service primitives applications use. TCP socket connection, then exchange these PDUs:
PrimitivePurposeS_BIND_REQUEST/RESPONSEClient registers with node, gets assigned SAP IDS_UNBIND_REQUEST/RESPONSEClient disconnectsS_UNIDATA_REQUEST/INDICATIONNon-ARQ (unreliable) data transferS_DATA_REQUEST/INDICATIONARQ (reliable) data transferS_EXPEDITED_DATA_*Priority trafficS_MANAGEMENT_*Config, status, control

Key Concepts
1. Node Address

7 bytes, usually maps to ALE address
Format supports group addresses for broadcast

2. Service Access Points (SAPs)

Logical endpoints (like ports)
Client binds to a SAP, receives traffic for that SAP
Rank priority: 0-15 (15 = highest)

3. Delivery Modes
ModeARQBest ForNon-ARQNoBroadcast, time-critical, poor channelsARQYesReliable point-to-pointARQ w/ Selective RepeatYesLonger messages
4. Physical Interface

TCP/IP is standard - typically port 5066
Legacy serial (HDLC framing) still defined but rarely used


PDU Structure (Wire Format)
┌─────────┬────────┬─────────────┬─────────────┐
│ Version │  Type  │   Length    │   Payload   │
│ (4 bit) │(4 bit) │  (16 bit)   │  (variable) │
└─────────┴────────┴─────────────┴─────────────┘
Type values for S_Primitives:

0x01: S_BIND_REQUEST
0x02: S_UNBIND_REQUEST
0x03: S_BIND_ACCEPTED
0x04: S_BIND_REJECTED
0x05: S_UNBIND_INDICATION
0x06: S_HARD_LINK_*
0x07: S_UNIDATA_*
0x08: S_DATA_*
etc.


ALE Integration (Channel Access Sublayer)
This is where 5066 talks to your MIL-STD-188-141 ALE:

Hard Links - Dedicated circuit, ALE establishes and holds
Soft Links - On-demand, ALE connects per-message
Broadcast - No link, just transmit

The sublayer translates S_DATA_REQUEST into:

ALE call (if not already linked)
Modem data burst
ALE termination (if soft link)


What You Need to Implement
Minimum Viable 5066:

TCP listener on port 5066
S_BIND/UNBIND handling
S_UNIDATA (non-ARQ) - easiest starting point
PDU encode/decode

Full Implementation:
5. S_DATA with ARQ state machine
6. Hard/soft link management
7. Multiple simultaneous clients
8. Expedited data queue

Implementation Notes for Phoenix

Separate module - phoenix-stanag-5066 fits your repo structure
Test against known implementations - Harris radios, or the open-source node-hf project had partial 5066
Start with non-ARQ - prove the interface works before tackling ARQ state machine
Document your SAP assignments - define which SAPs map to which MARS applications


Reference Docs

STANAG 5066 Ed. 3 - The full spec (NATO UNCLASSIFIED, findable)
MIL-STD-187-721 - US implementation guidance
STANAG 5066 Annex F - The S_Primitive definitions in detail