# Using with a custom deployment of ESP Private Agents

The examples are configured to work with the public deployment of ESP Private Agents out-of-the-box. \
If you wish to use them with a custom deployment in your own AWS account, you can configure the firmware using the following steps:

## Prerequisites

You need to have:

- ESP Private Agents API URL.
- Refresh token for you account:
  - Goto user website -> Profile -> Click on "Copy Refresh Token"

## Changing the ESP Private Agents API URL

- Open menuconfig from the example directory you wish to use.

```bash
idf.py menuconfig
```

- Navigate to `ESP Agent Config`
- Change the `ESP Private Agents API Endpoint` to your custom deployment URL. \

- You can now build and flash the example.

```bash
idf.py build flash monitor
```

## Setting up the device

- After your device boots, run the following commands to set your refresh token and agent ID.

  ```bash
  set-token <refresh_token>
  set-agent <agent_id>
  ```

- Setup the Wi-Fi network on the device by running the following command:

  ```bash
  set-wifi <ssid> <passphrase>
  ```

- Alternatively, you can also use ESP BLE Provisioning App to setup the Wi-Fi on device.
