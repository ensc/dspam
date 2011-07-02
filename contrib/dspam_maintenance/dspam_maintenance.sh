#!/bin/bash
#
# $Id: dspam_maintenance.sh,v 1.22 2011/07/02 12:54:17 sbajic Exp $
#
# Copyright 2007-2010 Stevan Bajic <stevan@bajic.ch>
# Distributed under the terms of the GNU Affero General Public License v3
#
# Purpose: Remove old signatures and unimportant tokens from the DSPAM
#          database. Purge old log entries in user and system logs.
#
# Note   : I originally wrote that script for Gentoo Linux in 2007 and it
#          is since then distributed with the Gentoo DSPAM ebuild. This
#          edition here is slightly changed to be more flexible. I have
#          not tested that script on other distros except on Gentoo Linux
#          and on a older CentOS distro. Other members from the DSPAM
#          mailing list have tested the script on Debian. Should you find
#          any issues with this script then please post a message on the
#          DSPAM user mailing list.
#
# Note   : This script can be called from the command line or run from
#          within cron. Either add a line in your crontab or add this
#          script in your cron.{hourly,daily,weekly,monthly}. Running
#          this script every hour might not be the best idea but it's
#          your decision if you want to do so.
###

DSPAM_CONFIGDIR=""
INCLUDE_DIRS=""
DSPAM_HOMEDIR=""
DSPAM_PURGE_SCRIPT_DIR=""
DSPAM_BIN_DIR=""
MYSQL_BIN_DIR="/usr/bin"
PGSQL_BIN_DIR="/usr/bin"
SQLITE_BIN_DIR="/usr/bin"
SQLITE3_BIN_DIR="/usr/bin"


#
# Parse optional command line parameters
#
for foo in $@
do
	case "${foo}" in
		--profile=*) PROFILE="${foo#--profile=}";;
		--logdays=*) LOGROTATE_AGE="${foo#--logdays=}";;
		--signatures=*) SIGNATURE_AGE="${foo#--signatures=}";;
		--neutral=*) NEUTRAL_AGE="${foo#--neutral=}";;
		--unused=*) UNUSED_AGE="${foo#--unused=}";;
		--hapaxes=*) HAPAXES_AGE="${foo#--hapaxes=}";;
		--hits1s=*) HITS1S_AGE="${foo#--hits1s=}";;
		--hits1i=*) HITS1I_AGE="${foo#--hits1i=}";;
		--purgescriptdir=*) DSPAM_PURGE_SCRIPT_DIR="${foo#--purgescriptdir=}";;
		--without-sql-purge) USE_SQL_PURGE="false";;
		--with-sql-optimization) USE_SQL_OPTIMIZATION="true";;
		--with-sql-autoupdate) USE_SQL_AUTOUPDATE="true";;
		--with-all-drivers) PURGE_ALL_DRIVERS="true";;
		--verbose) VERBOSE="true";;
		--help | -help | --h | -h)
			echo "Usage: $0"
			echo
			echo "  [--profile=[PROFILE]]"
			echo "    Specify a storage profile from dspam.conf. The storage profile"
			echo "    selected will be used for all database connectivity. See"
			echo "    dspam.conf for more information."
			echo
			echo "  [--logdays=no_of_days]"
			echo "    All log entries older than 'no_of_days' days will be removed."
			echo "    Note: Default is 31 days. (for more info: man dspam_logrotate)"
			echo
			echo "  [--signatures=no_of_days]"
			echo "    All signatures older than 'no_of_days' days will be removed."
			echo "    Note: Default value is set by the 'PurgeSignatures' option in"
			echo "    dspam.conf, if this option is not set, the default value is 14"
			echo "    days. This option is only used for the Hash driver."
			echo "    For more info: man dspam_clean"
			echo
			echo "  [--neutral=no_of_days]"
			echo "    Remove tokens whose probability is between 0.35 and 0.65."
			echo "    Note: Default value is set by the 'PurgeNeutral' option in dspam.conf,"
			echo "    if this option is not set, the default value is 90 days."
			echo "    For more info: man dspam_clean"
			echo
			echo "  [--unused=no_of_days]"
			echo "    Remove tokens which have not been used for a long period of time."
			echo "    Note: Default value is set by the 'PurgeUnused' option in dspam.conf,"
			echo "    if this option is not set, the default value is 90 days."
			echo "    For more info: man dspam_clean"
			echo
			echo "  [--hapaxes=no_of_days]"
			echo "    Remove tokens with a total hit count below 5."
			echo "    Note: Default value is set by the 'PurgeHapaxes' option in dspam.conf,"
			echo "    if this option is not set, the default value is 30 days."
			echo "    For more info: man dspam_clean"
			echo
			echo "  [--hits1s=no_of_days]"
			echo "    Remove tokens with a single SPAM hit."
			echo "    Note: Default value is set by the 'PurgeHits1S' option in dspam.conf,"
			echo "    if this option is not set, the default value is 15 days."
			echo "    For more info: man dspam_clean"
			echo
			echo "  [--hits1i=no_of_days]"
			echo "    Remove tokens with a single INNOCENT hit."
			echo "    Note: Default value is set by the 'PurgeHits1I' option in dspam.conf,"
			echo "    if this option is not set, the default value is 15 days."
			echo "    For more info: man dspam_clean"
			echo
			echo "  [--without-sql-purge]"
			echo "    Do not use SQL based purging. Only run dspam_clean."
			echo "    Note: Default is off (aka: use SQL based purging)."
			echo
			echo "  [--with-sql-optimization]"
			echo "    Run VACUUM (for PostgreSQL/SQLite) and/or OPTIMIZE (for MySQL)."
			echo "    Note: Default is off (aka: do not use optimizations)."
			echo
			echo "  [--with-sql-autoupdate]"
			echo "    Run SQL based purging with purge day values passed to this script."
			echo "    Note: Default is off (aka: do not attempt to modify SQL instructions)."
			echo
			echo "  [--purgescriptdir=[DIRECTORY]"
			echo "    Space separated list of directories where to search for SQL files"
			echo "    used for the SQL based purging."
			echo
			echo "  [--with-all-drivers]"
			echo "    Process all installed storage drivers (not just the active driver)."
			echo "    Note: Default is true (aka: process all installed drivers)."
			echo
			echo "  [--verbose]"
			echo "    Verbose output while running maintenance script."
			echo
			exit 1
			;;
		*)
			echo "Unrecognized parameter '${foo}'. Use parameter --help for more info."
			exit 1
			;;

	esac
done


#
# Function to run dspam_clean
#
run_dspam_clean() {
	[ "${VERBOSE}" = "true" ] && echo "Running dspam_clean ..."
	local PURGE_ALL="${1}"
	local ADD_PARAMETER=""
	read_dspam_params DefaultProfile
	if [ -n "${PROFILE}" -a -n "${DefaultProfile}" -a "${PROFILE/*.}" != "${DefaultProfile}" ]
	then
		ADD_PARAMETER="--profile=${PROFILE/*.}"
	fi
	if [ -z "${SIGNATURE_AGE}" -o -z "${NEUTRAL_AGE}" -o -z "${UNUSED_AGE}" -o -z "${HAPAXES_AGE}" -o -z "${HITS1S_AGE}" -o -z "${HITS1I_AGE}" ]
	then
		[ "${VERBOSE}" = "true" ] && echo " * with default parameters"
		${DSPAM_BIN_DIR}/dspam_clean ${ADD_PARAMETER}
	else
		if [ "${PURGE_ALL}" = "YES" ]
		then
			[ "${VERBOSE}" = "true" ] && echo "  * with full purging"
			${DSPAM_BIN_DIR}/dspam_clean ${ADD_PARAMETER} -s${SIGNATURE_AGE} -p${NEUTRAL_AGE} -u${UNUSED_AGE},${HAPAXES_AGE},${HITS1S_AGE},${HITS1I_AGE} >/dev/null 2>&1
		else
			[ "${VERBOSE}" = "true" ] && echo "  * with neutral token purging only"
			${DSPAM_BIN_DIR}/dspam_clean ${ADD_PARAMETER} -p${NEUTRAL_AGE} >/dev/null 2>&1
		fi
	fi
	return ${?}
}


