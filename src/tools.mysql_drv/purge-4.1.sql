-- $Id: purge-4.1.sql,v 1.11 2010/04/21 21:14:18 sbajic Exp $

--
-- This file contains statements for purging the DSPAM for MySQL 4.1 or greater.
--

-- ---------------------------------------------------------------------------
-- Note: Should you have modified your dspam.conf to have other intervals for
--       the purging or you have modified the TrainingMode to be other then
--       'TEFT' then please modify this SQL file to be in sync with your
--       dspam.conf.
--
-- Note: It is difficult to purge neutral tokens with SQL clauses the same way
--       as dspam_clean is doing it. So you should still run dspam_clean with
--       the "-u" parameter from time to time.
-- ---------------------------------------------------------------------------

--
-- Set some defaults
--
SET @TrainingMode    = 'TEFT';      -- Default training mode
SET @PurgeSignatures = 14;          -- Stale signatures
SET @PurgeUnused     = 90;          -- Unused tokens
SET @PurgeHapaxes    = 30;          -- Tokens with less than 5 hits (hapaxes)
SET @PurgeHits1S     = 15;          -- Tokens with only 1 spam hit
SET @PurgeHits1I     = 15;          -- Tokens with only 1 innocent hit
SET @today           = to_days(current_date());

--
-- Delete tokens with less than 5 hits (hapaxes)
--
START TRANSACTION;
DELETE LOW_PRIORITY QUICK
  FROM dspam_token_data
  WHERE from_days(@today-@PurgeHapaxes) > last_hit
    AND (2*innocent_hits)+spam_hits < 5;
COMMIT;

--
-- Delete tokens with only 1 spam hit
--
START TRANSACTION;
DELETE LOW_PRIORITY QUICK
  FROM dspam_token_data
  WHERE from_days(@today-@PurgeHits1S) > last_hit
    AND innocent_hits = 0 AND spam_hits = 1;
COMMIT;

--
-- Delete tokens with only 1 innocent hit
--
START TRANSACTION;
DELETE LOW_PRIORITY QUICK
  FROM dspam_token_data
  WHERE from_days(@today-@PurgeHits1I) > last_hit
    AND innocent_hits = 1 AND spam_hits = 0;
COMMIT;

--
-- Delete unused tokens, except for TOE, TUM and NOTRAIN modes
--
START TRANSACTION;
DELETE LOW_PRIORITY QUICK
  FROM t USING dspam_token_data t
    LEFT JOIN dspam_preferences p ON (p.preference = 'trainingMode' AND p.uid = t.uid)
    LEFT JOIN dspam_preferences d ON (d.preference = 'trainingMode' AND d.uid = 0)
  WHERE COALESCE(CONVERT(p.value USING latin1) COLLATE latin1_general_ci,CONVERT(d.value USING latin1) COLLATE latin1_general_ci,CONVERT(@TrainingMode USING latin1) COLLATE latin1_general_ci) NOT IN (_latin1 'TOE',_latin1 'TUM',_latin1 'NOTRAIN')
    AND from_days(@today-@PurgeUnused) > last_hit;
COMMIT;

--
-- Delete TUM tokens seen no more than 50 times
--
START TRANSACTION;
DELETE LOW_PRIORITY QUICK
  FROM t USING dspam_token_data t
    LEFT JOIN dspam_preferences p ON (p.preference = 'trainingMode' AND p.uid = t.uid)
    LEFT JOIN dspam_preferences d ON (d.preference = 'trainingMode' AND d.uid = 0)
  WHERE COALESCE(CONVERT(p.value USING latin1) COLLATE latin1_general_ci,CONVERT(d.value USING latin1) COLLATE latin1_general_ci,CONVERT(@TrainingMode USING latin1) COLLATE latin1_general_ci) = _latin1 'TUM'
    AND from_days(@today-@PurgeUnused) > last_hit
    AND innocent_hits + spam_hits < 50;
COMMIT;

--
-- Delete stale signatures
--
START TRANSACTION;
DELETE LOW_PRIORITY QUICK
  FROM dspam_signature_data
  WHERE from_days(@today-@PurgeSignatures) > created_on;
COMMIT;
