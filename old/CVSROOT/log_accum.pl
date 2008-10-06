#!/usr/bin/perl -w
#
# Perl filter to handle the log messages from the checkin of files in
# a directory.  This script will group the lists of files by log
# message, and mail a single consolidated log message at the end of
# the commit.
#
# This file assumes a pre-commit checking program that leaves the
# names of the first and last commit directories in a temporary file.
#
# Contributed by David Hampton <hampton@cisco.com>
# Roy Fielding removed useless code and added log/mail of new files
# Ken Coar added special processing (i.e., no diffs) for binary files
#
# $Id$

############################################################
#
# Configurable options
#
############################################################
#
# Where do you want the RCS ID and delta info?
# 0 = none,
# 1 = in mail only,
# 2 = rcsids in both mail and logs.
#
$rcsidinfo = 2;

############################################################
#
# Constants
#
############################################################
$STATE_NONE    = 0;
$STATE_CHANGED = 1;
$STATE_ADDED   = 2;
$STATE_REMOVED = 3;
$STATE_LOG     = 4;

$TMPDIR        = $ENV{'TMPDIR'} || '/tmp';
$FILE_PREFIX   = '#cvs.';

$LAST_FILE     = "$TMPDIR/${FILE_PREFIX}lastdir";
$CHANGED_FILE  = "$TMPDIR/${FILE_PREFIX}files.changed";
$ADDED_FILE    = "$TMPDIR/${FILE_PREFIX}files.added";
$REMOVED_FILE  = "$TMPDIR/${FILE_PREFIX}files.removed";
$LOG_FILE      = "$TMPDIR/${FILE_PREFIX}files.log";
$BRANCH_FILE   = "$TMPDIR/${FILE_PREFIX}files.branch";
$SUMMARY_FILE  = "$TMPDIR/${FILE_PREFIX}files.summary";

$CVSROOT       = $ENV{'CVSROOT'};

$MAIL_TO       = 'iputty-cvs@lists.kldp.net';
#$MLISTHOST     = 'lists.sourceforge.net';

############################################################
#
# Subroutines
#
############################################################

