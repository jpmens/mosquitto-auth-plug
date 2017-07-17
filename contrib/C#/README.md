## Program written to create pbkdf2 hashes that mosquitto-auth-plug will accept.

##Complile mosquitto-auth-plug with -DRAW_SALT flag (you could add this in the config.mk file to CFG_CFLAGS)

CFG_CFLAGS = -DRAW_SALT

## You can easily change the hashing algorithm by changing the --

   System.Security.Cryptography.HMACSHA256(password)

   to your desired algorithm.  ** you will need to change the final output string to indicate desired algorithm. 
   
   Author Mark Talent drop me a note at github@mtalentlive.com let me know if it helped