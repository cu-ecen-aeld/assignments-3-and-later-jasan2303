#!/bin/bash

writefile=$1
writestr=$2

if [ ! $# -eq 2 ]
then
 echo " $0 Error: Invalid number of arguments."
 echo " Total number of argument should be 2." 
 echo " The order of the arguments should be:"
 echo "   1. Path to the file "
 echo "   2. String to be written in the specified file"
exit 1
fi

 Directory="$(dirname "${writefile}")" ;  Filename="$(basename "${writefile}")"

echo " [${Directory}] [${Filename}]"

mkdir $Directory

touch $writefile
echo $writestr >$writefile
