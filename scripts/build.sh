#!/bin/bash

# install dependencies using apt
install_dependencies() {
  if [ -n "$1" ]; then
    echo "Installing build dependencies: $1"
    apt-get update
    apt-get install -y $1
  fi
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
  cd ../../build; ls ..; cmake .. -DNMEA2000_PATH=..; make
  cd ..
}

# create a Debian package

create_debian_package() {
  if [ -n "$1" ]; then
    package_name="ais-catcher"
    package_arch=$2
    package_version=$3

    # Extract dependencies using ldd and filter the required libraries
    echo "Extracting dependencies using ldd..."
    dependencies=$(ldd build/AIS-catcher | grep -E 'libairspy|libairspyhf|librtlsdr|libhackrf|libzmq|libz|libcrypto|libssl' | awk '{print $1}')

    # Initialize the depends variable
    depends=""

    echo "Processing dependencies..."
    # Iterate over each dependency and format it
    for dep in $dependencies; do
      # Extract the library name and version
      echo "Processing dependency: $dep"
      lib_name=$(echo "$dep" | awk -F'.so.' '{print $1}')
      lib_version=$(echo "$dep" | awk -F'.so.' '{print $2}')
      echo "Library name: $lib_name, Library version: $lib_version"
      depends="$depends ${lib_name}${lib_version},"
    done
    depends=${depends%,}

    echo "Final Depends line: Depends: $depends"

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

    echo "#!/bin/bash" > debian/DEBIAN/postinst
    echo "CONFIG_DIR=\"/etc/AIS-catcher\"" >> debian/DEBIAN/postinst
    echo "CONFIG_FILE=\"\$CONFIG_DIR/config.json\"" >> debian/DEBIAN/postinst
    echo "CMD_FILE=\"\$CONFIG_DIR/config.cmd\"" >> debian/DEBIAN/postinst
    echo "mkdir -p \"\$CONFIG_DIR\"" >> debian/DEBIAN/postinst
    echo "if [ ! -f \"\$CONFIG_FILE\" ]; then" >> debian/DEBIAN/postinst
    echo "  echo \"Creating empty config.json file...\"" >> debian/DEBIAN/postinst
    echo "  echo '{ \"config\": \"aiscatcher\", \"version\": 1 }' > \"\$CONFIG_FILE\"" >> debian/DEBIAN/postinst
    echo "fi" >> debian/DEBIAN/postinst
    echo "if [ ! -f \"\$CMD_FILE\" ]; then" >> debian/DEBIAN/postinst
    echo "  echo \"Creating empty config.cmd file...\"" >> debian/DEBIAN/postinst
    echo "  echo '' > \"\$CMD_FILE\"" >> debian/DEBIAN/postinst
    echo "fi" >> debian/DEBIAN/postinst
    echo "# Check if systemd is available" >> debian/DEBIAN/postinst
    echo "if [ -d /run/systemd/system ]; then" >> debian/DEBIAN/postinst
    echo "  # Check if ais-catcher service is running" >> debian/DEBIAN/postinst
    echo "  if systemctl is-active --quiet ais-catcher; then" >> debian/DEBIAN/postinst
    echo "    was_running=true" >> debian/DEBIAN/postinst
    echo "  else" >> debian/DEBIAN/postinst
    echo "    was_running=false" >> debian/DEBIAN/postinst
    echo "  fi" >> debian/DEBIAN/postinst
    echo "  # Stop ais-catcher service if it's running" >> debian/DEBIAN/postinst
    echo "  if \$was_running; then" >> debian/DEBIAN/postinst
    echo "    systemctl stop ais-catcher" >> debian/DEBIAN/postinst
    echo "  fi" >> debian/DEBIAN/postinst
    echo "  echo \"Reloading systemd daemon...\"" >> debian/DEBIAN/postinst
    echo "  systemctl daemon-reload" >> debian/DEBIAN/postinst
    echo "  # Restart ais-catcher service if it was running before" >> debian/DEBIAN/postinst
    echo "  if \$was_running; then" >> debian/DEBIAN/postinst
    echo "    systemctl start ais-catcher" >> debian/DEBIAN/postinst
    echo "  fi" >> debian/DEBIAN/postinst
    echo "fi" >> debian/DEBIAN/postinst
    chmod +x debian/DEBIAN/postinst

    echo "#!/bin/bash" > debian/DEBIAN/prerm
    echo "# Check if systemd is available" >> debian/DEBIAN/prerm
    echo "if [ -d /run/systemd/system ]; then" >> debian/DEBIAN/prerm
    echo "  echo \"Systemd detected.\"" >> debian/DEBIAN/prerm
    echo "  # Check if ais-catcher service is running" >> debian/DEBIAN/prerm
    echo "  if systemctl is-active --quiet ais-catcher; then" >> debian/DEBIAN/prerm
    echo "    echo \"Stopping ais-catcher service...\"" >> debian/DEBIAN/prerm
    echo "    # Stop ais-catcher service" >> debian/DEBIAN/prerm
    echo "    systemctl stop ais-catcher" >> debian/DEBIAN/prerm
    echo "    echo \"ais-catcher service stopped.\"" >> debian/DEBIAN/prerm
    echo "  else" >> debian/DEBIAN/prerm
    echo "    echo \"ais-catcher service is not running.\"" >> debian/DEBIAN/prerm
    echo "  fi" >> debian/DEBIAN/prerm
    echo "  echo \"Disabling ais-catcher service...\"" >> debian/DEBIAN/prerm
    echo "  # Disable ais-catcher service" >> debian/DEBIAN/prerm
    echo "  systemctl disable ais-catcher" >> debian/DEBIAN/prerm
    echo "  echo \"ais-catcher service disabled.\"" >> debian/DEBIAN/prerm
    echo "  echo \"Reloading systemd configuration...\"" >> debian/DEBIAN/prerm
    echo "  # Reload systemd configuration" >> debian/DEBIAN/prerm
    echo "  systemctl daemon-reload" >> debian/DEBIAN/prerm
    echo "  echo \"Systemd configuration reloaded.\"" >> debian/DEBIAN/prerm
    echo "else" >> debian/DEBIAN/prerm
    echo "  echo \"Systemd not detected.\"" >> debian/DEBIAN/prerm
    echo "fi" >> debian/DEBIAN/prerm
    echo "echo \"Removing ais-catcher service file...\"" >> debian/DEBIAN/prerm
    echo "# Remove the systemd service file" >> debian/DEBIAN/prerm
    echo "rm -f /lib/systemd/system/ais-catcher.service" >> debian/DEBIAN/prerm
    echo "echo \"ais-catcher service file removed.\"" >> debian/DEBIAN/prerm
    chmod +x debian/DEBIAN/prerm

    # Add systemd service file
    mkdir -p debian/lib/systemd/system
    echo "[Unit]" > debian/lib/systemd/system/ais-catcher.service
    echo "Description=AIS-catcher Service" >> debian/lib/systemd/system/ais-catcher.service
    echo "After=network.target" >> debian/lib/systemd/system/ais-catcher.service
    echo "" >> debian/lib/systemd/system/ais-catcher.service
    echo "[Service]" >> debian/lib/systemd/system/ais-catcher.service 
    echo "ExecStart=/bin/bash -c '/usr/bin/AIS-catcher -C /etc/AIS-catcher/config.json \$(/bin/grep -v \"^#\" /etc/AIS-catcher/config.cmd | /usr/bin/tr \"\\n\" \" \")'" >> debian/lib/systemd/system/ais-catcher.service
    echo "Restart=always" >> debian/lib/systemd/system/ais-catcher.service
    echo "" >> debian/lib/systemd/system/ais-catcher.service
    echo "[Install]" >> debian/lib/systemd/system/ais-catcher.service
    echo "WantedBy=multi-user.target" >> debian/lib/systemd/system/ais-catcher.service
    
    # Build the project and package
    mkdir -p debian/usr/bin
    cp build/AIS-catcher debian/usr/bin/

    cp scripts/aiscatcher-install debian/usr/bin/
    chmod +x debian/usr/bin/aiscatcher-install

    cp scripts/aiscatcher-control debian/usr/bin/
    chmod +x debian/usr/bin/aiscatcher-control

    dpkg-deb --build debian
    mv debian.deb "$package_name.deb"
    rm -rf debian
  fi
}

# Parse command line arguments
build_deps=$1
deb_package=$2
package_arch=$3
package_version=0.59~233-g621675c4
install_deps=$5

# Install build dependencies
install_dependencies "librtlsdr-dev libairspy-dev libairspyhf-dev libhackrf-dev libzmq3-dev libssl-dev zlib1g-dev $build_deps"

# Build the project
build_project

# Create Debian package if specified
create_debian_package "$deb_package" "$package_arch" "$package_version" "$install_deps"
