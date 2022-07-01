This is some help for the scripts in this folder.

ais_processor.sh:
Processes AIS messages from a raw iq data file (either binary or sult), with some pre-determined options.
It assumes that the AIS-catcher binary is in the same folder as the script.
It creates an output file that contains the NMEA text strings decoded from the raw iq file.
There is also an option to take .sult files as input. It will generate both the .iq and .nmea files into the output folder.
There are some runtime options:
    -i the input folder to search for .iq (or .sult) files. NOTE: If a filename is passed to the script then this input folder is ignored.
    -o the output folder to create decoded NMEA message files, default is '.'
    -s process sult data files, not raw IQ files (the filename is a .sult file)
    -r the IQ sample rate, defaults to 1MHz
    -c remove created files that didn't decode a valid AIS message
NOTE: If a sult file (or input folder) is specified, then the decoded IQ and NMEA files will have the same filename, with an encoded index of the pulse it contains
NOTE: The output file will have the same name as the input file, but with extension changed from .iq to .nmea.
