#!/usr/bin/perl
###
# $Id: dspam-retrain-forward.pl,v0.6 2011/08/11 22:20:04 sbajic Exp $
#
# Copyright 2006-2011, Stevan Bajic <stevan@bajic.ch>, all rights reserved.
# Distributed under the terms of the GNU Affero General Public License v3
#
# Purpose: Script for retraining DSPAM
###

use strict;
use vars qw { %CONFIG %ERROR_CODE %SIGNATURES @MESSAGE };

#
# Default DSPAM enviroment
#
$CONFIG{'BIN_DIR'}              = "/usr/bin";         # Path where the DSPAM binary is located.
$CONFIG{'FULL_EMAIL'}           = 1;                  # Use full email as username or only the
                                                      # local part (that's everything before the @)
                                                      # for DSPAM. 1 = yes, 0 = no
$CONFIG{'HEADERS_ONLY'}         = 1;                  # If you wish to parse the X-DSPAM-Signature
                                                      # header, then set this to 1 (=yes) or set it
                                                      # to 0 (=no) if you only wish to parse the
                                                      # signature found in body of the message.
                                                      # The headers will be parsed in case insensitive
                                                      # mode, since Outlook 2003 coverts the DSPAM
                                                      # header into lower case when forwarding mails.
                                                      # Headers will be even processed if they are in
                                                      # attachments.
$CONFIG{'BODIES_ONLY'}          = 0;                  # If you wish to parse the !DSPAM:...! signature
                                                      # in the body, then set this to 1 (=yes) or set
                                                      # it to 0 (=no) if you only wish to parse the
                                                      # signature found in header of the message.
                                                      # Bodies will be even processed if they are in
                                                      # attachments.
$CONFIG{'FIRST_SIGNATURE_ONLY'} = 0;                  # Should the retrain script process the complete
                                                      # mail and search for all the signature(s) it can
                                                      # find or should it stop after the first signature
                                                      # is find? Set this value to 0 (=no) to process/
                                                      # parse the complete mail and search for
                                                      # signature(s) or set it to 1 (=yes) to stop
                                                      # processing the mail after the first found
                                                      # signature.
$CONFIG{'SKIP_FIRST'}           = 0;                  # Should the retrain script skip the first
                                                      # signature it finds? Set this value to 0 (=no) or
                                                      # set it to 1 (=yes).
$CONFIG{'DEBUG'}                = 0;                  # 0 (=no) and 1 (=yes). Setting this to 1 (=yes)
                                                      # will not send anything to DSPAM, but print out
                                                      # some info about what the script would execute
                                                      # when running without the debug flag.

### DO NOT CONFIGURE ANYTHING BELOW THIS LINE

#
# Default values
#
$CONFIG{'DSPAM_BIN'}            = $CONFIG{'BIN_DIR'} . "/dspam";
$CONFIG{'MODE'}                 = "";
$CONFIG{'CLASS'}                = "";
$CONFIG{'SOURCE'}               = "";
$CONFIG{'SENDER'}               = "";
$CONFIG{'RECIPIENT'}            = "";
$CONFIG{'USER'}                 = "";
$CONFIG{'PARAMETERS'}           = "";

#
# Exit codes from <sysexits.h>
#
$ERROR_CODE{'EX_OK'}            = 0;                  # successful termination
$ERROR_CODE{'EX_USAGE'}         = 64;                 # command line usage error
$ERROR_CODE{'EX_UNAVAILABLE'}   = 69;                 # service unavailable
$ERROR_CODE{'EX_TEMPFAIL'}      = 75;                 # temp failure; user is invited to retry
$ERROR_CODE{'EX_CONFIG'}        = 78;                 # configuration error

