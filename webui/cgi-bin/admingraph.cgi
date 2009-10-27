#!/usr/bin/perl

# $Id: admingraph.cgi,v 1.42 2009/08/18 00:08:28 sbajic Exp $
# DSPAM
# COPYRIGHT (C) DSPAM PROJECT 2002-2009
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; version 2
# of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

use CGI ':standard';
use GD::Graph::bars;
use strict;
use vars qw { %CONFIG %FORM %LANG @spam @nonspam @period @data @inoc @sm @fp @wh @corpus @virus @black @block };

# Read configuration parameters common to all CGI scripts
require "configure.pl";

#
# Read language file
#
if (-s "$CONFIG{'TEMPLATES'}/strings.pl") {
  require "$CONFIG{'TEMPLATES'}/strings.pl";
} elsif (-s "$CONFIG{'TEMPLATES'}/../strings.pl") {
  require "$CONFIG{'TEMPLATES'}/../strings.pl";
} else {
  &error("Missing language file strings.pl.");
}

%FORM = &ReadParse();

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
    x_label     => "$FORM{'x_label'}",
    y_label     => "$LANG{'graph_legend_nb_messages'}",
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

$mygraph->set_legend_font(GD::gdMediumBoldFont);
$mygraph->set_legend(" $LANG{'graph_legend_inoculations'}"," $LANG{'graph_legend_corpusfeds'}"," $LANG{'graph_legend_virus'}"," $LANG{'graph_legend_RBL'}"," $LANG{'graph_legend_blocklisted'}"," $LANG{'graph_legend_whitelisted'}"," $LANG{'graph_legend_spam'}"," $LANG{'graph_legend_nonspam'}"," $LANG{'graph_legend_spam_misses'}"," $LANG{'graph_legend_falsepositives'}");
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
