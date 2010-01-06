#!/bin/sh
#
# $Id: dspam_maintenance.sh,v 1.03 2010/01/06 01:07:52 sbajic Exp $
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
		--without-sql-purge) USE_SQL_PURGE=false;;
		--with-all-drivers) PURGE_ALL_DRIVERS=true;;
		*)
			echo -ne "Usage: $0 \n\t[--profile=[PROFILE]]\n\t[--logdays=no_of_days]\n\t[--signatures=no_of_days]\n\t[--neutral=no_of_days]\n\t[--unused=no_of_days]\n\t[--hapaxes=no_of_days]\n\t[--hits1s=no_of_days]\n\t[--hits1i=no_of_days]\n\t[--without-sql-purge]\n\t[--with-all-drivers]\n"
			exit 1
			;;
	esac
done


#
# Function to run dspam_clean
#
run_dspam_clean() {
	local PURGE_SIG="${1}"
	local ADD_PARAMETER=""
	read_dspam_params DefaultProfile
	if [ -n "${PROFILE}" -a -n "${DefaultProfile}" -a "${PROFILE/*.}" != "${DefaultProfile}" ]
	then
		ADD_PARAMETER="--profile=${PROFILE/*.}"
	fi
	if [ "${PURGE_SIG}" = "YES" ]
	then
		${DSPAM_BIN_DIR}/dspam_clean ${ADD_PARAMETER} -s${SIGNATURE_AGE} -p${NEUTRAL_AGE} -u${UNUSED_AGE},${HAPAXES_AGE},${HITS1S_AGE},${HITS1I_AGE} >/dev/null 2>&1
	else
		${DSPAM_BIN_DIR}/dspam_clean ${ADD_PARAMETER} -p${NEUTRAL_AGE} -u${UNUSED_AGE},${HAPAXES_AGE},${HITS1S_AGE},${HITS1I_AGE} >/dev/null 2>&1
	fi
	return ${?}
}


