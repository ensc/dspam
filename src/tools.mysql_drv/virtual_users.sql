# $Id: virtual_users.sql,v 1.21 2009/05/25 11:54:52 sbajic Exp $

create table dspam_virtual_uids (
  uid int unsigned primary key AUTO_INCREMENT,
  username varchar(128)
) type=MyISAM;

create unique index id_virtual_uids_01 on dspam_virtual_uids(username);

