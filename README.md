# STM32 Multi-Protocol IoT Gateway (Prototype)

A gateway prototype that integrates **UART, I²C, SPI and CAN** field interfaces with an
**MQTT telemetry layer**. It was built on an **STM32F103C8T6 "Blue Pill" (ARM Cortex-M3)**,
with a port to the **STM32F407 Discovery (ARM Cortex-M4)** in progress.

- **UART:** a Raspberry Pi (3.3 V link) acts as the scheduler, changing how often each gateway task runs
- **I²C:** temperature sensor (LM75 / TMP102 compatible)
- **SPI:** external NOR flash used as a circular telemetry log ("black box")
- **CAN:** loopback mode, so the full CAN TX/RX path is exercised with a single board
- **MQTT:** JSON telemetry over a USB-UART adapter to a PC bridge that publishes to Mosquitto

The firmware is split into a **hardware-independent core** (`common/`) shared by every board,
and a thin **board-specific port layer** (`boards/<board>/`) written against the STM32 HAL.

## System architecture

```mermaid
flowchart LR
    subgraph EXT["External devices"]
        PI["Raspberry Pi<br/>scheduler, 3.3 V UART"]
        TMP["Temperature sensor<br/>I2C addr 0x48"]
        FL["SPI NOR flash<br/>W25Qxx"]
    end
    subgraph MCU["STM32 gateway<br/>F103 Blue Pill / F407 Discovery"]
        UP["UART<br/>Pi link, RX interrupt"]
        I1["I2C1<br/>temp task"]
        S1["SPI1<br/>flash log driver"]
        C1["CAN loopback<br/>heartbeat TX, RX interrupt"]
        SCH["gw_sched<br/>task periods"]
        Q["gw_queue<br/>ISR-safe ring, 16 msgs"]
        APP["gw_app loop<br/>command handler, logger, serializer"]
        U2["USART2<br/>uplink"]
    end
    ADP["USB-UART adapter<br/>CP2102 / CH340"]
    subgraph PC["Host PC"]
        BR["mqtt_bridge.py"]
        BROKER["Mosquitto broker"]
        SUB["Subscribers<br/>mosquitto_sub, dashboards"]
    end
    PI <-->|"commands and replies"| UP
    TMP --> I1
    UP --> Q
    I1 --> Q
    C1 --> Q
    S1 -->|"log status"| Q
    Q --> APP
    APP -->|"SCHED updates"| SCH
    SCH -->|"runs periodic tasks"| I1 & C1 & S1
    APP -->|"append 32-byte records"| S1
    S1 <--> FL
    APP -->|"JSON lines"| U2
    U2 --> ADP -->|"USB"| BR --> BROKER --> SUB
```

### Firmware layers

```mermaid
flowchart TB
    APP["Application: common/gw_app.c<br/>drain queue, handle Pi commands, log, publish"]
    CORE["Portable core: common/gw_queue.c, gw_sched.c, gw_serialize.c<br/>no HAL dependency"]
    DRV["Shared driver: common/drv_spi_flash.c<br/>W25Qxx + circular log"]
    IFACE["Port interface: common/gw_port.h"]
    F1["boards/f103-bluepill<br/>gw_port_stm32f1.c"]
    F4["boards/f407-discovery<br/>gw_port_stm32f4.c"]
    HAL["STM32 HAL / CMSIS"]
    HW["UART, I2C, SPI, CAN peripherals"]
    APP --> CORE
    APP --> IFACE
    CORE --> IFACE
    IFACE --> F1
    IFACE --> F4
    F1 --> DRV
    F4 --> DRV
    F1 --> HAL
    F4 --> HAL
    DRV --> HAL
    HAL --> HW
```

### Scheduling command from the Raspberry Pi

```mermaid
sequenceDiagram
    participant P as Raspberry Pi
    participant ISR as UART RX ISR
    participant A as gw_app loop
    participant S as gw_sched
    participant F as SPI flash
    participant M as PC bridge and MQTT
    P->>ISR: SCHED temp 2000
    ISR->>A: line queued as gw_msg_t
    A->>S: gw_sched_command
    S-->>A: OK temp 2000
    A->>P: OK temp 2000
    A->>F: append log record
