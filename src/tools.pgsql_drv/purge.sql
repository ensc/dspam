/* $Id: purge.sql,v 1.2 2005/01/17 23:17:29 jonz Exp $ */

DELETE FROM dspam_token_data
  WHERE (innocent_hits*2) + spam_hits < 5
  AND CURRENT_DATE - last_hit > 60;

DELETE FROM dspam_token_data
  WHERE (innocent_hits = 1 OR spam_hits = 1)
  AND (innocent_hits = 0 or spam_hits = 0)
  AND CURRENT_DATE - last_hit > 15;

DELETE FROM dspam_token_data
  WHERE CURRENT_DATE - last_hit > 90;

DELETE FROM dspam_signature_data
  WHERE CURRENT_DATE - created_on > 14;

VACUUM ANALYSE;
