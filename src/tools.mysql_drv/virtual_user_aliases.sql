# $Id: virtual_user_aliases.sql,v 1.2 2009/05/25 11:52:02 sbajic Exp $

create table dspam_virtual_uids (
  uid int unsigned not null,
  username varchar(128) not null
) type=MyISAM;

create unique index id_virtual_uids_01 on dspam_virtual_uids(username);
