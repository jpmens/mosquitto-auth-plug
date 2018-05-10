/**
 * ================================================================
 * This module provide PBKDF2 passwords functionality to be used
 * toguether with mosquitto-auth-plug, that uses this password
 * format but using its specific algorithm configuration.
 * License: MIT
 * @fileOverview PBKDF2 to be used with mosquitto-auth-plug.
 * @author enZina Technologies <info@enzinatec.com>
 * @version 1.0
 * ================================================================
 */

var pbkdf2 = require('pbkdf2');
var crypto = require('crypto');
// All events are managed by this event manager.
var EventEmitter = require('events').EventEmitter;
var localEventEmitter = new EventEmitter();
localEventEmitter.on('passwordCreationStarted', createNewHash);
localEventEmitter.on('newPasswordGenerated', onNewPasswordCreated);
localEventEmitter.on('passwordRecreationStarted', recreateExistingHash);
localEventEmitter.on('passwordVerificationFinished', onPasswordVerificationFinished);

// Parameters configuration. Modify as needed.
var separator = '$';
var tag = 'PBKDF2';
var algorithm = 'sha256';
var iterations = 901;
var keyLen = 24;
var saltLen = 12;

/**
 * ================================================================
 * This function start the creation of a new PBKDF2 password to be
 * stored on MySQL for user authentication. It runs asynchronously
 * and uses the format required by mosquitto-auth-plug.
 * @description Starts the creation of a new PBKDF2 asynchronously
 * following the format required by mosquitto-auth-plug.
 * @param plainPassword The plain password for the specified
 * username.
 * @param onNewPassword Callback function to be called once the new
 * password generation has finished. It is passed in to the next
 * callback until it is necessary.
 * @author Manuel Domínguez-Dorado <manuel.dominguez@enzinatec.com>
 * @since 1.0
 * ================================================================
 */
function createPasswordAsync(plainPassword, onNewPassword) {
	localEventEmitter.emit('passwordCreationStarted', plainPassword, onNewPassword);
}

/**
 * ================================================================
 * This function is a callback that is launched when a
 * 'passwordCreationStarted' event is triggered. Creates a new
 * PBKDF2 password using the values specified as parameters. It
 * runs asynchronously and uses the format required by
 * mosquitto-auth-plug. On success, it emits a
 * 'newPasswordGenerated' event to notify other functions.
 * @description Creates a new PBKDF2 asynchronously following the
 * format required by mosquitto-auth-plug.
 * @param plainPassword The plain password for the specified
 * username.
 * @param onNewPassword Callback function to be called once the new
 * password generation has finished. It is passed in to the next
 * callback until it is necessary.
 * @author Manuel Domínguez-Dorado <manuel.dominguez@enzinatec.com>
 * @since 1.0
 * ================================================================
 */
function createNewHash(plainPassword, onNewPassword) {
	setImmediate(function () {
		var pw = plainPassword;
		var newSalt;
		var newPasswordForMySQL;
		var newHash;
		try {
			newSalt = crypto.randomBytes(saltLen).toString('base64');
			newHash = pbkdf2.pbkdf2Sync(pw, newSalt, iterations, keyLen, algorithm).toString('base64');
			newPasswordForMySQL = tag + separator + algorithm + separator + iterations + separator + newSalt + separator + newHash;
		} catch (err) {
			console.log('Not enough entropy to generate random salt');
			newSalt = 'NewDefaultSalt16';
			newHash = pbkdf2.pbkdf2Sync(pw, newSalt, iterations, keyLen, algorithm).toString('base64');
			newPasswordForMySQL = tag + separator + algorithm + separator + iterations + separator + newSalt + separator + newHash;
		} finally {
			localEventEmitter.emit('newPasswordGenerated', newPasswordForMySQL, onNewPassword);
		}
	});
}

