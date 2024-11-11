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
    dependencies=$(ldd build/AIS-catcher | grep -E 'libairspy|libairspyhf|librtlsdr|libhackrf|libzmq|libz|libssl' | awk '{print $1}')

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
package_version=0.60~260-ga9e858dd
install_deps=$5

# Install build dependencies
install_dependencies "librtlsdr-dev libairspy-dev libairspyhf-dev libhackrf-dev libzmq3-dev libssl-dev zlib1g-dev $build_deps"

# Build the project
build_project

# Create Debian package if specified
create_debian_package "$deb_package" "$package_arch" "$package_version" "$install_deps"
