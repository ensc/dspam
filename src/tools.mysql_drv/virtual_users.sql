# $Id: virtual_users.sql,v 1.23 2011/05/09 23:02:12 sbajic Exp $

create table dspam_virtual_uids (
  uid int unsigned not null primary key auto_increment,
  username varchar(128)
) engine=MyISAM;

create unique index id_virtual_uids_01 on dspam_virtual_uids(username);

