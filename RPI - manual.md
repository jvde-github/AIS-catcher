# Installing and Running AIS-catcher on a Raspberry Pi

This tutorial will guide you through the process of installing and running AIS-catcher on a Raspberry Pi. AIS-catcher is a software tool used to receive and decode Automatic Identification System (AIS) messages from ships, using Software Defined Radio (SDR) devices like the RTL-SDR dongle.

## Table of Contents
- [Setting Up the Raspberry Pi](#setting-up-the-raspberry-pi)
- [Installing AIS-catcher](#installing-ais-catcher)
- [Configuring AIS-catcher via the Web GUI](#configuring-ais-catcher-via-the-web-gui)
  - [Accessing the Web GUI](#accessing-the-web-gui)
  - [Setting Up the Input Device](#setting-up-the-input-device)
  - [Configuring Output Settings](#configuring-output-settings)
  - [Starting the AIS-catcher Service](#starting-the-ais-catcher-service)
- [Accessing the AIS Web Viewer](#accessing-the-ais-web-viewer)
- [Conclusion](#conclusion)

## Setting Up the Raspberry Pi

1. **Install Raspberry Pi OS:**
   - Download and install the Raspberry Pi Imager from [here](https://www.raspberrypi.com/software/).
   - Launch the Raspberry Pi Imager and select:
     - Operating System: Choose your preferred Raspberry Pi OS version
     - Storage: Select the SD card you have inserted

2. **Configure Advanced Options:**
   - Click on the Settings (gear icon) to access advanced options
   - Configure the following:
     - Enable SSH: Check this option to enable SSH access
     - Set Hostname: For example, `zerowh`
     - Set Username and Password: For example, username `jasper`
     - Configure Wi-Fi: Enter your Wi-Fi SSID and password

3. **Boot the Raspberry Pi:**
   - Insert the SD card into the Raspberry Pi and power it on
   - Once booted, you can access it via terminal or SSH:
     ```bash
     ssh jasper@zerowh
     ```

## Installing AIS-catcher

To install or update AIS-catcher, run the following command on your Raspberry Pi:

```bash
sudo bash -c "$(wget -qO- https://raw.githubusercontent.com/jvde-github/AIS-catcher/main/scripts/aiscatcher-install)"
```

This script will:
- Install AIS-catcher and its configuration files
- Set up a system service to run AIS-catcher in the background
- Compile the program optimized for your device
- Install drivers from source for compatibility with devices like the RTL-SDR V4

> **Note:** Installation may take some time on less powerful devices due to the compilation process.

After installation, you can verify the installation by running:
```bash
AIS-catcher -h
```

## Configuring AIS-catcher via the Web GUI

For basic configuration, AIS-catcher provides a simple web-based GUI.

### Accessing the Web GUI

1. **Open a Web Browser:**
   - Navigate to `http://<raspberry_pi_ip>:8110`
   - Example: `http://zerowh:8110`

2. **Log In:**
   - Username: `admin`
   - Password: `admin`

3. **Change the Default Password:**
   - After logging in, you will be prompted to set a new secure password

### Setting Up the Input Device

1. **Navigate to the Input Section:**
   - Click on the Input tab in the GUI

2. **Select Your SDR Device:**
   - Click on Select Device to choose from connected devices
   - Alternatively, manually enter the device type or serial number
   - If you leave Device as None and don't provide a serial number, AIS-catcher will use any connected device (suitable if only one SDR is connected)

3. **Save the Configuration:**
   - Click on Save to update the configuration file
   - Note: Changes take effect after restarting the AIS-catcher service

### Configuring Output Settings

#### Sharing Data with AIScatcher.org

1. **Navigate to Output > Community:**
   - Click on the Output tab and select Community

2. **Enable Data Sharing:**
   - Check the option to share your data with AIScatcher.org (anonymous by default)

3. **Enter Sharing Key (Optional):**
   - To associate your station and view statistics:
     - Click on Create to generate a sharing key on AIScatcher.org
     - Enter the provided sharing key in the GUI

#### Setting Up the Web Viewer

1. **Navigate to Output > Web Viewer:**
   - Click on Web Viewer under the Output tab

2. **Configure the Web Viewer:**
   - Activate: Ensure the web viewer is active
   - Station Name: Enter a name for your station
   - Latitude and Longitude: Provide your station's coordinates

### Starting the AIS-catcher Service

1. **Navigate to the Control Section:**
   - Click on the Control tab

2. **Start the Service:**
   - Click on Start to run AIS-catcher
   - The Log section will display real-time feedback

3. **Enable Auto-Start:**
   - Toggle Auto-Start to automatically run AIS-catcher at boot and restart it if it stops unexpectedly

4. **Monitor Service Status:**
   - The Control page displays the current status and any error messages

## Accessing the AIS Web Viewer

With AIS-catcher running, you can view the received AIS data through the web viewer.

1. **Open a Web Browser:**
   - Navigate to `http://<raspberry_pi_ip>:8100`
   - Example: `http://zerowh:8100`

2. **View AIS Data:**
   - The web viewer displays real-time AIS messages and vessel positions
   - Use this interface to verify that your setup is functioning correctly

## Conclusion

You have successfully installed and configured AIS-catcher on your Raspberry Pi. Your station is now receiving AIS messages and, if configured, sharing data with AIScatcher.org. You can monitor vessel traffic in real-time using the web viewer.

For advanced configurations, you can edit the JSON configuration file located at:
```bash
/etc/AIS-catcher/config.json
```

Or modify command-line parameters in:
```bash
/etc/AIS-catcher/config.cmd
```

### References:
- [AIS-catcher GitHub Repository](https://github.com/jvde-github/AIS-catcher)
- [Raspberry Pi Official Website](https://www.raspberrypi.com/)

> **Note:** Always ensure your Raspberry Pi and AIS-catcher software are up to date to benefit from the latest features and security updates.
