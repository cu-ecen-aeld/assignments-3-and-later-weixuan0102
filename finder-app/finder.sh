#!/bin/sh
filesdir=$1
searchstr=$2

if [ $# != 2 ]
then
	echo "check your parameters."
	exit 1;
fi

if [ ! -d "$filesdir" ]
then
	echo "the directory doesn't exist."
	exit 1;
fi

X=$(ls $filesdir | wc -l)
Y=$(grep -r $searchstr $filesdir | wc -l)
echo "The number of files are ${X} and the number of matching lines are ${Y}"
