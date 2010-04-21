/* $Id: purge.sql,v 1.52 2010/04/21 11:30:39 sbajic Exp $ */

START TRANSACTION;
DELETE FROM dspam_token_data
  WHERE (innocent_hits*2) + spam_hits < 5
  AND last_hit < CURRENT_DATE - 30;
COMMIT;

START TRANSACTION;
DELETE FROM dspam_token_data
  WHERE ((innocent_hits=1 AND spam_hits=0) OR (innocent_hits=0 AND spam_hits=1))
  AND last_hit < CURRENT_DATE - 15;
COMMIT;

START TRANSACTION;
DELETE FROM dspam_token_data
  WHERE last_hit < CURRENT_DATE - 90;
COMMIT;

START TRANSACTION;
DELETE FROM dspam_signature_data
  WHERE created_on < CURRENT_DATE - 14;
COMMIT;

VACUUM ANALYSE dspam_token_data;
VACUUM ANALYSE dspam_signature_data;

REINDEX TABLE dspam_token_data;
REINDEX TABLE dspam_signature_data;
