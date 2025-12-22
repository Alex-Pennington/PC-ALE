# PC-ALE 2.0 Clean-Room Development Prospectus

**Repository:** https://github.com/Alex-Pennington/PC-ALE

**Project:** Phoenix Nest PC-ALE 2.0  
**Author:** Alex Pennington, AAM402/KY4OLB  
**Date:** 22 December 2025  
**Version:** Draft 1.0

---

## Executive Summary

Clean-room reimplementation of PC-ALE for amateur radio use, built entirely from public MIL-STD documents using AI-assisted development. No proprietary code. Modern cross-platform architecture.

**Target Platforms:** Windows 11, Debian 13, Raspberry Pi 5, macOS  
**License:** Open Source (MIT or GPL v2)  
**Relationship to MARS:** Separate codebase, interoperable output

---

## Philosophy: Community-Driven Development

### This Is Your Project

Let's be clear about something: **I don't have a dog in this fight.**

I don't care what PC-ALE 2.0 looks like in the end. I don't have a personal agenda for features. I'm not trying to build my vision of what ALE software should be - I'm trying to build **your** vision. The amateur radio community's vision.

This project exists because:
1. ION2G has problems and limited development
2. PC-ALE 1.x is abandonware
3. MARS has excellent tools locked behind membership requirements
4. The ham community deserves modern, open, cross-platform ALE software

### How Features Get Prioritized

Simple: **People who fund development get their features implemented.**

Not exclusively - everything is open source and shared with everyone. But if you contribute $50 toward development costs and say "I really need Elecraft K3 CAT support," that moves up the priority list. If someone else contributes $100 and says "I need VARA-compatible ARQ," we look at that seriously.

This isn't pay-to-play gatekeeping. It's practical resource allocation. AI compute costs money. The people investing in the project get input on direction. The code belongs to everyone.

### Multiple Ways to Contribute

Funding isn't the only way to participate:

| Contribution Type | What It Looks Like |
|-------------------|-------------------|
| **Direct Funding** | Sponsor AI development costs via GitHub Sponsors or PayPal |
| **AI-Assisted Development** | Use your own API access and dev environment to implement features, submit pull requests |
| **Testing** | Beta test releases, report bugs, validate on your hardware |
| **Documentation** | Write tutorials, improve user guides, translate to other languages |
| **Hardware Loans** | Loan radios or SDRs for compatibility testing |
| **OTA Testing** | Participate in on-air interop tests, provide signal recordings |
| **Code Review** | Review pull requests, audit security, improve code quality |

**The AI-assisted development path is real.** If you have your own API access and a coding environment set up, you can take a GitHub issue, work through the implementation, and submit a PR. You're covering your own compute costs and contributing code - that's valuable.

---

## Proven Track Record: What We've Already Built

This isn't a proposal for something that might happen. This is a status report on something that's **already happening.**

Over the past 3-4 months, Phoenix Nest and Claude have developed a working partnership that has produced real, functional code. Here's what that looks like:

### Phoenix Nest MARS Suite - MIL-STD-188-110A Implementation

**The crown jewel.** A complete clean-room implementation of the MIL-STD-188-110A PSK modem, built entirely from the public standard document with zero code derived from the Brain Core or any other proprietary implementation.

| Metric | Status |
|--------|--------|
| Overall Completion | 65.6% |
| Test Coverage | 924 tests passing |
| Waveform Modes | All 12 modes (75-4800 bps) |
| Interop Status | ~95% compatible with Brain Core |

**What this means:** We can generate 188-110A waveforms that MARS-ALE and MS-DMT can decode. We can decode waveforms they generate. This is the foundation everything else builds on.

### How Clean-Room Development Works With MARS Library Access

As MARS librarian (AAM402), I have access to the proprietary MARS source code library - including the Brain Core (Cm110s), MARS-ALE, and decades of accumulated HF modem implementations.

**What I cannot do:** Copy code, port implementations, or derive work from proprietary sources.

**What I can do:** Study algorithms, understand approaches, validate interoperability, and use it as a reference to verify that my independent implementation produces compatible output.

This is the same model as Linux vs. Unix, or AMD vs. Intel. You can study a reference implementation to understand what correct behavior looks like, then build your own implementation from public specifications. The key is that no code crosses the boundary.

