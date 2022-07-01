#!/bin/bash

show_call() {
  echo "$(printf %q "$(basename "$0")")$((($#)) && printf ' %q' "$@")"
}

show_help() {
  echo "Processes raw AIS IQ data files, decoding NMEA messages"
  echo ""
  echo "Usage: $(basename "$0") <ARGS> <FILENAME>"
  echo "    ARGS: -h       Show help message"
  echo "          -q       Suppress output to screen [default is false]"
  echo "          -s       Read data from Sult files, not IQ files [default is false]"
  echo "          -r       Sample rate of the input file [default is 1000000Hz]"
  echo "          -i       Folder to search for input IQ files [default is current folder]"
  echo "          -o       Folder to place the output files, [default is location of input file]"
  echo "          -c       Clean up files that contained no decoded messages"
  echo "    FILENAME       Filename to process [if not specified, all files in input folder]"
}

# hold the input folder location
IN_FOLDER="."
# hold the output folder location
OUT_FOLDER="."
# whether to print output to screen
PRINT_OUT=1
# whether to input Sult files
SULT_INPUT=0
# sample rate of raw data
SAMPLE_RATE=1000000
# clean up files that had no messages
CLEAN_FILES=0

# first parse the flags
while getopts ":hqsr:i:o:c" arg; do
case "$arg" in
    h)  show_help
        exit 0
        ;;
    q)  PRINT_OUT=0
        ;;
    s)  SULT_INPUT=1
        ;;
    r)  SAMPLE_RATE=${OPTARG}
        ;;
    i)  IN_FOLDER=${OPTARG}
        ;;
    o)  OUT_FOLDER=${OPTARG}
        ;;
    c)  CLEAN_FILES=1
        ;;
    ?) 
        show_call
        echo "Unknown argument ${OPTARG}"
        show_help
        exit 0
        ;;
esac
done
# remove the flags
shift $(($OPTIND - 1))

# create the output folder if it doesn't already exist
if [ ! -d $OUT_FOLDER ] ; then
    echo "Output folder $OUT_FOLDER doesn't exist, creating it"
    mkdir -p $OUT_FOLDER
fi


SULT_FILES=""
FILENAMES=""

# see if we need to convert from SULT to IQ
if [ $SULT_INPUT -ne 0 ]; then

    # see if there was a sult filename specified
    if [ $# -eq 0 ] ; then
        SULT_FILES=$(find $IN_FOLDER -maxdepth 1 -name '*.sult' -print0 | sort -z | xargs -r0)
    else
        SULT_FILES="$@"
    fi

    for FN in $SULT_FILES
    do
        if [ $PRINT_OUT -ne 0 ]; then
            echo "Processing: $FN"
        fi
        filename=$(basename $FN .sult)
        python3 sult.py $FN -e $OUT_FOLDER/$filename.iq
    done
    # get all the IQ data files
    FILENAMES=$(find $OUT_FOLDER -maxdepth 1 -name '*.iq' -print0 | sort -z | xargs -r0)
else
    # see if there was a filename specified
    if [ $# -eq 0 ] ; then
        FILENAMES=$(find $IN_FOLDER -maxdepth 1 -name '*.iq' -print0 | sort -z | xargs -r0)
    else
        FILENAMES="$@"
    fi
fi


# echo "Processing: $FILENAMES"
DECODE_COUNT=0

for IFN in $FILENAMES
do
    OFN=$OUT_FOLDER/$(basename $IFN .iq).nmea
    if [ $PRINT_OUT -ne 0 ]; then
        echo "Processing: $IFN"
        echo "Output: $OFN"
    fi

    ./AIS-catcher -q -s $SAMPLE_RATE -r cs16 $IFN -e $OFN
    
    # remove files that are empty
    if [ ! -s $OFN ] ; then
        if [ $CLEAN_FILES -ne 0 ] ; then
            if [ $SULT_INPUT -ne 0 ] ; then
                # remove created IQ file, only if decoded from SULT
                rm $IFN
            fi
            # remove the empty nmea file
            rm $OFN
        fi
    else
        ((DECODE_COUNT++))
    fi
done
echo "Found $DECODE_COUNT AIS Messages"