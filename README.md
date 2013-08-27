# mosquitto-auth-plug

This is a plugin to authenticate and authorize [Mosquitto] users from one
of several distinct back-ends:

* MySQL
* CDB
* [Redis] key/value store
* SQLite3 database

## Introduction

This plugin can perform authentication (check username / password) 
and authorization (ACL). Currently not all back-ends have the same capabilities
(the the section on the back-end you're interested in).

Passwords are obtained from the back-end as a PBKF2 string (see section
on Passwords below). Even if you try and store a clear-text password,
it simply won't work.

The plugin supports so-called _superusers_. These are usernames exempt
from ACL checking. In other words, if a user is a _superuser_, that user
doesn't require ACLs.

## Building the plugin

In order to compile the plugin you'll require a copy of the [Mosquitto] source
code together with the libraries required for the back-end you want to use in
the plugin. OpenSSL is also required.

Edit the `Makefile` and modify the definitions at the top to suit your building
environment, in particular, you have to configure which back-end you want to
provide as well as the path to the [Mosquitto] source and its library.

After a `make` you should have a shared object called `auth-plug.so`
which you will reference in your `mosquitto.conf`.

### MySQL

The `mysql` back-end is currently the most feature-complete: it supports
obtaining passwords, checking for _superusers_, and verifying ACLs by
configuring up to three distinct SQL queries used to obtain those results.

You configure the SQL queries in order to adapt to whichever schema
you currently have. 

The SQL query for looking up a user's password hash is mandatory. The query
MUST return a single row only (any other number of rows is considered to be
"user not found"), and it MUST return a single column only with the PBKF2
password hash. A single `'%s'` in the query string is replaced by the
username attempting to access the broker.

```sql
SELECT pw FROM users WHERE username = '%s' LIMIT 1
```

The SQL query for checking whether a user is a _superuser_ - and thus
circumventing ACL checks - is optional. If it is specified, the query MUST
return a single row with a single value: 0 is false and 1 is true. We recommend
using a `SELECT IFNULL(COUNT(*),0) FROM ...` for this query as it satisfies
both conditions. ). A single `'%s`' in the query string is replaced by the
username attempting to access the broker. The following example uses the
same `users` table, but it could just as well reference a distinct table
or view.

```sql
SELECT IFNULL(COUNT(*), 0) FROM users WHERE username = '%s' AND super = 1
```

The SQL query for checking ACLs is optional, but if it is specified, the
`mysql` back-end can try to limit access to particular topics or topic branches
depending on the value of a database table. The query MAY return zero or more
rows for a particular user, each returning EXACTLY one column containing a
topic (wildcards are supported). A single `'%s`' in the query string is
replaced by the username attempting to access the broker.

```sql
SELECT topic FROM acls WHERE username = '%s'
```

Mosquitto configuration for the `mysql` back-end:

```
auth_plugin /home/jpm/mosquitto-auth-plug/auth-plug.so
auth_opt_host localhost
auth_opt_port 3306
auth_opt_dbname test
#auth_opt_user jjj
#auth_opt_pass kkk
auth_opt_userquery SELECT pw FROM users WHERE username = '%s'
auth_opt_superquery SELECT COUNT(*) FROM users WHERE username = '%s' AND super = 1
auth_opt_aclquery SELECT topic FROM acls WHERE username = '%s'
```

Assuming the following database tables:

```
mysql> SELECT * FROM users;
+----+----------+---------------------------------------------------------------------+-------+
| id | username | pw                                                                  | super |
+----+----------+---------------------------------------------------------------------+-------+
|  1 | jjolie   | PBKDF2$sha256$901$x8mf3JIFTUFU9C23$Mid2xcgTrKBfBdye6W/4hE3GKeksu00+ |     0 |
|  2 | a        | PBKDF2$sha256$901$XPkOwNbd05p5XsUn$1uPtR6hMKBedWE44nqdVg+2NPKvyGst8 |     0 |
|  3 | su1      | PBKDF2$sha256$901$chEZ4HcSmKtlV0kf$yRh2N62uq6cHoAB6FIrxIN2iihYqNIJp |     1 |
+----+----------+---------------------------------------------------------------------+-------+

mysql> SELECT * FROM acls;
+----+----------+-------------------+----+
| id | username | topic             | rw |
+----+----------+-------------------+----+
|  1 | jjolie   | loc/jjolie        |  1 |
|  2 | jjolie   | $SYS/something    |  1 |
|  3 | a        | loc/test/#        |  1 |
|  4 | a        | $SYS/broker/log/+ |  1 |
|  5 | su1      | mega/secret       |  1 |
|  6 | nop      | mega/secret       |  1 |
+----+----------+-------------------+----+
```

the above SQL queries would enable the following combinations (note the `*` at
the beginning of the line indicating a _superuser_)