The Brain Core tells me "this is what a correct 188-110A waveform looks like." The MIL-STD document tells me how to build one. My implementation is built from the MIL-STD, validated against the Brain Core.

### Repository Architecture

The project started as a monolithic codebase. Claude helped restructure it into seven focused, maintainable modules:

| Repository | Purpose |
|------------|---------|
| `phoenix-kiss-fft` | FFT library (forked, optimized) |
| `phoenix-reference-library` | MIL-STD documents and references |
| `phoenix-sdr-core` | SDR abstraction layer |
| `phoenix-waterfall` | Spectrum visualization |
| `phoenix-wwv` | WWV time signal detection |
| `phoenix-sdr-net` | Network streaming protocol |
| `phoenix-sdr-utils` | Common utilities |

This isn't just organization for organization's sake. It means components can be reused independently. Someone building a different HF project can pull just the FFT library or just the waterfall display without dragging in modem code.

### WWV Time Signal Detection

A complete subsystem for detecting and decoding WWV/WWVH time signals. Why? Because accurate timing matters for ALE, and WWV provides a free, always-available reference that doubles as a smoke test for the SDR receive chain.

**Technical achievements:**
- BCD time code decoding
- Pulse detection algorithms with sub-second accuracy
- Automated parameter optimization achieving **250x improvement** in tick timing consistency
- Foundation for future NTP/WWV hybrid timing

### SDR Integration

Direct integration with SDRplay RSP2 Pro hardware:
- IQ streaming pipeline
- Waterfall visualization
- Signal analysis tooling for modem development
- Currently resolving API linking (the stub header issue being worked today)

### Real-Time MELP Voice Streaming

Steve Hajducek highlighted this in his announcement to the groups: **"He now has the vocoding streaming on RX in near-real time instead of waiting until MS110A EOM received which is HUGE."**

Previous implementations buffered the entire transmission before decoding voice. Ours streams in real-time. This matters for usability - you hear the other station as they talk, not after they finish.

### Raspberry Pi 3 Bare-Metal MS110A

Steve called this "a real feat" - and it is. Getting military serial-tone modems running on a Pi3 without a full Linux OS required:
- Bare-metal programming approach
- Careful optimization for limited resources
- Proof that embedded ALE is achievable on cheap, available hardware

This opens the door to battery-powered, portable ALE nodes that don't need a laptop.

### MARS Source Code Library Organization

As MARS librarian (AAM402), we reorganized decades of accumulated source code:

| Metric | Before | After | Reduction |
|--------|--------|-------|-----------|
| Files | 5,703 | 3,093 | 46% |
| Size | 4.67 GB | 359 MB | 92% |

Created comprehensive documentation:
- INDEX.md - Complete library catalog
- TECHNICAL_REFERENCE.md - Algorithm deep-dives
- MARS-ALE_Architecture.md - 11-layer system breakdown

This isn't just housekeeping. It's preserving institutional knowledge and making it accessible for future development.

### Beta Testing Infrastructure

Built out the framework for community testing:
- Formal beta tester documentation
- GitHub issue templates
- Structured feedback collection
- Version-controlled release process

### AI Development Workflow

Perhaps most importantly, we've developed a **repeatable process** for AI-assisted DSP development:

1. Start with the MIL-STD specification
2. Break into implementable components
3. Iterative development with AI assistance
4. Continuous testing against known-good reference (Brain Core)
5. Documentation as we go

This workflow is transferable. It's how PC-ALE 2.0 will be built. It's how anyone with a local AI can contribute.

### The Numbers

Over 3-4 months of development:
- **~$600** in AI API costs (actual, tracked)
- **Hundreds of hours** of AI-assisted coding sessions
- **Thousands of lines** of production code
- **Multiple working subsystems** ready for integration

**This is not vaporware. This is not a proposal. This is a progress report.**

---

## How AI-Assisted Development Actually Works

For those unfamiliar with AI-assisted coding, here's what the process looks like in practice:

### The Development Loop

```
1. Human identifies task: "Implement Golay (24,12) FEC encoder"
2. Human provides context: MIL-STD spec, input/output requirements, test vectors
3. AI generates implementation: First-pass code, often 80-90% correct
4. Human reviews and tests: Catches edge cases, integration issues
5. Human provides feedback: "The syndrome calculation is wrong for these inputs"
6. AI corrects: Targeted fixes based on specific feedback
7. Iterate until working: Usually 2-5 cycles for complex DSP
8. Human integrates: Merge into larger system, write tests
```

