#!/usr/bin/env python
#
# gottext - in-place translation tool for poor l10n environment.
#
# Copyright(C) 2003 Hye-Shik Chang. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
# IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
# $Id$
#

__doc__ = """\
gottext 1.0, a in-place translation tool for simple l10n.
Usage: gottext.py command [OPTION]... [FILE]...

  -V        display the version of gottext and exit.
  -h        print this help.

Commands:
  extract   extract all strings from [FILE]s into a pot.


`extract' command arguments:

Mail bug reports and suggestions to <perky@FreeBSD.org>.
"""
__copyright__ ="""\
gottext 1.0

Copyright (C) 2003 Hye-Shik Chang.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
"""

import getopt
import os

progname = 'unnamed'

def warnx(msg):
    print "%s: %s" % (progname, msg)

def errx(msg):
    warnx(msg)
    raise SystemExit

def runloop(f, fo, cb):
    linenum = 0
    inquote = incomment = False
    strs = []

    for line in fo:
        linenum += 1
        if line[-2:-1] in '\r\n':
            line = line[:-2]
        else:
            line = line[:-1]

        skip = 0
        strbegins = None
        for i in range(len(line)):
            c = line[i]

            if skip > 0:
                skip -= 1
                continue

            if incomment:
                if c == '*' and line[i:i+2] == '*/':
                    incomment = False
                    skip += 1
            elif not inquote:
                if c == '\\':
                    skip += 1
                elif c == '/' and line[i:i+2] == '//':
                    break
                elif c == '/' and line[i:i+2] == '/*':
                    incomment = True
                    skip += 1
                elif c == '"':
                    inquote = True
                    strbegins = (linenum, i + 1)
                elif not c.isspace() and strs:
                    cb(f, strs[0], strs[1], ''.join(strs[2:]))
                    del strs[:]
                elif c == "'":
                    skip += 2
            else:
                if c == '\\':
                    skip += 1
                elif c == '"':
                    inquote = False
                    if not strs:
                        strs.append(strbegins)
                        strs.append((linenum, i - 1))
                    else:
                        strs[1] = (linenum, i - 1)
                    txtfrag = line[strbegins[1]:i]
                    if txtfrag:
                        strs.append(txtfrag)

def extractcmd(args):
    try:
        optlist, args = getopt.getopt(args, 'o:')
    except getopt.GetoptError, why:
        args, optlist = None, []
        warnx(why)

    outputname = 'output.gt'
    for optch, optarg in optlist:
        if optch == '-o':
            outputname = optarg

    if not args:
        errx("no file is specified.")
    if not outputname:
        errx("output file name is not specified.")

    outf = open(outputname, "w")
    strs = {}

    def runcb(f, begin, end, s):
        if not strs.has_key(s):
            strs[s] = []
        strs[s].append((f, begin, end))

    for f in args:
        print "Processing %s..." % f
        fo = open(f)
        runloop(f, fo, runcb)

    sids = strs.keys()
    sids.sort()
    for k in sids:
        poss = ['']
        for f, begin, end in strs[k]:
            if len(poss[-1]) > 60:
                poss.append('')
            poss[-1] += ' %s:%d' % (f, begin[0])

        if not k:
            continue

        for p in poss:
            print >> outf, "#" + p
        
        soutput = False
        while k:
            if len(k) > 70:
                spt = k.find('\\n')
                if 0 <= spt < 70:
                    spt += 2
                else:
                    spt = 70
                thk, k = k[:spt], k[spt:]
            else:
                thk, k = k, None
            if not soutput:
                print >> outf, 'msgid "%s"' % thk
                soutput = True
            else:
                print >> outf, '"%s"' % thk
        print >> outf, 'msgstr ""'
        print >> outf

    print len(strs)