#
# Function to check if we have all needed tools
#
check_for_tool() {
	local myrc=0
	[ -z "${1}" ] && return 2
	for foo in ${@}
	do
		if ! which ${foo} >/dev/null 2>&1
		then
			echo "Command ${foo} not found!"
			myrc=1
			break
		fi
	done
	return ${myrc}
}


#
# Function to read dspam.conf parameters
#
read_dspam_params() {
	local PARAMETER VALUE
	for PARAMETER in $@ ; do
		VALUE=$(awk "BEGIN { IGNORECASE=1; } \$1==\"${PARAMETER}\" { print \$2; exit; }" "${DSPAM_CONFIGDIR}/dspam.conf" ${INCLUDE_DIRS[@]} 2>/dev/null)
		[ ${?} = 0 ] || return 1
		eval ${PARAMETER/.*}=\'${VALUE}\'
	done
	return 0
}


#
# Function to clean DSPAM MySQL data
#
clean_mysql_drv() {
	#
	# MySQL
	#
	[ "${VERBOSE}" = "true" ] && echo "Running MySQL storage driver data cleanup"
	if [ "${USE_SQL_PURGE}" = "true" ] && \
		read_dspam_params MySQLServer${PROFILE} MySQLPort${PROFILE} MySQLUser${PROFILE} MySQLPass${PROFILE} MySQLDb${PROFILE} MySQLCompress${PROFILE} && \
		[ -n "${MySQLServer}" -a -n "${MySQLUser}" -a -n "${MySQLDb}" ]
	then
		for foo in ${MYSQL_BIN_DIR} /usr/bin /usr/local/bin /usr/sbin /usr/local/sbin
		do
			if [ -e "${foo}/mysql_config" -o -e "${foo}/mysql" ]
			then
				MYSQL_BIN_DIR=${foo}
				break
			fi
		done
		if [ -e "${MYSQL_BIN_DIR}/mysql_config" ]
		then
			DSPAM_MySQL_VER=$(${MYSQL_BIN_DIR}/mysql_config --version | sed -n 's:^[^0-9]*\([0-9.]*\).*:\1:p')
		elif [ -e "${MYSQL_BIN_DIR}/mysql" ]
		then
			DSPAM_MySQL_VER=$(${MYSQL_BIN_DIR}/mysql --version | sed -n 's:^.*[\t ]Distrib[^0-9]*\([0-9.]*\).*:\1:p')
		fi

		if [ -z "${DSPAM_MySQL_VER}" ]
		then
			[ "${VERBOSE}" = "true" ] && echo "  Can not run MySQL purge script:"
			[ "${VERBOSE}" = "true" ] && echo "    ${MYSQL_BIN_DIR}/mysql_config or ${MYSQL_BIN_DIR}/mysql does not exist"
			return 1
		fi

		DSPAM_MySQL_PURGE_SQL=
		DSPAM_MySQL_PURGE_SQL_FILES=
		DSPAM_MySQL_MAJOR=$(echo "${DSPAM_MySQL_VER}" | cut -d. -f1)
		DSPAM_MySQL_MINOR=$(echo "${DSPAM_MySQL_VER}" | cut -d. -f2)
		DSPAM_MySQL_MICRO=$(echo "${DSPAM_MySQL_VER}" | cut -d. -f3)
		DSPAM_MySQL_INT=$(($DSPAM_MySQL_MAJOR * 65536 + $DSPAM_MySQL_MINOR * 256 + $DSPAM_MySQL_MICRO))

		if [ "${DSPAM_MySQL_INT}" -ge "262400" ]
		then
			# For MySQL >= 4.1 use the new purge script
			# For the 4.1-optimized version see:
			# http://securitydot.net/txt/id/32/type/articles/
			# Version >= 3.9.0 of DSPAM do already include a better purge script.
			DSPAM_MySQL_PURGE_SQL_FILES="mysql_purge-4.1 mysql_purge-4.1-optimized"
		else
			DSPAM_MySQL_PURGE_SQL_FILES="mysql_purge"

		fi

		#
		# We first search for the purge scripts in the directory the user has
		# told us to look for (command line option: --purgescriptdir
		# Then we look in DSPAM configuration directory under ./config/ and then
		# in the DSPAM configuration directory it self.
		#
		for foo in ${DSPAM_PURGE_SCRIPT_DIR} ${DSPAM_CONFIGDIR}/config ${DSPAM_CONFIGDIR} /usr/share/doc/libdspam7-drv-mysql/sql /usr/share/dspam/sql /usr/local/share/examples/dspam/mysql /usr/share/examples/dspam/mysql
		do
			for bar in ${DSPAM_MySQL_PURGE_SQL_FILES}
			do
				if [ -z "${DSPAM_MySQL_PURGE_SQL}" -a -f "${foo}/${bar}.sql" ]
				then
					DSPAM_MySQL_PURGE_SQL="${foo}/${bar}.sql"
					if (grep -iq "to_days" "${DSPAM_MySQL_PURGE_SQL}")
					then
						break
					else
						DSPAM_MySQL_PURGE_SQL=""
					fi
				fi
				if [ -z "${DSPAM_MySQL_PURGE_SQL}" -a -f "${foo}/${bar/_//}.sql" ]
				then
					DSPAM_MySQL_PURGE_SQL="${foo}/${bar/_//}.sql"
					if (grep -iq "to_days" "${DSPAM_MySQL_PURGE_SQL}")
					then
						break
					else
						DSPAM_MySQL_PURGE_SQL=""
					fi
				fi
				if [ -z "${DSPAM_MySQL_PURGE_SQL}" -a -f "${foo}/${bar/*_/}.sql" ]
				then
					DSPAM_MySQL_PURGE_SQL="${foo}/${bar/*_/}.sql"
					if (grep -iq "to_days" "${DSPAM_MySQL_PURGE_SQL}")
					then
						break
					else
						DSPAM_MySQL_PURGE_SQL=""
					fi
				fi
			done
			if [ -n "${DSPAM_MySQL_PURGE_SQL}" ] && (grep -iq "to_days" "${DSPAM_MySQL_PURGE_SQL}")
			then
				break
			else
				DSPAM_MySQL_PURGE_SQL=""
			fi
		done

		if [ -z "${DSPAM_MySQL_PURGE_SQL}" ]
		then
			[ "${VERBOSE}" = "true" ] && echo "  Can not run MySQL purge script:"
			[ "${VERBOSE}" = "true" ] && echo "    None of the ${DSPAM_MySQL_PURGE_SQL_FILES} SQL script(s) found"
			return 1
		fi

		if [ ! -r "${DSPAM_MySQL_PURGE_SQL}" ]
		then
			[ "${VERBOSE}" = "true" ] && echo "  Can not read MySQL purge script:"
			[ "${VERBOSE}" = "true" ] && echo "    ${DSPAM_MySQL_PURGE_SQL}"
			return 1
		fi

		if [ ! -e "${MYSQL_BIN_DIR}/mysql" ]
		then
			[ "${VERBOSE}" = "true" ] && echo "  Can not run MySQL purge script:"
			[ "${VERBOSE}" = "true" ] && echo "    ${MYSQL_BIN_DIR}/mysql does not exist"
			return 1
		fi

		# Construct mysql command line
		echo "[client]">"${DSPAM_CRON_TMPFILE}"
		MySQLQuotePass=""
		if [ "${MySQLPass}" != "${MySQLPass/[#\\\"\$]/}" ]
		then
			if [ "${DSPAM_MySQL_INT}" -lt "262160" ]
			then
				if [ "${VERBOSE}" = "true" ]
				then
					echo "  You will most likely have an authentication issue/failure with the"
					echo "  currently used MySQL DSPAM password and your current MySQL version."
				fi
			else
				MySQLQuotePass="'"
			fi
		fi
		echo -n "password=${MySQLQuotePass}">>"${DSPAM_CRON_TMPFILE}"
		awk "BEGIN { IGNORECASE=1; ORS=\"\"; } \$1==\"MySQLPass${PROFILE}\" { gsub(\"^\\\"|\\\"$\", \"\", \$2); print \$2 >>\"${DSPAM_CRON_TMPFILE}\"; exit; }" "${DSPAM_CONFIGDIR}/dspam.conf" ${INCLUDE_DIRS[@]} 2>/dev/null
		echo "${MySQLQuotePass}">>"${DSPAM_CRON_TMPFILE}"
		DSPAM_MySQL_CMD="${MYSQL_BIN_DIR}/mysql"
		DSPAM_MySQL_CMD="${DSPAM_MySQL_CMD} --defaults-file=${DSPAM_CRON_TMPFILE}"
		DSPAM_MySQL_CMD="${DSPAM_MySQL_CMD} --silent"
		DSPAM_MySQL_CMD="${DSPAM_MySQL_CMD} --user=${MySQLUser}"
		[ -S "${MySQLServer}" ] &&
			DSPAM_MySQL_CMD="${DSPAM_MySQL_CMD} --socket=${MySQLServer}" ||
			DSPAM_MySQL_CMD="${DSPAM_MySQL_CMD} --host=${MySQLServer}"
		[ -n "${MySQLPort}" ] &&
			DSPAM_MySQL_CMD="${DSPAM_MySQL_CMD} --port=${MySQLPort}"
		[ "${MySQLCompress}" = "true" ] &&
			DSPAM_MySQL_CMD="${DSPAM_MySQL_CMD} --compress"

		# Use updated purge script to reflect purge days passed to this script
		if [ "${USE_SQL_AUTOUPDATE}" = "true" ]
		then
			sed \
				-e "s:^\(SET[\t ]\{1,\}\@PurgeSignatures[\t ]*=[\t ]*\)[0-9]\{1,\}\(.*\)$:\1${SIGNATURE_AGE}\2:g" \
				-e "s:^\(SET[\t ]\{1,\}\@PurgeUnused[\t ]*=[\t ]*\)[0-9]\{1,\}\(.*\)$:\1${UNUSED_AGE}\2:g" \
				-e "s:^\(SET[\t ]\{1,\}\@PurgeHapaxes[\t ]*=[\t ]*\)[0-9]\{1,\}\(.*\)$:\1${HAPAXES_AGE}\2:g" \
				-e "s:^\(SET[\t ]\{1,\}\@PurgeHits1S[\t ]*=[\t ]*\)[0-9]\{1,\}\(.*\)$:\1${HITS1S_AGE}\2:g" \
				-e "s:^\(SET[\t ]\{1,\}\@PurgeHits1I[\t ]*=[\t ]*\)[0-9]\{1,\}\(.*\)$:\1${HITS1I_AGE}\2:g" \
				"${DSPAM_MySQL_PURGE_SQL}">"${DSPAM_SQL_TMPFILE}"
			[ -r "${DSPAM_SQL_TMPFILE}" ] && DSPAM_MySQL_PURGE_SQL="${DSPAM_SQL_TMPFILE}"
		fi

		# Run the MySQL purge script
		${DSPAM_MySQL_CMD} ${MySQLDb} < ${DSPAM_MySQL_PURGE_SQL} >/dev/null
		_RC=${?}
		if [ ${_RC} != 0 ]
		then
			echo "MySQL purge script returned error code ${_RC}"
		elif [ "${USE_SQL_OPTIMIZATION}" = "true" ]
		then
			for foo in dspam_preferences dspam_signature_data dspam_stats dspam_token_data dspam_virtual_uids
			do
				${DSPAM_MySQL_CMD} --batch -e "OPTIMIZE TABLE ${foo};" ${MySQLDb} >/dev/null 2>&1
				# ${DSPAM_MySQL_CMD} --batch -e "ANALYZE TABLE ${foo};" ${MySQLDb} >/dev/null 2>&1
			done
		fi
		echo "">"${DSPAM_CRON_TMPFILE}"
		echo "">"${DSPAM_SQL_TMPFILE}"

		return 0
	fi
}


