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
  if [ "$package_arch" = "armhf" ]; then  
    cmake .. -DNMEA2000_PATH=.. -DARMV6=ON; 
  else
    cmake .. -DNMEA2000_PATH=..  -DRTLSDR_STATIC=ON; 
  fi
  
  make
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
    # dependencies=$(ldd build/AIS-catcher | grep -E 'libairspy|libairspyhf|librtlsdr|libhackrf|libzmq|libz|libssl|libusb|libsqlite3' | awk '{print $1}')
    dependencies=$(ldd build/AIS-catcher | grep -E 'libairspy|libairspyhf|librtlsdr|libhackrf|libzmq3|libz|libssl|libusb-1\.0|libsqlite3' | awk '{print $1}')

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

  
    # Build the project and package
    mkdir -p debian/usr/bin
    cp build/AIS-catcher debian/usr/bin/

    mkdir -p debian/etc/AIS-catcher/plugins
    cp plugins/* debian/etc/AIS-catcher/plugins/

    dpkg-deb --build debian
    mv debian.deb "$package_name.deb"
    rm -rf debian
  fi
}

# Parse command line arguments
build_deps=$1
deb_package=$2
package_arch=$3
package_version=0.61~181-ga6aae2de
install_deps=$5

# Install build dependencies
install_dependencies "libairspy-dev libairspyhf-dev libhackrf-dev libzmq3-dev libssl-dev zlib1g-dev libsqlite3-dev  libusb-1.0-0-dev $build_deps"

# Build the project
build_project

# Create Debian package if specified
create_debian_package "$deb_package" "$package_arch" "$package_version" "$install_deps"
