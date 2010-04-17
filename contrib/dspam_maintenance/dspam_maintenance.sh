#!/bin/sh
#
# $Id: dspam_maintenance.sh,v 1.11 2010/04/17 16:58:31 sbajic Exp $
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
#          and on a older CentOS distro. Should you find any issues with
#          this script then please post a message on the DSPAM user
#          mailing list.
#
# Note   : This script can be called from the command line or run from
#          within cron. Either add a line in your crontab or add this
#          script in your cron.{hourly,daily,weekly,monthly}. Running
#          this script every hour might not be the best idea but it's
#          your decision if you want to do so.
###

MYSQL_BIN_DIR="/usr/bin"
PGSQL_BIN_DIR="/usr/bin"
SQLITE_BIN_DIR="/usr/bin"
SQLITE3_BIN_DIR="/usr/bin"
DSPAM_CONFIGDIR=""
DSPAM_HOMEDIR=""
DSPAM_BIN_DIR=""
DSPAM_PURGE_SCRIPT_DIR=""


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
		--with-all-drivers) PURGE_ALL_DRIVERS="true";;
		--verbose) VERBOSE="true";;
		*)
			echo "Usage: $0"
			echo "  [--profile=[PROFILE]]"
			echo "  [--logdays=no_of_days]"
			echo "  [--signatures=no_of_days]"
			echo "  [--neutral=no_of_days]"
			echo "  [--unused=no_of_days]"
			echo "  [--hapaxes=no_of_days]"
			echo "  [--hits1s=no_of_days]"
			echo "  [--hits1i=no_of_days]"
			echo "  [--without-sql-purge]"
			echo "  [--with-sql-optimization]"
			echo "  [--purgescriptdir=[DIRECTORY]"
			echo "  [--with-all-drivers]"
			echo "  [--verbose]"
			exit 1
			;;
	esac
done


#
# Function to run dspam_clean
#
run_dspam_clean() {
	[ "${VERBOSE}" = "true" ] && echo "Running dspam_clean ..."
	local PURGE_SIG="${1}"
	local ADD_PARAMETER=""
	read_dspam_params DefaultProfile
	if [ -n "${PROFILE}" -a -n "${DefaultProfile}" -a "${PROFILE/*.}" != "${DefaultProfile}" ]
	then
		ADD_PARAMETER="--profile=${PROFILE/*.}"
	fi
	if [ "${PURGE_SIG}" = "YES" ]
	then
		[ "${VERBOSE}" = "true" ] && echo "  * with purging old signatures"
		${DSPAM_BIN_DIR}/dspam_clean ${ADD_PARAMETER} -s${SIGNATURE_AGE} -p${NEUTRAL_AGE} -u${UNUSED_AGE},${HAPAXES_AGE},${HITS1S_AGE},${HITS1I_AGE} >/dev/null 2>&1
	else
		[ "${VERBOSE}" = "true" ] && echo "  * without purging old signatures"
		${DSPAM_BIN_DIR}/dspam_clean ${ADD_PARAMETER} -p${NEUTRAL_AGE} -u${UNUSED_AGE},${HAPAXES_AGE},${HITS1S_AGE},${HITS1I_AGE} >/dev/null 2>&1
	fi
	return ${?}
}


#
# Function to check if we have all needed tools
#
check_for_tools() {
	local myrc=0
	for foo in awk cut sed sort strings grep
	do
		if ! which ${foo} >/dev/null 2>&1
		then
			echo "Command ${foo} not found!"
			myrc=1
		fi
	done
	return ${myrc}
}


