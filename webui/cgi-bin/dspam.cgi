#!/usr/bin/perl

# $Id: dspam.cgi,v 1.57 2011/06/28 00:13:48 sbajic Exp $
# DSPAM
# COPYRIGHT (C) 2002-2012 DSPAM PROJECT
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

use strict;
use POSIX qw(strftime ctime);
use Time::Local;
use vars qw { %CONFIG %DATA %FORM %LANG $MAILBOX $CURRENT_USER $USER $TMPFILE $USERSELECT };
use vars qw { $CURRENT_STORE $LANGUAGE };

#
# Read configuration parameters common to all CGI scripts
#
if (!(-e "configure.pl") || !(-r "configure.pl")) {
  &htmlheader;
  print "<html><head><title>Error!</title></head><body bgcolor='white' text='black'><center><h1>";
  print "Missing file configure.pl";
  print "</h1></center></body></html>\n";
  exit;
}
require "configure.pl";

#
# The current CGI script
#

$CONFIG{'ME'} = "dspam.cgi";

$| = 1;

#
# Determine which extensions are available
#

if ($CONFIG{'AUTODETECT'} == 1 || $CONFIG{'AUTODETECT'} eq "") {
  $CONFIG{'PREFERENCES_EXTENSION'} = 0;
  $CONFIG{'LARGE_SCALE'} = 0;
  $CONFIG{'DOMAIN_SCALE'} = 0;
  do {
    my $x = `$CONFIG{'DSPAM'} --version`;
    if ($x =~ /--enable-preferences-extension/) {
      $CONFIG{'PREFERENCES_EXTENSION'} = 1;
    }
    if ($x =~ /--enable-large-scale/) {
      $CONFIG{'LARGE_SCALE'} = 1;
    }
    if ($x =~ /--enable-domain-scale/) {
      $CONFIG{'DOMAIN_SCALE'} = 1;
    }
  }
}

#
# Determine admin status
#

$CONFIG{'ADMIN'} = 0;
if ($ENV{'REMOTE_USER'} ne "") {
  open(FILE, "<./admins");
  while(<FILE>) {
    chomp;
    if ($_ eq $ENV{'REMOTE_USER'}) {
      $CONFIG{'ADMIN'} = 1;
      last;
    }
  }
  close(FILE);
}
$CONFIG{'SUBADMIN'} = 0;
$CONFIG{'SUBADMIN_USERS'} = {};
if ($ENV{'REMOTE_USER'} ne "" && $CONFIG{'ADMIN'} == 0) {
  open(FILE, "<./subadmins");
  while(<FILE>) {
    chomp;
    if ($_ !~ /^\s*#/) {
      my ($subadmin, $users) = (split(/\s*:\s*/))[0,1];
      if ($subadmin eq $ENV{'REMOTE_USER'}) {
        $CONFIG{'SUBADMIN'} = 1;
        $CONFIG{'SUBADMIN_USERS'}->{ $ENV{'REMOTE_USER'} } = 1;
        for (split(/\s*,\s*/,$users)) {
          my $user_clean = $_;
          $user_clean =~ s/^\s+//;
          $user_clean =~ s/\s+$//;
          $CONFIG{'SUBADMIN_USERS'}->{ $user_clean } = 1 if $user_clean ne "";
        }
        last;
      }
    }
  }
  close(FILE);
}

#
# Parse form
#

%FORM = &ReadParse;

#
# Configure languages
#

if ($FORM{'language'} ne "") {
  $LANGUAGE = $FORM{'language'};
} else {
  $LANGUAGE = $CONFIG{'LANGUAGE_USED'};
}
if (! defined $CONFIG{'LANG'}->{$LANGUAGE}->{'NAME'}) {
  $LANGUAGE = $CONFIG{'LANGUAGE_USED'};
}
my @dslanguages = ();
for (split(/\n/,$CONFIG{'LANGUAGES'})) {
  my $lang = $_;
  $lang =~ s/\s*selected>/>/;
  if ($lang =~ /value="([^"]+)"/) {
    $lang =~ s/>/ selected>/ if ($1 eq $LANGUAGE);
  }
  push(@dslanguages, $lang);
}
$CONFIG{'LANGUAGES'} = join("\n", @dslanguages);
$CONFIG{'TEMPLATES'} = $CONFIG{'LANG'}->{$LANGUAGE}->{'TEMPLATEDIR'};

#
# Configure Filesystem
#

$CURRENT_USER = $ENV{'REMOTE_USER'};

if ($FORM{'user'} ne "") {
  if ($CONFIG{'ADMIN'} == 1) {
    $CURRENT_USER = $FORM{'user'};
  } elsif ($CONFIG{'SUBADMIN'} == 1) {
    my $form_user_domain = (split(/@/, $FORM{'user'}))[1];
    if ($CONFIG{'SUBADMIN_USERS'}->{ $FORM{'user'} } == 1 || ($form_user_domain ne "" && $CONFIG{'SUBADMIN_USERS'}->{ "*@" . $form_user_domain } == 1)) {
      $CURRENT_USER = $FORM{'user'};
    } else {
      $FORM{'user'} = $CURRENT_USER;
    }
  } else {
   $FORM{'user'} = $CURRENT_USER;
  }
} else {
  $FORM{'user'} = $CURRENT_USER;
}

$CONFIG{'DSPAM_ARGS'} =~ s/%CURRENT_USER%/$CURRENT_USER/g;

# Current Store
do {
  my(%PREF) = GetPrefs($CURRENT_USER);
  $CURRENT_STORE = $PREF{"localStore"};
  if ($CURRENT_STORE eq "") { $CURRENT_STORE = $CURRENT_USER; }
};

$USER = GetPath($CURRENT_STORE);
$MAILBOX = $USER . ".mbox";
$TMPFILE = $USER . ".tmp";

if ($CURRENT_USER eq "") {
  &error("$CONFIG{'LANG'}->{$LANGUAGE}->{'error_no_identity'}");
}

if ($FORM{'template'} eq "" || $FORM{'template'} !~ /^([A-Z0-9]*)$/i) {
  $FORM{'template'} = "performance";
}

#
# Create a list of known users
#
if ($CONFIG{'ADMIN'} == 1 || $CONFIG{'SUBADMIN'} == 1) {
  my @dsusers = ();
  push(@dsusers, qq!<select name="user">!);
  open(IN, "$CONFIG{'DSPAM_STATS'}|");
  while(<IN>) {
    chomp;
    my($username) = (split(/\s+/))[0,2,4,6,8,10,12];
    if ($username ne "") {
      if ($username eq $CURRENT_USER) {
        # Add always current user to selection list
        push(@dsusers, qq!<option value="$username" selected>&nbsp;$username</option>!);
      } else {
        if ($CONFIG{'ADMIN'} == 1) {
          # Real administrators can see all users
          push(@dsusers, qq!<option value="$username">&nbsp;$username</option>!);
        } elsif ($CONFIG{'SUBADMIN'} == 1) {
          # Sub-administrators can only see their users. Either full
          # qualified email address or *@example.org
          my $form_user_domain = (split(/@/, $username))[1];
          if($CONFIG{'SUBADMIN_USERS'}->{ $username } == 1 || ($form_user_domain ne "" && $CONFIG{'SUBADMIN_USERS'}->{ "*@" . $form_user_domain } == 1)) {
            $CONFIG{'SUBADMIN_USERS'}->{ $username } = 1; # Add full email to hash list so that we
                                                          # can later check to ensure and secure
                                                          # access/switch to target user by sub-
                                                          # administrator.
            push(@dsusers, qq!<option value="$username">&nbsp;$username</option>!);
          }
        }
      }
    }
  }
  push(@dsusers, qq!</select>!);
  close(IN);
  $USERSELECT = join("\n",@dsusers);
  @dsusers=();
}

my($MYURL);
$MYURL = qq!$CONFIG{'ME'}?user=$FORM{'user'}&amp;template=$FORM{'template'}&amp;language=$LANGUAGE!;

#
# Set up initial display variables
#
&CheckQuarantine;
$DATA{'REMOTE_USER'} = $CURRENT_USER;

#
# Display DSPAM Version
#
$DATA{'DSPAMVERSION'} = $CONFIG{'DSPAM_VERSION'};

#
# Process Commands
#

# Performance
if ($FORM{'template'} eq "performance") {
  if ($FORM{'command'} eq "resetStats") {
    &ResetStats;
    redirect($MYURL);
  } elsif ($FORM{'command'} eq "tweak") {
    &Tweak;
    redirect($MYURL);
  } else {
    &DisplayIndex;
  }

# Quarantine
} elsif ($FORM{'template'} eq "quarantine") {
  if ($FORM{'command'} eq "viewMessage") {
    &Quarantine_ViewMessage;
  } else {
    $MYURL .= "&amp;sortby=$FORM{'sortby'}" if ($FORM{'sortby'} ne "");
    if ($FORM{'command'} eq "processQuarantine") {
      &ProcessQuarantine;
      redirect($MYURL);
    } elsif ($FORM{'command'} eq "processFalsePositive") {
      &ProcessFalsePositive;
      redirect($MYURL);
    } else {
      &DisplayQuarantine;
    }
  }

# Alerts
} elsif ($FORM{'template'} eq "alerts") {
  if ($FORM{'command'} eq "addAlert") {
    &AddAlert;
    redirect($MYURL);
  } elsif ($FORM{'command'} eq "deleteAlert") {
    &DeleteAlert;
    redirect($MYURL);
  } else {
    &DisplayAlerts;
  }

# Preferences
} elsif ($FORM{'template'} eq "preferences") {
  &DisplayPreferences;

# Analysis
} elsif ($FORM{'template'} eq "analysis") {
  &DisplayAnalysis;

# History
} elsif ($FORM{'template'} eq "history") {
  &DisplayHistory;
} elsif ($FORM{'template'} eq "fragment") {
  &DisplayFragment;
} else {
  &error("$CONFIG{'LANG'}->{$LANGUAGE}->{'error_invalid_command'} $FORM{'COMMAND'}");
}