```
  jjolie     PBKDF2$sha256$901$x8mf3JIFTUFU9C23$Mid2xcgTrKBfBdye6W/4hE3GKeksu00+
	loc/a                                    DENY
	loc/jjolie                               PERMIT
	mega/secret                              DENY
	loc/test                                 DENY
	$SYS/broker/log/N                        DENY
  nop        <nil>
	loc/a                                    DENY
	loc/jjolie                               DENY
	mega/secret                              PERMIT
	loc/test                                 DENY
	$SYS/broker/log/N                        DENY
  a          PBKDF2$sha256$901$XPkOwNbd05p5XsUn$1uPtR6hMKBedWE44nqdVg+2NPKvyGst8
	loc/a                                    DENY
	loc/jjolie                               DENY
	mega/secret                              DENY
	loc/test                                 PERMIT
	$SYS/broker/log/N                        PERMIT
* su1        PBKDF2$sha256$901$chEZ4HcSmKtlV0kf$yRh2N62uq6cHoAB6FIrxIN2iihYqNIJp
	loc/a                                    PERMIT
	loc/jjolie                               PERMIT
	mega/secret                              PERMIT
	loc/test                                 PERMIT
	$SYS/broker/log/N                        PERMIT
```

### CDB

### SQLITE

### Redis

Usernames in Redis can have a prefix (e.g. `users:`) which is applied to
all users attempting to authenticate to this plugin.


## Passwords

A user's password is stored as a [PBKDF2] hash in the back-end. An example
"password" is a string with five pieces in it, delimited by `$`, inspired by
[this][1].

```
PBKDF2$sha256$901$8ebTR72Pcmjl3cYq$SCVHHfqn9t6Ev9sE6RMTeF3pawvtGqTu
--^--- --^--- -^- ------^--------- -------------^------------------
  |      |     |        |                       |
  |      |     |        |                       +-- : hashed password
  |      |     |        +-------------------------- : salt
  |      |     +----------------------------------- : iterations
  |      +----------------------------------------- : hash function
  +------------------------------------------------ : marker
```

## Creating a user

A trivial utility to generate hashes is included as `np`. Copy and paste the
whole string generated into the respective back-end.

```bash
$ np
Enter password:
Re-enter same password:
PBKDF2$sha256$901$Qh18ysY4wstXoHhk$g8d2aDzbz3rYztvJiO3dsV698jzECxSg
```

For example, in [Redis]:

```
$ redis-cli
> SET n2 PBKDF2$sha256$901$Qh18ysY4wstXoHhk$g8d2aDzbz3rYztvJiO3dsV698jzECxSg
> QUIT
```

with "n2" being the name of the user you want to add. If you're using username
prefixes you would prepend that:

```
$ redis-cli
> SET users:n2 PBKDF2$sha256$901$Qh18ysY4wstXoHhk$g8d2aDzbz3rYztvJiO3dsV698jzECxSg
> QUIT
```

## Configure Mosquitto

```
listener 1883

auth_plugin /path/to/auth-plug.so
# Optional: prefix users with the following string
auth_opt_redis_username_prefix users:
auth_opt_redis_host 127.0.0.1
auth_opt_redis_port 6379

# Clients may PUB/SUB to the following prefix. '%' is replaced
# with an authorized user's username (sans username_prefix). So,
# user 'jjolie' may PUB/SUB to "/location/jjolie" and her password
# is at Redis key "users:jjolie"
auth_opt_topic_prefix /location/%

# Usernames with this fnmatch(3) (a.k.a glob(3))  pattern are exempt from the
# module's ACL checking
auth_opt_superusers S*
```

## ACL

In addition to ACL checking which is possibly performed by a back-end,
there's a more "static" checking which can be configured in `mosquitto.conf`.

Note that if ACLs are being verified by the plugin, this also applies to 
Will topics (_last will and testament_). Failing to correctly set up
an ACL for these, will cause a broker to silently fail with a 'not 
authorized' message.

The plugin has support for checking topics allowed to a user. By default,
a topic_prefix is assumed, configured as `auth_opt_topic_prefix`.
Any number of `%` characters in this prefix are replaced by the username.

In the example above, a user "n2" would be allowed to access the
topic `"/location/n2"`, whereas a user "jjolie" would be allowed access
to the topic branch `"/location/jjolie"`.

Users can be given "superuser" status (i.e. they may access any topic)
if their username matches the _glob_ specified in `auth_opt_superusers`.

In our example above, any user with a username beginning with a capital `"S"`
is exempt from ACL-checking.

Wildcards are also supported. In the following example, the `%` will be replaced
by a username, and the `#` is an MQTT wild card.

```
auth_opt_topic_prefix /location/%/#
```

## PUB/SUB

At this point you ought to be able to connect to [Mosquitto].

```
mosquitto_pub  -t '/location/n2' -m hello -u n2 -P secret
```

## Requirements

* [hiredis], the Minimalistic C client for Redis
* OpenSSL (tested with 1.0.0c, but should work with earlier versions)
* A [Mosquitto] broker
* A [Redis] server
* MySQL
* [TinyCDB](http://www.corpit.ru/mjt/tinycdb.html) by Michael Tokarev (included in `contrib/`).

## Credits

* Uses `base64.[ch]` (and yes, I know OpenSSL has base64 routines, but no thanks). These files are
>  Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Hgskolan (Royal Institute of Technology, Stockholm, Sweden).


 [Mosquitto]: http://mosquitto.org
 [Redis]: http://redis.io
 [pbkdf2]: http://en.wikipedia.org/wiki/PBKDF2
 [1]: https://exyr.org/2011/hashing-passwords/
 [hiredis]: https://github.com/redis/hiredis
