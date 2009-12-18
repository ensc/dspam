-- $Id: purge-4.1.sql,v 1.8 2008/12/18 01:44:36 sbajic Exp $

--
-- Set some defaults
--
SET @stale=15;
SET @today=to_days(current_date());
SET @trainingmode=(
  SELECT COALESCE(
    (SELECT value
      FROM dspam_preferences
        WHERE uid = 0
          AND preference = 'trainingMode'
        LIMIT 1
    )
  ,'TEFT')
);

--
-- Delete fairly neutral tokens older than 4*@stale days
--
START TRANSACTION;
DELETE LOW_PRIORITY QUICK
  FROM dspam_token_data
    WHERE from_days(@today-(4*@stale)) > last_hit
      AND (innocent_hits*2) + spam_hits < 5;
COMMIT;

--
-- Delete tokens seen only one time per class in the past @stale days
--
START TRANSACTION;
DELETE LOW_PRIORITY QUICK
  FROM dspam_token_data
    WHERE from_days(@today-@stale) > last_hit
      AND ((innocent_hits = 1 AND spam_hits = 0) OR (innocent_hits = 0 AND spam_hits = 1));
COMMIT;

--
-- Delete all tokens never seen in the last 4*@stale days, except for TOE, TUM and NOTRAIN modes
--

-- For users having set 'trainingMode' in their preferences
START TRANSACTION;
DELETE LOW_PRIORITY QUICK
  FROM t USING dspam_token_data t
    LEFT JOIN dspam_preferences p
      ON (t.uid = p.uid AND p.preference = 'trainingMode' AND p.value NOT IN ('TOE','TUM','NOTRAIN') LIMIT 1)
  WHERE from_days(@today-(4*@stale)) > last_hit;
COMMIT;

-- For users NOT having set 'trainingMode' in their preferences (therefor using default/uid 0)
START TRANSACTION;
DELETE LOW_PRIORITY QUICK
  FROM t USING dspam_token_data t
    LEFT JOIN dspam_preferences p
      ON (t.uid = p.uid)
  WHERE @trainingmode NOT IN ('TOE','TUM','NOTRAIN')
    AND NOT EXISTS (SELECT uid FROM dspam_preferences WHERE uid = t.uid AND preference = 'trainingMode' LIMIT 1)
    AND from_days(@today-(4*@stale)) > last_hit;
COMMIT;

--
-- Delete TUM tokens seen no more than 50 times in any class for the last 6*@stale days
--

-- For users having set 'trainingMode' in their preferences
START TRANSACTION;
  DELETE LOW_PRIORITY QUICK
    FROM t USING dspam_token_data t
      LEFT JOIN dspam_preferences p
        ON (t.uid = p.uid AND p.preference = 'trainingMode' AND p.value = 'TUM' LIMIT 1)
    WHERE from_days(@today-(6*@stale)) > last_hit
      AND innocent_hits + spam_hits < 50;
COMMIT;

-- For users NOT having set 'trainingMode' in their preferences (therefor using default/uid 0)
START TRANSACTION;
  DELETE LOW_PRIORITY QUICK
    FROM t USING dspam_token_data t
      LEFT JOIN dspam_preferences p
        ON (t.uid = p.uid)
    WHERE @trainingmode = 'TUM'
      AND NOT EXISTS (SELECT uid FROM dspam_preferences WHERE uid = t.uid AND preference = 'trainingMode' LIMIT 1)
      AND from_days(@today-(6*@stale)) > last_hit
      AND innocent_hits + spam_hits < 50;
COMMIT;

--
-- Delete signatures older than @stale days
--
START TRANSACTION;
  DELETE LOW_PRIORITY QUICK
    FROM dspam_signature_data
    WHERE from_days(@today-(@stale-1)) > created_on;
COMMIT;
