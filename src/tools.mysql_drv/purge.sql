-- $Id: purge.sql,v 1.7 2009/12/18 00:43:54 sbajic Exp $

--
-- Set some defaults
--
SET @today=to_days(current_date());
SET @stale=15;

--
-- Delete fairly neutral tokens older than 4*@stale days
--
DELETE
  FROM dspam_token_data
    WHERE from_days(@today-(4*@stale)) > last_hit
      AND (innocent_hits*2) + spam_hits < 5;

--
-- Delete tokens seen only one time per class in the past @stale days
--
DELETE
  FROM dspam_token_data
    WHERE from_days(@today-@stale) > last_hit
      AND ((innocent_hits = 1 AND spam_hits = 0) OR (innocent_hits = 0 AND spam_hits = 1));

--
-- Delete all tokens never seen in the last 4*@stale days
--
DELETE
  FROM dspam_token_data
    WHERE from_days(@today-(6*@stale)) > last_hit;

--
-- Delete signatures older than @stale days
--
DELETE
  FROM dspam_signature_data
    WHERE from_days(@today-(@stale-1)) > created_on;
