#!/usr/bin/perl

# $Id: strings.pl,v 1.00 2009/08/18 01:17:50 sbajic Exp $
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

$LANG{'empty'}				= "Vide";
$LANG{'admin_suite'}			= "Administration";
$LANG{'alert_name'}			= "Nom de l'alerte";
$LANG{'remove_alert'}			= "Supprimer";
$LANG{'user_form'}			= "Centre de contr&ocirc;le DSPAM pour";
$LANG{'user_form_submit'}		= "Changer";

$LANG{'option_disable_filtering'}       = "D&eacute;sactiver le filtre DSPAM";
$LANG{'option_enable_filtering'}        = "Activer le filtre DSPAM";

$LANG{'quarantine_rating'}		= "Score";
$LANG{'quarantine_date'}		= "Date";
$LANG{'quarantine_from'}		= "Exp&eacute;diteur";
$LANG{'quarantine_subject'}		= "Objet";

$LANG{'history_show'}			= "Afficher&nbsp;";
$LANG{'history_show_all'}		= "tous";
$LANG{'history_show_spam'}		= "pourriels";
$LANG{'history_show_innocent'}		= "l&eacute;gitimes";
$LANG{'history_show_virus'}		= "virus";
$LANG{'history_show_whitelisted'}	= "mis en liste blanche";
$LANG{'history_retrain_as_spam'}	= "pourriel";
$LANG{'history_retrain_as_innocent'}	= "l&eacute;gitime";
$LANG{'history_retrain_as'}		= "comme";
$LANG{'history_retrained'}		= "R&eacute;-analys&eacute;";
$LANG{'history_retrain_undo'}		= "d&eacute;faire";
$LANG{'history_label_resend'}		= "Renvoi";
$LANG{'history_label_whitelist'}	= "Liste blanche";
$LANG{'history_label_spam'}		= "Pourriel";
$LANG{'history_label_innocent'}		= "L&eacute;gitime";
$LANG{'history_label_miss'}		= "Manqu&eacute;";
$LANG{'history_label_virus'}		= "Virus";
$LANG{'history_label_RBL'}		= "RBL";
$LANG{'history_label_block'}		= "Bloqu&eacute;";
$LANG{'history_label_corpus'}		= "Corpus";
$LANG{'history_label_unknown'}		= "Inconnu";
$LANG{'history_label_error'}		= "Erreur";

$LANG{'error_no_historic'}		= "Aucune donn&eacute;e d'historique n'est disponible.";
$LANG{'error_cannot_open_log'}		= "Impossible d'ouvrir le journal";
$LANG{'error_no_identity'}		= "Erreur syst&egrave;me. Impossible de d&eacute;terminer votre identit&eacute;.";
$LANG{'error_invalid_command'}		= "Commande invalide";
$LANG{'error_cannot_write_prefs'}	= "Impossible d'enregistrer les pr&eacute;f&eacute;rences";
$LANG{'error_no_sigid'}			= "Aucun identifiant de message n'a &eacute;t&eacute; sp&eacute;cifi&eacute;";
$LANG{'error_no_alert_specified'}	= "Aucune alerte sp&eacute;cifi&eacute;e.";
$LANG{'error_message_part1'}		= "L'erreur suivante s'est produite pendant le traitement de votre requ&ecirc;te&nbsp;:";
$LANG{'error_message_part2'}		= "Si le probl&egrave;me persiste, contactez votre administrateur.";
$LANG{'error_filesystem_scale'}		= "Impossible de d&eacute;terminer l'organisation du syst&egrave;me de fichiers";
$LANG{'error_load_default_prefs'}	= "Impossible de charger les pr&eacute;f&eacute;rences par d&eacute;faut.";
$LANG{'error_access_denied'}		= "Acc&egrave;s interdit";

$LANG{'graph_legend_nb_messages'}	= "Nombre de messages";
$LANG{'graph_legend_spam'}		= "Pourriels";
$LANG{'graph_legend_good'}		= "Messages legitimes";
$LANG{'graph_legend_inoculations'}	= "Inoculations";
$LANG{'graph_legend_corpusfeds'}	= "Depuis le corpus";
$LANG{'graph_legend_virus'}		= "Virus";
$LANG{'graph_legend_RBL'}		= "RBL";
$LANG{'graph_legend_blocklisted'}	= "Mis en liste noire";
$LANG{'graph_legend_whitelisted'}	= "Mis en liste blanche";
$LANG{'graph_legend_nonspam'}		= "Messages legitimes";
$LANG{'graph_legend_spam_misses'}	= "Pourriels manques";
$LANG{'graph_legend_falsepositives'}	= "Faux positifs";

1;
