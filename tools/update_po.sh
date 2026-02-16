#!/bin/sh
#
# Update .pot and .po files. Assumes this script is located in a subdirectory.
#
SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
cd $SCRIPTPATH/..
find core shell -not -path "core/deps/*" -a -not -path "*/.c*" -a -type f -a \( -name \*.h -o -name \*.c -o -name \*.cpp -o -name \*.hpp \) > cfiles
xgettext -k -kt -kT -kTnop -kTs -ktranslateCtx:1c,2 -ktranslatePlural:1,2 --from-code UTF-8 --files-from=cfiles --c++ -o resources/i18n/flycast.pot
# TODO java? objective C? (with --join-existing)
# msgmerge --update resources/i18n/fr.po resources/i18n/flycast.pot
rm -f cfiles
