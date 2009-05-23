# $Id: virtual_users.sql,v 1.2 2009/05/23 18:28:57 sbajic Exp $

create table dspam_virtual_uids (
  uid mediumint unsigned primary key AUTO_INCREMENT,
  username varchar(128)
) type=MyISAM;

create unique index id_virtual_uids_01 on dspam_virtual_uids(username);