#
# Get arguments
#
while ($_ = $ARGV[0], /^\-\-/) {
  last if /^\-\-$/;
  if ($ARGV[0] =~ /^\-\-help$/) {
    print STDERR "Usage: $0\n";
    print STDERR "  [--help]\n";
    print STDERR "    This help screen.\n";
    print STDERR "  [--debug=[yes|no]]\n";
    print STDERR "    Turn on debugging. No action will be performed, just printed.\n";
    print STDERR "  [--user username]]\n";
    print STDERR "    User name to use for training.\n";
    print STDERR "  [--client]\n";
    print STDERR "    To run in client mode.\n";
    print STDERR "  [--class=spam|innocent]\n";
    print STDERR "    Class used for the training.\n";
    print STDERR "  [--mode=teft|toe|tum|notrain|unlearn]\n";
    print STDERR "    Configures the training mode to be used for this process, overriding\n";
    print STDERR "    any defaults in dspam.conf or the preference extension.\n";
    print STDERR "  [--source=error|corpus|inoculation]\n";
    print STDERR "    The source tells DSPAM how to learn the message being presented.\n";
    print STDERR "  [--full-email=[yes|no]]\n";
    print STDERR "    Use the full email address as username or just the local part.\n";
    print STDERR "  [--headers-only=[yes|no]]\n";
    print STDERR "    Process only headers (aka: X-DSPAM-Signature) of the message.\n";
    print STDERR "  [--bodies-only=[yes|no]]\n";
    print STDERR "    Process only bodies (aka: !DSPAM:xxx!) of the message.\n";
    print STDERR "  [--first-only=[yes|no]]\n";
    print STDERR "    Only use the first found DSPAM signature and skip the others.\n";
    print STDERR "  [--skip-first=[yes|no]]\n";
    print STDERR "    Skip the first found signature.\n";
    print STDERR "  [--mail-from=sender-address]\n";
    print STDERR "    Set the MAIL FROM sent on delivery of the message (only valid for\n";
    print STDERR "    SMTP/LMTP delivery).\n";
    print STDERR "  [--rcpt-to=recipient-address]\n";
    print STDERR "    Define the RCPT TO which will be used for the delivery (only valid\n";
    print STDERR "    for SMTP/LMTP delivery).\n";
    print STDERR "  [--bin-dir=path]\n";
    print STDERR "    Define the path where dspam/dspamc is located.\n";
    exit $ERROR_CODE{'EX_USAGE'};
  } elsif ($ARGV[0] =~ /^\-\-debug$/) {
    $CONFIG{'DEBUG'} = 1;
    shift;
  } elsif ($ARGV[0] =~ /^\-\-debug=(.*)$/) {
    if ($1 =~ /^(yes|no)$/i) {
      if ($1 =~ /^yes$/i) {
        $CONFIG{'DEBUG'} = 1;
      } else {
        $CONFIG{'DEBUG'} = 0;
      }
      shift;
    } else {
      print STDERR "Debug must be either yes or no.\n" if ($CONFIG{'DEBUG'} == 1);
      exit $ERROR_CODE{'EX_USAGE'};
    }
  } elsif ($ARGV[0] =~ /^\-\-client$/) {
    $CONFIG{'DSPAM_BIN'} = $CONFIG{'BIN_DIR'} . "/dspamc";
    shift;
  } elsif ($ARGV[0] =~ /^(\-\-user)$/) {
    if (defined($ARGV[1])) {
      $CONFIG{'USER'} = $ARGV[1];
      shift;
    } else {
      print STDERR "You need to specify a username for the retraining.\n" if ($CONFIG{'DEBUG'} == 1);
      exit $ERROR_CODE{'EX_USAGE'};
    }
    shift;
  } elsif ($ARGV[0] =~ /^\-\-class=(.+)$/) {
    $CONFIG{'CLASS'} = $1;
    shift;
  } elsif ($ARGV[0] =~ /^\-\-mode=(.+)$/) {
    $CONFIG{'MODE'} = $1;
    shift;
  } elsif ($ARGV[0] =~ /^\-\-source=(.+)$/) {
    $CONFIG{'SOURCE'} = $1;
    shift;
  } elsif ($ARGV[0] =~ /^\-\-full\-email$/) {
    $CONFIG{'FULL_EMAIL'} = 1;
    shift;
  } elsif ($ARGV[0] =~ /^\-\-full\-email=(.*)$/) {
    if ($1 =~ /^(yes|no)$/i) {
      if ($1 =~ /^yes$/i) {
        $CONFIG{'FULL_EMAIL'} = 1;
      } else {
        $CONFIG{'FULL_EMAIL'} = 0;
      }
    } else {
      print STDERR "Full email must be either yes or no.\n" if ($CONFIG{'DEBUG'} == 1);
      exit $ERROR_CODE{'EX_USAGE'};
    }
    shift;
  } elsif ($ARGV[0] =~ /^\-\-headers\-only$/) {
    $CONFIG{'HEADERS_ONLY'} = 1;
    shift;
  } elsif ($ARGV[0] =~ /^\-\-headers\-only=(.*)$/) {
    if ($1 =~ /^(yes|no)$/i) {
      if ($1 =~ /^yes$/i) {
        $CONFIG{'HEADERS_ONLY'} = 1;
      } else {
        $CONFIG{'HEADERS_ONLY'} = 0;
      }
    } else {
      print STDERR "Header only must be either yes or no.\n" if ($CONFIG{'DEBUG'} == 1);
      exit $ERROR_CODE{'EX_USAGE'};
    }
    shift;
  } elsif ($ARGV[0] =~ /^\-\-bodies\-only$/) {
    $CONFIG{'BODIES_ONLY'} = 1;
    shift;
  } elsif ($ARGV[0] =~ /^\-\-bodies\-only=(.*)$/) {
    if ($1 =~ /^(yes|no)$/i) {
      if ($1 =~ /^yes$/i) {
        $CONFIG{'BODIES_ONLY'} = 1;
      } else {
        $CONFIG{'BODIES_ONLY'} = 0;
      }
    } else {
      print STDERR "Bodies only must be either yes or no.\n" if ($CONFIG{'DEBUG'} == 1);
      exit $ERROR_CODE{'EX_USAGE'};
    }
    shift;
  } elsif ($ARGV[0] =~ /^\-\-first\-only$/) {
    $CONFIG{'FIRST_SIGNATURE_ONLY'} = 1;
    shift;
  } elsif ($ARGV[0] =~ /^\-\-first\-only=(.*)$/) {
    if ($1 =~ /^(yes|no)$/i) {
      if ($1 =~ /^yes$/i) {
        $CONFIG{'FIRST_SIGNATURE_ONLY'} = 1;
      } else {
        $CONFIG{'FIRST_SIGNATURE_ONLY'} = 0;
      }
    } else {
      print STDERR "First match only must be either yes or no.\n" if ($CONFIG{'DEBUG'} == 1);
      exit $ERROR_CODE{'EX_USAGE'};
    }
    shift;
  } elsif ($ARGV[0] =~ /^\-\-skip\-first$/) {
    $CONFIG{'SKIP_FIRST'} = 1;
    shift;
  } elsif ($ARGV[0] =~ /^\-\-skip\-first=(.*)$/) {
    if ($1 =~ /^(yes|no)$/i) {
      if ($1 =~ /^yes$/i) {
        $CONFIG{'SKIP_FIRST'} = 1;
      } else {
        $CONFIG{'SKIP_FIRST'} = 0;
      }
    } else {
      print STDERR "Skip fist must be either yes or no.\n" if ($CONFIG{'DEBUG'} == 1);
      exit $ERROR_CODE{'EX_USAGE'};
    }
    shift;
  } elsif ($ARGV[0] =~ /^\-\-mail\-from=(.+)$/) {
    $CONFIG{'SENDER'} = $1;
    shift;
  } elsif ($ARGV[0] =~ /^(\-\-rcpt\-to)$/) {
    if (defined($ARGV[1])) {
      $CONFIG{'RECIPIENT'} = $ARGV[1];
      shift;
    } else {
      print STDERR "You need to specify a recipient when using --rcpt-to switch.\n" if ($CONFIG{'DEBUG'} == 1);
      exit $ERROR_CODE{'EX_USAGE'};
    }
    shift;
  } elsif ($ARGV[0] =~ /^\-\-bin\-dir=(.*)$/) {
    if (-d "$1") {
      $CONFIG{'BIN_DIR'} = $1;
      $CONFIG{'DSPAM_BIN'} = $CONFIG{'BIN_DIR'} . "/dspam";
    } else {
      print STDERR "The directory '" . $CONFIG{'BIN_DIR'} . "' does not exist.\n" if ($CONFIG{'DEBUG'} == 1);
      exit $ERROR_CODE{'EX_USAGE'};
    }
    shift;
  } else {
    shift;
  }
}