#
# Function to clean DSPAM PostgreSQL data
#
clean_pgsql_drv() {
	#
	# PostgreSQL
	#
	[ "${VERBOSE}" = "true" ] && echo "Running PostgreSQL storage driver data cleanup"
	if [ "${USE_SQL_PURGE}" = "true" ] && \
		read_dspam_params PgSQLServer${PROFILE} PgSQLPort${PROFILE} PgSQLUser${PROFILE} PgSQLDb${PROFILE} && \
		[ -n "${PgSQLServer}" -a -n "${PgSQLUser}" -a -n "${PgSQLDb}" ]
	then
		for foo in ${PGSQL_BIN_DIR} /usr/bin /usr/local/bin /usr/sbin /usr/local/sbin
		do
			if [ -e "${foo}/psql" ]
			then
				PGSQL_BIN_DIR=${foo}
				break
			fi
		done

		DSPAM_PgSQL_PURGE_SQL=
		DSPAM_PgSQL_PURGE_SQL_FILES="pgsql_pe-purge pgsql_purge-pe pgsql_purge"

		#
		# We first search for the purge scripts in the directory the user has
		# told us to look for (command line option: --purgescriptdir
		# Then we look in DSPAM configuration directory under ./config/ and then
		# in the DSPAM configuration directory it self.
		#
		for foo in ${DSPAM_PURGE_SCRIPT_DIR} ${DSPAM_CONFIGDIR}/config ${DSPAM_CONFIGDIR} /usr/share/doc/libdspam7-drv-pgsql/sql /usr/share/dspam/sql /usr/local/share/examples/dspam/pgsql /usr/share/examples/dspam/pgsql
		do
			for bar in ${DSPAM_PgSQL_PURGE_SQL_FILES}
			do
				if [ -z "${DSPAM_PgSQL_PURGE_SQL}" -a -f "${foo}/${bar}.sql" ]
				then
					DSPAM_PgSQL_PURGE_SQL="${foo}/${bar}.sql"
					if (grep -iq "VACUUM ANALYSE" "${DSPAM_PgSQL_PURGE_SQL}")
					then
						break
					else
						DSPAM_PgSQL_PURGE_SQL=""
					fi
				fi
				if [ -z "${DSPAM_PgSQL_PURGE_SQL}" -a -f "${foo}/${bar/_//}.sql" ]
				then
					DSPAM_PgSQL_PURGE_SQL="${foo}/${bar/_//}.sql"
					if (grep -iq "VACUUM ANALYSE" "${DSPAM_PgSQL_PURGE_SQL}")
					then
						break
					else
						DSPAM_PgSQL_PURGE_SQL=""
					fi
				fi
				if [ -z "${DSPAM_PgSQL_PURGE_SQL}" -a -f "${foo}/${bar/*_/}.sql" ]
				then
					DSPAM_PgSQL_PURGE_SQL="${foo}/${bar/*_/}.sql"
					if (grep -iq "VACUUM ANALYSE" "${DSPAM_PgSQL_PURGE_SQL}")
					then
						break
					else
						DSPAM_PgSQL_PURGE_SQL=""
					fi
				fi
			done
			if [ -n "${DSPAM_PgSQL_PURGE_SQL}" ] && (grep -q "VACUUM ANALYSE" "${DSPAM_PgSQL_PURGE_SQL}")
			then
				break
			else
				DSPAM_PgSQL_PURGE_SQL=""
			fi
		done

		if [ -z "${DSPAM_PgSQL_PURGE_SQL}" ]
		then
			[ "${VERBOSE}" = "true" ] && echo "  Can not run PostgreSQL purge script:"
			[ "${VERBOSE}" = "true" ] && echo "    None of the ${DSPAM_PgSQL_PURGE_SQL_FILES} SQL script(s) found"
			return 1
		fi

		if [ ! -r "${DSPAM_PgSQL_PURGE_SQL}" ]
		then
			[ "${VERBOSE}" = "true" ] && echo "  Can not read PostgreSQL purge script:"
			[ "${VERBOSE}" = "true" ] && echo "    ${DSPAM_PgSQL_PURGE_SQL}"
			return 1
		fi

		if [ ! -e "${PGSQL_BIN_DIR}/psql" ]
		then
			[ "${VERBOSE}" = "true" ] && echo "  Can not run PostgreSQL purge script:"
			[ "${VERBOSE}" = "true" ] && echo "    ${PGSQL_BIN_DIR}/psql does not exist"
			return 1
		fi

		# Construct psql command line
		echo -n "*:*:${PgSQLDb}:${PgSQLUser}:">"${DSPAM_CRON_TMPFILE}"
		awk "BEGIN { IGNORECASE=1; } \$1==\"PgSQLPass${PROFILE}\" { gsub(\"^\\\"|\\\"$\", \"\", \$2); print \$2 >>\"${DSPAM_CRON_TMPFILE}\"; exit; }" "${DSPAM_CONFIGDIR}/dspam.conf" ${INCLUDE_DIRS[@]} 2>/dev/null
		DSPAM_PgSQL_CMD="${PGSQL_BIN_DIR}/psql -q -U ${PgSQLUser} -h ${PgSQLServer} -d ${PgSQLDb}"
		[ -n "${PgSQLPort}" ] &&
			DSPAM_PgSQL_CMD="${DSPAM_PgSQL_CMD} -p ${PgSQLPort}"

		# Use updated purge script to reflect purge days passed to this script
		if [ "${USE_SQL_AUTOUPDATE}" = "true" ]
		then
			if [ "${HITS1S_AGE}" -gt "${HITS1I_AGE}" ]
			then
				HITS1MAX=${HITS1S_AGE}
			else
				HITS1MAX=${HITS1I_AGE}
			fi
			sed \
				-e "/^DELETE FROM dspam_signature_data/,/COMMIT/{s:^\([\t ]*WHERE[\t ]\{1,\}created_on[\t ]\{1,\}<[\t ]\{1,\}CURRENT_DATE[\t ]\{1,\}\-[\t ]\{1,\}\)14\(.*\)$:\1${SIGNATURE_AGE}\2:g}" \
				-e "/^DELETE FROM dspam_token_data/,/COMMIT/{s:^\([\t ]*AND[\t ]\{1,\}last_hit[\t ]\{1,\}<[\t ]\{1,\}CURRENT_DATE[\t ]\{1,\}\-[\t ]\{1,\}\)[36]0\(.*\)$:\1${HAPAXES_AGE}\2:g}" \
				-e "/^DELETE FROM dspam_token_data/,/COMMIT/{s:^\([\t ]*AND[\t ]\{1,\}last_hit[\t ]\{1,\}<[\t ]\{1,\}CURRENT_DATE[\t ]\{1,\}\-[\t ]\{1,\}\)15\(.*\)$:\1${HITS1MAX}\2:g}" \
				-e "/^DELETE FROM dspam_token_data/,/COMMIT/{s:^\([\t ]*\(AND\|WHERE\)[\t ]\{1,\}last_hit[\t ]\{1,\}<[\t ]\{1,\}CURRENT_DATE[\t ]\{1,\}\-[\t ]\{1,\}\)90\(.*\)$:\1${UNUSED_AGE}\3:g}" \
				"${DSPAM_PgSQL_PURGE_SQL}">"${DSPAM_SQL_TMPFILE}"
			[ -r "${DSPAM_SQL_TMPFILE}" ] && DSPAM_PgSQL_PURGE_SQL="${DSPAM_SQL_TMPFILE}"
		fi

		# Run the PostgreSQL purge script
		PGPASSFILE="${DSPAM_CRON_TMPFILE}" ${DSPAM_PgSQL_CMD} -f "${DSPAM_PgSQL_PURGE_SQL}" >/dev/null
		_RC=${?}
		if [ ${_RC} != 0 ]
		then
			[ "${VERBOSE}" = "true" ] && echo "PostgreSQL purge script returned error code ${_RC}"
		fi
		echo "">"${DSPAM_CRON_TMPFILE}"
		echo "">"${DSPAM_SQL_TMPFILE}"

		return 0
	fi
}


