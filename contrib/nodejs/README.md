mosquitto-pbkdf2
================
[![NPM](https://nodei.co/npm/mosquitto-pbkdf2.png?downloads=true&downloadRank=true&stars=true)](https://nodei.co/npm/mosquitto-pbkdf2/)

A small module to generate/validate PBKDF2-sha256 passwords as required by mosquitto-auth-plug in node.js 

USAGE
=====


createPasswordAsync(password, callback);

- password: Desired password (plain)
- callback: To be called after PBKDF2 hash creation. Expects a string parameter that is the PBKDF2 hash generated.



verifyCredentials(password, PBKDF2Hash, callback);

- password: Desired password (plain)
- PBKDF2Hash: A PBKDF2 in mosquitto-auth-plug format to be compared to plain 'password'.
- callback: To be called after password-PBKDF2Hash verification. Expects a boolean parameter (true=Password ok, false=Password does not match).



See test.js for a more detailed example.

LICENSE
=======

MIT :-)