#
# History Functions
# 

sub DisplayFragment {
  $FORM{'signatureID'} =~ s/\///g;
  $DATA{'FROM'} = $FORM{'from'};
  $DATA{'SUBJECT'} = $FORM{'subject'};
  $DATA{'INFO'} = $FORM{'info'};
  $DATA{'TIME'} = $FORM{'time'};
  open(FILE, "<$USER.frag/$FORM{'signatureID'}.frag") || &error($!);
  while(<FILE>) {
    s/</&lt\;/g;
    s/>/&gt\;/g;
    $DATA{'MESSAGE'} .= $_;
  }
  close(FILE);
  &output(%DATA);
}

sub DisplayHistory {
  my($all_lines , $begin, $history_pages, $rec, $history_page);
  unless ($history_page = $FORM{'history_page'}) { $history_page = 1;}

  my(@buffer, @history, $line, %rec);
  my($rowclass) = "rowEven";

  if ($CONFIG{'HISTORY_PER_PAGE'} == 0) {
    $CONFIG{'HISTORY_PER_PAGE'} = 50;
  }

  my($show) = $FORM{'show'};
  if ($show eq "") {
    $show = "all";
  }

  if ($FORM{'command'} eq "retrainChecked") {
    foreach my $i (0 .. $#{ $FORM{retrain_checked} }) {
      my ($retrain, $signature) = split(/:/, $FORM{retrain_checked}[$i]);
      if ($retrain eq "innocent") {
        $FORM{'signatureID'} = $signature;
        &ProcessFalsePositive();
        undef $FORM{'signatureID'};
      } elsif ($retrain eq "innocent" or $retrain eq "spam") {
        system("$CONFIG{'DSPAM'} --source=error --class=" . quotemeta($retrain) . " --signature=" . quotemeta($signature) . " --user " . quotemeta("$CURRENT_USER"));
      }
    }
    redirect("$MYURL&amp;show=$show&amp;history_page=$history_page");
  } elsif ($FORM{'retrain'} ne "") {
    if ($FORM{'retrain'} eq "innocent") {
      &ProcessFalsePositive();
    } else {
      system("$CONFIG{'DSPAM'} --source=error --class=" . quotemeta($FORM{'retrain'}) . " --signature=" . quotemeta($FORM{'signatureID'}) . " --user " . quotemeta("$CURRENT_USER"));
    }
    redirect("$MYURL&amp;show=$show&amp;history_page=$history_page");
  }

  my($LOG) = "$USER.log";
  if (! -e $LOG) {
    &error("$CONFIG{'LANG'}->{$LANGUAGE}->{'error_no_historic'}");
  }

  # Preseed retraining information and delivery errors
 
  open(LOG, "< $LOG") or die "Can't open log file $LOG";
  while(<LOG>) {
    my($time, $class, $from, $signature, $subject, $info, $messageid) 
      = split(/\t/, $_);
    next if ($signature eq "");

    # not good to check for messages to show here, we're skipping
    # the retraining data so retrained messages won't show

    if ($class eq "M" || $class eq "F" || $class eq "E" || $class eq "U") {
      if ($class eq "E" || $class eq "U") {
        $rec{$signature}->{'info'} = $info;
      } elsif ($class eq "F" || $class eq "M") {
        $rec{$signature}->{'class'} = $class;
        $rec{$signature}->{'count'}++;
        $rec{$signature}->{'info'} = $info 
          if ($rec{$signature}->{'info'} eq "");
      }
      # filter out resents if there are any. Since it's the same
      # message we only allow retraining on the 1st occurence of it.
    } elsif ($messageid == ''
	     || $rec{$signature}->{'messageid'} != $messageid
	     || $CONFIG{'HISTORY_DUPLICATES'} ne "no") {

      # skip unwanted messages
      next if ($class ne "S" && $show eq "spam");
      next if ($class ne "I" && $show eq "innocent");
      next if ($class ne "W" && $show eq "whitelisted");
      next if ($class ne "V" && $show eq "virus");
      next if ($class ne "A" && $show eq "blacklisted");
      next if ($class ne "O" && $show eq "blocklisted");

      $rec{$signature}->{'time'} = $time;
      $rec{$signature}->{'class'} = $class;
      $rec{$signature}->{'from'} = $from;
      $rec{$signature}->{'signature'} = $signature;
      $rec{$signature}->{'subject'} = $subject;
      $rec{$signature}->{'info'} = $info;
      $rec{$signature}->{'messageid'} = $messageid;

      unshift(@buffer, $rec{$signature});
    }
  }
  close(LOG);

  if($CONFIG{'HISTORY_SIZE'} < ($#buffer+1)) {
    $history_pages = int($CONFIG{'HISTORY_SIZE'} / $CONFIG{'HISTORY_PER_PAGE'});
    $history_pages += 1 if($CONFIG{'HISTORY_SIZE'} % $CONFIG{'HISTORY_PER_PAGE'});
  } else {
    $history_pages = int( ($#buffer+1) / $CONFIG{'HISTORY_PER_PAGE'});
    $history_pages += 1 if(($#buffer+1) % $CONFIG{'HISTORY_PER_PAGE'});
  }
  $begin = int(($history_page - 1) * $CONFIG{'HISTORY_PER_PAGE'}) ;

  # Now lets just keep the information that we really need. 
  @buffer = splice(@buffer, $begin,$CONFIG{'HISTORY_PER_PAGE'});

  my $retrain_checked_msg_no = 0;
  my $counter = 0;
  while ($rec = pop@buffer) {
    $counter++;
    my($time, $class, $from, $signature, $subject, $info, $messageid);

    $time = $rec->{'time'};
    $class = $rec->{'class'};
    $from = $rec->{'from'};
    $signature = $rec->{'signature'};
    $subject = $rec->{'subject'};
    $info = $rec->{'info'};
    $messageid = $rec->{'messageid'};

    next if ($signature eq "");
    next if ($rec{$signature}->{'displayed'} ne "");
    next if ($class eq "E");
    $rec{$signature}->{'displayed'} = 1;

    # Resends of retrained messages will need the original from/subject line
    if ($messageid ne "") {
      $from = $rec{$messageid}->{'from'} 
        if ($from eq "<None Specified>");
      $subject = $rec{$messageid}->{'subject'} 
        if ($subject eq "<None Specified>");

      $rec{$messageid}->{'from'} = $from 
        if ($rec{$messageid}->{'from'} eq "");
      $rec{$messageid}->{'subject'} = $subject 
        if ($rec{$messageid}->{'subject'} eq "");
    }

    $from = "<None Specified>" if ($from eq "");
    $subject = "<None Specified>" if ($subject eq "");

    my $ctime;
    if($CONFIG{"DATE_FORMAT"}) {
      $ctime = strftime($CONFIG{"DATE_FORMAT"}, localtime($time));
    } else {
      $ctime = ctime($time);
      my(@t) = split(/\:/, (split(/\s+/, $ctime))[3]);
      my($x) = (split(/\s+/, $ctime))[0];
      my($m) = "a";
      if ($t[0]>=12) { $t[0] -= 12; $m = "p"; }
      if ($t[0] == 0) { $t[0] = 12; }
      $ctime = "$x $t[0]:$t[1]$m";
    }

    # Set the appropriate type and label for this message

    my($cl, $cllabel);
    $class = $rec{$signature}->{'class'} if ($rec{$signature}->{'class'} ne "");
    if ($class eq "S") { $cl = "spam"; $cllabel="$CONFIG{'LANG'}->{$LANGUAGE}->{'history_label_spam'}"; }
    elsif ($class eq "I") { $cl = "innocent"; $cllabel="$CONFIG{'LANG'}->{$LANGUAGE}->{'history_label_innocent'}"; }
    elsif ($class eq "F") {
      if ($rec{$signature}->{'count'} % 2 != 0) {
        $cl = "false"; $cllabel="$CONFIG{'LANG'}->{$LANGUAGE}->{'history_label_miss'}";
      } else {
        $cl = "innocent"; $cllabel="$CONFIG{'LANG'}->{$LANGUAGE}->{'history_label_innocent'}";
      }
    }
    elsif ($class eq "M") { 
      if ($rec{$signature}->{'count'} % 2 != 0) {
          $cl = "missed"; $cllabel="$CONFIG{'LANG'}->{$LANGUAGE}->{'history_label_miss'}";
      } else {
          $cl = "spam"; $cllabel="$CONFIG{'LANG'}->{$LANGUAGE}->{'history_label_spam'}";
      }
    }
    elsif ($class eq "W") { $cl = "whitelisted"; $cllabel="$CONFIG{'LANG'}->{$LANGUAGE}->{'history_label_whitelist'}"; }
    elsif ($class eq "V") { $cl = "virus"; $cllabel="$CONFIG{'LANG'}->{$LANGUAGE}->{'history_label_virus'}"; }
    elsif ($class eq "A") { $cl = "blacklisted"; $cllabel="$CONFIG{'LANG'}->{$LANGUAGE}->{'history_label_rbl'}"; }
    elsif ($class eq "O") { $cl = "blocklisted"; $cllabel="$CONFIG{'LANG'}->{$LANGUAGE}->{'history_label_block'}"; }
    elsif ($class eq "N") { $cl = "inoculation"; $cllabel="$CONFIG{'LANG'}->{$LANGUAGE}->{'history_label_spam'}"; }
    elsif ($class eq "C") { $cl = "corpus"; $cllabel="$CONFIG{'LANG'}->{$LANGUAGE}->{'history_label_corpus'}"; }
    elsif ($class eq "U") { $cl = "unknown"; $cllabel="$CONFIG{'LANG'}->{$LANGUAGE}->{'history_label_unknown'}"; }
    elsif ($class eq "E") { $cl = "error"; $cllabel="$CONFIG{'LANG'}->{$LANGUAGE}->{'history_label_error'}"; }
    if ($messageid ne "" && $messageid ne "1") {
      if ($rec{$messageid}->{'resend'} ne "") {
        $cl = "relay";
        $cllabel = "$CONFIG{'LANG'}->{$LANGUAGE}->{'history_label_resend'}";
      }
      $rec{$messageid}->{'resend'} = $signature;
    }

    $info = $rec{$signature}->{'info'} if ($rec{$signature}->{'info'} ne "");

    $from = substr($from, 0, $CONFIG{'MAX_COL_LEN'} - 3) . "..." if (length($from)>$CONFIG{'MAX_COL_LEN'});
    $subject = substr($subject, 0, $CONFIG{'MAX_COL_LEN'} - 3) . "..." if (length($subject)>$CONFIG{'MAX_COL_LEN'});

    $from =~ s/&/&amp;/g;
    $from =~ s/</&lt;/g;
    $from =~ s/>/&gt;/g;
    $from =~ s/"/&quot;/g;
    $from =~ s/'/&#39;/g;	# MSIE doesn't know "&apos;"

    $subject =~ s/&/&amp;/g;
    $subject =~ s/</&lt;/g;
    $subject =~ s/>/&gt;/g;
    $subject =~ s/"/&quot;/g;
    $subject =~ s/'/&#39;/g;	# MSIE doesn't know "&apos;"

    my($rclass);
    $rclass = "spam" if ($class eq "I" || $class eq "W" || $class eq "F");
    $rclass = "innocent" if ($class eq "S" || $class eq "M" || $class eq "V" || $class eq "A" || $class eq "O");

    my($retrain);
    if ($rec{$signature}->{'class'} =~ /^(M|F)$/ && $rec{$signature}->{'count'} % 2 != 0) {
      $retrain = "<b>$CONFIG{'LANG'}->{$LANGUAGE}->{'history_retrained'}</b>";
    } 

    if ($retrain eq "") {
      $retrain = qq!<A HREF="$MYURL&amp;show=$show&amp;history_page=$history_page&amp;retrain=$rclass&amp;signatureID=$signature">$CONFIG{'LANG'}->{$LANGUAGE}->{'history_retrain_as'}&nbsp;! . ucfirst($CONFIG{'LANG'}->{$LANGUAGE}->{'history_retrain_as_'.$rclass}) . "</A>";
    } else {
      $retrain .= qq! (<A HREF="$MYURL&amp;show=$show&amp;history_page=$history_page&amp;retrain=$rclass&amp;signatureID=$signature">$CONFIG{'LANG'}->{$LANGUAGE}->{'history_retrain_undo'}</A>)!;
    }

    my($path) = "$USER.frag/$signature.frag";
    if (-e $path) {
      my(%pairs);
      $pairs{'template'} = "fragment";
      $pairs{'signatureID'} = $signature;
      my($sub) = $subject;
      $sub =~ s/#//g;
      $sub =~ s/(['])/\\$1/g;
      $pairs{'subject'} = $sub;
      $pairs{'from'} = $from;
      $pairs{'info'} = $info;
      $pairs{'time'} = $ctime;
      $pairs{'user'} = $FORM{'user'};
      my($url) = &SafeVars(%pairs);
      $from = qq!<a href="javascript:openwin(580,400,1,'$CONFIG{'ME'}?$url')">$from</a>!;
    }

    my $retrain_action = "";
    if ( $class eq "V" || $class eq "A" || $class eq "O" || $class eq "U" || $class eq "") {
      $retrain_action = qq!<small>&nbsp;</small>!;
    } else {
      $retrain_action = qq!<input name="msgid$retrain_checked_msg_no" type="checkbox" value="$rclass:$signature" id="checkbox-$counter" onclick="checkboxclicked(this)"><small>$retrain</small>!;
    }

    # HTMLize special characters
    if ($CONFIG{'HISTORY_HTMLIZE'} eq "yes") {
      $from=htmlize($from);
      $subject=htmlize($subject);
    }

    my($entry) = <<_END;
<tr>
 <td class="$cl $rowclass" nowrap="nowrap"><small>$cllabel</small></td>
 <td class="$rowclass" nowrap="nowrap">$retrain_action</td>
 <td class="$rowclass" nowrap="nowrap"><small>$ctime</small></td>
 <td class="$rowclass" nowrap="nowrap"><small>$from</small></td>
 <td class="$rowclass" nowrap="nowrap"><small>$subject</small></td>
 <td class="$rowclass" nowrap="nowrap"><small>$info</small></td>
</tr>
_END

    $retrain_checked_msg_no++;
    push(@history, $entry);

    if ($rowclass eq "rowEven") {
      $rowclass = "rowOdd";
    } else {
      $rowclass = "rowEven";
    }

  }

  while($line = pop(@history)) { $DATA{'HISTORY'} .= $line; }

  $DATA{'HISTORYPAGES'} = qq!<input name="history_page" type="hidden" value="$history_page">!;

  if ($CONFIG{'HISTORY_PER_PAGE'} > 0) {
    $DATA{'HISTORYPAGES'} .= "<div class=\"historypages\">";
    $DATA{'HISTORYPAGES'} .= "[" if ($history_pages > 0);
    if (($history_pages > 1) && ($history_page > 1)) {
      my $i = $history_page-1;
      $DATA{'HISTORYPAGES'} .= "<a href=\"$MYURL&amp;show=$show&amp;history_page=$i\">&nbsp;&lt;&nbsp;</a>";
    }
    for(my $i = 1; $i <= $history_pages; $i++) {
      $DATA{'HISTORYPAGES'} .= "<a href=\"$MYURL&amp;show=$show&amp;history_page=$i\">";
      $DATA{'HISTORYPAGES'} .= "<big><strong>" if ($i == $history_page);
      $DATA{'HISTORYPAGES'} .= "&nbsp;$i&nbsp;";
      $DATA{'HISTORYPAGES'} .= "</strong></big>" if ($i == $history_page);
      $DATA{'HISTORYPAGES'} .= "</a>";
    }
    if (($history_pages > 1) && ($history_page < $history_pages)) {
      my $i = $history_page+1;
      $DATA{'HISTORYPAGES'} .= "<a href=\"$MYURL&amp;show=$show&amp;history_page=$i\">&nbsp;&gt;&nbsp;</a>";
    }
    $DATA{'HISTORYPAGES'} .= "]" if ($history_pages > 0);
    $DATA{'HISTORYPAGES'} .= "</div>";
  }

  $DATA{'SHOW'} = $show;
  $DATA{'SHOW_SELECTOR'} .=  "$CONFIG{'LANG'}->{$LANGUAGE}->{'history_show'}: <a href=\"$MYURL&amp;show=all\">";
  if ($show eq "all") {
    $DATA{'SHOW_SELECTOR'} .= "<strong>$CONFIG{'LANG'}->{$LANGUAGE}->{'history_show_all'}</strong>";
  } else {
    $DATA{'SHOW_SELECTOR'} .= "$CONFIG{'LANG'}->{$LANGUAGE}->{'history_show_all'}";
  }
  $DATA{'SHOW_SELECTOR'} .=  "</a> | <a href=\"$MYURL&amp;show=spam\">";
  if ($show eq "spam") {
    $DATA{'SHOW_SELECTOR'} .= "<strong>$CONFIG{'LANG'}->{$LANGUAGE}->{'history_show_spam'}</strong>";
  } else {
    $DATA{'SHOW_SELECTOR'} .= "$CONFIG{'LANG'}->{$LANGUAGE}->{'history_show_spam'}";
  }
  $DATA{'SHOW_SELECTOR'} .=  "</a> | <a href=\"$MYURL&amp;show=innocent\">";
  if ($show eq "innocent") {
    $DATA{'SHOW_SELECTOR'} .= "<strong>$CONFIG{'LANG'}->{$LANGUAGE}->{'history_show_innocent'}</strong>";
  } else {
    $DATA{'SHOW_SELECTOR'} .= "$CONFIG{'LANG'}->{$LANGUAGE}->{'history_show_innocent'}";
  }
  $DATA{'SHOW_SELECTOR'} .=  "</a> | <a href=\"$MYURL&amp;show=whitelisted\">";
  if ($show eq "whitelisted") {
    $DATA{'SHOW_SELECTOR'} .= "<strong>$CONFIG{'LANG'}->{$LANGUAGE}->{'history_show_whitelisted'}</strong>";
  } else {
    $DATA{'SHOW_SELECTOR'} .= "$CONFIG{'LANG'}->{$LANGUAGE}->{'history_show_whitelisted'}";
  }
  $DATA{'SHOW_SELECTOR'} .=  "</a>";

  &output(%DATA);
}

#
# Analysis Functions
#

sub DisplayAnalysis {
  my($LOG) = "$USER.log";

  my %Stats=(
    daily	=> {},
    weekly	=> {},
  );

  my ($min, $hour, $mday, $mon, $year) = (localtime(time))[1,2,3,4,5];
  my ($daystart) = timelocal(0, 0, 0, $mday, $mon, $year);
  my ($periodstart) = $daystart - (3600*24*13); # 2 Weeks ago
  my ($dailystart) = time - (3600*23);

  if (! -e $LOG) {
    &error("$CONFIG{'LANG'}->{$LANGUAGE}->{'error_no_historic'}");
  }

  open(LOG, "<$LOG") || &error("$CONFIG{'LANG'}->{$LANGUAGE}->{'error_cannot_open_log'}: $!");
  while(<LOG>) {
    my($t_log, $c_log) = split(/\t/);

    # Only Parse Log Data in our Time Period
    if ($t_log>=$periodstart) {
      my($tmin, $thour, $tday, $tmon) = (localtime($t_log))[1,2,3,4];
      $tmon++;

      foreach my $period (qw( weekly daily )) {
        my $idx;
        if ($period eq "weekly") {
          $idx="$tmon/$tday";
        } else { 
          ($t_log>=$dailystart) || next;
          $idx=To12Hour($thour);
        }
        if (!exists $Stats{$period}->{$idx}) {
          $Stats{$period}->{$idx}={
		nonspam	=>	0,
		spam	=>	0,
		title	=>	$idx,
		idx	=>	$t_log,
          };
        }
        my $hr=$Stats{$period}->{$idx};
        if ($c_log eq "S" || $c_log eq "V" || $c_log eq "A" || $c_log eq "O") {
          $hr->{spam}++;
        }
        if ($c_log eq "I" || $c_log eq "W") {
          $hr->{nonspam}++;
        }
        if ($c_log eq "F") {
          $hr->{spam}--;
          ($hr->{spam}<0) && ($hr->{spam}=0);
          $hr->{nonspam}++;
        }
        if ($c_log eq "M") {
          $hr->{nonspam}--;
          ($hr->{nonspam}<0) && ($hr->{nonspam}=0);
          $hr->{spam}++;
        }
      }
    }
  }
  close(LOG);

  foreach my $period (qw( daily weekly )) {
    my $uc_period=uc($period);
    my $hk="DATA_$uc_period";
    my %lst=(spam => [], nonspam => [], title => []);
    foreach my $hr (sort {$a->{idx}<=>$b->{idx}} (values %{$Stats{$period}})) {
      foreach my $type (qw( spam nonspam title )) {
        push(@{$lst{$type}},$hr->{$type});
        my $totk="";
        if ($type eq "spam") { $totk="S"; }
        elsif ($type eq "nonspam") { $totk="I"; }
        ($totk eq "") && next;
        my $sk="T${totk}_$uc_period";
        (exists $DATA{$sk}) || ($DATA{$sk}=0);
        $DATA{$sk}+=$hr->{$type};
      }
    }
    foreach my $type (qw( spam nonspam title )) {
      (exists $lst{$type}) || ($lst{$type}=[]);
      @{$lst{$type}} = (0) if (scalar(@{$lst{$type}}) == 0);
    }
    $DATA{$hk}=join("_",
		join(",",@{$lst{spam}}),
		join(",",@{$lst{nonspam}}),
		join(",",@{$lst{title}}),
	);
  }

  &output(%DATA);
}

#
# Preferences Functions
#

sub DisplayPreferences {
  my(%PREFS);
  my($FILE) = "$USER.prefs";

  my $username = $CURRENT_USER;

  if ($FORM{'submit'} ne "") {

    if ($FORM{'enableBNR'} ne "on") {
      $FORM{'enableBNR'} = "off";
    }

    if ($FORM{'optIn'} ne "on") {
      $FORM{'optIn'} = "off";
    }

    if ($FORM{'optOut'} ne "on") {
      $FORM{'optOut'} = "off";
    }

    if ($FORM{'showFactors'} ne "on") {
      $FORM{'showFactors'} = "off";
    }
                                                                                
    if ($FORM{'enableWhitelist'} ne "on") {
      $FORM{'enableWhitelist'} = "off";
    }
    if ($FORM{'dailyQuarantineSummary'} ne "on") {
      $FORM{'dailyQuarantineSummary'} = "off";
    }

    if ($CONFIG{'PREFERENCES_EXTENSION'} == 1) {

      if ($FORM{'spamSubject'} eq "") {
        $FORM{'spamSubject'} = "''";
      } else {
        $FORM{'spamSubject'} = quotemeta($FORM{'spamSubject'});
      }

      system("$CONFIG{'DSPAM_BIN'}/dspam_admin ch pref ".quotemeta($CURRENT_USER).
        " trainingMode " . quotemeta($FORM{'trainingMode'}) . " > /dev/null");
      system("$CONFIG{'DSPAM_BIN'}/dspam_admin ch pref ".quotemeta($CURRENT_USER).
        " spamAction " . quotemeta($FORM{'spamAction'}) . " > /dev/null");
      system("$CONFIG{'DSPAM_BIN'}/dspam_admin ch pref ".quotemeta($CURRENT_USER).
        " signatureLocation "
        . quotemeta($FORM{'signatureLocation'}) . " > /dev/null");
      system("$CONFIG{'DSPAM_BIN'}/dspam_admin ch pref ".quotemeta($CURRENT_USER).
        " spamSubject " . $FORM{'spamSubject'} . " > /dev/null");
      system("$CONFIG{'DSPAM_BIN'}/dspam_admin ch pref ".quotemeta($CURRENT_USER).
        " statisticalSedation "
        . quotemeta($FORM{'statisticalSedation'}) . " > /dev/null");
      system("$CONFIG{'DSPAM_BIN'}/dspam_admin ch pref ".quotemeta($CURRENT_USER).
        " enableBNR "
        . quotemeta($FORM{'enableBNR'}) . "> /dev/null");
      system("$CONFIG{'DSPAM_BIN'}/dspam_admin ch pref ".quotemeta($CURRENT_USER).
        " optOut "
        . quotemeta($FORM{'optOut'}) . ">/dev/null");
      system("$CONFIG{'DSPAM_BIN'}/dspam_admin ch pref ".quotemeta($CURRENT_USER).
        " optIn "
        . quotemeta($FORM{'optIn'}) . ">/dev/null");
      system("$CONFIG{'DSPAM_BIN'}/dspam_admin ch pref ".quotemeta($CURRENT_USER).
        " showFactors "
        . quotemeta($FORM{'showFactors'}) . "> /dev/null");
      system("$CONFIG{'DSPAM_BIN'}/dspam_admin ch pref ".quotemeta($CURRENT_USER).
        " enableWhitelist "
        . quotemeta($FORM{'enableWhitelist'}) . "> /dev/null");
      system("$CONFIG{'DSPAM_BIN'}/dspam_admin ch pref ".quotemeta($CURRENT_USER).
        " dailyQuarantineSummary "
        . quotemeta($FORM{'dailyQuarantineSummary'}) . "> /dev/null");


    } else {
      open(FILE, ">$FILE") || do { &error("$CONFIG{'LANG'}->{$LANGUAGE}->{'error_cannot_write_prefs'}: $!"); };
      print FILE <<_END;
trainingMode=$FORM{'trainingMode'}
spamAction=$FORM{'spamAction'}
spamSubject=$FORM{'spamSubject'}
statisticalSedation=$FORM{'statisticalSedation'}
enableBNR=$FORM{'enableBNR'}
optIn=$FORM{'optIn'}
optOut=$FORM{'optOut'}
showFactors=$FORM{'showFactors'}
enableWhitelist=$FORM{'enableWhitelist'}
signatureLocation=$FORM{'signatureLocation'}
dailyQuarantineSummary=$FORM{'dailyQuarantineSummary'}
_END
      close(FILE);
    }
  redirect("$CONFIG{'ME'}?user=$FORM{'user'}&amp;template=preferences&amp;language=$LANGUAGE");
  }

  %PREFS = GetPrefs();

  $DATA{"SEDATION_$PREFS{'statisticalSedation'}"} = "CHECKED";
  $DATA{"S_".$PREFS{'trainingMode'}} = "CHECKED";
  $DATA{"S_ACTION_".uc($PREFS{'spamAction'})} = "CHECKED"; 
  $DATA{"S_LOC_".uc($PREFS{'signatureLocation'})} = "CHECKED";
  $DATA{"SPAM_SUBJECT"} = $PREFS{'spamSubject'};
  if ($PREFS{'optIn'} eq "on") {
    $DATA{'C_OPTIN'} = "CHECKED";
  }
  if ($PREFS{'optOut'} eq "on") {
    $DATA{'C_OPTOUT'} = "CHECKED";
  }
  if ($PREFS{"enableBNR"} eq "on") {
    $DATA{"C_BNR"} = "CHECKED";
  }
  if ($PREFS{"showFactors"} eq "on") {
    $DATA{"C_FACTORS"} = "CHECKED"; 
  }
  if ($PREFS{"enableWhitelist"} eq "on") {
    $DATA{"C_WHITELIST"} = "CHECKED";
  }
  if ($PREFS{"dailyQuarantineSummary"} eq "on") {
    $DATA{"C_SUMMARY"} = "CHECKED";
  }

  if ($CONFIG{'OPTMODE'} eq "OUT") {
    $DATA{"OPTION"} = "<INPUT TYPE=CHECKBOX NAME=optOut " . $DATA{'C_OPTOUT'} . ">$CONFIG{'LANG'}->{$LANGUAGE}->{'option_disable_filtering'}<br>";
  } elsif ($CONFIG{'OPTMODE'} eq "IN") {
    $DATA{"OPTION"} = "<INPUT TYPE=CHECKBOX NAME=optIn " . $DATA{'C_OPTIN'} . ">$CONFIG{'LANG'}->{$LANGUAGE}->{'option_enable_filtering'}<br>";
  } else {
    $DATA{"OPTION"} = "";
  }

  &output(%DATA);
}

#
# Quarantine Functions
#

sub ProcessQuarantine {
  if ($FORM{'manyNotSpam'} ne "") {
    &Quarantine_ManyNotSpam;
  } else {
    &Quarantine_DeleteSpam;
  }
  &CheckQuarantine;
  return;
}

sub ProcessFalsePositive {
  my(@buffer, %head, $found);
  if ($FORM{'signatureID'} eq "") {
    &error("$CONFIG{'LANG'}->{$LANGUAGE}->{'error_no_sigid'}");
  }
  open(FILE, "<$MAILBOX");
  while(<FILE>) {
    s/\r?\n$//;
    push(@buffer, $_);
  }
  close(FILE);

  while($#buffer>=0) {
    my($buff, $mode, @temp);
    $mode = 0;
    @temp = ();
    while(($buff !~ /^From /) && ($#buffer>=0)) {
      $buff = $buffer[0];
      if ($buff =~ /^From /) {
        if ($mode == 0) { $mode = 1; }
        else { next; }
      }
      $buff = shift(@buffer);
      if ($buff !~ /^From /) {
        push(@temp, $buff);
      }
      next;
    }
    foreach(@temp) {
      last if ($_ eq "");
      my($key, $val) = split(/\: ?/, $_, 2);
      $head{$key} = $val;
    }
    if ($head{'X-DSPAM-Signature'} eq $FORM{'signatureID'}) {
      $found = 1;
      open(PIPE, "|$CONFIG{'DSPAM'} $CONFIG{'DSPAM_ARGS'}  >$TMPFILE 2>&1") || &error($!);
      foreach(@temp) {
        print PIPE "$_\n";
      }
      close(PIPE);
    }
  }

  # Couldn't find the message, so just retrain on signature
  if (!$found) {
    system("$CONFIG{'DSPAM'} --source=error --class=innocent --signature=" . quotemeta($FORM{'signatureID'}) . " --user " . quotemeta("$CURRENT_USER"));
  }

  if ($?) {
    my(@log);
    open(LOG, "<$TMPFILE");
    @log = <LOG>;
    close(LOG);
    unlink("$TMPFILE");
    &error("<PRE>".join('', @log)."</PRE>");
  }

  unlink("$TMPFILE");
  $FORM{$FORM{'signatureID'}} = "on";
  &Quarantine_DeleteSpam();
  return;
}

sub Quarantine_ManyNotSpam {
  my(@buffer, @errors);

  open(FILE, "<$MAILBOX");
  while(<FILE>) {
    s/\r?\n$//;
    push(@buffer, $_);
  }
  close(FILE);

  open(FILE, ">$MAILBOX") || &error($!);
  open(RETRAIN, ">>$USER.retrain.log");

  while($#buffer>=0) {
    my($buff, $mode, @temp, %head, $delivered);
    $mode = 0;
    while(($buff !~ /^From /) && ($#buffer>=0)) {
      $buff = $buffer[0];
      if ($buff =~ /^From /) {
        if ($mode == 0) {
          $mode = 1;
          $buff = shift(@buffer);
          push(@temp, $buff);
          $buff = "";
          next;
        } else {
          next;
        }
      }
      $buff = shift(@buffer);
      push(@temp, $buff);
      next;
    }
    foreach(@temp) {
      last if ($_ eq "");
      my($key, $val) = split(/\: ?/, $_, 2);
      $head{$key} = $val;
    }
    $delivered = 0;
    if ($FORM{$head{'X-DSPAM-Signature'}} ne "") {
      my($err);
      $err = &Deliver(@temp);
      if ($err eq "") {
        $delivered = 1;
      } else {
        push(@errors, $err);
      }
    }
    if (!$delivered) {
      foreach(@temp) {
        print FILE "$_\n";
      }
    } else {
      print RETRAIN time . "\t$head{'X-DSPAM-Signature'}\tinnocent\n";
    }
  }
  close(RETRAIN);
  close(FILE);
  if (@errors > 0) {
    &error(join("<BR>", @errors));
  }
  return;
}

sub Deliver {
  my(@temp) = @_;
  open(PIPE, "|$CONFIG{'DSPAM'} $CONFIG{'DSPAM_ARGS'}") || return $!;
  foreach(@temp) {
    print PIPE "$_\n" || return $!;
  }
  close(PIPE) || return $!;
  return "";
}

sub Quarantine_ViewMessage {
  my(@buffer);

  if ($FORM{'signatureID'} eq "") {
    &error("$CONFIG{'LANG'}->{$LANGUAGE}->{'error_no_sigid'}");
  }

  $DATA{'MESSAGE_ID'} = $FORM{'signatureID'};

  open(FILE, "<$MAILBOX");
  while(<FILE>) {
    s/\r?\n//;
    push(@buffer, $_);
  }
  close(FILE);

  while($#buffer>=0) {
    my($buff, $mode, @temp, %head);
    $mode = 0;
    @temp = ();
    while(($buff !~ /^From /) && ($#buffer>=0)) {
      $buff = $buffer[0];
      if ($buff =~ /^From /) {
        if ($mode == 0) { $mode = 1; }
        else { next; }
      }
      $buff = shift(@buffer);
      if ($buff !~ /^From /) {
        push(@temp, $buff);
      }
      next;
    }
    foreach(@temp) {
      last if ($_ eq "");
      my($key, $val) = split(/\: ?/, $_, 2);
      $head{$key} = $val;
    }
    if ($head{'X-DSPAM-Signature'} eq $FORM{'signatureID'}) {
      foreach(@temp) {
        s/</\&lt\;/g;
        s/>/\&gt\;/g;
        $DATA{'MESSAGE'} .= "$_\n";
      }
    }
  }
  $FORM{'template'} = "viewmessage";
  &output(%DATA);
}

sub Quarantine_DeleteSpam {
  my(@buffer);
  if ($FORM{'deleteAll'} ne "") {
    my($sz);

    my($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
     $atime,$mtime,$ctime,$blksize,$blocks)
     = stat("$USER.mbox");
                                                                                
    open(FILE, "<$USER.mbox.size");
    $sz = <FILE>;
    close(FILE); 
    chomp($sz);

    if ($sz == $size) {
      open(FILE, ">$MAILBOX");
      close(FILE);
      unlink("$USER.mbox.size");
      unlink("$USER.mboxwarn");
    } else {
      return;
    }

    $FORM{'template'} = "performance";
    &CheckQuarantine;
    redirect("$CONFIG{'ME'}?user=$FORM{'user'}&amp;template=$FORM{'template'}&amp;language=$LANGUAGE");
    return; 
  }
  open(FILE, "<$MAILBOX");
  while(<FILE>) {
    s/\r?\n//;
    push(@buffer, $_);
  }
  close(FILE);
 

  open(FILE, ">$MAILBOX");
 
  while($#buffer>=0) {
    my($buff, $mode, @temp, %head);
    $mode = 0;
    while(($buff !~ /^From /) && ($#buffer>=0)) {
      $buff = $buffer[0];
      if ($buff =~ /^From /) {
        if ($mode == 0) { 
          $mode = 1; 
          $buff = shift(@buffer); 
          push(@temp, $buff); 
          $buff = ""; 
          next; 
        } else { 
          next; 
        }
      }
      $buff = shift(@buffer);
      push(@temp, $buff);
      next;
    }
    foreach(@temp) {
      last if ($_ eq "");
      my($key, $val) = split(/\: ?/, $_, 2);
      $head{$key} = $val;
    }
    if ($FORM{$head{'X-DSPAM-Signature'}} eq "") {
      foreach(@temp) {
        print FILE "$_\n";
      }
    }
  }
  close(FILE);
  return;
}

sub by_rating { $a->{'rating'} <=> $b->{'rating'} }
sub by_subject { lc($a->{'Subject'}) cmp lc($b->{'Subject'}) }
sub by_from { lc($a->{'From'}) cmp lc($b->{'From'}) }

sub DisplayQuarantine {
  my(@alerts);

  my($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
   $atime,$mtime,$ctime,$blksize,$blocks)
   = stat("$USER.mbox");

  open(FILE, ">$USER.mbox.size");
  print(FILE "$size");
  close(FILE);

  open(FILE, ">$MAILBOX.stamp");
  close(FILE);
  chmod 0660, "$MAILBOX.stamp";

  open(FILE, "<$USER.alerts");
  while(<FILE>) {
    chomp;
    push(@alerts, $_);
  }
  close(FILE);

  my($next, @buffer, $rowclass, $mode, $markclass, $marklabel, @headings);
  $rowclass="rowEven";
  open(FILE, "<$MAILBOX");
  while(<FILE>) {
    s/\r?\n//;
    if ($_ ne "") {
      if ($mode eq "") { 
        if ($_ =~ /^From /) {
          $mode = 1;
        } else { 
          next; 
        } 
      }
      push(@buffer, $_);
      next;
    }
    next if ($mode eq "");

    my($new, $start, $alert);
    $alert = 0;
    $new = {};
    foreach(@buffer) {
      my($al);
      foreach $al (@alerts) {
        if (/$al/i) {
          $alert = 1;
        }
      }
      if ($_ =~ /^From /) {
        my(@a) = split(/ /, $_);
        my($x) = 2;
        for (0..$#a) { 
          if (($a[$_] =~ /\@|>/) && ($_>$x)) { 
            $x = $_ + 1;
          }
        }
        for(1..$x) { shift(@a); }
        $start = join(" ", @a);
      } else {
        my($key, $val) = split(/\: ?/, $_, 2);
        $new->{$key} = $val; 
      }
    }
    if ($rowclass eq "rowEven") { 
      $rowclass = "rowOdd";
    } else { 
      $rowclass = "rowEven";
    }

    $new->{'alert'} = $alert;

    if ($alert) { $rowclass="rowAlert"; }

    # HTMLize special characters
    if ($CONFIG{'QUARANTINE_HTMLIZE'} eq "yes") {
      $new->{'Subject'} = htmlize($new->{'Subject'});
      $new->{'From'} = htmlize($new->{'From'});
    }

    $new->{'Sub2'} = $new->{'X-DSPAM-Signature'};
    if (length($new->{'Subject'})>$CONFIG{'MAX_COL_LEN'}) {
      $new->{'Subject'} = substr($new->{'Subject'}, 0, $CONFIG{'MAX_COL_LEN'} - 3) . "...";
    } 
 
    if (length($new->{'From'})>$CONFIG{'MAX_COL_LEN'}) {
      $new->{'From'} = substr($new->{'From'}, 0, $CONFIG{'MAX_COL_LEN'} - 3) . "...";
    }

    if ($new->{'Subject'} eq "") {
      $new->{'Subject'} = "<None Specified>";
    }

#   $new->{'rating'} = $new->{'X-DSPAM-Probability'} * $new->{'X-DSPAM-Confidence'};
    $new->{'rating'} = $new->{'X-DSPAM-Confidence'};

    foreach(keys(%$new)) {
      next if ($_ eq "X-DSPAM-Signature");
      $new->{$_} =~ s/</\&lt\;/g;
      $new->{$_} =~ s/>/\&gt\;/g;
    }

    push(@headings, $new);

    @buffer = ();
    $mode = "";
    next;
  }

  my($sortBy) = $FORM{'sortby'};
  if ($sortBy eq "") {
    $sortBy = $CONFIG{'SORT_DEFAULT'};
  }
  if ($sortBy eq "Rating") {
    @headings = sort by_rating @headings;
  }
  if ($sortBy eq "Subject") {
    @headings = sort by_subject @headings;
  }
  if ($sortBy eq "From") {
    @headings = sort by_from @headings;
  }
  if ($sortBy eq "Date") {
    @headings = reverse @headings;
  }

  $DATA{'SORTBY'} = $sortBy;
  $DATA{'SORT_QUARANTINE'} .=  "<th><a href=\"$CONFIG{'ME'}?user=$FORM{'user'}&amp;template=quarantine&amp;language=$LANGUAGE&amp;sortby=Rating&amp;user=$FORM{'user'}\">";
  if ($sortBy eq "Rating") {
    $DATA{'SORT_QUARANTINE'} .= "<strong>$CONFIG{'LANG'}->{$LANGUAGE}->{'quarantine_rating'}</strong>";
  } else {
    $DATA{'SORT_QUARANTINE'} .= "$CONFIG{'LANG'}->{$LANGUAGE}->{'quarantine_rating'}";
  }
  $DATA{'SORT_QUARANTINE'} .=  "</a></th>\n\t<th><a href=\"$CONFIG{'ME'}?user=$FORM{'user'}&amp;template=quarantine&amp;language=$LANGUAGE&amp;sortby=Date&amp;user=$FORM{'user'}\">";
  if ($sortBy eq "Date") {
    $DATA{'SORT_QUARANTINE'} .= "<strong>$CONFIG{'LANG'}->{$LANGUAGE}->{'quarantine_date'}</strong>";
  } else {
    $DATA{'SORT_QUARANTINE'} .= "$CONFIG{'LANG'}->{$LANGUAGE}->{'quarantine_date'}";
  }
  $DATA{'SORT_QUARANTINE'} .=  "</a></th>\n\t<th><a href=\"$CONFIG{'ME'}?user=$FORM{'user'}&amp;template=quarantine&amp;language=$LANGUAGE&amp;sortby=From&amp;user=$FORM{'user'}\">";
  if ($sortBy eq "From") {
    $DATA{'SORT_QUARANTINE'} .= "<strong>$CONFIG{'LANG'}->{$LANGUAGE}->{'quarantine_from'}</strong>";
  } else {
    $DATA{'SORT_QUARANTINE'} .= "$CONFIG{'LANG'}->{$LANGUAGE}->{'quarantine_from'}";
  }
  $DATA{'SORT_QUARANTINE'} .=  "</a></th>\n\t<th><a href=\"$CONFIG{'ME'}?user=$FORM{'user'}&amp;template=quarantine&amp;language=$LANGUAGE&amp;sortby=Subject&amp;user=$FORM{'user'}\">";
  if ($sortBy eq "Subject") {
    $DATA{'SORT_QUARANTINE'} .= "<strong>$CONFIG{'LANG'}->{$LANGUAGE}->{'quarantine_subject'}</strong>";
  } else {
    $DATA{'SORT_QUARANTINE'} .= "$CONFIG{'LANG'}->{$LANGUAGE}->{'quarantine_subject'}";
  }
  $DATA{'SORT_QUARANTINE'} .=  "</a></th>";


  my($row, $rowclass, $counter);
  $rowclass = "rowEven";
  $counter = 0;
  for $row (@headings) {
    $counter++;
    my($rating, $url, $markclass, $outclass);
    $rating = sprintf("%3.0f%%", $row->{'rating'} * 100.0);
    if ($row->{'rating'} > 0.8) {
	$markclass = "high";
    } else {
	if ($row->{'rating'} < 0.7) {
	    $markclass = "low";
	} else {
	    $markclass = "medium";
	}
    }

    my(%PAIRS);
    $PAIRS{'signatureID'} = $row->{'X-DSPAM-Signature'};
    $PAIRS{'command'}  = "viewMessage";
    $PAIRS{'user'} = $CURRENT_USER;
    $PAIRS{'template'} = "quarantine";
    $url = &SafeVars(%PAIRS);

    if ($row->{'alert'}) {
      $outclass = "rowAlert";
    } else {
      $outclass = $rowclass;
    }

    my(@ptfields) = split(/\s+/, $row->{'X-DSPAM-Processed'});
    my(@times) = split(/\:/, $ptfields[3]);
    my($ptime);
    if($CONFIG{"DATE_FORMAT"}) {
      my($month);
      $month->{'Jan'}=0;
      $month->{'Feb'}=1;
      $month->{'Mar'}=2;
      $month->{'Apr'}=3;
      $month->{'May'}=4;
      $month->{'Jun'}=5;
      $month->{'Jul'}=6;
      $month->{'Aug'}=7;
      $month->{'Sep'}=8;
      $month->{'Oct'}=9;
      $month->{'Nov'}=10;
      $month->{'Dec'}=11;
      $ptime = strftime($CONFIG{"DATE_FORMAT"}, $times[2],$times[1],$times[0],$ptfields[2],$month->{$ptfields[1]},$ptfields[4]-1900);
    } else {
      my($mer) = "a";
      if ($times[0] > 12) { $times[0] -= 12; $mer = "p"; }
      if ($times[0] == 0) { $times[0] = "12"; }
      $ptime = "$ptfields[1] $ptfields[2] $times[0]:$times[1]$mer";
      }

    $DATA{'QUARANTINE'} .= <<_END;
<tr>
 <td class="$outclass" nowrap="nowrap"><input type="checkbox" name="$row->{'X-DSPAM-Signature'}" id="checkbox-$counter" onclick="checkboxclicked(this)"></td>
 <td class="$outclass $markclass" nowrap="nowrap">$rating</td>
 <td class="$outclass" nowrap="nowrap">$ptime</td>
 <td class="$outclass" nowrap="nowrap">$row->{'From'}</td>
 <td class="$outclass" nowrap="nowrap"><a href="$CONFIG{'ME'}?$url">$row->{'Subject'}</a></td>
</tr>
_END

    if ($rowclass eq "rowEven") {
      $rowclass = "rowOdd";
    } else {
      $rowclass = "rowEven";
    }
  }

  &output(%DATA);
}

#
# Performance Functions
#

sub ResetStats {
  my($ts, $ti, $tm, $fp, $sc, $ic);
  my($group);
  open(FILE, "<$USER.stats");
  chomp($ts = <FILE>);
  chomp($group = <FILE>); 
  close(FILE);
  ($ts, $ti, $tm, $fp, $sc, $ic) = split(/\,/, $ts);
  
  if ($group ne "") {
    my($GROUP) = GetPath($group) . ".stats";
    my($gts, $gti, $gtm, $gfp, $gsc, $gic);
    open(FILE, "<$GROUP");
    chomp($gts = <FILE>);
    close(FILE);
    ($gts, $gti, $gtm, $gfp, $gsc, $gic) = split(/\,/, $gts);
    $ts       -= $gts;
    $ti       -= $gti;
    $tm       -= $gtm;
    $fp       -= $gfp;
    $sc       -= $gsc;
    $ic       -= $gic;
  }

  open(FILE, ">$USER.rstats");
  print FILE "$ts" . "," . "$ti" . "," . "$tm" . "," .
             "$fp" . "," . "$sc" . "," . "$ic\n";
  close(FILE);
}

sub Tweak {
  my($ts, $ti, $tm, $fp, $sc, $ic);
  open(FILE, "<$USER.rstats");
  chomp($ts = <FILE>);
  close(FILE);
  ($ts, $ti, $tm, $fp, $sc, $ic) = split(/\,/, $ts);
  $tm++;
  open(FILE, ">$USER.rstats");
  print FILE "$ts,$ti,$tm,$fp,$sc,$ic\n";
  close(FILE);
}

sub DisplayIndex {
  my($spam, $innocent, $ratio, $fp, $misses);
  my($rts, $rti, $rtm, $rfp, $sc, $ic, $overall, $fpratio, $monthly, 
     $real_missed, $real_caught, $real_fp, $real_innocent);
  my($time) = ctime(time);
  my($group);

  open(FILE, "<$USER.stats");
  chomp($spam = <FILE>);
  chomp($group = <FILE>); 
  close(FILE);
  ($spam, $innocent, $misses, $fp, $sc, $ic) = split(/\,/, $spam);

  if ($group ne "") {
    my($GROUP) = GetPath($group) . ".stats";
    my($gspam, $ginnocent, $gmisses, $gfp, $gsc, $gic);
    open(FILE, "<$GROUP");
    chomp($gspam = <FILE>);
    close(FILE);
    ($gspam, $ginnocent, $gmisses, $gfp, $gsc, $gic) = split(/\,/, $gspam);
    $spam     -= $gspam;
    $innocent -= $ginnocent;
    $misses   -= $gmisses;
    $fp       -= $gfp;
    $sc       -= $gsc;
    $ic       -= $gic;
  }

  if ($spam+$innocent>0) { 
    $ratio = sprintf("%2.3f", 
      (($spam+$misses)/($spam+$misses+$fp+$innocent)*100)); 
  } else { 
    $ratio = 0; 
  }

  if (open(FILE, "<$USER.rstats")) {
    my($rstats);
   
    chomp($rstats = <FILE>);
    ($rts, $rti, $rtm, $rfp) = split(/\,/, $rstats);
    close(FILE);
    $real_missed = $misses-$rtm;
    $real_caught = $spam-$rts;
    $real_fp = $fp-$rfp;
    if ($real_fp < 0) { $real_fp = 0; }
    $real_innocent = $innocent-$rti; 
    if (($spam-$rts > 0) && ($spam-$rts + $misses-$rtm != 0) &&
        ($real_caught+$real_missed>0) && ($real_fp+$real_innocent>0)) {
      $monthly = sprintf("%2.3f", 
        (100.0-(($real_missed)/($real_caught+$real_missed))*100.0));
      $overall = sprintf("%2.3f", 
        (100.0-(($real_missed+$real_fp) / 
        ($real_fp+$real_innocent+$real_caught+$real_missed))*100.0));
    } else {
      if ($real_caught == 0 && $real_missed > 0) {
        $monthly = 0;
        $overall = 0;
      } else {
        $monthly = 100;
        $overall = 100;
      }
    }

    if ($real_fp+$real_innocent>0) {
      $fpratio = sprintf("%2.3f", ($real_fp/($real_fp+$real_innocent)*100));
    } else {
      $fpratio = 0;
    }

  } else {
    $rts = $spam+$misses;
    $rti = $innocent;
    $rtm = $misses;
    $rfp = $fp;
    open(FILE, ">$USER.rstats");
    print FILE "$rts,$rti,$rtm,$rfp\n";
    close(FILE);
    $monthly = "N/A";
    $fpratio = "N/A";
    $overall = "N/A";
  }

  $DATA{'TIME'} = $time;
  $DATA{'TOTAL_SPAM_SCANNED'} = $spam;
  $DATA{'TOTAL_SPAM_LEARNED'} = $misses;
  $DATA{'TOTAL_NONSPAM_SCANNED'} = $innocent;
  $DATA{'TOTAL_NONSPAM_LEARNED'} = $fp;
  $DATA{'SPAM_RATIO'} = $ratio;
  $DATA{'SPAM_ACCURACY'} = $monthly;
  $DATA{'NONSPAM_ERROR_RATE'} = $fpratio;
  $DATA{'OVERALL_ACCURACY'} = $overall;
  $DATA{'TOTAL_SPAM_CORPUSFED'} = $sc;
  $DATA{'TOTAL_NONSPAM_CORPUSFED'} = $ic;
  $DATA{'TOTAL_SPAM_MISSED'} = $real_missed;
  $DATA{'TOTAL_SPAM_CAUGHT'} = $real_caught;
  $DATA{'TOTAL_NONSPAM_MISSED'} = $real_fp;
  $DATA{'TOTAL_NONSPAM_CAUGHT'} = $real_innocent;

  if ($CURRENT_USER !~ /\@/) {
    $DATA{'LOCAL_DOMAIN'} = "\@$CONFIG{'LOCAL_DOMAIN'}";
  }  

  &output(%DATA);
}

#
# Alert Functions
#

sub AddAlert {
  if ($FORM{'ALERT'} eq "") {
    &error("$CONFIG{'LANG'}->{$LANGUAGE}->{'error_no_alert_specified'}");
  }
  open(FILE, ">>$USER.alerts");
  print FILE "$FORM{'ALERT'}\n";
  close(FILE);
  return;
}
                                                                                
sub DeleteAlert {
  my($line, @alerts);
  $line = 0;
  if ($FORM{'line'} eq "") {
    &Error("$CONFIG{'LANG'}->{$LANGUAGE}->{'error_no_alert_specified'}");
  }
  open(FILE, "<$USER.alerts");
  while(<FILE>) {
    if ($line != $FORM{'line'}) {
      push(@alerts, $_);
    }
    $line++;
  }
  close(FILE);
                                                                                
  open(FILE, ">$USER.alerts");
  foreach(@alerts) { print FILE $_; }
  close(FILE);
  return;
}

sub DisplayAlerts {
  my($supp);
  
  $DATA{'ALERTS'} = <<_end;
<table border="0" cellspacing="0" cellpadding="2">
	<tr>
		<th>$CONFIG{'LANG'}->{$LANGUAGE}->{'alert_name'}</th>
		<th>&nbsp;</th>
	</tr>
_end

  my($rowclass);
  $rowclass = "rowEven";

  do {
    my($line) = 0;
    open(FILE, "<$USER.alerts");
    while(<FILE>) {
      s/</&lt;/g;
      s/>/&gt;/g;
      $DATA{'ALERTS'} .= qq!<tr><td class="$rowclass">$_</td><td class="$rowclass">[<a href="$CONFIG{'ME'}?command=deleteAlert&amp;user=$FORM{'user'}&amp;template=alerts&amp;language=$LANGUAGE&amp;line=$line">$CONFIG{'LANG'}->{$LANGUAGE}->{'remove_alert'}</a>]</td></tr>\n!;
      $line++;

      if ($rowclass eq "rowEven") {
	$rowclass = "rowOdd";
      } else {
	$rowclass = "rowEven";
      }
    }
  }; 

$DATA{'ALERTS'} .= <<_end;
</table>
_end

  &output(%DATA);
  exit;
}

#
# Global Functions
#

sub is_utf8 {
  my $s = "\x80" . $_[0];
  my $internal = unpack "p", pack "p", $s;
  return $s ne $internal;
}

sub htmlize {
  #
  # Replace some characters
  # to be HTML characters
  #
  my($text) = @_;

  my $has_encode = eval{require Encode;};
  my $has_html_entities = eval{require HTML::Entities;};

  if ($text =~ /^(.*?)=\?([^?]+)\?([qb])\?([^?]*)\?=(.*)$/is) {
    if ($has_encode) {
      $text = Encode::decode($2, Encode::decode('MIME-Header', $text));
    }
  } elsif ($text =~ /([\xC2-\xDF][\x80-\xBF]
                     | \xE0[\xA0-\xBF][\x80-\xBF]
                     |[\xE1-\xEC\xEE\xEF][\x80-\xBF]{2}
                     | \xED[\x80-\x9F][\x80-\xBF]
                     | \xF0[\x90-\xBF][\x80-\xBF]{2}
                     |[\xF1-\xF3][\x80-\xBF]{3}
                     | \xF4[\x80-\x8F][\x80-\xBF]{2})/x) {
    if ($has_encode) {
      $text = Encode::decode("utf8", $text);
    }
  }
  if ($has_html_entities) {
    $text = HTML::Entities::encode_entities(HTML::Entities::decode_entities($text));
  }
  if ($text =~ /[\xC2-\xDF][\x80-\xBF]/) {
    if ((-e "htmlize.pl") && (-r "htmlize.pl")) {
      require "htmlize.pl";
      $text = htmlize_chars($text);
    }
  }
  return $text;
}

sub redirect {
  my($loc) = @_;
  $loc =~ s/&amp\;/&/g;
  print "Expires: now\n";
  print "Pragma: no-cache\n";
  print "Cache-control: no-cache\n";
  print "Location: $loc\n\n";
  exit(0);
}

sub htmlheader {
  print "Expires: now\n";
  print "Pragma: no-cache\n";
  print "Cache-control: no-cache\n";
  print "Content-type: text/html\n\n";
}

sub output {
  if ($FORM{'template'} eq "" || $FORM{'template'} !~ /^([A-Z0-9]*)$/i) {
    $FORM{'template'} = "performance";
  }
  &htmlheader;
  my(%DATA) = @_;
  $DATA{'WEB_ROOT'} = $CONFIG{'WEB_ROOT'};
  $DATA{'LANG'} = $LANGUAGE;

  # Check admin permissions
  do {
    if ($CONFIG{'ADMIN'} == 1) {
      $DATA{'NAV_ADMIN'} = qq!<li><a href="admin.cgi?language=$LANGUAGE">$CONFIG{'LANG'}->{$LANGUAGE}->{'admin_suite'}</a></li>!;
      $DATA{'FORM_USER'} = qq!<form action="$CONFIG{'ME'}"><input type="hidden" name="template" value="$FORM{'template'}">$CONFIG{'LANG'}->{$LANGUAGE}->{'user_form'}&nbsp;$USERSELECT&nbsp;&nbsp;&nbsp;&nbsp;$CONFIG{'LANG'}->{$LANGUAGE}->{'lang_select'}$CONFIG{'LANGUAGES'}&nbsp;&nbsp;<input type="submit" value="$CONFIG{'LANG'}->{$LANGUAGE}->{'user_form_submit'}"></form>!;
    } elsif ($CONFIG{'SUBADMIN'} == 1) {
      $DATA{'FORM_USER'} = qq!<form action="$CONFIG{'ME'}"><input type="hidden" name="template" value="$FORM{'template'}"><input type="hidden" name="language" value="$LANGUAGE">$CONFIG{'LANG'}->{$LANGUAGE}->{'user_form'}&nbsp;$USERSELECT&nbsp;&nbsp;&nbsp;&nbsp;$CONFIG{'LANG'}->{$LANGUAGE}->{'lang_select'}$CONFIG{'LANGUAGES'}&nbsp;&nbsp;<input type="submit" value="$CONFIG{'LANG'}->{$LANGUAGE}->{'user_form_submit'}"></form>!;
    } else {
      $DATA{'NAV_ADMIN'} = '';
      $DATA{'FORM_USER'} = qq!<form action="$CONFIG{'ME'}">$CONFIG{'LANG'}->{$LANGUAGE}->{'user_form'}&nbsp;<strong>$CURRENT_USER</strong>&nbsp;&nbsp;&nbsp;&nbsp;$CONFIG{'LANG'}->{$LANGUAGE}->{'lang_select'}$CONFIG{'LANGUAGES'}&nbsp;&nbsp;<input type="submit" value="$CONFIG{'LANG'}->{$LANGUAGE}->{'user_form_submit'}"></form>!;
    }
  };

  open(FILE, "<$CONFIG{'TEMPLATES'}/nav_$FORM{'template'}.html");
  while(<FILE>) { 
    s/\$CGI\$/$CONFIG{'ME'}/g;
    if($FORM{'user'}) {
      if($CONFIG{'ADMIN'} == 1) {
        s/\$USER\$/user=$FORM{'user'}&amp;/g;
      } elsif ($CONFIG{'SUBADMIN'} == 1) {
        my $form_user_domain = (split(/@/, $FORM{'user'}))[1];
        if($CONFIG{'SUBADMIN_USERS'}->{ $FORM{'user'} } == 1 || ($form_user_domain ne "" && $CONFIG{'SUBADMIN_USERS'}->{ "*@" . $form_user_domain } == 1)) {
          s/\$USER\$/user=$FORM{'user'}&amp;/g;
        } else {
          s/\$USER\$//g;
        }
      } else {
        s/\$USER\$//g;
      }
    } else {
      s/\$USER\$//g;
    }
    s/\$([A-Z0-9_]*)\$/$DATA{$1}/g;
    print;
  }
  close(FILE);
  exit(0);
}

sub SafeVars {
  my(%PAIRS) = @_;
  my($url, $key);
  foreach $key (keys(%PAIRS)) {
    my($value) = $PAIRS{$key};
    $value =~ s/([^A-Z0-9])/sprintf("%%%x", ord($1))/gie;
    $url .= "$key=$value&amp;";
  }
  $url =~ s/\&$//;
  return $url;
}

sub error {
  my($error) = @_;
  $FORM{'template'} = "error";
  $DATA{'MESSAGE'} = <<_end;
$CONFIG{'LANG'}->{$LANGUAGE}->{'error_message_part1'}
<BR>
<B>$error</B><BR>
<BR>
$CONFIG{'LANG'}->{$LANGUAGE}->{'error_message_part2'}
_end
  &output(%DATA);
  exit;
}

sub ReadParse {
  my(@pairs, %FORM);
  if ($ENV{'REQUEST_METHOD'} =~ /GET/i)
    { @pairs = split(/&/, $ENV{'QUERY_STRING'}); }
  else {
    my($buffer);
    read(STDIN, $buffer, $ENV{'CONTENT_LENGTH'});
    @pairs = split(/\&/, $buffer);
  }
  foreach(@pairs) {
    my($name, $value) = split(/\=/, $_);

    $name =~ tr/+/ /;
    $name =~ s/%([a-fA-F0-9][a-fA-F0-9])/pack("C", hex($1))/eg;
    $value =~ tr/+/ /;
    $value =~ s/%([a-fA-F0-9][a-fA-F0-9])/pack("C", hex($1))/eg;
    $value =~ s/<!--(.|\n)*-->//g;

    if ($name =~ /^msgid[\d]+$/) {
	push(@{ $FORM{retrain_checked} }, $value);
    } else {
	$FORM{$name} = $value;
    }
  }
  return %FORM;
}

sub CheckQuarantine {
  my($f)=0;
  open(FILE, "<$MAILBOX");
  while(<FILE>) {
    next unless (/^From /);
    $f++;
  }
  close(FILE);   
  if ($f == 0) {
    $f = "$CONFIG{'LANG'}->{$LANGUAGE}->{'empty'}";
  }

  $DATA{'TOTAL_QUARANTINED_MESSAGES'} = $f;
}

sub To12Hour {
  my($h) = @_;
  if ($h < 0) { $h += 24; }
  if ($h>11) { $h -= 12 if ($h>12) ; $h .= "p"; }
  else { $h = "12" if ($h == 0); $h .= "a"; }
  return $h;
}

sub GetPath {
  my($USER) = @_;
  my($PATH);

  # Domain-scale
  if ($CONFIG{'DOMAIN_SCALE'} == 1) {
    my($VPOPUSERNAME, $VPOPDOMAIN);

    $VPOPUSERNAME = (split(/@/, $USER))[0];
    $VPOPDOMAIN = (split(/@/, $USER))[1];
    $VPOPDOMAIN = "local" if ($VPOPDOMAIN eq "");
    ($VPOPUSERNAME = $VPOPDOMAIN, $VPOPDOMAIN = "local") if ($VPOPUSERNAME eq "" && $VPOPDOMAIN ne "");

    $PATH = "$CONFIG{'DSPAM_HOME'}/data/$VPOPDOMAIN/$VPOPUSERNAME/" .
            "$VPOPUSERNAME";
    return $PATH;

  # Normal scale
  } elsif ($CONFIG{'LARGE_SCALE'} == 0) {
    $PATH = "$CONFIG{'DSPAM_HOME'}/data/$USER/$USER";
    return $PATH;
                                                                                
  # Large-scale
  } else {
    if (length($USER)>1) {
      $PATH = "$CONFIG{'DSPAM_HOME'}/data/" . substr($USER, 0, 1) .
        "/". substr($USER, 1, 1) . "/$USER/$USER";
    } else {
      $PATH = "$CONFIG{'DSPAM_HOME'}/data/$USER/$USER/$USER";
    }
    return $PATH;
  }

  &error("Unable to determine filesystem scale");
}


sub GetPrefs {
  my(%PREFS);

  my($FILE) = "$USER.prefs";

  if ($CONFIG{'PREFERENCES_EXTENSION'} == 1) {
    my $PREF_USER = $CURRENT_USER;
    $PREF_USER = "default" if($CURRENT_USER eq "");
    open(PIPE, "$CONFIG{'DSPAM_BIN'}/dspam_admin agg pref " . quotemeta($PREF_USER) . "|");
    while(<PIPE>) {
      chomp;
      my($directive, $value) = split(/\=/);
      $PREFS{$directive} = $value;
    }
    close(PIPE);
  }

  if (keys(%PREFS) eq "0" || $CONFIG{'PREFERENCES_EXTENSION'} != 1) {

    if (! -e "./default.prefs") {
      &error("$CONFIG{'LANG'}->{$LANGUAGE}->{'error_load_default_prefs'}");
    }
    open(FILE, "<./default.prefs");
    while(<FILE>) {
      chomp;
      my($directive, $value) = split(/\=/);
      $PREFS{$directive} = $value;
    }
    close(FILE);

    if( -e $FILE) {
      open(FILE, "<$FILE");
      while(<FILE>) {
	chomp;
	my($directive, $value) = split(/\=/);
	$PREFS{$directive} = $value;
      }
      close(FILE);
    }
  }
  return %PREFS
}
