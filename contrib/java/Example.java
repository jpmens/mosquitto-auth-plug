package MosquittoPBKDF2;

import MosquittoPBKDF2.*;

/**
 * This is an example to generate a PBKDF2 password (in mosquito-auth-plug
 * format), and verify a couple plainPassword-PBKDF2Password with different
 * results.
 *
 * @author Manuel Domínguez Dorado - manuel.dominguez@enzinatec.com
 * @version 1.0
 */
public class Example {

    public static void main(String[] args) {
        MosquittoPBKDF2 pbkdf2Factory = new MosquittoPBKDF2();
        String hashedPassword = "";
        // Creating a new PBKDF2 password (in mosquitto-auth-plug format)
        System.out.println("Creating a new hash for \"" + Example.myPlainPassword + "\"...");
        hashedPassword = pbkdf2Factory.createPassword(Example.myPlainPassword);
        System.out.println("\t-> New hash for \"" + Example.myPlainPassword + "\": " + hashedPassword);
        // Validating the hashed password (this should be correct)
        System.out.println("Checking if \"" + hashedPassword + "\" is a valid hash for \"" + Example.myPlainPassword + "\"...");
        if (pbkdf2Factory.isValidPassword(Example.myPlainPassword, hashedPassword)) {
            System.out.println("\t-> Password match. It is valid!");
        } else {
            System.out.println("\t-> Password does not match. It is not valid");
        }
        // Validating the hashed password (this should be wrong)
        System.out.println("Checking if \"" + hashedPassword + "\" is a valid hash for \"" + Example.myIncorrectPlainPassword + "\"...");
        if (pbkdf2Factory.isValidPassword(Example.myIncorrectPlainPassword, hashedPassword)) {
            System.out.println("\t-> Password match. It is valid!");
        } else {
            System.out.println("\t-> Password does not match. It is not valid");
        }
    }

    private static final String myPlainPassword = "TheCorrectPassword";
    private static final String myIncorrectPlainPassword = "AWrongPassword";
}
