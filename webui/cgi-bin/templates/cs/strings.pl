#!/usr/bin/perl

# $Id: strings.pl,v 1.04 2011/06/28 00:13:48 sbajic Exp $
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

$LANG{'lang_name'}			= "&#x10c;esky";
$LANG{'lang_select'}			= "Jazyk";

$LANG{'empty'}				= "Pr&aacute;zdn&yacute;";
$LANG{'admin_suite'}			= "Administrace";
$LANG{'alert_name'}			= "Jm&eacute;no v&yacute;strahy";
$LANG{'remove_alert'}			= "Vyjmout";
$LANG{'user_form'}			= "Statistick&aacute; ochrana proti SPAMu pro";
$LANG{'user_form_submit'}		= "Zm&#283;nit";

$LANG{'admin_form'}			= "Statistick&aacute; ochrana proti SPAMu pro <strong>Administrator</strong>";
$LANG{'admin_form_submit'}		= "Zm&#283;nit";

$LANG{'option_disable_filtering'}	= "Zaka&#382; filtrov&aacute;n&iacute; DSPAMem";
$LANG{'option_enable_filtering'}	= "Povol filtrov&aacute;n&iacute; DSPAMem";

$LANG{'quarantine_rating'}		= "Ohodnocen&iacute;";
$LANG{'quarantine_date'}		= "Datum";
$LANG{'quarantine_from'}		= "Od";
$LANG{'quarantine_subject'}		= "Subjekt";

$LANG{'history_show'}			= "Uka&#382;";
$LANG{'history_show_all'}		= "v&scaron;e";
$LANG{'history_show_spam'}		= "spam";
$LANG{'history_show_innocent'}		= "v po&#345;&aacute;dku";
$LANG{'history_show_whitelisted'}	= "na Whitelistu";
$LANG{'history_retrain_as_spam'}	= "spam";
$LANG{'history_retrain_as_innocent'}	= "v po&#345;&aacute;dku";
$LANG{'history_retrain_as'}		= "Jako";
$LANG{'history_retrain_undo'}		= "Zp&#283;t";
$LANG{'history_retrained'}		= "Rekvalifikov&aacute;no";
$LANG{'history_label_resend'}		= "P&#345;eposl&aacute;no";
$LANG{'history_label_whitelist'}	= "Whitelist";
$LANG{'history_label_spam'}		= "SPAM";
$LANG{'history_label_innocent'}		= "Dobr&eacute;";
$LANG{'history_label_miss'}		= "Omyl";
$LANG{'history_label_virus'}		= "Virus";
$LANG{'history_label_RBL'}		= "RBL";
$LANG{'history_label_block'}		= "BLOCK";
$LANG{'history_label_corpus'}		= "Hromadn&eacute;";
$LANG{'history_label_unknown'}		= "NEZN&Aacute;M&Yacute;";
$LANG{'history_label_error'}		= "Chyba";

$LANG{'error_no_historic'}		= "Historie nem&aacute; &#382;&aacute;dn&aacute; data.";
$LANG{'error_cannot_open_log'}		= "Nelze otev&#345;&iacute;t logovac&iacute; soubor";
$LANG{'error_no_identity'}		= "Syst&eacute;mov&aacute; chyba. Nemohl jsem ur&#269;it va&scaron;i identitu.";
$LANG{'error_invalid_command'}		= "Neplatn&yacute; p&#345;&iacute;kaz";
$LANG{'error_cannot_write_prefs'}	= "Nelze zapsat p&#345;edvolby";
$LANG{'error_no_sigid'}			= "Nespecifikov&aacute;no ID zpr&aacute;vy";
$LANG{'error_no_alert_specified'}	= "Nespecifikov&aacute;na v&yacute;straha.";
$LANG{'error_message_part1'}		= "B&#283;hem zpracov&aacute;n&iacute; va&scaron;eho po&#382;adavku se vyskytla tato chyba";
$LANG{'error_message_part2'}		= "Pokud tento probl&eacute;m p&#345;etrv&aacute;v&aacute;, kontaktujte pros&iacute;m va&scaron;eho spr&aacute;vce.";
$LANG{'error_filesystem_scale'}		= "Nelze zjistit &scaron;k&aacute;lov&aacute;n&iacute; va&scaron;eho filesyst&eacute;mu";
$LANG{'error_load_default_prefs'}	= "Nelze na&#269;&iacute;st defaultn&iacute; volby";
$LANG{'error_access_denied'}		= "P&#345;&iacute;stup zak&aacute;z&aacute;n";

$LANG{'graph_legend_x_label_hour'}	= "Hodina dne";
$LANG{'graph_legend_x_label_date'}	= "Datum";
$LANG{'graph_legend_nb_messages'}	= "Pocet zprav";
$LANG{'graph_legend_spam'}		= "SPAM";
$LANG{'graph_legend_good'}		= "OK";
$LANG{'graph_legend_inoculations'}	= "Ockovani";
$LANG{'graph_legend_corpusfeds'}	= "Hromadne ladovani";
$LANG{'graph_legend_virus'}		= "Virus";
$LANG{'graph_legend_RBL'}		= "Na Blacklistu (RBL)";
$LANG{'graph_legend_blocklisted'}	= "Na Blocklistu";
$LANG{'graph_legend_whitelisted'}	= "Na Auto-Whitelistu";
$LANG{'graph_legend_nonspam'}		= "Ne-spam";
$LANG{'graph_legend_spam_misses'}	= "Spam proklouzl";
$LANG{'graph_legend_falsepositives'}	= "Zadrzen dobry";

1;