#
# Function to check if we have all needed tools
#
check_for_tools() {
	local myrc=0
	for foo in awk cut sed sort
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
	if	${USE_SQL_PURGE} && \
		read_dspam_params MySQLServer${PROFILE} MySQLPort${PROFILE} MySQLUser${PROFILE} MySQLPass${PROFILE} MySQLDb${PROFILE} MySQLCompress${PROFILE} && \
		[ -n "${MySQLServer}" -a -n "${MySQLUser}" -a -n "${MySQLDb}" ]
	then
		if [ ! -e "${MYSQL_BIN_DIR}/mysql_config" ]
		then
			echo "Can not run MySQL purge script:"
			echo "  ${MYSQL_BIN_DIR}/mysql_config does not exist"
			return 1
		fi
		DSPAM_MySQL_PURGE_SQL=
		DSPAM_MySQL_VER=$(${MYSQL_BIN_DIR}/mysql_config --version | sed "s:[^0-9.]*::g")
		DSPAM_MySQL_MAJOR=$(echo "${DSPAM_MySQL_VER}" | cut -d. -f1)
		DSPAM_MySQL_MINOR=$(echo "${DSPAM_MySQL_VER}" | cut -d. -f2)
		DSPAM_MySQL_MICRO=$(echo "${DSPAM_MySQL_VER}" | cut -d. -f3)
		DSPAM_MySQL_INT=$(($DSPAM_MySQL_MAJOR * 65536 + $DSPAM_MySQL_MINOR * 256 + $DSPAM_MySQL_MICRO))

		# For MySQL >= 4.1 use the new purge script
		if [ "${DSPAM_MySQL_INT}" -ge "262400" ]
		then
			if [ -f "${DSPAM_CONFIGDIR}/config/mysql_purge-4.1-optimized.sql" -o -f "${DSPAM_CONFIGDIR}/mysql_purge-4.1-optimized.sql" ]
			then
				# See: http://securitydot.net/txt/id/32/type/articles/
				[ -f "${DSPAM_CONFIGDIR}/config/mysql_purge-4.1-optimized.sql" ] && DSPAM_MySQL_PURGE_SQL="${DSPAM_CONFIGDIR}/config/mysql_purge-4.1-optimized.sql"
				[ -f "${DSPAM_CONFIGDIR}/mysql_purge-4.1-optimized.sql" ] && DSPAM_MySQL_PURGE_SQL="${DSPAM_CONFIGDIR}/mysql_purge-4.1-optimized.sql"
			else
				[ -f "${DSPAM_CONFIGDIR}/config/mysql_purge-4.1.sql" ] && DSPAM_MySQL_PURGE_SQL="${DSPAM_CONFIGDIR}/config/mysql_purge-4.1.sql"
				[ -f "${DSPAM_CONFIGDIR}/mysql_purge-4.1.sql" ] && DSPAM_MySQL_PURGE_SQL="${DSPAM_CONFIGDIR}/mysql_purge-4.1.sql"
			fi
		else
			[ -f "${DSPAM_CONFIGDIR}/config/mysql_purge.sql" ] && DSPAM_MySQL_PURGE_SQL="${DSPAM_CONFIGDIR}/config/mysql_purge.sql"
			[ -f "${DSPAM_CONFIGDIR}/mysql_purge.sql" ] && DSPAM_MySQL_PURGE_SQL="${DSPAM_CONFIGDIR}/mysql_purge.sql"
		fi

		if [ -z "${DSPAM_MySQL_PURGE_SQL}" ]
		then
			echo "Can not run MySQL purge script:"
			echo "  No mysql_purge SQL script found"
			return 1
		fi

		if [ ! -e "${MYSQL_BIN_DIR}/mysql" ]
		then
			echo "Can not run MySQL purge script:"
			echo "  ${MYSQL_BIN_DIR}/mysql does not exist"
			return 1
		fi

		# Construct mysql command line
		DSPAM_MySQL_CMD="${MYSQL_BIN_DIR}/mysql --silent --user=${MySQLUser}"
		[ -S "${MySQLServer}" ] &&
			DSPAM_MySQL_CMD="${DSPAM_MySQL_CMD} --socket=${MySQLServer}" ||
			DSPAM_MySQL_CMD="${DSPAM_MySQL_CMD} --host=${MySQLServer}"
		[ -n "${MySQLPort}" ] &&
			DSPAM_MySQL_CMD="${DSPAM_MySQL_CMD} --port=${MySQLPort}"
		[ "${MySQLCompress}" = "true" ] &&
			DSPAM_MySQL_CMD="${DSPAM_MySQL_CMD} --compress"

		# Run the MySQL purge script
		MYSQL_PWD="${MySQLPass}" ${DSPAM_MySQL_CMD} ${MySQLDb} < ${DSPAM_MySQL_PURGE_SQL} >/dev/null
		_RC=${?}
		if [ ${_RC} != 0 ]
		then
			echo "MySQL purge script returned error code ${_RC}"
		fi

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
	if	${USE_SQL_PURGE} && \
		read_dspam_params PgSQLServer${PROFILE} PgSQLPort${PROFILE} PgSQLUser${PROFILE} PgSQLPass${PROFILE} PgSQLDb${PROFILE} && \
		[ -n "${PgSQLServer}" -a -n "${PgSQLUser}" -a -n "${PgSQLDb}" ]
	then
		DSPAM_PgSQL_PURGE_SQL=""
		if [ -f "${DSPAM_CONFIGDIR}/config/config/pgsql_pe-purge.sql" -o -f "${DSPAM_CONFIGDIR}/pgsql_pe-purge.sql" ]
		then
			[ -f "${DSPAM_CONFIGDIR}/config/pgsql_pe-purge.sql" ] && DSPAM_MySQL_PURGE_SQL="${DSPAM_CONFIGDIR}/config/pgsql_pe-purge.sql"
			[ -f "${DSPAM_CONFIGDIR}/pgsql_pe-purge.sql" ] && DSPAM_MySQL_PURGE_SQL="${DSPAM_CONFIGDIR}/pgsql_pe-purge.sql"
		else
			[ -f "${DSPAM_CONFIGDIR}/config/pgsql_purge.sql" ] && DSPAM_PgSQL_PURGE_SQL="${DSPAM_CONFIGDIR}/config/pgsql_purge.sql"
			[ -f "${DSPAM_CONFIGDIR}/pgsql_purge.sql" ] && DSPAM_PgSQL_PURGE_SQL="${DSPAM_CONFIGDIR}/pgsql_purge.sql"
		fi
		if [ -z "${DSPAM_PgSQL_PURGE_SQL}" ]
		then
			echo "Can not run PostgreSQL purge script:"
			echo "  No pgsql_purge SQL script found"
			return 1
		fi

		if [ ! -e "${PGSQL_BIN_DIR}/psql" ]
		then
			echo "Can not run PostgreSQL purge script:"
			echo "  ${PGSQL_BIN_DIR}/psql does not exist"
			return 1
		fi

		# Construct psql command line
		DSPAM_PgSQL_CMD="${PGSQL_BIN_DIR}/psql -q -U ${PgSQLUser} -h ${PgSQLServer} -d ${PgSQLDb}"
		[ -n "${PgSQLPort}" ] &&
			DSPAM_PgSQL_CMD="${DSPAM_PgSQL_CMD} -p ${PgSQLPort}"

		# Run the PostgreSQL purge script
		PGUSER="${PgSQLUser}" PGPASSWORD="${PgSQLPass}" ${DSPAM_PgSQL_CMD} -f "${DSPAM_PgSQL_PURGE_SQL}" >/dev/null

		_RC=${?}
		if [ ${_RC} != 0 ]
		then
			echo "PostgreSQL purge script returned error code ${_RC}"
		fi

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
	if [ -d "${DSPAM_HOMEDIR}/data" ]
	then
		if [ -e ${DSPAM_BIN_DIR}/cssclean ]
		then
			find ${DSPAM_HOMEDIR}/data/ -maxdepth 4 -mindepth 1 -type f -name "*.css" | while read name
			do
				${DSPAM_BIN_DIR}/cssclean "${name}" 1>/dev/null 2>&1
			done
		else
			echo "DSPAM cssclean binary not found!"
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
	if	${USE_SQL_PURGE}
	then
		DSPAM_SQLite3_PURGE_SQL=""
		[ -f "${DSPAM_CONFIGDIR}/config/sqlite3_purge.sql" ] && DSPAM_SQLite3_PURGE_SQL="${DSPAM_CONFIGDIR}/config/sqlite3_purge.sql"
		[ -f "${DSPAM_CONFIGDIR}/sqlite3_purge.sql" ] && DSPAM_SQLite3_PURGE_SQL="${DSPAM_CONFIGDIR}/sqlite3_purge.sql"

		if [ -z "${DSPAM_SQLite3_PURGE_SQL}" ]
		then
			echo "Can not run SQLite3 purge script:"
			echo "  No sqlite_purge SQL script found"
			return 1
		fi

		if [ ! -e "${SQLITE3_BIN_DIR}/sqlite3" ]
		then
			echo "Can not run SQLite3 purge script:"
			echo "  ${SQLITE3_BIN_DIR}/sqlite3 does not exist"
			return 1
		fi

		# Run the SQLite3 purge script and vacuum
		find "${DSPAM_HOMEDIR}" -name "*.sdb" -print | while read name
		do
			${SQLITE3_BIN_DIR}/sqlite3 "${name}" < "${DSPAM_SQLite3_PURGE_SQL}" >/dev/null
			# Enable the next line if you don't vacuum in the purge script
			# echo "vacuum;" | ${SQLITE3_BIN_DIR}/sqlite3 "${name}" >/dev/null
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
	if	${USE_SQL_PURGE}
	then
		DSPAM_SQLite_PURGE_SQL=""
		[ -f "${DSPAM_CONFIGDIR}/config/sqlite_purge.sql" ] && DSPAM_SQLite_PURGE_SQL="${DSPAM_CONFIGDIR}/config/sqlite_purge.sql"
		[ -f "${DSPAM_CONFIGDIR}/sqlite_purge.sql" ] && DSPAM_SQLite_PURGE_SQL="${DSPAM_CONFIGDIR}/sqlite_purge.sql"

		if [ -z "${DSPAM_SQLite_PURGE_SQL}" ]
		then
			echo "Can not run SQLite purge script:"
			echo "  No sqlite_purge SQL script found"
			return 1
		fi

		if [ ! -e "${SQLITE_BIN_DIR}/sqlite" ]
		then
			echo "Can not run SQLite purge script:"
			echo "  ${SQLITE_BIN_DIR}/sqlite does not exist"
			return 1
		fi

		# Run the SQLite purge script and vacuum
		find "${DSPAM_HOMEDIR}" -name "*.sdb" -print | while read name
		do
			${SQLITE_BIN_DIR}/sqlite "${name}" < "${DSPAM_SQLite_PURGE_SQL}" >/dev/null
			# Enable the next line if you don't vacuum in the purge script
			# echo "vacuum;" | ${SQLITE_BIN_DIR}/sqlite "${name}" >/dev/null
		done 1>/dev/null 2>&1
		return 0
	fi

}


