# $Id: purge-4.1.sql,v 1.1 2004/10/24 20:53:29 jonz Exp $
set @a=to_days(current_date());
delete from dspam_token_data 
  where (innocent_hits*2) + spam_hits < 5
  and @a-to_days(last_hit) > 30;

delete from dspam_token_data
  where (innocent_hits = 1 or spam_hits = 1)
  and (innocent_hits = 0 or spam_hits = 0)
  and @a-to_days(last_hit) > 15;

delete from dspam_token_data
USING
  dspam_token_data LEFT JOIN dspam_preferences
  ON dspam_token_data.uid = dspam_preferences.uid
  AND dspam_preferences.preference = 'trainingMode'
  AND dspam_preferences.value in('TOE','TUM')
WHERE @a-to_days(dspam_token_data.last_hit) > 90
AND dspam_preferences.uid IS NULL;

delete from dspam_token_data
USING
  dspam_token_data LEFT JOIN dspam_preferences
  ON dspam_token_data.uid = dspam_preferences.uid
  AND dspam_preferences.preference = 'trainingMode'
  AND dspam_preferences.value = 'TUM'
WHERE @a-to_days(dspam_token_data.last_hit) > 90
AND innocent_hits + spam_hits < 50
AND dspam_preferences.uid IS NOT NULL;

delete from dspam_signature_data
  where @a-14 > to_days(created_on);
