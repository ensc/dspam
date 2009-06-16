-- $Id: purge-3.sql,v 1.0 2009/06/15 23:42:52 sbajic Exp $
delete from dspam_token_data
  where (innocent_hits*2) + spam_hits < 5
  and (julianday('now')-60) > julianday(last_hit);
delete from dspam_token_data
  where innocent_hits + spam_hits = 1
  and (julianday('now')-15) > julianday(last_hit);
delete from dspam_token_data
  where (julianday('now')-90) > julianday(last_hit);
delete from dspam_signature_data
  where (julianday('now')-14) > julianday(created_on);

vacuum;
