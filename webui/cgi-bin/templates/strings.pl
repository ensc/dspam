#!/usr/bin/perl

# $Id: strings.pl,v 1.03 2011/06/28 00:13:48 sbajic Exp $
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

$LANG{'lang_name'}			= "English";
$LANG{'lang_select'}			= "Language";

$LANG{'empty'}				= "Empty";
$LANG{'admin_suite'}			= "Administrative Suite";
$LANG{'alert_name'}			= "Alert Name";
$LANG{'remove_alert'}			= "Remove";
$LANG{'user_form'}			= "Statistical SPAM Protection for";
$LANG{'user_form_submit'}		= "Change";

$LANG{'admin_form'}			= "Statistical SPAM Protection for <strong>Administrator</strong>";
$LANG{'admin_form_submit'}		= "Change";

$LANG{'option_disable_filtering'}	= "Disable DSPAM filtering";
$LANG{'option_enable_filtering'}	= "Enable DSPAM filtering";

$LANG{'quarantine_rating'}		= "Rating";
$LANG{'quarantine_date'}		= "Date";
$LANG{'quarantine_from'}		= "From";
$LANG{'quarantine_subject'}		= "Subject";

$LANG{'history_show'}			= "Show";
$LANG{'history_show_all'}		= "all";
$LANG{'history_show_spam'}		= "spam";
$LANG{'history_show_innocent'}		= "innocent";
$LANG{'history_show_whitelisted'}	= "whitelisted";
$LANG{'history_retrain_as_spam'}	= "spam";
$LANG{'history_retrain_as_innocent'}	= "innocent";
$LANG{'history_retrain_as'}		= "As";
$LANG{'history_retrain_undo'}		= "Undo";
$LANG{'history_retrained'}		= "Retrained";
$LANG{'history_label_resend'}		= "Resend";
$LANG{'history_label_whitelist'}	= "Whitelist";
$LANG{'history_label_spam'}		= "SPAM";
$LANG{'history_label_innocent'}		= "Good";
$LANG{'history_label_miss'}		= "Miss";
$LANG{'history_label_virus'}		= "Virus";
$LANG{'history_label_RBL'}		= "RBL";
$LANG{'history_label_block'}		= "BLOCK";
$LANG{'history_label_corpus'}		= "Corpus";
$LANG{'history_label_unknown'}		= "UNKN";
$LANG{'history_label_error'}		= "Error";

$LANG{'error_no_historic'}		= "No historical data is available.";
$LANG{'error_cannot_open_log'}		= "Unable to open logfile";
$LANG{'error_no_identity'}		= "System Error. I was unable to determine your identity.";
$LANG{'error_invalid_command'}		= "Invalid Command";
$LANG{'error_cannot_write_prefs'}	= "Unable to write preferences";
$LANG{'error_no_sigid'}			= "No Message ID Specified";
$LANG{'error_no_alert_specified'}	= "No Alert Specified.";
$LANG{'error_message_part1'}		= "The following error occured while trying to process your request:";
$LANG{'error_message_part2'}		= "If this problem persists, please contact your administrator.";
$LANG{'error_filesystem_scale'}		= "Unable to determine filesystem scale";
$LANG{'error_load_default_prefs'}	= "Unable to load default preferences";
$LANG{'error_access_denied'}		= "Access Denied";

$LANG{'graph_legend_x_label_hour'}	= "Hour of the day";
$LANG{'graph_legend_x_label_date'}	= "Date";
$LANG{'graph_legend_nb_messages'}	= "Number of Messages";
$LANG{'graph_legend_spam'}		= "SPAM";
$LANG{'graph_legend_good'}		= "Good";
$LANG{'graph_legend_inoculations'}	= "Inoculations";
$LANG{'graph_legend_corpusfeds'}	= "Corpusfeds";
$LANG{'graph_legend_virus'}		= "Virus";
$LANG{'graph_legend_RBL'}		= "Blacklisted (RBL)";
$LANG{'graph_legend_blocklisted'}	= "Blocklisted";
$LANG{'graph_legend_whitelisted'}	= "Auto-Whitelisted";
$LANG{'graph_legend_nonspam'}		= "Nonspam";
$LANG{'graph_legend_spam_misses'}	= "Spam Misses";
$LANG{'graph_legend_falsepositives'}	= "False Positives";

1;
