#!/bin/sh
perl mkfiles.pl
# These are text files.
text=`{ find . -name CVS -prune -o \
               -name .cvsignore -prune -o \
               -name .svn -prune -o \
               -name LATEST.VER -prune -o \
               -name CHECKLST.txt -prune -o \
               -name mksrcarc.sh -prune -o \
               -name '*.dsp' -prune -o \
               -name '*.dsw' -prune -o \
               -type f -print | sed 's/^\.\///'; } | \
      grep -ivE 'testdata/.*\.txt|MODULE|putty.iss|website.url' | grep -vF .ico | grep -vF .icns`
# These are files which I'm _sure_ should be treated as text, but
# which zip might complain about, so we direct its moans to
# /dev/null! Apparently its heuristics are doubtful of UTF-8 text
# files.
bintext=testdata/*.txt
# These are actual binary files which we don't want transforming.
bin=`{ ls -1 windows/*.ico windows/putty.iss windows/website.url macosx/*.icns; \
       find . -name '*.dsp' -print -o -name '*.dsw' -print; }`
zip -k -l putty-src.zip $text > /dev/null
zip -k -l putty-src.zip $bintext >& /dev/null
zip -k putty-src.zip $bin > /dev/null
