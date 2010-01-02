#!/usr/bin/perl

# $Id: strings.pl,v 1.03 2010/01/02 03:55:14 sbajic Exp $
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
$LANG{'history_retrained'}		= "נלמדו";
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
# GRAPHS_Y_LABEL_FONT and GRAPHS_LEGEND_FONT in configue.pl to a TTF font that
# is capable to display Hebrew characters.
$LANG{'graph_legend_nb_messages'}	= "&#1502;&#1505;&#1508;&#1512; &#1492;&#1492;&#1493;&#1491;&#1506;&#1493;&#1514;";
$LANG{'graph_legend_spam'}		= "&#1491;&#1493;&#1488;&#1512; &#1494;&#1489;&#1500;";
$LANG{'graph_legend_good'}		= "&#1514;&#1511;&#1497;&#1503;";
$LANG{'graph_legend_inoculations'}	= "&#1495;&#1497;&#1505;&#1493;&#1504;&#1497;&#1501;";
$LANG{'graph_legend_corpusfeds'}	= "&#1502;&#1502;&#1488;&#1490;&#1512;";
$LANG{'graph_legend_virus'}		= "&#1493;&#1497;&#1512;&#1493;&#1505;";
$LANG{'graph_legend_RBL'}		= "&#1489;&#1512;&#1513;&#1497;&#1502;&#1492; &#1513;&#1497;&#1493;&#1512;&#1492;";
$LANG{'graph_legend_blocklisted'}	= "&#1504;&#1495;&#1505;&#1501;&#1493;";
$LANG{'graph_legend_whitelisted'}	= "&#1502;&#1495;&#1493;&#1505;&#1504;&#1497;&#1501;";
$LANG{'graph_legend_nonspam'}		= "&#1514;&#1511;&#1497;&#1503;";
$LANG{'graph_legend_spam_misses'}	= "&#1492;&#1495;&#1496;&#1488;&#1493;&#1514; &#1491;&#1493;&#1488;&#1512; &#1494;&#1489;&#1500;";
$LANG{'graph_legend_falsepositives'}	= "&#1492;&#1495;&#1496;&#1488;&#1493;&#1514; &#1491;&#1493;&#1488;&#1512; &#1514;&#1511;&#1497;&#1503;";

1;
