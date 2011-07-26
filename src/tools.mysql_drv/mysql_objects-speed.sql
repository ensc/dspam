# $Id: mysql_objects-speed.sql,v 1.32 2011/05/09 22:43:52 sbajic Exp $

-- ---------------------------------------------------------------------------
--  ! ! ! ! ! ! ! THIS FILE SHOULD ONLY BE USED ON MYSQL < 4.1 ! ! ! ! ! ! !
--  ! ! ! ! ! FOR MYSQL 4.1+ PLEASE USE mysql_objects-4.1.sql FILE ! ! ! ! !
-- ---------------------------------------------------------------------------

create table dspam_token_data (
  uid int unsigned not null,
  token char(20) not null,
  spam_hits bigint unsigned not null,
  innocent_hits bigint unsigned not null,
  last_hit date not null
) engine=MyISAM;

create unique index id_token_data_01 on dspam_token_data(uid,token);
create index spam_hits on dspam_token_data(spam_hits);
create index innocent_hits on dspam_token_data(innocent_hits);
create index last_hit on dspam_token_data(last_hit);

create table dspam_signature_data (
  uid int unsigned not null,
  signature char(32) not null,
  data longblob not null,
  length int unsigned not null,
  created_on date not null
) engine=MyISAM;

create unique index id_signature_data_01 on dspam_signature_data(uid,signature);
create index id_signature_data_02 on dspam_signature_data(created_on);

create table dspam_stats (
  uid int unsigned primary key,
  spam_learned bigint unsigned not null,
  innocent_learned bigint unsigned not null,
  spam_misclassified bigint unsigned not null,
  innocent_misclassified bigint unsigned not null,
  spam_corpusfed bigint unsigned not null,
  innocent_corpusfed bigint unsigned not null,
  spam_classified bigint unsigned not null,
  innocent_classified bigint unsigned not null
) engine=MyISAM;

create table dspam_preferences (
  uid int unsigned not null,
  preference varchar(32) not null,
  value varchar(64) not null
) engine=MyISAM;

create unique index id_preferences_01 on dspam_preferences(uid, preference);