def patchcmd(args):
    try:
        optlist, args = getopt.getopt(args, 's:i:')
    except getopt.GetoptError, why:
        args, optlist = None, []
        warnx(why)

    extsufx = ''
    srcname = 'output.gt'
    for optch, optarg in optlist:
        if optch == '-i':
            srcname = optarg
        elif optch == '-s':
            extsufx = optarg

    if not args:
        errx("no file is specified.")
    if not srcname:
        errx("translation file name is not specified.")

    print "Loading translation file \"%s\"..." % srcname
    translations = {}
    srclines = open(srcname).readlines()
    lasttup = [[], []]
    lastmode = None
    for i in range(len(srclines)):
        if srclines[i].startswith('msgid ') or (
                lastmode == 'msgid' and srclines[i].startswith('"')):
            try:
                firstquote = srclines[i].find('"')
                lasttup[0].append(srclines[i][firstquote:].strip()[1:-1])
                lastmode = 'msgid'
            except:
                errx("Couldn't parse translation file on line %d" % (i + 1))
        elif srclines[i].startswith('msgstr ') or (
                lastmode == 'msgstr' and srclines[i].startswith('"')):
            try:
                firstquote = srclines[i].find('"')
                lasttup[1].append(srclines[i][firstquote:].strip()[1:-1])
                lastmode = 'msgstr'
            except:
                errx("Couldn't parse translation file on line %d" % (i + 1))
        elif lasttup[0] and lasttup[1]:
            trsrc = ''.join(lasttup[0])
            trdst = ''.join(lasttup[1])
            del lasttup[0][:]
            del lasttup[1][:]
            lastmode = None
            if trsrc == '' or trdst == '':
                continue
            elif translations.has_key(trsrc):
                warnx("Duplicated translation for \"%s\" on line %d." %
                        (trsrc, i + 1))
            else:
                translations[trsrc] = trdst
        elif lasttup[0] or lasttup[1]:
            errx("Unexpected termination on line %d." % (i + 1))
        else:
            lastmode = None

    patchpoints = []
    def runcb(f, begin, end, s):
        # column starts with 0 and row starts with 1
        patchpoints.append([runcb.lastpos, (begin[0], begin[1]-1), None])
        patchpoints.append([begin, end, translations.get(s)])
        runcb.lastpos = (end[0], end[1]+1)

    for f in args:
        del patchpoints[:]
        runcb.lastpos = (1, 0)

        print "Loading %s..." % f
        fo = open(f)
        runloop(f, fo, runcb)

        print " + Finding patchpoints...",
        sys.stdout.flush()
        fo.seek(0)
        flines = fo.readlines()
        if patchpoints:
            patchpoints.append([runcb.lastpos, # till eof
                                (len(flines), len(flines[-1])-1), None])
        for ppts in patchpoints:
            if ppts[2] is not None:
                continue

            begin, end, s = ppts
            if begin[0] == end[0]: # one line string
                ppts[2] = flines[begin[0]-1][begin[1]:end[1]+1]
            else:
                text = []
                text.append(flines[begin[0]-1][begin[1]:])
                text.extend(flines[begin[0]:end[0]-1])
                text.append(flines[end[0]-1][:end[1]+1])
                ppts[2] = ''.join(text)
        print "done with %d patchpoints" % len(patchpoints)

        print " + Writing translated file"
        nf = open(f+extsufx, 'w')
        for begin, end, s in patchpoints:
            nf.write(s)
        print " - Finished"

if __name__ == '__main__':
    import sys

    try:
        optlist, args = getopt.getopt(sys.argv[1:], 'hV')
    except getopt.GetoptError, why:
        args, optlist = None, []
        warnx(why)

    for optch, optarg in optlist:
        if optch == '-h':
            print __doc__,
            raise SystemExit
        elif optch == '-V':
            print __copyright__,
            raise SystemExit

    if not args:
        print __doc__,
        raise SystemExit

    progname = sys.argv[0]
    cmd = args[0]
    if len(os.path.commonprefix([cmd, 'extract'])) >= 2:
        extractcmd(args[1:])
    elif len(os.path.commonprefix([cmd, 'patch'])) >= 2:
        patchcmd(args[1:])
    else:
        errx("unknown command `%s'." % cmd)

# ex: ts=8 sts=4 sw=4 et