#
# Do some error checking
#
if ($CONFIG{'HEADERS_ONLY'} == 1 && $CONFIG{'BODIES_ONLY'} == 1) {
  print STDERR "You can not force header only AND body only processing at the same time.\n" if ($CONFIG{'DEBUG'} == 1);
  exit $ERROR_CODE{'EX_USAGE'};
}

#
# Do not continue if DSPAM binary does not exist
#
if (($CONFIG{'DSPAM_BIN'} eq "") || !(-e $CONFIG{'DSPAM_BIN'}) || !(-r $CONFIG{'DSPAM_BIN'})) {
  print STDERR "ERROR: DSPAM binary '" . $CONFIG{'DSPAM_BIN'} . "' does not exist.\n" if ($CONFIG{'DEBUG'} == 1);
  exit $ERROR_CODE{'EX_USAGE'};
}

#
# Process username for DSPAM
#
if ($CONFIG{'USER'} eq "") {
  if ($CONFIG{'RECIPIENT'} ne "") {
    if ($CONFIG{'RECIPIENT'} =~ /^(ham|(no[nt]?)?spam)\-?(.+)$/) {
      $CONFIG{'USER'} = $3;
    }
    if ($CONFIG{'RECIPIENT'} =~ /^(ham|(no[nt]?)spam)\-?(.+)$/) {
      $CONFIG{'CLASS'} = "innocent";
    } elsif ($CONFIG{'RECIPIENT'} =~ /^spam\-?(.+)$/) {
      $CONFIG{'CLASS'} = "spam";
    }
  } elsif ($CONFIG{'SENDER'} =~ /^(.+)$/) {
    $CONFIG{'USER'} = $CONFIG{'SENDER'};
  } else {
    $CONFIG{'USER'} = (getpwuid($<))[0];
  }
}
if (($CONFIG{'FULL_EMAIL'} == 0) && ($CONFIG{'USER'} =~ /^(.+)@(.+)$/)) {
  $CONFIG{'USER'} = $1;
}
if ($CONFIG{'USER'} eq "") {
  print STDERR "ERROR: Can't determine user.\n" if ($CONFIG{'DEBUG'} == 1);
  exit $ERROR_CODE{'EX_TEMPFAIL'};
}