#
# Function to clean DSPAM Hash data
#
clean_hash_drv() {
	#
	# Hash
	#
	local myrc=0
	[ "${VERBOSE}" = "true" ] && echo "Running Hash storage driver data cleanup"
	if [ -d "${DSPAM_HOMEDIR}/data" ]
	then
		if [ -e ${DSPAM_BIN_DIR}/cssclean ]
		then
			if ! check_for_tool find
			then
				[ "${VERBOSE}" = "true" ] && echo "  Can not run cleanup without 'find' binary"
				myrc=1
			else
				find ${DSPAM_HOMEDIR}/data/ -maxdepth 4 -mindepth 1 -type f -name "*.css" | while read name
				do
					${DSPAM_BIN_DIR}/cssclean "${name}" 1>/dev/null 2>&1
				done
			fi
		else
			[ "${VERBOSE}" = "true" ] && echo "  DSPAM cssclean binary not found!"
		fi
		if ! check_for_tool find
		then
			[ "${VERBOSE}" = "true" ] && echo "  Can not purge old signatures without 'find' binary"
			myrc=1
		else
			find ${DSPAM_HOMEDIR}/data/ -maxdepth 4 -mindepth 1 -type d -name "*.sig" | while read name
			do
				find "${name}" -maxdepth 1 -mindepth 1 -type f -mtime +${SIGNATURE_AGE} -name "*.sig" -exec rm "{}" ";"
			done
		fi
	else
		myrc=1
	fi
	return ${myrc}
}


