create table dspam_neural_data (
  uid smallint unsigned,
  node smallint unsigned,
  total_correct int,
  total_incorrect int
) type=MyISAM;

create unique index id_neural_data_01 on dspam_neural_data(uid,node);

create table dspam_neural_decisions (
  uid smallint unsigned,
  signature char(32),
  data blob,
  length smallint,
  created_on date
) type=MyISAM;

create unique index id_neural_decisions_01 on dspam_neural_decisions(uid, signature);
