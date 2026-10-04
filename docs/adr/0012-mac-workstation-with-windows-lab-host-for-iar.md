---
status: accepted
date: 2026-10-03
decision-makers: jlurg
---

# Mac workstation with Windows Lab Host for IAR (VS Code Remote-SSH)

## Context and Problem Statement

The IAR licence (EWARM, C-STAT and Visual State) is node-locked to one Windows PC; no cloud licence is available. Daily work happens on a MacBook, which runs git, Docker (ESP-IDF and Ceedling), Flutter, Python and the Arm GCC toolchain from STM32CubeCLT. IAR Visual State has no macOS host. The HIL bench is connected to the same Windows PC. The IAR Build and IAR C-SPY Debug extensions for VS Code run where the toolchain is installed. How is DCU work with IAR organised?

## Decision Drivers

- The node-locked licence stays on the Lab Host.
- Native editing experience on the Mac.
- Debugging on the bench NUCLEO connected to the Lab Host.
- Access to the graphical tools (Visual State, IAR IDE options, C-STAT triage).
- Reproducible, official IAR results independent of the developer's session.
- One source of truth: the git repository.

## Considered Options

- Mac workstation with VS Code Remote-SSH to a clone on the Lab Host, remote desktop for graphical tools, and a self-hosted runner for official builds
- Windows PC as the only workstation
- Remote desktop for all IAR work
- Windows virtual machine on the Mac

## Decision Outcome

Chosen option: "Mac workstation with VS Code Remote-SSH to a clone on the Lab Host, remote desktop for graphical tools, and a self-hosted runner for official builds", because it keeps the licence, the toolchain and the debug probe on the Lab Host while editing happens in a native Mac window. The maintainer has verified this setup with the IAR extensions before M0.

- Primary IAR flow: VS Code on the Mac, Remote-SSH to the developer clone on the Lab Host, IAR Build and IAR C-SPY Debug extensions on the remote side.
- Remote desktop (Windows App on the Mac, Windows 11 Pro host): Visual State modelling, Validator, Documenter and C-SPYLink animation; IAR IDE project options; C-STAT triage.
- Official verification: every push runs the IAR build and C-STAT on the Lab Host runner and reports `iar-gate`.
- All other work stays on the Mac: ESP-IDF in a container, Flutter, Ceedling in Docker, the GCC shadow build of the DCU, Python.
- A branch is edited in one clone at a time; before switching machines, commit and push.

### Consequences

- Good, because the licence is used where it is installed and the bench is attached.
- Good, because the GCC shadow build and Ceedling keep cloud CI and local tests independent of the licence.
- Bad, because two clones must be kept in sync by discipline.
- Bad, because the Lab Host is a single point of failure for IAR work, HIL and runners.
- Bad, because a Windows client edition allows one interactive session, so remote desktop sessions must be coordinated with the HIL runner session.

### Confirmation

The Lab Host day-1 checks in [Lab Host](../08_process/lab_host.md) record the licence, C-STAT, Visual State and extension results. `iar-gate` reports on every DCU-relevant push.

## Pros and Cons of the Options

### Mac workstation with Remote-SSH, remote desktop and a runner

- Good, because each tool runs where it works best.
- Neutral, because it depends on the network connection to the Lab Host.

### Windows PC as the only workstation

- Good, because everything is local to the licence.
- Bad, because the Mac toolchains (Xcode for iOS, the existing ESP-IDF and CubeCLT setup) would be duplicated or lost.

### Remote desktop for all IAR work

- Good, because it needs no extension support.
- Bad, because editing over a remote desktop is slower, and it occupies the single interactive session.

### Windows virtual machine on the Mac

- Bad, because the licence is node-locked to the Lab Host and the bench is attached there.

## More Information

- [Lab Host](../08_process/lab_host.md)
- [ADR 0004: Lab Host self-hosted runner trust model](0004-lab-host-self-hosted-runner-trust-model.md)
- [VS Code: remote development using SSH](https://code.visualstudio.com/docs/remote/ssh)
