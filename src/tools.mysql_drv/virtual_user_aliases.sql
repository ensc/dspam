# $Id: virtual_user_aliases.sql,v 1.3 2011/05/09 23:01:51 sbajic Exp $

create table dspam_virtual_uids (
  uid int unsigned not null,
  username varchar(128) not null
) engine=MyISAM;

create unique index id_virtual_uids_01 on dspam_virtual_uids(username);
