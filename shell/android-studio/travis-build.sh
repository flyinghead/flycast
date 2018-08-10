#!/bin/bash
set -ev
if [ "${TRAVIS_PULL_REQUEST}" = "false" ]; then
	./gradlew build
else
	./gradlew assembleDebug
fi
