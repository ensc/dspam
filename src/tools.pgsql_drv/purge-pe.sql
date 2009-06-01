/* $Id: purge-pe.sql,v 1.51 2009/06/01 15:46:52 sbajic Exp $ */

START TRANSACTION;
DELETE FROM dspam_token_data
  WHERE (innocent_hits*2) + spam_hits < 5
  AND last_hit < CURRENT_DATE - 60;
COMMIT;

START TRANSACTION;
DELETE FROM dspam_token_data
  WHERE ((innocent_hits=1 AND spam_hits=0) OR (innocent_hits=0 AND spam_hits=1))
  AND last_hit < CURRENT_DATE - 15;
COMMIT;

START TRANSACTION;
DELETE FROM dspam_token_data
  WHERE last_hit < CURRENT_DATE - 90
  AND uid NOT IN
    (SELECT uid FROM dspam_preferences
     WHERE preference = 'trainingMode'
     AND UPPER(value) IN('TOE','TUM','NOTRAIN'));
COMMIT;

START TRANSACTION;
DELETE FROM dspam_token_data
  WHERE uid IN
    (SELECT uid FROM dspam_preferences
     WHERE preference = 'trainingMode'
     AND UPPER(value) = 'TUM')
  AND last_hit < CURRENT_DATE - 90
  AND innocent_hits + spam_hits < 50
  AND uid IS NOT NULL;
COMMIT;

START TRANSACTION;
DELETE FROM dspam_signature_data
  WHERE created_on < CURRENT_DATE - 14;
COMMIT;

VACUUM ANALYSE dspam_token_data;
VACUUM ANALYSE dspam_signature_data;

REINDEX TABLE dspam_token_data;
REINDEX TABLE dspam_signature_data;