#
# Function to clean DSPAM SQLite3 data
#
clean_sqlite3_drv() {
	#
	# SQLite3
	#
	[ "${VERBOSE}" = "true" ] && echo "Running SQLite v3 storage driver data cleanup"
	if [ "${USE_SQL_PURGE}" = "true" ]
	then
		for foo in ${SQLITE3_BIN_DIR} /usr/bin /usr/local/bin /usr/sbin /usr/local/sbin
		do
			if [ -e "${foo}/sqlite3" ]
			then
				SQLITE3_BIN_DIR=${foo}
				break
			fi
		done

		DSPAM_SQLite3_PURGE_SQL=""
		DSPAM_SQLite3_PURGE_SQL_FILES="sqlite3_purge sqlite3_purge-3"

		#
		# We first search for the purge scripts in the directory the user has
		# told us to look for (command line option: --purgescriptdir
		# Then we look in DSPAM configuration directory under ./config/ and then
		# in the DSPAM configuration directory it self.
		#
		for foo in ${DSPAM_PURGE_SCRIPT_DIR} ${DSPAM_CONFIGDIR}/config ${DSPAM_CONFIGDIR} /usr/share/doc/libdspam7-drv-sqlite3/sql /usr/share/dspam/sql /usr/share/dspam/clean /usr/local/share/examples/dspam/sqlite /usr/share/examples/dspam/sqlite
		do
			for bar in ${DSPAM_SQLite3_PURGE_SQL_FILES}
			do
				if [ -z "${DSPAM_SQLite3_PURGE_SQL}" -a -f "${foo}/${bar}.sql" ]
				then
					DSPAM_SQLite3_PURGE_SQL="${foo}/${bar}.sql"
					if (grep -iq "julianday.*now" "${DSPAM_SQLite3_PURGE_SQL}")
					then
						break
					else
						DSPAM_SQLite3_PURGE_SQL=""
					fi
				fi
				if [ -z "${DSPAM_SQLite3_PURGE_SQL}" -a  -f "${foo}/${bar/_//}.sql" ]
				then
					DSPAM_SQLite3_PURGE_SQL="${foo}/${bar/_//}.sql"
					if (grep -iq "julianday.*now" "${DSPAM_SQLite3_PURGE_SQL}")
					then
						break
					else
						DSPAM_SQLite3_PURGE_SQL=""
					fi
				fi
				if [ -z "${DSPAM_SQLite3_PURGE_SQL}" -a -f "${foo}/${bar/*_/}.sql" ]
				then
					DSPAM_SQLite3_PURGE_SQL="${foo}/${bar/*_/}.sql"
					if (grep -iq "julianday.*now" "${DSPAM_SQLite3_PURGE_SQL}")
					then
						break
					else
						DSPAM_SQLite3_PURGE_SQL=""
					fi
				fi
			done
			if [ -n "${DSPAM_SQLite3_PURGE_SQL}" ] && (grep -iq "julianday.*now" "${DSPAM_SQLite3_PURGE_SQL}")
			then
				break
			else
				DSPAM_SQLite3_PURGE_SQL=""
			fi
		done

		if [ -z "${DSPAM_SQLite3_PURGE_SQL}" ]
		then
			[ "${VERBOSE}" = "true" ] && echo "  Can not run SQLite3 purge script:"
			[ "${VERBOSE}" = "true" ] && echo "    None of the ${DSPAM_SQLite3_PURGE_SQL_FILES} SQL script(s) found"
			return 1
		fi

		if [ ! -r "${DSPAM_SQLite3_PURGE_SQL}" ]
		then
			[ "${VERBOSE}" = "true" ] && echo "  Can not read SQLite3 purge script:"
			[ "${VERBOSE}" = "true" ] && echo "    ${DSPAM_SQLite3_PURGE_SQL}"
			return 1
		fi

		if [ ! -e "${SQLITE3_BIN_DIR}/sqlite3" ]
		then
			[ "${VERBOSE}" = "true" ] && echo "  Can not run SQLite3 purge script:"
			[ "${VERBOSE}" = "true" ] && echo "    ${SQLITE3_BIN_DIR}/sqlite3 does not exist"
			return 1
		fi

		# Use updated purge script to reflect purge days passed to this script
		if [ "${USE_SQL_AUTOUPDATE}" = "true" ]
		then
			if [ "${HITS1S_AGE}" -gt "${HITS1I_AGE}" ]
			then
				HITS1MAX=${HITS1S_AGE}
			else
				HITS1MAX=${HITS1I_AGE}
			fi
			sed \
				-e "s:^\([\t ]*and[\t ]\{1,\}(julianday('now')\-\)[36]0\()[\t ]\{1,\}>[\t ]\{1,\}julianday(last_hit).*\)$:\1${HAPAXES_AGE}\2:g" \
				-e "s:^\([\t ]*and[\t ]\{1,\}(julianday('now')\-\)15\()[\t ]\{1,\}>[\t ]\{1,\}julianday(last_hit).*\)$:\1${HITS1MAX}\2:g" \
				-e "s:^\([\t ]*and[\t ]\{1,\}(julianday('now')\-\)90\()[\t ]\{1,\}>[\t ]\{1,\}julianday(last_hit).*\)$:\1${UNUSED_AGE}\2:g" \
				-e "s:^\([\t ]*and[\t ]\{1,\}(julianday('now')\-\)14\()[\t ]\{1,\}>[\t ]\{1,\}julianday(created_on).*\)$:\1${SIGNATURE_AGE}\2:g" \
				"${DSPAM_SQLite3_PURGE_SQL}">"${DSPAM_SQL_TMPFILE}"
			[ -r "${DSPAM_SQL_TMPFILE}" ] && DSPAM_SQLite3_PURGE_SQL="${DSPAM_SQL_TMPFILE}"
		fi

		# Run the SQLite3 purge script and vacuum
		find "${DSPAM_HOMEDIR}" -name "*.sdb" -print | while read name
		do
			${SQLITE3_BIN_DIR}/sqlite3 "${name}" < "${DSPAM_SQLite3_PURGE_SQL}" >/dev/null
			if [ "${USE_SQL_OPTIMIZATION}" = "true" ]
			then
				# Enable the next line if you don't vacuum in the purge script
				echo "vacuum;" | ${SQLITE3_BIN_DIR}/sqlite3 "${name}" >/dev/null
			fi
		done 1>/dev/null 2>&1

		echo "">"${DSPAM_SQL_TMPFILE}"

		return 0
	fi

}


