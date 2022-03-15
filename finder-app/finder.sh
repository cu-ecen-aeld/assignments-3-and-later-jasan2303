#!/bin/sh
filesdir=$1 
searchstr=$2

#Checks for argumnets and prints message accordingly
if [ ! $# -eq 2 ]
then 
 echo " $0 : Not Enough arguments"
 echo " $0 Error: Invalid number of arguments."
 echo " Total number of argument should be 2." 
 echo " The order of the arguments should be:"
 echo "   1. Path to the directory on the file system "
 echo "   2. String to be searched in these files"
 exit 1
fi

# Checks if the entered directory is a valid directory on file system
if [ ! -d $filesdir ]
then
 echo " ${filesdir} is not a directory on filesystem"
 exit 1
fi

#Computes number of files in the given directory
X=$( ls ${filesdir} | wc -l )

#Counts for the number search found
Y=$( grep -r $searchstr $filesdir | wc -l )

#prints the required data
echo " The number of files are ${X} and the number of matching lines are ${Y} "
