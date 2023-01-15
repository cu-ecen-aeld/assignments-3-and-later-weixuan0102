#!/bin/sh
writefile=$1
writestr=$2

if [ $# != 2 ]
then
	echo "check your parameters."
	exit 1
fi

if ! [ -d $(dirname $writefile) ]
then
	mkdir $(dirname $writefile)
fi

echo $writestr > $writefile

if [ $? -ne 0 ]
then 
	echo "ERROR: the file couldn't be created."
	exit 1
fi