#
# Function to clean DSPAM SQLite < 3.0 data
#
clean_sqlite_drv() {
	#
	# SQLite
	#
	[ "${VERBOSE}" = "true" ] && echo "Running SQLite v2 storage driver data cleanup"
	if [ "${USE_SQL_PURGE}" = "true" ]
	then
		for foo in ${SQLITE_BIN_DIR} /usr/bin /usr/local/bin /usr/sbin /usr/local/sbin
		do
			if [ -e "${foo}/sqlite" ]
			then
				SQLITE_BIN_DIR=${foo}
				break
			fi
		done

		DSPAM_SQLite_PURGE_SQL=""
		DSPAM_SQLite_PURGE_SQL_FILES="sqlite2_purge sqlite2_purge-2"

		#
		# We first search for the purge scripts in the directory the user has
		# told us to look for (command line option: --purgescriptdir
		# Then we look in DSPAM configuration directory under ./config/ and then
		# in the DSPAM configuration directory it self.
		#
		for foo in ${DSPAM_PURGE_SCRIPT_DIR} ${DSPAM_CONFIGDIR}/config ${DSPAM_CONFIGDIR} /usr/share/doc/libdspam7-drv-sqlite/sql /usr/share/dspam/sql /usr/share/dspam/clean /usr/local/share/examples/dspam/sqlite /usr/share/examples/dspam/sqlite
		do
			for bar in ${DSPAM_SQLite_PURGE_SQL_FILES}
			do
				if [ -z "${DSPAM_SQLite_PURGE_SQL}" -a -f "${foo}/${bar}.sql" ]
				then
					DSPAM_SQLite_PURGE_SQL="${foo}/${bar}.sql"
					if (grep -iq "date.*now.*date" "${DSPAM_SQLite_PURGE_SQL}")
					then
						break
					else
						DSPAM_SQLite_PURGE_SQL=""
					fi
				fi
				if [ -z "${DSPAM_SQLite_PURGE_SQL}" -a -f "${foo}/${bar/_//}.sql" ]
				then
					DSPAM_SQLite_PURGE_SQL="${foo}/${bar/_//}.sql"
					if (grep -iq "date.*now.*date" "${DSPAM_SQLite_PURGE_SQL}")
					then
						break
					else
						DSPAM_SQLite_PURGE_SQL=""
					fi
				fi
				if [ -z "${DSPAM_SQLite_PURGE_SQL}" -a -f "${foo}/${bar/*_/}.sql" ]
				then
					DSPAM_SQLite_PURGE_SQL="${foo}/${bar/*_/}.sql"
					if (grep -iq "date.*now.*date" "${DSPAM_SQLite_PURGE_SQL}")
					then
						break
					else
						DSPAM_SQLite_PURGE_SQL=""
					fi
				fi
			done
			if [ -n "${DSPAM_SQLite_PURGE_SQL}" ] && (grep -iq "date.*now.*date" "${DSPAM_SQLite_PURGE_SQL}")
			then
				break
			else
				DSPAM_SQLite_PURGE_SQL=""
			fi
		done

		if [ -z "${DSPAM_SQLite_PURGE_SQL}" ]
		then
			[ "${VERBOSE}" = "true" ] && echo "  Can not run SQLite purge script:"
			[ "${VERBOSE}" = "true" ] && echo "    None of the ${DSPAM_SQLite_PURGE_SQL_FILES} SQL script(s) found"
			return 1
		fi

		if [ ! -r "${DSPAM_SQLite_PURGE_SQL}" ]
		then
			[ "${VERBOSE}" = "true" ] && echo "  Can not read SQLite purge script:"
			[ "${VERBOSE}" = "true" ] && echo "    ${DSPAM_SQLite_PURGE_SQL}"
			return 1
		fi

		if [ ! -e "${SQLITE_BIN_DIR}/sqlite" ]
		then
			[ "${VERBOSE}" = "true" ] && echo "  Can not run SQLite purge script:"
			[ "${VERBOSE}" = "true" ] && echo "    ${SQLITE_BIN_DIR}/sqlite does not exist"
			return 1
		fi

		# Use updated purge script to reflect purge days passed to this script
		if [ "${USE_SQL_AUTOUPDATE}" = "true" ]
		then
			if [ "${HITS1S_AGE}" -gt "${HITS1I_AGE}" ]
			then
				HITS1MAX=${HITS1S_AGE}
			else
				HITS1MAX=${HITS1I_AGE}
			fi
			sed \
				-e "s:^\([\t ]*and[\t ]\{1,\}date('now')\-date(last_hit)[\t ]\{1,\}>[\t ]\{1,\}\)[36]0\(.*\)$:\1${HAPAXES_AGE}\2:g" \
				-e "s:^\([\t ]*and[\t ]\{1,\}date('now')\-date(last_hit)[\t ]\{1,\}>[\t ]\{1,\}\)15\(.*\)$:\1${HITS1MAX}\2:g" \
				-e "s:^\([\t ]*and[\t ]\{1,\}date('now')\-date(last_hit)[\t ]\{1,\}>[\t ]\{1,\}\)90\(.*\)$:\1${UNUSED_AGE}\2:g" \
				-e "s:^\([\t ]*and[\t ]\{1,\}date('now')\-date(created_on)[\t ]\{1,\}>[\t ]\{1,\}\)14\(.*\)$:\1${SIGNATURE_AGE}\2:g" \
				"${DSPAM_SQLite_PURGE_SQL}">"${DSPAM_SQL_TMPFILE}"
			[ -r "${DSPAM_SQL_TMPFILE}" ] && DSPAM_SQLite_PURGE_SQL="${DSPAM_SQL_TMPFILE}"
		fi

		# Run the SQLite purge script and vacuum
		find "${DSPAM_HOMEDIR}" -name "*.sdb" -print | while read name
		do
			${SQLITE_BIN_DIR}/sqlite "${name}" < "${DSPAM_SQLite_PURGE_SQL}" >/dev/null
			if [ "${USE_SQL_OPTIMIZATION}" = "true" ]
			then
				# Enable the next line if you don't vacuum in the purge script
				echo "vacuum;" | ${SQLITE_BIN_DIR}/sqlite "${name}" >/dev/null
			fi
		done 1>/dev/null 2>&1

		echo "">"${DSPAM_SQL_TMPFILE}"

		return 0
	fi

}


#
# Use a lock file to prevent multiple runs at the same time
#
DSPAM_CRON_LOCKFILE="/var/run/$(basename $0 .sh).pid"


#
# File used to save temporary data
#
DSPAM_CRON_TMPFILE="/tmp/.ds_$$_$RANDOM"
DSPAM_SQL_TMPFILE="/tmp/.ds_$$_$RANDOM"


#
# Kill process if lockfile is older or equal 12 hours
#
if [ -f ${DSPAM_CRON_LOCKFILE} ]; then
	DSPAM_REMOVE_CRON_LOCKFILE="YES"
	for foo in $(cat ${DSPAM_CRON_LOCKFILE} 2>/dev/null); do
		if [ -L "/proc/${foo}/exe" -a "$(readlink -f /proc/${foo}/exe)" = "$(readlink -f /proc/$$/exe)" ]; then
			DSPAM_REMOVE_CRON_LOCKFILE="NO"
		fi
	done
	if [ "${DSPAM_REMOVE_CRON_LOCKFILE}" = "YES" ]; then
		rm -f ${DSPAM_CRON_LOCKFILE} >/dev/null 2>&1
	elif [ $(($(date +%s)-$(stat --printf="%X" ${DSPAM_CRON_LOCKFILE}))) -ge $((12*60*60)) ]; then
		for foo in $(cat ${DSPAM_CRON_LOCKFILE} 2>/dev/null); do
			if [ -L "/proc/${foo}/exe" -a "$(readlink -f /proc/${foo}/exe)" = "$(readlink -f /proc/$$/exe)" ]; then
				kill -s KILL ${foo} >/dev/null 2>&1
			fi
		done
		DSPAM_REMOVE_CRON_LOCKFILE="YES"
		for foo in $(cat ${DSPAM_CRON_LOCKFILE} 2>/dev/null); do
			if [ -L "/proc/${foo}/exe" -a "$(readlink -f /proc/${foo}/exe)" = "$(readlink -f /proc/$$/exe)" ]; then
				DSPAM_REMOVE_CRON_LOCKFILE="NO"
			fi
		done
		if [ "${DSPAM_REMOVE_CRON_LOCKFILE}" = "YES" ]; then
			rm -f ${DSPAM_CRON_LOCKFILE} >/dev/null 2>&1
		fi
	fi
fi


