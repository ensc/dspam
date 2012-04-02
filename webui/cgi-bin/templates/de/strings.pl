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

$LANG{'lang_name'}			= "Deutsch";
$LANG{'lang_select'}			= "Sprache";

$LANG{'empty'}				= "Leer";
$LANG{'admin_suite'}			= "Administrativer Verwaltungsbereich";
$LANG{'alert_name'}			= "Alarm Name";
$LANG{'remove_alert'}			= "Entfehrne";
$LANG{'user_form'}			= "Statistischer SPAM Schutz f&uuml;r";
$LANG{'user_form_submit'}		= "&Auml;ndern";

$LANG{'admin_form'}			= "Statistischer SPAM Schutz f&uuml;r <strong>Administrator</strong>";
$LANG{'admin_form_submit'}		= "&Auml;ndern";

$LANG{'option_disable_filtering'}	= "Ausdr&uuml;ckliche Ablehnung der DSPAM Filterung";
$LANG{'option_enable_filtering'}	= "Ausdr&uuml;ckliche Zustimmung der DSPAM Filterung";

$LANG{'quarantine_rating'}		= "Bewertung";
$LANG{'quarantine_date'}		= "Datum";
$LANG{'quarantine_from'}		= "Von";
$LANG{'quarantine_subject'}		= "Betreff";

$LANG{'history_show'}			= "Anzeigen";
$LANG{'history_show_all'}		= "alle";
$LANG{'history_show_spam'}		= "spam";
$LANG{'history_show_innocent'}		= "unschuldig";
$LANG{'history_show_whitelisted'}	= "whitelisted";
$LANG{'history_retrain_as_spam'}	= "spam";
$LANG{'history_retrain_as_innocent'}	= "unschuldig";
$LANG{'history_retrain_as'}		= "Als";
$LANG{'history_retrain_undo'}		= "R&uuml;ckg&auml;ngig";
$LANG{'history_retrained'}		= "Umtrainiert";
$LANG{'history_label_resend'}		= "Resend";
$LANG{'history_label_whitelist'}	= "Whitelist";
$LANG{'history_label_spam'}		= "SPAM";
$LANG{'history_label_innocent'}		= "Gut";
$LANG{'history_label_miss'}		= "Verfehlt";
$LANG{'history_label_virus'}		= "Virus";
$LANG{'history_label_RBL'}		= "RBL";
$LANG{'history_label_block'}		= "BLOCK";
$LANG{'history_label_corpus'}		= "Korpus";
$LANG{'history_label_unknown'}		= "UNKN";
$LANG{'history_label_error'}		= "Fehler";

$LANG{'error_no_historic'}		= "Keine historischen Daten vorhanden.";
$LANG{'error_cannot_open_log'}		= "Kann Protokolldatei nicht &ouml;ffnen";
$LANG{'error_no_identity'}		= "System Fehler. Kann ihre Identit&auml;t nicht ermitteln.";
$LANG{'error_invalid_command'}		= "Ung&uuml;ltiger Befehl";
$LANG{'error_cannot_write_prefs'}	= "Kann Einstellungen nicht speichern";
$LANG{'error_no_sigid'}			= "Keine Nachrichtenkennung angegeben";
$LANG{'error_no_alert_specified'}	= "Kein Alarm spezifiziert.";
$LANG{'error_message_part1'}		= "Der folgende Fehler ist aufgetreten, w&auml;hrend versucht wurde ihre Anfrage zu verarbeiten:";
$LANG{'error_message_part2'}		= "Wenn dieser Fehler bestehen bleibt, dann kontaktieren sie ihren Administrator.";
$LANG{'error_filesystem_scale'}		= "Kann nicht Dateisystemskalierung festzustellen";
$LANG{'error_load_default_prefs'}	= "Kann vorgabe Werte nicht laden";
$LANG{'error_access_denied'}		= "Zugriff verweigert";

$LANG{'graph_legend_x_label_hour'}	= "Stunde des Tages";
$LANG{'graph_legend_x_label_date'}	= "Datum";
$LANG{'graph_legend_nb_messages'}	= "Anzahl der Nachrichten";
$LANG{'graph_legend_spam'}		= "SPAM";
$LANG{'graph_legend_good'}		= "Gut";
$LANG{'graph_legend_inoculations'}	= "Impfungen";
$LANG{'graph_legend_corpusfeds'}	= "Korpuszufuhr";
$LANG{'graph_legend_virus'}		= "Virus";
$LANG{'graph_legend_RBL'}		= "Blacklisted (RBL)";
$LANG{'graph_legend_blocklisted'}	= "Blocklisted";
$LANG{'graph_legend_whitelisted'}	= "Auto-Whitelisted";
$LANG{'graph_legend_nonspam'}		= "Kein Spam";
$LANG{'graph_legend_spam_misses'}	= "Spam Verfehlungen";
$LANG{'graph_legend_falsepositives'}	= "Falsch positives Ergebnis";

1;
