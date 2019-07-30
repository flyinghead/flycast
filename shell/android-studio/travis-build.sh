#!/bin/bash
set -ev
#if [ "${TRAVIS_PULL_REQUEST}" = "false" ]; then
#	./gradlew build --configure-on-demand --console=plain
#else
	./gradlew assembleDreamcastDebug  --configure-on-demand --console=plain
#fi
