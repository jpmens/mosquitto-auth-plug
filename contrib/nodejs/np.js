var mosquittoPBKDF2 = require('./mosquitto_pbkdf2');
var prompt = require('prompt');

var schema = {
    properties: {
        pwd1: {
            hidden: true,
            required: true,
            description: "Enter password"
        },
        pwd2: {
            hidden: true,
            required: true,
            description: "Re-enter password"
        }
    }
};

prompt.get(schema, function (err, result) {
    if (!err) {
        if (result.pwd1 !== result.pwd2) {
            console.log('Passwords do not match!');
        } else {
            mosquittoPBKDF2.createPasswordAsync(result.pwd1, function(newPBKDF2Password){
                console.log('New PBKDF2 hash: '+newPBKDF2Password);
            });
        }
    }
});