#
# Acquire lock file and start processing
#
if ( set -o noclobber; echo "$$" > "${DSPAM_CRON_LOCKFILE}") 2> /dev/null; then

	trap 'rm -f "${DSPAM_CRON_LOCKFILE}" "${DSPAM_CRON_TMPFILE}"; exit ${?}' INT TERM EXIT


	#
	# Create the temporary file
	#
	UMASK_OLD="$(umask)"
	umask 077
	[ -e "${DSPAM_CRON_TMPFILE}" ] && rm -f "${DSPAM_CRON_TMPFILE}" >/dev/null 2>&1
	[ -e "${DSPAM_SQL_TMPFILE}" ] && rm -f "${DSPAM_SQL_TMPFILE}" >/dev/null 2>&1
	touch "${DSPAM_CRON_TMPFILE}"
	touch "${DSPAM_SQL_TMPFILE}"
	umask "${UMASK_OLD}"


	#
	# Check for needed tools
	#
	if ! check_for_tool awk cut sed sort tr grep
	then
		# We don't have the tools needed to continue.
		[ "${VERBOSE}" = "true" ] && echo "This script needs awk, cut, sed, sort, tr and grep to work."
		exit 2
	fi


	#
	# Try to read most of the configuration options from DSPAM
	#
	DSPAM_CONFIG_PARAMETERS=($(dspam --version 2>&1 | sed -n "s:^Configuration parameters\:[\t ]*\(.*\)$:\1:g;s:' '\-\-:\n--:g;s:^'::g;s:' '[a-zA-Z].*::gp"))
	if [ -z "${DSPAM_CONFIG_PARAMETERS}" -o "${#DSPAM_CONFIG_PARAMETERS[@]}" -lt 3 ]
	then
		DSPAM_CONFIG_PARAMETERS=($(dspam --version 2>&1 | sed -n "s:^Configuration parameters\:[\t ]*\(.*\)$:\1:gp" | tr -s "' '" "'\n'" | sed -n "s:^'\(\-\-.*\)'[\t ]*$:\1:gp"))
	fi
	if [ -z "${DSPAM_CONFIG_PARAMETERS}" -o "${#DSPAM_CONFIG_PARAMETERS[@]}" -lt 3 ]
	then
		# Not good! dspam --version does not print out configuration parameters.
		# Try getting the information by parsing the strings in the DSPAM binary.
		if check_for_tool strings
		then
			DSPAM_CONFIG_PARAMETERS=($(strings $(whereis dspam | awk '{print $2}') 2>&1 | sed -n "/'\-\-[^']*'[\t ]*'\-\-[^']*'/p" 2>/dev/null | sed -n "s:' '\-\-:\n--:g;s:^[\t ]*'::g;s:' '[a-zA-Z].*::gp"))
			if [ -z "${DSPAM_CONFIG_PARAMETERS}" -o "${#DSPAM_CONFIG_PARAMETERS[@]}" -lt 3 ]
			then
				DSPAM_CONFIG_PARAMETERS=($(strings $(whereis dspam | awk '{print $2}') 2>&1 | sed -n "/'\-\-[^']*'[\t ]*'\-\-[^']*'/p" 2>/dev/null | tr -s "' '" "'\n'" | sed -n "s:^'\(\-\-.*\)'[\t ]*$:\1:gp"))
			fi
		fi
	fi
	if [ -n "${DSPAM_CONFIG_PARAMETERS}" ]
	then
		for foo in ${DSPAM_CONFIG_PARAMETERS[@]}
		do
			case "${foo}" in
				--sysconfdir=*)
					if [ -z "${DSPAM_CONFIGDIR}" -o ! -d "${DSPAM_CONFIGDIR}" ]
					then
						if [ -f "${foo#--sysconfdir=}/dspam.conf" ]
						then
							DSPAM_CONFIGDIR="${foo#--sysconfdir=}"
						fi
					fi
					;;
				--with-dspam-home=*)
					if [ -z "${DSPAM_HOMEDIR}" -o ! -d "${DSPAM_HOMEDIR}" ]
					then
						if [ -d "${foo#--with-dspam-home=}/data" ]
						then
							DSPAM_HOMEDIR="${foo#--with-dspam-home=}"
						fi
					fi
					;;
				--prefix=*)
					if [ -z "${DSPAM_BIN_DIR}" -o ! -d "${DSPAM_BIN_DIR}" -o ! -e "${DSPAM_BIN_DIR}/dspam" ]
					then
						if [ -e "${foo#--prefix=}/bin/dspam" ]
						then
							DSPAM_BIN_DIR="${foo#--prefix=}"/bin
						fi
					fi
					;;
				--with-storage-driver=*)
					STORAGE_DRIVERS="${foo#--with-storage-driver=}"
					#STORAGE_DRIVERS=($(echo ${STORAGE_DRIVERS} | sed "s:,:\n:g" | sort -u))
					STORAGE_DRIVERS=($(echo ${STORAGE_DRIVERS} | tr -s "," "\n" | sort -u))
					;;
			esac
		done
		[ "${VERBOSE}" = "true" -a -n "${STORAGE_DRIVERS}" ] && echo "Enabled drivers are: ${STORAGE_DRIVERS[@]}"
	else
		[ "${VERBOSE}" = "true" ] && echo "Warning: dspam --version does not print configuration parameters!"
	fi


	#
	# Try to get DSPAM bin directory
	#
	if [ -z "${DSPAM_BIN_DIR}" ]
	then
		for foo in /usr/bin /usr/local/bin /usr/sbin /usr/local/sbin /opt/dspam /opt/dspam/bin
		do
			if [ -e "${foo}/dspam" -a -e "${foo}/dspam_clean" ]
			then
				DSPAM_BIN_DIR=${foo}
				break
			fi
		done
		if [ -n "${DSPAM_BIN_DIR}" -a ! -e "${DSPAM_BIN_DIR}/dspam" ]
		then
			[ "${VERBOSE}" = "true" ] && echo "DSPAM binary in directory ${DSPAM_BIN_DIR} not found!"
			exit 2
		elif [ -n "${DSPAM_BIN_DIR}" -a ! -e "${DSPAM_BIN_DIR}/dspam_clean" ]
		then
			[ "${VERBOSE}" = "true" ] && echo "DSPAM clean binary in directory ${DSPAM_BIN_DIR} not found!"
			exit 2
		else
			[ "${VERBOSE}" = "true" ] && echo "DSPAM binary directory not found!"
			exit 2
		fi
	else
		for foo in ${DSPAM_BIN_DIR}
		do
			if [ -d "${foo}" -a -e "${foo}/dspam" ]
			then
				DSPAM_BIN_DIR=${foo}
				break
			fi
		done
	fi
	if [ -z "${DSPAM_BIN_DIR}" -o ! -e "${DSPAM_BIN_DIR}/dspam" -o ! -e "${DSPAM_BIN_DIR}/dspam_clean" ]
	then
		[ "${VERBOSE}" = "true" ] && echo "Binary for dspam and/or dspam_clean not found! Can not continue without it."
		exit 2
	fi


	#
	# Try to get DSPAM config directory
	#
	if [ -z "${DSPAM_CONFIGDIR}" ]
	then
		for foo in /etc/mail/dspam /etc/dspam /etc /usr/local/etc/mail/dspam /usr/local/etc/dspam /usr/local/etc
		do
			if [ -f "${foo}/dspam.conf" ]
			then
				DSPAM_CONFIGDIR=${foo}
				break
			fi
		done
	fi
	if [ -z "${DSPAM_CONFIGDIR}" -o ! -f "${DSPAM_CONFIGDIR}/dspam.conf" ]
	then
		[ "${VERBOSE}" = "true" ] && echo "dspam.conf not found! Can not continue without it."
		exit 2
	fi
	INCLUDE_DIRS=$(awk "BEGIN { IGNORECASE=1; } \$1==\"Include\" { print \$2 \"/*.conf\"; }" "${DSPAM_CONFIGDIR}/dspam.conf" 2>/dev/null)

	#
	# Parameters
	#
	if [ -z "${PROFILE}" ]
	then
		if read_dspam_params DefaultProfile && [ -n "${DefaultProfile}" ]
		then
			PROFILE=.${DefaultProfile}
		fi
	else
		PROFILE=.${PROFILE}
	fi
	[ -z "${LOGROTATE_AGE}" ] && LOGROTATE_AGE=31				# System and user log
	[ -z "${USE_SQL_PURGE}" ] && USE_SQL_PURGE="true"			# Run SQL purge scripts
	[ -z "${USE_SQL_OPTIMIZATION}" ] && USE_SQL_OPTIMIZATION="false"	# Do not run vacuum or optimize
	[ -z "${USE_SQL_AUTOUPDATE}" ] && USE_SQL_AUTOUPDATE="false"		# Do not automatically modify purge days in SQL clause
	[ -z "${PURGE_ALL_DRIVERS}" ] && PURGE_ALL_DRIVERS="false"		# Only purge active driver
	[ -z "${VERBOSE}" ] && VERBOSE="false"					# No additional output
	if [ -z "${SIGNATURE_AGE}" ]
	then
		if read_dspam_params PurgeSignatures && [ -n "${PurgeSignatures}" -a "${PurgeSignatures}" != "off" ]
		then
			SIGNATURE_AGE=${PurgeSignatures}
		else
			SIGNATURE_AGE=14		# Stale signatures
		fi
	fi
	if [ -z "${NEUTRAL_AGE}" ]
	then
		if read_dspam_params PurgeNeutral && [ -n "${PurgeNeutral}" -a "${PurgeNeutral}" != "off" ]
		then
			NEUTRAL_AGE=${PurgeNeutral}
		else
			NEUTRAL_AGE=90			# Tokens with neutralish probabilities
		fi
	fi
	if [ -z "${UNUSED_AGE}" ]
	then
		if read_dspam_params PurgeUnused && [ -n "${PurgeUnused}" -a "${PurgeUnused}" != "off" ]
		then
			UNUSED_AGE=${PurgeUnused}
		else
			UNUSED_AGE=90			# Unused tokens
		fi
	fi
	if [ -z "${HAPAXES_AGE}" ]
	then
		if read_dspam_params PurgeHapaxes && [ -n "${PurgeHapaxes}" -a "${PurgeHapaxes}" != "off" ]
		then
			HAPAXES_AGE=${PurgeHapaxes}
		else
			HAPAXES_AGE=30			# Tokens with less than 5 hits (hapaxes)
		fi
	fi
	if [ -z "${HITS1S_AGE}" ]
	then
		if read_dspam_params PurgeHits1S && [ -n "${PurgeHits1S}" -a "${PurgeHits1S}" != "off" ]
		then
			HITS1S_AGE=${PurgeHits1S}
		else
			HITS1S_AGE=15			# Tokens with only 1 spam hit
		fi
	fi
	if [ -z "${HITS1I_AGE}" ]
	then
		if read_dspam_params PurgeHits1I && [ -n "${PurgeHits1I}" -a "${PurgeHits1I}" != "off" ]
		then
			HITS1I_AGE=${PurgeHits1I}
		else
			HITS1I_AGE=15			# Tokens with only 1 innocent hit
		fi
	fi


	#
	# Try to get DSPAM data home directory
	#
	if read_dspam_params Home && [ -d "${Home}" ]
	then
		DSPAM_HOMEDIR=${Home}
	else
		# Something is wrong in dspam.conf! Check if /var/spool/dspam exists instead.
		if [ -z "${DSPAM_HOMEDIR}" -a -d /var/spool/dspam ]
		then
			DSPAM_HOMEDIR="/var/spool/dspam"
		fi
	fi
	if [ ! -d "${DSPAM_HOMEDIR}" -o -z "${DSPAM_HOMEDIR}" ]
	then
		[ "${VERBOSE}" = "true" ] && echo "Home directory not found! Please fix your dspam.conf."
		exit 2
	fi


	#
	# System and user log purging
	#
	if [ ! -e "${DSPAM_BIN_DIR}/dspam_logrotate" ]
	then
		[ "${VERBOSE}" = "true" ] && echo "dspam_logrotate not found! Not purging system and user logs."
	else
		[ "${VERBOSE}" = "true" ] && echo "Running dspam_logrotate in the background"
		${DSPAM_BIN_DIR}/dspam_logrotate -a ${LOGROTATE_AGE} -d "${DSPAM_HOMEDIR}" >/dev/null &
	fi


	#
	# Don't purge signatures with dspam_clean if we purged them with SQL
	#
	RUN_FULL_DSPAM_CLEAN="NO"


	#
	# Currently active storage driver
	#
	ACTIVE_DRIVER=""
	if [ ${#STORAGE_DRIVERS[@]} -eq 1 ]
	then
		ACTIVE_DRIVER="${STORAGE_DRIVERS[0]}"
	else
		read_dspam_params StorageDriver
		if [ -n "${StorageDriver}" ]
		then
			for foo in hash_drv mysql_drv pgsql_drv sqlite3_drv sqlite_drv
			do
				if ( echo "${StorageDriver}" | grep -q "${foo}" )
				then
					ACTIVE_DRIVER="${foo}"
					break
				fi
			done
		fi
	fi
	[ "${VERBOSE}" = "true" -a -n "${ACTIVE_DRIVER}" ] && echo "Active driver is: ${ACTIVE_DRIVER}"


	#
	# Which drivers to process/purge
	#
	if [ "${PURGE_ALL_DRIVERS}" = "false" -a -n "${ACTIVE_DRIVER}" ]
	then
		DRIVERS_TO_PROCESS=${ACTIVE_DRIVER}
	elif [ ${#STORAGE_DRIVERS[@]} -gt 0 ]
	then
		DRIVERS_TO_PROCESS=${STORAGE_DRIVERS[@]}
	else
		[ "${VERBOSE}" = "true" ] && echo "Warning: Could not get a list of supported storage drivers!"
		[ "${VERBOSE}" = "true" ] && echo "Warning: Could not determine the currently active storage driver!"
		DRIVERS_TO_PROCESS=""
		RUN_FULL_DSPAM_CLEAN="YES"
	fi


	#
	# Process selected storage drivers
	#
	for foo in ${DRIVERS_TO_PROCESS}
	do
		case "${foo}" in
			hash_drv)
				clean_hash_drv
				;;
			mysql_drv)
				clean_mysql_drv
				;;
			pgsql_drv)
				clean_pgsql_drv
				;;
			sqlite3_drv)
				clean_sqlite3_drv
				;;
			sqlite_drv)
				clean_sqlite_drv
				;;
			*)
				RUN_FULL_DSPAM_CLEAN="YES"
				echo "Unknown storage ${foo} driver"
				;;
		esac
	done


	#
	# Run the dspam_clean command
	#
	# NOTE:	The hash driver does not work with dspam_clean. So under no
	#	circumstances do run dspam_clean if the hash driver is the
	#	ONLY or the currently used driver!
	if [ -n "${ACTIVE_DRIVER}" ]
	then
		if [ "${ACTIVE_DRIVER}" = "hash_drv" ]
		then
			[ "${VERBOSE}" = "true" ] && echo "Hash storage driver detected (not running dspam_clean)"
		else
			[ "${USE_SQL_PURGE}" = "false" ] && RUN_FULL_DSPAM_CLEAN="YES"
			run_dspam_clean ${RUN_FULL_DSPAM_CLEAN}
		fi
	else
		# Storage driver probably statically linked. Not running dspam_clean
		# because of potential risk that the storage driver used is the Hash
		# driver.
		[ "${VERBOSE}" = "true" ] && echo "Could not detect current storage driver (not running dspam_clean)"
	fi


	#
	# Release lock and delete temp file
	#
	rm -f "${DSPAM_CRON_LOCKFILE}"
	rm -f "${DSPAM_CRON_TMPFILE}"
	rm -f "${DSPAM_SQL_TMPFILE}"
	trap - INT TERM EXIT
fi
