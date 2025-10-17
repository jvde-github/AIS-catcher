#!/bin/bash

# install dependencies using apt
install_dependencies() {
  if [ -n "$1" ]; then
    echo "Installing build dependencies: $1"
    apt-get update
    apt-get install -y $1
  fi

  echo "Installing rtl-sdr from source"	
  git clone https://gitea.osmocom.org/sdr/rtl-sdr.git
  cd rtl-sdr; mkdir build; cd build; cmake ../ -DINSTALL_UDEV_RULES=ON -DDETACH_KERNEL_DRIVER=ON; make; make install; 
  cd ..; cd ..
  cp rtl-sdr/rtl-sdr.rules /etc/udev/rules.d/
  ldconfig
}

# build the project
build_project() {
  if [ -d "build" ]; then
    rm -rf build
    mkdir build
    echo "Build directory deleted and recreated."
  else
    mkdir build
    echo "build directory created"
  fi

  if [ -d "NMEA2000" ]; then
    rm -rf NMEA2000
    echo "NMEA2000 library directory deleted."
  fi

  git clone https://github.com/ttlappalainen/NMEA2000.git
  cd NMEA2000; cd src
  g++ -O3 -c  N2kMsg.cpp  N2kStream.cpp N2kMessages.cpp   N2kTimer.cpp  NMEA2000.cpp  N2kGroupFunctionDefaultHandlers.cpp  N2kGroupFunction.cpp  -I.
  ar rcs libnmea2000.a *.o 
  cd ../../build; ls ..; 
  cmake .. -DNMEA2000_PATH=..  -DRTLSDR_STATIC=ON; 
  
  make
  cd ..
}

# create a Debian package

# create a Debian package

