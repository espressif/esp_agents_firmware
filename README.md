
# ESP Agents Firmware

The ESP Private Agents Platform (<https://agents.espressif.com>) is a platform that allows building and hosting AI Agents for your organisation. The Agents Platform can be used to create conversational AI Agents that you can communicate with using an Espressif powered device. This repository contains the firmware SDK and examples that implement the device side features for communicating with these agents.

The firmware SDK and examples for the ESP Agents will be coming soon. The following Agent personas are implemented in this repository:

## Firmwares

### Generic Agent Firmware

This generic firmware is expected to work with most agents created with ESP Private Agents Platform. This firmware supports local tools like set_emotion, set_volume, set_reminder, get_local_time. You may refer [this](docs/firmwares/generic_firmware.md) for more details.

The firmware is equipped with a default agent. The default agent can be replaced by your own custom agent created through the ESP Private Agents Platform.

#### Pre-built Images

<table>
  <tr>
    <th align="center">Device</th>
    <th align="center">Firmware</th>
    <th align="center">User Guide</th>
  </tr>
  <tr>
    <td align="center">
      <img src="https://github.com/espressif/esp-agents-firmware/wiki/images/EchoEarListening.jpeg" alt="EchoEar" width="100"><br>
      EchoEar
    </td>
    <td align="center">
      <a href="https://espressif.github.io/esp-launchpad/minimal-launchpad/?flashConfigURL=https://raw.githubusercontent.com/espressif/esp-agents-firmware/refs/heads/main/docs/launchpad/friend/echo_ear.toml" target="_blank">Flash Now</a>
    </td>
    <td align="center">
      <a href="docs/guides/echo_ear.md">User Guide</a>
    </td>
  </tr>
  <tr>
    <td align="center">
      <img src="https://github.com/espressif/esp-agents-firmware/wiki/images/M5StackCoreS3Listening.png" alt="M5Stack CoreS3" width="100"><br>
      M5Stack CoreS3
    </td>
    <td align="center">
      <a href="https://espressif.github.io/esp-launchpad/minimal-launchpad/?flashConfigURL=https://raw.githubusercontent.com/espressif/esp-agents-firmware/refs/heads/main/docs/launchpad/friend/m5stack_cores3.toml" target="_blank">Flash Now</a>
    </td>
    <td align="center">
      <a href="docs/guides/m5stack_cores3.md">User Guide</a>
    </td>
  </tr>
  <tr>
    <td align="center">
      <img src="https://github.com/espressif/esp-agents-firmware/wiki/images/ESPBox3Listening.png" alt="ESP-BOX-3" width="100"><br>
      ESP-BOX-3
    </td>
    <td align="center">
      <a href="https://espressif.github.io/esp-launchpad/minimal-launchpad/?flashConfigURL=https://raw.githubusercontent.com/espressif/esp-agents-firmware/refs/heads/main/docs/launchpad/friend/esp_box_3.toml" target="_blank">Flash Now</a>
    </td>
    <td align="center">
      <a href="docs/guides/esp_box_3.md">User Guide</a>
    </td>
  </tr>
</table>

### Matter Controller + Thread Border Router Firmware

This firmware supports Matter Controller functionality and Thread Border Router functionality, apart from the common Agents functionality that is described above. It supports tools like get_device_list, set_volume, set_emotion. You may refer [this](docs/firmwares/matter_controller.md) for more details.

The firmware is equipped with a default agent. The default agent can be replaced by your own custom agent created through the ESP Private Agents Platform.

#### Pre-built Images

<table>
  <tr>
    <th align="center">Device</th>
    <th align="center">Firmware</th>
    <th align="center">User Guide</th>
  </tr>
  <tr>
    <td align="center">
      <img src="https://github.com/espressif/esp-agents-firmware/wiki/images/M5StackCoreS3Listening.png" alt="M5Stack CoreS3 + M5Stack Module Gateway H2" width="100"><br>
      M5Stack CoreS3 + M5Stack Module Gateway H2
    </td>
    <td align="center">
      <a href="https://espressif.github.io/esp-launchpad/minimal-launchpad/?flashConfigURL=https://raw.githubusercontent.com/espressif/esp-agents-firmware/refs/heads/main/docs/launchpad/matter_controller/m5stack_cores3.toml" target="_blank">Flash Now</a>
    </td>
    <td align="center">
      <a href="docs/guides/m5stack_cores3_matter_controller.md">User Guide</a>
    </td>
  </tr>
</table>

## Agents

The firmwares that are implemented above, commonly work with some default agent that is pre-configured in the firmware. Here is a list of these.

### Generic Assistant

This is your virtual friend, you can chat and have fun conversations about any topic.
It can do the following tasks:

* Having fun conversations
* Knowing local time at your location
* Setting Reminders
* Adjusting volume of your device
* Updating the emoji on the display based on mood of the conversation
* Controlling your ESP RainMaker devices

### Device (Matter) Controller

The Matter Controller agent is capable of controlling local Matter devices in the network.
It can do the following tasks:

* Controlling your Matter devices (Wi-Fi/Thread), supported Matter clusters:
  * OnOff
  * LevelControl
  * ColorControl
* Knowing local time at your location
* Adjusting volume of your device
* Updating the emoji on the display based on mood of the conversation

---
