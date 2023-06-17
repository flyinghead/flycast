#!/bin/sh

if [ -z "$TESTUPNPREPLYPARSE" ] ; then
	TESTUPNPREPLYPARSE=./build/testupnpreplyparse
fi

for f in testreplyparse/*.xml ; do
	bf="`dirname $f`/`basename $f .xml`"
	if $TESTUPNPREPLYPARSE $f $bf.namevalue ; then
		echo "$f : passed"
	else
		echo "$f : FAILED"
		exit 1
	fi
done

exit 0