create_debian_package() {
  if [ -n "$1" ]; then
    package_name="ais-catcher"
    package_arch=$2
    package_version=$3

    # Extract dependencies using ldd and filter the required libraries
    echo "Extracting dependencies using ldd..."
    # dependencies=$(ldd build/AIS-catcher | grep -E 'libairspy|libairspyhf|librtlsdr|libhackrf|libzmq|libz|libssl|libusb|libsqlite3' | awk '{print $1}')
    dependencies=$(ldd build/AIS-catcher | grep -E 'libairspy|libairspyhf|librtlsdr|libhackrf|libzmq3|libz|libssl|libusb-1\.0|libsqlite3|libpq' | awk '{print $1}')

    # Initialize the depends variable
    depends=""

    # Print all found dependencies first
    echo "Found dependencies:"
    printf '%s\n' "$dependencies"
    echo ""

    echo "Processing dependencies for package control file:"
    # Iterate over each dependency and format it
    for dep in $dependencies; do
      # Extract the library name and version
      lib_name=$(echo "$dep" | awk -F'.so.' '{print $1}')
      lib_version=$(echo "$dep" | awk -F'.so.' '{print $2}')
      
      # Check if lib_name ends in a digit
      if [[ $lib_name =~ [0-9]$ ]]; then
          # Add hyphen before the version
          echo "  $dep -> ${lib_name}-${lib_version} (added hyphen)"
          depends="$depends ${lib_name}-${lib_version},"
      else
          echo "  $dep -> ${lib_name}${lib_version}"
          depends="$depends ${lib_name}${lib_version},"
      fi
    done
    depends=${depends%,}
    
    echo ""
    echo "Final dependency string:"
    echo "$depends"
    echo "======================\n"

    echo "Creating Debian package: $package_name"
    mkdir -p debian/DEBIAN
    echo "Package: $package_name" > debian/DEBIAN/control
    echo "Version: $package_version" >> debian/DEBIAN/control
    echo "Architecture: $package_arch" >> debian/DEBIAN/control
    echo "Maintainer: jvde-github <jvde-github@gmail.com>" >> debian/DEBIAN/control
    echo "Description: AIS-catcher" >> debian/DEBIAN/control
    echo "Depends: $depends" >> debian/DEBIAN/control  # Ensure this line ends with a newline character

    cat  debian/DEBIAN/control

    echo "#!/bin/bash" > debian/DEBIAN/preinst
    echo "echo \"*******************************************************************\" > /dev/tty" >> debian/DEBIAN/preinst
    echo "echo \"*                                                                 *\" > /dev/tty" >> debian/DEBIAN/preinst
    echo "echo \"* AIS-catcher is distributed under the GNU GPL v3 license.        *\" > /dev/tty" >> debian/DEBIAN/preinst
    echo "echo \"* This software is research and experimental software.            *\" > /dev/tty" >> debian/DEBIAN/preinst
    echo "echo \"* It is not intended for mission-critical use.                    *\" > /dev/tty" >> debian/DEBIAN/preinst
    echo "echo \"* Use it at your own risk and responsibility.                     *\" > /dev/tty" >> debian/DEBIAN/preinst
    echo "echo \"* It is your responsibility to ensure the legality of using       *\" > /dev/tty" >> debian/DEBIAN/preinst
    echo "echo \"* this software in your region.                                   *\" > /dev/tty" >> debian/DEBIAN/preinst
    echo "echo \"*                                                                 *\" > /dev/tty" >> debian/DEBIAN/preinst
    echo "echo \"*******************************************************************\" > /dev/tty" >> debian/DEBIAN/preinst

    chmod +x debian/DEBIAN/preinst

    # Create postinst script for udev rules installation
    echo "#!/bin/bash" > debian/DEBIAN/postinst
    echo "" >> debian/DEBIAN/postinst
    echo "# Install RTL-SDR udev rules" >> debian/DEBIAN/postinst
    echo "RULES_FILE=\"rtl-sdr.rules\"" >> debian/DEBIAN/postinst
    echo "SOURCE_RULES=\"/usr/share/ais-catcher/\$RULES_FILE\"" >> debian/DEBIAN/postinst
    echo "TARGET_RULES=\"/etc/udev/rules.d/\$RULES_FILE\"" >> debian/DEBIAN/postinst
    echo "" >> debian/DEBIAN/postinst
    echo "install_udev_rules() {" >> debian/DEBIAN/postinst
    echo "    echo \"Installing RTL-SDR udev rules...\"" >> debian/DEBIAN/postinst
    echo "    " >> debian/DEBIAN/postinst
    echo "    # Check if target rules already exist" >> debian/DEBIAN/postinst
    echo "    if [ -f \"\$TARGET_RULES\" ]; then" >> debian/DEBIAN/postinst
    echo "        # Compare checksums to see if files are different" >> debian/DEBIAN/postinst
    echo "        if command -v md5sum >/dev/null 2>&1; then" >> debian/DEBIAN/postinst
    echo "            existing_md5=\$(md5sum \"\$TARGET_RULES\" | cut -d' ' -f1)" >> debian/DEBIAN/postinst
    echo "            new_md5=\$(md5sum \"\$SOURCE_RULES\" | cut -d' ' -f1)" >> debian/DEBIAN/postinst
    echo "            " >> debian/DEBIAN/postinst
    echo "            if [ \"\$existing_md5\" = \"\$new_md5\" ]; then" >> debian/DEBIAN/postinst
    echo "                echo \"RTL-SDR udev rules are already up to date.\"" >> debian/DEBIAN/postinst
    echo "                return 0" >> debian/DEBIAN/postinst
    echo "            else" >> debian/DEBIAN/postinst
    echo "                echo \"Warning: Different RTL-SDR udev rules already exist.\"" >> debian/DEBIAN/postinst
    echo "                echo \"Backing up existing rules to \$TARGET_RULES.backup\"" >> debian/DEBIAN/postinst
    echo "                cp \"\$TARGET_RULES\" \"\$TARGET_RULES.backup\" 2>/dev/null || true" >> debian/DEBIAN/postinst
    echo "            fi" >> debian/DEBIAN/postinst
    echo "        else" >> debian/DEBIAN/postinst
    echo "            echo \"Warning: RTL-SDR udev rules already exist. Backing up...\"" >> debian/DEBIAN/postinst
    echo "            cp \"\$TARGET_RULES\" \"\$TARGET_RULES.backup\" 2>/dev/null || true" >> debian/DEBIAN/postinst
    echo "        fi" >> debian/DEBIAN/postinst
    echo "    fi" >> debian/DEBIAN/postinst
    echo "    " >> debian/DEBIAN/postinst
    echo "    # Copy the new rules" >> debian/DEBIAN/postinst
    echo "    if cp \"\$SOURCE_RULES\" \"\$TARGET_RULES\" 2>/dev/null; then" >> debian/DEBIAN/postinst
    echo "        echo \"RTL-SDR udev rules installed successfully.\"" >> debian/DEBIAN/postinst
    echo "        " >> debian/DEBIAN/postinst
    echo "        # Trigger udev reload" >> debian/DEBIAN/postinst
    echo "        reload_udev_rules" >> debian/DEBIAN/postinst
    echo "    else" >> debian/DEBIAN/postinst
    echo "        echo \"Error: Failed to install RTL-SDR udev rules.\"" >> debian/DEBIAN/postinst
    echo "        echo \"You may need to manually copy \$SOURCE_RULES to \$TARGET_RULES\"" >> debian/DEBIAN/postinst
    echo "        return 1" >> debian/DEBIAN/postinst
    echo "    fi" >> debian/DEBIAN/postinst
    echo "}" >> debian/DEBIAN/postinst
    echo "" >> debian/DEBIAN/postinst
    echo "reload_udev_rules() {" >> debian/DEBIAN/postinst
    echo "    echo \"Reloading udev rules...\"" >> debian/DEBIAN/postinst
    echo "    " >> debian/DEBIAN/postinst
    echo "    # Try different methods to reload udev rules" >> debian/DEBIAN/postinst
    echo "    if command -v udevadm >/dev/null 2>&1; then" >> debian/DEBIAN/postinst
    echo "        if udevadm control --reload-rules 2>/dev/null; then" >> debian/DEBIAN/postinst
    echo "            echo \"Udev rules reloaded successfully.\"" >> debian/DEBIAN/postinst
    echo "            " >> debian/DEBIAN/postinst
    echo "            # Trigger udev to reprocess devices" >> debian/DEBIAN/postinst
    echo "            if udevadm trigger --subsystem-match=usb 2>/dev/null; then" >> debian/DEBIAN/postinst
    echo "                echo \"Udev USB devices retriggered.\"" >> debian/DEBIAN/postinst
    echo "            else" >> debian/DEBIAN/postinst
    echo "                echo \"Note: Could not retrigger USB devices automatically.\"" >> debian/DEBIAN/postinst
    echo "            fi" >> debian/DEBIAN/postinst
    echo "        else" >> debian/DEBIAN/postinst
    echo "            echo \"Warning: Failed to reload udev rules using udevadm.\"" >> debian/DEBIAN/postinst
    echo "            suggest_manual_reload" >> debian/DEBIAN/postinst
    echo "        fi" >> debian/DEBIAN/postinst
    echo "    elif [ -f /proc/sys/kernel/hotplug ]; then" >> debian/DEBIAN/postinst
    echo "        # Fallback for older systems" >> debian/DEBIAN/postinst
    echo "        echo \"Attempting to reload using legacy method...\"" >> debian/DEBIAN/postinst
    echo "        killall -USR1 udevd 2>/dev/null || killall -USR1 systemd-udevd 2>/dev/null || true" >> debian/DEBIAN/postinst
    echo "        echo \"Legacy udev reload attempted.\"" >> debian/DEBIAN/postinst
    echo "    else" >> debian/DEBIAN/postinst
    echo "        echo \"Warning: Could not find udevadm or legacy udev reload method.\"" >> debian/DEBIAN/postinst
    echo "        suggest_manual_reload" >> debian/DEBIAN/postinst
    echo "    fi" >> debian/DEBIAN/postinst
    echo "}" >> debian/DEBIAN/postinst
    echo "" >> debian/DEBIAN/postinst
    echo "suggest_manual_reload() {" >> debian/DEBIAN/postinst
    echo "    echo \"\"" >> debian/DEBIAN/postinst
    echo "    echo \"To activate the RTL-SDR udev rules, you can try one of the following:\"" >> debian/DEBIAN/postinst
    echo "    echo \"  1. Disconnect and reconnect your RTL-SDR device\"" >> debian/DEBIAN/postinst
    echo "    echo \"  2. Run: sudo udevadm control --reload-rules && sudo udevadm trigger\"" >> debian/DEBIAN/postinst
    echo "    echo \"  3. Reboot your system\"" >> debian/DEBIAN/postinst
    echo "    echo \"\"" >> debian/DEBIAN/postinst
    echo "}" >> debian/DEBIAN/postinst
    echo "" >> debian/DEBIAN/postinst
    echo "# Main execution" >> debian/DEBIAN/postinst
    echo "case \"\$1\" in" >> debian/DEBIAN/postinst
    echo "    configure)" >> debian/DEBIAN/postinst
    echo "        install_udev_rules" >> debian/DEBIAN/postinst
    echo "        ;;" >> debian/DEBIAN/postinst
    echo "    *)" >> debian/DEBIAN/postinst
    echo "        ;;" >> debian/DEBIAN/postinst
    echo "esac" >> debian/DEBIAN/postinst
    echo "" >> debian/DEBIAN/postinst
    echo "exit 0" >> debian/DEBIAN/postinst

    chmod +x debian/DEBIAN/postinst

    # Build the project and package
    mkdir -p debian/usr/bin
    cp build/AIS-catcher debian/usr/bin/

    mkdir -p debian/etc/AIS-catcher/plugins
    cp plugins/* debian/etc/AIS-catcher/plugins/

    # Store udev rules in a proper location within the package
    mkdir -p debian/usr/share/ais-catcher
    cp rtl-sdr/rtl-sdr.rules debian/usr/share/ais-catcher/

    dpkg-deb --build debian
    mv debian.deb "$package_name.deb"
    rm -rf debian
  fi
}

# Parse command line arguments
build_deps=$1
deb_package=$2
package_arch=$3
package_version=0.62~187-g6e898aaf
install_deps=$5

# Install build dependencies
install_dependencies "libairspy-dev libairspyhf-dev libhackrf-dev libzmq3-dev libssl-dev zlib1g-dev libsqlite3-dev  libusb-1.0-0-dev libpq-dev $build_deps"

# Build the project
build_project

# Create Debian package if specified
create_debian_package "$deb_package" "$package_arch" "$package_version" "$install_deps"
