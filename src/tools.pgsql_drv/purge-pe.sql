/* $Id: purge-pe.sql,v 1.4 2005/04/11 00:58:27 jonz Exp $ */

DELETE FROM dspam_token_data
  WHERE (innocent_hits*2) + spam_hits < 5
  AND CURRENT_DATE - last_hit > 60;

DELETE FROM dspam_token_data
  WHERE innocent_hits = 0 AND spam_hits = 1
  AND CURRENT_DATE - last_hit > 15;

DELETE FROM dspam_token_data
  WHERE innocent_hits = 1 AND spam_hits = 0
  AND CURRENT_DATE - last_hit > 15;

DELETE FROM dspam_token_data
  WHERE CURRENT_DATE - last_hit > 90
  AND uid NOT IN
    (SELECT uid FROM dspam_preferences
     WHERE preference = 'trainingMode'
     AND value IN('TOE','TUM','NOTRAIN'));

DELETE FROM dspam_token_data
  WHERE uid IN
    (SELECT uid FROM dspam_preferences
     WHERE preference = 'trainingMode'
     AND value = 'TUM')
  AND CURRENT_DATE - last_hit > 90
  AND innocent_hits + spam_hits < 50;

DELETE FROM dspam_signature_data
  WHERE CURRENT_DATE - created_on > 14;

VACUUM ANALYSE;
