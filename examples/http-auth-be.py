#!/usr/bin/env python
# -*- coding: utf-8 -*-

__author__    = 'Jan-Piet Mens <jp@mens.de>'
__copyright__ = 'Copyright 2014 Jan-Piet Mens'

import sys
import bottle
from bottle import response, request

app = application = bottle.Bottle()

@app.route('/auth', method='POST')
def auth():
    response.content_type = 'text/plain'
    response.status = 403

    # data = bottle.request.body.read()   # username=jane%40mens.de&password=jolie&topic=&acc=-1

    username = request.forms.get('username')
    password = request.forms.get('password')
    topic    = request.forms.get('topic')
    acc      = request.forms.get('acc')

    if username == 'jane@mens.de' and password == 'jolie':
        response.status = 200

    return None

@app.route('/superuser', method='POST')
def superuser():
    response.content_type = 'text/plain'
    response.status = 403

    data = bottle.request.body.read()   # username=jane%40mens.de&password=&topic=&acc=-1

    username = request.forms.get('username')

    if username == 'special':
        response.status = 200

    return None


@app.route('/acl', method='POST')
def acl():
    response.content_type = 'text/plain'
    response.status = 403

    data = bottle.request.body.read()   # username=jane%40mens.de&password=&topic=t%2F1&acc=2&clientid=JANESUB

    username = request.forms.get('username')
    topic    = request.forms.get('topic')
    clientid = request.forms.get('clientid')
    acc      = request.forms.get('acc') # 1 == SUB, 2 == PUB

    if username == 'jane@mens.de' and topic == 't/1':
        response.status = 200

    return None

if __name__ == '__main__':

    bottle.debug(True)
    bottle.run(app,
        # server='python_server',
        host= "127.0.0.1",
        port= 8100)
