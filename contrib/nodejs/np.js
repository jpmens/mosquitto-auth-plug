var mosquittoPBKDF2 = require('./mosquitto_pbkdf2');
var promptly = require('promptly');

// do not allow empty password here...
var errFunc = function(err) {
    // err object not set for some reason
    console.log('Password must not be empty');
};
var notEmptyValidator = function (value) {
    if (value.length < 1) {
        throw new Error('Password must not be empty');
    }
    return value;
};

var options = {
    validator: notEmptyValidator,
    replace: '*',
    retry: false,
    default: ''
};

promptly.password('Enter password: ', options).then(function(pwd1) {
    promptly.password('Re-enter password: ', options).then(function(pwd2) {
        if (pwd1 !== pwd2) {
            console.log('Passwords do not match!');
        }
        else {
            mosquittoPBKDF2.createPasswordAsync(pwd1, function(newPBKDF2Password){
                console.log('New PBKDF2 hash: '+newPBKDF2Password);
            });
        }
    }, errFunc);
}, errFunc);
