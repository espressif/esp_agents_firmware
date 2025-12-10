# User Guide

The fastest way to get started is to get the ESP RainMaker Home app from the respective stores:

<div style="display: flex; align-items: center; gap: 12px;">
  <a href="https://apps.apple.com/in/app/esp-rainmaker-home/id1563728960" target="_blank">
    <img src="https://developer.apple.com/assets/elements/badges/download-on-the-app-store.svg" width="160">
  </a>

  <a href="https://play.google.com/store/apps/details?id=com.espressif.novahome" target="_blank">
    <img src="https://play.google.com/intl/en_us/badges/static/images/badges/en_badge_web_generic.png" width="200">
  </a>
</div>

## Configuring the Device

The device is initially in unconfigured mode. You will need to set it up to connect to your home's Wi-Fi network, so you can begin using the device.

* Once you have installed and launched the app, the app will ask for Bluetooth/Location permission. This is necessary for the app to detect the unconfigured device's advertisement data.

* Power-on the device, the device will then show a QR Code (For devices with no display, the QR Code will be part of the product packaging)

<img src="https://github.com/espressif/esp_agents_firmware/wiki/images/ESPBox3Provisioning.png" width="200" style="display:block">

* In the ESP RainMaker Home app, click on the "+" icon on the top right, and select 'Scan QR Code'. With the phone's camera scan the QR code of the device.

<img src="https://github.com/espressif/esp_agents_firmware/wiki/images/AddDevices.jpeg" width="200" style="display:block">

* Once the app detects the device, it will ask for the Wi-Fi network and its passphrase to be programmed in the device

* After the device is configured, the device is ready to be used.

<img src="https://github.com/espressif/esp_agents_firmware/wiki/images/SelectWiFi.jpeg" width="200" style="display:block">

## Using the Device

- Once the device is in configured mode, it connects to your Wi-Fi network.

<img src="https://github.com/espressif/esp_agents_firmware/wiki/images/ESPBox3BootUp.png" width="150" style="display:block">

* Now you can say "Hi, ESP" to wake the device up. The device will play a chime, indicating it's ready for conversation.

* The device is in listening mode when it shows the below icon. You may now ask it any questions, and it will respond to you.

<img src="https://github.com/espressif/esp_agents_firmware/wiki/images/ESPBox3Listening.png" width="150" style="display:block">

- You can see the response on screen(green color).

<img src="https://github.com/espressif/esp_agents_firmware/wiki/images/ESPBox3Speaking.png" width="150" style="display:block">

- When response playback stops, you can next question.
- Device goes to sleep after 15 seconds of inactivity, you can wake it again by saying "Hi, ESP".

**Note: Ensure that the microphone is not muted while using the device. You can look at the mute LED on the top for verifying microphone state.**

## Reset to Factory

You can factory reset your device using either of following methods:
* Reflash the firmware using <a href="https://espressif.github.io/esp-launchpad/minimal-launchpad/?flashConfigURL=https://raw.githubusercontent.com/espressif/esp-agents-firmware/refs/heads/main/docs/launchpad/friend/esp_box_3.toml" target="_blank">ESP Launchpad</a>
* Goto device settings in ESP RainMaker Home app -> Factory Reset

## Changing the Agent on the Device

When you create a new agent, in the Agents Dashboard you can click on Share Agent and generate the QR Code.

<img src="https://github.com/espressif/esp_agents_firmware/wiki/images/ShareAgent.png" width="150" style="display:block">

Now scan the QR Code from your phone's camera app / Google lens. The app will then launch the ESP RainMaker Home app.

The app will ask you to select the device that needs to be configured with this agent, and then it will go ahead and configure that device.

<img src="https://github.com/espressif/esp_agents_firmware/wiki/images/ShareAgentDevice.jpeg" width="200" style="display:block">

After selecting your device, the app will update the Agent ID on that device.

<img src="https://github.com/espressif/esp_agents_firmware/wiki/images/ShareAgentSuccess.jpeg" width="200" style="display:block">
