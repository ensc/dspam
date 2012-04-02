#!/usr/bin/perl

# $Id: admingraph.cgi,v 1.46 2011/06/28 00:13:48 sbajic Exp $
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
use GD::Graph::bars;
use strict;
use vars qw { %CONFIG %FORM %LANG $LANGUAGE @spam @nonspam @period @data @inoc @sm @fp @wh @corpus @virus @black @block };

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
  my($spam, $nonspam, $sm, $fp, $inoc, $wh, $corpus, $virus, $black, $block, $period) = split(/\_/, $FORM{'data'});
  @spam = split(/\,/, $spam);
  @nonspam = split(/\,/, $nonspam);
  @sm = split(/\,/, $sm);
  @fp = split(/\,/, $fp);
  @inoc = split(/\,/, $inoc);
  @wh = split(/\,/, $wh);
  @corpus = split(/\,/, $corpus);
  @virus = split(/\,/, $virus);
  @black = split(/\,/, $black);
  @block = split(/\,/, $block);
  @period = split(/\,/, $period);
};

@data = ([@period], [@inoc], [@corpus], [@virus], [@black], [@block], [@wh], [@spam], [@nonspam], [@sm], [@fp]);
my $mygraph = GD::Graph::bars->new(500, 250);
$mygraph->set(
    x_label     => "$CONFIG{'LANG'}->{$LANGUAGE}->{'graph_legend_x_label_'.$FORM{'x_label'}}",
    y_label     => "$CONFIG{'LANG'}->{$LANGUAGE}->{'graph_legend_nb_messages'}",
    title       => "$FORM{'title'}",
    legend_placement => 'RT',
    legend_spacing => 2,
    bar_width   => 4,
    bar_spacing => 0,
    long_ticks  => 1,
    legend_marker_height => 4,
    show_values => 0,
    boxclr => 'gray90',
    cumulate => 1,
    x_labels_vertical => 1,
    y_tick_number => 4,
    fgclr => 'gray85',
    boxclr => 'gray95',
    textclr => 'black',
    legendclr => 'black',
    labelclr => 'gray60',
    axislabelclr => 'gray40',
    borderclrs => [ undef ],
    dclrs => [ qw ( mediumblue orangered2 deeppink1 black darkturquoise purple red green yellow orange ) ]
) or warn $mygraph->error;

if ($CONFIG{'3D_GRAPHS'} == 1) {
  $mygraph->set(
      shadowclr => 'darkgray',
      shadow_depth => 3,
      bar_width   => 3,
      bar_spacing => 2,
      borderclrs => [ qw ( black ) ]
  ) or warn $mygraph->error;
}

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
$mygraph->set_legend(" $CONFIG{'LANG'}->{$LANGUAGE}->{'graph_legend_inoculations'}"," $CONFIG{'LANG'}->{$LANGUAGE}->{'graph_legend_corpusfeds'}"," $CONFIG{'LANG'}->{$LANGUAGE}->{'graph_legend_virus'}"," $CONFIG{'LANG'}->{$LANGUAGE}->{'graph_legend_RBL'}"," $CONFIG{'LANG'}->{$LANGUAGE}->{'graph_legend_blocklisted'}"," $CONFIG{'LANG'}->{$LANGUAGE}->{'graph_legend_whitelisted'}"," $CONFIG{'LANG'}->{$LANGUAGE}->{'graph_legend_spam'}"," $CONFIG{'LANG'}->{$LANGUAGE}->{'graph_legend_nonspam'}"," $CONFIG{'LANG'}->{$LANGUAGE}->{'graph_legend_spam_misses'}"," $CONFIG{'LANG'}->{$LANGUAGE}->{'graph_legend_falsepositives'}");
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