### What AI Is Good At

- **Boilerplate generation** - Repetitive code patterns, file structures
- **Algorithm implementation** - Translating math from specs to code
- **Documentation** - Comments, README files, API docs
- **Refactoring** - Restructuring existing code cleanly
- **Debugging assistance** - Analyzing error messages, suggesting fixes
- **Test generation** - Creating test cases from specifications

### What AI Needs Human Guidance For

- **Architecture decisions** - Overall system design
- **Domain expertise** - "That's not how HF propagation works"
- **Hardware realities** - "This needs to run on a Pi3 with 1GB RAM"
- **Community requirements** - "Hams need this to work with Icom CI-V"
- **Integration** - Connecting components into working systems
- **Validation** - Confirming output matches real-world expectations

### The Economics

Traditional development: One developer writes ~50-200 lines of production code per day.

AI-assisted development: Same developer can produce 500-2000 lines per day, with the AI handling the typing and the human handling the thinking.

But the AI isn't free. API costs roughly:
- $15/million input tokens (reading your code and specs)
- $75/million output tokens (generating code)

A complex DSP module might consume 100K-500K tokens. That's $2-10 per module. Multiply by dozens of modules and you get real costs.

### Running Your Own AI Development Environment

You don't have to go through Phoenix Nest to contribute code. If you have:
- Your own API access
- A development environment (VSCode with AI extensions, Claude desktop with tools enabled, etc.)

...you can take a GitHub issue, work through the implementation yourself, and submit a pull request. You're covering your own compute costs and contributing code. That's valuable, and that's welcome.

---

## Scope Definition

### In Scope (Clean-Room)

| Module | Standard | Status | Complexity |
|--------|----------|--------|------------|
| 2G ALE (8-FSK) | MIL-STD-188-141 | New | Medium |
| AQC-ALE | Rockwell spec | New | Medium |
| 188-110A Modem | MIL-STD-188-110A | **DONE** (Pennington Core) | - |
| FS-1052 ARQ | Federal Standard 1052 | New | High |
| Modern Qt6 UI | - | New | Medium |
| SDR Integration | SDRplay API | **In Progress** | Medium |
| Radio CAT Control | Various | New | Low-Medium |
| HF Channel Simulator | Watterson Model | Port IONOS (MIT) | Low |

### Out of Scope (Patent/License Issues)

| Module | Reason | Alternative |
|--------|--------|-------------|
| MELPe Voice | TI Patent | Codec2 (open source) or license |
| Brain Core (Cm110s) | Proprietary | Use Pennington Core |
| MARS-ALE UI Code | Proprietary | Clean-room Qt6 |

### Reference Only (No Code Derivation)

| Resource | Use |
|----------|-----|
| LinuxALE (GPL) | Algorithm reference for 8-FSK |
| IONOS Simulator (MIT) | Can port directly |
| MIL-STD Documents | Primary specification source |
| Brain Core | Interop testing only |

---

## Module Breakdown & AI Cost Estimates

### 1. 2G ALE Modem (MIL-STD-188-141)

**Deliverables:**
- 8-FSK modulator/demodulator
- Sliding FFT implementation
- Golay (24,12) FEC encoder/decoder
- Trellis decoder
- Word structure parser (preambles, addresses)
- Scanning/calling state machine

**Estimated AI Cost:** $150-250

---

### 2. AQC-ALE (Rockwell Enhanced AFSK)

**Deliverables:**
- Enhanced AFSK modem
- Improved acquisition
- Better noise immunity than standard 2G

**Estimated AI Cost:** $75-125

---

### 3. FS-1052 Data Link Protocol

**Deliverables:**
- ARQ state machine
- Frame formatting (Variable ARQ, Broadcast, Circuit, Fixed ARQ)
- ACK/NAK handling
- Auto rate negotiation (2400→300 bps)
- Retransmit logic

**Estimated AI Cost:** $175-275

---

### 4. Qt6 Modern UI

