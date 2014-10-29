
var mosquittoPBKDF2 = require('./mosquitto_pbkdf2');


// PASSWORD GENERATION TEST
// We create a new password.
mosquittoPBKDF2.createPasswordAsync('password', doSomethingWhenPBKDF2PasswordIsCreated);

function doSomethingWhenPBKDF2PasswordIsCreated(pbkdf2Password) {
	console.log("New password created: " + pbkdf2Password);
}



// PASSWORD VERIFICATION TEST
//We simulate that the following PBKDF2 has been queried from MySQL :-)
//The next variable 'passwordFromMySQL' correspond to 'password'
passwordFromMySQL = 'PBKDF2$sha256$901$j24KtOVCjYfsqfjL$bUIeZ0n39NuQ3MU+Y3pofaSsTRNlfang';

// We verify two passwords (one incorrect and other correct).
mosquittoPBKDF2.verifyCredentials('password', passwordFromMySQL, doSomethingAfterVerification);
mosquittoPBKDF2.verifyCredentials('incorrectPassword', passwordFromMySQL, doSomethingAfterVerification);

function doSomethingAfterVerification(validPassword) {
	if (validPassword) {
		console.log('The password is correct.');
	} else {
		console.log('The password is incorrect.');
	}
}
