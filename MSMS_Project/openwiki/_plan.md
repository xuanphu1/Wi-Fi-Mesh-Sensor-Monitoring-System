# OpenWiki Documentation Plan

## Core Documentation Structure
1. **Quickstart** (`/openwiki/quickstart.md`)
   - Project overview (from README)
   - Key components and architecture
   - Getting started guide
   - Links to detailed sections

2. **Architecture** (`/openwiki/architecture/`)
   - Layered architecture overview
   - Component diagram (reference to .puml files)
   - Core system initialization flow

3. **UI System** (`/openwiki/ui/`)
   - Menu system structure
   - Screen management
   - Button handling

4. **Sensor Management** (`/openwiki/sensors/`)
   - Registry pattern
   - Adding new sensors
   - Data flow

5. **Network** (`/openwiki/network/`)
   - Wi-Fi AP/STA configuration
   - Web interface
   - Mesh networking (future)

6. **Development** (`/openwiki/development/`)
   - Build system
   - Adding new components
   - Coding conventions

## Source References
- Main entrypoint: `/main/main.c`
- Core components: `/component/core/`
- UI components: `/component/ui/`
- Network config: `/INTEGRATE_ESP_MDF.md`

## Open Questions
- Current status of mesh networking feature
- Any planned major architectural changes
- Production deployment considerations