/**
 * ================================================================
 * This function is a callback that is launched when a
 * 'newPasswordGenerated' event is triggered. It calls the callback
 * function that will receive the created PBKDF2 password.
 * @description Calls the callback function that will receive the
 * created PBKDF2 password.
 * @param pbkdf2Password The generated PBKDF2 that correspond to
 * the plainPassword that was specified at the begining of the
 * process.
 * @param onNewPassword Callback function that will receive the
 * created PBKDF2 password.
 * @author Manuel Domínguez-Dorado <manuel.dominguez@enzinatec.com>
 * @since 1.0
 * ================================================================
 */
function onNewPasswordCreated(pbkdf2Password, onNewPassword) {
	setImmediate(onNewPassword, pbkdf2Password);
}

/**
 * ================================================================
 * This function start the process of verifying if a given plain
 * password match a PBKDF2 hash (in mosquitto-auth-plug format),
 * and returns the result of such verification.
 * @description Verify if plain password match the supplied PBKDF2
 * hash.
 * @param plainPassword The plain password to be checked.
 * @param pbkdf2Password A PBKDF2 hash (in mosquitto-auth-plug
 * format) to be compared with the plain password supplied.
 * @param onVerificationFinished Callback function to be called
 * once the verificaation process has finished. It is passed in to
 * the next callback until it is necessary.
 * @author Manuel Domínguez-Dorado <manuel.dominguez@enzinatec.com>
 * @since 1.0
 * ================================================================
 */
function verifyCredentials(plainPassword, pbkdf2Password, onVerificationFinished) {
	localEventEmitter.emit('passwordRecreationStarted', plainPassword, pbkdf2Password, onVerificationFinished);
}

/**
 * ================================================================
 * This function is a callback that is launched when a
 * 'passwordRecreationStarted' event is triggered. Creates a new
 * PBKDF2 password using the values specified as parameters and
 * those oncluded in a previous PBKDF2 hash. It runs asynchronously
 * and uses the format required by mosquitto-auth-plug. On success,
 * it emits a 'passwordVerificationFinished' event to notify other
 * functions.
 * @description Recreates a PBKDF2 hash from a plain password and
 * checks whether it matches the previous PBKDF2 hash.
 * @param plainPassword The plain password to be checked.
 * @param pbkdf2Password A PBKDF2 hash (in mosquitto-auth-plug
 * format) to be compared with the plain password supplied.
 * @param onVerificationFinished Callback function to be called
 * once the verificaation process has finished. It is passed in to
 * the next callback until it is necessary.
 * @author Manuel Domínguez-Dorado <manuel.dominguez@enzinatec.com>
 * @since 1.0
 * ================================================================
 */
function recreateExistingHash(plainPassword, pbkdf2Password, onVerificationFinished) {
	setImmediate(function () {
		var pw = plainPassword;
		var fields = [];
		fields = pbkdf2Password.split(separator);
		var storedIterations = Number(fields[2]);
		var storedSalt = fields[3];
		var recreatedPasswordForMySQL;
		var recreatedHash;
		recreatedHash = pbkdf2.pbkdf2Sync(pw, storedSalt, storedIterations, keyLen, algorithm).toString('base64');
		recreatedPasswordForMySQL = tag + separator + algorithm + separator + iterations + separator + storedSalt + separator + recreatedHash;
		if (pbkdf2Password === recreatedPasswordForMySQL) {
			localEventEmitter.emit('passwordVerificationFinished', true, onVerificationFinished);
		} else {
			localEventEmitter.emit('passwordVerificationFinished', false, onVerificationFinished);
		}
	});
}

/**
 * ================================================================
 * This function is a callback that is launched when a
 * 'passwordVerificationFinished' event is triggered. It calls the
 * callback function that will receive the result of the password
 * verification.
 * @description Calls the callback function that will receive the
 * password verification result.
 * @param validPassword The result value, 'true' if password
 * verification is successful or 'false' on the contrary.
 * @param onVerificationFinished Callback function that will
 * receive the result of the password verification.
 * @author Manuel Domínguez-Dorado <manuel.dominguez@enzinatec.com>
 * @since 1.0
 * ================================================================
 */
function onPasswordVerificationFinished(validPassword, onVerificationFinished) {
	setImmediate(onVerificationFinished, validPassword);
}

exports.createPasswordAsync = createPasswordAsync;
exports.verifyCredentials = verifyCredentials;
