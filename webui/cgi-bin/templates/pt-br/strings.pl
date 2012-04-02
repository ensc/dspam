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

$LANG{'lang_name'}			= "Portugu&ecirc;s brasileiro";
$LANG{'lang_select'}			= "L&iacute;ngua";

$LANG{'empty'}				= "Vazio";
$LANG{'admin_suite'}			= "Suite Administrativa";
$LANG{'alert_name'}			= "Nome em Alerta";
$LANG{'remove_alert'}			= "Remover";
$LANG{'user_form'}			= "Prote&ccedil;&atilde;o Estat&iacute;stica para";
$LANG{'user_form_submit'}		= "Trocar";

$LANG{'admin_form'}			= "Prote&ccedil;&atilde;o Estat&iacute;stica para <strong>Administrador</strong>";
$LANG{'admin_form_submit'}		= "Trocar";

$LANG{'option_disable_filtering'}	= " Desabilita DSPAM";
$LANG{'option_enable_filtering'}	= " Habilita DSPAM";

$LANG{'quarantine_rating'}		= "&Iacute;ndice";
$LANG{'quarantine_date'}		= "Data";
$LANG{'quarantine_from'}		= "De";
$LANG{'quarantine_subject'}		= "Assunto";

$LANG{'history_show'}			= "Mostrar";
$LANG{'history_show_all'}		= "todas";
$LANG{'history_show_spam'}		= "spam";
$LANG{'history_show_innocent'}		= "inocente";
$LANG{'history_show_whitelisted'}	= "lista branca";
$LANG{'history_retrain_as_spam'}	= "spam";
$LANG{'history_retrain_as_innocent'}	= "inocente";
$LANG{'history_retrain_as'}		= "Como";
$LANG{'history_retrain_undo'}		= "Desfazer";
$LANG{'history_retrained'}		= "Retreinado";
$LANG{'history_label_resend'}		= "Reenviado";
$LANG{'history_label_whitelist'}	= "Lista Branca";
$LANG{'history_label_spam'}		= "SPAM";
$LANG{'history_label_innocent'}		= "Inocente";
$LANG{'history_label_miss'}		= "Perdido";
$LANG{'history_label_virus'}		= "V&iacute;rus";
$LANG{'history_label_RBL'}		= "RBL";
$LANG{'history_label_block'}		= "BLOQUEADO";
$LANG{'history_label_corpus'}		= "Ger. Corpus";
$LANG{'history_label_unknown'}		= "DESC";
$LANG{'history_label_error'}		= "Erro";

$LANG{'error_no_historic'}		= "Nenhum dado historico dispoiível";
$LANG{'error_cannot_open_log'}		= "Incapaz de abrir log";
$LANG{'error_no_identity'}		= "Erro de Sistema. Nao fui capaz de identifiaá-lo.";
$LANG{'error_invalid_command'}		= "Comando Invalido";
$LANG{'error_cannot_write_prefs'}	= "Incapaz de salvar preferencias";
$LANG{'error_no_sigid'}			= "ID da Mensagem nao especificado";
$LANG{'error_no_alert_specified'}	= "Nenhum Alerta especificado.";
$LANG{'error_message_part1'}		= "Ocorreu o seguinte erro enquanto tentava processar sua requisicaão:";
$LANG{'error_message_part2'}		= "Se este problema persistir, contate seu administrador.";
$LANG{'error_filesystem_scale'}		= "Incapaz de determinar o tamanho do sistema de arquivos";
$LANG{'error_load_default_prefs'}	= "Incapaz de carregar preferencias padrao";
$LANG{'error_access_denied'}		= "Acesso Negado";

$LANG{'graph_legend_x_label_hour'}	= "Hora do dia";
$LANG{'graph_legend_x_label_date'}	= "Data";
$LANG{'graph_legend_nb_messages'}	= "Numero de Mensagens";
$LANG{'graph_legend_spam'}		= "SPAM";
$LANG{'graph_legend_good'}		= "Inocentes";
$LANG{'graph_legend_inoculations'}	= "Inoculacoes";
$LANG{'graph_legend_corpusfeds'}	= "Treino Base";
$LANG{'graph_legend_virus'}		= "Virus";
$LANG{'graph_legend_RBL'}		= "Lista Negra (RBL)";
$LANG{'graph_legend_blocklisted'}	= "Bloqueadas";
$LANG{'graph_legend_whitelisted'}	= "Auto-Lista Branca";
$LANG{'graph_legend_nonspam'}		= "Nao spam";
$LANG{'graph_legend_spam_misses'}	= "Spams Perdidos";
$LANG{'graph_legend_falsepositives'}	= "Falsos Positivos";

1;