#
# Check if class is known by DSPAM
#
if ($CONFIG{'CLASS'} eq "") {
  print STDERR "ERROR: Class can not be empty.\n" if ($CONFIG{'DEBUG'} == 1);
  exit $ERROR_CODE{'EX_USAGE'};
} elsif ($CONFIG{'CLASS'} !~ /^(spam|innocent)$/) {
  open(DSPAM, "$CONFIG{'DSPAM_BIN'} --help 2>&1|");
  while(<DSPAM>) {
    chomp;
    if (/^.*\-\-class=\[([^\]]+).*$/) {
      if ($CONFIG{'CLASS'} !~ /^($1)$/) {
        close(DSPAM);
        print STDERR "ERROR: Class '" . $CONFIG{'CLASS'} . "' is not known by DSPAM.\n" if ($CONFIG{'DEBUG'} == 1);
        exit $ERROR_CODE{'EX_USAGE'};
      }
      last;
    }
  }
  close(DSPAM);
}

#
# Check if source is known by DSPAM
#
if ($CONFIG{'SOURCE'} eq "") {
  print STDERR "ERROR: Source can not be empty.\n" if ($CONFIG{'DEBUG'} == 1);
  exit $ERROR_CODE{'EX_USAGE'};
} elsif ($CONFIG{'SOURCE'} !~ /^(error|corpus|inoculation)$/) {
  open(DSPAM, "$CONFIG{'DSPAM_BIN'} --help 2>&1|");
  while(<DSPAM>) {
    chomp;
    if (/^.*\-\-source=\[([^\]]+).*$/) {
      if ($CONFIG{'SOURCE'} !~ /^($1)$/) {
        close(DSPAM);
        print STDERR "ERROR: Source '" . $CONFIG{'SOURCE'} . "' is not known by DSPAM.\n" if ($CONFIG{'DEBUG'} == 1);
        exit $ERROR_CODE{'EX_USAGE'};
      }
      last;
    }
  }
  close(DSPAM);
}

