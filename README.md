# Aero Desktop

<p align="center">
  <strong>A modern desktop environment for DreamyOS.</strong>
  <br>
  A beautiful, fluid and modular desktop experience built with C++ and Qt.
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Status-Early%20Development-9B7BFF?style=for-the-badge" />
  <img src="https://img.shields.io/badge/Language-C%2B%2B20-6C9EFF?style=for-the-badge" />
  <img src="https://img.shields.io/badge/Framework-Qt%206-41CD52?style=for-the-badge" />
  <img src="https://img.shields.io/badge/Platform-DreamyOS-9B7BFF?style=for-the-badge" />
</p>

---

## Overview

**Aero Desktop** is a modern graphical desktop environment designed as part of the DreamyOS ecosystem.

Built with C++20 and Qt 6, Aero aims to combine elegant visual design, smooth animations and a modular architecture into one cohesive desktop experience.

Inspired by modern desktop environments, Aero focuses on simplicity, fluidity and visual consistency without sacrificing functionality.

The project is being developed with platform independence in mind, allowing its interface and core components to be developed and tested separately from the DreamyOS kernel.

## Design philosophy

Aero Desktop follows a visual direction centered around **Liquid Glass**.

Key design principles:

- Minimal and elegant interface.
- Translucent surfaces and layered UI.
- Smooth and expressive animations.
- Rounded corners and carefully balanced spacing.
- Adaptive light and dark themes.
- Purple and violet accent colors.
- Consistent design across system applications.

The goal is to create an environment that feels cohesive, responsive and enjoyable to use.

## Features

### Desktop experience

- [ ] Desktop workspace
- [ ] Top Bar
- [ ] Application Dock
- [ ] Control Center
- [ ] Spotlight-style search
- [ ] Workspace management
- [ ] Notification Center
- [ ] Hot Corners
- [ ] Desktop widgets

### Window management

- [ ] Window management system
- [ ] Window animations
- [ ] Resizing and snapping
- [ ] Virtual desktops
- [ ] Multi-window support

### System applications

Planned native applications:

- [ ] Aero Finder
- [ ] Aero Terminal
- [ ] System Settings
- [ ] Activity Monitor
- [ ] Calculator
- [ ] Calendar
- [ ] Text Editor
- [ ] Archive Manager

### System integration

- [ ] Display management
- [ ] Audio management
- [ ] Network management
- [ ] Bluetooth support
- [ ] Power management
- [ ] User sessions
- [ ] Notifications
- [ ] System updates

*Features are planned and may change during development.*

## Technology stack

| Technology | Purpose |
|---|---|
| C++20 | Core implementation |
| Qt 6 Widgets | User interface |
| Qt Core | Application infrastructure |
| Qt Gui | Graphics and rendering |
| QPainter | Custom interface rendering |
| Qt Animation Framework | UI animations |
| CMake | Build system |

Aero uses a code-first approach.

**QML, Qt Quick and Qt Designer are not part of the project's UI architecture.**

## Architecture

Aero Desktop is designed to remain independent of the underlying operating system implementation.

The project uses an abstraction layer called `AeroPlatform`, which separates desktop functionality from platform-specific services.

This allows Aero to use different backends depending on the environment.

### Development backend

During development, Aero can run as a standalone application on Linux.

This makes it possible to test the interface, applications and desktop components without modifying the host operating system.

### DreamyOS backend

A dedicated backend is planned for native integration with DreamyOS.

It will connect Aero to the operating system's services, including display management, input handling, processes, audio and system notifications.

## Development

### Requirements

- C++20-compatible compiler
- CMake
- Qt 6
- Ninja (recommended)
- Git

### Build on Fedora

Install the required dependencies:

```bash
sudo dnf install gcc-c++ cmake ninja-build git
sudo dnf install qt6-qtbase-devel qt6-qtsvg-devel
```

Clone the repository:

```bash
git clone https://github.com/USERNAME/Aero-Desktop.git
cd Aero-Desktop
```

Build the project:

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

Run the application:

```bash
./build/AeroDesktop
```

*Build instructions may change as the project evolves.*

## Project structure

The planned project structure:

```text
Aero-Desktop/
├── CMakeLists.txt
├── README.md
├── assets/
│   ├── icons/
│   ├── wallpapers/
│   └── fonts/
├── src/
│   ├── core/
│   ├── platform/
│   ├── desktop/
│   ├── compositor/
│   ├── widgets/
│   ├── applications/
│   └── main.cpp
└── docs/
    └── architecture/
```

## Project status

Aero Desktop is currently in early development.

The initial development phase focuses on:

1. Establishing the core architecture.
2. Building the visual design system.
3. Implementing reusable interface components.
4. Developing the standalone Linux backend.
5. Creating the first system applications.
6. Preparing for native DreamyOS integration.

The architecture and feature set are subject to change.

## Relationship with DreamyOS

Aero Desktop is a core component of the DreamyOS ecosystem, but it is developed as a separate project.

This separation allows the desktop environment to evolve independently while maintaining a clear integration boundary with the operating system.

**DreamyOS provides the system. Aero provides the experience.**

## License

The project license has not yet been finalized.

---

<p align="center">
  <strong>Aero Desktop</strong>
  <br>
  Designed for DreamyOS.
  <br>
  <sub>Built with passion, C++ and a little bit of purple.</sub>
</p>
