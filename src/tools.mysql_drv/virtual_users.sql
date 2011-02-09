# $Id: virtual_users.sql,v 1.22 2011/01/07 00:59:32 sbajic Exp $

create table dspam_virtual_uids (
  uid int unsigned not null primary key AUTO_INCREMENT,
  username varchar(128)
) type=MyISAM;

create unique index id_virtual_uids_01 on dspam_virtual_uids(username);

