#!/bin/bash

# Function to determine OS and version
detect_os_version() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        OS=$ID
        VERSION=$VERSION_CODENAME
        echo "Detected OS: $OS"
        echo "Detected Version: $VERSION"
    else
        echo "Cannot determine OS version"
        exit 1
    fi
}

# Function to determine architecture
detect_architecture() {
    ARCH=$(dpkg --print-architecture)
    echo "Detected architecture: $ARCH"
}

# Function to install dependencies
install_dependencies() {
    echo "Updating package list..."
    sudo apt update
    echo "Installing required dependencies..."
    sudo apt install -y wget gdebi-core
}

# Function to download and install the correct package
install_package() {
    BASE_URL="https://github.com/jvde-github/AIS-catcher/releases/download/Edge"
    PACKAGE_ARCH=$ARCH

    if [ "$OS" == "raspbian" ] && [ "$ARCH" == "aarch64" ]; then
        PACKAGE_ARCH="amd64"
        echo "Using amd64 package for aarch64 on Raspbian."
    fi

    FILE_NAME="ais-catcher_${OS}_${VERSION}_${PACKAGE_ARCH}.deb"
    PACKAGE_URL="${BASE_URL}/${FILE_NAME}"
    
    echo "Downloading package from: $PACKAGE_URL"
    wget -O /tmp/ais-catcher.deb "$PACKAGE_URL"
    if [ $? -ne 0 ]; then
        echo "Failed to download the package. Exiting."
        exit 1
    fi

    echo "Installing package..."
    sudo apt install /tmp/ais-catcher.deb
    if [ $? -ne 0 ]; then
        echo "Failed to install the package. Exiting."
        exit 1
    fi

    # Clean up
    echo "Cleaning up temporary files..."
    rm /tmp/ais-catcher.deb

    echo "Installation complete."
}

# Main script execution
echo "Starting AIS Catcher installation script..."
detect_os_version
detect_architecture
install_dependencies
install_package
echo "AIS Catcher installation script finished."
