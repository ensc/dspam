/* $Id: pgsql_objects.sql,v 1.9 2005/08/03 12:37:47 jonz Exp $ */

CREATE TABLE dspam_token_data (
  uid smallint,
  token bigint,
  spam_hits int,
  innocent_hits int,
  last_hit date,
  UNIQUE (uid, token)
) WITHOUT OIDS;

CREATE TABLE dspam_signature_data (
  uid smallint,
  signature varchar(128),
  data bytea,
  length int,
  created_on date,
  UNIQUE (uid, signature)
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
  UNIQUE (uid, preference)
) WITHOUT OIDS;

create function lookup_tokens(integer,bigint[])
  returns setof dspam_token_data
  language plpgsql stable
  as '
declare
  v_rec record;
begin
  for v_rec in select * from dspam_token_data
                where uid=$1
                  and token in (select $2[i]
                                  from generate_series(array_lower($2,1),
                                                       array_upper($2,1)) s(i))
  loop
    return next v_rec;
  end loop;
  return;
end;';