sub format_names {
    local($dir, @files) = @_;
    local(@lines);

    $lines[0] = sprintf(" %-08s", $dir);
    foreach $file (@files) {
	if (length($lines[$#lines]) + length($file) > 60) {
	    $lines[++$#lines] = sprintf(" %8s", " ");
	}
	$lines[$#lines] .= " ".$file;
    }
    @lines;
}

sub cleanup_tmpfiles {
    local(@files);

    opendir(DIR, $TMPDIR);
    push(@files, grep(/^${FILE_PREFIX}.*\.${id}$/, readdir(DIR)));
    closedir(DIR);
    foreach (@files) {
	unlink "$TMPDIR/$_";
    }
}

sub write_logfile {
    local($filename, @lines) = @_;

    open(FILE, ">$filename") || die ("Cannot open log file $filename: $!\n");
    print(FILE join("\n", @lines), "\n");
    close(FILE);
}

sub append_to_file {
    local($filename, $dir, @files) = @_;

    if (@files) {
	local(@lines) = &format_names($dir, @files);
	open(FILE, ">>$filename") || die ("Cannot open file $filename: $!\n");
	print(FILE join("\n", @lines), "\n");
	close(FILE);
    }
}

sub write_line {
    local($filename, $line) = @_;

    open(FILE, ">$filename") || die("Cannot open file $filename: $!\n");
    print(FILE $line, "\n");
    close(FILE);
}

sub append_line {
    local($filename, $line) = @_;

    open(FILE, ">>$filename") || die("Cannot open file $filename: $!\n");
    print(FILE $line, "\n");
    close(FILE);
}

sub read_line {
    local($filename) = @_;
    local($line);

    open(FILE, "<$filename") || die("Cannot open file $filename: $!\n");
    $line = <FILE>;
    close(FILE);
    chomp($line);
    $line;
}

sub read_file {
    local($filename, $leader) = @_;
    local(@text) = ();

    open(FILE, "<$filename") || return ();
    while (<FILE>) {
	chomp;
	push(@text, sprintf("  %-10s  %s", $leader, $_));
	$leader = "";
    }
    close(FILE);
    @text;
}

sub read_logfile {
    local($filename, $leader) = @_;
    local(@text) = ();

    open(FILE, "<$filename") || die ("Cannot open log file $filename: $!\n");
    while (<FILE>) {
	chomp;
	push(@text, $leader.$_);
    }
    close(FILE);
    @text;
}

#
# do an 'cvs -Qn status' on each file in the arguments, and extract info.
#
sub change_summary {
    local($out, @filenames) = @_;
    local(@revline);
    local($file, $rev, $rcsfile, $line);

    while (@filenames) {
	$file = shift @filenames;

	if ("$file" eq "") {
	    next;
	}

	open(RCS, "-|") || exec 'cvs', '-Qn', 'status', $file;

	$rev = "";
	$delta = "";
	$rcsfile = "";


	while (<RCS>) {
	    if (/^[ \t]*Repository revision/) {
		chomp;
		@revline = split(' ', $_);
		$rev = $revline[2];
		$rcsfile = $revline[3];
		$rcsfile =~ s,^$CVSROOT/,,;
		$rcsfile =~ s/,v$//;
	    }
	}
	close(RCS);


	if ($rev ne '' && $rcsfile ne '') {
	    open(RCS, "-|") || exec 'cvs', '-Qn', 'log', "-r$rev", $file;
	    while (<RCS>) {
		if (/^date:/) {
		    chomp;
		    $delta = $_;
		    $delta =~ s/^.*;//;
		    $delta =~ s/^[\s]+lines://;
		}
	    }
	    close(RCS);
	}

	$diff = "\n\n";

	#
	# If this is a binary file, don't try to report a diff; not only is
	# it meaningless, but it also screws up some mailers.  We rely on
	# Perl's 'is this binary' algorithm; it's pretty good.  But not
	# perfect.
	#
	if (($file =~ /\.(?:pdf|gif|jpg|mpg)$/i) || (-B $file)) {
	    $diff .= "\t<<Binary file>>\n\n";
	}
	else {
	    #
	    # Get the differences between this and the previous revision,
	    # being aware that new files always have revision '1.1' and
	    # new branches always end in '.n.1'.
	    #
	    if ($rev =~ /^(.*)\.([0-9]+)$/) {
		$prev = $2 - 1;
		$prev_rev = $1 . '.' .  $prev;

		$prev_rev =~ s/\.[0-9]+\.0$//;# Truncate if first rev on branch

		if ($rev eq '1.1') {
		    open(DIFF, "-|")
			|| exec 'cvs', '-Qn', 'update', '-p', '-r1.1', $file;
		    $diff .= "Index: $file\n=================================="
			. "=================================\n";
		}
		else {
		    open(DIFF, "-|")
			|| exec 'cvs', '-Qn', 'diff', '-u',
			      "-r$prev_rev", "-r$rev", $file;
		}

		while (<DIFF>) {
		    $diff .= $_;
		}
		close(DIFF);
		$diff .= "\n\n";
	    }
	}

	&append_line($out, sprintf("%-9s%-12s%s%s", $rev, $delta,
				   $rcsfile, $diff));
    }
}


sub build_header {
    local($header);
    delete $ENV{'TZ'};
    local($sec,$min,$hour,$mday,$mon,$year) = localtime(time);

    $header = sprintf("%-8s    %02d/%02d/%02d %02d:%02d:%02d",
		       $login, $year%100, $mon+1, $mday,
		       $hour, $min, $sec);
}

# !!! Mailing-list and history file mappings here !!!
sub mlist_map
{
    local($path) = @_;
   
    if ($path =~ /^([^\/]+)/) { return $1; }
    else                      { return 'apache'; }
}    

sub do_changes_file
{
    local($category, @text) = @_;
    local($changes);

    $changes = "$CVSROOT/CVSROOT/commitlogs/$category";

    if (open(CHANGES, ">>$changes")) {
        print(CHANGES join("\n", @text), "\n\n");
        close(CHANGES);
    }
    else { 
        warn "Cannot open $changes: $!\n";
    }
}

sub mail_notification
{
    local(@text) = @_;

    print "Mailing the commit message...\n";

    open(MAIL, "| /usr/sbin/sendmail $MAIL_TO");

    $subject = "Subject: $ARGV[0]";
    if ($subject ne "") {
	print(MAIL $subject, "\n");
    }
    print(MAIL "To: iPuTTY Mailing List <$MAIL_TO>\n");
    print(MAIL "\n");
    print(MAIL join("\n", @text));

    close(MAIL);
}

#############################################################
#
# Main Body
#
############################################################
#
# Setup environment
#
umask (002);

#
# Initialize basic variables
#
$id = getpgrp();
$state = $STATE_NONE;
$login = $ENV{'USER'} || getlogin || (getpwuid($<))[0] || sprintf("uid#%d",$<);
@files = split(' ', $ARGV[0]);
@path = split('/', $files[0]);
#$repository = $path[0];
if ($#path == 0) {
    $dir = ".";
} else {
    $dir = join('/', @path[1..$#path]);
}
#print("ARGV  - ", join(":", @ARGV), "\n");
#print("files - ", join(":", @files), "\n");
#print("path  - ", join(":", @path), "\n");
#print("dir   - ", $dir, "\n");
#print("id    - ", $id, "\n");

#
# Map the repository directory to a name for commitlogs.
#
$mlist = &mlist_map($files[0]);

##########################
# Uncomment the following if we ever have per-repository cvs mail
#
# if (defined($mlist)) {
#     $MAIL_TO = $mlist . "-cvs\@$MLISTHOST";
# }
# else { undef $MAIL_TO; }

##########################
#
# Check for a new directory first.  This will always appear as a
# single item in the argument list, and an empty log message.
#
if ($ARGV[0] =~ /New directory/) {
    $header = &build_header;
    @text = ();
    push(@text, $header);
    push(@text, "");
    push(@text, "  ".$ARGV[0]);
    &do_changes_file($mlist, @text);
    &mail_notification(@text) if defined($MAIL_TO);
    exit 0;
}

#
# Iterate over the body of the message collecting information.
#
while (<STDIN>) {
    chomp;			# Drop the newline
    if (/^Revision\/Branch:/) {
        s,^Revision/Branch:,,;
        push (@branch_lines, split);
        next;
    }
#    next if (/^[ \t]+Tag:/ && $state != $STATE_LOG);
    if (/^Modified Files/) { $state = $STATE_CHANGED; next; }
    if (/^Added Files/)    { $state = $STATE_ADDED;   next; }
    if (/^Removed Files/)  { $state = $STATE_REMOVED; next; }
    if (/^Log Message/)    { $state = $STATE_LOG;     next; }
    s/[ \t\n]+$//;		# delete trailing space
    
    push (@changed_files, split) if ($state == $STATE_CHANGED);
    push (@added_files,   split) if ($state == $STATE_ADDED);
    push (@removed_files, split) if ($state == $STATE_REMOVED);
    if ($state == $STATE_LOG) {
	if (/^PR:$/i ||
	    /^Reviewed by:$/i ||
	    /^Submitted by:$/i ||
	    /^Obtained from:$/i) {
	    next;
	}
	push (@log_lines,     $_);
    }
}

#
# Strip leading and trailing blank lines from the log message.  Also
# compress multiple blank lines in the body of the message down to a
# single blank line.
# (Note, this only does the mail and changes log, not the rcs log).
#
while ($#log_lines > -1) {
    last if ($log_lines[0] ne "");
    shift(@log_lines);
}
while ($#log_lines > -1) {
    last if ($log_lines[$#log_lines] ne "");
    pop(@log_lines);
}
for ($i = $#log_lines; $i > 0; $i--) {
    if (($log_lines[$i - 1] eq "") && ($log_lines[$i] eq "")) {
	splice(@log_lines, $i, 1);
    }
}

#
# Find the log file that matches this log message
#
for ($i = 0; ; $i++) {
    last if (! -e "$LOG_FILE.$i.$id");
    @text = &read_logfile("$LOG_FILE.$i.$id", "");
    last if ($#text == -1);
    last if (join(" ", @log_lines) eq join(" ", @text));
}

#
# Spit out the information gathered in this pass.
#
&write_logfile("$LOG_FILE.$i.$id", @log_lines);
&append_to_file("$BRANCH_FILE.$i.$id",  $dir, @branch_lines);
&append_to_file("$ADDED_FILE.$i.$id",   $dir, @added_files);
&append_to_file("$CHANGED_FILE.$i.$id", $dir, @changed_files);
&append_to_file("$REMOVED_FILE.$i.$id", $dir, @removed_files);
if ($rcsidinfo) {
    &change_summary("$SUMMARY_FILE.$i.$id", (@changed_files, @added_files));
}

#
# Check whether this is the last directory.  If not, quit.
#
if (-e "$LAST_FILE.$id") {
   $_ = &read_line("$LAST_FILE.$id");
   $tmpfiles = $files[0];
   $tmpfiles =~ s,([^a-zA-Z0-9_/]),\\$1,g;
   if (! grep(/$tmpfiles$/, $_)) {
	print "More commits to come...\n";
	exit 0
   }
}

#
# This is it.  The commits are all finished.  Lump everything together
# into a single message, fire a copy off to the mailing list, and drop
# it on the end of the Changes file.
#
$header = &build_header;

#
# Produce the final compilation of the log messages
#
@text = ();
push(@text, $header);
push(@text, "");
for ($i = 0; ; $i++) {
    last if (! -e "$LOG_FILE.$i.$id");
    push(@text, &read_file("$BRANCH_FILE.$i.$id", "Branch:"));
    push(@text, &read_file("$CHANGED_FILE.$i.$id", "Modified:"));
    push(@text, &read_file("$ADDED_FILE.$i.$id", "Added:"));
    push(@text, &read_file("$REMOVED_FILE.$i.$id", "Removed:"));
    push(@text, "  Log:");
    push(@text, &read_logfile("$LOG_FILE.$i.$id", "  "));
    if ($rcsidinfo == 2) {
	if (-e "$SUMMARY_FILE.$i.$id") {
	    push(@text, "  ");
	    push(@text, "  Revision  Changes    Path");
	    push(@text, &read_logfile("$SUMMARY_FILE.$i.$id", "  "));
	}
    }
    push(@text, "");
}
#
# Append the log message to the commitlogs/<module> file
#
&do_changes_file($mlist, @text);
#
# Now generate the extra info for the mail message..
#
if ($rcsidinfo == 1) {
    $revhdr = 0;
    for ($i = 0; ; $i++) {
	last if (! -e "$LOG_FILE.$i.$id");
	if (-e "$SUMMARY_FILE.$i.$id") {
	    if (!$revhdr++) {
		push(@text, "Revision  Changes    Path");
	    }
	    push(@text, &read_logfile("$SUMMARY_FILE.$i.$id", ""));
	}
    }
    if ($revhdr) {
	push(@text, "");	# consistancy...
    }
}
#
# Mail out the notification.
#
&mail_notification(@text) if defined($MAIL_TO);
&cleanup_tmpfiles;
exit 0;

