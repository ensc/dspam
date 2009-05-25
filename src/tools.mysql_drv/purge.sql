# $Id: purge.sql,v 1.6 2009/05/25 11:46:46 mjohnson Exp $
set @a=to_days(current_date());
delete from dspam_token_data 
  where (innocent_hits*2) + spam_hits < 5
  and from_days(@a-60) > last_hit;
delete from dspam_token_data
  where ((innocent_hits = 1 and spam_hits = 0) or (innocent_hits = 0 and spam_hits = 1))
  and from_days(@a-15) > last_hit;
delete from dspam_token_data
  where from_days(@a-90) > last_hit;
delete from dspam_signature_data
  where from_days(@a-14) > created_on;

optimize table dspam_token_data, dspam_signature_data;
