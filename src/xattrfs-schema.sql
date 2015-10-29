-- xattrfs-schema.sql

begin transaction;

drop table if exists xdb_ns;
drop table if exists xdb_xattr;

create table xdb_ns (
  nid integer not null,
  ns text not null,
  primary key (nid)
);

insert into xdb_ns (nid, ns) values (0, '');
insert into xdb_ns (nid, ns) values (1, 'system');
insert into xdb_ns (nid, ns) values (2, 'security');
insert into xdb_ns (nid, ns) values (3, 'trusted');
insert into xdb_ns (nid, ns) values (4, 'user');

create table xdb_xattr (
  xid integer not null,
  ino integer not null,
  nid integer not null references xdb_ns(nid),
  name text not null,
  value blob not null,
  primary key (xid)
);

create unique index idx_xattr_path on xdb_xattr(ino, nid, name);
create index idx_xattr_nid  on xdb_xattr(nid, name);

end transaction;

