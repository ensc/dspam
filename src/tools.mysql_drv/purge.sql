# $Id: purge.sql,v 1.2 2005/01/17 23:17:29 jonz Exp $
set @a=to_days(current_date());
delete from dspam_token_data 
  where (innocent_hits*2) + spam_hits < 5
  and @a-to_days(last_hit) > 60;
delete from dspam_token_data
  where (innocent_hits = 1 or spam_hits = 1)
  and (innocent_hits = 0 or spam_hits = 0)
  and @a-to_days(last_hit) > 15;
delete from dspam_token_data
  where @a-to_days(last_hit) > 90;
delete from dspam_signature_data
  where @a-14 > to_days(created_on);
