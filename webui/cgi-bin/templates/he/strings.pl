#!/usr/bin/perl

# $Id: strings.pl,v 1.07 2011/06/28 00:13:48 sbajic Exp $
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

$LANG{'lang_name'}			= "&#1506;&#1489;&#1512;&#1497;&#1514;";
$LANG{'lang_select'}			= "שפה";

$LANG{'empty'}				= "ריק";
$LANG{'admin_suite'}			= "ערכת ניהול";
$LANG{'alert_name'}			= "שם ההתראה";
$LANG{'remove_alert'}			= "הסר";
$LANG{'user_form'}			= "מערכת הגנה סטטיסטית עבור";
$LANG{'user_form_submit'}		= "החלף";

$LANG{'admin_form'}			= "מערכת הגנה סטטיסטית עבור <b>מנהל המערכת</b>";
$LANG{'admin_form_submit'}		= "החלף";

$LANG{'option_disable_filtering'}	= "הפסק שימוש ב-DSPAM";
$LANG{'option_enable_filtering'}	= "אפשר שימוש ב-DSPAM";

$LANG{'quarantine_rating'}		= "ניקוד";
$LANG{'quarantine_date'}		= "תאריך";
$LANG{'quarantine_from'}		= "מאת";
$LANG{'quarantine_subject'}		= "נושא";

$LANG{'history_show'}			= "הצג";
$LANG{'history_show_all'}		= "הכל";
$LANG{'history_show_spam'}		= "זבל";
$LANG{'history_show_innocent'}		= "תקין";
$LANG{'history_show_whitelisted'}	= "מחוסן";
$LANG{'history_retrain_as_spam'}	= "זבל";
$LANG{'history_retrain_as_innocent'}	= "תקין";
$LANG{'history_retrain_as'}		= "כ-";
$LANG{'history_retrain_undo'}		= "בטל";
$LANG{'history_retrained'}		= "נלמד";
$LANG{'history_label_resend'}		= "נשלח שוב";
$LANG{'history_label_whitelist'}	= "מחוסן";
$LANG{'history_label_spam'}		= "דואר זבל";
$LANG{'history_label_innocent'}		= "תקין";
$LANG{'history_label_miss'}		= "החטאה";
$LANG{'history_label_virus'}		= "וירוס";
$LANG{'history_label_RBL'}		= "רשימה שחורה";
$LANG{'history_label_block'}		= "חסום";
$LANG{'history_label_corpus'}		= "ממאגר";
$LANG{'history_label_unknown'}		= "לא ידוע";
$LANG{'history_label_error'}		= "שגיאה";

$LANG{'error_no_historic'}		= "לא נמצא מידע היסטורי.";
$LANG{'error_cannot_open_log'}		= "לא ניתן לפתוח את קובץ ה-לוג";
$LANG{'error_no_identity'}		= "שגיאת מערכת. לא ניתן לקבוע את זהותך.";
$LANG{'error_invalid_command'}		= "פקודה לא חןקית";
$LANG{'error_cannot_write_prefs'}	= "לא ניתן לכתוב את ההעדפות";
$LANG{'error_no_sigid'}			= "לא צויין מציין הןדעה )MSG ID(";
$LANG{'error_no_alert_specified'}	= "לא צויינה התראה.";
$LANG{'error_message_part1'}		= "השגיאה הבאה ארעה בזמן עיבוד בקשתך:";
$LANG{'error_message_part2'}		= "אם בעיה לא לא נעלמת, נא פנה למנהל המערכת.";
$LANG{'error_filesystem_scale'}		= "לא ניתן לקבוע את הגדרת המערכת";
$LANG{'error_load_default_prefs'}	= "לא ניתן לטעון את העדפות ברירת המחדל";
$LANG{'error_access_denied'}		= "הגישה נשחתה";

# The text for the legend should be encoded in HTML character entities in order
# to be displayed correctly by GD. You should as well set GRAPHS_X_LABEL_FONT,
# GRAPHS_Y_LABEL_FONT and GRAPHS_LEGEND_FONT in configure.pl to a TTF font that
# is capable to display Hebrew characters. Unfortunately U+202B and U+202C don't
# work with GD and you NEED to write all the text backwards since GD does not
# know anything about right-to-left page direction.
# If you don't know how to convert the characters to be HTML charater entities
# then use something like recode (http://recode.progiciels-bpi.ca/) and/or
# htmlrecode (http://bisqwit.iki.fi/source/htmlrecode.html).
$LANG{'graph_legend_x_label_hour'}	= "&#1492;&#1506;&#1513;";
$LANG{'graph_legend_x_label_date'}	= "&#1498;&#1497;&#1512;&#1488;&#1514;";
$LANG{'graph_legend_nb_messages'}	= "&#1514;&#1493;&#1506;&#1491;&#1493;&#1492;&#1492; &#1512;&#1508;&#1505;&#1502;";
$LANG{'graph_legend_spam'}		= "&#1500;&#1489;&#1494; &#1512;&#1488;&#1493;&#1491;";
$LANG{'graph_legend_good'}		= "&#1503;&#1497;&#1511;&#1514;";
$LANG{'graph_legend_inoculations'}	= "&#1501;&#1497;&#1504;&#1493;&#1505;&#1497;&#1495;";
$LANG{'graph_legend_corpusfeds'}	= "&#1512;&#1490;&#1488;&#1502;&#1502;";
$LANG{'graph_legend_virus'}		= "&#1505;&#1493;&#1512;&#1497;&#1493;";
$LANG{'graph_legend_RBL'}		= "&#1492;&#1512;&#1493;&#1495;&#1513; &#1492;&#1502;&#1497;&#1513;&#1512;&#1489;";
$LANG{'graph_legend_blocklisted'}	= "&#1493;&#1501;&#1505;&#1495;&#1504;";
$LANG{'graph_legend_whitelisted'}	= "&#1501;&#1497;&#1504;&#1505;&#1493;&#1495;&#1502;";
$LANG{'graph_legend_nonspam'}		= "&#1503;&#1497;&#1511;&#1514;";
$LANG{'graph_legend_spam_misses'}	= "&#1500;&#1489;&#1494; &#1512;&#1488;&#1493;&#1491; &#1514;&#1493;&#1488;&#1496;&#1495;&#1492;";
$LANG{'graph_legend_falsepositives'}	= "&#1503;&#1497;&#1511;&#1514; &#1512;&#1488;&#1493;&#1491; &#1514;&#1493;&#1488;&#1496;&#1495;&#1492;";

1;
