/* $Id: pgsql_objects.sql,v 1.2 2004/12/03 13:19:52 jonz Exp $ */

CREATE TABLE dspam_token_data (
  uid smallint,
  token numeric(20),
  spam_hits int,
  innocent_hits int,
  last_hit date,
  UNIQUE (token, uid)
) WITHOUT OIDS;

CREATE INDEX id_token_data_01 ON dspam_token_data(innocent_hits);
CREATE INDEX id_token_data_02 ON dspam_token_data(spam_hits);
CREATE INDEX id_token_data_03 ON dspam_token_data(token);

/* This index is only necessary to fix a buggy pgsql query builder */

CREATE INDEX id_token_data_04 ON dspam_token_data(uid);

CREATE TABLE dspam_signature_data (
  uid smallint,
  signature varchar(128),
  data bytea,
  length int,
  created_on date,
  UNIQUE (signature, uid)
) WITHOUT OIDS;

CREATE TABLE dspam_stats (
  uid smallint PRIMARY KEY,
  spam_learned int,
  innocent_learned int,
  spam_misclassified int,
  innocent_misclassified int,
  spam_corpusfed int,
  innocent_corpusfed int,
  spam_classified int,
  innocent_classified int
) WITHOUT OIDS;

CREATE TABLE dspam_neural_data (
  uid smallint,
  node smallint,
  total_correct int,
  total_incorrect int,
  UNIQUE (node, uid)
) WITHOUT OIDS;

CREATE INDEX id_neural_data_01 ON dspam_neural_data(uid);

CREATE TABLE dspam_neural_decisions (
  uid smallint,
  signature varchar(128),
  data bytea,
  length int,
  created_on date,
  UNIQUE (signature, uid)
) WITHOUT OIDS;

CREATE TABLE dspam_preferences (
  uid smallint,
  preference varchar(128),
  value varchar(128),
  UNIQUE (preference, uid)
) WITHOUT OIDS;
