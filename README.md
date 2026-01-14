# ESP Private Agents Firmware

The ESP Private Agents Platform (<https://agents.espressif.com>) is a platform that allows building and hosting AI Agents for your organisation (more in the [blog](https://developer.espressif.com/blog/2025/12/annoucing_esp_private_agents_platform/)). The Agents Platform can be used to create conversational AI Agents that you can communicate with using an Espressif powered device. This repository contains the firmware SDK and examples that implement the device side features for communicating with these agents.

## Examples

The examples in this repository primarily pull together the display, mic, speaker, etc. in a meaningful way. Additionally they have some local tools that the agent can execute. These local tools run on the device itself.

The firmware can talk to any agent (with matching tools) but they are pre-configured with some "Default Agents". The agent can be changed by replacing the default agent with your own custom agent created through the ESP Private Agents Platform.

### Voice Chat (`examples/voice_chat/`): Generic Agent Firmware

This is a generic firmware that is expected to work with most agents created with ESP Private Agents Platform. This firmware supports local tools like set_emotion, set_volume, set_reminder, get_local_time. You may refer [this](examples/voice_chat/README.md) for more details.

Default Agent: `friend`

### Matter Controller (`examples/matter_controller/`): Matter Controller + Thread Border Router Firmware

This firmware supports Matter Controller functionality and Thread Border Router functionality, apart from the common Agents functionality that is described above. It supports tools like get_device_list, set_volume, set_emotion, set_reminder, get_local_time. You may refer [this](docs/firmwares/matter_controller.md) for more details. Users can control other compatible Matter devices using natural voice commands

Default Agent: `matter_controller`

## Supported Boards

The examples support the following boards out of the box:

1. **EchoEar Core Board v1.2**
2. **ESP-BOX-3**
3. **M5Stack-CoreS3**
4. **M5Stack-CoreS3 + M5Stack Module Gateway H2** (with H2 Module for Thread Border Router functionality)

## Try Now

You can head over to [examples](examples/) to build and flash the examples yourself.

Or, the fastest way to try out ESP Private Agents firmware is to flash the examples using ESP Launchpad from the links below:

### Pre-built Images

Flash the examples using ESP Launchpad from the links below and then refer the [Setup Guide](docs/setup_guide.md) for the next steps.

<table>
  <tr>
    <th align="center">Device</th>
    <th align="center">Agent: Friend <br> (Based on the Voice Chat Example)</th>
    <th align="center">Agent: Matter Controller <br> (Based on the Matter Controller Example)</th>
  </tr>
  <tr>
    <td align="center">
      <img src="https://github.com/espressif/esp-agents-firmware/wiki/images/EchoEarListening.jpeg" alt="EchoEar" width="100"><br>
      EchoEar
    </td>
    <td align="center">
      <a href="https://espressif.github.io/esp-launchpad/minimal-launchpad/?flashConfigURL=https://espressif.github.io/esp-agents-firmware/voice_chat/echoear_core_board_v1_2/launchpad.toml" target="_blank">Flash Now</a>
    </td>
    <td align="center">
      <a href="https://espressif.github.io/esp-launchpad/minimal-launchpad/?flashConfigURL=https://espressif.github.io/esp-agents-firmware/matter_controller/echoear_core_board_v1_2/launchpad.toml" target="_blank">Flash Now</a>
    </td>
  </tr>
  <tr>
    <td align="center">
      <img src="https://github.com/espressif/esp-agents-firmware/wiki/images/ESPBox3Listening.png" alt="ESP-BOX-3" width="100"><br>
      ESP-BOX-3
    </td>
    <td align="center">
      <a href="https://espressif.github.io/esp-launchpad/minimal-launchpad/?flashConfigURL=https://espressif.github.io/esp-agents-firmware/voice_chat/esp_box_3/launchpad.toml" target="_blank">Flash Now</a>
    </td>
    <td align="center">
      <a href="https://espressif.github.io/esp-launchpad/minimal-launchpad/?flashConfigURL=https://espressif.github.io/esp-agents-firmware/matter_controller/esp_box_3/launchpad.toml" target="_blank">Flash Now</a>
    </td>
  </tr>
  <tr>
    <td align="center">
      <img src="https://github.com/espressif/esp-agents-firmware/wiki/images/M5StackCoreS3Listening.png" alt="M5Stack CoreS3" width="100"><br>
      M5Stack CoreS3
    </td>
    <td align="center">
      <a href="https://espressif.github.io/esp-launchpad/minimal-launchpad/?flashConfigURL=https://espressif.github.io/esp-agents-firmware/voice_chat/m5stack_cores3/launchpad.toml" target="_blank">Flash Now</a>
    </td>
    <td align="center">
      <a href="https://espressif.github.io/esp-launchpad/minimal-launchpad/?flashConfigURL=https://espressif.github.io/esp-agents-firmware/matter_controller/m5stack_cores3/launchpad.toml" target="_blank">Flash Now</a>
    </td>
  </tr>
  <tr>
    <td align="center">
      <img src="https://github.com/espressif/esp-agents-firmware/wiki/images/M5StackCoreS3Listening.png" alt="M5Stack CoreS3" width="100"><br>
      M5Stack CoreS3 + M5Stack Module Gateway H2
    </td>
    <td align="center">
      <a href="https://espressif.github.io/esp-launchpad/minimal-launchpad/?flashConfigURL=https://espressif.github.io/esp-agents-firmware/voice_chat/m5stack_cores3_h2_gateway/launchpad.toml" target="_blank">Flash Now</a>
    </td>
    <td align="center">
      <a href="https://espressif.github.io/esp-launchpad/minimal-launchpad/?flashConfigURL=https://espressif.github.io/esp-agents-firmware/matter_controller/m5stack_cores3_h2_gateway/launchpad.toml" target="_blank">Flash Now</a>
    </td>
  </tr>
</table>