#
# Function to read dspam.conf parameters
#
read_dspam_params() {
	local PARAMETER VALUE
	local INCLUDE_DIRS
	INCLUDE_DIRS=$(awk "BEGIN { IGNORECASE=1; } \$1==\"Include\" { print \$2 \"/*.conf\"; }" "${DSPAM_CONFIGDIR}/dspam.conf" 2>/dev/null)
	for PARAMETER in $@ ; do
		VALUE=$(awk "BEGIN { IGNORECASE=1; } \$1==\"${PARAMETER}\" { print \$2; exit; }" "${DSPAM_CONFIGDIR}/dspam.conf" ${INCLUDE_DIRS[@]} 2>/dev/null)
		[ ${?} = 0 ] || return 1
		eval ${PARAMETER/.*}=\"${VALUE}\"
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
	if	[ "${USE_SQL_PURGE}" = "true" ] && \
		read_dspam_params MySQLServer${PROFILE} MySQLPort${PROFILE} MySQLUser${PROFILE} MySQLPass${PROFILE} MySQLDb${PROFILE} MySQLCompress${PROFILE} && \
		[ -n "${MySQLServer}" -a -n "${MySQLUser}" -a -n "${MySQLDb}" ]
	then
		if [ -e "${MYSQL_BIN_DIR}/mysql_config" ]
		then
			DSPAM_MySQL_VER=$(${MYSQL_BIN_DIR}/mysql_config --version | sed -e "s:[^0-9.]*::g" -e "1,/./{//d;}")
		elif [ -e "${MYSQL_BIN_DIR}/mysql" ]
		then
			DSPAM_MySQL_VER=$(${MYSQL_BIN_DIR}/mysql --version | sed -e "s:^.*Distrib[\t ]\{1,\}\([0-9.]*\).*:\1:g" -e "1,/./{//d;}")
		fi

		if [ -z "${DSPAM_MySQL_VER}" ]
		then
			echo "  Can not run MySQL purge script:"
			echo "    ${MYSQL_BIN_DIR}/mysql_config or ${MYSQL_BIN_DIR}/mysql does not exist"
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
			DSPAM_MySQL_PURGE_SQL_FILES="mysql_purge-4.1-optimized mysql_purge-4.1"
		else
			DSPAM_MySQL_PURGE_SQL_FILES="mysql_purge"

		fi

		#
		# We first search for the purge scripts in the directory the user has
		# told us to look for (command line option: --purgescriptdir
		# Then we look in DSPAM configuration directory under ./config/ and then
		# in the DSPAM configuration directory it self.
		#
		for foo in ${DSPAM_PURGE_SCRIPT_DIR} ${DSPAM_CONFIGDIR}/config ${DSPAM_CONFIGDIR}
		do
			for bar in ${DSPAM_MySQL_PURGE_SQL_FILES}
			do
				if [ -f "${foo}/${bar}.sql" ]
				then
					DSPAM_MySQL_PURGE_SQL="${foo}/${bar}.sql"
					break
				elif [ -f "${foo}/${bar/_//}.sql" ]
				then
					DSPAM_MySQL_PURGE_SQL="${foo}/${bar/_//}.sql"
					break
				fi
			done
			[ -n "${DSPAM_MySQL_PURGE_SQL}" ] && break
		done

		if [ -z "${DSPAM_MySQL_PURGE_SQL}" ]
		then
			echo "  Can not run MySQL purge script:"
			echo "    None of the ${DSPAM_MySQL_PURGE_SQL_FILES} SQL script(s) found"
			return 1
		fi

		if [ ! -r "${DSPAM_MySQL_PURGE_SQL}" ]
		then
			echo "  Can not read MySQL purge script:"
			echo "    ${DSPAM_MySQL_PURGE_SQL}"
			return 1
		fi

		if [ ! -e "${MYSQL_BIN_DIR}/mysql" ]
		then
			echo "  Can not run MySQL purge script:"
			echo "    ${MYSQL_BIN_DIR}/mysql does not exist"
			return 1
		fi

		# Construct mysql command line
		echo "[client]">"${DSPAM_CRON_TMPFILE}"
		echo "password=${MySQLPass}">>"${DSPAM_CRON_TMPFILE}"
		DSPAM_MySQL_CMD="${MYSQL_BIN_DIR}/mysql"
		DSPAM_MySQL_CMD="${DSPAM_MySQL_CMD} --defaults-extra-file=${DSPAM_CRON_TMPFILE}"
		DSPAM_MySQL_CMD="${DSPAM_MySQL_CMD} --silent"
		DSPAM_MySQL_CMD="${DSPAM_MySQL_CMD} --user=${MySQLUser}"
		[ -S "${MySQLServer}" ] &&
			DSPAM_MySQL_CMD="${DSPAM_MySQL_CMD} --socket=${MySQLServer}" ||
			DSPAM_MySQL_CMD="${DSPAM_MySQL_CMD} --host=${MySQLServer}"
		[ -n "${MySQLPort}" ] &&
			DSPAM_MySQL_CMD="${DSPAM_MySQL_CMD} --port=${MySQLPort}"
		[ "${MySQLCompress}" = "true" ] &&
			DSPAM_MySQL_CMD="${DSPAM_MySQL_CMD} --compress"

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
	if	[ "${USE_SQL_PURGE}" = "true" ] && \
		read_dspam_params PgSQLServer${PROFILE} PgSQLPort${PROFILE} PgSQLUser${PROFILE} PgSQLPass${PROFILE} PgSQLDb${PROFILE} && \
		[ -n "${PgSQLServer}" -a -n "${PgSQLUser}" -a -n "${PgSQLDb}" ]
	then
		DSPAM_PgSQL_PURGE_SQL=
		DSPAM_PgSQL_PURGE_SQL_FILES="pgsql_pe-purge"

		#
		# We first search for the purge scripts in the directory the user has
		# told us to look for (command line option: --purgescriptdir
		# Then we look in DSPAM configuration directory under ./config/ and then
		# in the DSPAM configuration directory it self.
		#
		for foo in ${DSPAM_PURGE_SCRIPT_DIR} ${DSPAM_CONFIGDIR}/config ${DSPAM_CONFIGDIR}
		do
			for bar in ${DSPAM_PgSQL_PURGE_SQL_FILES}
			do
				if [ -f "${foo}/${bar}.sql" ]
				then
					DSPAM_PgSQL_PURGE_SQL="${foo}/${bar}.sql"
					break
				elif [ -f "${foo}/${bar/_//}.sql" ]
				then
					DSPAM_PgSQL_PURGE_SQL="${foo}/${bar/_//}.sql"
					break
				fi
			done
			[ -n "${DSPAM_PgSQL_PURGE_SQL}" ] && break
		done

		if [ -z "${DSPAM_PgSQL_PURGE_SQL}" ]
		then
			echo "  Can not run PostgreSQL purge script:"
			echo "    None of the ${DSPAM_PgSQL_PURGE_SQL_FILES} SQL script(s) found"
			return 1
		fi

		if [ ! -r "${DSPAM_PgSQL_PURGE_SQL}" ]
		then
			echo "  Can not read PostgreSQL purge script:"
			echo "    ${DSPAM_PgSQL_PURGE_SQL}"
			return 1
		fi

		if [ ! -e "${PGSQL_BIN_DIR}/psql" ]
		then
			echo "  Can not run PostgreSQL purge script:"
			echo "    ${PGSQL_BIN_DIR}/psql does not exist"
			return 1
		fi

		# Construct psql command line
		echo "*:*:${PgSQLDb}:${PgSQLUser}:${PgSQLPass}">"${DSPAM_CRON_TMPFILE}"
		DSPAM_PgSQL_CMD="${PGSQL_BIN_DIR}/psql -q -U ${PgSQLUser} -h ${PgSQLServer} -d ${PgSQLDb}"
		[ -n "${PgSQLPort}" ] &&
			DSPAM_PgSQL_CMD="${DSPAM_PgSQL_CMD} -p ${PgSQLPort}"

		# Run the PostgreSQL purge script
		PGPASSFILE="${DSPAM_CRON_TMPFILE}" ${DSPAM_PgSQL_CMD} -f "${DSPAM_PgSQL_PURGE_SQL}" >/dev/null
		_RC=${?}
		if [ ${_RC} != 0 ]
		then
			echo "PostgreSQL purge script returned error code ${_RC}"
		fi
		echo "">"${DSPAM_CRON_TMPFILE}"

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
	[ "${VERBOSE}" = "true" ] && echo "Running Hash storage driver data cleanup"
	if [ -d "${DSPAM_HOMEDIR}/data" ]
	then
		if [ -e ${DSPAM_BIN_DIR}/cssclean ]
		then
			find ${DSPAM_HOMEDIR}/data/ -maxdepth 4 -mindepth 1 -type f -name "*.css" | while read name
			do
				${DSPAM_BIN_DIR}/cssclean "${name}" 1>/dev/null 2>&1
			done
		else
			echo "  DSPAM cssclean binary not found!"
		fi
		find ${DSPAM_HOMEDIR}/data/ -maxdepth 4 -mindepth 1 -type d -name "*.sig" | while read name
		do
			find "${name}" -maxdepth 1 -mindepth 1 -type f -mtime +${SIGNATURE_AGE} -name "*.sig" -exec /bin/rm "{}" ";"
		done
		return 0
	else
		return 1
	fi
}


