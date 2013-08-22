#!/usr/bin/env python

import sys
from getpass import getpass
import hashing_passwords as hp
                          # https://raw.github.com/SimonSapin/snippets/master/hashing_passwords.py
                          # requires https://github.com/mitsuhiko/python-pbkdf2.git

pw = getpass("Enter password: ")
pw2 = getpass("Re-enter password: ")

if pw != pw2:
    sys.exit("Passwords don't match!")

print hp.make_hash(pw)

