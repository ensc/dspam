#!/usr/bin/perl

# $Id: graph.cgi,v 1.45 2011/06/28 00:13:48 sbajic Exp $
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

use CGI ':standard';
use GD::Graph::lines3d;
use GD::Graph::lines;
use strict;
use vars qw { %CONFIG %FORM %LANG $LANGUAGE @spam_day @nonspam_day @period @data };

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
# Parse form
#
%FORM = &ReadParse();

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

GD::Graph::colour::read_rgb("rgb.txt"); 

do {
  my($spam, $nonspam, $period) = split(/\_/, $FORM{'data'});
  @spam_day = split(/\,/, $spam);
  @nonspam_day = split(/\,/, $nonspam);
  @period = split(/\,/, $period);
};

@data = ([@period], [@spam_day], [@nonspam_day]);
my $mygraph;
if ($CONFIG{'3D_GRAPHS'} == 1) {
  $mygraph = GD::Graph::lines3d->new(500, 200);
} else {
  $mygraph = GD::Graph::lines->new(500, 200);
}
$mygraph->set(
    x_label     => "$CONFIG{'LANG'}->{$LANGUAGE}->{'graph_legend_x_label_'.$FORM{'x_label'}}",
    y_label     => "$CONFIG{'LANG'}->{$LANGUAGE}->{'graph_legend_nb_messages'}",
#   title       => "$FORM{'title'}",
    line_width   => 2,
    dclrs => [ qw(lred dgreen) ],
    fgclr => 'gray85',
    boxclr => 'gray95',
    textclr => 'black',
    legendclr => 'black',
    labelclr => 'gray60',
    axislabelclr => 'gray40',
    long_ticks  => 1,
    show_values => 0
) or warn $mygraph->error;

#         dclrs => [ qw( darkorchid2 mediumvioletred deeppink darkturquoise ) ],

if (defined $CONFIG{'GRAPHS_X_LABEL_FONT'} && $CONFIG{'GRAPHS_X_LABEL_FONT'} ne "" && -r $CONFIG{'GRAPHS_X_LABEL_FONT'}) {
  $mygraph->set_x_label_font([$CONFIG{'GRAPHS_X_LABEL_FONT'}, GD::gdMediumBoldFont, 'verdana', 'arial'], 8);
} else {
  $mygraph->set_x_label_font(GD::gdMediumBoldFont);
}
if (defined $CONFIG{'GRAPHS_Y_LABEL_FONT'} && $CONFIG{'GRAPHS_Y_LABEL_FONT'} ne "" && -r $CONFIG{'GRAPHS_Y_LABEL_FONT'}) {
  $mygraph->set_y_label_font([$CONFIG{'GRAPHS_Y_LABEL_FONT'}, GD::gdMediumBoldFont, 'verdana', 'arial'], 8);
} else {
  $mygraph->set_y_label_font(GD::gdMediumBoldFont);
}
if (defined $CONFIG{'GRAPHS_LEGEND_FONT'} && $CONFIG{'GRAPHS_LEGEND_FONT'} ne "" && -r $CONFIG{'GRAPHS_LEGEND_FONT'}) {
  $mygraph->set_legend_font([$CONFIG{'GRAPHS_LEGEND_FONT'}, GD::gdMediumBoldFont, 'verdana', 'arial'], 8);
} else {
  $mygraph->set_legend_font(GD::gdMediumBoldFont);
}
$mygraph->set_legend("$CONFIG{'LANG'}->{$LANGUAGE}->{'graph_legend_spam'}","$CONFIG{'LANG'}->{$LANGUAGE}->{'graph_legend_good'}");
my $myimage = $mygraph->plot(\@data) or die $mygraph->error;
                                                                                
print "Content-type: image/png\n\n";
print $myimage->png;

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
    $FORM{$name} = $value;
  }
  return %FORM;
}

sub htmlheader {
  print "Expires: now\n";
  print "Pragma: no-cache\n";
  print "Cache-control: no-cache\n";
  print "Content-type: text/html\n\n";
}
