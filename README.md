# mosquitto-auth-plug

This is a plugin to authenticate and authorize [Mosquitto] users from one
of several distinct back-ends:

* [Redis] key/value store
* CDB
* SQLite3 database

## Building the plugin

In order to compile the plugin you'll require a copy of the [Mosquitto] source
code together with the libraries required for the back-end you want to use in
the plugin. OpenSSL is also required.

Edit the `Makefile` and modify the definitions at the top to suit your building
environment, in particular, you have to configure which back-end you want to
provide as well as the path to the [Mosquitto] source.

After a `make` you should have a shared object called `auth-plug.so`
which you will reference in your `mosquitto.conf`.


## Passwords

Usernames in Redis can have a prefix (e.g. `users:`) which is applied to
all users attempting to authenticate to this plugin. A user's password
is stored as a [PBKDF2] hash in the back-end. An example "password" is a 
string with five pieces in it, delimited by `$`, inspired by [this][1].

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

A trivial utility to generate hashes is included as `np`.

```bash
$ np
Enter password:
Re-enter same password:
PBKDF2$sha256$901$Qh18ysY4wstXoHhk$g8d2aDzbz3rYztvJiO3dsV698jzECxSg
```

Copy and paste the "PBKDF2" string and add it to [Redis]:

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

auth_plugin /path/to/redis-auth.so
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

## Credits

* Uses `base64.[ch]` (and yes, I know OpenSSL has base64 routines, but no thanks). These files are
>  Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Hgskolan (Royal Institute of Technology, Stockholm, Sweden).


 [Mosquitto]: http://mosquitto.org
 [Redis]: http://redis.io
 [pbkdf2]: http://en.wikipedia.org/wiki/PBKDF2
 [1]: https://exyr.org/2011/hashing-passwords/
 [hiredis]: https://github.com/redis/hiredis