#
# Check if mode is known by DSPAM
#
if ($CONFIG{'MODE'} ne "") {
  if ($CONFIG{'MODE'} !~ /^(toe|tum|teft|notrain|unlearn)$/) {
    open(DSPAM, "$CONFIG{'DSPAM_BIN'} --help 2>&1|");
    while(<DSPAM>) {
      chomp;
      if (/^.*\-\-mode=\[([^\]]+).*$/) {
        if ($CONFIG{'MODE'} !~ /^($1)$/) {
          close(DSPAM);
          print STDERR "ERROR: Mode '" . $CONFIG{'MODE'} . "' is not known by DSPAM.\n" if ($CONFIG{'DEBUG'} == 1);
          exit $ERROR_CODE{'EX_USAGE'};
        }
        last;
      }
    }
    close(DSPAM);
  }
}

#
# Additional parameters to add when calling DSPAM
#
$CONFIG{'PARAMETERS'} = $CONFIG{'PARAMETERS'} . " --mode=" . quotemeta($CONFIG{'MODE'}) if ($CONFIG{'MODE'} ne "");

#
# Send complete message to DSPAM if source is inoculation or corups
#
if ($CONFIG{'SOURCE'} =~ /^(inoculation|corpus)$/) {
  if ($CONFIG{'DEBUG'} == 1) {
    print STDERR "DEBUG: Piping message to: $CONFIG{'DSPAM_BIN'} --source=" . quotemeta($CONFIG{'SOURCE'}) ." --class=" . quotemeta($CONFIG{'CLASS'}) . $CONFIG{'PARAMETERS'} . " --user " . quotemeta($CONFIG{'USER'}) . "\n";
    exit $ERROR_CODE{'EX_OK'};
  } else {
    @MESSAGE = <>;
    open(DSPAM, "|$CONFIG{'DSPAM_BIN'} --source=" . quotemeta($CONFIG{'SOURCE'}) ." --class=" . quotemeta($CONFIG{'CLASS'}) . $CONFIG{'PARAMETERS'} . " --user " . quotemeta($CONFIG{'USER'}));
    print DSPAM @MESSAGE;
    close(DSPAM);
    exit $ERROR_CODE{'EX_OK'};
  }
}