#
# Use a lock file to prevent multiple runs at the same time
#
DSPAM_CRON_LOCKFILE="/var/run/$(basename $0 .sh).pid"


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

	trap 'rm -f "${DSPAM_CRON_LOCKFILE}"; exit ${?}' INT TERM EXIT

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
		esac
	done

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
	[ -z "${LOGROTATE_AGE}" ] && LOGROTATE_AGE=15			# System and user log
	[ -z "${USE_SQL_PURGE}" ] && USE_SQL_PURGE=true			# Run SQL purge scripts
	[ -z "${PURGE_ALL_DRIVERS}" ] && PURGE_ALL_DRIVERS=false	# Only purge active driver
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
	${DSPAM_BIN_DIR}/dspam_logrotate -a ${LOGROTATE_AGE} -d "${DSPAM_HOMEDIR}" >/dev/null &


	#
	# Don't purge signatures with dspam_clean if we purged them with SQL
	#
	RUN_FULL_DSPAM_CLEAN="NO"


	#
	# Available DSPAM storage drivers
	#
	STORAGE_DRIVERS=($(${DSPAM_BIN_DIR}/dspam --version 2>&1 | sed -n "s:,: :;s:^.*\-\-with\-storage\-driver=\([^'\"]*\).*:\1:p" | sort -u))

	#
	# Currently active storage driver
	#
	if [ ${#STORAGE_DRIVERS[@]} -gt 1 ]
	then
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
	else
		ACTIVE_DRIVER="${STORAGE_DRIVERS[@]}"
	fi


	#
	# Which drivers to process/purge
	#
	if ! ${PURGE_ALL_DRIVERS} && [ -n "${ACTIVE_DRIVER}" ]
	then
		DRIVERS_TO_PROCESS=${ACTIVE_DRIVER}
	else
		DRIVERS_TO_PROCESS=${STORAGE_DRIVERS[@]}
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
	run_dspam_clean ${RUN_FULL_DSPAM_CLEAN}


	#
	# Release lock
	#
	/bin/rm -f "${DSPAM_CRON_LOCKFILE}"
	trap - INT TERM EXIT
fi