**Deliverables:**
- Cross-platform Qt6 application shell
- Waterfall/spectrum display
- Channel status panel
- Call history/logging
- Configuration dialogs
- Radio control interface

**Estimated AI Cost:** $100-175

---

### 5. Radio CAT Control

**Deliverables:**
- Serial/USB CAT interface
- Frequency/mode control
- PTT control
- Support for 15+ common radios (Icom, Yaesu, Kenwood, Elecraft)

**Estimated AI Cost:** $75-125

---

### 6. SDR Integration

**Deliverables:**
- SDRplay RSP2/RSPdx support
- RTL-SDR support (stretch)
- Direct IQ to modem pipeline
- Waterfall from SDR

**Status:** In progress (stub header issue being resolved)

**Remaining AI Cost:** $50-100

---

### 7. Testing & Integration

**Deliverables:**
- Unit test suite
- Interop testing with Brain Core (MARS-ALE/MS-DMT)
- HF simulator integration (IONOS port)
- OTA testing procedures

**Estimated AI Cost:** $100-175

---

## Summary: Total Development Cost

### AI API Costs (Realistic Estimates)

Based on actual development experience with the 188-110A Pennington Core and SDR integration work, AI costs are higher than naive estimates suggest. Complex DSP algorithms, protocol state machines, and debugging cycles consume significant tokens.

| Module | AI Cost (Realistic) |
|--------|---------------------|
| 2G ALE Modem | $150-250 |
| AQC-ALE | $75-125 |
| FS-1052 ARQ | $175-275 |
| Qt6 UI | $100-175 |
| Radio CAT | $75-125 |
| SDR Integration | $50-100 |
| Testing/Integration | $100-175 |
| **TOTAL REMAINING** | **$725-1,225** |

**Already Invested:**
- 188-110A Modem (Pennington Core) + SDR work: **~$600** (actual)

**Total Project AI Cost:** **$1,325-1,825**

### Human Time Investment

AI-assisted development dramatically reduces human coding time. The developer's role shifts to:
- Architecture decisions
- Specification interpretation  
- Testing and validation
- Integration oversight
- Community coordination

| Activity | Hours |
|----------|-------|
| Architecture & spec work | 15-20 |
| AI session management | 25-35 |
| Testing & validation | 20-30 |
| Integration & debugging | 15-25 |
| Documentation & community | 10-15 |
| **TOTAL** | **85-125** |

This does NOT include community testing time, which is separate and volunteer-based.

---

## Timeline Estimate

| Phase | Duration | Modules |
|-------|----------|---------|
| Phase 1 | 4-6 weeks | SDR completion, 2G ALE core |
| Phase 2 | 4-6 weeks | FS-1052, AQC-ALE |
| Phase 3 | 3-4 weeks | Qt6 UI, Radio CAT |
| Phase 4 | 2-3 weeks | Integration, Testing |
| **Total** | **13-19 weeks** | Full PC-ALE 2.0 |

*Assumes part-time development (~15-20 hrs/week)*

---

## Risk Factors

| Risk | Mitigation |
|------|------------|
| MIL-STD interpretation ambiguity | Use Brain Core for interop validation |
| SDRplay API complexity | Current work addresses this |
| Qt6 cross-platform issues | Start with Windows, port to Linux |
| MELPe patent | Use Codec2 or defer voice |
| Limited OTA testing | IONOS simulator, coordinate with MARS testers |

---

## Deliverables Summary

**PC-ALE 2.0 Release Package:**
1. `pcale2` - Main executable (Windows/Linux/macOS)
2. `libale` - 2G/AQC ALE modem library
3. `libm110a` - Pennington 188-110A modem (existing)
4. `libfs1052` - ARQ protocol library
5. `libcat` - Radio control library
6. `libsdr` - SDR abstraction layer
7. Documentation (User Manual, API Reference)
8. Test suite

**Repository Structure:**
```
pcale2/
├── src/
│   ├── ale/        # 2G ALE, AQC-ALE
│   ├── modem/      # 188-110A (Pennington)
│   ├── arq/        # FS-1052
│   ├── cat/        # Radio control
│   ├── sdr/        # SDR integration
│   └── ui/         # Qt6 interface
├── tests/
├── docs/
└── tools/          # IONOS simulator port
```

---

## Network Infrastructure: Backend for the Future

