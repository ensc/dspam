# $Id: purge-4.1.sql,v 1.7 2008/05/25 11:37:21 sbajic Exp $
set @a=to_days(current_date());

START TRANSACTION;
delete from dspam_token_data 
  where (innocent_hits*2) + spam_hits < 5
  and from_days(@a-60) > last_hit;
COMMIT;

START TRANSACTION;
delete from dspam_token_data
  where innocent_hits = 1 and spam_hits = 0
  and from_days(@a-15) > last_hit;
COMMIT;

START TRANSACTION;
delete from dspam_token_data
  where innocent_hits = 0 and spam_hits = 1
  and from_days(@a-15) > last_hit;
COMMIT;

START TRANSACTION;
delete from dspam_token_data
USING
  dspam_token_data LEFT JOIN dspam_preferences
  ON dspam_token_data.uid = dspam_preferences.uid
  AND dspam_preferences.preference = 'trainingMode'
  AND dspam_preferences.value in('TOE','TUM','NOTRAIN')
WHERE from_days(@a-90) > dspam_token_data.last_hit
AND dspam_preferences.uid IS NULL;
COMMIT;

START TRANSACTION;
delete from dspam_token_data
USING
  dspam_token_data LEFT JOIN dspam_preferences
  ON dspam_token_data.uid = dspam_preferences.uid
  AND dspam_preferences.preference = 'trainingMode'
  AND dspam_preferences.value = 'TUM'
WHERE from_days(@a-90) > dspam_token_data.last_hit
AND innocent_hits + spam_hits < 50
AND dspam_preferences.uid IS NOT NULL;
COMMIT;

START TRANSACTION;
delete from dspam_signature_data
  where from_days(@a-14) > created_on;
COMMIT;

optimize table dspam_token_data, dspam_signature_data;
