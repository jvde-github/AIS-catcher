# Installing and Running AIS-catcher on a Raspberry Pi

This tutorial will guide you through the process of installing and running AIS-catcher on a Raspberry Pi. AIS-catcher is a software tool used to receive and decode Automatic Identification System (AIS) messages from ships, using Software Defined Radio (SDR) devices like the RTL-SDR dongle.

## Table of Contents
- [Setting Up the Raspberry Pi](#setting-up-the-raspberry-pi)
- [Installing AIS-catcher](#installing-ais-catcher)
- [Configuring AIS-catcher via the Web GUI](#configuring-ais-catcher-via-the-web-gui)
- [Accessing the AIS Web Viewer](#accessing-the-ais-web-viewer)
- [Conclusion](#conclusion)

## Setting Up the Raspberry Pi

The first step in setting up AIS-catcher is preparing your Raspberry Pi. Begin by downloading and installing the Raspberry Pi Imager from the [official website](https://www.raspberrypi.com/software/). Once installed, launch the Raspberry Pi Imager application. In the application, you'll need to select your preferred Raspberry Pi OS version and choose the SD card you've inserted as your storage device.

Before writing the image, access the advanced options by clicking the settings gear icon. Here, you'll need to configure several important settings. Enable SSH access to allow remote connections to your Pi. Set up a hostname for your device (for example, 'zerowh') and create your username and password credentials. Don't forget to configure your Wi-Fi settings by entering your network's SSID and password.

After configuring these settings, insert the SD card into your Raspberry Pi and power it on. Once the system has booted, you can connect to it via SSH using a terminal. For example, if you used the hostname 'zerowh' and username 'jasper', you would connect using:

```bash
ssh jasper@zerowh
```

## Installing AIS-catcher

Installing AIS-catcher is straightforward using the provided installation script. Open your terminal and run the following command:

```bash
sudo bash -c "$(wget -qO- https://raw.githubusercontent.com/jvde-github/AIS-catcher/main/scripts/aiscatcher-install)"
```

This installation script performs several important tasks. It installs AIS-catcher along with its configuration files, sets up a system service to run AIS-catcher in the background, compiles the program with optimizations for your specific device, and installs necessary drivers from source for compatibility with devices like the RTL-SDR V4.

> **Note:** Be patient during the installation process, as compilation can take considerable time on less powerful devices.

To verify that the installation was successful, you can run the following command:
```bash
AIS-catcher -h
```

## Configuring AIS-catcher via the Web GUI

AIS-catcher provides a web-based graphical user interface for easy configuration. It needs to be installed as a separate service by entering in the terminal:
```bash
sudo bash -c "$(curl -fsSL https://raw.githubusercontent.com/jvde-github/AIS-catcher-control/main/install_ais_catcher_control.sh)"
```
To access it, open your web browser and navigate to your Raspberry Pi's IP address on port 8110 (for example, `http://zerowh:8110`). 

![Login Screen](https://github.com/user-attachments/assets/2c10c830-84e2-42b1-bb67-9300be6d53be)

When you first access the interface, use the default credentials (username: `admin`, password: `admin`). You'll be prompted to change this password immediately for security purposes.

![Password Change](https://github.com/user-attachments/assets/bce2f1e6-cd6f-4c29-af52-03c90c72d04c)

### Input Device Configuration

In the Input section of the web GUI, you'll need to configure your SDR device. The interface allows you to select from any connected devices or manually specify a device type and serial number. If you're using a single SDR device, you can leave the device selection as 'None', and AIS-catcher will automatically use the available device.

![Input Configuration](https://github.com/user-attachments/assets/b960cc3e-276a-403f-acf9-50734886374f)

### Output Settings

AIS-catcher offers the ability to share your data with the aiscatcher.org community. Navigate to the Output > Community section to enable this feature. By default, sharing is anonymous, but you can generate and enter a sharing key to associate the data with your station and view statistics.

![Community Sharing](https://github.com/user-attachments/assets/b04c5889-f783-416f-a774-5ca0b430c538)

The web viewer configuration can be found under Output > Web Viewer. Here, you should activate the viewer and enter your station details, including a name and your geographical coordinates.

![Web Viewer Settings](https://github.com/user-attachments/assets/c6fc1a5f-c47d-41b2-96b1-82308eea2b14)

### Service Control

The Control section is where you manage the AIS-catcher service. Here you can start and stop the service, enable auto-start functionality, and monitor the service status through the log display. 

> **Note:** After any modification to the settings the changes need to be saved and the program needs to be (re)started for the changes to become effective.

![Service Control](https://github.com/user-attachments/assets/d6cfc5d6-6c7a-4cd7-90a6-67772077afd3)

## Accessing the AIS Web Viewer

Once AIS-catcher is running, you can view your received AIS data through the web viewer. Access it by navigating to your Raspberry Pi's IP address on port 8100 (for example, `http://zerowh:8100`). The viewer provides a real-time display of AIS messages and vessel positions, allowing you to verify that your setup is working correctly.

![AIS Web Viewer](https://github.com/user-attachments/assets/d81ac931-81dc-43d4-aba3-b6de2641953f)

## Conclusion

With these steps completed, you now have a fully functional AIS receiving station running on your Raspberry Pi. The system will receive AIS messages from nearby vessels and, if configured, share this data with the AIScatcher.org community. You can monitor vessel traffic in real-time through the web viewer interface.

For advanced users who want to fine-tune their setup, AIS-catcher provides two configuration files:

The JSON configuration file at:
```bash
/etc/AIS-catcher/config.json
```

And the command-line parameters file at:
```bash
/etc/AIS-catcher/config.cmd
```

> **Note:** The GUI script can also be run for existing installations that are based on the AIS-catcher install script. But once configuration files are manually edited they cannot be edited via the HTML forms anymore. The configuration files still can be edited though under the advanced options menu. 

### References:
- [AIS-catcher GitHub Repository](https://github.com/jvde-github/AIS-catcher)
- [Raspberry Pi Official Website](https://www.raspberrypi.com/)

> **Note:** Always ensure your Raspberry Pi and AIS-catcher software are up to date to benefit from the latest features and security updates.