### Why Network Nodes Matter

Modern ALE isn't just point-to-point radio. The community needs:
- **Gateway nodes** connecting HF to internet
- **Message relay** for store-and-forward traffic
- **Presence servers** knowing who's online where
- **Logging aggregation** for LQA and propagation analysis

### The Approach

We're not going to over-engineer this upfront. We're going to:

1. **Let the AI figure out the architecture** - Present the requirements, let Claude or your local LLM propose solutions
2. **Guide its progress** - Course-correct when proposals don't fit ham radio realities
3. **Implement incrementally** - Start with basic TCP relay, evolve toward full mesh
4. **Learn from what works** - Winlink, JS8Call, APRS-IS all have lessons to teach

### Potential Components

| Component | Purpose | Priority |
|-----------|---------|----------|
| TCP Relay | Basic node-to-node connectivity | Phase 1 |
| Message Store | Store-and-forward for offline stations | Phase 2 |
| Presence Protocol | "Who's listening on what frequency" | Phase 2 |
| LQA Aggregator | Centralized link quality database | Phase 3 |
| Web Dashboard | Browser-based monitoring | Phase 3 |

### Community Infrastructure

If we need network nodes for backend hauling, **the community provides them.** Just like APRS-IS or Winlink RMS nodes:
- Volunteers run gateway nodes
- Shared infrastructure benefits everyone
- Geographic distribution improves coverage
- Redundancy through community participation

This isn't centralized infrastructure we need to fund. This is distributed infrastructure the community operates.

---

## Funding Model: Community-Supported Development

### The Proposition

Phoenix Nest LLC will:
- Lead the development effort
- Provide the technical expertise and AI workflow
- Organize and coordinate contributors
- Manage the codebase and releases
- Handle documentation and support

The community provides:
- **AI development costs** (~$725-1,225 remaining)
- Beta testing and validation
- OTA testing across diverse HF conditions
- Hardware testing (various radios, SDRs)
- Documentation feedback

### Why This Model?

AI-assisted development is a game-changer for open source HF software. A project that would have taken years with traditional volunteer development can be completed in months. But the AI costs are real - someone has to pay for the compute.

This isn't about profit. The code will be **fully open source**. But $1,000-1,500 in AI API costs is a reasonable ask from a community that has wanted modern ALE software for years.

### Funding Options

| Method | Link | Notes |
|--------|------|-------|
| GitHub Sponsors | github.com/sponsors/[TBD] | Monthly or one-time |
| PayPal | [TBD] | Direct contribution |
| ARRL Foundation | arrl.org/grants | Potential grant application |

### Transparency

All AI development costs will be tracked and reported. Contributors will see exactly where their funding goes. Monthly cost reports will be posted to the PC-ALE group.

### Call to Action

If you want PC-ALE 2.0 to happen:

1. **Join the groups** - PC-ALE, Pi-ALE, P0W-ALE on groups.io
2. **Contribute to development costs** - Any amount helps
3. **Volunteer for testing** - We need diverse HF conditions and hardware
4. **Spread the word** - The more people involved, the faster this happens

This is how open source works in the AI era. The code is free. The compute is not.

---

## Conclusion

**PC-ALE 2.0 is achievable with community support.**

| What | Cost/Effort |
|------|-------------|
| AI Development (remaining) | $725-1,225 |
| AI Development (already invested) | ~$600 |
| Human Hours (remaining) | 85-125 |
| Timeline | 4-5 months part-time |

The 188-110A modem is done. The SDR integration is nearly there. The path forward is clear.

**What we're offering:**
- Leadership and technical expertise
- Proven AI-assisted development workflow
- Commitment to open source release
- Coordination with MARS for interop testing

**What we're asking:**
- Community funding for AI compute costs
- Volunteer testers with diverse hardware
- Patience and engagement as we build this together

The amateur radio community has been asking for modern ALE software for years. ION2G has issues. PC-ALE is stale. MARS has the good stuff locked up.

This is the path to something better. Clean-room. Open source. Modern. Cross-platform.

**Let's build it together.**

---

*Draft prepared 22 December 2025*
*Alex Pennington, AAM402/KY4OLB*
*Phoenix Nest LLC*

*Contact: projects@organicengineer.com*
*GitHub: github.com/Alex-Pennington*
