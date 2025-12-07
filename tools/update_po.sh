#!/bin/sh
#
# Update .pot and .po files
#
find core shell -not -path "core/deps/*" -a -not -path "*/.c*" -a -type f -a \( -name \*.h -o -name \*.c -o -name \*.cpp -o -name \*.hpp \) > cfiles
xgettext -k -kt -kT -kTnop -kTs --from-code UTF-8 --files-from=cfiles --c++ -o resources/i18n/flycast.pot --join-existing
# TODO java? objective C?
msmerge --update resources/18n/fr.po resources/i18n/flycast.pot
rm cfiles