#
# Extract DSPAM signature(s) and pass them to DSAPM
#
my $header = 1;
my $sig_counter = 0;
while (<>) {
  chomp;
  $header = 0 if (/^$/);
  #last if ($header == 0 && $CONFIG{'HEADERS_ONLY'} == 1);
  if (/^(X\-DSPAM\-Signature|[\t ]*!DSPAM):/i) {
    if ($CONFIG{'HEADERS_ONLY'} == 1 || $CONFIG{'BODIES_ONLY'} == 0) {
      if (/^X\-DSPAM\-Signature:[\t ](([0-9]+[,]{1})?([a-f0-9]+))[\t ]*$/i) {
        if ($SIGNATURES{$1} != 1) {
          $sig_counter++;
          if ($sig_counter == 1 && $CONFIG{'SKIP_FIRST'} == 1) {
            if ($CONFIG{'DEBUG'} == 1) {
              print STDERR "DEBUG: Found DSPAM signature '" . $1 . "' in header";
              print STDERR " (attachment)" if ($header == 0);
              print STDERR ".\n";
              print STDERR "DEBUG: By request skipping this DSPAM signature.\n";
            }
          } elsif ($CONFIG{'DEBUG'} == 1) {
            print STDERR "DEBUG: Found DSPAM signature '" . $1 . "' in header";
            print STDERR " (attachment)" if ($header == 0);
            print STDERR ".\n";
            print STDERR "DEBUG: $CONFIG{'DSPAM_BIN'} --source=" . quotemeta($CONFIG{'SOURCE'}) ." --class=" . quotemeta($CONFIG{'CLASS'}) . " --signature=" . quotemeta($1) . $CONFIG{'PARAMETERS'} . " --user " . quotemeta($CONFIG{'USER'}) . "\n";
          } else {
            system("$CONFIG{'DSPAM_BIN'} --source=" . quotemeta($CONFIG{'SOURCE'}) ." --class=" . quotemeta($CONFIG{'CLASS'}) . " --signature=" . quotemeta($1) . $CONFIG{'PARAMETERS'} . " --user " . quotemeta($CONFIG{'USER'}));
          }
          if ($sig_counter == 2 && $CONFIG{'SKIP_FIRST'} == 1 && $CONFIG{'FIRST_SIGNATURE_ONLY'} == 1) {
            last;
          } elsif ($sig_counter == 1 && $CONFIG{'SKIP_FIRST'} == 0 && $CONFIG{'FIRST_SIGNATURE_ONLY'} == 1) {
            last;
          }
          $SIGNATURES{$1} = 1;
        }
      }
    } elsif ($header == 0 && ($CONFIG{'HEADERS_ONLY'} == 0 || $CONFIG{'BODIES_ONLY'} == 1)) {
      if (/^[\t ]*!DSPAM:(([0-9]+[,]{1})?([a-f0-9]+))![\t ]*$/i) {
        if ($SIGNATURES{$1} != 1) {
          $sig_counter++;
          if ($sig_counter == 1 && $CONFIG{'SKIP_FIRST'} == 1) {
            if ($CONFIG{'DEBUG'} == 1) {
              print STDERR "DEBUG: Found DSPAM signature '" . $1 . "' in body.\n";
              print STDERR "DEBUG: By request skipping this DSPAM signature.\n";
            }
          } elsif ($CONFIG{'DEBUG'} == 1) {
            print STDERR "DEBUG: Found DSPAM signature '" . $1 . "' in body.\n";
            print STDERR "DEBUG: $CONFIG{'DSPAM_BIN'} --source=" . quotemeta($CONFIG{'SOURCE'}) ." --class=" . quotemeta($CONFIG{'CLASS'}) . " --signature=" . quotemeta($1) . $CONFIG{'PARAMETERS'} . " --user " . quotemeta($CONFIG{'USER'}) . "\n";
          } else {
            system("$CONFIG{'DSPAM_BIN'} --source=" . quotemeta($CONFIG{'SOURCE'}) ." --class=" . quotemeta($CONFIG{'CLASS'}) . " --signature=" . quotemeta($1) . $CONFIG{'PARAMETERS'} . " --user " . quotemeta($CONFIG{'USER'}));
          }
          if ($sig_counter == 2 && $CONFIG{'SKIP_FIRST'} == 1 && $CONFIG{'FIRST_SIGNATURE_ONLY'} == 1) {
            last;
          } elsif ($sig_counter == 1 && $CONFIG{'SKIP_FIRST'} == 0 && $CONFIG{'FIRST_SIGNATURE_ONLY'} == 1) {
            last;
          }
          $SIGNATURES{$1} = 1;
        }
      }
    }
  } elsif ($CONFIG{'DEBUG'} == 1) {
    if (/(X\-DSPAM\-Signature|[\t ]*!DSPAM):[\t ]*(([0-9]+[,]{1})?([a-f0-9]+))!?.*/i) {
      print STDERR "DEBUG: Found DSPAM signature '" . $2 . "' in message.\n";
      print STDERR "DEBUG: The signature will NOT be used. The original line is:\n";
      print STDERR "DEBUG: $_\n";
    }
  }
}

#
# Exit
#
exit $ERROR_CODE{'EX_OK'};
