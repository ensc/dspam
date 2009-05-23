# $Id: mysql_objects-4.1.sql,v 1.4 2009/05/23 15:02:47 sbajic Exp $

create table dspam_token_data (
  uid mediumint unsigned not null,
  token bigint unsigned not null,
  spam_hits bigint unsigned not null,
  innocent_hits bigint unsigned not null,
  last_hit date not null
) type=MyISAM PACK_KEYS=1;

create unique index id_token_data_01 on dspam_token_data(uid,token);

create table dspam_signature_data (
  uid mediumint unsigned not null,
  signature char(32) not null,
  data longblob not null,
  length int unsigned not null,
  created_on date not null
) type=MyISAM max_rows=2500000 avg_row_length=8096;

create unique index id_signature_data_01 on dspam_signature_data(uid,signature);
create index id_signature_data_02 on dspam_signature_data(created_on);

create table dspam_stats (
  uid mediumint unsigned primary key,
  spam_learned bigint unsigned not null,
  innocent_learned bigint unsigned not null,
  spam_misclassified bigint unsigned not null,
  innocent_misclassified bigint unsigned not null,
  spam_corpusfed bigint unsigned not null,
  innocent_corpusfed bigint unsigned not null,
  spam_classified bigint unsigned not null,
  innocent_classified bigint unsigned not null
) type=MyISAM;

create table dspam_preferences (
  uid mediumint unsigned not null,
  preference varchar(32) not null,
  value varchar(64) not null
) type=MyISAM;

create unique index id_preferences_01 on dspam_preferences(uid, preference);
