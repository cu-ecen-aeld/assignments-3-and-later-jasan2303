#!/bin/bash
filesdir=$1 
searchstr=$2

if [ ! $# -eq 2 ]
then 
 echo " $0 : Not Enough arguments"
 exit 1
fi

if [ ! -d $filesdir ]
then
 echo " ${filesdir} is not a directory on filesystem"
 exit 1
fi

X=$( ls ${filesdir} | wc -l )
Y=$( grep -r $searchstr $filesdir | wc -l )
echo " The number of files are ${X} and the number of matching lines are ${Y} "
