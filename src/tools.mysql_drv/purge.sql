-- $Id: purge.sql,v 1.8 2009/12/19 17:50:03 sbajic Exp $

-- ---------------------------------------------------------------------------
--  ! ! ! ! ! ! ! THIS FILE SHOULD ONLY BE USED ON MYSQL < 4.1 ! ! ! ! ! ! !
--  ! ! ! ! ! ! ! FOR MYSQL 4.1+ PLEASE USE purge-4.1.sql FILE ! ! ! ! ! ! !
-- ---------------------------------------------------------------------------

-- ---------------------------------------------------------------------------
-- Note: Should you have modified your dspam.conf to have other intervals for
--       the purging then please modify this SQL file to be in sync with your
--       dspam.conf.
--
-- Note: It is difficult to purge neutral tokens with SQL clauses the same
--       way as dspam_clean is doing it. So you should still run dspam_clean
--       with the "-u" parameter from time to time.
-- ---------------------------------------------------------------------------

--
-- Set some defaults
--
SET @PurgeSignatures = 14;          -- Stale signatures
SET @PurgeUnused     = 90;          -- Unused tokens
SET @PurgeHapaxes    = 30;          -- Tokens with less than 5 hits (hapaxes)
SET @PurgeHits1S     = 15;          -- Tokens with only 1 spam hit
SET @PurgeHits1I     = 15;          -- Tokens with only 1 innocent hit
SET @today           = to_days(current_date());

--
-- Delete tokens with less than 5 hits (hapaxes)
--
DELETE
  FROM dspam_token_data
    WHERE from_days(@today-@PurgeHapaxes) > last_hit
      AND (2*innocent_hits)+spam_hits < 5;

--
-- Delete tokens with only 1 spam hit
--
DELETE
  FROM dspam_token_data
    WHERE from_days(@today-@PurgeHits1S) > last_hit
      AND innocent_hits = 0 AND spam_hits = 1;

--
-- Delete tokens with only 1 innocent hit
--
DELETE
  FROM dspam_token_data
    WHERE from_days(@today-@PurgeHits1I) > last_hit
      AND innocent_hits = 1 AND spam_hits = 0;

--
-- Delete old tokens not seen for a while
--
DELETE
  FROM dspam_token_data
    WHERE from_days(@today-@PurgeUnused) > last_hit;

--
-- Delete stale signatures
--
DELETE
  FROM dspam_signature_data
    WHERE from_days(@today-@PurgeSignatures) > created_on;
