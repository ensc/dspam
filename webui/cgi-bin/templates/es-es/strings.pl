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

$LANG{'lang_name'}			= "Espa&ntilde;ol";
$LANG{'lang_select'}			= "Lengua";

$LANG{'empty'}				= "Vacio";
$LANG{'admin_suite'}			= "Suite Administrativa";
$LANG{'alert_name'}			= "Nombre de alerta";
$LANG{'remove_alert'}			= "Eliminar";
$LANG{'user_form'}			= "Proteccion estadistica de SPAM para";
$LANG{'user_form_submit'}		= "Cambiar";

$LANG{'admin_form'}			= "Proteccion estadistica de SPAM para <strong>Administrador</strong>";
$LANG{'admin_form_submit'}		= "Cambiar";

$LANG{'option_disable_filtering'}	= "Desactivar filtro DSPAM";
$LANG{'option_enable_filtering'}	= "Activar filtro DSPAM";

$LANG{'quarantine_rating'}		= "probabilidad";
$LANG{'quarantine_date'}		= "Fecha";
$LANG{'quarantine_from'}		= "Remitente";
$LANG{'quarantine_subject'}		= "Asunto";

$LANG{'history_show'}			= "Mostrar";
$LANG{'history_show_all'}		= "todos";
$LANG{'history_show_spam'}		= "spam";
$LANG{'history_show_innocent'}		= "inocente";
$LANG{'history_show_whitelisted'}	= "lista blanca";
$LANG{'history_retrain_as_spam'}	= "spam";
$LANG{'history_retrain_as_innocent'}	= "inocente";
$LANG{'history_retrain_as'}		= "Como";
$LANG{'history_retrain_undo'}		= "Deshacer";
$LANG{'history_retrained'}		= "Re-ense&ntilde;ado";
$LANG{'history_label_resend'}		= "Reenviar";
$LANG{'history_label_whitelist'}	= "lista blanca";
$LANG{'history_label_spam'}		= "SPAM";
$LANG{'history_label_innocent'}		= "Bueno";
$LANG{'history_label_miss'}		= "Fallo";
$LANG{'history_label_virus'}		= "Virus";
$LANG{'history_label_RBL'}		= "RBL";
$LANG{'history_label_block'}		= "BLOQUEAR";
$LANG{'history_label_corpus'}		= "Corpus";
$LANG{'history_label_unknown'}		= "UNKN";
$LANG{'history_label_error'}		= "Error";

$LANG{'error_no_historic'}		= "No hay informaci&oacute;n del historial.";
$LANG{'error_cannot_open_log'}		= "No es posible abrir el archivo de errores";
$LANG{'error_no_identity'}		= "Error de sistema. No se ha podido determinar su identidad.";
$LANG{'error_invalid_command'}		= "Comando no valido";
$LANG{'error_cannot_write_prefs'}	= "No se han podido guardar los ajustes";
$LANG{'error_no_sigid'}			= "No se ha especificado ID del mensaje";
$LANG{'error_no_alert_specified'}	= "No se ha especificado una alerta.";
$LANG{'error_message_part1'}		= "El siguiente error ha ocurrido al intentar procesar tu peticion:";
$LANG{'error_message_part2'}		= "Si este problema continua, ponte en contacto con tu administrador.";
$LANG{'error_filesystem_scale'}		= "No se ha podido determinar la escala del sistema de archivos";
$LANG{'error_load_default_prefs'}	= "No se han podido cargar los ajustes por defecto";
$LANG{'error_access_denied'}		= "Acceso denegado";

$LANG{'graph_legend_x_label_hour'}	= "Hora del d&iacute;a";
$LANG{'graph_legend_x_label_date'}	= "Fecha";
$LANG{'graph_legend_nb_messages'}	= "Numero de mensajes";
$LANG{'graph_legend_spam'}		= "SPAM";
$LANG{'graph_legend_good'}		= "Buenos";
$LANG{'graph_legend_inoculations'}	= "Inoculaciones";
$LANG{'graph_legend_corpusfeds'}	= "Corpusfeds";
$LANG{'graph_legend_virus'}		= "Virus";
$LANG{'graph_legend_RBL'}		= "En lista negra (RBL)";
$LANG{'graph_legend_blocklisted'}	= "Blocklisted";
$LANG{'graph_legend_whitelisted'}	= "Lista blanca automatica";
$LANG{'graph_legend_nonspam'}		= "No spam";
$LANG{'graph_legend_spam_misses'}	= "Mensajes spam no cazados";
$LANG{'graph_legend_falsepositives'}	= "Falsos positivos";

1;