#
# Function to clean DSPAM SQLite3 data
#
clean_sqlite3_drv() {
	#
	# SQLite3
	#
	[ "${VERBOSE}" = "true" ] && echo "Running SQLite v3 storage driver data cleanup"
	if	[ "${USE_SQL_PURGE}" = "true" ]
	then
		DSPAM_SQLite3_PURGE_SQL=""
		DSPAM_SQLite3_PURGE_SQL_FILES="sqlite3_purge"

		#
		# We first search for the purge scripts in the directory the user has
		# told us to look for (command line option: --purgescriptdir
		# Then we look in DSPAM configuration directory under ./config/ and then
		# in the DSPAM configuration directory it self.
		#
		for foo in ${DSPAM_PURGE_SCRIPT_DIR} ${DSPAM_CONFIGDIR}/config ${DSPAM_CONFIGDIR}
		do
			for bar in ${DSPAM_SQLite3_PURGE_SQL_FILES}
			do
				if [ -f "${foo}/${bar}.sql" ]
				then
					DSPAM_SQLite3_PURGE_SQL="${foo}/${bar}.sql"
					break
				elif [ -f "${foo}/${bar/_//}.sql" ]
				then
					DSPAM_SQLite3_PURGE_SQL="${foo}/${bar/_//}.sql"
					break
				fi
			done
			[ -n "${DSPAM_SQLite3_PURGE_SQL}" ] && break
		done

		if [ -z "${DSPAM_SQLite3_PURGE_SQL}" ]
		then
			echo "  Can not run SQLite3 purge script:"
			echo "    None of the ${DSPAM_SQLite3_PURGE_SQL_FILES} SQL script(s) found"
			return 1
		fi

		if [ ! -r "${DSPAM_SQLite3_PURGE_SQL}" ]
		then
			echo "  Can not read SQLite3 purge script:"
			echo "    ${DSPAM_SQLite3_PURGE_SQL}"
			return 1
		fi

		if [ ! -e "${SQLITE3_BIN_DIR}/sqlite3" ]
		then
			echo "  Can not run SQLite3 purge script:"
			echo "    ${SQLITE3_BIN_DIR}/sqlite3 does not exist"
			return 1
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
	if	[ "${USE_SQL_PURGE}" = "true" ]
	then
		DSPAM_SQLite_PURGE_SQL=""
		DSPAM_SQLite_PURGE_SQL_FILES="sqlite_purge"

		#
		# We first search for the purge scripts in the directory the user has
		# told us to look for (command line option: --purgescriptdir
		# Then we look in DSPAM configuration directory under ./config/ and then
		# in the DSPAM configuration directory it self.
		#
		for foo in ${DSPAM_PURGE_SCRIPT_DIR} ${DSPAM_CONFIGDIR}/config ${DSPAM_CONFIGDIR}
		do
			for bar in ${DSPAM_SQLite_PURGE_SQL_FILES}
			do
				if [ -f "${foo}/${bar}.sql" ]
				then
					DSPAM_SQLite_PURGE_SQL="${foo}/${bar}.sql"
					break
				elif [ -f "${foo}/${bar/_//}.sql" ]
				then
					DSPAM_SQLite_PURGE_SQL="${foo}/${bar/_//}.sql"
					break
				fi
			done
			[ -n "${DSPAM_SQLite_PURGE_SQL}" ] && break
		done

		if [ -z "${DSPAM_SQLite_PURGE_SQL}" ]
		then
			echo "  Can not run SQLite purge script:"
			echo "    None of the ${DSPAM_SQLite_PURGE_SQL_FILES} SQL script(s) found"
			return 1
		fi

		if [ ! -r "${DSPAM_SQLite_PURGE_SQL}" ]
		then
			echo "  Can not read SQLite purge script:"
			echo "    ${DSPAM_SQLite_PURGE_SQL}"
			return 1
		fi

		if [ ! -e "${SQLITE_BIN_DIR}/sqlite" ]
		then
			echo "  Can not run SQLite purge script:"
			echo "    ${SQLITE_BIN_DIR}/sqlite does not exist"
			return 1
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
DSPAM_CRON_TMPFILE="/tmp/.ds_$$"


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
	[ -e "${DSPAM_CRON_TMPFILE}" ] && /bin/rm -f "${DSPAM_CRON_TMPFILE}" >/dev/null 2>&1
	touch "${DSPAM_CRON_TMPFILE}"
	umask "${UMASK_OLD}"


	#
	# Check for needed tools
	#
	if ! check_for_tools
	then
		# We have not all needed tools installed. Run just the dspam_clean part.
		run_dspam_clean
		exit ${?}
	fi


	#
	# Try to read most of the configuration options from DSPAM
	#
	DSPAM_CONFIG_PARAMETERS=$(dspam --version 2>&1 | sed -n "s:^Configuration parameters\:[\t ]*\(.*\)$:\1:gI;s:' '\-\-:\n--:g;s:^'::g;s:' '[a-zA-Z].*::gp")
	if [ -z "${DSPAM_CONFIG_PARAMETERS}" ]
	then
		# Not good! dspam --version does not print out configuration parameters.
		# Try getting the information by parsing the strings in the DSPAM binary.
		DSPAM_CONFIG_PARAMETERS=$(strings $(whereis dspam | awk '{print $2}') 2>&1 | sed -n "/'\-\-[^']*'[\t ]*'\-\-[^']*'/p" 2>/dev/null | sed -n "s:' '\-\-:\n--:g;s:^[\t ]*'::g;s:' '[a-zA-Z].*::gp")
	fi
	if [ -n "${DSPAM_CONFIG_PARAMETERS}" ]
	then
		for foo in ${DSPAM_CONFIG_PARAMETERS}
		do
			case "${foo}" in
				--sysconfdir=*)
					DSPAM_CONFIGDIR="${foo#--sysconfdir=}"
					;;
				--with-dspam-home=*)
					DSPAM_HOMEDIR="${foo#--with-dspam-home=}"
					;;
				--prefix=*)
					DSPAM_BIN_DIR="${foo#--prefix=}"/bin
					;;
				--with-storage-driver=*)
					STORAGE_DRIVERS="${foo#--with-storage-driver=}"
					STORAGE_DRIVERS=($(echo ${STORAGE_DRIVERS} | sed "s:,:\n:g" | sort -u))
					;;
			esac
		done
		[ "${VERBOSE}" = "true" -a -n "${STORAGE_DRIVERS}" ] && echo "Enabled drivers are: ${STORAGE_DRIVERS[@]}"
	else
		echo "Warning: dspam --version does not print configuration parameters!"
	fi


	#
	# Try to get DSPAM bin directory
	#
	if [ -z "${DSPAM_BIN_DIR}" ]
	then
		if [ -e /usr/bin/dspam -a -e /usr/bin/dspam_clean ]
		then
			DSPAM_BIN_DIR="/usr/bin"
		else
			echo "DSPAM binary directory not found!"
			exit 2
		fi
	fi
	if [ ! -e "${DSPAM_BIN_DIR}/dspam" -o ! -e "${DSPAM_BIN_DIR}/dspam_clean" ]
	then
		echo "Binary for dspam and/or dspam_clean not found! Can not continue without it."
		exit 2
	fi


	#
	# Try to get DSPAM config directory
	#
	if [ -z "${DSPAM_CONFIGDIR}" ]
	then
		if [ -f /etc/mail/dspam/dspam.conf ]
		then
			DSPAM_CONFIGDIR="/etc/mail/dspam"
		elif [ -f /etc/dspam/dspam.conf ]
		then
			DSPAM_CONFIGDIR="/etc/dspam"
		else
			echo "Configuration directory not found!"
			exit 2
		fi
	fi
	if [ ! -f "${DSPAM_CONFIGDIR}/dspam.conf" ]
	then
		echo "dspam.conf not found! Can not continue without it."
		exit 2
	fi


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
		echo "Home directory not found! Please fix your dspam.conf."
		exit 2
	fi


	#
	# System and user log purging
	#
	if [ ! -e "${DSPAM_BIN_DIR}/dspam_logrotate" ]
	then
		echo "dspam_logrotate not found! Can not continue without it."
		exit 2
	fi
	[ "${VERBOSE}" = "true" ] && echo "Running dspam_logrotate"
	${DSPAM_BIN_DIR}/dspam_logrotate -a ${LOGROTATE_AGE} -d "${DSPAM_HOMEDIR}" >/dev/null &


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
		echo "Warning: Could not get a list of supported storage drivers!"
		echo "Warning: Could not determine the currently active storage driver!"
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
			echo "Hash storage driver detected (not running dspam_clean)"
		else
			run_dspam_clean ${RUN_FULL_DSPAM_CLEAN}
		fi
	else
		# Storage driver probably statically linked. Not running dspam_clean
		# because of potential risk that the storage driver used is the Hash
		# driver.
		echo "Could not detect current storage driver (not running dspam_clean)"
	fi


	#
	# Release lock and delete temp file
	#
	/bin/rm -f "${DSPAM_CRON_LOCKFILE}"
	/bin/rm -f "${DSPAM_CRON_TMPFILE}"
	trap - INT TERM EXIT
fi
